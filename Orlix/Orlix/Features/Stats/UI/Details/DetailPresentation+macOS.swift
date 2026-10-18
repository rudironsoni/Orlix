#if os(macOS)
import AppKit
import SwiftUI

package struct StatsDetailShell<Controls: View, Content: View>: View {
    @Environment(\.dismiss) private var dismiss
    package let title: String
    package let systemImage: String
    package let tint: Color
    package let showsControls: Bool
    package let controls: () -> Controls
    package let content: () -> Content

    package init(
        _ title: String,
        systemImage: String,
        tint: Color,
        @ViewBuilder controls: @escaping () -> Controls,
        @ViewBuilder content: @escaping () -> Content
    ) {
        self.title = title
        self.systemImage = systemImage
        self.tint = tint
        self.showsControls = true
        self.controls = controls
        self.content = content
    }

    package init(
        _ title: String,
        systemImage: String,
        tint: Color,
        @ViewBuilder content: @escaping () -> Content
    ) where Controls == EmptyView {
        self.title = title
        self.systemImage = systemImage
        self.tint = tint
        self.showsControls = false
        self.controls = { EmptyView() }
        self.content = content
    }

    package var body: some View {
        VStack(spacing: 0) {
            HStack(spacing: 12) {
                StatsSheetTitle(title: title, systemImage: systemImage, tint: tint)

                Spacer(minLength: 12)

                if showsControls {
                    controls()
                }

                Button(String(localized: "Close")) {
                    close()
                }
                .keyboardShortcut(.cancelAction)
            }
            .padding(.horizontal, 14)
            .padding(.vertical, 12)
            .background(.bar)

            Divider()

            content()
        }
        .background(Color(nsColor: .windowBackgroundColor))
    }

    private func close() {
        dismiss()
    }
}

private struct StatsSheetTitle: View {
    package let title: String
    package let systemImage: String
    package let tint: Color

    package var body: some View {
        Label {
            Text(title)
                .font(.headline.weight(.semibold))
                .lineLimit(1)
        } icon: {
            Image(systemName: systemImage)
                .font(.system(size: 15, weight: .semibold))
                .symbolRenderingMode(.hierarchical)
                .foregroundStyle(tint)
        }
        .labelStyle(.titleAndIcon)
    }
}

package struct StatsSearchField: View {
    package let prompt: String
    @Binding package var text: String

    package var body: some View {
        HStack(spacing: 8) {
            Image(systemName: "magnifyingglass")
                .foregroundStyle(.secondary)

            TextField(prompt, text: $text)
                .textFieldStyle(.plain)
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 7)
        .background(Color.primary.opacity(0.07), in: RoundedRectangle(cornerRadius: 9, style: .continuous))
    }
}

private struct StatsSheetCloseToolbarModifier: ViewModifier {
    @Environment(\.dismiss) private var dismiss
    package let placement: StatsSheetClosePlacement

    private var closePlacement: ToolbarItemPlacement {
        switch placement {
        case .automatic, .trailing:
            return .automatic
        case .leading:
            return .cancellationAction
        }
    }

    package func body(content: Content) -> some View {
        content
            .toolbar {
                ToolbarItem(placement: closePlacement) {
                    Button {
                        dismiss()
                    } label: {
                        Image(systemName: "xmark")
                            .font(.system(size: 16, weight: .semibold))
                            .symbolRenderingMode(.hierarchical)
                            .foregroundStyle(.secondary)
                    }
                    .accessibilityLabel(Text("Close"))
                }
            }
    }
}

package extension View {
    package func statsSheetCloseToolbar(placement: StatsSheetClosePlacement = .automatic) -> some View {
        modifier(StatsSheetCloseToolbarModifier(placement: placement))
    }

    @ViewBuilder
    package func statsDetailPresentation<Content: View>(
        isPresented: Binding<Bool>,
        size: CGSize = StatsPresentationSize.standard,
        @ViewBuilder content: @escaping () -> Content
    ) -> some View {
        sheet(isPresented: isPresented) {
            content()
                .frame(minWidth: size.width, minHeight: size.height)
        }
    }

    @ViewBuilder
    package func statsDetailPresentation<Item: Identifiable, Content: View>(
        item: Binding<Item?>,
        size: CGSize = StatsPresentationSize.standard,
        @ViewBuilder content: @escaping (Item) -> Content
    ) -> some View {
        sheet(item: item) { value in
            content(value)
                .frame(minWidth: size.width, minHeight: size.height)
        }
    }
}
#endif
