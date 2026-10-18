import Foundation

nonisolated package struct ServerCredentialBinding: Codable, Equatable, Sendable {
    package let host: String
    package let port: Int
    package let eternalTerminalPort: Int?
    package let username: String
    package let connectionMode: SSHConnectionMode
    package let authMethod: AuthMethod
    package let cloudflareAccessMode: CloudflareAccessMode?
    package let cloudflareTeamDomain: String?
    package let cloudflareAppDomain: String?

    package init(server: Server) {
        host = Self.normalizedNetworkName(server.host)
        port = server.port
        eternalTerminalPort = server.connectionMode == .eternalTerminal
            ? server.eternalTerminalPort
            : nil
        username = server.username.trimmingCharacters(in: .whitespacesAndNewlines)
        connectionMode = server.connectionMode
        authMethod = server.authMethod

        if server.connectionMode == .cloudflare {
            cloudflareAccessMode = server.cloudflareAccessMode ?? .oauth
            cloudflareTeamDomain = Self.normalizedOptionalNetworkName(
                server.cloudflareTeamDomainOverride
            )
            cloudflareAppDomain = Self.normalizedOptionalNetworkName(
                server.cloudflareAppDomainOverride
            )
        } else {
            cloudflareAccessMode = nil
            cloudflareTeamDomain = nil
            cloudflareAppDomain = nil
        }
    }

    private static func normalizedOptionalNetworkName(_ value: String?) -> String? {
        guard let value else { return nil }
        let normalized = normalizedNetworkName(value)
        return normalized.isEmpty ? nil : normalized
    }

    private static func normalizedNetworkName(_ value: String) -> String {
        var normalized = value
            .trimmingCharacters(in: .whitespacesAndNewlines)
            .lowercased(with: Locale(identifier: "en_US_POSIX"))
        while normalized.last == "." {
            normalized.removeLast()
        }
        return normalized
    }
}

package extension ServerCredentials {
    nonisolated package func isAuthorized(for server: Server) -> Bool {
        serverId == server.id
            && credentialBinding == ServerCredentialBinding(server: server)
    }

}
