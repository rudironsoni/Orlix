import Foundation
import os.log
@preconcurrency import TrzszSSH

nonisolated struct TSSHTransportRef: Hashable, Sendable { fileprivate let id: UUID }
nonisolated struct TSSHSessionRef: Hashable, Sendable { fileprivate let id: UUID }
nonisolated struct TSSHForwarderRef: Hashable, Sendable { fileprivate let id: UUID }

nonisolated struct TSSHTransportParameters: Sendable {
    let host: String
    let info: TSSHServerInfo
    let mtu: Int
    let debugLabel: String
}

nonisolated struct TSSHForwardParameters: Sendable {
    let id: String
    let direction: String
    let bindAddress: String
    let bindPort: Int
    let targetHost: String
    let targetPort: Int
}

nonisolated struct TSSHTransportStatistics: Equatable, Sendable {
    let mode: String
    let smoothedRTTMilliseconds: Int64
    let bytesSent: Int64
    let bytesReceived: Int64
    let packetsLost: Int64
    let retransmittedSegments: Int64
}

private nonisolated final class TSSHRegistry: @unchecked Sendable {
    struct Storage {
        var transports: [TSSHTransportRef: IosbridgeTransport] = [:]
        var sessions: [TSSHSessionRef: IosbridgeTransportSession] = [:]
        var forwarders: [TSSHForwarderRef: IosbridgePortForwarder] = [:]
    }

    private let lock = NSLock()
    private var storage = Storage()

    func withLock<Result>(_ body: (inout Storage) -> Result) -> Result {
        lock.lock()
        defer { lock.unlock() }
        return body(&storage)
    }
}

actor TSSHCallGate {
    static let shared = TSSHCallGate()

    private nonisolated let queue = DispatchQueue(
        label: "com.rudironsoni.orlix.tssh.native",
        qos: .userInitiated,
        attributes: .concurrent
    )
    private nonisolated let registry = TSSHRegistry()

    private init() {}

    private nonisolated func perform<Result: Sendable>(
        _ operation: @Sendable @escaping () throws -> Result
    ) async throws -> Result {
        try await withCheckedThrowingContinuation { continuation in
            queue.async {
                do { continuation.resume(returning: try operation()) }
                catch { continuation.resume(throwing: error) }
            }
        }
    }

    func connect(_ parameters: TSSHTransportParameters) async throws -> TSSHTransportRef {
        guard let config = IosbridgeNewTransportConfig() else {
            throw TSSHRuntimeError.transportFailed("Cannot create native configuration")
        }
        let info = parameters.info
        config.host = parameters.host.contains(":") && !parameters.host.hasPrefix("[")
            ? "[\(parameters.host)]" : parameters.host
        config.port = info.port
        config.serverVersion = info.serverVersion
        config.mode = info.mode.rawValue
        config.clientID = info.clientID
        config.serverID = info.serverID
        config.mtu = parameters.mtu
        config.debugLabel = parameters.debugLabel
        config.proxyKey = info.proxyKeyHex ?? ""
        if let pass = info.kcpPassHex, let salt = info.kcpSaltHex {
            config.setKcpCredentials(pass, salt: salt)
        }
        if let serverCert = info.serverCertHex,
           let clientCert = info.clientCertHex,
           let clientKey = info.clientKeyHex {
            config.setQuicCredentials(
                serverCert,
                clientCert: clientCert,
                clientKey: clientKey
            )
        }
        let validation = config.validate()
        guard validation.isEmpty else {
            throw TSSHRuntimeError.transportFailed(validation)
        }
        nonisolated(unsafe) let nativeConfig = config
        let transport: IosbridgeTransport = try await perform {
            var error: NSError?
            guard let transport = IosbridgeConnectTransport(nativeConfig, &error) else {
                throw TSSHRuntimeError.transportFailed(
                    error?.localizedDescription ?? "Native connection failed"
                )
            }
            return transport
        }
        let reference = TSSHTransportRef(id: UUID())
        registry.withLock { $0.transports[reference] = transport }
        return reference
    }

    func configure(
        _ reference: TSSHTransportRef,
        keepPendingInput: Bool,
        keepPendingOutput: Bool,
        state: TSSHStateBridge,
        health: TSSHHealthBridge,
        discard: TSSHDiscardBridge
    ) async throws {
        guard let transport = registry.withLock({ $0.transports[reference] }) else {
            throw TSSHRuntimeError.transportFailed("Unknown native transport")
        }
        nonisolated(unsafe) let native = transport
        try await perform {
            try native.setKeepPendingInput(keepPendingInput)
            try native.setKeepPendingOutput(keepPendingOutput)
            native.setStateCallback(state)
            native.setHealthNotifier(health)
            native.setDiscardNotifier(discard)
        }
    }

    func enableAgent(
        _ callback: TSSHAgentBridge,
        on reference: TSSHTransportRef
    ) async throws {
        guard let transport = registry.withLock({ $0.transports[reference] }) else {
            throw TSSHRuntimeError.transportFailed("Unknown native transport")
        }
        nonisolated(unsafe) let native = transport
        try await perform { try native.enableAgentForwarding(callback) }
    }

    func openSession(
        on transportReference: TSSHTransportRef,
        term: String,
        rows: Int,
        columns: Int,
        requestAgent: Bool,
        command: String?,
        output: TSSHOutputBridge
    ) async throws -> (TSSHSessionRef, Int64) {
        guard let transport = registry.withLock({ $0.transports[transportReference] }) else {
            throw TSSHRuntimeError.transportFailed("Unknown native transport")
        }
        nonisolated(unsafe) let native = transport
        let session: IosbridgeTransportSession = try await perform {
            let session = try native.newSession()
            session.setOutputCallback(output)
            if requestAgent { try session.requestAgentForwarding() }
            try session.requestPty(term, rows: rows, cols: columns)
            if let command { try session.startCommand(command) }
            else { try session.shell() }
            return session
        }
        let reference = TSSHSessionRef(id: UUID())
        registry.withLock { $0.sessions[reference] = session }
        return (reference, session.getID())
    }

    func attachSession(
        on transportReference: TSSHTransportRef,
        sessionID: Int64,
        term: String,
        rows: Int,
        columns: Int,
        output: TSSHOutputBridge
    ) async throws -> (TSSHSessionRef, Int64) {
        guard let transport = registry.withLock({ $0.transports[transportReference] }) else {
            throw TSSHRuntimeError.transportFailed("Unknown native transport")
        }
        nonisolated(unsafe) let native = transport
        let session: IosbridgeTransportSession = try await perform {
            let session = try native.attachSession(
                sessionID,
                term: term,
                rows: rows,
                cols: columns
            )
            session.setOutputCallback(output)
            return session
        }
        let reference = TSSHSessionRef(id: UUID())
        registry.withLock { $0.sessions[reference] = session }
        return (reference, session.getID())
    }

    func write(_ data: Data, to reference: TSSHSessionRef) async throws {
        guard let session = registry.withLock({ $0.sessions[reference] }) else { return }
        nonisolated(unsafe) let native = session
        try await perform { try native.write(data) }
    }

    func resize(_ reference: TSSHSessionRef, rows: Int, columns: Int) async throws {
        guard let session = registry.withLock({ $0.sessions[reference] }) else { return }
        nonisolated(unsafe) let native = session
        try await perform { try native.windowChange(rows, cols: columns) }
    }

    func closeSession(_ reference: TSSHSessionRef) async {
        guard let session = registry.withLock({ $0.sessions.removeValue(forKey: reference) }) else {
            return
        }
        nonisolated(unsafe) let native = session
        _ = try? await perform { try native.close() }
    }

    nonisolated func forgetSession(_ reference: TSSHSessionRef) {
        _ = registry.withLock { $0.sessions.removeValue(forKey: reference) }
    }

    func closeTransport(_ reference: TSSHTransportRef, preserveServer: Bool) async {
        guard let transport = registry.withLock({ $0.transports[reference] }) else {
            return
        }
        nonisolated(unsafe) let native = transport
        if preserveServer {
            _ = registry.withLock { $0.transports.removeValue(forKey: reference) }
            await performWithoutThrowing { native.abandon() }
        } else {
            let registry = self.registry
            let queue = self.queue
            let fallback = Task {
                try? await Task.sleep(for: .seconds(2))
                guard !Task.isCancelled,
                      registry.withLock({ $0.transports[reference] != nil }) else { return }
                queue.async { native.abandon() }
            }
            _ = try? await perform { try native.close() }
            fallback.cancel()
            _ = registry.withLock { $0.transports.removeValue(forKey: reference) }
        }
    }

    private nonisolated func performWithoutThrowing(
        _ operation: @Sendable @escaping () -> Void
    ) async {
        await withCheckedContinuation { continuation in
            queue.async {
                operation()
                continuation.resume()
            }
        }
    }

    func statistics(for reference: TSSHTransportRef) async -> TSSHTransportStatistics? {
        guard let transport = registry.withLock({ $0.transports[reference] }) else { return nil }
        nonisolated(unsafe) let native = transport
        return try? await perform {
            guard let stats = native.getStats() else {
                throw TSSHRuntimeError.transportFailed("Statistics unavailable")
            }
            return TSSHTransportStatistics(
                mode: stats.mode,
                smoothedRTTMilliseconds: stats.srttMs,
                bytesSent: stats.bytesSent,
                bytesReceived: stats.bytesReceived,
                packetsLost: stats.packetsLost,
                retransmittedSegments: stats.retransSegs
            )
        }
    }

    func isAlive(_ reference: TSSHTransportRef) async -> Bool {
        guard let transport = registry.withLock({ $0.transports[reference] }) else {
            return false
        }
        nonisolated(unsafe) let native = transport
        return (try? await perform { !native.isClosed() }) ?? false
    }

    func createForwarder(
        on transportReference: TSSHTransportRef,
        callback: TSSHForwardBridge
    ) async throws -> TSSHForwarderRef {
        guard let transport = registry.withLock({ $0.transports[transportReference] }) else {
            throw TSSHRuntimeError.transportFailed("Unknown native transport")
        }
        nonisolated(unsafe) let native = transport
        let forwarder: IosbridgePortForwarder = try await perform {
            var error: NSError?
            guard let forwarder = IosbridgeNewPortForwarder(native, callback, &error) else {
                throw TSSHRuntimeError.transportFailed(
                    error?.localizedDescription ?? "Cannot create port forwarder"
                )
            }
            return forwarder
        }
        let reference = TSSHForwarderRef(id: UUID())
        registry.withLock { $0.forwarders[reference] = forwarder }
        return reference
    }

    func startForward(
        _ parameters: TSSHForwardParameters,
        on reference: TSSHForwarderRef
    ) async throws {
        guard let forwarder = registry.withLock({ $0.forwarders[reference] }) else {
            throw TSSHRuntimeError.transportFailed("Unknown native forwarder")
        }
        nonisolated(unsafe) let native = forwarder
        try await perform {
            guard let config = IosbridgeNewForwardConfig() else {
                throw TSSHRuntimeError.transportFailed("Cannot create forward configuration")
            }
            config.forwardID = parameters.id
            config.direction = parameters.direction
            config.bindAddress = parameters.bindAddress
            config.bindPort = parameters.bindPort
            config.targetHost = parameters.targetHost
            config.targetPort = parameters.targetPort
            try native.startForward(config)
        }
    }

    func stopForward(_ id: String, on reference: TSSHForwarderRef) async {
        guard let forwarder = registry.withLock({ $0.forwarders[reference] }) else { return }
        nonisolated(unsafe) let native = forwarder
        await performWithoutThrowing { native.stopForward(id) }
    }

    func closeForwarder(_ reference: TSSHForwarderRef) async {
        guard let forwarder = registry.withLock({ $0.forwarders.removeValue(forKey: reference) }) else {
            return
        }
        nonisolated(unsafe) let native = forwarder
        await performWithoutThrowing { native.close() }
    }

    nonisolated func emergencyCloseForwarder(_ reference: TSSHForwarderRef) {
        guard let forwarder = registry.withLock({ $0.forwarders.removeValue(forKey: reference) }) else {
            return
        }
        queue.async { forwarder.close() }
    }

    func runCommand(_ command: String, on reference: TSSHTransportRef) async throws -> Data {
        guard let transport = registry.withLock({ $0.transports[reference] }) else {
            throw TSSHRuntimeError.transportFailed("Unknown native transport")
        }
        nonisolated(unsafe) let native = transport
        return try await perform { try native.runCommand(command) }
    }

    func enableStreamLocalForwarding(
        on reference: TSSHTransportRef,
        remotePath: String,
        callback: TSSHStreamLocalBridge
    ) async throws {
        guard let transport = registry.withLock({ $0.transports[reference] }) else {
            throw TSSHRuntimeError.transportFailed("Unknown native transport")
        }
        nonisolated(unsafe) let native = transport
        try await perform {
            try native.enableStreamLocalForwarding(remotePath, callback: callback)
        }
    }

    func disableStreamLocalForwarding(
        on reference: TSSHTransportRef,
        remotePath: String
    ) async throws {
        guard let transport = registry.withLock({ $0.transports[reference] }) else { return }
        nonisolated(unsafe) let native = transport
        try await perform { try native.disableStreamLocalForwarding(remotePath) }
    }

    func streamLocalRead(
        on reference: TSSHTransportRef,
        channelReference: Int64,
        maximumBytes: Int
    ) async throws -> Data? {
        guard let transport = registry.withLock({ $0.transports[reference] }) else {
            throw TSSHRuntimeError.transportFailed("Unknown native transport")
        }
        nonisolated(unsafe) let native = transport
        let boundedMaximum = Int32(max(1, min(maximumBytes, Int(Int32.max))))
        return try await perform {
            let data = try native.streamLocalRead(
                channelReference,
                maxBytes: boundedMaximum
            )
            return data.isEmpty ? nil : data
        }
    }

    func streamLocalWrite(
        on reference: TSSHTransportRef,
        channelReference: Int64,
        data: Data
    ) async throws -> Int {
        guard let transport = registry.withLock({ $0.transports[reference] }) else {
            throw TSSHRuntimeError.transportFailed("Unknown native transport")
        }
        nonisolated(unsafe) let native = transport
        return try await perform {
            var written: Int32 = 0
            try native.streamLocalWrite(
                channelReference,
                data: data,
                ret0_: &written
            )
            return Int(written)
        }
    }

    func streamLocalClose(
        on reference: TSSHTransportRef,
        channelReference: Int64
    ) async throws {
        guard let transport = registry.withLock({ $0.transports[reference] }) else { return }
        nonisolated(unsafe) let native = transport
        try await perform { try native.streamLocalClose(channelReference) }
    }

    nonisolated func emergencyAbandon(_ reference: TSSHTransportRef) {
        guard let transport = registry.withLock({ $0.transports.removeValue(forKey: reference) }) else {
            return
        }
        queue.async { transport.abandon() }
    }
}
