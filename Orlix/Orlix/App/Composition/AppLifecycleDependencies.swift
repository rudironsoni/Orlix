#if ORLIX_TEST_HOST
import OrlixTestImplementation
#else
import OrlixImplementation
#endif
#if ORLIX_TEST_HOST
import OrlixTestTerminal
#else
import OrlixTerminal
#endif
@MainActor
struct AppLifecycleDependencies {
    let subscribeToRemoteChanges: @MainActor () async -> Void
    let refreshNetwork: @MainActor () -> Void

    #if os(iOS)
    let endLiveActivitiesForApplicationTermination: @MainActor () -> Bool
    #endif
}
