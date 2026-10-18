#if ORLIX_MODULAR_APP
import OrlixModels
#endif
@MainActor
final package class StatsPreferencesPendingMutationHandler {
    private let cloud: any StatsPreferencesCloudClient
    private let resolutionPublisher: any StatsPreferencesResolutionPublishing

    package init(
        cloud: any StatsPreferencesCloudClient,
        resolutionPublisher: any StatsPreferencesResolutionPublishing
    ) {
        self.cloud = cloud
        self.resolutionPublisher = resolutionPublisher
    }

    package func handle(_ preferences: StatsPreferences) async throws {
        let resolvedPreferences = try await cloud.syncStatsPreferences(preferences)
        resolutionPublisher.publishStatsPreferences(resolvedPreferences)
    }
}
