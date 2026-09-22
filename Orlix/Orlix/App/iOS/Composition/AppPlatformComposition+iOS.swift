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
#if os(iOS)
import UIKit

@MainActor
struct AppPlatformComposition {
    let applicationIsActive: @MainActor @Sendable () -> Bool
    let lifecycleDependencies: AppLifecycleDependencies

    static func live(
        cloudKitManager: CloudKitManager,
        networkMonitor: NetworkMonitor,
        liveActivityManager: LiveActivityManager
    ) -> Self {
        Self(
            applicationIsActive: {
                UIApplication.shared.applicationState == .active
            },
            lifecycleDependencies: AppLifecycleDependencies(
                subscribeToRemoteChanges: {
                    await cloudKitManager.subscribeToChanges()
                },
                refreshNetwork: {
                    networkMonitor.refreshCurrentPath()
                },
                endLiveActivitiesForApplicationTermination: {
                    liveActivityManager.endForApplicationTermination()
                }
            )
        )
    }
}
#endif
