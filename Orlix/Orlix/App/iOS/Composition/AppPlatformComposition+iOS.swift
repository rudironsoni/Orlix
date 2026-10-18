#if os(iOS)
import UIKit

#if ORLIX_MODULAR_APP
import OrlixModels
import OrlixTerminal
#endif
@MainActor
package struct AppPlatformComposition {
    package let applicationIsActive: @MainActor @Sendable () -> Bool
    package let lifecycleDependencies: AppLifecycleDependencies

    static package func live(
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
