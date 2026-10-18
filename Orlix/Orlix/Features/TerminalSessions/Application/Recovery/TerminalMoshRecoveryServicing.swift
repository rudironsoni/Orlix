import Foundation

#if ORLIX_MODULAR_APP
import OrlixSSH
#endif
@MainActor
package protocol TerminalMoshRecoveryServicing: Sendable {
    func hasCheckpoint(for paneId: UUID) -> Bool
    func restoreShell(
        for paneId: UUID,
        using client: SSHClient,
        cols: Int,
        rows: Int
    ) async -> ShellHandle?
    func persistCheckpoint(
        for paneId: UUID,
        using client: SSHClient,
        shellId: UUID,
        isCurrentOwner: @MainActor @Sendable @escaping () -> Bool
    ) async
    func prepareForApplicationBackground(
        for paneId: UUID,
        using client: SSHClient,
        shellId: UUID,
        isCurrentOwner: @MainActor @Sendable @escaping () -> Bool
    ) async
    func resumeFromApplicationBackground(
        for paneId: UUID,
        using client: SSHClient,
        shellId: UUID,
        isCurrentOwner: @MainActor @Sendable @escaping () -> Bool
    ) async
    func deleteCheckpoint(for paneId: UUID) throws
}

#if DEBUG
@MainActor
package struct UnavailableTerminalMoshRecoveryService: TerminalMoshRecoveryServicing {
    package func hasCheckpoint(for paneId: UUID) -> Bool { false }

    package func restoreShell(
        for paneId: UUID,
        using client: SSHClient,
        cols: Int,
        rows: Int
    ) async -> ShellHandle? {
        nil
    }

    package func persistCheckpoint(
        for paneId: UUID,
        using client: SSHClient,
        shellId: UUID,
        isCurrentOwner: @MainActor @Sendable @escaping () -> Bool
    ) async {}

    package func prepareForApplicationBackground(
        for paneId: UUID,
        using client: SSHClient,
        shellId: UUID,
        isCurrentOwner: @MainActor @Sendable @escaping () -> Bool
    ) async {}

    package func resumeFromApplicationBackground(
        for paneId: UUID,
        using client: SSHClient,
        shellId: UUID,
        isCurrentOwner: @MainActor @Sendable @escaping () -> Bool
    ) async {}

    package func deleteCheckpoint(for paneId: UUID) throws {}
}
#endif
