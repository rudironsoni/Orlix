#if os(iOS)
import SwiftUI

package struct StorageHealthDetailsPlatformShell<Content: View>: View {
    package let content: () -> Content

    package init(@ViewBuilder content: @escaping () -> Content) {
        self.content = content
    }

    package var body: some View {
        NavigationView {
            content()
                .navigationTitle(Text("Storage Health"))
                .navigationBarTitleDisplayMode(.inline)
                .statsSheetCloseToolbar()
        }
        .orlixLargePresentationDetent()
    }
}
#endif
