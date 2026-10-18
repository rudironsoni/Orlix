import Foundation
#if ORLIX_MODULAR_APP
import OrlixModels
import OrlixSSH
#endif

/// Owns transport identities, queued I/O, and cleanup for one terminal manager.
@MainActor
final package class TerminalTransportLifetime {
    package struct SSHResizeState {
        package let clientIdentity: ObjectIdentifier
        package let shellId: UUID
        package let cols: Int
        package let rows: Int
        package let pixelSize: TerminalPixelSize?
    }

    package let registry: TerminalTransportRegistry<EternalTerminalRuntime>

    private var writeQueuesByPane: [UUID: TerminalTransportWriteQueue] = [:]
    private var lastSSHResizeByPane: [UUID: SSHResizeState] = [:]

    package init(staleShellStartThreshold: TimeInterval = 120) {
        registry = TerminalTransportRegistry(
            staleShellStartThreshold: staleShellStartThreshold
        )
    }

    isolated deinit {
        let drainedTransports = drain()
        for client in drainedTransports.clients {
            Task { await client.disconnect() }
        }
        for runtime in drainedTransports.runtimes {
            Task { @MainActor in await runtime.close() }
        }
    }

    package func writeQueue(for paneId: UUID) -> TerminalTransportWriteQueue {
        if let queue = writeQueuesByPane[paneId] {
            return queue
        }
        let queue = TerminalTransportWriteQueue()
        writeQueuesByPane[paneId] = queue
        return queue
    }

    package func shouldScheduleResize(
        for paneId: UUID,
        client: SSHClient,
        shellId: UUID,
        cols: Int,
        rows: Int,
        pixelSize: TerminalPixelSize?
    ) -> Bool {
        let state = SSHResizeState(
            clientIdentity: ObjectIdentifier(client),
            shellId: shellId,
            cols: cols,
            rows: rows,
            pixelSize: pixelSize
        )
        if let previous = lastSSHResizeByPane[paneId],
           previous.clientIdentity == state.clientIdentity,
           previous.shellId == state.shellId,
           previous.cols == state.cols,
           previous.rows == state.rows,
           previous.pixelSize == state.pixelSize {
            return false
        }
        lastSSHResizeByPane[paneId] = state
        return true
    }

    package func cancelQueuedIO(for paneId: UUID) {
        writeQueuesByPane.removeValue(forKey: paneId)?.cancel()
        lastSSHResizeByPane.removeValue(forKey: paneId)
    }

    package func unregisterShell(
        for paneId: UUID,
        ifOwnedBy registration: TerminalRemoteSessionShellRegistration
    ) async {
        guard registry.ownsShell(
            client: registration.client,
            shellId: registration.shellID,
            for: paneId
        ) else { return }

        registry.cancelConnectionTask(for: paneId)
        cancelQueuedIO(for: paneId)
        let ownership = registry.unregisterShell(for: paneId)
        guard let removed = ownership.registration else { return }

        await registry.performTrackedCleanup(for: removed.client) {
            if !self.registry.hasClientReferences(removed.client) {
                await removed.client.disconnect()
            } else {
                await removed.client.closeShell(removed.shellId)
            }
        }
    }

    @discardableResult
    package func unregisterRuntime(
        for paneId: UUID,
        ifOwnedBy runtime: EternalTerminalRuntime
    ) async -> Bool {
        guard registry.detachRuntime(runtime, for: paneId) else { return false }
        await runtime.close()
        return true
    }

    package func drain() -> TerminalTransportRegistry<EternalTerminalRuntime>.DrainResult {
        let drainedTransports = registry.drain()
        let queues = Array(writeQueuesByPane.values)
        writeQueuesByPane.removeAll()
        lastSSHResizeByPane.removeAll()
        for queue in queues {
            queue.cancel()
        }
        return drainedTransports
    }
}
