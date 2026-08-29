import Foundation
import os.log
@preconcurrency import TrzszSSH

nonisolated struct TSSHStreamLocalChannel: Sendable {
    let reference: Int64
    private let transport: TSSHTransportRef
    private let callGate: TSSHCallGate

    init(
        reference: Int64,
        transport: TSSHTransportRef,
        callGate: TSSHCallGate = .shared
    ) {
        self.reference = reference
        self.transport = transport
        self.callGate = callGate
    }

    func read(maximumBytes: Int = 32 * 1_024) async throws -> Data? {
        try await callGate.streamLocalRead(
            on: transport,
            channelReference: reference,
            maximumBytes: maximumBytes
        )
    }

    func write(_ data: Data) async throws {
        var offset = 0
        while offset < data.count {
            let remaining = data.subdata(in: offset..<data.count)
            let written = try await callGate.streamLocalWrite(
                on: transport,
                channelReference: reference,
                data: remaining
            )
            guard written > 0, written <= remaining.count else {
                throw TSSHRuntimeError.transportFailed(
                    "The Unix-socket channel made no write progress"
                )
            }
            offset += written
        }
    }

    func close() async {
        try? await callGate.streamLocalClose(
            on: transport,
            channelReference: reference
        )
    }
}

nonisolated final class TSSHStreamLocalBridge: NSObject,
    IosbridgeStreamLocalCallbackProtocol, @unchecked Sendable {
    typealias ConnectionHandler = @Sendable (TSSHStreamLocalChannel) async -> Void

    private let transport: TSSHTransportRef
    private let callGate: TSSHCallGate
    private let handler: ConnectionHandler
    private let logger = Logger(
        subsystem: Bundle.main.bundleIdentifier ?? "Orlix",
        category: "TSSHStreamLocalBridge"
    )

    init(
        transport: TSSHTransportRef,
        callGate: TSSHCallGate = .shared,
        handler: @escaping ConnectionHandler
    ) {
        self.transport = transport
        self.callGate = callGate
        self.handler = handler
    }

    func onAccept(_ channelRef: Int64) {
        let channel = TSSHStreamLocalChannel(
            reference: channelRef,
            transport: transport,
            callGate: callGate
        )
        Task {
            await handler(channel)
            await channel.close()
        }
    }

    func onError(_ message: String?) {
        logger.error(
            "TSSH Unix-socket forwarding failed: \(message ?? "Unknown error", privacy: .public)"
        )
    }
}
