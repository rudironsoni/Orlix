import Foundation

nonisolated struct TSSHServerInfo: Codable, Equatable, Sendable {
    nonisolated enum Mode: String, Codable, Sendable {
        case kcp = "KCP"
        case quic = "QUIC"
    }

    let serverVersion: String
    let protocolVersion: Int
    let port: Int
    let mode: Mode
    let serverCertHex: String?
    let clientCertHex: String?
    let clientKeyHex: String?
    let kcpPassHex: String?
    let kcpSaltHex: String?
    let proxyKeyHex: String?
    let proxyMode: String
    let mtu: Int
    var clientID: UInt64
    let serverID: UInt64

    private struct BootstrapResponse: Decodable {
        let serverVersion: String
        let protocolVersion: Int?
        let port: Int
        let mode: Mode
        let serverCertHex: String?
        let clientCertHex: String?
        let clientKeyHex: String?
        let kcpPassHex: String?
        let kcpSaltHex: String?
        let proxyKeyHex: String?
        let proxyMode: String?
        let mtu: Int?
        let clientID: UInt64?
        let serverID: UInt64?

        private enum CodingKeys: String, CodingKey {
            case serverVersion = "ServerVer"
            case protocolVersion = "ProtoVer"
            case port = "Port"
            case mode = "Mode"
            case serverCertHex = "ServerCert"
            case clientCertHex = "ClientCert"
            case clientKeyHex = "ClientKey"
            case kcpPassHex = "Pass"
            case kcpSaltHex = "Salt"
            case proxyKeyHex = "ProxyKey"
            case proxyMode = "ProxyMode"
            case mtu = "MTU"
            case clientID = "ClientID"
            case serverID = "ServerID"
        }
    }

    static func parse(output: String) throws -> Self {
        guard let start = output.firstIndex(of: "{"),
              let end = output.lastIndex(of: "}"), start <= end,
              let data = String(output[start...end]).data(using: .utf8),
              data.count <= 64 * 1_024
        else {
            throw TSSHRuntimeError.invalidServerResponse
        }
        let response: BootstrapResponse
        do {
            response = try JSONDecoder().decode(BootstrapResponse.self, from: data)
        } catch {
            throw TSSHRuntimeError.invalidServerResponse
        }
        guard !response.serverVersion.isEmpty,
              response.serverVersion.utf8.count <= 128,
              (1...65_535).contains(response.port) else {
            throw TSSHRuntimeError.invalidServerResponse
        }
        let clientID = response.clientID ?? 0
        let serverID = response.serverID ?? 0
        let result = Self(
            serverVersion: response.serverVersion,
            protocolVersion: response.protocolVersion ?? 0,
            port: response.port,
            mode: response.mode,
            serverCertHex: response.serverCertHex,
            clientCertHex: response.clientCertHex,
            clientKeyHex: response.clientKeyHex,
            kcpPassHex: response.kcpPassHex,
            kcpSaltHex: response.kcpSaltHex,
            proxyKeyHex: response.proxyKeyHex,
            proxyMode: response.proxyMode ?? "",
            mtu: response.mtu ?? 0,
            clientID: clientID,
            serverID: serverID
        )
        guard result.hasRequiredCredentials,
              result.protocolVersion >= 0,
              result.protocolVersion <= 1_000,
              result.proxyMode.isEmpty || result.proxyMode == "TCP",
              result.mtu == 0 || (576...9_000).contains(result.mtu) else {
            throw TSSHRuntimeError.invalidServerResponse
        }
        return result
    }

    var hasRequiredCredentials: Bool {
        guard Self.isBoundedHex(proxyKeyHex) else { return false }
        switch mode {
        case .kcp:
            return Self.isBoundedHex(kcpPassHex) && Self.isBoundedHex(kcpSaltHex)
        case .quic:
            return Self.isBoundedHex(serverCertHex)
                && Self.isBoundedHex(clientCertHex)
                && Self.isBoundedHex(clientKeyHex)
        }
    }

    private static func isBoundedHex(_ value: String?) -> Bool {
        guard let value,
              !value.isEmpty,
              value.utf8.count <= 32 * 1_024,
              value.utf8.count.isMultiple(of: 2) else { return false }
        return value.unicodeScalars.allSatisfy { scalar in
            switch scalar.value {
            case 48...57, 65...70, 97...102:
                return true
            default:
                return false
            }
        }
    }

    func assigningClientIDIfNeeded() -> Self {
        guard clientID == 0 else { return self }
        var copy = self
        copy.clientID = UInt64.random(in: 1...UInt64.max)
        return copy
    }

    func advancingClientID() -> Self {
        var copy = self
        copy.clientID = clientID == UInt64.max ? 1 : max(1, clientID + 1)
        return copy
    }

    func advancingClientIDForResume(vpnEnabled: Bool) -> Self {
        let nextClient = advancingClientID()
        guard vpnEnabled else { return nextClient }
        return nextClient.advancingClientID()
    }
}

nonisolated enum TSSHRuntimeError: LocalizedError, Sendable {
    case invalidProfile
    case unsupportedRemoteEnvironment
    case tsshdNotFound
    case invalidServerResponse
    case bootstrapFailed(String)
    case transportFailed(String)
    case vpnStartFailed(String)
    case resumeStateExpired
    case resumeStateUnavailable

    var errorDescription: String? {
        switch self {
        case .invalidProfile:
            return "The TSSH profile is invalid."
        case .unsupportedRemoteEnvironment:
            return "TSSH requires a POSIX remote shell."
        case .tsshdNotFound:
            return "tsshd is not installed on the remote host."
        case .invalidServerResponse:
            return "tsshd returned invalid connection data."
        case .bootstrapFailed(let reason):
            return "TSSH bootstrap failed: \(reason)"
        case .transportFailed(let reason):
            return "TSSH transport failed: \(reason)"
        case .vpnStartFailed(let reason):
            return "TSSH VPN failed to start: \(reason)"
        case .resumeStateExpired:
            return "The saved TSSH session has expired."
        case .resumeStateUnavailable:
            return "The saved TSSH session is unavailable."
        }
    }
}
