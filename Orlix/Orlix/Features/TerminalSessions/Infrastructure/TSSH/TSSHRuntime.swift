import Foundation
import os.log

@MainActor
struct TSSHRuntimeOwnerAccess {
    let isCurrent: (_ paneID: UUID, _ token: UUID) -> Bool
    let updateConnectionState: (_ paneID: UUID, _ state: ConnectionState) -> Void
    let markTransport: (_ paneID: UUID) -> Void
    let handleShellEnd: (_ paneID: UUID, _ token: UUID) -> Void
}

nonisolated struct TSSHForwardStatus: Equatable, Sendable {
    enum State: Equatable, Sendable { case starting, ready, failed(String), stopped }

    let id: UUID
    var state: State
    var actualPort: Int
    var activeConnections: Int
    var bytesIn: Int64
    var bytesOut: Int64
}

nonisolated enum TSSHResumeFailurePolicy {
    static let maximumAttempts = 3

    static func shouldDiscard(after attempt: Int, errorDescription: String) -> Bool {
        if attempt >= maximumAttempts { return true }
        let description = errorDescription.lowercased()
        return description.contains("session [") && (
            description.contains("not found")
                || description.contains("is closed")
                || description.contains("attach is not allowed")
                || description.contains("not a pty session")
                || description.contains("invalid session state")
        )
    }
}

@MainActor
final class TSSHRuntime {
    let paneID: UUID
    let identityToken = UUID()
    private let server: Server
    private let credentials: ServerCredentials
    private let sshClientFactory: SSHClientFactory
    private let resumeStore: any TSSHResumeStoring
    private let callGate: TSSHCallGate
    private let ownerAccess: TSSHRuntimeOwnerAccess
    private let logger = Logger(
        subsystem: Bundle.main.bundleIdentifier ?? "Orlix",
        category: "TSSHRuntime"
    )

    private weak var surface: (any TerminalSurface)?
    private var columns = 80
    private var rows = 24
    private var startTask: Task<Void, Never>?
    private var writeTask: Task<Void, Never>?
    private var transport: TSSHTransportRef?
    private var session: TSSHSessionRef?
    private var forwarder: TSSHForwarderRef?
    private var resumeState: TSSHResumeState?
    private var outputBridge: TSSHOutputBridge?
    private var stateBridge: TSSHStateBridge?
    private var healthBridge: TSSHHealthBridge?
    private var discardBridge: TSSHDiscardBridge?
    private var forwardBridge: TSSHForwardBridge?
    private var agentBridge: TSSHAgentBridge?
    private var forwardStatuses: [UUID: TSSHForwardStatus] = [:]
    private var connectionGeneration = UUID()
    private var isClosing = false

    var isStartInFlight: Bool { startTask != nil }
    var hasLiveTransport: Bool { transport != nil }

    init(
        paneID: UUID,
        server: Server,
        credentials: ServerCredentials,
        sshClientFactory: SSHClientFactory,
        resumeStore: any TSSHResumeStoring = TSSHResumeStore.shared,
        callGate: TSSHCallGate = .shared,
        ownerAccess: TSSHRuntimeOwnerAccess
    ) {
        self.paneID = paneID
        self.server = server
        self.credentials = credentials
        self.sshClientFactory = sshClientFactory
        self.resumeStore = resumeStore
        self.callGate = callGate
        self.ownerAccess = ownerAccess
    }

    func attach(to surface: any TerminalSurface) {
        self.surface = surface
        if transport != nil {
            ownerAccess.markTransport(paneID)
            ownerAccess.updateConnectionState(paneID, .connected)
        }
    }

    func resize(cols: Int, rows: Int, pixelSize: TerminalPixelSize?) {
        guard cols > 0, rows > 0 else { return }
        columns = cols
        self.rows = rows
        guard let session else { return }
        Task {
            do { try await callGate.resize(session, rows: rows, columns: cols) }
            catch { logger.warning("TSSH resize failed: \(error.localizedDescription, privacy: .public)") }
        }
    }

    func startIfNeeded() {
        guard !isClosing, transport == nil, startTask == nil else { return }
        ownerAccess.markTransport(paneID)
        ownerAccess.updateConnectionState(paneID, .connecting)
        startTask = Task { [weak self] in
            guard let self else { return }
            await self.start()
            self.startTask = nil
        }
    }

    func send(_ data: Data) {
        guard let session else { return }
        let precedingWrite = writeTask
        writeTask = Task { [weak self] in
            _ = await precedingWrite?.result
            guard let self, !Task.isCancelled, !self.isClosing else { return }
            do { try await self.callGate.write(data, to: session) }
            catch { await self.reportFailure(error) }
        }
    }

    func prepareForApplicationBackground() async {
        agentBridge?.suspend()
        guard let resumeState else { return }
        do { try resumeStore.save(resumeState, for: paneID) }
        catch { logger.error("Cannot persist TSSH resume state: \(error.localizedDescription, privacy: .public)") }
        guard !server.tsshProfile.keepTunnelsInBackground,
              let forwarder else { return }
        callGate.emergencyCloseForwarder(forwarder)
        self.forwarder = nil
        forwardBridge = nil
    }

    func resumeFromApplicationBackground() async {
        agentBridge?.resume()
        guard let transport else {
            startIfNeeded()
            return
        }
        guard forwarder == nil, !server.tsshProfile.forwards.isEmpty else { return }
        do {
            try await startForwarding(on: transport, profile: server.tsshProfile)
        } catch {
            await reportFailure(error)
        }
    }

    func close(preserveServer: Bool = false, deleteResumeState: Bool = true) async {
        guard !isClosing else { return }
        isClosing = true
        invalidateConnectionGeneration()
        startTask?.cancel()
        startTask = nil
        writeTask?.cancel()
        writeTask = nil
        let transportFallback = transport.map(scheduleTransportFallback)
        if let forwarder { await callGate.closeForwarder(forwarder) }
        if let session {
            if preserveServer { callGate.forgetSession(session) }
            else { await callGate.closeSession(session) }
        }
        if let transport { await callGate.closeTransport(transport, preserveServer: preserveServer) }
        transportFallback?.cancel()
        if deleteResumeState { try? resumeStore.delete(for: paneID) }
        self.forwarder = nil
        self.session = nil
        self.transport = nil
        resumeState = nil
        outputBridge = nil
        stateBridge = nil
        healthBridge = nil
        discardBridge = nil
        forwardBridge = nil
        agentBridge = nil
        ownerAccess.updateConnectionState(paneID, .disconnected)
    }

    func abortConnection() {
        invalidateConnectionGeneration()
        startTask?.cancel()
        startTask = nil
        writeTask?.cancel()
        writeTask = nil
        if let session { callGate.forgetSession(session) }
        if let forwarder { callGate.emergencyCloseForwarder(forwarder) }
        if let transport { callGate.emergencyAbandon(transport) }
        transport = nil
        session = nil
        forwarder = nil
        outputBridge = nil
        stateBridge = nil
        healthBridge = nil
        discardBridge = nil
        forwardBridge = nil
        agentBridge = nil
        resumeState = nil
        try? resumeStore.delete(for: paneID)
    }

    func statistics() async -> TSSHTransportStatistics? {
        guard let transport else { return nil }
        return await callGate.statistics(for: transport)
    }

    func health() async -> TSSHTransportHealth? {
        guard let transport else { return nil }
        return await callGate.health(for: transport)
    }

    func forwardingStatus() -> [TSSHForwardStatus] {
        forwardStatuses.values.sorted { $0.id.uuidString < $1.id.uuidString }
    }

    private func start() async {
        do {
            if try await resumeSavedSession() { return }
            try await startFreshSession()
        } catch is CancellationError {
            await discardFailedStart()
            return
        } catch {
            await discardFailedStart()
            await reportFailure(error)
        }
    }

    private func discardFailedStart() async {
        invalidateConnectionGeneration()
        let transportFallback = transport.map(scheduleTransportFallback)
        if let forwarder { await callGate.closeForwarder(forwarder) }
        if let session { await callGate.closeSession(session) }
        if let transport { await callGate.closeTransport(transport, preserveServer: false) }
        transportFallback?.cancel()
        try? resumeStore.delete(for: paneID)
        self.forwarder = nil
        self.session = nil
        self.transport = nil
        resumeState = nil
        outputBridge = nil
        stateBridge = nil
        healthBridge = nil
        discardBridge = nil
        forwardBridge = nil
        agentBridge = nil
    }

    private func scheduleTransportFallback(
        _ transport: TSSHTransportRef
    ) -> Task<Void, Never> {
        Task { [callGate] in
            try? await Task.sleep(for: .seconds(2))
            guard !Task.isCancelled else { return }
            callGate.emergencyAbandon(transport)
        }
    }

    private func invalidateConnectionGeneration() {
        connectionGeneration = UUID()
    }

    private func resumeSavedSession() async throws -> Bool {
        let loadedState: TSSHResumeState?
        do {
            loadedState = try resumeStore.load(for: paneID)
        } catch {
            try? resumeStore.delete(for: paneID)
            logger.warning(
                "Discarded invalid TSSH resume state: \(error.localizedDescription, privacy: .public)"
            )
            return false
        }
        guard var state = loadedState else { return false }
        let expiresAt = state.savedAt.addingTimeInterval(86_400)
        var backoffSeconds = 1
        var attempt = 0
        while !Task.isCancelled, !isClosing, Date() < expiresAt {
            attempt += 1
            state = TSSHResumeState(
                host: state.host,
                info: state.info.advancingClientID(),
                sessionID: state.sessionID,
                profile: state.profile,
                savedAt: state.savedAt
            )
            try resumeStore.save(state, for: paneID)
            do {
                let transport = try await connect(state.host, info: state.info, profile: state.profile)
                self.transport = transport
                try Task.checkCancellation()
                try await enableAgentIfRequested(profile: state.profile, on: transport)
                let generation = connectionGeneration
                let outputBridge = makeOutputBridge(generation: generation)
                self.outputBridge = outputBridge
                let attached = try await callGate.attachSession(
                    on: transport,
                    sessionID: state.sessionID,
                    term: RemoteTerminalBootstrap.defaultTerminalType.rawValue,
                    rows: rows,
                    columns: columns,
                    output: outputBridge
                )
                session = attached.0
                try Task.checkCancellation()
                resumeState = TSSHResumeState(
                    host: state.host,
                    info: state.info,
                    sessionID: attached.1,
                    profile: state.profile,
                    savedAt: Date()
                )
                try resumeStore.save(resumeState!, for: paneID)
                try await startForwarding(on: transport, profile: state.profile)
                await startVPNIfRequested(profile: state.profile)
                didConnect()
                return true
            } catch {
                let shouldDiscard = TSSHResumeFailurePolicy.shouldDiscard(
                    after: attempt,
                    errorDescription: error.localizedDescription
                )
                await cleanupFailedResume(preserveServer: !shouldDiscard)
                if shouldDiscard {
                    try? resumeStore.delete(for: paneID)
                    logger.info(
                        "Saved TSSH session is unavailable after \(attempt) attempt(s); starting a new session"
                    )
                    return false
                }
                logger.info(
                    "Saved TSSH attach failed, retrying in \(backoffSeconds)s: \(error.localizedDescription, privacy: .public)"
                )
                try await Task.sleep(for: .seconds(backoffSeconds))
                backoffSeconds = min(backoffSeconds * 2, 30)
            }
        }
        try? resumeStore.delete(for: paneID)
        return false
    }

    private func cleanupFailedResume(preserveServer: Bool) async {
        if preserveServer {
            if let session { callGate.forgetSession(session) }
            if let forwarder { callGate.emergencyCloseForwarder(forwarder) }
            if let transport { callGate.emergencyAbandon(transport) }
        } else {
            if let forwarder { await callGate.closeForwarder(forwarder) }
            if let session { await callGate.closeSession(session) }
            if let transport {
                await callGate.closeTransport(transport, preserveServer: false)
            }
        }
        invalidateConnectionGeneration()
        transport = nil
        session = nil
        forwarder = nil
    }

    private func startFreshSession() async throws {
        let sshClient = sshClientFactory.makeClient(
            connectTimeout: .seconds(server.tsshProfile.connectTimeoutSeconds)
        )
        defer { Task { await sshClient.disconnect() } }
        let bootstrap = try await TSSHBootstrap.start(
            server: server,
            credentials: credentials,
            client: sshClient
        )
        try Task.checkCancellation()
        let transport = try await connect(
            bootstrap.host,
            info: bootstrap.info,
            profile: server.tsshProfile
        )
        self.transport = transport
        try Task.checkCancellation()
        try await enableAgentIfRequested(profile: server.tsshProfile, on: transport)

        let generation = connectionGeneration
        let outputBridge = makeOutputBridge(generation: generation)
        self.outputBridge = outputBridge
        let command = server.remoteShellStartupAction?.command
        let opened = try await callGate.openSession(
            on: transport,
            term: RemoteTerminalBootstrap.defaultTerminalType.rawValue,
            rows: rows,
            columns: columns,
            requestAgent: server.tsshProfile.sshAgentForwarding,
            command: command,
            output: outputBridge
        )
        session = opened.0
        try Task.checkCancellation()
        let state = TSSHResumeState(
            host: bootstrap.host,
            info: bootstrap.info,
            sessionID: opened.1,
            profile: server.tsshProfile,
            savedAt: Date()
        )
        try resumeStore.save(state, for: paneID)
        resumeState = state
        try await startForwarding(on: transport, profile: server.tsshProfile)
        await startVPNIfRequested(profile: server.tsshProfile)
        didConnect()
    }

    private func connect(
        _ host: String,
        info: TSSHServerInfo,
        profile: TSSHProfile
    ) async throws -> TSSHTransportRef {
        let generation = UUID()
        connectionGeneration = generation
        let transport = try await callGate.connect(TSSHTransportParameters(
            host: host,
            info: info,
            mtu: profile.mtu,
            connectTimeoutSeconds: profile.connectTimeoutSeconds,
            aliveTimeoutSeconds: profile.aliveTimeoutSeconds,
            heartbeatTimeoutSeconds: profile.heartbeatTimeoutSeconds,
            debugLabel: "\(server.username)@\(server.host)"
        ))
        if Task.isCancelled {
            callGate.emergencyAbandon(transport)
            throw CancellationError()
        }
        let health = TSSHHealthBridge { [weak self] healthy, _ in
            Task { @MainActor [weak self] in
                guard let self,
                      self.isCurrent,
                      self.connectionGeneration == generation else { return }
                self.ownerAccess.updateConnectionState(
                    self.paneID,
                    healthy ? .connected : .reconnecting(attempt: 1)
                )
            }
        }
        let state = TSSHStateBridge { [weak self] event in
            Task { @MainActor [weak self] in
                guard let self,
                      self.isCurrent,
                      self.connectionGeneration == generation,
                      !self.isClosing else { return }
                switch event {
                case .state("connected"):
                    self.ownerAccess.updateConnectionState(self.paneID, .connected)
                case .state("connecting"):
                    self.ownerAccess.updateConnectionState(self.paneID, .connecting)
                case .state("reconnecting"), .state("disconnected"):
                    self.ownerAccess.updateConnectionState(self.paneID, .reconnecting(attempt: 1))
                case .state:
                    break
                case .reconnecting(let attempt):
                    self.ownerAccess.updateConnectionState(
                        self.paneID,
                        .reconnecting(attempt: max(1, attempt))
                    )
                case .failed(let message):
                    await self.reportFailure(TSSHRuntimeError.transportFailed(message))
                }
            }
        }
        let discard = TSSHDiscardBridge { [weak self] inputBytes, outputLines, outputBytes in
            guard inputBytes > 0 || outputLines > 0 || outputBytes > 0 else { return }
            Task { @MainActor [weak self] in
                guard let self, self.connectionGeneration == generation else { return }
                self.logger.warning(
                    "TSSH discarded input=\(inputBytes) outputLines=\(outputLines) outputBytes=\(outputBytes)"
                )
            }
        }
        try await callGate.configure(
            transport,
            keepPendingInput: profile.keepPendingInput,
            keepPendingOutput: profile.keepPendingOutput,
            state: state,
            health: health,
            discard: discard
        )
        if Task.isCancelled {
            callGate.emergencyAbandon(transport)
            throw CancellationError()
        }
        stateBridge = state
        healthBridge = health
        discardBridge = discard
        return transport
    }

    private func startForwarding(
        on transport: TSSHTransportRef,
        profile: TSSHProfile
    ) async throws {
        guard !profile.forwards.isEmpty else { return }
        let generation = connectionGeneration
        let bridge = TSSHForwardBridge { [weak self] event in
            Task { @MainActor [weak self] in
                guard let self, self.connectionGeneration == generation else { return }
                switch event {
                case .ready(let id, let port):
                    self.updateForward(id: id) { status in
                        status.state = .ready
                        status.actualPort = port
                    }
                case .failed(let id, let message):
                    self.updateForward(id: id) { $0.state = .failed(message) }
                    self.logger.error("TSSH forward \(id, privacy: .public) failed: \(message, privacy: .public)")
                case .stopped(let id):
                    self.updateForward(id: id) { $0.state = .stopped }
                case .opened(let id, _):
                    self.updateForward(id: id) { $0.activeConnections += 1 }
                case .closed(let id, _, let bytesIn, let bytesOut):
                    self.updateForward(id: id) { status in
                        status.activeConnections = max(0, status.activeConnections - 1)
                        status.bytesIn += bytesIn
                        status.bytesOut += bytesOut
                    }
                }
            }
        }
        let forwarder = try await callGate.createForwarder(on: transport, callback: bridge)
        self.forwarder = forwarder
        forwardBridge = bridge
        for rule in profile.forwards {
            try Task.checkCancellation()
            forwardStatuses[rule.id] = TSSHForwardStatus(
                id: rule.id,
                state: .starting,
                actualPort: rule.bindPort,
                activeConnections: 0,
                bytesIn: 0,
                bytesOut: 0
            )
            try await callGate.startForward(
                TSSHForwardParameters(
                    id: rule.id.uuidString,
                    direction: rule.direction.rawValue,
                    bindAddress: rule.bindAddress,
                    bindPort: rule.bindPort,
                    targetHost: rule.targetHost,
                    targetPort: rule.targetPort
                ),
                on: forwarder
            )
        }
    }

    private func updateForward(
        id: String,
        _ update: (inout TSSHForwardStatus) -> Void
    ) {
        guard let uuid = UUID(uuidString: id), var status = forwardStatuses[uuid] else { return }
        update(&status)
        forwardStatuses[uuid] = status
    }

    private func enableAgentIfRequested(
        profile: TSSHProfile,
        on transport: TSSHTransportRef
    ) async throws {
        guard profile.sshAgentForwarding else { return }
        let bridge = try TSSHAgentBridge(
            credentials: credentials,
            comment: server.name,
            approvalMode: profile.sshAgentApprovalMode
        )
        try await callGate.enableAgent(bridge, on: transport)
        agentBridge = bridge
    }

    private func startVPNIfRequested(profile: TSSHProfile) async {
        guard profile.vpnEnabled else { return }
        let client = sshClientFactory.makeClient(
            connectTimeout: .seconds(profile.connectTimeoutSeconds)
        )
        do {
            let bootstrap = try await TSSHBootstrap.start(
                server: server,
                credentials: credentials,
                client: client
            )
            await client.disconnect()
            try await TSSHVPNManager.shared.installAndStart(TSSHVPNConfiguration(
                host: bootstrap.host,
                info: bootstrap.info,
                profile: profile
            ))
        } catch {
            await client.disconnect()
            logger.error("TSSH VPN failed to start: \(error.localizedDescription, privacy: .public)")
        }
    }

    private func makeOutputBridge(generation: UUID) -> TSSHOutputBridge {
        TSSHOutputBridge(
            output: { [weak self] data in
                Task { @MainActor [weak self] in
                    guard let self,
                          self.isCurrent,
                          self.connectionGeneration == generation else { return }
                    self.surface?.receiveTerminalOutput(data)
                }
            },
            failure: { [weak self] message in
                Task { @MainActor [weak self] in
                    guard let self,
                          self.connectionGeneration == generation else { return }
                    await self.reportFailure(TSSHRuntimeError.transportFailed(message))
                }
            },
            exit: { [weak self] _ in
                Task { @MainActor [weak self] in
                    guard let self,
                          self.connectionGeneration == generation else { return }
                    self.shellEnded()
                }
            },
            close: { [weak self] in
                Task { @MainActor [weak self] in
                    guard let self,
                          self.connectionGeneration == generation else { return }
                    self.shellEnded()
                }
            }
        )
    }

    private var isCurrent: Bool {
        ownerAccess.isCurrent(paneID, identityToken)
    }

    private func didConnect() {
        guard isCurrent, !isClosing, !Task.isCancelled else { return }
        ownerAccess.markTransport(paneID)
        ownerAccess.updateConnectionState(paneID, .connected)
    }

    private func shellEnded() {
        guard isCurrent, !isClosing else { return }
        ownerAccess.handleShellEnd(paneID, identityToken)
    }

    private func reportFailure(_ error: Error) async {
        guard isCurrent, !isClosing else { return }
        logger.error("TSSH failed: \(error.localizedDescription, privacy: .public)")
        ownerAccess.updateConnectionState(
            paneID,
            .failed(.external(
                message: error.localizedDescription,
                retryDisposition: .automatic,
                requiredAction: nil
            ))
        )
    }
}
