import Foundation
import Security

nonisolated struct TSSHResumeState: Codable, Equatable, Sendable {
    let serverIdentity: TSSHResumeServerIdentity
    let sshHostKeyFingerprint: String
    let serverProcess: TSSHServerProcessIdentity
    let host: String
    let info: TSSHServerInfo
    let sessionID: Int64
    let profile: TSSHProfile
    let savedAt: Date

    var isExpired: Bool {
        Date().timeIntervalSince(savedAt) >= 86_400
    }

    func refreshed(at date: Date) -> Self {
        Self(
            serverIdentity: serverIdentity,
            sshHostKeyFingerprint: sshHostKeyFingerprint,
            serverProcess: serverProcess,
            host: host,
            info: info,
            sessionID: sessionID,
            profile: profile,
            savedAt: date
        )
    }
}

nonisolated struct TSSHResumeCleanupState: Codable, Equatable, Sendable {
    let serverIdentity: TSSHResumeServerIdentity
    let serverProcess: TSSHServerProcessIdentity
    let createdAt: Date
}

nonisolated struct TSSHResumeServerIdentity: Codable, Equatable, Sendable {
    let id: UUID
    let host: String
    let port: Int
    let username: String
    let updatedAt: Date

    init(server: Server) {
        id = server.id
        host = server.host.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        port = server.port
        username = server.username.trimmingCharacters(in: .whitespacesAndNewlines)
        updatedAt = server.updatedAt
    }
}

nonisolated enum TSSHResumeCompatibilityPolicy {
    static func canResume(
        _ state: TSSHResumeState,
        with server: Server,
        trustedHostFingerprint: String?
    ) -> Bool {
        state.serverIdentity == TSSHResumeServerIdentity(server: server)
            && state.profile == server.tsshProfile
            && !state.sshHostKeyFingerprint.isEmpty
            && state.sshHostKeyFingerprint == trustedHostFingerprint
    }
}

nonisolated protocol TSSHResumeStoring: Sendable {
    func load(for paneID: UUID) throws -> TSSHResumeState?
    func hasCheckpoint(for paneID: UUID) -> Bool
    func save(_ state: TSSHResumeState, for paneID: UUID) throws
    func delete(for paneID: UUID) throws
    func loadCleanup(for paneID: UUID) throws -> TSSHResumeCleanupState?
    func saveCleanup(_ state: TSSHResumeCleanupState, for paneID: UUID) throws
    func deleteCleanup(for paneID: UUID) throws
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
        let serverIdentity: TSSHResumeServerIdentity
        let sshHostKeyFingerprint: String
        let serverProcess: TSSHServerProcessIdentity
        let host: String
        let serverVersion: String
        let protocolVersion: Int
        let port: Int
        let mode: TSSHServerInfo.Mode
        let proxyMode: String
        let mtu: Int
        let clientID: UInt64
        let serverID: UInt64
        let sessionID: Int64
        let profile: TSSHProfile
        let savedAt: Date

        private enum CodingKeys: String, CodingKey {
            case serverIdentity, sshHostKeyFingerprint, serverProcess, host
            case serverVersion, protocolVersion
            case port, mode, proxyMode, mtu
            case clientID, serverID, sessionID, profile, savedAt
        }

        init(
            serverIdentity: TSSHResumeServerIdentity,
            sshHostKeyFingerprint: String,
            serverProcess: TSSHServerProcessIdentity,
            host: String,
            serverVersion: String,
            protocolVersion: Int,
            port: Int,
            mode: TSSHServerInfo.Mode,
            proxyMode: String,
            mtu: Int,
            clientID: UInt64,
            serverID: UInt64,
            sessionID: Int64,
            profile: TSSHProfile,
            savedAt: Date
        ) {
            self.serverIdentity = serverIdentity
            self.sshHostKeyFingerprint = sshHostKeyFingerprint
            self.serverProcess = serverProcess
            self.host = host
            self.serverVersion = serverVersion
            self.protocolVersion = protocolVersion
            self.port = port
            self.mode = mode
            self.proxyMode = proxyMode
            self.mtu = mtu
            self.clientID = clientID
            self.serverID = serverID
            self.sessionID = sessionID
            self.profile = profile
            self.savedAt = savedAt
        }

        init(from decoder: Decoder) throws {
            let container = try decoder.container(keyedBy: CodingKeys.self)
            serverIdentity = try container.decode(TSSHResumeServerIdentity.self, forKey: .serverIdentity)
            sshHostKeyFingerprint = try container.decodeIfPresent(
                String.self,
                forKey: .sshHostKeyFingerprint
            ) ?? ""
            serverProcess = try container.decode(
                TSSHServerProcessIdentity.self,
                forKey: .serverProcess
            )
            host = try container.decode(String.self, forKey: .host)
            serverVersion = try container.decode(String.self, forKey: .serverVersion)
            protocolVersion = try container.decodeIfPresent(Int.self, forKey: .protocolVersion) ?? 0
            port = try container.decode(Int.self, forKey: .port)
            mode = try container.decode(TSSHServerInfo.Mode.self, forKey: .mode)
            proxyMode = try container.decodeIfPresent(String.self, forKey: .proxyMode) ?? ""
            mtu = try container.decodeIfPresent(Int.self, forKey: .mtu) ?? 0
            clientID = try container.decode(UInt64.self, forKey: .clientID)
            serverID = try container.decode(UInt64.self, forKey: .serverID)
            sessionID = try container.decode(Int64.self, forKey: .sessionID)
            profile = try container.decode(TSSHProfile.self, forKey: .profile)
            savedAt = try container.decode(Date.self, forKey: .savedAt)
        }
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
            protocolVersion: checkpoint.protocolVersion,
            port: checkpoint.port,
            mode: checkpoint.mode,
            serverCertHex: secret.serverCertHex,
            clientCertHex: secret.clientCertHex,
            clientKeyHex: secret.clientKeyHex,
            kcpPassHex: secret.kcpPassHex,
            kcpSaltHex: secret.kcpSaltHex,
            proxyKeyHex: secret.proxyKeyHex,
            proxyMode: checkpoint.proxyMode,
            mtu: checkpoint.mtu,
            clientID: checkpoint.clientID,
            serverID: checkpoint.serverID
        )
        guard info.hasRequiredCredentials else { throw TSSHResumeStoreError.corruptState }
        let state = TSSHResumeState(
            serverIdentity: checkpoint.serverIdentity,
            sshHostKeyFingerprint: checkpoint.sshHostKeyFingerprint,
            serverProcess: checkpoint.serverProcess,
            host: checkpoint.host,
            info: info,
            sessionID: checkpoint.sessionID,
            profile: checkpoint.profile,
            savedAt: checkpoint.savedAt
        )
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
            serverIdentity: state.serverIdentity,
            sshHostKeyFingerprint: state.sshHostKeyFingerprint,
            serverProcess: state.serverProcess,
            host: state.host,
            serverVersion: state.info.serverVersion,
            protocolVersion: state.info.protocolVersion,
            port: state.info.port,
            mode: state.info.mode,
            proxyMode: state.info.proxyMode,
            mtu: state.info.mtu,
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

    func loadCleanup(for paneID: UUID) throws -> TSSHResumeCleanupState? {
        let cleanupURL = cleanupURL(for: paneID)
        guard fileManager.fileExists(atPath: cleanupURL.path) else { return nil }
        do {
            return try JSONDecoder().decode(
                TSSHResumeCleanupState.self,
                from: Data(contentsOf: cleanupURL)
            )
        } catch {
            throw TSSHResumeStoreError.corruptState
        }
    }

    func saveCleanup(_ state: TSSHResumeCleanupState, for paneID: UUID) throws {
        do {
            try fileManager.createDirectory(
                at: root,
                withIntermediateDirectories: true,
                attributes: [.protectionKey: FileProtectionType.completeUntilFirstUserAuthentication]
            )
            let cleanupURL = cleanupURL(for: paneID)
            try JSONEncoder().encode(state).write(to: cleanupURL, options: .atomic)
            try fileManager.setAttributes(
                [
                    .posixPermissions: 0o600,
                    .protectionKey: FileProtectionType.completeUntilFirstUserAuthentication,
                ],
                ofItemAtPath: cleanupURL.path
            )
        } catch {
            throw TSSHResumeStoreError.checkpointStorage
        }
    }

    func deleteCleanup(for paneID: UUID) throws {
        let cleanupURL = cleanupURL(for: paneID)
        if fileManager.fileExists(atPath: cleanupURL.path) {
            try fileManager.removeItem(at: cleanupURL)
        }
    }

    private func url(for paneID: UUID) -> URL {
        root.appendingPathComponent(paneID.uuidString).appendingPathExtension("json")
    }

    private func cleanupURL(for paneID: UUID) -> URL {
        root.appendingPathComponent(paneID.uuidString).appendingPathExtension("cleanup.json")
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
