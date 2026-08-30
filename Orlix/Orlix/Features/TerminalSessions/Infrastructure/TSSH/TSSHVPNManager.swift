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

@MainActor
final class TSSHVPNOwnershipCoordinator {
    private(set) var ownerID: UUID?

    func acquire(_ requestedOwnerID: UUID) throws {
        guard ownerID == nil || ownerID == requestedOwnerID else {
            throw TSSHRuntimeError.vpnStartFailed(
                "Another TSSH pane already owns the system VPN tunnel."
            )
        }
        ownerID = requestedOwnerID
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

    var json: String? {
        guard let data = try? JSONEncoder().encode(self) else { return nil }
        return String(data: data, encoding: .utf8)
    }
}

@MainActor
final class TSSHVPNManager {
    static let shared = TSSHVPNManager()
    private let ownership = TSSHVPNOwnershipCoordinator()
    private let startupTimeoutSeconds: TimeInterval = 30

    private init() {}

    func installAndStart(_ configuration: TSSHVPNConfiguration, ownerID: UUID) async throws {
        guard let json = configuration.json else { throw TSSHRuntimeError.invalidProfile }
        try ownership.acquire(ownerID)
        var retainsOwnership = false
        defer {
            if !retainsOwnership { ownership.release(ownerID) }
        }
        let manager = try await loadManager()
        let existingOwnerID = (manager.protocolConfiguration as? NETunnelProviderProtocol)?
            .providerConfiguration?["tsshOwnerID"] as? String
        if existingOwnerID == ownerID.uuidString {
            switch manager.connection.status {
            case .connected:
                retainsOwnership = true
                return
            case .connecting, .reasserting:
                try await waitUntilConnected(manager.connection)
                retainsOwnership = true
                return
            default:
                break
            }
        } else if existingOwnerID != nil {
            switch manager.connection.status {
            case .connecting, .connected, .reasserting, .disconnecting:
                throw TSSHRuntimeError.vpnStartFailed(
                    "Another TSSH pane already owns the system VPN tunnel."
                )
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
        if let previousKey, previousKey != configKey {
            try? TSSHVPNSecretStore.delete(key: previousKey)
        }
    }

    func stop(ifOwnedBy ownerID: UUID) async throws {
        let manager = try await loadManager()
        guard let provider = manager.protocolConfiguration as? NETunnelProviderProtocol,
              provider.providerConfiguration?["tsshOwnerID"] as? String == ownerID.uuidString,
              ownership.ownerID == nil || ownership.isOwned(by: ownerID) else {
            return
        }
        manager.connection.stopVPNTunnel()
        ownership.release(ownerID)
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

    private func loadManager() async throws -> NETunnelProviderManager {
        let managers = try await NETunnelProviderManager.loadAllFromPreferences()
        return managers.first { $0.localizedDescription == "Orlix TSSH VPN" }
            ?? NETunnelProviderManager()
    }
}
