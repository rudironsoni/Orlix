import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
@MainActor
package protocol ServerFormCredentialLoading: AnyObject {
    func loadFormCredentials(for server: Server?) async throws -> ServerFormCredentialLoad
    func loadStoredKey(_ entry: SSHKeyEntry) async throws -> ServerFormStoredKeyLoad
}

@MainActor
package struct ServerFormCredentialLoad {
    package enum SavedCredentials {
        case notRequested
        case loaded(ServerCredentials, selectedStoredKeyID: UUID?)
        case failed(message: String)
    }

    package let storedKeys: [SSHKeyEntry]
    package let savedCredentials: SavedCredentials
}

@MainActor
package struct ServerFormStoredKeyLoad {
    package let id: UUID
    package let privateKey: String?
    package let passphrase: String?
    package let publicKey: String
}

@MainActor
final package class ServerFormCredentialLoader: ServerFormCredentialLoading {
    private let repository: any ServerCredentialRepository

    package init(repository: any ServerCredentialRepository) {
        self.repository = repository
    }

    package func loadFormCredentials(for server: Server?) async throws -> ServerFormCredentialLoad {
        await Task.yield()
        try Task.checkCancellation()

        let storedKeys = repository.getStoredSSHKeys()
        guard let server else {
            return ServerFormCredentialLoad(
                storedKeys: storedKeys,
                savedCredentials: .notRequested
            )
        }

        do {
            let credentials = try repository.getCredentials(for: server)
            let selectedStoredKeyID = matchingStoredKeyID(
                for: server,
                credentials: credentials,
                storedKeys: storedKeys
            )
            try Task.checkCancellation()
            return ServerFormCredentialLoad(
                storedKeys: storedKeys,
                savedCredentials: .loaded(
                    credentials,
                    selectedStoredKeyID: selectedStoredKeyID
                )
            )
        } catch is CancellationError {
            throw CancellationError()
        } catch {
            return ServerFormCredentialLoad(
                storedKeys: storedKeys,
                savedCredentials: .failed(message: error.localizedDescription)
            )
        }
    }

    package func loadStoredKey(_ entry: SSHKeyEntry) async throws -> ServerFormStoredKeyLoad {
        await Task.yield()
        try Task.checkCancellation()
        let keyData = try repository.getStoredSSHKeyData(for: entry.id)
        try Task.checkCancellation()
        return ServerFormStoredKeyLoad(
            id: entry.id,
            privateKey: keyData.flatMap { String(data: $0.key, encoding: .utf8) },
            passphrase: keyData?.passphrase,
            publicKey: entry.publicKey ?? ""
        )
    }

    private func matchingStoredKeyID(
        for server: Server,
        credentials: ServerCredentials,
        storedKeys: [SSHKeyEntry]
    ) -> UUID? {
        guard server.authMethod != .password,
              let privateKey = credentials.privateKey else {
            return nil
        }

        let passphrase = server.authMethod == .sshKeyWithPassphrase
            ? credentials.passphrase ?? ""
            : ""
        for entry in storedKeys {
            guard let keyData = try? repository.getStoredSSHKeyData(for: entry.id),
                  keyData.key == privateKey else {
                continue
            }
            if let storedPassphrase = keyData.passphrase,
               !storedPassphrase.isEmpty,
               storedPassphrase != passphrase {
                continue
            }
            return entry.id
        }
        return nil
    }
}
