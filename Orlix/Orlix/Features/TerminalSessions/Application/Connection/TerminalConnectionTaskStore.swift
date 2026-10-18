import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
import OrlixSSH
#endif
@MainActor
final package class TerminalConnectionTaskStore {
    private struct Entry {
        package let id: UUID
        package let task: Task<Void, Never>
    }

    private var entries: [UUID: Entry] = [:]

    package var paneIds: Set<UUID> {
        Set(entries.keys)
    }

    @discardableResult
    package func start(
        for paneId: UUID,
        operation: @escaping @Sendable (_ taskId: UUID) async -> Void
    ) -> UUID? {
        guard entries[paneId] == nil else { return nil }

        let taskId = UUID()
        let task = Task.detached(priority: .userInitiated) { [weak self] in
            await operation(taskId)
            await self?.finish(taskId: taskId, for: paneId)
        }
        entries[paneId] = Entry(id: taskId, task: task)
        return taskId
    }

    package func isCurrent(taskId: UUID, for paneId: UUID) -> Bool {
        entries[paneId]?.id == taskId
    }

    @discardableResult
    package func cancel(for paneId: UUID) -> Bool {
        guard let entry = entries.removeValue(forKey: paneId) else { return false }
        entry.task.cancel()
        return true
    }

    package func cancelAll() {
        let tasks = entries.values.map(\.task)
        entries.removeAll()
        for task in tasks {
            task.cancel()
        }
    }

    private func finish(taskId: UUID, for paneId: UUID) {
        guard entries[paneId]?.id == taskId else { return }
        entries.removeValue(forKey: paneId)
    }
}

package struct TerminalSSHConnectionContext {
    package let isCurrent: @MainActor @Sendable () -> Bool
    package let updateConnectionState: @MainActor @Sendable (ConnectionState) -> Void
    package let startupPlan: @MainActor @Sendable () async throws -> TerminalShellStartupPlan
    package let restoreMoshShell: @MainActor @Sendable (
        _ cols: Int,
        _ rows: Int
    ) async -> SSHConnectionRestoredShell?
    package let registerShell: @MainActor @Sendable (ShellHandle) async -> Bool
    package let setStartupActionReplayPending: @MainActor @Sendable (Bool) -> Void
    package let remoteSessionAttached: @MainActor @Sendable () -> Void
    package let persistMoshCheckpoint: @MainActor @Sendable (_ shellId: UUID) async -> Void
    package let updateTitle: @MainActor @Sendable (String) -> Void
    package let hasOtherRegistrations: @MainActor @Sendable () async -> Bool
    package let handleShellEnd: @MainActor @Sendable (_ shellId: UUID, _ reason: TerminalShellEndReason) -> Void
    package let handleFailure: @MainActor @Sendable (TerminalConnectionFailure) -> Void
    package let workingDirectory: @MainActor @Sendable () -> String?
}

nonisolated package extension TerminalConnectionFailure {
    static package func transport(_ error: Error) -> Self {
        let message = error.localizedDescription
        guard let sshError = error as? SSHError else {
            return .external(
                message: message,
                retryDisposition: .automatic,
                requiredAction: message.contains("host key approval is required")
                    ? .approveHostKey
                    : nil
            )
        }

        let requiredAction: TerminalConnectionRequiredAction? = switch sshError {
        case .hostKeyApprovalRequired:
            .approveHostKey
        default:
            nil
        }
        return .external(
            message: message,
            retryDisposition: sshError.allowsAutomaticReconnectRetry ? .automatic : .manual,
            requiredAction: requiredAction
        )
    }
}
