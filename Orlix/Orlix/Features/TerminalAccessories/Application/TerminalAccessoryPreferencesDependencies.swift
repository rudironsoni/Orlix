import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package enum TerminalAccessoryCloudClientError: Error, Equatable, Sendable {
    case conflictRetryLimitReached
}

@MainActor
package protocol TerminalAccessoryCloudClient: AnyObject {
    func syncTerminalAccessoryProfile(
        _ localProfile: TerminalAccessoryProfile
    ) async throws -> TerminalAccessoryProfile
}

@MainActor
package protocol TerminalAccessoryMutationQueue: AnyObject {
    func enqueueTerminalAccessoryProfileUpsert(_ profile: TerminalAccessoryProfile) throws
    func drainPendingMutations() async
}

@MainActor
package protocol TerminalAccessorySyncLifecycle: AnyObject {
    func observe(
        _ observer: @escaping (CloudKitSyncLifecycleEvent) -> Void
    ) -> UUID
    func removeObserver(_ id: UUID)
}

@MainActor
package protocol TerminalAccessoryResolutionSource: AnyObject {
    func observeTerminalAccessoryProfile(
        _ observer: @escaping (TerminalAccessoryProfile) -> Void
    ) -> UUID
    func removeTerminalAccessoryProfileObserver(_ id: UUID)
}

@MainActor
package protocol TerminalAccessoryResolutionPublishing: AnyObject {
    func publishTerminalAccessoryProfile(_ profile: TerminalAccessoryProfile)
}

@MainActor
package protocol TerminalAccessoryProfilePersisting: AnyObject {
    func loadProfile(defaultWriterID: String) -> TerminalAccessoryProfile
    func saveProfile(_ profile: TerminalAccessoryProfile)
}

@MainActor
package struct TerminalAccessoryPreferencesDependencies {
    package let profileStore: any TerminalAccessoryProfilePersisting
    package let cloud: any TerminalAccessoryCloudClient
    package let mutationQueue: any TerminalAccessoryMutationQueue
    package let syncLifecycle: any TerminalAccessorySyncLifecycle
    package let resolutionSource: any TerminalAccessoryResolutionSource
    package let writerID: String
    package let isSyncEnabled: () -> Bool
    package let now: () -> Date
    package let makeID: () -> UUID
    package let trackCustomActionCreated: (TerminalAccessoryCustomActionKind) -> Void
    package let waitForSyncDebounce: () async throws -> Void
    package let startsSynchronization: Bool
}
