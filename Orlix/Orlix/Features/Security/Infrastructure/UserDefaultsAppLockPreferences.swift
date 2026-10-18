import Foundation

@MainActor
final package class UserDefaultsAppLockPreferences: AppLockPreferences {
    private enum Key {
        static package let fullAppLockEnabled = "security.fullAppLockEnabled"
        static package let lockOnBackground = "security.lockOnBackground"
        static package let authGraceSeconds = "security.authGraceSeconds"
    }

    private let defaults: UserDefaults

    package init(defaults: UserDefaults) {
        self.defaults = defaults
    }

    package var fullAppLockEnabled: Bool? {
        get { defaults.object(forKey: Key.fullAppLockEnabled) as? Bool }
        set { set(newValue, forKey: Key.fullAppLockEnabled) }
    }

    package var lockOnBackground: Bool? {
        get { defaults.object(forKey: Key.lockOnBackground) as? Bool }
        set { set(newValue, forKey: Key.lockOnBackground) }
    }

    package var authGraceSeconds: Int? {
        get { defaults.object(forKey: Key.authGraceSeconds) as? Int }
        set { set(newValue, forKey: Key.authGraceSeconds) }
    }

    private func set(_ value: Any?, forKey key: String) {
        if let value {
            defaults.set(value, forKey: key)
        } else {
            defaults.removeObject(forKey: key)
        }
    }
}
