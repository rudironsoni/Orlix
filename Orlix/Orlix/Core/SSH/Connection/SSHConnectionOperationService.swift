#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package actor SSHConnectionOperationService {
    private let clientFactory: SSHClientFactory

    package init(clientFactory: SSHClientFactory) {
        self.clientFactory = clientFactory
    }

    package func runWithConnection<T: Sendable>(
        using client: SSHClient,
        server: Server,
        credentials: ServerCredentials,
        disconnectWhenDone: Bool = false,
        operation: @escaping @Sendable (SSHClient) async throws -> T
    ) async throws -> T {
        do {
            _ = try await client.connect(to: server, credentials: credentials)
            let result = try await operation(client)
            if disconnectWhenDone {
                await client.disconnect()
            }
            return result
        } catch {
            if disconnectWhenDone {
                await client.disconnect()
            }
            throw error
        }
    }

    package func withTemporaryConnection<T: Sendable>(
        server: Server,
        credentials: ServerCredentials,
        operation: @escaping @Sendable (SSHClient) async throws -> T
    ) async throws -> T {
        let client = clientFactory.makeClient()
        return try await runWithConnection(
            using: client,
            server: server,
            credentials: credentials,
            disconnectWhenDone: true,
            operation: operation
        )
    }
}
