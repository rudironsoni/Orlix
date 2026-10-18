#if os(iOS)
import SwiftUI

private struct StatsSheetCloseToolbarModifier: ViewModifier {
    @Environment(\.dismiss) private var dismiss
    package let placement: StatsSheetClosePlacement

    private var closePlacement: ToolbarItemPlacement {
        switch placement {
        case .automatic, .trailing:
            return .topBarTrailing
        case .leading:
            return .topBarLeading
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
        }
    }
}
#endif
