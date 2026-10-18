#if os(macOS)
import SwiftUI

package struct StorageHealthDetailsPlatformShell<Content: View>: View {
    package let content: () -> Content

    package init(@ViewBuilder content: @escaping () -> Content) {
        self.content = content
    }

    package var body: some View {
        StatsDetailShell(
            String(localized: "Storage Health"),
            systemImage: "internaldrive",
            tint: .orange
        ) {
            content()
                .listStyle(.plain)
        }
    }
}
#endif
