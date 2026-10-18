import Combine
import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
import OrlixSSH
#endif
nonisolated package enum TerminalNetworkReadiness: String, Hashable, Sendable {
    case unknown
    case ready
    case unavailable
}

nonisolated package enum TerminalRemoteSessionDefaults {
    static package let enabledKey = "terminalRemoteSessionEnabledDefault"
    static package let backendIdentifierKey = "terminalRemoteSessionBackendIdentifierDefault"
    static package let startupBehaviorKey = "terminalRemoteSessionStartupBehaviorDefault"
    static package let legacyEnabledKey = "terminalTmuxEnabledDefault"
    static package let legacyStartupBehaviorKey = "terminalTmuxStartupBehaviorDefault"
}

nonisolated package protocol TerminalRemoteSessionServicing: Sendable {
    var backendMetadata: [RemoteSessionBackendMetadata] { get }
    func managedIdentifier(
        deviceID: String,
        entityID: UUID,
        serverName: String,
        backendIdentifier: RemoteSessionBackendIdentifier
    ) throws -> RemoteSessionIdentifier
    func availability(
        for backendIdentifier: RemoteSessionBackendIdentifier,
        using client: SSHClient
    ) async -> RemoteSessionAvailability
    func listSessions(
        scope: RemoteSessionListScope,
        using client: SSHClient,
        runtime: RemoteSessionRuntime
    ) async throws -> [RemoteSessionDescriptor]
    func prepareManagedSession(
        using client: SSHClient,
        terminalType: RemoteTerminalType,
        themeStyle: RemoteSessionThemeStyle,
        runtime: RemoteSessionRuntime
    ) async
    func launchPlan(
        for request: RemoteSessionLaunchRequest,
        runtime: RemoteSessionRuntime
    ) async throws -> RemoteSessionBackendLaunchPlan
    func installScript(
        attachment: RemoteSessionAttachment,
        workingDirectory: String,
        terminalType: RemoteTerminalType,
        themeStyle: RemoteSessionThemeStyle,
        using client: SSHClient,
        attachAfterInstall: Bool
    ) async -> String?
    func sendScript(
        _ script: String,
        using client: SSHClient,
        shellId: UUID
    ) async throws
    func killSession(
        _ identifier: RemoteSessionIdentifier,
        using client: SSHClient,
        runtime: RemoteSessionRuntime?
    ) async
    func cleanupSessions(
        keeping identifiers: Set<RemoteSessionIdentifier>,
        using client: SSHClient,
        runtime: RemoteSessionRuntime
    ) async
    func currentWorkingDirectory(
        for attachment: RemoteSessionAttachment,
        using client: SSHClient,
        runtime: RemoteSessionRuntime?
    ) async -> String?
}

package extension TerminalRemoteSessionServicing {
    nonisolated func managedIdentifier(
        deviceID: String,
        entityID: UUID,
        serverName: String,
        backendIdentifier: RemoteSessionBackendIdentifier
    ) throws -> RemoteSessionIdentifier {
        try RemoteSessionManagedIdentifierPolicy.identifier(
            backendIdentifier: backendIdentifier,
            serverName: serverName,
            deviceID: deviceID,
            entityID: entityID
        )
    }

}

nonisolated package protocol TerminalRemoteMoshServicing: Sendable {
    func installMoshServer(using client: SSHClient) async throws
}

@MainActor
package struct TerminalNetworkReadinessSource {
    package let initial: TerminalNetworkReadiness
    package let updates: AnyPublisher<TerminalNetworkReadiness, Never>
}

@MainActor
package struct TerminalAppLockSource {
    package let initialIsLocked: Bool
    package let updates: AnyPublisher<Bool, Never>
}

@MainActor
package struct TerminalSessionApplicationEffects {
    package let authorizeServer: (Server) async -> Bool
    package let prepareInitialConnection: (Server) -> Void
    package let refreshLiveActivity: ([ConnectionState]) -> Void
    package let recordSuccessfulConnection: (UUID, String) -> Void
    package let noteTerminalSessionEnded: (Bool) -> Void
    package let recordSplitPaneCreated: () -> Void
}

@MainActor
package struct TerminalRemoteSessionConfiguration {
    package struct ServerSettings {
        package let name: String
        package let enabledOverride: Bool?
        package let backendIdentifier: RemoteSessionBackendIdentifier
        package let startupBehaviorOverride: RemoteSessionStartupBehavior?
        package let startupAction: RemoteShellStartupAction?
    }

    package let deviceID: String
    package let enabledByDefault: () -> Bool
    package let backendIdentifierByDefault: () -> RemoteSessionBackendIdentifier
    package let startupBehaviorByDefault: () -> RemoteSessionStartupBehavior
    package let serverSettings: (UUID) -> ServerSettings?
    package let themeStyle: @MainActor () -> RemoteSessionThemeStyle
}

@MainActor
package struct TerminalTabManagerDependencies {
    package let sshClientFactory: SSHClientFactory
    package let networkReadiness: TerminalNetworkReadinessSource
    package let applicationIsActive: @MainActor @Sendable () -> Bool
    package let appLock: TerminalAppLockSource
    package let effects: TerminalSessionApplicationEffects
    package let remoteMosh: any TerminalRemoteMoshServicing
    package let eternalTerminalRuntime: EternalTerminalRuntimeDependencies
}

#if DEBUG
package extension TerminalTabManagerDependencies {
    static package func testing(
        networkReadinessPublisher: AnyPublisher<TerminalNetworkReadiness, Never>?,
        liveActivityRefresh: @escaping ([ConnectionState]) -> Void
    ) -> Self {
        let updates = networkReadinessPublisher
            ?? Empty<TerminalNetworkReadiness, Never>().eraseToAnyPublisher()
        return Self(
            sshClientFactory: .testing(),
            networkReadiness: TerminalNetworkReadinessSource(
                initial: .ready,
                updates: updates
            ),
            applicationIsActive: { true },
            appLock: TerminalAppLockSource(
                initialIsLocked: false,
                updates: Empty<Bool, Never>().eraseToAnyPublisher()
            ),
            effects: TerminalSessionApplicationEffects(
                authorizeServer: { _ in true },
                prepareInitialConnection: { _ in },
                refreshLiveActivity: liveActivityRefresh,
                recordSuccessfulConnection: { _, _ in },
                noteTerminalSessionEnded: { _ in },
                recordSplitPaneCreated: {}
            ),
            remoteMosh: UnavailableTerminalRemoteMoshService(),
            eternalTerminalRuntime: .testing
        )
    }
}

private actor UnavailableTerminalRemoteMoshService: TerminalRemoteMoshServicing {
    package func installMoshServer(using client: SSHClient) async throws {
        throw SSHError.notConnected
    }
}
#endif
