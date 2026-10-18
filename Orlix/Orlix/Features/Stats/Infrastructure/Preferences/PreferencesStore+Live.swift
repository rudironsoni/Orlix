import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package extension CloudKitSyncCoordinator: StatsPreferencesMutationQueue {
    package func enqueueStatsPreferencesUpsert(_ preferences: StatsPreferences) throws {
        try enqueue(.statsPreferencesUpsert(preferences))
    }
}
package extension CloudKitSyncLifecycleDriver: StatsPreferencesSyncLifecycle {}

extension PreferencesStoreDependencies {
    static func live(
        defaults: UserDefaults,
        cloud: any StatsPreferencesCloudClient,
        mutationQueue: any StatsPreferencesMutationQueue,
        syncLifecycle: any StatsPreferencesSyncLifecycle,
        resolutionSource: any StatsPreferencesResolutionSource,
        writerID: String,
        isSyncEnabled: @escaping () -> Bool,
        now: @escaping () -> Date
    ) -> Self {
        PreferencesStoreDependencies(
            persistence: UserDefaultsStatsPreferencesStore(
                defaults: defaults,
                key: UserDefaultsStatsPreferencesStore.storageKey
            ),
            cloud: cloud,
            mutationQueue: mutationQueue,
            syncLifecycle: syncLifecycle,
            resolutionSource: resolutionSource,
            writerID: writerID,
            isSyncEnabled: isSyncEnabled,
            now: now,
            waitForSyncDebounce: {
                try await Task.sleep(nanoseconds: 650_000_000)
            },
            startsSynchronization: true
        )
    }
}
