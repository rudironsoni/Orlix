#if os(iOS)
import WidgetKit
import SwiftUI

@available(iOS 16.1, *)
@main
struct OrlixLiveActivityBundle: WidgetBundle {
    var body: some Widget {
        OrlixLauncherWidget()
        OrlixLiveActivityWidget()
    }
}
#endif
