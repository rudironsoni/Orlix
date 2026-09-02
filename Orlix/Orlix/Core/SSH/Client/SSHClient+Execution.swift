import Foundation

extension SSHClient {
    // MARK: - Command Execution

    func execute(
        _ command: String,
        timeout: Duration? = nil,
        maxOutputBytes: Int = SSHExecOutputBudget.defaultMaximumBytes,
        retainPartialOutputOnFailure: Bool = false
    ) async throws -> String {
        guard !isAborted else {
            throw SSHError.notConnected
        }
        guard let session = session else {
            throw SSHError.notConnected
        }
        let effectiveTimeout = timeout ?? execTimeout
        let partialOutputCapture = retainPartialOutputOnFailure
            ? SSHCommandPartialOutputCapture()
            : nil
        return try await SSHClient.runCommandWithDeadline(
            effectiveTimeout,
            partialOutputCapture: partialOutputCapture,
            onTimeout: { session.abort() }
        ) {
            try Task.checkCancellation()
            return try await session.execute(
                command,
                maxOutputBytes: maxOutputBytes,
                retainPartialOutputOnFailure: retainPartialOutputOnFailure,
                partialOutputCapture: partialOutputCapture
            )
        }
    }

    nonisolated static func runCommandWithDeadline(
        _ timeout: Duration,
        partialOutputCapture: SSHCommandPartialOutputCapture?,
        onTimeout: @escaping @Sendable () -> Void = {},
        operation: @escaping @Sendable () async throws -> String
    ) async throws -> String {
        do {
            return try await runWithDeadline(
                timeout,
                onTimeout: onTimeout,
                operation: operation
            )
        } catch {
            guard !(error is SSHCommandExecutionError),
                  let partialOutputCapture else {
                throw error
            }
            let partialOutput = partialOutputCapture.output()
            guard !partialOutput.isEmpty else { throw error }
            throw SSHCommandExecutionError(
                underlyingDescription: error.localizedDescription,
                partialOutput: partialOutput
            )
        }
    }

    func remoteEndpointHost() async -> String? {
        await session?.remoteEndpointHost()
    }
}
