import Foundation
import Security

nonisolated struct TSSHResumeState: Codable, Equatable, Sendable {
    let host: String
    let info: TSSHServerInfo
    let sessionID: Int64
    let profile: TSSHProfile
    let savedAt: Date

    var isExpired: Bool {
        Date().timeIntervalSince(savedAt) >= 86_400
    }
}

nonisolated protocol TSSHResumeStoring: Sendable {
    func load(for paneID: UUID) throws -> TSSHResumeState?
    func hasCheckpoint(for paneID: UUID) -> Bool
    func save(_ state: TSSHResumeState, for paneID: UUID) throws
    func delete(for paneID: UUID) throws
}

nonisolated enum TSSHResumeStoreError: LocalizedError, Sendable {
    case secureStorage(OSStatus)
    case checkpointStorage
    case corruptState

    var errorDescription: String? {
        switch self {
        case .secureStorage(let status):
            return "TSSH Keychain access failed with status \(status)."
        case .checkpointStorage:
            return "TSSH recovery metadata could not be stored."
        case .corruptState:
            return "TSSH recovery data is damaged."
        }
    }
}

nonisolated final class TSSHResumeStore: TSSHResumeStoring, @unchecked Sendable {
    static let shared = TSSHResumeStore()

    private struct Secret: Codable {
        let serverCertHex: String?
        let clientCertHex: String?
        let clientKeyHex: String?
        let kcpPassHex: String?
        let kcpSaltHex: String?
        let proxyKeyHex: String?
    }

    private struct Checkpoint: Codable {
        let host: String
        let serverVersion: String
        let port: Int
        let mode: TSSHServerInfo.Mode
        let clientID: Int64
        let serverID: Int64
        let sessionID: Int64
        let profile: TSSHProfile
        let savedAt: Date
    }

    private let fileManager: FileManager
    private let root: URL
    private let service = "com.rudironsoni.orlix.tssh-resume"

    init(fileManager: FileManager = .default) {
        self.fileManager = fileManager
        let base = fileManager.urls(for: .applicationSupportDirectory, in: .userDomainMask).first
            ?? fileManager.temporaryDirectory
        root = base.appendingPathComponent("TSSHResume", isDirectory: true)
    }

    func load(for paneID: UUID) throws -> TSSHResumeState? {
        let checkpointURL = url(for: paneID)
        guard fileManager.fileExists(atPath: checkpointURL.path) else { return nil }
        let checkpoint: Checkpoint
        do {
            checkpoint = try JSONDecoder().decode(
                Checkpoint.self,
                from: Data(contentsOf: checkpointURL)
            )
        } catch {
            throw TSSHResumeStoreError.corruptState
        }
        guard let secretData = try readSecret(account: paneID.uuidString) else {
            throw TSSHResumeStoreError.corruptState
        }
        let secret: Secret
        do { secret = try JSONDecoder().decode(Secret.self, from: secretData) }
        catch { throw TSSHResumeStoreError.corruptState }

        let info = TSSHServerInfo(
            serverVersion: checkpoint.serverVersion,
            port: checkpoint.port,
            mode: checkpoint.mode,
            serverCertHex: secret.serverCertHex,
            clientCertHex: secret.clientCertHex,
            clientKeyHex: secret.clientKeyHex,
            kcpPassHex: secret.kcpPassHex,
            kcpSaltHex: secret.kcpSaltHex,
            proxyKeyHex: secret.proxyKeyHex,
            clientID: checkpoint.clientID,
            serverID: checkpoint.serverID
        )
        guard info.hasRequiredCredentials else { throw TSSHResumeStoreError.corruptState }
        let state = TSSHResumeState(
            host: checkpoint.host,
            info: info,
            sessionID: checkpoint.sessionID,
            profile: checkpoint.profile,
            savedAt: checkpoint.savedAt
        )
        if state.isExpired {
            try? delete(for: paneID)
            return nil
        }
        return state
    }

    func hasCheckpoint(for paneID: UUID) -> Bool {
        fileManager.fileExists(atPath: url(for: paneID).path)
    }

    func save(_ state: TSSHResumeState, for paneID: UUID) throws {
        guard state.info.hasRequiredCredentials,
              state.sessionID > 0,
              state.profile.isValid else { throw TSSHResumeStoreError.corruptState }
        let secret = Secret(
            serverCertHex: state.info.serverCertHex,
            clientCertHex: state.info.clientCertHex,
            clientKeyHex: state.info.clientKeyHex,
            kcpPassHex: state.info.kcpPassHex,
            kcpSaltHex: state.info.kcpSaltHex,
            proxyKeyHex: state.info.proxyKeyHex
        )
        try writeSecret(try JSONEncoder().encode(secret), account: paneID.uuidString)
        let checkpoint = Checkpoint(
            host: state.host,
            serverVersion: state.info.serverVersion,
            port: state.info.port,
            mode: state.info.mode,
            clientID: state.info.clientID,
            serverID: state.info.serverID,
            sessionID: state.sessionID,
            profile: state.profile,
            savedAt: state.savedAt
        )
        do {
            try fileManager.createDirectory(
                at: root,
                withIntermediateDirectories: true,
                attributes: [.protectionKey: FileProtectionType.completeUntilFirstUserAuthentication]
            )
            let data = try JSONEncoder().encode(checkpoint)
            try data.write(to: url(for: paneID), options: .atomic)
            try fileManager.setAttributes(
                [
                    .posixPermissions: 0o600,
                    .protectionKey: FileProtectionType.completeUntilFirstUserAuthentication,
                ],
                ofItemAtPath: url(for: paneID).path
            )
        } catch {
            try? deleteSecret(account: paneID.uuidString)
            if fileManager.fileExists(atPath: url(for: paneID).path) {
                try? fileManager.removeItem(at: url(for: paneID))
            }
            throw TSSHResumeStoreError.checkpointStorage
        }
    }

    func delete(for paneID: UUID) throws {
        try deleteSecret(account: paneID.uuidString)
        let checkpoint = url(for: paneID)
        if fileManager.fileExists(atPath: checkpoint.path) {
            try fileManager.removeItem(at: checkpoint)
        }
    }

    private func url(for paneID: UUID) -> URL {
        root.appendingPathComponent(paneID.uuidString).appendingPathExtension("json")
    }

    private func readSecret(account: String) throws -> Data? {
        var query = baseQuery(account: account)
        query[kSecReturnData as String] = true
        query[kSecMatchLimit as String] = kSecMatchLimitOne
        var result: CFTypeRef?
        let status = SecItemCopyMatching(query as CFDictionary, &result)
        if status == errSecItemNotFound { return nil }
        guard status == errSecSuccess, let data = result as? Data else {
            throw TSSHResumeStoreError.secureStorage(status)
        }
        return data
    }

    private func writeSecret(_ data: Data, account: String) throws {
        let query = baseQuery(account: account)
        let attributes: [String: Any] = [kSecValueData as String: data]
        let update = SecItemUpdate(query as CFDictionary, attributes as CFDictionary)
        if update == errSecSuccess { return }
        guard update == errSecItemNotFound else {
            throw TSSHResumeStoreError.secureStorage(update)
        }
        var insert = query
        insert[kSecValueData as String] = data
        insert[kSecAttrAccessible as String] = kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly
        let status = SecItemAdd(insert as CFDictionary, nil)
        guard status == errSecSuccess else {
            throw TSSHResumeStoreError.secureStorage(status)
        }
    }

    private func deleteSecret(account: String) throws {
        let status = SecItemDelete(baseQuery(account: account) as CFDictionary)
        guard status == errSecSuccess || status == errSecItemNotFound else {
            throw TSSHResumeStoreError.secureStorage(status)
        }
    }

    private func baseQuery(account: String) -> [String: Any] {
        [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: account,
            kSecAttrSynchronizable as String: kCFBooleanFalse as Any,
        ]
    }
}
