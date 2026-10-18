import Foundation

nonisolated package struct TerminalOutputStream: AsyncSequence, Sendable {
    package typealias Element = Data

    package struct AsyncIterator: AsyncIteratorProtocol {
        package let channel: TerminalOutputChannel

        mutating package func next() async -> Data? {
            let channel = channel
            return await withTaskCancellationHandler {
                await channel.nextValue()
            } onCancel: {
                Task {
                    await channel.cancel()
                }
            }
        }
    }

    package let channel: TerminalOutputChannel

    package func makeAsyncIterator() -> AsyncIterator {
        AsyncIterator(channel: channel)
    }
}

package actor TerminalOutputChannel {
    package enum OverflowPolicy: Equatable, Sendable {
        case suspendProducer
        case rejectNewData
    }

    package struct Snapshot: Equatable, Sendable {
        package let queuedBytes: Int
        package let queuedItems: Int
        package let pendingSends: Int
    }

    private enum State: Equatable {
        case open
        case finished
        case cancelled
    }

    private struct PendingSend {
        package let id: UUID
        package let data: Data
        package let continuation: CheckedContinuation<Bool, Never>
    }

    static package let standardMaximumQueuedBytes = 1024 * 1024
    static package let standardMaximumQueuedItems = 256
    static package let standardMaximumItemBytes = 64 * 1024

    private let maximumQueuedBytes: Int
    private let maximumQueuedItems: Int
    private let maximumItemBytes: Int
    private let overflowPolicy: OverflowPolicy
    private var state = State.open
    private var queue: [Data] = []
    private var queuedBytes = 0
    private var pendingSend: PendingSend?
    private var waitingConsumer: CheckedContinuation<Data?, Never>?

    package init(
        maximumQueuedBytes: Int = standardMaximumQueuedBytes,
        maximumQueuedItems: Int = standardMaximumQueuedItems,
        maximumItemBytes: Int = standardMaximumItemBytes,
        overflowPolicy: OverflowPolicy = .suspendProducer
    ) {
        self.maximumQueuedBytes = max(1, maximumQueuedBytes)
        self.maximumQueuedItems = max(1, maximumQueuedItems)
        self.maximumItemBytes = max(1, min(maximumItemBytes, maximumQueuedBytes))
        self.overflowPolicy = overflowPolicy
    }

    package func send(_ data: Data) async -> Bool {
        guard !data.isEmpty else { return state == .open }
        guard overflowPolicy != .rejectNewData || data.count <= maximumQueuedBytes else {
            return false
        }

        var offset = 0
        while offset < data.count {
            guard state == .open else { return false }
            let end = min(data.count, offset + maximumItemBytes)
            let accepted = await sendChunk(data.subdata(in: offset..<end))
            guard accepted else { return false }
            offset = end
        }
        return true
    }

    package func nextValue() async -> Data? {
        if !queue.isEmpty {
            let data = queue.removeFirst()
            queuedBytes -= data.count
            admitPendingSends()
            return data
        }

        guard state == .open else { return nil }
        return await withCheckedContinuation { continuation in
            waitingConsumer = continuation
        }
    }

    package func finish() {
        guard state == .open else { return }
        state = .finished
        rejectPendingSends()
        if queue.isEmpty {
            waitingConsumer?.resume(returning: nil)
            waitingConsumer = nil
        }
    }

    package func cancel() {
        guard state != .cancelled else { return }
        state = .cancelled
        queue.removeAll(keepingCapacity: false)
        queuedBytes = 0
        rejectPendingSends()
        waitingConsumer?.resume(returning: nil)
        waitingConsumer = nil
    }

    package func snapshot() -> Snapshot {
        Snapshot(
            queuedBytes: queuedBytes,
            queuedItems: queue.count,
            pendingSends: pendingSend == nil ? 0 : 1
        )
    }

    private func sendChunk(_ data: Data) async -> Bool {
        let sendID = UUID()
        return await withTaskCancellationHandler {
            await withCheckedContinuation { continuation in
                enqueue(
                    PendingSend(id: sendID, data: data, continuation: continuation)
                )
            }
        } onCancel: {
            Task {
                await self.cancelPendingSend(sendID)
            }
        }
    }

    private func enqueue(_ send: PendingSend) {
        guard state == .open else {
            send.continuation.resume(returning: false)
            return
        }

        if let waitingConsumer {
            self.waitingConsumer = nil
            waitingConsumer.resume(returning: send.data)
            send.continuation.resume(returning: true)
            return
        }

        if canQueue(send.data) {
            queue.append(send.data)
            queuedBytes += send.data.count
            send.continuation.resume(returning: true)
            return
        }

        guard overflowPolicy == .suspendProducer, pendingSend == nil else {
            send.continuation.resume(returning: false)
            return
        }
        pendingSend = send
    }

    private func canQueue(_ data: Data) -> Bool {
        guard queue.count < maximumQueuedItems else { return false }
        let (newByteCount, overflow) = queuedBytes.addingReportingOverflow(data.count)
        return !overflow && newByteCount <= maximumQueuedBytes
    }

    private func admitPendingSends() {
        guard let pendingSend, canQueue(pendingSend.data) else { return }
        self.pendingSend = nil
        queue.append(pendingSend.data)
        queuedBytes += pendingSend.data.count
        pendingSend.continuation.resume(returning: true)
    }

    private func cancelPendingSend(_ sendID: UUID) {
        guard let send = pendingSend, send.id == sendID else { return }
        pendingSend = nil
        send.continuation.resume(returning: false)
    }

    private func rejectPendingSends() {
        guard let pendingSend else { return }
        self.pendingSend = nil
        pendingSend.continuation.resume(returning: false)
    }
}
