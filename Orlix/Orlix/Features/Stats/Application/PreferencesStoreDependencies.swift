import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package enum StatsPreferencesCloudClientError: Error, Equatable, Sendable {
    case conflictRetryLimitReached
}

@MainActor
package protocol StatsPreferencesCloudClient: AnyObject {
    func syncStatsPreferences(
        _ localPreferences: StatsPreferences
    ) async throws -> StatsPreferences
}

@MainActor
package protocol StatsPreferencesMutationQueue: AnyObject {
    func enqueueStatsPreferencesUpsert(_ preferences: StatsPreferences) throws
    func drainPendingMutations() async
}

@MainActor
package protocol StatsPreferencesSyncLifecycle: AnyObject {
    func observe(
        _ observer: @escaping (CloudKitSyncLifecycleEvent) -> Void
    ) -> UUID
    func removeObserver(_ id: UUID)
}

@MainActor
package protocol StatsPreferencesResolutionSource: AnyObject {
    func observeStatsPreferences(
        _ observer: @escaping (StatsPreferences) -> Void
    ) -> UUID
    func removeStatsPreferencesObserver(_ id: UUID)
}

@MainActor
package protocol StatsPreferencesResolutionPublishing: AnyObject {
    func publishStatsPreferences(_ preferences: StatsPreferences)
}

@MainActor
package protocol StatsPreferencesPersisting: AnyObject {
    func loadPreferences(defaultWriterID: String) -> StatsPreferences
    func savePreferences(_ preferences: StatsPreferences)
}

@MainActor
package struct PreferencesStoreDependencies {
    package let persistence: any StatsPreferencesPersisting
    package let cloud: any StatsPreferencesCloudClient
    package let mutationQueue: any StatsPreferencesMutationQueue
    package let syncLifecycle: any StatsPreferencesSyncLifecycle
    package let resolutionSource: any StatsPreferencesResolutionSource
    package let writerID: String
    package let isSyncEnabled: () -> Bool
    package let now: () -> Date
    package let waitForSyncDebounce: () async throws -> Void
    package let startsSynchronization: Bool
}
