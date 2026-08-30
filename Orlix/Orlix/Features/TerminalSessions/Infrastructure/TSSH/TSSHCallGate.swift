import Foundation
import os.log
@preconcurrency import TrzszSSH

nonisolated struct TSSHTransportRef: Hashable, Sendable { fileprivate let id: UUID }
nonisolated struct TSSHSessionRef: Hashable, Sendable { fileprivate let id: UUID }
nonisolated struct TSSHForwarderRef: Hashable, Sendable { fileprivate let id: UUID }
nonisolated struct TSSHAuxiliaryStreamRef: Hashable, Sendable { fileprivate let id: UUID }
nonisolated struct TSSHExecRef: Hashable, Sendable { fileprivate let id: UUID }

nonisolated struct TSSHTransportParameters: Sendable {
    let host: String
    let info: TSSHServerInfo
    let mtu: Int
    let connectTimeoutSeconds: Int
    let aliveTimeoutSeconds: Int
    let heartbeatTimeoutSeconds: Int
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
    let rttVarianceMilliseconds: Int64
    let minimumRTTMilliseconds: Int64
    let latestRTTMilliseconds: Int64
    let retransmissionTimeoutMilliseconds: Int64
    let bytesSent: Int64
    let bytesReceived: Int64
    let packetsSent: Int64
    let packetsReceived: Int64
    let bytesLost: Int64
    let packetsLost: Int64
    let retransmittedSegments: Int64
    let hasMinimumRTT: Bool
    let hasRetransmissionTimeout: Bool
    let hasLoss: Bool
    let retransmissionsAreGlobal: Bool
}

nonisolated struct TSSHTransportHealth: Equatable, Sendable {
    let mode: String
    let isAlive: Bool
    let isTimedOut: Bool
    let lastActiveAt: Date?
    let lastReconnectError: String?
}

private nonisolated final class TSSHCancellableContinuation<Value: Sendable>: @unchecked Sendable {
    private let lock = NSLock()
    private let discardLateValue: @Sendable (Value) -> Void
    private var continuation: CheckedContinuation<Value, any Error>?
    private var isCancelled = false
    private var isComplete = false

    init(discardLateValue: @Sendable @escaping (Value) -> Void) {
        self.discardLateValue = discardLateValue
    }

    func install(_ continuation: CheckedContinuation<Value, any Error>) {
        lock.lock()
        if isCancelled {
            lock.unlock()
            continuation.resume(throwing: CancellationError())
            return
        }
        self.continuation = continuation
        lock.unlock()
    }

    func cancel() {
        let continuation: CheckedContinuation<Value, any Error>?
        lock.lock()
        guard !isComplete, !isCancelled else {
            lock.unlock()
            return
        }
        isCancelled = true
        continuation = self.continuation
        self.continuation = nil
        lock.unlock()
        continuation?.resume(throwing: CancellationError())
    }

    func complete(_ result: Result<Value, any Error>) {
        let continuation: CheckedContinuation<Value, any Error>?
        let lateValue: Value?
        let wasCancelled: Bool
        lock.lock()
        guard !isComplete else {
            lock.unlock()
            return
        }
        isComplete = true
        continuation = self.continuation
        self.continuation = nil
        wasCancelled = isCancelled
        lateValue = wasCancelled ? try? result.get() : nil
        lock.unlock()
        if let lateValue { discardLateValue(lateValue) }
        if !wasCancelled { continuation?.resume(with: result) }
    }
}

nonisolated func tsshPerformCancellable<Value: Sendable>(
    on queue: DispatchQueue,
    operation: @Sendable @escaping () throws -> Value,
    discardLateValue: @Sendable @escaping (Value) -> Void
) async throws -> Value {
    let state = TSSHCancellableContinuation(discardLateValue: discardLateValue)
    return try await withTaskCancellationHandler {
        let value = try await withCheckedThrowingContinuation { continuation in
            state.install(continuation)
            queue.async {
                do { state.complete(.success(try operation())) }
                catch { state.complete(.failure(error)) }
            }
        }
        do {
            try Task.checkCancellation()
            return value
        } catch {
            discardLateValue(value)
            throw error
        }
    } onCancel: {
        state.cancel()
    }
}

private nonisolated final class TSSHRegistry: @unchecked Sendable {
    struct Storage {
        var transports: [TSSHTransportRef: IosbridgeTransport] = [:]
        var sessions: [TSSHSessionRef: IosbridgeTransportSession] = [:]
        var forwarders: [TSSHForwarderRef: IosbridgePortForwarder] = [:]
        var auxiliaryStreams: [TSSHAuxiliaryStreamRef: (IosbridgeTransport, Int64)] = [:]
        var execs: [TSSHExecRef: (IosbridgeTransport, Int64)] = [:]
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
        config.clientID = Int64(bitPattern: info.clientID)
        config.serverID = Int64(bitPattern: info.serverID)
        config.proxyMode = info.proxyMode
        config.mtu = parameters.mtu > 0 ? parameters.mtu : info.mtu
        config.connectTimeoutSec = parameters.connectTimeoutSeconds
        config.aliveTimeoutSec = parameters.aliveTimeoutSeconds
        config.heartbeatTimeoutSec = parameters.heartbeatTimeoutSeconds
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
        let transport: IosbridgeTransport = try await tsshPerformCancellable(
            on: queue,
            operation: {
                var error: NSError?
                guard let transport = IosbridgeConnectTransport(nativeConfig, &error) else {
                    throw TSSHRuntimeError.transportFailed(
                        error?.localizedDescription ?? "Native connection failed"
                    )
                }
                return transport
            },
            discardLateValue: { $0.abandon() }
        )
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
        registry.withLock { storage in
            storage.auxiliaryStreams = storage.auxiliaryStreams.filter { $0.value.0 !== transport }
            storage.execs = storage.execs.filter { $0.value.0 !== transport }
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
                rttVarianceMilliseconds: stats.rttVarMs,
                minimumRTTMilliseconds: stats.minRttMs,
                latestRTTMilliseconds: stats.latestRttMs,
                retransmissionTimeoutMilliseconds: stats.rtoMs,
                bytesSent: stats.bytesSent,
                bytesReceived: stats.bytesReceived,
                packetsSent: stats.packetsSent,
                packetsReceived: stats.packetsReceived,
                bytesLost: stats.bytesLost,
                packetsLost: stats.packetsLost,
                retransmittedSegments: stats.retransSegs,
                hasMinimumRTT: stats.hasMinRtt,
                hasRetransmissionTimeout: stats.hasRto,
                hasLoss: stats.hasLoss,
                retransmissionsAreGlobal: stats.retransIsGlobal
            )
        }
    }

    func health(for reference: TSSHTransportRef) async -> TSSHTransportHealth? {
        guard let transport = registry.withLock({ $0.transports[reference] }) else { return nil }
        nonisolated(unsafe) let native = transport
        return try? await perform {
            let reconnectError: String?
            do {
                try native.getLastReconnectError()
                reconnectError = nil
            } catch {
                reconnectError = error.localizedDescription
            }
            let milliseconds = native.getLastActiveTime()
            return TSSHTransportHealth(
                mode: native.getMode(),
                isAlive: !native.isClosed(),
                isTimedOut: native.isTimeout(),
                lastActiveAt: milliseconds > 0
                    ? Date(timeIntervalSince1970: TimeInterval(milliseconds) / 1_000)
                    : nil,
                lastReconnectError: reconnectError
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

    func dialTCP(host: String, port: Int, on reference: TSSHTransportRef) async throws -> TSSHAuxiliaryStreamRef {
        guard !host.isEmpty, (1...65_535).contains(port),
              let transport = registry.withLock({ $0.transports[reference] }) else {
            throw TSSHRuntimeError.transportFailed("Invalid TCP dial request")
        }
        nonisolated(unsafe) let native = transport
        let channel: Int64 = try await perform {
            var channel: Int64 = 0
            try native.dialTCP(host, port: port, ret0_: &channel)
            return channel
        }
        let stream = TSSHAuxiliaryStreamRef(id: UUID())
        registry.withLock { $0.auxiliaryStreams[stream] = (transport, channel) }
        return stream
    }

    func dialUnix(path: String, on reference: TSSHTransportRef) async throws -> TSSHAuxiliaryStreamRef {
        guard !path.isEmpty, path.utf8.count <= 1_024,
              let transport = registry.withLock({ $0.transports[reference] }) else {
            throw TSSHRuntimeError.transportFailed("Invalid Unix socket dial request")
        }
        nonisolated(unsafe) let native = transport
        let channel: Int64 = try await perform {
            var channel: Int64 = 0
            try native.dialUnix(path, ret0_: &channel)
            return channel
        }
        let stream = TSSHAuxiliaryStreamRef(id: UUID())
        registry.withLock { $0.auxiliaryStreams[stream] = (transport, channel) }
        return stream
    }

    func read(_ reference: TSSHAuxiliaryStreamRef, maximumBytes: Int) async throws -> Data? {
        guard let (transport, channel) = registry.withLock({ $0.auxiliaryStreams[reference] }) else {
            throw TSSHRuntimeError.transportFailed("Unknown auxiliary stream")
        }
        nonisolated(unsafe) let native = transport
        let boundedMaximum = Int32(max(1, min(maximumBytes, 1_048_576)))
        return try await perform {
            let data = try native.streamLocalRead(channel, maxBytes: boundedMaximum)
            return data.isEmpty ? nil : data
        }
    }

    func write(_ data: Data, to reference: TSSHAuxiliaryStreamRef) async throws -> Int {
        guard data.count <= 1_048_576,
              let (transport, channel) = registry.withLock({ $0.auxiliaryStreams[reference] }) else {
            throw TSSHRuntimeError.transportFailed("Invalid auxiliary stream write")
        }
        nonisolated(unsafe) let native = transport
        return try await perform {
            var written: Int32 = 0
            try native.streamLocalWrite(channel, data: data, ret0_: &written)
            return Int(written)
        }
    }

    func close(_ reference: TSSHAuxiliaryStreamRef) async {
        guard let (transport, channel) = registry.withLock({ $0.auxiliaryStreams.removeValue(forKey: reference) }) else { return }
        nonisolated(unsafe) let native = transport
        _ = try? await perform { try native.streamLocalClose(channel) }
    }

    func openExec(_ command: String, on reference: TSSHTransportRef) async throws -> TSSHExecRef {
        guard !command.isEmpty, command.utf8.count <= 65_536,
              let transport = registry.withLock({ $0.transports[reference] }) else {
            throw TSSHRuntimeError.transportFailed("Invalid exec request")
        }
        nonisolated(unsafe) let native = transport
        let handle: Int64 = try await perform {
            var handle: Int64 = 0
            try native.openExec(command, ret0_: &handle)
            return handle
        }
        let exec = TSSHExecRef(id: UUID())
        registry.withLock { $0.execs[exec] = (transport, handle) }
        return exec
    }

    func readExec(_ reference: TSSHExecRef, standardError: Bool, maximumBytes: Int) async throws -> Data? {
        guard let (transport, handle) = registry.withLock({ $0.execs[reference] }) else {
            throw TSSHRuntimeError.transportFailed("Unknown exec request")
        }
        nonisolated(unsafe) let native = transport
        let boundedMaximum = max(1, min(maximumBytes, 1_048_576))
        return try await perform {
            do {
                let data = standardError
                    ? try native.execReadStderr(handle, maxBytes: boundedMaximum)
                    : try native.execRead(handle, maxBytes: boundedMaximum)
                return data.isEmpty ? nil : data
            } catch {
                // gomobile reports a nil byte slice plus nil Go error as
                // `nilError`. A completed exec means this is clean EOF.
                if native.execExitCode(handle) >= 0 || String(describing: error) == "nilError" {
                    return nil
                }
                throw error
            }
        }
    }

    func writeExec(_ data: Data, to reference: TSSHExecRef) async throws -> Int {
        guard data.count <= 1_048_576,
              let (transport, handle) = registry.withLock({ $0.execs[reference] }) else {
            throw TSSHRuntimeError.transportFailed("Invalid exec write")
        }
        nonisolated(unsafe) let native = transport
        return try await perform {
            var written: Int32 = 0
            try native.execWrite(handle, data: data, ret0_: &written)
            return Int(written)
        }
    }

    func closeExecInput(_ reference: TSSHExecRef) async throws {
        guard let (transport, handle) = registry.withLock({ $0.execs[reference] }) else { return }
        nonisolated(unsafe) let native = transport
        try await perform { try native.execCloseStdin(handle) }
    }

    func execExitCode(_ reference: TSSHExecRef) async -> Int? {
        guard let (transport, handle) = registry.withLock({ $0.execs[reference] }) else { return nil }
        nonisolated(unsafe) let native = transport
        return try? await perform { native.execExitCode(handle) }
    }

    func closeExec(_ reference: TSSHExecRef) async {
        guard let (transport, handle) = registry.withLock({ $0.execs.removeValue(forKey: reference) }) else { return }
        nonisolated(unsafe) let native = transport
        _ = try? await perform { try native.execClose(handle) }
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
