#if ORLIX_TEST_HOST
import OrlixTestImplementation
#else
import OrlixImplementation
#endif
#if ORLIX_TEST_HOST
import OrlixTestTerminal
#else
import OrlixTerminal
#endif
import Foundation

/// Breaks the production initialization cycle between server state and terminal state.
@MainActor
final class ServerDeletionTerminalCleanupRelay {
    private weak var tabManager: TerminalTabManager?

    func bind(to tabManager: TerminalTabManager) {
        self.tabManager = tabManager
    }

    func handleServerDeletion(_ serverID: UUID) {
        tabManager?.disconnectServer(serverID)
    }
}
