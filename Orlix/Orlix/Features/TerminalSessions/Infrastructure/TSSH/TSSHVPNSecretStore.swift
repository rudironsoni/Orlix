import Foundation
import Security

nonisolated enum TSSHVPNSecretStoreError: LocalizedError, Sendable {
    case keychain(OSStatus)
    case missing

    var errorDescription: String? {
        switch self {
        case .keychain(let status):
            return "TSSH VPN Keychain access failed with status \(status)."
        case .missing:
            return "The TSSH VPN secret is unavailable."
        }
    }
}

nonisolated enum TSSHVPNSecretStore {
    private static let service = "com.rudironsoni.orlix.tssh-vpn"

    static func save(_ value: String, key: String) throws {
        guard UUID(uuidString: key) != nil,
              let data = value.data(using: .utf8) else {
            throw TSSHVPNSecretStoreError.missing
        }
        let query = baseQuery(key: key)
        let attributes = [kSecValueData as String: data]
        let updateStatus = SecItemUpdate(query as CFDictionary, attributes as CFDictionary)
        if updateStatus == errSecSuccess { return }
        guard updateStatus == errSecItemNotFound else {
            throw TSSHVPNSecretStoreError.keychain(updateStatus)
        }
        var insert = query
        insert[kSecValueData as String] = data
        insert[kSecAttrAccessible as String] = kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly
        let insertStatus = SecItemAdd(insert as CFDictionary, nil)
        guard insertStatus == errSecSuccess else {
            throw TSSHVPNSecretStoreError.keychain(insertStatus)
        }
    }

    static func load(key: String) throws -> String {
        guard UUID(uuidString: key) != nil else { throw TSSHVPNSecretStoreError.missing }
        var query = baseQuery(key: key)
        query[kSecReturnData as String] = true
        query[kSecMatchLimit as String] = kSecMatchLimitOne
        var result: CFTypeRef?
        let status = SecItemCopyMatching(query as CFDictionary, &result)
        guard status != errSecItemNotFound else { throw TSSHVPNSecretStoreError.missing }
        guard status == errSecSuccess else {
            throw TSSHVPNSecretStoreError.keychain(status)
        }
        guard let data = result as? Data,
              let value = String(data: data, encoding: .utf8) else {
            throw TSSHVPNSecretStoreError.missing
        }
        return value
    }

    static func delete(key: String) throws {
        guard UUID(uuidString: key) != nil else { throw TSSHVPNSecretStoreError.missing }
        let status = SecItemDelete(baseQuery(key: key) as CFDictionary)
        guard status == errSecSuccess || status == errSecItemNotFound else {
            throw TSSHVPNSecretStoreError.keychain(status)
        }
    }

    private static func baseQuery(key: String) -> [String: Any] {
        [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: key,
            kSecAttrSynchronizable as String: kCFBooleanFalse as Any,
        ]
    }
}
