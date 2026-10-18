import SwiftUI

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package struct NoticeAppHost<Content: View>: View {
    @ObservedObject private var networkMonitor: NetworkMonitor
    package let content: Content

    package init(networkMonitor: NetworkMonitor, @ViewBuilder content: () -> Content) {
        self.networkMonitor = networkMonitor
        self.content = content()
    }

    private var topBannerNotice: NoticeItem? {
        guard networkMonitor.isOffline else { return nil }

        return NoticeItem(
            id: "app-offline",
            lane: .topBanner,
            level: .warning,
            leading: .icon("wifi.slash"),
            title: String(localized: "Offline"),
            message: String(localized: "No network connection. Network-dependent features are paused.")
        )
    }

    package var body: some View {
        NoticeHost(topBanner: topBannerNotice, topInsetBehavior: .safeAreaTop) {
            content
        }
        .animation(.easeInOut(duration: 0.2), value: networkMonitor.readiness)
    }
}
