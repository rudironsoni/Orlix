import Foundation

#if ORLIX_MODULAR_APP
import OrlixSSH
#endif
nonisolated package enum EternalTerminalRuntimeEvent: Equatable, Sendable {
    case connectionAttempted
    case connectionReconnecting
    case connectionFailed(reason: String)
}

nonisolated package protocol EternalTerminalRemoteSessionKilling: Sendable {
    func killSession(_ identifier: RemoteSessionIdentifier, using client: SSHClient) async
}

nonisolated package struct EternalTerminalRuntimeDependencies: Sendable {
    private let recordEvent: @MainActor @Sendable (EternalTerminalRuntimeEvent) -> Void
    private let remoteSessionKiller: any EternalTerminalRemoteSessionKilling
    package let sessionPreparer: any EternalTerminalSessionPreparing

    package init(
        recordEvent: @MainActor @Sendable @escaping (EternalTerminalRuntimeEvent) -> Void,
        remoteSessionKiller: any EternalTerminalRemoteSessionKilling,
        sessionPreparer: any EternalTerminalSessionPreparing
    ) {
        self.recordEvent = recordEvent
        self.remoteSessionKiller = remoteSessionKiller
        self.sessionPreparer = sessionPreparer
    }

    @MainActor
    package func record(_ event: EternalTerminalRuntimeEvent) {
        recordEvent(event)
    }

    package func killRemoteSession(
        _ identifier: RemoteSessionIdentifier,
        using client: SSHClient
    ) async {
        await remoteSessionKiller.killSession(identifier, using: client)
    }
}

#if DEBUG
package extension EternalTerminalRuntimeDependencies {
    static package var testing: Self {
        Self(
            recordEvent: { _ in },
            remoteSessionKiller: NoOpEternalTerminalRemoteSessionKiller(),
            sessionPreparer: UnavailableEternalTerminalSessionPreparer()
        )
    }
}

private actor NoOpEternalTerminalRemoteSessionKiller: EternalTerminalRemoteSessionKilling {
    package func killSession(_ identifier: RemoteSessionIdentifier, using client: SSHClient) async {}
}

@MainActor
private struct UnavailableEternalTerminalSessionPreparer: EternalTerminalSessionPreparing {
    package func prepareSession(
        request: EternalTerminalSessionRequest,
        startupPlanProvider: @Sendable @escaping (SSHClient) async throws -> TerminalShellStartupPlan,
        isCurrentOwner: @MainActor @Sendable @escaping () -> Bool
    ) async throws -> PreparedEternalTerminalSession {
        throw CancellationError()
    }

    package func discardResumeState(for paneId: UUID) throws {}
}
#endif
