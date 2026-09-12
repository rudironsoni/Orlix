import ActivityKit
import SwiftUI
import WidgetKit

@available(iOS 16.1, *)
struct LiveActivitySmokeAttributes: ActivityAttributes {
    public struct ContentState: Codable, Hashable {
        var label: String
    }
}

@available(iOS 16.1, *)
struct LiveActivitySmokeWidget: Widget {
    var body: some WidgetConfiguration {
        ActivityConfiguration(for: LiveActivitySmokeAttributes.self) { context in
            Text(context.state.label)
        } dynamicIsland: { context in
            DynamicIsland {
                DynamicIslandExpandedRegion(.leading) {
                    Text(context.state.label)
                }
            } compactLeading: {
                Text("O")
            } compactTrailing: {
                Text("L")
            } minimal: {
                Text("O")
            }
        }
    }
}

@available(iOS 16.1, *)
@main
struct LiveActivitySmokeBundle: WidgetBundle {
    var body: some Widget {
        LiveActivitySmokeWidget()
    }
}
