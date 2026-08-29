import Foundation

enum DeviceIdentity {
    private static let storageKey = "orlix.deviceId"
    private static let keychain = KeychainStore(service: "com.rudironsoni.orlix")

    static let id: String = {
        let storedValue = (try? keychain.getString(storageKey, scope: .deviceOnly)) ?? nil
        if let value = storedValue, !value.isEmpty {
            return value
        }

        let defaults = UserDefaults.standard
        if let existing = defaults.string(forKey: storageKey), !existing.isEmpty {
            try? keychain.setString(existing, forKey: storageKey, scope: .deviceOnly)
            return existing
        }

        let newId = UUID().uuidString
        try? keychain.setString(newId, forKey: storageKey, scope: .deviceOnly)
        defaults.set(newId, forKey: storageKey)
        return newId
    }()
}
