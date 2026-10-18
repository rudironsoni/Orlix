import Foundation

/// Preserves the order in which terminal input reaches an asynchronous
/// transport, even when an individual write suspends.
@MainActor
final package class TerminalTransportWriteQueue {
    private struct Entry {
        package let task: Task<Void, Never>
    }

    private var entries: [UUID: Entry] = [:]
    private var tail: Task<Void, Never>?

    package func enqueue(_ operation: @escaping @MainActor @Sendable () async -> Void) {
        let id = UUID()
        let previousWrite = tail
        let task = Task(priority: .userInitiated) { [weak self] in
            await previousWrite?.value
            guard !Task.isCancelled,
                  self?.entries[id] != nil else { return }
            await operation()
            self?.finish(id)
        }
        entries[id] = Entry(task: task)
        tail = task
    }

    package func cancel() {
        let tasks = entries.values.map(\.task)
        entries.removeAll()
        tail = nil
        for task in tasks {
            task.cancel()
        }
    }

    package func waitForPendingWrites() async {
        await tail?.value
    }

    private func finish(_ id: UUID) {
        entries.removeValue(forKey: id)
    }
}
