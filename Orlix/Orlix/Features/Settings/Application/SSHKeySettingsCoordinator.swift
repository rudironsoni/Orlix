import Combine
import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated struct ImportedSSHKeyRequest: Equatable {
    let name: String
    let privateKey: Data
    let passphrase: String?
}

nonisolated struct GeneratedSSHKeyRequest: Equatable {
    let name: String
    let comment: String
    let passphrase: String?
    let keyType: SSHKeyType
}

nonisolated enum SSHKeySettingsFailure: Error, Equatable, Sendable {
    case keychain(status: Int32)
    case keychainEncodingFailed
    case keychainDecodingFailed
    case keychainItemNotFound
    case credentialServerMismatch
    case keychainCopyVerificationFailed
    case keyGenerationFailed
    case keyEncodingFailed
    case rsaExportFailed
    case invalidKeyData
    case unavailable
}

@MainActor
protocol SSHKeySettingsRepository: AnyObject {
    func loadKeys() -> [SSHKeyEntry]
    func storeImportedKey(_ request: ImportedSSHKeyRequest) throws(SSHKeySettingsFailure) -> SSHKeyEntry
    func generateAndStoreKey(_ request: GeneratedSSHKeyRequest) throws(SSHKeySettingsFailure) -> SSHKeyEntry
    func deleteKey(id: UUID) throws(SSHKeySettingsFailure)
}

@MainActor
final package class SSHKeySettingsCoordinator: ObservableObject {
    nonisolated package enum Operation: Hashable, Sendable {
        case importKey
        case generateKey
        case deleteKey
    }

    @Published private(set) var keys: [SSHKeyEntry] = []
    @Published private(set) var operationFailures: [Operation: SSHKeySettingsFailure] = [:]

    private let repository: any SSHKeySettingsRepository

    package init(repository: any SSHKeySettingsRepository) {
        self.repository = repository
    }

    package func loadKeys() {
        keys = repository.loadKeys()
    }

    package func failure(for operation: Operation) -> SSHKeySettingsFailure? {
        operationFailures[operation]
    }

    package func clearFailure(for operation: Operation) {
        operationFailures.removeValue(forKey: operation)
    }

    package func storeImportedKey(
        name: String,
        privateKey: Data,
        passphrase: String?
    ) -> SSHKeyEntry? {
        do {
            let entry = try repository.storeImportedKey(ImportedSSHKeyRequest(
                name: name,
                privateKey: privateKey,
                passphrase: passphrase
            ))
            clearFailure(for: .importKey)
            loadKeys()
            return entry
        } catch let failure {
            operationFailures[.importKey] = failure
            return nil
        }
    }

    package func generateAndStoreKey(
        name: String,
        passphrase: String?,
        keyType: SSHKeyType
    ) -> SSHKeyEntry? {
        do {
            let entry = try repository.generateAndStoreKey(GeneratedSSHKeyRequest(
                name: name,
                comment: name.replacingOccurrences(of: " ", with: "_"),
                passphrase: passphrase,
                keyType: keyType
            ))
            clearFailure(for: .generateKey)
            loadKeys()
            return entry
        } catch let failure {
            operationFailures[.generateKey] = failure
            return nil
        }
    }

    package func deleteKey(_ key: SSHKeyEntry) {
        do {
            try repository.deleteKey(id: key.id)
            clearFailure(for: .deleteKey)
            loadKeys()
        } catch let failure {
            operationFailures[.deleteKey] = failure
        }
    }
}
