import Foundation
@preconcurrency import TrzszSSH

nonisolated final class TSSHOutputBridge: NSObject, IosbridgeTSSHOutputCallbackProtocol, @unchecked Sendable {
    private let output: @Sendable (Data) -> Void
    private let failure: @Sendable (String) -> Void
    private let exit: @Sendable (Int) -> Void
    private let close: @Sendable () -> Void

    init(
        output: @escaping @Sendable (Data) -> Void,
        failure: @escaping @Sendable (String) -> Void,
        exit: @escaping @Sendable (Int) -> Void,
        close: @escaping @Sendable () -> Void
    ) {
        self.output = output
        self.failure = failure
        self.exit = exit
        self.close = close
    }

    func onOutput(_ data: Data?) { if let data { output(data) } }
    func onError(_ message: String?) { failure(message ?? "Unknown TSSH error") }
    func onExit(_ exitCode: Int) { exit(exitCode) }
    func onClose() { close() }
}

nonisolated final class TSSHHealthBridge: NSObject, IosbridgeHealthNotifierProtocol, @unchecked Sendable {
    private let transition: @Sendable (Bool, Int64) -> Void
    init(transition: @escaping @Sendable (Bool, Int64) -> Void) {
        self.transition = transition
    }
    func onHealthTimeout(_ lastActiveMs: Int64) { transition(false, lastActiveMs) }
    func onHealthRecovered(_ lastActiveMs: Int64) { transition(true, lastActiveMs) }
}

nonisolated final class TSSHStateBridge: NSObject, IosbridgeTransportStateCallbackProtocol, @unchecked Sendable {
    nonisolated enum Event: Sendable {
        case state(String)
        case reconnecting(Int)
        case failed(String)
    }

    private let event: @Sendable (Event) -> Void

    init(event: @escaping @Sendable (Event) -> Void) {
        self.event = event
    }

    func onStateChange(_ state: String?) {
        event(.state(state ?? "disconnected"))
    }

    func onReconnecting(_ attempt: Int) {
        event(.reconnecting(attempt))
    }

    func onError(_ message: String?) {
        event(.failed(message ?? "Unknown TSSH transport error"))
    }
}

nonisolated final class TSSHDiscardBridge: NSObject, IosbridgeDiscardNotifierProtocol, @unchecked Sendable {
    private let discard: @Sendable (Int, Int, Int) -> Void
    init(discard: @escaping @Sendable (Int, Int, Int) -> Void) { self.discard = discard }
    func onDiscard(_ inputBytes: Int, outputLines: Int, outputBytes: Int) {
        discard(inputBytes, outputLines, outputBytes)
    }
}

nonisolated final class TSSHForwardBridge: NSObject, IosbridgeForwardCallbackProtocol, @unchecked Sendable {
    nonisolated enum Event: Sendable {
        case ready(String, Int)
        case failed(String, String)
        case stopped(String)
        case opened(String, Int64)
        case closed(String, Int64, Int64, Int64)
    }

    private let event: @Sendable (Event) -> Void
    init(event: @escaping @Sendable (Event) -> Void) { self.event = event }

    func onForwardReady(_ id: String?, actualPort: Int) {
        event(.ready(id ?? "", actualPort))
    }
    func onForwardError(_ id: String?, message: String?) {
        event(.failed(id ?? "", message ?? "Unknown forwarding error"))
    }
    func onForwardStopped(_ id: String?) { event(.stopped(id ?? "")) }
    func onConnectionOpened(_ id: String?, connectionID: Int64) {
        event(.opened(id ?? "", connectionID))
    }
    func onConnectionClosed(
        _ id: String?,
        connectionID: Int64,
        bytesIn: Int64,
        bytesOut: Int64
    ) {
        event(.closed(id ?? "", connectionID, bytesIn, bytesOut))
    }
}
