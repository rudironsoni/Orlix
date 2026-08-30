import CryptoKit
import Foundation
import NetworkExtension

nonisolated enum TSSHVPNStartupDecision: Equatable, Sendable {
    case waiting
    case connected
    case failed(String)
}

nonisolated struct TSSHVPNStartupMonitor: Sendable {
    private(set) var observedProgress = false

    mutating func decision(for status: NEVPNStatus, elapsedSeconds: TimeInterval) -> TSSHVPNStartupDecision {
        switch status {
        case .connected:
            return .connected
        case .connecting, .reasserting:
            observedProgress = true
            return .waiting
        case .invalid:
            return .failed("The VPN configuration is invalid.")
        case .disconnecting:
            return .failed("The VPN disconnected during startup.")
        case .disconnected where observedProgress || elapsedSeconds >= 1:
            return .failed("The VPN disconnected during startup.")
        case .disconnected:
            return .waiting
        @unknown default:
            return .failed("The VPN entered an unknown state during startup.")
        }
    }
}

nonisolated enum TSSHVPNPostConnectDecision: Equatable, Sendable {
    case healthy
    case failed(String)
}

nonisolated func tsshVPNPostConnectDecision(for status: NEVPNStatus) -> TSSHVPNPostConnectDecision {
    switch status {
    case .connected, .connecting, .reasserting:
        return .healthy
    case .disconnecting, .disconnected:
        return .failed("The TSSH VPN disconnected after startup.")
    case .invalid:
        return .failed("The TSSH VPN became invalid after startup.")
    @unknown default:
        return .failed("The TSSH VPN entered an unknown state after startup.")
    }
}

@MainActor
final class TSSHVPNOwnershipCoordinator {
    enum Acquisition: Equatable {
        case existingOwner
        case vacant
    }

    private(set) var ownerID: UUID?

    func acquire(_ requestedOwnerID: UUID) throws -> Acquisition {
        guard ownerID == nil || ownerID == requestedOwnerID else {
            throw TSSHRuntimeError.vpnStartFailed(
                "Another TSSH pane already owns the system VPN tunnel."
            )
        }
        let acquisition: Acquisition = ownerID == nil ? .vacant : .existingOwner
        ownerID = requestedOwnerID
        return acquisition
    }

    func isOwned(by requestedOwnerID: UUID) -> Bool {
        ownerID == requestedOwnerID
    }

    func release(_ requestedOwnerID: UUID) {
        guard ownerID == requestedOwnerID else { return }
        ownerID = nil
    }
}

nonisolated struct TSSHVPNConfiguration: Encodable, Sendable {
    let transportType = "tssh"
    let tsshHost: String
    let tsshPort: Int
    let tsshMode: String
    let tsshServerVer: String
    let tsshPass: String?
    let tsshSalt: String?
    let tsshServerCert: String?
    let tsshClientCert: String?
    let tsshClientKey: String?
    let tsshProxyKey: String?
    let tsshClientID: UInt64
    let tsshServerID: UInt64
    let trzszMTU: Int
    let dnsServers: [String]
    let excludedRoutes: [String]
    let mtu: Int
    let blockQUIC: Bool

    init(host: String, info: TSSHServerInfo, profile: TSSHProfile) {
        tsshHost = host
        tsshPort = info.port
        tsshMode = info.mode.rawValue
        tsshServerVer = info.serverVersion
        tsshPass = info.kcpPassHex
        tsshSalt = info.kcpSaltHex
        tsshServerCert = info.serverCertHex
        tsshClientCert = info.clientCertHex
        tsshClientKey = info.clientKeyHex
        tsshProxyKey = info.proxyKeyHex
        tsshClientID = max(1, info.clientID)
        tsshServerID = info.serverID
        trzszMTU = profile.mtu > 0 ? profile.mtu : info.mtu
        dnsServers = profile.vpnDNSServers
        excludedRoutes = Array(Set(profile.vpnExcludedRoutes)).sorted()
        mtu = profile.mtu > 0 ? profile.mtu : 1_400
        blockQUIC = profile.blockQUICInVPN
    }

    private var encodedData: Data? {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.sortedKeys]
        return try? encoder.encode(self)
    }

    var json: String? {
        guard let data = encodedData else { return nil }
        return String(data: data, encoding: .utf8)
    }

    var fingerprint: String? {
        guard let data = encodedData else { return nil }
        return SHA256.hash(data: data).map { String(format: "%02x", $0) }.joined()
    }
}

nonisolated func tsshVPNConfigurationMatches(
    existingHost: String?,
    existingFingerprint: String?,
    requestedConfiguration: TSSHVPNConfiguration
) -> Bool {
    guard let requestedFingerprint = requestedConfiguration.fingerprint else { return false }
    return existingHost == requestedConfiguration.tsshHost
        && existingFingerprint == requestedFingerprint
}

@MainActor
final class TSSHVPNManager {
    static let shared = TSSHVPNManager()
    private let ownership = TSSHVPNOwnershipCoordinator()
    private let startupTimeoutSeconds: TimeInterval = 30
    private var statusMonitorTask: Task<Void, Never>?
    private var monitoredOwnerID: UUID?

    private init() {}

    func installAndStart(
        _ configuration: TSSHVPNConfiguration,
        ownerID: UUID,
        onUnexpectedDisconnect: @MainActor @Sendable @escaping (String) async -> Void
    ) async throws {
        guard let json = configuration.json,
              let configurationFingerprint = configuration.fingerprint else {
            throw TSSHRuntimeError.invalidProfile
        }
        let acquisition = try ownership.acquire(ownerID)
        var retainsOwnership = false
        defer {
            if !retainsOwnership { ownership.release(ownerID) }
        }
        let manager = try await loadManager()
        let existingProvider = manager.protocolConfiguration as? NETunnelProviderProtocol
        let existingOwnerID = existingProvider?
            .providerConfiguration?["tsshOwnerID"] as? String
        let existingConfigurationMatches = tsshVPNConfigurationMatches(
            existingHost: existingProvider?.providerConfiguration?["tsshHost"] as? String,
            existingFingerprint: existingProvider?
                .providerConfiguration?["tsshConfigFingerprint"] as? String,
            requestedConfiguration: configuration
        )
        if existingOwnerID == ownerID.uuidString && existingConfigurationMatches {
            switch manager.connection.status {
            case .connected:
                beginStatusMonitoring(
                    manager.connection,
                    ownerID: ownerID,
                    onUnexpectedDisconnect: onUnexpectedDisconnect
                )
                retainsOwnership = true
                return
            case .connecting, .reasserting:
                try await waitUntilConnected(manager.connection)
                beginStatusMonitoring(
                    manager.connection,
                    ownerID: ownerID,
                    onUnexpectedDisconnect: onUnexpectedDisconnect
                )
                retainsOwnership = true
                return
            default:
                break
            }
        } else if existingOwnerID == ownerID.uuidString {
            cancelStatusMonitoring(ifOwnedBy: ownerID)
            switch manager.connection.status {
            case .connecting, .connected, .reasserting:
                manager.connection.stopVPNTunnel()
                try await waitUntilDisconnected(manager.connection)
            case .disconnecting:
                try await waitUntilDisconnected(manager.connection)
            default:
                break
            }
        } else if existingOwnerID != nil {
            let canReclaimActiveTunnel = acquisition == .vacant
                && existingConfigurationMatches
            if canReclaimActiveTunnel {
                switch manager.connection.status {
                case .connecting, .connected, .reasserting:
                    var providerConfiguration = existingProvider?.providerConfiguration ?? [:]
                    providerConfiguration["tsshOwnerID"] = ownerID.uuidString
                    existingProvider?.providerConfiguration = providerConfiguration
                    manager.protocolConfiguration = existingProvider
                    try await manager.saveToPreferences()
                    try await manager.loadFromPreferences()
                    try await waitUntilConnected(manager.connection)
                    beginStatusMonitoring(
                        manager.connection,
                        ownerID: ownerID,
                        onUnexpectedDisconnect: onUnexpectedDisconnect
                    )
                    retainsOwnership = true
                    return
                default:
                    break
                }
            }
            switch manager.connection.status {
            case .connecting, .connected, .reasserting:
                if acquisition == .vacant {
                    manager.connection.stopVPNTunnel()
                    try await waitUntilDisconnected(manager.connection)
                    break
                }
                throw TSSHRuntimeError.vpnStartFailed(
                    "Another TSSH pane already owns the system VPN tunnel."
                )
            case .disconnecting:
                try await waitUntilDisconnected(manager.connection)
            default:
                break
            }
        }
        let previousKey = (manager.protocolConfiguration as? NETunnelProviderProtocol)?
            .providerConfiguration?["tsshConfigKey"] as? String
        let configKey = UUID().uuidString
        try TSSHVPNSecretStore.save(json, key: configKey)
        let provider = NETunnelProviderProtocol()
        provider.providerBundleIdentifier = "com.rudironsoni.orlix.packet-tunnel"
        provider.serverAddress = configuration.tsshHost
        provider.providerConfiguration = [
            "tsshConfigKey": configKey,
            "tsshConfigFingerprint": configurationFingerprint,
            "tsshHost": configuration.tsshHost,
            "tsshOwnerID": ownerID.uuidString,
        ]
        manager.protocolConfiguration = provider
        manager.localizedDescription = "Orlix TSSH VPN"
        manager.isEnabled = true
        do {
            try await manager.saveToPreferences()
            try await manager.loadFromPreferences()
            try manager.connection.startVPNTunnel()
            try await waitUntilConnected(manager.connection)
        } catch {
            manager.connection.stopVPNTunnel()
            try? TSSHVPNSecretStore.delete(key: configKey)
            throw error
        }
        retainsOwnership = true
        beginStatusMonitoring(
            manager.connection,
            ownerID: ownerID,
            onUnexpectedDisconnect: onUnexpectedDisconnect
        )
        if let previousKey, previousKey != configKey {
            try? TSSHVPNSecretStore.delete(key: previousKey)
        }
    }

    func stop(ifOwnedBy ownerID: UUID) async throws {
        cancelStatusMonitoring(ifOwnedBy: ownerID)
        let manager = try await loadManager()
        guard let provider = manager.protocolConfiguration as? NETunnelProviderProtocol,
              provider.providerConfiguration?["tsshOwnerID"] as? String == ownerID.uuidString,
              ownership.ownerID == nil || ownership.isOwned(by: ownerID) else {
            return
        }
        manager.connection.stopVPNTunnel()
        ownership.release(ownerID)
    }

    private func beginStatusMonitoring(
        _ connection: NEVPNConnection,
        ownerID: UUID,
        onUnexpectedDisconnect: @MainActor @Sendable @escaping (String) async -> Void
    ) {
        statusMonitorTask?.cancel()
        monitoredOwnerID = ownerID
        statusMonitorTask = Task { [weak self, weak connection] in
            guard let self, let connection else { return }
            while !Task.isCancelled {
                try? await Task.sleep(for: .milliseconds(100))
                guard !Task.isCancelled, self.ownership.isOwned(by: ownerID) else { return }
                guard case .failed(let message) = tsshVPNPostConnectDecision(
                    for: connection.status
                ) else {
                    continue
                }
                self.ownership.release(ownerID)
                self.monitoredOwnerID = nil
                self.statusMonitorTask = nil
                await onUnexpectedDisconnect(message)
                return
            }
        }
    }

    private func cancelStatusMonitoring(ifOwnedBy ownerID: UUID) {
        guard monitoredOwnerID == ownerID else { return }
        statusMonitorTask?.cancel()
        statusMonitorTask = nil
        monitoredOwnerID = nil
    }

    private func waitUntilConnected(_ connection: NEVPNConnection) async throws {
        let startedAt = ContinuousClock.now
        var monitor = TSSHVPNStartupMonitor()
        while true {
            let elapsed = startedAt.duration(to: .now)
            let elapsedSeconds = Double(elapsed.components.seconds)
                + Double(elapsed.components.attoseconds) / 1e18
            switch monitor.decision(for: connection.status, elapsedSeconds: elapsedSeconds) {
            case .connected:
                return
            case .failed(let message):
                throw TSSHRuntimeError.vpnStartFailed(message)
            case .waiting where elapsedSeconds >= startupTimeoutSeconds:
                throw TSSHRuntimeError.vpnStartFailed(
                    "The VPN did not connect within \(Int(startupTimeoutSeconds)) seconds."
                )
            case .waiting:
                try await Task.sleep(for: .milliseconds(100))
            }
        }
    }

    private func waitUntilDisconnected(_ connection: NEVPNConnection) async throws {
        let deadline = ContinuousClock.now.advanced(by: .seconds(30))
        while connection.status != .disconnected && connection.status != .invalid {
            guard ContinuousClock.now < deadline else {
                throw TSSHRuntimeError.vpnStartFailed(
                    "The previous VPN tunnel did not stop within 30 seconds."
                )
            }
            try await Task.sleep(for: .milliseconds(100))
        }
    }

    private func loadManager() async throws -> NETunnelProviderManager {
        let managers = try await NETunnelProviderManager.loadAllFromPreferences()
        return managers.first { $0.localizedDescription == "Orlix TSSH VPN" }
            ?? NETunnelProviderManager()
    }
}
