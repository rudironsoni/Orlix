#if os(macOS)
import AppKit

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
        liveActivityManager _: LiveActivityManager
    ) -> Self {
        Self(
            applicationIsActive: {
                NSApplication.shared.isActive
            },
            lifecycleDependencies: AppLifecycleDependencies(
                subscribeToRemoteChanges: {
                    await cloudKitManager.subscribeToChanges()
                },
                refreshNetwork: {
                    networkMonitor.refreshCurrentPath()
                }
            )
        )
    }
}
#endif
