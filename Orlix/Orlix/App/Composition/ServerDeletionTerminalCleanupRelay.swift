import Foundation
#if ORLIX_MODULAR_APP
import OrlixTerminal
#endif

/// Breaks the production initialization cycle between server state and terminal state.
@MainActor
final package class ServerDeletionTerminalCleanupRelay {
    private weak var tabManager: TerminalTabManager?

    package func bind(to tabManager: TerminalTabManager) {
        self.tabManager = tabManager
    }

    package func handleServerDeletion(_ serverID: UUID) {
        tabManager?.disconnectServer(serverID)
    }
}
