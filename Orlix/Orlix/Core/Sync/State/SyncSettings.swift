import Foundation

package enum SyncSettings {
    nonisolated static package let enabledKey = CloudKitSyncConstants.syncEnabledKey

    nonisolated static package var isEnabled: Bool {
        isEnabled(in: .standard)
    }

    nonisolated static package func isEnabled(in defaults: UserDefaults) -> Bool {
        defaults.object(forKey: enabledKey) as? Bool ?? true
    }

    nonisolated static package func persistEnabled(
        _ enabled: Bool,
        in defaults: UserDefaults = .standard
    ) throws {
        defaults.set(enabled, forKey: enabledKey)
        guard defaults.object(forKey: enabledKey) as? Bool == enabled else {
            throw SyncSettingsPersistenceError.persistenceFailed
        }
    }
}

nonisolated package enum SyncSettingsPersistenceError: Error, Sendable {
    case persistenceFailed
}
