import Foundation
import NetworkExtension

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
    private init() {}

    func installAndStart(_ configuration: TSSHVPNConfiguration) async throws {
        guard let json = configuration.json else { throw TSSHRuntimeError.invalidProfile }
        let manager = try await loadManager()
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
        ]
        manager.protocolConfiguration = provider
        manager.localizedDescription = "Orlix TSSH VPN"
        manager.isEnabled = true
        do {
            try await manager.saveToPreferences()
            try await manager.loadFromPreferences()
            try manager.connection.startVPNTunnel()
        } catch {
            try? TSSHVPNSecretStore.delete(key: configKey)
            throw error
        }
        if let previousKey, previousKey != configKey {
            try? TSSHVPNSecretStore.delete(key: previousKey)
        }
    }

    func stop() async throws {
        let manager = try await loadManager()
        manager.connection.stopVPNTunnel()
    }

    private func loadManager() async throws -> NETunnelProviderManager {
        let managers = try await NETunnelProviderManager.loadAllFromPreferences()
        return managers.first { $0.localizedDescription == "Orlix TSSH VPN" }
            ?? NETunnelProviderManager()
    }
}
