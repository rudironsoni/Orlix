import ETBootstrap
import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
import OrlixSSH
#endif
nonisolated package enum EternalTerminalHostCompatibility: Equatable {
    case supported
    case unsupportedNativeWindows
    case unsupportedShell

    package init(environment: RemoteEnvironment) {
        if environment.platform == .windows {
            self = .unsupportedNativeWindows
        } else if environment.shellProfile.supportsPOSIXExecWrapper {
            self = .supported
        } else {
            self = .unsupportedShell
        }
    }

    package var bootstrapDiagnostic: String? {
        switch self {
        case .supported:
            nil
        case .unsupportedNativeWindows:
            "VVTERM_ET_UNSUPPORTED_NATIVE_WINDOWS"
        case .unsupportedShell:
            "VVTERM_ET_REQUIRES_POSIX_SHELL"
        }
    }
}

/// Runs swift-et's SSH bootstrap and auxiliary remote operations through Orlix's SSH stack.
package actor SSHETBootstrapExecutor: ETBootstrapExecutor {
    nonisolated static package var bootstrapOptions: ETBootstrapOptions {
        ETBootstrapOptions(etterminalPath: "etterminal --logtostdout")
    }

    private let client: SSHClient
    private let connection: Connection?
    private let startupPlanProvider: (@Sendable (SSHClient) async throws -> TerminalShellStartupPlan)?
    private var startupPlan: TerminalShellStartupPlan = .plainShell
    private var terminalType = RemoteTerminalBootstrap.defaultTerminalType

    private struct Connection: Sendable {
        package let server: Server
        package let credentials: ServerCredentials
    }

    package init(
        server: Server,
        credentials: ServerCredentials,
        client: SSHClient,
        startupPlanProvider: (@Sendable (SSHClient) async throws -> TerminalShellStartupPlan)? = nil
    ) {
        self.client = client
        connection = Connection(server: server, credentials: credentials)
        self.startupPlanProvider = startupPlanProvider
    }

    /// Used while a caller already owns the temporary SSH connection lifecycle.
    package init(connectedClient: SSHClient) {
        client = connectedClient
        connection = nil
        startupPlanProvider = nil
    }

    package func preparedStartupPlan() -> TerminalShellStartupPlan {
        startupPlan
    }

    package func preparedTerminalType() -> RemoteTerminalType {
        terminalType
    }

    package func run(command: String) async throws -> String {
        let command = Self.remoteBootstrapCommand(command)
        guard let connection else {
            return try await client.execute(command, timeout: .seconds(20))
        }

        do {
            _ = try await client.connect(
                to: connection.server,
                credentials: connection.credentials
            )
            let compatibility = EternalTerminalHostCompatibility(
                environment: await client.remoteEnvironment()
            )
            if let diagnostic = compatibility.bootstrapDiagnostic {
                await client.disconnect()
                return diagnostic
            }
            terminalType = await client.remoteTerminalType()
            if let startupPlanProvider {
                startupPlan = try await startupPlanProvider(client)
            }
            let output = try await client.execute(command, timeout: .seconds(20))
            await client.disconnect()
            return output
        } catch {
            await client.disconnect()
            throw error
        }
    }

    /// Use a known POSIX shell even when the account's login shell is fish, and
    /// make common package-manager locations available to non-interactive SSH.
    nonisolated static package func remoteBootstrapCommand(_ command: String) -> String {
        let script = """
        \(RemoteTerminalBootstrap.shellPathExport());
        if ! command -v etterminal >/dev/null 2>&1; then
          printf 'etterminal was not found in the remote PATH';
          exit 127;
        fi;
        \(command)
        """
        return RemoteTerminalBootstrap.wrapPOSIXShellCommand(script)
    }

    package func withConnectedClient<Result: Sendable>(
        _ operation: @Sendable (SSHClient) async throws -> Result
    ) async throws -> Result {
        guard let connection else {
            return try await operation(client)
        }
        return try await withConnectedClient(connection: connection, operation)
    }

    private func withConnectedClient<Result: Sendable>(
        connection: Connection,
        _ operation: @Sendable (SSHClient) async throws -> Result
    ) async throws -> Result {
        do {
            _ = try await client.connect(
                to: connection.server,
                credentials: connection.credentials
            )
            let result = try await operation(client)
            await client.disconnect()
            return result
        } catch {
            await client.disconnect()
            throw error
        }
    }
}
