#if os(iOS)
import SwiftUI
import WidgetKit

@available(iOS 16.1, *)
struct OrlixLauncherWidget: Widget {
    var body: some WidgetConfiguration {
        StaticConfiguration(kind: OrlixWidgetKind.launcher, provider: OrlixLauncherProvider()) { _ in
            OrlixLauncherWidgetView()
        }
        .configurationDisplayName(String(localized: "Open Orlix"))
        .description(String(localized: "Open Orlix from the Lock Screen."))
        .supportedFamilies([.accessoryCircular])
    }
}

@available(iOS 16.1, *)
private struct OrlixLauncherEntry: TimelineEntry {
    let date: Date
}

@available(iOS 16.1, *)
private struct OrlixLauncherProvider: TimelineProvider {
    func placeholder(in context: Context) -> OrlixLauncherEntry {
        OrlixLauncherEntry(date: Date())
    }

    func getSnapshot(
        in context: Context,
        completion: @escaping (OrlixLauncherEntry) -> Void
    ) {
        completion(OrlixLauncherEntry(date: Date()))
    }

    func getTimeline(
        in context: Context,
        completion: @escaping (Timeline<OrlixLauncherEntry>) -> Void
    ) {
        completion(Timeline(entries: [OrlixLauncherEntry(date: Date())], policy: .never))
    }
}

@available(iOS 16.1, *)
private struct OrlixLauncherWidgetView: View {
    @ViewBuilder
    var body: some View {
        if #available(iOS 17.0, *) {
            launcherGlyph
                .containerBackground(for: .widget) {
                    AccessoryWidgetBackground()
                }
        } else {
            ZStack {
                AccessoryWidgetBackground()
                launcherGlyph
            }
        }
    }

    private var launcherGlyph: some View {
        Text(">_")
            .font(.system(size: 18, weight: .bold, design: .monospaced))
            .widgetAccentable()
            .accessibilityLabel(Text("Open Orlix"))
    }
}
#endif
