import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package enum ServerTransportSelection: String, CaseIterable, Identifiable, Equatable, Sendable {
    case standard
    case tailscale
    case mosh
    case eternalTerminal
    case cloudflare

    package var id: String { rawValue }

    package var connectionMode: SSHConnectionMode {
        switch self {
        case .standard:
            return .standard
        case .tailscale:
            return .tailscale
        case .mosh:
            return .mosh
        case .eternalTerminal:
            return .eternalTerminal
        case .cloudflare:
            return .cloudflare
        }
    }

    package init(server: Server) {
        switch server.connectionMode {
        case .standard:
            self = .standard
        case .tailscale:
            self = .tailscale
        case .mosh:
            self = .mosh
        case .eternalTerminal:
            self = .eternalTerminal
        case .cloudflare:
            self = .cloudflare
        }
    }
}

nonisolated package struct ServerFormModel: Equatable, Sendable {
    nonisolated package enum WakeOnLANSavePlan: Equatable, Sendable {
        case clearConfiguration
        case configured(WakeOnLANConfiguration)
        case resolveAutomatically
    }

    nonisolated package struct ConnectionSnapshot: Equatable, Sendable {
        package let host: String
        package let port: String
        package let eternalTerminalPort: String
        package let username: String
        package let transportSelection: ServerTransportSelection
        package let authMethod: AuthMethod
        package let password: String
        package let sshKey: String
        package let sshPassphrase: String
        package let sshPublicKey: String
        package let cloudflareAccessMode: CloudflareAccessMode
        package let cloudflareClientID: String
        package let cloudflareClientSecret: String
        package let cloudflareTeamDomainOverride: String
    }

    package var name: String
    package var host: String
    package var port: String
    package var eternalTerminalPort: String
    package var username: String
    package var transportSelection: ServerTransportSelection
    package var authMethod: AuthMethod
    package var password: String
    package var sshKey: String
    package var sshPassphrase: String
    package var sshPublicKey: String
    package var cloudflareAccessMode: CloudflareAccessMode
    package var cloudflareClientID: String
    package var cloudflareClientSecret: String
    package var cloudflareTeamDomainOverride: String
    package var autoWakeOnLANEnabled: Bool
    package var workspaceID: UUID?
    package var environment: ServerEnvironment
    package var notes: String
    package var requiresBiometricUnlock: Bool
    package var remoteSessionEnabled: Bool
    package var remoteSessionBackendIdentifier: RemoteSessionBackendIdentifier
    package var remoteSessionStartupBehavior: RemoteSessionStartupBehavior
    package var remoteShellStartupAction: RemoteShellStartupActionFormModel

    package init(
        server: Server? = nil,
        workspaceID: UUID? = nil,
        defaultRemoteSessionEnabled: Bool,
        defaultRemoteSessionBackendIdentifier: RemoteSessionBackendIdentifier,
        defaultRemoteSessionStartupBehavior: RemoteSessionStartupBehavior
    ) {
        name = server?.name ?? ""
        host = server?.host ?? ""
        port = String(server?.port ?? 22)
        eternalTerminalPort = String(server?.eternalTerminalPort ?? 2022)
        username = server?.username ?? ""
        transportSelection = server.map(ServerTransportSelection.init) ?? .standard
        authMethod = server?.authMethod ?? .password
        password = ""
        sshKey = ""
        sshPassphrase = ""
        sshPublicKey = ""
        cloudflareAccessMode = server?.cloudflareAccessMode ?? .oauth
        cloudflareClientID = ""
        cloudflareClientSecret = ""
        cloudflareTeamDomainOverride = server?.cloudflareTeamDomainOverride ?? ""
        autoWakeOnLANEnabled = server?.autoWakeOnLANEnabled ?? false
        self.workspaceID = server?.workspaceId ?? workspaceID
        environment = server?.environment ?? .production
        notes = server?.notes ?? ""
        requiresBiometricUnlock = server?.requiresBiometricUnlock ?? false
        remoteSessionEnabled = server?.remoteSessionEnabledOverride
            ?? defaultRemoteSessionEnabled
        remoteSessionBackendIdentifier = server?.remoteSessionBackendIdentifier
            ?? defaultRemoteSessionBackendIdentifier
        remoteSessionStartupBehavior = server?.remoteSessionStartupBehaviorOverride
            ?? defaultRemoteSessionStartupBehavior
        remoteShellStartupAction = RemoteShellStartupActionFormModel(
            action: server?.remoteShellStartupAction
        )
    }

    package var isValid: Bool {
        !name.isEmpty
            && !host.isEmpty
            && validPort(port)
            && (transportSelection != .eternalTerminal || validPort(eternalTerminalPort))
            && hasValidCredentials
            && remoteShellStartupAction.isValid
    }

    package var effectiveUsername: String {
        let trimmed = username.trimmingCharacters(in: .whitespacesAndNewlines)
        return trimmed.isEmpty ? "root" : trimmed
    }

    package var connectionSnapshot: ConnectionSnapshot {
        ConnectionSnapshot(
            host: host,
            port: port,
            eternalTerminalPort: eternalTerminalPort,
            username: effectiveUsername,
            transportSelection: transportSelection,
            authMethod: authMethod,
            password: password,
            sshKey: sshKey,
            sshPassphrase: sshPassphrase,
            sshPublicKey: sshPublicKey,
            cloudflareAccessMode: cloudflareAccessMode,
            cloudflareClientID: cloudflareClientID,
            cloudflareClientSecret: cloudflareClientSecret,
            cloudflareTeamDomainOverride: cloudflareTeamDomainOverride
        )
    }

    package func wakeOnLANSavePlan(existingServer: Server?) -> WakeOnLANSavePlan {
        if let existingServer,
           Self.normalizedHost(existingServer.host) == Self.normalizedHost(host),
           let configuration = existingServer.wakeOnLANConfiguration {
            return .configured(configuration)
        }
        return autoWakeOnLANEnabled ? .resolveAutomatically : .clearConfiguration
    }

    mutating package func applyPrefill(
        name: String,
        host: String,
        port: Int,
        username: String?
    ) {
        self.name = name
        self.host = host
        self.port = String(port)
        if let username, !username.isEmpty {
            self.username = username
        }
    }

    mutating package func apply(_ credentials: ServerCredentials, for server: Server) {
        if server.connectionMode != .tailscale {
            switch server.authMethod {
            case .password:
                password = credentials.password ?? ""
            case .sshKey, .sshKeyWithPassphrase:
                if let privateKey = credentials.privateKey,
                   let key = String(data: privateKey, encoding: .utf8) {
                    sshKey = key
                }
                if server.authMethod == .sshKeyWithPassphrase {
                    sshPassphrase = credentials.passphrase ?? ""
                }
            }
        }

        sshPublicKey = credentials.publicKey.flatMap { String(data: $0, encoding: .utf8) } ?? ""
        cloudflareClientID = credentials.cloudflareClientID ?? ""
        cloudflareClientSecret = credentials.cloudflareClientSecret ?? ""
    }

    package func makeServer(
        id: UUID,
        workspaceID: UUID,
        createdAt: Date
    ) -> Server {
        Server(
            id: id,
            workspaceId: workspaceID,
            environment: environment,
            name: name,
            host: host,
            port: Int(port) ?? 22,
            eternalTerminalPort: Int(eternalTerminalPort) ?? 2022,
            username: effectiveUsername,
            connectionMode: transportSelection.connectionMode,
            authMethod: transportSelection == .tailscale ? .password : authMethod,
            cloudflareAccessMode: transportSelection == .cloudflare ? cloudflareAccessMode : nil,
            cloudflareTeamDomainOverride: transportSelection == .cloudflare
                ? normalizedOptional(cloudflareTeamDomainOverride)
                : nil,
            cloudflareAppDomainOverride: nil,
            autoWakeOnLANEnabled: autoWakeOnLANEnabled,
            notes: notes.isEmpty ? nil : notes,
            requiresBiometricUnlock: requiresBiometricUnlock,
            remoteSessionEnabledOverride: remoteSessionEnabled,
            remoteSessionBackendIdentifier: remoteSessionBackendIdentifier,
            remoteSessionStartupBehaviorOverride: remoteSessionStartupBehavior,
            remoteShellStartupCommand: remoteShellStartupAction.commandForPersistence(),
            createdAt: createdAt
        )
    }

    package func makeCredentials(serverID: UUID) -> ServerCredentials {
        var credentials = ServerCredentials(serverId: serverID)

        guard transportSelection != .tailscale else {
            return credentials
        }

        switch authMethod {
        case .password:
            credentials.password = password
        case .sshKey:
            credentials.sshKey = sshKey.data(using: .utf8)
            credentials.publicKey = nonEmptyData(sshPublicKey)
        case .sshKeyWithPassphrase:
            credentials.sshKey = sshKey.data(using: .utf8)
            credentials.sshPassphrase = sshPassphrase
            credentials.publicKey = nonEmptyData(sshPublicKey)
        }

        if transportSelection == .cloudflare, cloudflareAccessMode == .serviceToken {
            credentials.cloudflareClientID = normalizedOptional(cloudflareClientID)
            credentials.cloudflareClientSecret = normalizedOptional(cloudflareClientSecret)
        }

        return credentials
    }

    private var hasValidCredentials: Bool {
        guard transportSelection != .tailscale else {
            return true
        }

        if transportSelection == .cloudflare,
           cloudflareAccessMode == .serviceToken,
           (normalizedOptional(cloudflareClientID) == nil
                || normalizedOptional(cloudflareClientSecret) == nil) {
            return false
        }

        switch authMethod {
        case .password:
            return !password.isEmpty
        case .sshKey:
            return !sshKey.isEmpty
        case .sshKeyWithPassphrase:
            return !sshKey.isEmpty && !sshPassphrase.isEmpty
        }
    }

    private func validPort(_ value: String) -> Bool {
        Int(value).map { (1...65_535).contains($0) } == true
    }

    private static func normalizedHost(_ value: String) -> String {
        value.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
    }

    private func normalizedOptional(_ value: String) -> String? {
        let trimmed = value.trimmingCharacters(in: .whitespacesAndNewlines)
        return trimmed.isEmpty ? nil : trimmed
    }

    private func nonEmptyData(_ value: String) -> Data? {
        value.isEmpty ? nil : value.data(using: .utf8)
    }
}
