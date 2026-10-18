import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package enum ServerMutation: Equatable, Sendable {
    case create(Server)
    case update(Server)

    package var server: Server {
        switch self {
        case .create(let server), .update(let server):
            return server
        }
    }
}

@MainActor
package protocol ServerMutationRepository: AnyObject {
    func validate(_ mutation: ServerMutation, hasProAccess: Bool) throws
    func server(id: UUID) -> Server?
    func apply(
        _ mutation: ServerMutation,
        credentials: ServerCredentials
    ) async throws -> Server
}

@MainActor
package protocol ServerCredentialStoring: AnyObject {
    func storeCredentials(_ credentials: ServerCredentials, for server: Server) throws
}

@MainActor
package protocol ServerCredentialTransactionRepository: ServerCredentialStoring {
    func getCredentials(for server: Server) throws -> ServerCredentials
    func deleteCredentials(for serverID: UUID) throws
}

@MainActor
package protocol ServerCredentialRepository: ServerCredentialTransactionRepository {
    func getStoredSSHKeys() -> [SSHKeyEntry]
    func getStoredSSHKeyData(for id: UUID) throws -> (key: Data, passphrase: String?)?
}

@MainActor
package struct ServerSaveUseCase {
    private let mutations: any ServerMutationRepository

    package init(mutations: any ServerMutationRepository) {
        self.mutations = mutations
    }

    package func execute(
        _ mutation: ServerMutation,
        credentials newCredentials: ServerCredentials,
        hasProAccess: Bool
    ) async throws -> Server {
        try mutations.validate(mutation, hasProAccess: hasProAccess)
        return try await mutations.apply(mutation, credentials: newCredentials)
    }
}
