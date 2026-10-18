#if os(macOS)
import SwiftUI

package struct StorageDetailsPlatformShell<Controls: View, Content: View>: View {
    @Binding package var searchText: String
    package let controls: () -> Controls
    package let content: () -> Content

    package init(
        searchText: Binding<String>,
        @ViewBuilder controls: @escaping () -> Controls,
        @ViewBuilder content: @escaping () -> Content
    ) {
        _searchText = searchText
        self.controls = controls
        self.content = content
    }

    package var body: some View {
        StatsDetailShell(
            String(localized: "Storage"),
            systemImage: "internaldrive",
            tint: .orange
        ) {
            controls()
        } content: {
            VStack(spacing: 0) {
                StatsSearchField(prompt: String(localized: "Search Volumes"), text: $searchText)
                    .padding(.horizontal, 16)
                    .padding(.vertical, 10)
                Divider()
                content()
            }
        }
    }
}

package extension View {
    package func storageVolumeToggleLabelAlignment() -> some View {
        alignmentGuide(.firstTextBaseline) { dimensions in
            dimensions[VerticalAlignment.center]
        }
    }
}
#endif
