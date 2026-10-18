#if ORLIX_MODULAR_APP
import OrlixModels
#endif
@MainActor
final package class TerminalAccessoryPendingMutationHandler {
    private let cloud: any TerminalAccessoryCloudClient
    private let resolutionPublisher: any TerminalAccessoryResolutionPublishing

    package init(
        cloud: any TerminalAccessoryCloudClient,
        resolutionPublisher: any TerminalAccessoryResolutionPublishing
    ) {
        self.cloud = cloud
        self.resolutionPublisher = resolutionPublisher
    }

    package func handle(_ profile: TerminalAccessoryProfile) async throws {
        let resolvedProfile = try await cloud.syncTerminalAccessoryProfile(profile)
        resolutionPublisher.publishTerminalAccessoryProfile(resolvedProfile)
    }
}
