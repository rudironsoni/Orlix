import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
@MainActor
private final class KeychainSSHKeySettingsRepository: SSHKeySettingsRepository {
    private let keychain: KeychainManager

    package init(keychain: KeychainManager) {
        self.keychain = keychain
    }

    package func loadKeys() -> [SSHKeyEntry] {
        keychain.getStoredSSHKeys()
    }

    package func storeImportedKey(_ request: ImportedSSHKeyRequest) throws(SSHKeySettingsFailure) -> SSHKeyEntry {
        do {
            return try keychain.storeSSHKeyEntry(
                name: request.name,
                privateKey: request.privateKey,
                passphrase: request.passphrase
            )
        } catch {
            throw Self.failure(for: error)
        }
    }

    package func generateAndStoreKey(_ request: GeneratedSSHKeyRequest) throws(SSHKeySettingsFailure) -> SSHKeyEntry {
        do {
            let key = try SSHKeyGenerator.generate(
                type: request.keyType,
                comment: request.comment
            )
            return try keychain.storeSSHKeyEntry(
                name: request.name,
                privateKey: key.privateKey,
                passphrase: request.passphrase,
                keyType: key.keyType,
                publicKey: key.publicKey
            )
        } catch {
            throw Self.failure(for: error)
        }
    }

    package func deleteKey(id: UUID) throws(SSHKeySettingsFailure) {
        do {
            try keychain.deleteStoredSSHKey(id)
        } catch {
            throw Self.failure(for: error)
        }
    }

    private static func failure(for error: Error) -> SSHKeySettingsFailure {
        if let keychainError = error as? KeychainError {
            return switch keychainError {
            case .unhandled(let status): .keychain(status: status)
            case .encodingFailed: .keychainEncodingFailed
            case .decodingFailed: .keychainDecodingFailed
            case .itemNotFound: .keychainItemNotFound
            case .credentialServerMismatch: .credentialServerMismatch
            case .copyVerificationFailed: .keychainCopyVerificationFailed
            }
        }
        if let generationError = error as? SSHKeyGeneratorError {
            return switch generationError {
            case .keyGenerationFailed: .keyGenerationFailed
            case .encodingFailed: .keyEncodingFailed
            case .rsaExportFailed: .rsaExportFailed
            case .invalidKeyData: .invalidKeyData
            }
        }
        return .unavailable
    }
}

@MainActor
package enum SSHKeySettingsLiveComposition {
    static package func makeCoordinator(keychain: KeychainManager) -> SSHKeySettingsCoordinator {
        SSHKeySettingsCoordinator(
            repository: KeychainSSHKeySettingsRepository(keychain: keychain)
        )
    }
}

#if DEBUG
@MainActor
private final class PreviewSSHKeySettingsRepository: SSHKeySettingsRepository {
    package func loadKeys() -> [SSHKeyEntry] { [] }

    package func storeImportedKey(_ request: ImportedSSHKeyRequest) throws(SSHKeySettingsFailure) -> SSHKeyEntry {
        throw .unavailable
    }

    package func generateAndStoreKey(_ request: GeneratedSSHKeyRequest) throws(SSHKeySettingsFailure) -> SSHKeyEntry {
        throw .unavailable
    }

    package func deleteKey(id: UUID) throws(SSHKeySettingsFailure) {
        throw .unavailable
    }
}

package extension SSHKeySettingsCoordinator {
    static package var preview: SSHKeySettingsCoordinator {
        SSHKeySettingsCoordinator(repository: PreviewSSHKeySettingsRepository())
    }
}
#endif
