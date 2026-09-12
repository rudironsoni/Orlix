#if os(iOS)
import SwiftUI

struct StorageHealthDetailsPlatformShell<Content: View>: View {
    let content: () -> Content

    init(@ViewBuilder content: @escaping () -> Content) {
        self.content = content
    }

    var body: some View {
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
