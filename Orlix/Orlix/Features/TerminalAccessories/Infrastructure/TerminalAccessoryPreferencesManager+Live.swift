import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
extension CloudKitSyncCoordinator: TerminalAccessoryMutationQueue {
    package func enqueueTerminalAccessoryProfileUpsert(_ profile: TerminalAccessoryProfile) throws {
        try enqueue(.terminalAccessoryProfileUpsert(profile))
    }
}
extension CloudKitSyncLifecycleDriver: TerminalAccessorySyncLifecycle {}

package extension TerminalAccessoryPreferencesDependencies {
    static package func live(
        defaults: UserDefaults,
        cloud: any TerminalAccessoryCloudClient,
        mutationQueue: any TerminalAccessoryMutationQueue,
        syncLifecycle: any TerminalAccessorySyncLifecycle,
        resolutionSource: any TerminalAccessoryResolutionSource,
        writerID: String,
        isSyncEnabled: @escaping () -> Bool,
        now: @escaping () -> Date,
        makeID: @escaping () -> UUID,
        trackCustomActionCreated: @escaping (TerminalAccessoryCustomActionKind) -> Void
    ) -> Self {
        TerminalAccessoryPreferencesDependencies(
            profileStore: UserDefaultsTerminalAccessoryProfileStore(
                defaults: defaults,
                key: UserDefaultsTerminalAccessoryProfileStore.storageKey
            ),
            cloud: cloud,
            mutationQueue: mutationQueue,
            syncLifecycle: syncLifecycle,
            resolutionSource: resolutionSource,
            writerID: writerID,
            isSyncEnabled: isSyncEnabled,
            now: now,
            makeID: makeID,
            trackCustomActionCreated: trackCustomActionCreated,
            waitForSyncDebounce: {
                try await Task.sleep(nanoseconds: 650_000_000)
            },
            startsSynchronization: true
        )
    }
}
