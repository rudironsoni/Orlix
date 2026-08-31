import Foundation
import os.log

@MainActor
struct TSSHRuntimeOwnerAccess {
    let isCurrent: (_ paneID: UUID, _ token: UUID) -> Bool
    let startupPlan: (
        _ paneID: UUID,
        _ serverID: UUID,
        _ client: SSHClient,
        _ token: UUID
    ) async throws -> TerminalShellStartupPlan
    let resumeContext: (_ paneID: UUID) -> RemoteSessionLifecycleContext?
    let startupActionReplayPending: (_ paneID: UUID) -> Bool
    let setResumeContext: (_ paneID: UUID, _ context: RemoteSessionLifecycleContext?) -> Void
    let setStartupActionReplayPending: (_ paneID: UUID, _ isPending: Bool) -> Void
    let remoteSessionAttached: (_ paneID: UUID) -> Void
    let updateConnectionState: (_ paneID: UUID, _ state: ConnectionState) -> Void
    let markTransport: (_ paneID: UUID) -> Void
    let handleShellEnd: (
        _ paneID: UUID,
        _ token: UUID,
        _ reason: TerminalShellEndReason
    ) -> Void
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

nonisolated func tsshForwardFailureNotice(id: String, message: String) -> Data {
    Data("\r\n\u{001B}[31mTSSH port forward \(id) failed: \(message)\u{001B}[0m\r\n".utf8)
}

nonisolated enum TSSHResumeLifecyclePolicy {
    static func shouldAwaitStandaloneStartupAction(
        hasRemoteSessionLifecycle: Bool,
        replayPending: Bool
    ) -> Bool {
        !hasRemoteSessionLifecycle && replayPending
    }
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

    static func retryWouldExpire(
        now: Date,
        retryDelaySeconds: Int,
        expiresAt: Date
    ) -> Bool {
        now.addingTimeInterval(TimeInterval(retryDelaySeconds)) >= expiresAt
    }
}

nonisolated struct TSSHTerminalEventGate {
    private var deliveredGeneration: UUID?

    mutating func claim(_ generation: UUID) -> Bool {
        guard deliveredGeneration != generation else { return false }
        deliveredGeneration = generation
        return true
    }
}

nonisolated func tsshPublishedTransportState(
    isHealthy: Bool,
    startupReady: Bool
) -> ConnectionState {
    guard isHealthy else { return .reconnecting(attempt: 1) }
    return startupReady ? .connected : .connecting
}

nonisolated func tsshShouldPreserveCancelledStartServer(
    preservationRequested: Bool,
    hasCheckpoint: Bool
) -> Bool {
    preservationRequested && hasCheckpoint
}

@MainActor
private enum TSSHDeferredCleanup {
    private static let logger = Logger(
        subsystem: Bundle.main.bundleIdentifier ?? "Orlix",
        category: "TSSHDeferredCleanup"
    )

    static func processAll(
        resumeStore: any TSSHResumeStoring,
        sshClientFactory: SSHClientFactory
    ) async {
        for paneID in resumeStore.pendingCleanupPaneIDs() {
            await process(
                paneID: paneID,
                resumeStore: resumeStore,
                sshClientFactory: sshClientFactory,
                fallbackServer: nil,
                fallbackCredentials: nil
            )
        }
    }

    static func process(
        paneID: UUID,
        resumeStore: any TSSHResumeStoring,
        sshClientFactory: SSHClientFactory,
        fallbackServer: Server?,
        fallbackCredentials: ServerCredentials?
    ) async {
        let cleanup: TSSHResumeCleanupState
        do {
            guard let pending = try resumeStore.loadCleanup(for: paneID) else { return }
            cleanup = pending
        } catch {
            logger.warning(
                "Cannot load TSSH remote cleanup state: \(error.localizedDescription, privacy: .public)"
            )
            return
        }

        let cleanupCredentials: ServerCredentials
        if let persisted = cleanup.credentials {
            cleanupCredentials = persisted
        } else if let fallbackServer,
                  let fallbackCredentials,
                  cleanup.serverIdentity.matchesEndpoint(of: fallbackServer),
                  fallbackCredentials.isAuthorized(for: fallbackServer) {
            cleanupCredentials = fallbackCredentials
        } else {
            logger.warning("Deferred legacy TSSH cleanup has no endpoint-bound credentials")
            return
        }

        guard cleanupCredentials.serverId == cleanup.serverIdentity.id,
              let binding = cleanupCredentials.credentialBinding,
              binding.host == cleanup.serverIdentity.host,
              binding.port == cleanup.serverIdentity.port,
              binding.username == cleanup.serverIdentity.username,
              binding.connectionMode == .tssh else {
            logger.error("Rejected TSSH cleanup credentials that are not bound to the saved endpoint")
            return
        }

        let cleanupServer = Server(
            id: cleanup.serverIdentity.id,
            workspaceId: UUID(),
            name: "Deferred TSSH cleanup",
            host: cleanup.serverIdentity.host,
            port: cleanup.serverIdentity.port,
            username: cleanup.serverIdentity.username,
            connectionMode: .tssh,
            authMethod: binding.authMethod,
            updatedAt: cleanup.serverIdentity.updatedAt
        )
        let sshClient = sshClientFactory.makeClient(connectTimeout: .seconds(15))
        defer { Task { await sshClient.disconnect() } }
        do {
            _ = try await sshClient.connect(
                to: cleanupServer,
                credentials: cleanupCredentials
            )
            try await TSSHBootstrap.terminateServerForCleanup(
                cleanup.serverProcess,
                using: sshClient
            )
            try resumeStore.deleteCleanup(for: paneID)
        } catch {
            logger.warning(
                "Deferred TSSH remote cleanup for a later retry: \(error.localizedDescription, privacy: .public)"
            )
        }
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
    private let trustedHostFingerprint: @Sendable (String, Int) -> String?
    private let logger = Logger(
        subsystem: Bundle.main.bundleIdentifier ?? "Orlix",
        category: "TSSHRuntime"
    )

    private weak var surface: (any TerminalSurface)?
    private var columns = 80
    private var rows = 24
    private var startTask: Task<Void, Never>?
    private var writeTask: Task<Void, Never>?
    private var resizeTask: Task<Void, Never>?
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
    private var ownsVPN = false
    private var forwardStatuses: [UUID: TSSHForwardStatus] = [:]
    private var connectionGeneration = UUID()
    private var remoteSessionLifecycle: RemoteSessionLifecycleContext?
    private var remoteSessionLifecycleParser: RemoteSessionLifecycleStreamParser?
    private var lastRemoteSessionEvent: RemoteSessionEvent?
    private var standaloneStartupActionAwaitingExit = false
    private var terminalEventGate = TSSHTerminalEventGate()
    private var cancelledStartPreservesServer = false
    private var cancelledStartDeletesResumeState = true
    private var resumeHostKeyFingerprint: String?
    private var securityBindingHostKeyFingerprint: String?
    private var startupReady = false
    private var isClosing = false

    var isStartInFlight: Bool { startTask != nil }
    var hasLiveTransport: Bool { transport != nil }

    func isBound(to requestedServer: Server, credentials requestedCredentials: ServerCredentials) -> Bool {
        server.id == requestedServer.id
            && ServerCredentialBinding(server: server) == ServerCredentialBinding(server: requestedServer)
            && server.tsshProfile == requestedServer.tsshProfile
            && credentials == requestedCredentials
            && securityBindingHostKeyFingerprint
                == trustedHostFingerprint(requestedServer.host, requestedServer.port)
    }

    func matchesTrustedHost(host: String, port: Int) -> Bool {
        let normalizedHost = host.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        return TSSHResumeServerIdentity(server: server).host == normalizedHost
            && server.port == port
    }

    init(
        paneID: UUID,
        server: Server,
        credentials: ServerCredentials,
        sshClientFactory: SSHClientFactory,
        resumeStore: any TSSHResumeStoring = TSSHResumeStore.shared,
        callGate: TSSHCallGate = .shared,
        trustedHostFingerprint: @escaping @Sendable (String, Int) -> String? = { host, port in
            KnownHostsManager.shared.entry(for: host, port: port)?.fingerprint
        },
        ownerAccess: TSSHRuntimeOwnerAccess
    ) {
        self.paneID = paneID
        self.server = server
        self.credentials = credentials
        self.sshClientFactory = sshClientFactory
        self.resumeStore = resumeStore
        self.callGate = callGate
        self.trustedHostFingerprint = trustedHostFingerprint
        securityBindingHostKeyFingerprint = trustedHostFingerprint(server.host, server.port)
        self.ownerAccess = ownerAccess
    }

    func attach(to surface: any TerminalSurface) {
        self.surface = surface
        if transport != nil {
            ownerAccess.markTransport(paneID)
            ownerAccess.updateConnectionState(
                paneID,
                tsshPublishedTransportState(isHealthy: true, startupReady: startupReady)
            )
        }
    }

    func resize(cols: Int, rows: Int, pixelSize: TerminalPixelSize?) {
        guard cols > 0, rows > 0 else { return }
        columns = cols
        self.rows = rows
        guard let session else { return }
        let precedingResize = resizeTask
        resizeTask = Task { [weak self] in
            _ = await precedingResize?.result
            guard let self,
                  !Task.isCancelled,
                  !self.isClosing,
                  self.session == session else { return }
            do { try await self.callGate.resize(session, rows: rows, columns: cols) }
            catch {
                self.logger.warning(
                    "TSSH resize failed: \(error.localizedDescription, privacy: .public)"
                )
            }
        }
    }

    func startIfNeeded() {
        guard !isClosing, transport == nil, startTask == nil else { return }
        cancelledStartPreservesServer = false
        cancelledStartDeletesResumeState = true
        startupReady = false
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
        let generation = connectionGeneration
        let precedingWrite = writeTask
        writeTask = Task { [weak self] in
            _ = await precedingWrite?.result
            guard let self,
                  !Task.isCancelled,
                  !self.isClosing,
                  self.connectionGeneration == generation,
                  self.session == session else { return }
            do { try await self.callGate.write(data, to: session) }
            catch {
                guard !Task.isCancelled,
                      self.connectionGeneration == generation,
                      self.session == session else { return }
                await self.reportFailure(error)
            }
        }
    }

    func prepareForApplicationBackground() async {
        agentBridge?.suspend()
        guard transport != nil, session != nil, let resumeState else { return }
        let refreshedState = resumeState.refreshed(at: Date())
        do {
            try resumeStore.save(refreshedState, for: paneID)
            self.resumeState = refreshedState
        } catch {
            logger.error(
                "Cannot persist TSSH resume state: \(error.localizedDescription, privacy: .public)"
            )
        }
        guard !server.tsshProfile.keepTunnelsInBackground,
              let forwarder else { return }
        callGate.emergencyCloseForwarder(forwarder)
        self.forwarder = nil
        forwardBridge = nil
    }

    func resumeFromApplicationBackground() async {
        agentBridge?.resume()
        await cleanupStaleServerIfNeeded()
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
        if let pendingStart = startTask {
            cancelledStartPreservesServer = preserveServer
            cancelledStartDeletesResumeState = deleteResumeState
            pendingStart.cancel()
            await pendingStart.value
        }
        startTask = nil
        writeTask?.cancel()
        writeTask = nil
        resizeTask?.cancel()
        resizeTask = nil
        let transportFallback = transport.map(scheduleTransportFallback)
        if let forwarder { await callGate.closeForwarder(forwarder) }
        if let session {
            if preserveServer { callGate.forgetSession(session) }
            else { await callGate.closeSession(session) }
        }
        if let transport { await callGate.closeTransport(transport, preserveServer: preserveServer) }
        await stopOwnedVPN()
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
        startupReady = false
        ownerAccess.updateConnectionState(paneID, .disconnected)
    }

    func processDeferredCleanupBeforeRemoval() async {
        await TSSHDeferredCleanup.process(
            paneID: paneID,
            resumeStore: resumeStore,
            sshClientFactory: sshClientFactory,
            fallbackServer: server,
            fallbackCredentials: credentials
        )
    }

    func closeForSecurityBindingReplacement() async {
        if transport == nil,
           let savedState = try? resumeStore.load(for: paneID) {
            try? stageCleanup(for: savedState)
            await processDeferredCleanupBeforeRemoval()
        }
        await close()
    }

    static func processPersistedDeferredCleanup(
        resumeStore: any TSSHResumeStoring = TSSHResumeStore.shared,
        sshClientFactory: SSHClientFactory
    ) async {
        await TSSHDeferredCleanup.processAll(
            resumeStore: resumeStore,
            sshClientFactory: sshClientFactory
        )
    }

    func prepareForReconnect() async {
        invalidateConnectionGeneration()
        if let pendingStart = startTask {
            cancelledStartPreservesServer = true
            cancelledStartDeletesResumeState = false
            pendingStart.cancel()
            await pendingStart.value
        }
        startTask = nil
        writeTask?.cancel()
        writeTask = nil
        resizeTask?.cancel()
        resizeTask = nil
        if let session { callGate.forgetSession(session) }
        if let forwarder { callGate.emergencyCloseForwarder(forwarder) }
        if let transport { callGate.emergencyAbandon(transport) }
        await stopOwnedVPN()
        transport = nil
        session = nil
        forwarder = nil
        outputBridge = nil
        stateBridge = nil
        healthBridge = nil
        discardBridge = nil
        forwardBridge = nil
        agentBridge = nil
        startupReady = false
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
            await cleanupStaleServerIfNeeded()
            if try await resumeSavedSession() { return }
            await cleanupStaleServerIfNeeded()
            try await startFreshSession()
        } catch is CancellationError {
            let preserveServer = tsshShouldPreserveCancelledStartServer(
                preservationRequested: cancelledStartPreservesServer,
                hasCheckpoint: resumeStore.hasCheckpoint(for: paneID)
            )
            await discardFailedStart(
                preserveServer: preserveServer,
                deleteResumeState: cancelledStartDeletesResumeState
            )
            return
        } catch TSSHRuntimeError.resumeCheckpointUpdateFailed {
            await discardFailedStart(preserveServer: true, deleteResumeState: false)
            await reportFailure(TSSHRuntimeError.resumeCheckpointUpdateFailed)
        } catch {
            await discardFailedStart(preserveServer: false, deleteResumeState: true)
            await reportFailure(error)
        }
    }

    private func discardFailedStart(
        preserveServer: Bool,
        deleteResumeState: Bool
    ) async {
        invalidateConnectionGeneration()
        let transportFallback = transport.map(scheduleTransportFallback)
        if preserveServer {
            if let forwarder { callGate.emergencyCloseForwarder(forwarder) }
            if let session { callGate.forgetSession(session) }
            if let transport { callGate.emergencyAbandon(transport) }
        } else {
            if let forwarder { await callGate.closeForwarder(forwarder) }
            if let session { await callGate.closeSession(session) }
            if let transport { await callGate.closeTransport(transport, preserveServer: false) }
        }
        await stopOwnedVPN()
        transportFallback?.cancel()
        if deleteResumeState { try? resumeStore.delete(for: paneID) }
        self.forwarder = nil
        self.session = nil
        self.transport = nil
        if deleteResumeState { resumeState = nil }
        outputBridge = nil
        stateBridge = nil
        healthBridge = nil
        discardBridge = nil
        forwardBridge = nil
        agentBridge = nil
        startupReady = false
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
            if let recoveredState = resumeStore.recoverCleanupState(for: paneID) {
                let recoveryCredentials = recoveredState.serverIdentity
                    .matchesEndpoint(of: server)
                    && credentials.isAuthorized(for: server)
                    ? credentials
                    : nil
                let cleanupState = TSSHResumeCleanupState(
                    serverIdentity: recoveredState.serverIdentity,
                    serverProcess: recoveredState.serverProcess,
                    credentials: recoveryCredentials,
                    createdAt: recoveredState.createdAt
                )
                do {
                    try resumeStore.saveCleanup(cleanupState, for: paneID)
                    try resumeStore.delete(for: paneID)
                } catch {
                    logger.warning(
                        "Preserved TSSH checkpoint metadata after cleanup staging failed: \(error.localizedDescription, privacy: .public)"
                    )
                    throw TSSHRuntimeError.resumeCheckpointUpdateFailed
                }
            } else {
                try? resumeStore.delete(for: paneID)
            }
            logger.warning(
                "Discarded invalid TSSH resume state: \(error.localizedDescription, privacy: .public)"
            )
            return false
        }
        guard var state = loadedState else { return false }
        if state.isExpired {
            try stageCleanup(for: state)
            logger.info("Staged expired TSSH session for remote cleanup")
            return false
        }
        let currentIdentity = TSSHResumeServerIdentity(server: server)
        guard TSSHResumeCompatibilityPolicy.canResume(
            state,
            with: server,
            trustedHostFingerprint: trustedHostFingerprint(server.host, server.port)
        ) else {
            try stageCleanup(for: state)
            logger.info("Staged incompatible TSSH session for remote cleanup")
            return false
        }
        resumeHostKeyFingerprint = state.sshHostKeyFingerprint
        let currentProfile = server.tsshProfile
        let savedHost = state.host
        let expiresAt = state.savedAt.addingTimeInterval(86_400)
        var backoffSeconds = 1
        var attempt = 0
        while !Task.isCancelled, !isClosing, Date() < expiresAt {
            attempt += 1
            state = TSSHResumeState(
                serverIdentity: currentIdentity,
                sshHostKeyFingerprint: state.sshHostKeyFingerprint,
                serverProcess: state.serverProcess,
                host: savedHost,
                info: state.info.advancingClientIDForResume(
                    vpnEnabled: currentProfile.vpnEnabled
                ),
                sessionID: state.sessionID,
                profile: currentProfile,
                savedAt: state.savedAt
            )
            do {
                try resumeStore.save(state, for: paneID)
            } catch {
                logger.warning(
                    "Preserved the previous TSSH checkpoint after an update failed: \(error.localizedDescription, privacy: .public)"
                )
                throw TSSHRuntimeError.resumeCheckpointUpdateFailed
            }
            do {
                let transport = try await connect(savedHost, info: state.info, profile: currentProfile)
                self.transport = transport
                try Task.checkCancellation()
                try await enableAgentIfRequested(profile: currentProfile, on: transport)
                let generation = connectionGeneration
                restoreRemoteSessionLifecycle()
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
                    serverIdentity: currentIdentity,
                    sshHostKeyFingerprint: state.sshHostKeyFingerprint,
                    serverProcess: state.serverProcess,
                    host: savedHost,
                    info: state.info,
                    sessionID: attached.1,
                    profile: currentProfile,
                    savedAt: Date()
                )
                try resumeStore.save(resumeState!, for: paneID)
                try await startForwarding(on: transport, profile: currentProfile)
                try await startVPNIfRequested(
                    host: savedHost,
                    info: state.info,
                    profile: currentProfile
                )
                didConnect()
                return true
            } catch {
                if let runtimeError = error as? TSSHRuntimeError,
                   case .vpnStartFailed = runtimeError {
                    await cleanupFailedResume(preserveServer: false)
                    throw runtimeError
                }
                let shouldDiscard = TSSHResumeFailurePolicy.shouldDiscard(
                    after: attempt,
                    errorDescription: error.localizedDescription
                ) || TSSHResumeFailurePolicy.retryWouldExpire(
                    now: Date(),
                    retryDelaySeconds: backoffSeconds,
                    expiresAt: expiresAt
                )
                await cleanupFailedResume(preserveServer: !shouldDiscard)
                if shouldDiscard {
                    try? stageCleanup(for: state)
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
        await cleanupFailedResume(preserveServer: false)
        try? stageCleanup(for: state)
        return false
    }

    private func stageCleanup(for state: TSSHResumeState) throws {
        do {
            try resumeStore.saveCleanup(
                TSSHResumeCleanupState(
                    serverIdentity: state.serverIdentity,
                    serverProcess: state.serverProcess,
                    credentials: credentials,
                    createdAt: Date()
                ),
                for: paneID
            )
            try resumeStore.delete(for: paneID)
        } catch {
            throw TSSHRuntimeError.resumeCheckpointUpdateFailed
        }
    }

    private func cleanupStaleServerIfNeeded() async {
        await processDeferredCleanupBeforeRemoval()
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
        let startupPlan: TerminalShellStartupPlan
        let connectedTransport: TSSHTransportRef
        do {
            try Task.checkCancellation()
            startupPlan = try await ownerAccess.startupPlan(
                paneID,
                server.id,
                sshClient,
                identityToken
            )
            try Task.checkCancellation()
            connectedTransport = try await connect(
                bootstrap.host,
                info: bootstrap.info,
                profile: server.tsshProfile
            )
            self.transport = connectedTransport
        } catch {
            await TSSHBootstrap.terminateServer(bootstrap.serverProcess, using: sshClient)
            throw error
        }
        let transport = connectedTransport
        try Task.checkCancellation()
        try await enableAgentIfRequested(profile: server.tsshProfile, on: transport)

        let generation = connectionGeneration
        let outputBridge = makeOutputBridge(generation: generation)
        self.outputBridge = outputBridge
        acceptStartupPlan(startupPlan)
        let opened = try await callGate.openSession(
            on: transport,
            term: RemoteTerminalBootstrap.defaultTerminalType.rawValue,
            rows: rows,
            columns: columns,
            requestAgent: server.tsshProfile.sshAgentForwarding,
            command: startupPlan.command,
            output: outputBridge
        )
        session = opened.0
        try Task.checkCancellation()
        let state = TSSHResumeState(
            serverIdentity: TSSHResumeServerIdentity(server: server),
            sshHostKeyFingerprint: try currentResumeHostKeyFingerprint(),
            serverProcess: bootstrap.serverProcess,
            host: bootstrap.host,
            info: bootstrap.info,
            sessionID: opened.1,
            profile: server.tsshProfile,
            savedAt: Date()
        )
        try resumeStore.save(state, for: paneID)
        resumeState = state
        try await startForwarding(on: transport, profile: server.tsshProfile)
        try await startVPNIfRequested(
            host: bootstrap.host,
            info: bootstrap.info,
            profile: server.tsshProfile
        )
        didConnect()
    }

    private func currentResumeHostKeyFingerprint() throws -> String {
        if let resumeHostKeyFingerprint { return resumeHostKeyFingerprint }
        guard let fingerprint = trustedHostFingerprint(server.host, server.port),
              !fingerprint.isEmpty else {
            throw TSSHRuntimeError.resumeStateUnavailable
        }
        resumeHostKeyFingerprint = fingerprint
        securityBindingHostKeyFingerprint = fingerprint
        return fingerprint
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
                    tsshPublishedTransportState(
                        isHealthy: healthy,
                        startupReady: self.startupReady
                    )
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
                    self.ownerAccess.updateConnectionState(
                        self.paneID,
                        tsshPublishedTransportState(
                            isHealthy: true,
                            startupReady: self.startupReady
                        )
                    )
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
        do {
            try await callGate.configure(
                transport,
                keepPendingInput: profile.keepPendingInput,
                keepPendingOutput: profile.keepPendingOutput,
                state: state,
                health: health,
                discard: discard
            )
        } catch {
            callGate.emergencyAbandon(transport)
            throw error
        }
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
                    self.surface?.receiveTerminalOutput(
                        tsshForwardFailureNotice(id: id, message: message)
                    )
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

    private func startVPNIfRequested(
        host: String,
        info: TSSHServerInfo,
        profile: TSSHProfile
    ) async throws {
        guard profile.vpnEnabled else {
            try await TSSHVPNManager.shared.stopUnclaimedPersistedTunnel(forServerID: server.id)
            return
        }
        do {
            try await TSSHVPNManager.shared.installAndStart(
                TSSHVPNConfiguration(
                    reusing: host,
                    info: info,
                    profile: profile
                ),
                serverID: server.id,
                ownerID: identityToken,
                onUnexpectedDisconnect: { [weak self] message in
                    guard let self else { return }
                    await self.handleUnexpectedVPNDisconnect(message)
                }
            )
            ownsVPN = true
        } catch {
            logger.error("TSSH VPN failed to start: \(error.localizedDescription, privacy: .public)")
            throw TSSHRuntimeError.vpnStartFailed(error.localizedDescription)
        }
    }

    private func stopOwnedVPN() async {
        guard ownsVPN else { return }
        do {
            try await TSSHVPNManager.shared.stop(ifOwnedBy: identityToken)
        } catch {
            logger.error("TSSH VPN failed to stop: \(error.localizedDescription, privacy: .public)")
        }
        ownsVPN = false
    }

    private func handleUnexpectedVPNDisconnect(_ message: String) async {
        guard ownsVPN, isCurrent, !isClosing else { return }
        await prepareForReconnect()
        await reportFailure(TSSHRuntimeError.vpnStartFailed(message))
    }

    private func makeOutputBridge(generation: UUID) -> TSSHOutputBridge {
        TSSHOutputBridge(
            output: { [weak self] data in
                Task { @MainActor [weak self] in
                    guard let self,
                          self.isCurrent,
                          self.connectionGeneration == generation else { return }
                    self.consumeOutput(data)
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
                    self.deliverShellEnded(for: generation)
                }
            },
            close: { [weak self] in
                Task { @MainActor [weak self] in
                    guard let self,
                          self.connectionGeneration == generation else { return }
                    self.deliverShellEnded(for: generation)
                }
            }
        )
    }

    private var isCurrent: Bool {
        ownerAccess.isCurrent(paneID, identityToken)
    }

    private func deliverShellEnded(for generation: UUID) {
        guard connectionGeneration == generation,
              terminalEventGate.claim(generation) else { return }
        shellEnded()
    }

    private func didConnect() {
        guard isCurrent, !isClosing, !Task.isCancelled else { return }
        startupReady = true
        ownerAccess.markTransport(paneID)
        ownerAccess.updateConnectionState(paneID, .connected)
    }

    private func shellEnded() {
        guard isCurrent, !isClosing else { return }
        let reason: TerminalShellEndReason
        if standaloneStartupActionAwaitingExit {
            standaloneStartupActionAwaitingExit = false
            reason = .standaloneStartupActionCompleted
        } else {
            reason = TerminalShellEndReason.resolve(
                lifecycle: remoteSessionLifecycle,
                event: lastRemoteSessionEvent,
                sessionExists: nil
            )
        }
        ownerAccess.handleShellEnd(paneID, identityToken, reason)
    }

    private func acceptStartupPlan(_ plan: TerminalShellStartupPlan) {
        standaloneStartupActionAwaitingExit = plan.mayExecuteStandaloneUserStartupAction
        ownerAccess.setStartupActionReplayPending(
            paneID,
            plan.mayExecuteUserStartupAction
        )
        setRemoteSessionLifecycle(plan.remoteSessionLifecycle)
        ownerAccess.setResumeContext(paneID, plan.remoteSessionLifecycle)
    }

    private func restoreRemoteSessionLifecycle() {
        let context = ownerAccess.resumeContext(paneID)
        standaloneStartupActionAwaitingExit = TSSHResumeLifecyclePolicy
            .shouldAwaitStandaloneStartupAction(
                hasRemoteSessionLifecycle: context != nil,
                replayPending: ownerAccess.startupActionReplayPending(paneID)
            )
        setRemoteSessionLifecycle(context)
    }

    private func setRemoteSessionLifecycle(_ context: RemoteSessionLifecycleContext?) {
        remoteSessionLifecycle = context
        remoteSessionLifecycleParser = context.map {
            RemoteSessionLifecycleStreamParser(observation: $0.observation)
        }
        lastRemoteSessionEvent = nil
    }

    private func consumeOutput(_ data: Data) {
        guard var parser = remoteSessionLifecycleParser else {
            surface?.receiveTerminalOutput(data)
            return
        }
        let result = parser.consume(data)
        remoteSessionLifecycleParser = parser
        if !result.output.isEmpty {
            surface?.receiveTerminalOutput(result.output)
        }
        if result.events.contains(.attached) {
            ownerAccess.remoteSessionAttached(paneID)
        }
        if let event = result.events.last {
            lastRemoteSessionEvent = event
        }
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
