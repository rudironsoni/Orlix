import Foundation
import MoshCore
import os.log

#if ORLIX_MODULAR_APP
import OrlixSSH
#endif
@MainActor
package struct TerminalMoshClientOperations {
    package let restoreShell: @MainActor @Sendable (
        _ client: SSHClient,
        _ snapshot: MoshSnapshot,
        _ cols: Int,
        _ rows: Int
    ) async throws -> ShellHandle
    package let checkpoint: @MainActor @Sendable (
        _ client: SSHClient,
        _ shellId: UUID
    ) async throws -> MoshSnapshot?
    package let prepareForApplicationBackground: @MainActor @Sendable (
        _ client: SSHClient,
        _ shellId: UUID
    ) async throws -> MoshSnapshot?
    package let resumeFromApplicationBackground: @MainActor @Sendable (
        _ client: SSHClient,
        _ shellId: UUID
    ) async throws -> Void

    static package let live = Self(
        restoreShell: { client, snapshot, cols, rows in
            try await client.restoreMoshShell(
                from: snapshot,
                cols: cols,
                rows: rows
            )
        },
        checkpoint: { client, shellId in
            try await client.moshSnapshot(for: shellId)
        },
        prepareForApplicationBackground: { client, shellId in
            try await client.prepareMoshShellForApplicationBackground(shellId)
        },
        resumeFromApplicationBackground: { client, shellId in
            try await client.resumeMoshShellFromApplicationBackground(shellId)
        }
    )
}

@MainActor
final package class TerminalMoshRecoveryService: TerminalMoshRecoveryServicing {
    private let store: any MoshResumeStoring
    private let client: TerminalMoshClientOperations
    private let logger = Logger(
        subsystem: Bundle.main.bundleIdentifier ?? "Orlix",
        category: "MoshRecovery"
    )

    package init(
        store: any MoshResumeStoring,
        client: TerminalMoshClientOperations = .live
    ) {
        self.store = store
        self.client = client
    }

    package func hasCheckpoint(for paneId: UUID) -> Bool {
        store.hasSnapshot(for: paneId)
    }

    package func restoreShell(
        for paneId: UUID,
        using sshClient: SSHClient,
        cols: Int,
        rows: Int
    ) async -> ShellHandle? {
        let snapshot: MoshSnapshot
        do {
            guard let stored = try store.snapshot(for: paneId) else {
                return nil
            }
            snapshot = stored
        } catch {
            discardCheckpointIfNeeded(after: error, paneId: paneId)
            logger.warning(
                "Unable to load Mosh recovery snapshot: \(error.localizedDescription, privacy: .public)"
            )
            return nil
        }

        do {
            return try await client.restoreShell(
                sshClient,
                snapshot,
                cols,
                rows
            )
        } catch {
            discardCheckpointIfNeeded(after: error, paneId: paneId)
            logger.warning(
                "Unable to restore Mosh session; falling back to bootstrap: \(error.localizedDescription, privacy: .public)"
            )
            return nil
        }
    }

    package func persistCheckpoint(
        for paneId: UUID,
        using sshClient: SSHClient,
        shellId: UUID,
        isCurrentOwner: @MainActor @Sendable @escaping () -> Bool
    ) async {
        do {
            guard let snapshot = try await client.checkpoint(sshClient, shellId) else {
                return
            }
            guard isCurrentOwner() else { return }
            try store.save(snapshot, for: paneId)
        } catch {
            logger.warning(
                "Unable to save Mosh recovery snapshot: \(error.localizedDescription, privacy: .public)"
            )
        }
    }

    package func prepareForApplicationBackground(
        for paneId: UUID,
        using sshClient: SSHClient,
        shellId: UUID,
        isCurrentOwner: @MainActor @Sendable @escaping () -> Bool
    ) async {
        do {
            guard let snapshot = try await client.prepareForApplicationBackground(
                sshClient,
                shellId
            ) else {
                return
            }
            guard isCurrentOwner() else { return }
            try store.save(snapshot, for: paneId)
        } catch {
            logger.warning(
                "Unable to prepare Mosh session for background: \(error.localizedDescription, privacy: .public)"
            )
        }
    }

    package func resumeFromApplicationBackground(
        for paneId: UUID,
        using sshClient: SSHClient,
        shellId: UUID,
        isCurrentOwner: @MainActor @Sendable @escaping () -> Bool
    ) async {
        do {
            try await client.resumeFromApplicationBackground(sshClient, shellId)
        } catch {
            logger.warning(
                "Unable to resume Mosh session from background: \(error.localizedDescription, privacy: .public)"
            )
            return
        }
        guard isCurrentOwner() else { return }
        await persistCheckpoint(
            for: paneId,
            using: sshClient,
            shellId: shellId,
            isCurrentOwner: isCurrentOwner
        )
    }

    package func deleteCheckpoint(for paneId: UUID) throws {
        try store.deleteSnapshot(for: paneId)
    }

    private func discardCheckpointIfNeeded(after error: Error, paneId: UUID) {
        let disposition: MoshStoredStateDisposition
        if let storeError = error as? MoshResumeStoreError {
            disposition = storeError.storedStateDisposition
        } else if let sessionError = error as? MoshSessionError {
            disposition = MoshResumePolicy.storedStateDisposition(after: sessionError)
        } else {
            disposition = .keep
        }
        guard disposition == .discard else { return }
        do {
            try store.deleteSnapshot(for: paneId)
        } catch {
            logger.error(
                "Unable to delete invalid Mosh recovery snapshot: \(error.localizedDescription, privacy: .public)"
            )
        }
    }
}
