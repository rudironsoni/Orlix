#if os(macOS)
import SwiftUI

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package struct CustomThemeManagerRow: View {
    package let theme: TerminalTheme
    package let assignment: String?
    package let usePerAppearanceTheme: Bool
    package let isHovered: Bool
    package let isSelected: Bool
    package let onApply: (TerminalThemeSelectionTarget) -> Void
    package let onEdit: () -> Void
    package let onDeleteRequest: () -> Void

    @Environment(\.controlActiveState) private var controlActiveState

    private var selectionFillColor: Color {
        let base = NSColor.unemphasizedSelectedContentBackgroundColor
        let alpha: Double = controlActiveState == .key ? 0.26 : 0.18
        return Color(nsColor: base).opacity(alpha)
    }

    private var selectedTextColor: Color {
        Color(nsColor: .selectedTextColor)
    }

    package var body: some View {
        HStack(spacing: 10) {
            Text(theme.name)
                .font(.body)
                .fontWeight(.semibold)
                .foregroundStyle(isSelected ? selectedTextColor : .primary)
                .lineLimit(1)

            Spacer(minLength: 8)

            if let assignment {
                PillBadge(text: assignment, color: .secondary)
            }

            if case .needsRepair = theme.validationState {
                PillBadge(text: String(localized: "Needs Repair"), color: .orange)
            }

            if isHovered || isSelected {
                if theme.canApply {
                    Menu {
                        applyMenuItems
                    } label: {
                        Image(systemName: "paintbrush.pointed.fill")
                            .foregroundStyle(isSelected ? selectedTextColor.opacity(0.9) : .secondary)
                            .imageScale(.medium)
                    }
                    .menuStyle(.borderlessButton)
                }

                Button {
                    onEdit()
                } label: {
                    Image(systemName: "pencil.circle.fill")
                        .foregroundStyle(isSelected ? selectedTextColor.opacity(0.9) : .secondary)
                        .imageScale(.medium)
                }
                .buttonStyle(.plain)
            }
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 8)
        .background(isSelected ? selectionFillColor : Color.clear, in: RoundedRectangle(cornerRadius: 6))
        .contentShape(Rectangle())
        .onTapGesture {
            guard theme.canApply else {
                onEdit()
                return
            }
            if usePerAppearanceTheme {
                onApply(.both)
            } else {
                onApply(.dark)
            }
        }
        .contextMenu {
            if theme.canApply {
                applyMenuItems
                Divider()
            }
            Button("Edit") {
                onEdit()
            }
            Button("Delete", role: .destructive) {
                onDeleteRequest()
            }
        }
    }

    @ViewBuilder
    private var applyMenuItems: some View {
        if usePerAppearanceTheme {
            Button("Apply to Dark") {
                onApply(.dark)
            }
            Button("Apply to Light") {
                onApply(.light)
            }
            Button("Apply to Both") {
                onApply(.both)
            }
        } else {
            Button("Use Theme") {
                onApply(.dark)
            }
        }
    }
}
#endif
