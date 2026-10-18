#if os(iOS)
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
        NavigationView {
            content()
                .navigationTitle(Text("Storage"))
                .navigationBarTitleDisplayMode(.inline)
                .searchable(text: $searchText, prompt: Text("Search Volumes"))
                .toolbar {
                    ToolbarItem(placement: .topBarTrailing) {
                        controls()
                    }
                }
                .statsSheetCloseToolbar(placement: .leading)
        }
        .orlixLargePresentationDetent()
    }
}

package extension View {
    package func storageVolumeToggleLabelAlignment() -> some View { self }
}
#endif
