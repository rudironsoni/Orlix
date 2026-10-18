import Combine
import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package struct TerminalReconnectPaneFacts: Sendable {
    package let connectionState: ConnectionState
    package let hasEstablishedConnection: Bool

    package init(
        connectionState: ConnectionState,
        hasEstablishedConnection: Bool
    ) {
        self.connectionState = connectionState
        self.hasEstablishedConnection = hasEstablishedConnection
    }
}

#if os(macOS)
nonisolated package struct MacTerminalRecoveryCandidate: Sendable {
    package let paneId: UUID
    package let strategy: MacTerminalRecoveryPolicy.ReadyStrategy
}
#endif

@MainActor
package struct TerminalReconnectAccess {
    package let paneFacts: @MainActor @Sendable (UUID) -> TerminalReconnectPaneFacts?
    package let paneIDs: @MainActor @Sendable () -> [UUID]
    package let paneIDsForServer: @MainActor @Sendable (UUID) -> [UUID]
    package let networkPathBecameReady: @MainActor @Sendable (UUID) -> Void
    package let prepareTransport: @MainActor @Sendable (UUID) async -> Void
    package let startConnection: @MainActor @Sendable (UUID) -> Bool
    package let failConnection: @MainActor @Sendable (UUID) -> Void

    #if os(macOS)
    package let offlineMacRecoveryPaneIDs: @MainActor @Sendable () -> [UUID]
    package let macRecoveryCandidates: @MainActor @Sendable () -> [MacTerminalRecoveryCandidate]
    package let beginEternalTerminalProbe: @MainActor @Sendable (UUID) async -> UUID?
    package let hasVerifiedLiveTransport: @MainActor @Sendable (UUID, UUID?) async -> Bool
    package let markMoshConnected: @MainActor @Sendable (UUID) -> Void
    #endif

    #if os(macOS)
    package init(
        paneFacts: @escaping @MainActor @Sendable (UUID) -> TerminalReconnectPaneFacts?,
        paneIDs: @escaping @MainActor @Sendable () -> [UUID],
        paneIDsForServer: @escaping @MainActor @Sendable (UUID) -> [UUID],
        networkPathBecameReady: @escaping @MainActor @Sendable (UUID) -> Void,
        prepareTransport: @escaping @MainActor @Sendable (UUID) async -> Void,
        startConnection: @escaping @MainActor @Sendable (UUID) -> Bool,
        failConnection: @escaping @MainActor @Sendable (UUID) -> Void,
        offlineMacRecoveryPaneIDs: @escaping @MainActor @Sendable () -> [UUID],
        macRecoveryCandidates: @escaping @MainActor @Sendable () -> [MacTerminalRecoveryCandidate],
        beginEternalTerminalProbe: @escaping @MainActor @Sendable (UUID) async -> UUID?,
        hasVerifiedLiveTransport: @escaping @MainActor @Sendable (UUID, UUID?) async -> Bool,
        markMoshConnected: @escaping @MainActor @Sendable (UUID) -> Void
    ) {
        self.paneFacts = paneFacts
        self.paneIDs = paneIDs
        self.paneIDsForServer = paneIDsForServer
        self.networkPathBecameReady = networkPathBecameReady
        self.prepareTransport = prepareTransport
        self.startConnection = startConnection
        self.failConnection = failConnection
        self.offlineMacRecoveryPaneIDs = offlineMacRecoveryPaneIDs
        self.macRecoveryCandidates = macRecoveryCandidates
        self.beginEternalTerminalProbe = beginEternalTerminalProbe
        self.hasVerifiedLiveTransport = hasVerifiedLiveTransport
        self.markMoshConnected = markMoshConnected
    }
    #else
    package init(
        paneFacts: @escaping @MainActor @Sendable (UUID) -> TerminalReconnectPaneFacts?,
        paneIDs: @escaping @MainActor @Sendable () -> [UUID],
        paneIDsForServer: @escaping @MainActor @Sendable (UUID) -> [UUID],
        networkPathBecameReady: @escaping @MainActor @Sendable (UUID) -> Void,
        prepareTransport: @escaping @MainActor @Sendable (UUID) async -> Void,
        startConnection: @escaping @MainActor @Sendable (UUID) -> Bool,
        failConnection: @escaping @MainActor @Sendable (UUID) -> Void
    ) {
        self.paneFacts = paneFacts
        self.paneIDs = paneIDs
        self.paneIDsForServer = paneIDsForServer
        self.networkPathBecameReady = networkPathBecameReady
        self.prepareTransport = prepareTransport
        self.startConnection = startConnection
        self.failConnection = failConnection
    }
    #endif
}

@MainActor
final package class TerminalReconnectCoordinator: ObservableObject {
    nonisolated package enum Phase: Hashable, Sendable {
        case preparing
        case waitingForNetwork
        case waitingForApplication
        case waitingForUnlock
        case connecting
    }

    nonisolated package struct Attempt: Hashable, Sendable {
        package let id: UUID
        package let paneId: UUID
        package let generation: UUID
        package let startedAt: Date
        package var phase: Phase
    }

    nonisolated package enum EventStage: String, Equatable, Sendable {
        case preparationStarted
        case cleanupStarted
        case cleanupCompleted
        case preparationDeadline
        case waitingForNetwork
        case waitingForApplication
        case waitingForUnlock
        case connecting
        case connectionDeadline
        case cleanupDeadline
        case completed
        case invalidated
        case staleResultRejected
    }

    nonisolated package struct Event: Sendable {
        package let attempt: Attempt
        package let stage: EventStage
        package let systemUptime: TimeInterval
    }

    private nonisolated enum CleanupOutcome: Equatable, Sendable {
        case completed
        case deadline
        case cancelled
        case failed
    }

    package typealias Sleep = @Sendable (Duration) async throws -> Void
    package typealias EventHandler = @MainActor @Sendable (Event) -> Void

    private struct Record {
        package var attempt: Attempt
        package var requiresReadyNetwork: Bool
        package var task: Task<Void, Never>?
    }

    private struct AutomaticContext {
        package var sceneIsActive: Bool
        package var automaticReconnectAllowed: Bool
    }

    private final class AutomaticRetry {
        package var task: Task<Void, Never>?

        package func cancel() {
            task?.cancel()
        }
    }

    package let access: TerminalReconnectAccess
    private let preparationTimeout: Duration
    private let connectionTimeout: Duration
    private let retryDelay: Duration
    private let applicationIsActiveQuery: @MainActor @Sendable () -> Bool
    private let now: @Sendable () -> Date
    private let sleep: Sleep
    private let onEvent: EventHandler
    private let onChange: @MainActor @Sendable () -> Void

    private var records: [UUID: Record] = [:]
    private var connectionGenerations: [UUID: UUID] = [:]
    private var automaticContexts: [UUID: AutomaticContext] = [:]
    private var automaticRetries: [UUID: AutomaticRetry] = [:]
    private var signalCancellables: Set<AnyCancellable> = []

    private(set) var currentNetworkReadiness: TerminalNetworkReadiness
    private(set) var applicationIsActive: Bool
    private(set) var appIsLocked: Bool

    package var applicationActivityIsActive: Bool {
        applicationIsActiveQuery()
    }

    #if os(macOS)
    package var macRecoveryGate = MacTerminalRecoveryGate()
    package var macReconciliationID: UUID?
    package var macRecoveryTask: Task<Void, Never>?
    #elseif os(iOS)
    package var iosNetworkRecoveryGate = TerminalNetworkRecoveryGate()
    #endif

    package init(
        access: TerminalReconnectAccess,
        initialNetworkReadiness: TerminalNetworkReadiness,
        networkUpdates: AnyPublisher<TerminalNetworkReadiness, Never>? = nil,
        applicationIsActive: @escaping @MainActor @Sendable () -> Bool,
        initialAppIsLocked: Bool,
        appLockUpdates: AnyPublisher<Bool, Never>? = nil,
        preparationTimeout: Duration = .seconds(5),
        connectionTimeout: Duration = .seconds(45),
        retryDelay: Duration = .seconds(5),
        now: @escaping @Sendable () -> Date = { Date() },
        sleep: @escaping Sleep = { duration in try await Task.sleep(for: duration) },
        onEvent: @escaping EventHandler = { _ in },
        onChange: @escaping @MainActor @Sendable () -> Void
    ) {
        self.access = access
        self.currentNetworkReadiness = initialNetworkReadiness
        self.applicationIsActiveQuery = applicationIsActive
        self.applicationIsActive = applicationIsActive()
        self.appIsLocked = initialAppIsLocked
        self.preparationTimeout = preparationTimeout
        self.connectionTimeout = connectionTimeout
        self.retryDelay = retryDelay
        self.now = now
        self.sleep = sleep
        self.onEvent = onEvent
        self.onChange = onChange

        networkUpdates?
            .sink { [weak self] readiness in
                self?.receiveNetworkReadiness(readiness)
            }
            .store(in: &signalCancellables)
        appLockUpdates?
            .sink { [weak self] isLocked in
                self?.receiveAppLock(isLocked)
            }
            .store(in: &signalCancellables)
    }

    isolated deinit {
        records.values.forEach { $0.task?.cancel() }
        automaticRetries.values.forEach { $0.cancel() }
        #if os(macOS)
        macRecoveryTask?.cancel()
        #endif
        signalCancellables.forEach { $0.cancel() }
    }

    package func attempt(for paneId: UUID) -> Attempt? {
        records[paneId]?.attempt
    }

    package var activePaneIDs: Set<UUID> {
        Set(records.keys)
    }

    package func connectionGeneration(for paneId: UUID) -> UUID {
        connectionGenerations[paneId] ?? paneId
    }

    package var recoveryPaneIDs: [UUID] {
        access.paneIDs()
    }

    package func recoveryPaneFacts(for paneId: UUID) -> TerminalReconnectPaneFacts? {
        access.paneFacts(paneId)
    }

    #if os(macOS)
    package var offlineMacRecoveryPaneIDs: [UUID] {
        access.offlineMacRecoveryPaneIDs()
    }

    package var macRecoveryCandidates: [MacTerminalRecoveryCandidate] {
        access.macRecoveryCandidates()
    }

    package func beginEternalTerminalRecoveryProbe(_ paneId: UUID) async -> UUID? {
        await access.beginEternalTerminalProbe(paneId)
    }

    package func verifyLiveTransport(
        for paneId: UUID,
        eternalTerminalProbeID: UUID?
    ) async -> Bool {
        await access.hasVerifiedLiveTransport(paneId, eternalTerminalProbeID)
    }

    package func markMoshConnectedIfNeeded(_ paneId: UUID) {
        access.markMoshConnected(paneId)
    }
    #endif

    @discardableResult
    package func request(
        for paneId: UUID,
        requiresReadyNetwork: Bool,
        generation: UUID = UUID(),
        replacingCurrent: Bool = false
    ) -> Bool {
        guard access.paneFacts(paneId) != nil else { return false }

        #if os(iOS)
        if currentNetworkReadiness == .unavailable {
            return queueIOSReconnectUntilNetworkReady(
                for: paneId,
                replacingCurrent: replacingCurrent
            )
        }
        #endif

        guard !requiresReadyNetwork || currentNetworkReadiness == .ready else {
            return false
        }
        return requestAttempt(
            for: paneId,
            generation: generation,
            requiresReadyNetwork: requiresReadyNetwork,
            replacingCurrent: replacingCurrent
        )
    }

    package func receiveApplicationActivity(_ isActive: Bool) {
        guard applicationIsActive != isActive else {
            if isActive {
                resumeEligibleAttempts()
                reconcileAllAutomaticReconnects()
                #if os(macOS)
                startMacReconciliationIfEligible()
                #endif
            }
            return
        }
        applicationIsActive = isActive
        if isActive {
            resumeEligibleAttempts()
            reconcileAllAutomaticReconnects()
            #if os(macOS)
            startMacReconciliationIfEligible()
            #endif
        } else {
            suspendConnectingAttempts()
            #if os(macOS)
            pauseMacReconciliation()
            #endif
        }
    }

    package func receiveAppLock(_ isLocked: Bool) {
        guard appIsLocked != isLocked else { return }
        appIsLocked = isLocked
        if isLocked {
            suspendConnectingAttempts()
            #if os(macOS)
            pauseMacReconciliation()
            #endif
        } else {
            resumeEligibleAttempts()
            reconcileAllAutomaticReconnects()
            #if os(macOS)
            startMacReconciliationIfEligible()
            #endif
        }
    }

    package func receiveNetworkReadiness(_ readiness: TerminalNetworkReadiness) {
        guard currentNetworkReadiness != readiness else { return }
        currentNetworkReadiness = readiness
        if readiness == .ready {
            for paneId in access.paneIDs() {
                access.networkPathBecameReady(paneId)
            }
        }
        #if os(macOS)
        receiveMacRecoverySignal(.networkChanged(readiness))
        #elseif os(iOS)
        handleIOSNetworkReadinessChange(readiness)
        #endif
        if readiness == .ready {
            resumeEligibleAttempts()
        } else {
            suspendConnectingAttemptsRequiringReadyNetwork()
        }
        reconcileAllAutomaticReconnects()
    }

    package func reconcileAutomaticReconnect(
        for paneId: UUID,
        sceneIsActive: Bool,
        automaticReconnectAllowed: Bool
    ) {
        automaticContexts[paneId] = AutomaticContext(
            sceneIsActive: sceneIsActive,
            automaticReconnectAllowed: automaticReconnectAllowed
        )
        receiveApplicationActivity(applicationActivityIsActive)
        reconcileAutomaticReconnect(for: paneId)
    }

    package func removeAutomaticReconnectContext(for paneId: UUID) {
        automaticContexts.removeValue(forKey: paneId)
        cancelAutomaticRetry(for: paneId)
    }

    package func cancelAutomaticRetry(for paneId: UUID) {
        automaticRetries.removeValue(forKey: paneId)?.cancel()
    }

    package func connectionStateDidChange(for paneId: UUID) {
        guard let facts = access.paneFacts(paneId) else {
            removePane(paneId)
            return
        }

        #if os(iOS)
        if facts.connectionState.isConnecting,
           facts.hasEstablishedConnection,
           currentNetworkReadiness == .unavailable {
            _ = queueIOSReconnectUntilNetworkReady(for: paneId)
        }
        #endif

        switch facts.connectionState {
        case .connecting, .reconnecting:
            cancelAutomaticRetry(for: paneId)
        case .failed:
            complete(for: paneId)
            reconcileFailedAutomaticRetry(for: paneId)
            return
        case .disconnected, .connected, .idle:
            complete(for: paneId)
        }
        reconcileAutomaticReconnect(for: paneId)
    }

    package func invalidatePreparations(forServer serverId: UUID) {
        for paneId in access.paneIDsForServer(serverId) {
            invalidate(for: paneId)
        }
    }

    package func complete(for paneId: UUID) {
        removeAttempt(for: paneId, event: .completed)
    }

    package func invalidate(for paneId: UUID) {
        removeAttempt(for: paneId, event: .invalidated)
    }

    package func removePane(_ paneId: UUID) {
        invalidate(for: paneId)
        cancelAutomaticRetry(for: paneId)
        automaticContexts.removeValue(forKey: paneId)
        connectionGenerations.removeValue(forKey: paneId)
    }

    package func prepareForApplicationTermination() {
        invalidateAllAttempts()
        automaticRetries.values.forEach { $0.cancel() }
        automaticRetries.removeAll()
        automaticContexts.removeAll()
        resetPlatformRecovery()
    }

    package func reset() {
        prepareForApplicationTermination()
        connectionGenerations.removeAll()
    }

    @discardableResult
    private func requestAttempt(
        for paneId: UUID,
        generation: UUID,
        requiresReadyNetwork: Bool,
        replacingCurrent: Bool
    ) -> Bool {
        if let current = records[paneId] {
            if current.attempt.generation == generation || !replacingCurrent {
                return false
            }
            current.task?.cancel()
            emit(.invalidated, for: current.attempt)
        }

        let attempt = Attempt(
            id: UUID(),
            paneId: paneId,
            generation: generation,
            startedAt: now(),
            phase: .preparing
        )
        records[paneId] = Record(
            attempt: attempt,
            requiresReadyNetwork: requiresReadyNetwork,
            task: nil
        )
        emit(.preparationStarted, for: attempt)
        notifyChange()
        emit(.cleanupStarted, for: attempt)

        let prepareTransport = access.prepareTransport
        let timeout = preparationTimeout
        let sleep = sleep
        records[paneId]?.task = Task { [weak self] in
            let outcome = await Self.performCleanup(
                paneId: attempt.paneId,
                timeout: timeout,
                sleep: sleep,
                prepareTransport: prepareTransport
            )
            self?.preparationFinished(attempt, outcome: outcome)
        }
        return true
    }

    private nonisolated static func performCleanup(
        paneId: UUID,
        timeout: Duration,
        sleep: @escaping Sleep,
        prepareTransport: @escaping @MainActor @Sendable (UUID) async -> Void
    ) async -> CleanupOutcome {
        do {
            try await HardOperationDeadline.run(
                timeout: timeout,
                sleep: sleep,
                operation: {
                    await prepareTransport(paneId)
                }
            )
            return .completed
        } catch is CancellationError {
            return .cancelled
        } catch HardOperationDeadlineError.exceeded {
            return .deadline
        } catch {
            return .failed
        }
    }

    private func preparationFinished(_ attempt: Attempt, outcome: CleanupOutcome) {
        guard outcome != .cancelled, outcome != .failed else { return }
        guard currentRecord(for: attempt) != nil else {
            emit(.staleResultRejected, for: attempt)
            return
        }
        emit(outcome == .completed ? .cleanupCompleted : .preparationDeadline, for: attempt)
        waitOrBegin(attempt)
    }

    private func waitOrBegin(_ attempt: Attempt) {
        guard let record = currentRecord(for: attempt) else { return }
        switch waitPhase(for: record) {
        case .none:
            beginConnection(attempt)
        case .some(let phase):
            wait(attempt, phase: phase)
        }
    }

    private func beginConnection(_ attempt: Attempt) {
        guard var record = currentRecord(for: attempt),
              waitPhase(for: record) == nil else { return }
        record.attempt.phase = .connecting
        record.task = nil
        records[attempt.paneId] = record

        let previousGeneration = connectionGenerations[attempt.paneId]
        connectionGenerations[attempt.paneId] = UUID()
        guard access.startConnection(attempt.paneId) else {
            records.removeValue(forKey: attempt.paneId)
            connectionGenerations[attempt.paneId] = previousGeneration
            emit(.staleResultRejected, for: attempt)
            notifyChange()
            return
        }

        emit(.connecting, for: record.attempt)
        notifyChange()
        let sleep = sleep
        let connectionTimeout = connectionTimeout
        let preparationTimeout = preparationTimeout
        let prepareTransport = access.prepareTransport
        records[attempt.paneId]?.task = Task { [weak self] in
            do {
                try await sleep(connectionTimeout)
            } catch {
                return
            }
            guard self?.beginConnectionDeadline(record.attempt) == true else { return }
            let outcome = await Self.performCleanup(
                paneId: record.attempt.paneId,
                timeout: preparationTimeout,
                sleep: sleep,
                prepareTransport: prepareTransport
            )
            self?.connectionDeadlineFinished(record.attempt, outcome: outcome)
        }
    }

    private func beginConnectionDeadline(_ attempt: Attempt) -> Bool {
        guard currentRecord(for: attempt) != nil else {
            emit(.staleResultRejected, for: attempt)
            return false
        }
        emit(.connectionDeadline, for: attempt)
        emit(.cleanupStarted, for: attempt)
        return true
    }

    private func connectionDeadlineFinished(_ attempt: Attempt, outcome: CleanupOutcome) {
        guard outcome != .cancelled, outcome != .failed else { return }
        guard currentRecord(for: attempt) != nil else {
            emit(.staleResultRejected, for: attempt)
            return
        }
        emit(outcome == .completed ? .cleanupCompleted : .cleanupDeadline, for: attempt)
        records.removeValue(forKey: attempt.paneId)
        access.failConnection(attempt.paneId)
        notifyChange()
    }

    private func wait(_ attempt: Attempt, phase: Phase) {
        guard var record = currentRecord(for: attempt) else { return }
        record.attempt.phase = phase
        record.task = nil
        records[attempt.paneId] = record
        emit(eventStage(for: phase), for: record.attempt)
        notifyChange()
    }

    private func waitPhase(for record: Record) -> Phase? {
        if record.requiresReadyNetwork, currentNetworkReadiness != .ready {
            return .waitingForNetwork
        }
        if !applicationIsActive {
            return .waitingForApplication
        }
        if appIsLocked {
            return .waitingForUnlock
        }
        return nil
    }

    private func eventStage(for phase: Phase) -> EventStage {
        switch phase {
        case .waitingForNetwork: .waitingForNetwork
        case .waitingForApplication: .waitingForApplication
        case .waitingForUnlock: .waitingForUnlock
        case .preparing, .connecting:
            preconditionFailure("A wait event requires a wait phase")
        }
    }

    private func resumeEligibleAttempts(generation: UUID? = nil) {
        let attempts = records.values.compactMap { record -> Attempt? in
            guard generation == nil || record.attempt.generation == generation else {
                return nil
            }
            switch record.attempt.phase {
            case .waitingForNetwork, .waitingForApplication, .waitingForUnlock:
                return record.attempt
            case .preparing, .connecting:
                return nil
            }
        }
        for attempt in attempts {
            waitOrBegin(attempt)
        }
    }

    private func suspendConnectingAttempts(generation: UUID? = nil) {
        let activeRecords = records.values.filter { record in
            record.attempt.phase == .connecting
                && (generation == nil || record.attempt.generation == generation)
        }
        for activeRecord in activeRecords {
            let attempt = activeRecord.attempt
            guard currentRecord(for: attempt) != nil else { continue }
            records.removeValue(forKey: attempt.paneId)
            activeRecord.task?.cancel()
            emit(.invalidated, for: attempt)
            _ = requestAttempt(
                for: attempt.paneId,
                generation: attempt.generation,
                requiresReadyNetwork: activeRecord.requiresReadyNetwork,
                replacingCurrent: true
            )
        }
    }

    private func suspendConnectingAttemptsRequiringReadyNetwork() {
        let activeRecords = records.values.filter {
            $0.attempt.phase == .connecting && $0.requiresReadyNetwork
        }
        for activeRecord in activeRecords {
            let attempt = activeRecord.attempt
            guard currentRecord(for: attempt) != nil else { continue }
            records.removeValue(forKey: attempt.paneId)
            activeRecord.task?.cancel()
            emit(.invalidated, for: attempt)
            _ = requestAttempt(
                for: attempt.paneId,
                generation: attempt.generation,
                requiresReadyNetwork: true,
                replacingCurrent: true
            )
        }
    }

    package func markNetworkUnavailable(for generation: UUID) {
        let matching = records.values.filter { $0.attempt.generation == generation }
        for record in matching where record.attempt.phase != .connecting {
            records[record.attempt.paneId]?.requiresReadyNetwork = true
        }
        suspendConnectingAttempts(generation: generation)
    }

    package func markNetworkReady(for generation: UUID) {
        resumeEligibleAttempts(generation: generation)
    }

    @discardableResult
    package func requestWaitingForNetwork(
        for paneId: UUID,
        generation: UUID,
        replacingCurrent: Bool
    ) -> Bool {
        guard access.paneFacts(paneId) != nil else { return false }
        return requestAttempt(
            for: paneId,
            generation: generation,
            requiresReadyNetwork: true,
            replacingCurrent: replacingCurrent
        )
    }

    private func reconcileAutomaticReconnect(for paneId: UUID) {
        guard let context = automaticContexts[paneId],
              let facts = access.paneFacts(paneId) else {
            cancelAutomaticRetry(for: paneId)
            return
        }

        if TerminalAutoReconnectPolicy.shouldAttempt(
            sceneIsActive: context.sceneIsActive,
            applicationIsActive: applicationIsActive,
            appIsLocked: appIsLocked,
            networkReadiness: currentNetworkReadiness,
            automaticReconnectAllowed: context.automaticReconnectAllowed,
            reconnectInFlight: records[paneId] != nil,
            hasEstablishedConnection: facts.hasEstablishedConnection,
            connectionState: facts.connectionState
        ) {
            cancelAutomaticRetry(for: paneId)
            _ = request(for: paneId, requiresReadyNetwork: true)
            return
        }

        if TerminalAutoReconnectPolicy.shouldScheduleRetry(
            automaticReconnectAllowed: context.automaticReconnectAllowed,
            hasEstablishedConnection: facts.hasEstablishedConnection,
            connectionState: facts.connectionState
        ) {
            scheduleAutomaticRetry(for: paneId)
        } else {
            cancelAutomaticRetry(for: paneId)
        }
    }

    private func reconcileFailedAutomaticRetry(for paneId: UUID) {
        guard let context = automaticContexts[paneId],
              let facts = access.paneFacts(paneId),
              TerminalAutoReconnectPolicy.shouldScheduleRetry(
                automaticReconnectAllowed: context.automaticReconnectAllowed,
                hasEstablishedConnection: facts.hasEstablishedConnection,
                connectionState: facts.connectionState
              ) else {
            cancelAutomaticRetry(for: paneId)
            return
        }
        scheduleAutomaticRetry(for: paneId)
    }

    private func reconcileAllAutomaticReconnects() {
        for paneId in Array(automaticContexts.keys) {
            reconcileAutomaticReconnect(for: paneId)
        }
    }

    private func scheduleAutomaticRetry(for paneId: UUID) {
        guard automaticRetries[paneId] == nil else { return }
        let retry = AutomaticRetry()
        let sleep = sleep
        let retryDelay = retryDelay
        retry.task = Task { [weak self] in
            do {
                try await sleep(retryDelay)
            } catch {
                return
            }
            self?.automaticRetryDeadlineReached(for: paneId, retry: retry)
        }
        automaticRetries[paneId] = retry
    }

    private func automaticRetryDeadlineReached(for paneId: UUID, retry: AutomaticRetry) {
        guard automaticRetries[paneId] === retry else { return }
        automaticRetries.removeValue(forKey: paneId)
        reconcileAutomaticReconnect(for: paneId)
    }

    private func currentRecord(for attempt: Attempt) -> Record? {
        guard let record = records[attempt.paneId], record.attempt.id == attempt.id else {
            return nil
        }
        return record
    }

    private func removeAttempt(for paneId: UUID, event: EventStage) {
        guard let record = records.removeValue(forKey: paneId) else { return }
        record.task?.cancel()
        emit(event, for: record.attempt)
        notifyChange()
    }

    private func invalidateAllAttempts() {
        let activeRecords = Array(records.values)
        records.removeAll()
        for record in activeRecords {
            record.task?.cancel()
            emit(.invalidated, for: record.attempt)
        }
        if !activeRecords.isEmpty {
            notifyChange()
        }
    }

    private func resetPlatformRecovery() {
        #if os(macOS)
        macRecoveryTask?.cancel()
        macRecoveryTask = nil
        macReconciliationID = nil
        macRecoveryGate = MacTerminalRecoveryGate()
        #elseif os(iOS)
        iosNetworkRecoveryGate = TerminalNetworkRecoveryGate()
        #endif
    }

    private func emit(_ stage: EventStage, for attempt: Attempt) {
        onEvent(Event(
            attempt: attempt,
            stage: stage,
            systemUptime: Foundation.ProcessInfo.processInfo.systemUptime
        ))
    }

    private func notifyChange() {
        objectWillChange.send()
        onChange()
    }
}
