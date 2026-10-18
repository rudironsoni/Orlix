import ETBootstrap
import ETSession
import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
import OrlixSSH
#endif
@MainActor
package struct LiveEternalTerminalSessionPreparer: EternalTerminalSessionPreparing {
    private let resumeStore: any EternalTerminalResumeStoring
    private let sshClientFactory: SSHClientFactory

    package init(
        resumeStore: any EternalTerminalResumeStoring,
        sshClientFactory: SSHClientFactory
    ) {
        self.resumeStore = resumeStore
        self.sshClientFactory = sshClientFactory
    }

    package func prepareSession(
        request: EternalTerminalSessionRequest,
        startupPlanProvider: @Sendable @escaping (SSHClient) async throws -> TerminalShellStartupPlan,
        isCurrentOwner: @MainActor @Sendable @escaping () -> Bool
    ) async throws -> PreparedEternalTerminalSession {
        let executor = SSHETBootstrapExecutor(
            server: request.server,
            credentials: request.credentials,
            client: sshClientFactory.makeClient(),
            startupPlanProvider: startupPlanProvider
        )
        let port = UInt16(exactly: request.server.eternalTerminalPort) ?? 2022

        do {
            if let credentials = try resumeStore.credentials(for: request.paneId) {
                if let checkpoint = try resumeStore.checkpoint(for: request.paneId) {
                    let session = try ETTerminalSession(
                        host: request.server.host,
                        port: port,
                        clientID: credentials.clientID,
                        passkey: credentials.passkey,
                        checkpoint: checkpoint
                    )
                    return PreparedEternalTerminalSession(
                        session: LiveEternalTerminalSession(
                            session: session,
                            executor: executor,
                            paneId: request.paneId,
                            resumeStore: resumeStore
                        ),
                        origin: .resumed
                    )
                }
                // Older versions saved credentials without the protocol state
                // required to resume the returning byte stream.
                try resumeStore.deleteResumeState(for: request.paneId)
            }
        } catch let error as EternalTerminalResumeCredentialError {
            if error.shouldDeleteStoredCredentials {
                try? resumeStore.deleteResumeState(for: request.paneId)
            }
            throw EternalTerminalVendorErrorMapper.failure(for: error)
        } catch {
            throw EternalTerminalVendorErrorMapper.failure(for: error)
        }

        do {
            let credentials = try await ETBootstrap(
                options: SSHETBootstrapExecutor.bootstrapOptions
            ).run(using: executor)
            try Task.checkCancellation()
            guard isCurrentOwner() else { throw CancellationError() }

            let resumeCredentials = try EternalTerminalResumeCredentials(credentials)
            try resumeStore.save(resumeCredentials, for: request.paneId)
            let terminalType = await executor.preparedTerminalType()
            try Task.checkCancellation()
            guard isCurrentOwner() else { throw CancellationError() }

            let session = try ETTerminalSession(
                host: request.server.host,
                port: port,
                clientID: resumeCredentials.clientID,
                passkey: resumeCredentials.passkey,
                environmentVariables: RemoteTerminalBootstrap.terminalEnvironmentDictionary(
                    terminalType: terminalType,
                    transport: .eternalTerminal
                )
            )
            return PreparedEternalTerminalSession(
                session: LiveEternalTerminalSession(
                    session: session,
                    executor: executor,
                    paneId: request.paneId,
                    resumeStore: resumeStore
                ),
                origin: .bootstrapped
            )
        } catch is CancellationError {
            throw CancellationError()
        } catch {
            throw EternalTerminalVendorErrorMapper.failure(for: error)
        }
    }

    package func discardResumeState(for paneId: UUID) throws {
        try resumeStore.deleteResumeState(for: paneId)
    }
}

private nonisolated struct LiveEternalTerminalSession: EternalTerminalSession {
    private let session: ETTerminalSession
    private let executor: SSHETBootstrapExecutor
    private let paneId: UUID
    private let resumeStore: any EternalTerminalResumeStoring

    nonisolated package init(
        session: ETTerminalSession,
        executor: SSHETBootstrapExecutor,
        paneId: UUID,
        resumeStore: any EternalTerminalResumeStoring
    ) {
        self.session = session
        self.executor = executor
        self.paneId = paneId
        self.resumeStore = resumeStore
    }

    nonisolated package var output: AsyncStream<Data> {
        session.output
    }

    nonisolated package var stateChanges: AsyncStream<EternalTerminalSessionState> {
        let upstream = session.stateChanges
        return AsyncStream(bufferingPolicy: .bufferingNewest(16)) { continuation in
            let task = Task {
                for await state in upstream {
                    guard !Task.isCancelled else { return }
                    continuation.yield(EternalTerminalVendorErrorMapper.state(for: state))
                }
                continuation.finish()
            }
            continuation.onTermination = { @Sendable _ in
                task.cancel()
            }
        }
    }

    package func connect() async throws {
        try await mapFailure { try await session.connect() }
    }

    package func send(_ data: Data) async throws {
        try await mapFailure { try await session.send(data) }
    }

    package func resize(
        rows: Int,
        cols: Int,
        pixelWidth: Int?,
        pixelHeight: Int?
    ) async throws {
        try await mapFailure {
            try await session.resize(
                rows: rows,
                cols: cols,
                pixelWidth: pixelWidth,
                pixelHeight: pixelHeight
            )
        }
    }

    package func notifyNetworkPathChanged() async {
        await session.notifyNetworkPathChanged()
    }

    package func persistCheckpoint(
        ifCurrentOwner: @MainActor @Sendable @escaping () -> Bool
    ) async throws {
        try await mapFailure {
            let checkpoint = try await session.checkpoint()
            guard await ifCurrentOwner() else { throw CancellationError() }
            try await MainActor.run {
                try resumeStore.save(checkpoint, for: paneId)
            }
        }
    }

    package func prepareForApplicationBackground(
        ifCurrentOwner: @MainActor @Sendable @escaping () -> Bool
    ) async throws {
        try await mapFailure {
            let checkpoint = try await session.prepareForApplicationBackground()
            guard await ifCurrentOwner() else { throw CancellationError() }
            try await MainActor.run {
                try resumeStore.save(checkpoint, for: paneId)
            }
        }
    }

    package func resumeFromApplicationBackground() async {
        await session.resumeFromApplicationBackground()
    }

    package func preparedStartupPlan() async -> TerminalShellStartupPlan {
        await executor.preparedStartupPlan()
    }

    package func withBootstrapSSHClient<Result: Sendable>(
        _ operation: @Sendable (SSHClient) async throws -> Result
    ) async throws -> Result {
        try await mapFailure {
            try await executor.withConnectedClient(operation)
        }
    }

    package func close() async {
        await session.close()
    }

    private func mapFailure<Result: Sendable>(
        _ operation: @Sendable () async throws -> Result
    ) async throws -> Result {
        do {
            return try await operation()
        } catch is CancellationError {
            throw CancellationError()
        } catch {
            throw EternalTerminalVendorErrorMapper.failure(for: error)
        }
    }
}

nonisolated package enum EternalTerminalVendorErrorMapper {
    static package func state(for state: ETConnectionState) -> EternalTerminalSessionState {
        switch state {
        case .idle:
            .idle
        case .bootstrapping:
            .bootstrapping
        case .connecting:
            .connecting
        case .connected:
            .connected
        case .disconnected:
            .disconnected
        case .reconnecting:
            .reconnecting
        case .failed(let error):
            .failed(failure(for: error))
        case .closed:
            .closed
        }
    }

    static package func failure(for error: Error) -> EternalTerminalSessionFailure {
        if let failure = error as? EternalTerminalSessionFailure {
            return failure
        }
        if let resumeError = error as? EternalTerminalResumeCredentialError {
            return .resumeState(
                message: resumeError.localizedDescription,
                discardStoredState: resumeError.shouldDeleteStoredCredentials
            )
        }
        if let bootstrapError = error as? ETBootstrapError {
            return switch bootstrapError {
            case .sshFailed:
                .bootstrapSSH
            case .markerNotFound(let excerpt):
                .bootstrapResponse(excerpt)
            case .malformedCredentials:
                .malformedBootstrapCredentials
            }
        }
        guard let clientError = error as? ETClientError else { return .unknown }
        return switch clientError {
        case .transportFailure:
            .transport
        case .invalidKey:
            .invalidKey
        case .mismatchedProtocol:
            .protocolMismatch
        case .disconnectedBufferFull:
            .disconnectedBufferFull
        case .connectionInProgress:
            .connectionInProgress
        case .connectionClosed:
            .connectionClosed
        case .applicationSuspended:
            .applicationSuspended
        case .sessionUnrecoverable:
            .sessionUnrecoverable
        case .invalidPasskeyLength, .unexpectedConnectStatus, .initializationFailed,
             .malformedFrame, .invalidTerminalSize, .invalidTerminalPixels,
             .invalidTunnelSpecification, .forwardingFailure:
            .client
        }
    }
}

package extension EternalTerminalFailureAnalytics {
    nonisolated static package func analyticsCategory(for error: Error) -> String {
        analyticsCategory(
            for: EternalTerminalVendorErrorMapper.failure(for: error)
        )
    }
}

package extension EternalTerminalResumePolicy {
    nonisolated static package func shouldDiscardCredentials(after error: Error) -> Bool {
        shouldDiscardCredentials(
            after: EternalTerminalVendorErrorMapper.failure(for: error)
        )
    }
}
