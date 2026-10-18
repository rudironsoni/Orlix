import SwiftUI

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package struct ZenModeFloatingOverlay<Panel: View>: View {
    @Binding package var isPanelPresented: Bool
    package let panel: (CGFloat) -> Panel

    #if os(macOS)
    private let chromeTopPadding: CGFloat = 6
    private let chromeTrailingPadding: CGFloat = 8
    private let panelMaximumWidth: CGFloat = 360
    #else
    private let chromeTopPadding: CGFloat = 12
    private let chromeTrailingPadding: CGFloat = 12
    private let panelMaximumWidth: CGFloat = 400
    #endif

    #if os(iOS)
    private let launcherSize: CGFloat = 44
    #else
    private let launcherSize: CGFloat = 40
    #endif

    package init(
        isPanelPresented: Binding<Bool>,
        indicatorColor: Color? = nil,
        @ViewBuilder panel: @escaping (CGFloat) -> Panel
    ) {
        self._isPanelPresented = isPanelPresented
        self.panel = panel
    }

    package var body: some View {
        GeometryReader { proxy in
            let panelWidth = max(250, min(proxy.size.width - 24, panelMaximumWidth))

            ZStack(alignment: .topTrailing) {
                if isPanelPresented {
                    Color.black.opacity(0.001)
                        .ignoresSafeArea()
                        .contentShape(Rectangle())
                        .onTapGesture {
                            closePanel()
                        }
                        .transition(.opacity)
                }

                chromeStack(panelWidth: panelWidth)
                    .padding(.top, chromeTopPadding)
                    .padding(.trailing, chromeTrailingPadding)
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topTrailing)
        }
    }

    @ViewBuilder
    private func chromeStack(panelWidth: CGFloat) -> some View {
        if #available(iOS 26, macOS 26, *) {
            GlassEffectContainer(spacing: 12) {
                overlayContent(panelWidth: panelWidth)
            }
        } else {
            overlayContent(panelWidth: panelWidth)
        }
    }

    private func overlayContent(panelWidth: CGFloat) -> some View {
        VStack(alignment: .trailing, spacing: 10) {
            launcherButton

            if isPanelPresented {
                panel(panelWidth)
                    .transition(.move(edge: .top).combined(with: .opacity))
            }
        }
    }

    private var launcherButton: some View {
        Button {
            withAnimation(.spring(response: 0.26, dampingFraction: 0.84)) {
                isPanelPresented.toggle()
            }
        } label: {
            Image(systemName: "slider.horizontal.3")
                .font(.system(size: 15, weight: .semibold))
                .frame(width: 24, height: 24)
                .foregroundStyle(.primary)
        }
        .zenModeLauncherButtonStyle(hitTargetSize: launcherSize)
        .accessibilityLabel(String(localized: "Zen controls"))
        .accessibilityValue(isPanelPresented ? String(localized: "Expanded") : String(localized: "Collapsed"))
        .accessibilityIdentifier("orlix.zen.controls")
    }

    private func closePanel() {
        withAnimation(.easeInOut(duration: 0.18)) {
            isPanelPresented = false
        }
    }
}

private extension View {
    @ViewBuilder
    package func zenModeLauncherButtonStyle(hitTargetSize: CGFloat) -> some View {
        if #available(iOS 26, macOS 26, *) {
            self
                .buttonStyle(SwiftUI.GlassButtonStyle())
                .buttonBorderShape(.circle)
                .frame(width: hitTargetSize, height: hitTargetSize)
                .contentShape(Rectangle())
        } else {
            self
                .buttonStyle(.plain)
                .frame(width: hitTargetSize, height: hitTargetSize)
                .background(.ultraThinMaterial, in: Circle())
                .overlay(
                    Circle()
                        .stroke(Color.primary.opacity(0.12), lineWidth: 1)
                )
        }
    }
}

package struct ZenModePanelCard<Content: View>: View {
    package let width: CGFloat
    package let backgroundColor: Color?
    package let content: Content
    private let cornerRadius: CGFloat = 22

    package init(width: CGFloat, backgroundColor: Color? = nil, @ViewBuilder content: () -> Content) {
        self.width = width
        self.backgroundColor = backgroundColor
        self.content = content()
    }

    package var body: some View {
        let cardShape = RoundedRectangle(cornerRadius: cornerRadius, style: .continuous)

        ScrollView(showsIndicators: false) {
            VStack(alignment: .leading, spacing: 16) {
                content
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(16)
        }
        .frame(width: width)
        .frame(maxHeight: 430)
        .background(panelBackground(for: cardShape))
        .clipShape(cardShape)
        .overlay(
            cardShape
                .stroke(Color.primary.opacity(0.1), lineWidth: 1)
        )
        .shadow(color: Color.black.opacity(0.14), radius: 14, y: 10)
    }

    @ViewBuilder
    private func panelBackground(for shape: RoundedRectangle) -> some View {
        if let backgroundColor {
            shape
                .fill(backgroundColor)
                .overlay(
                    shape.fill(Color.white.opacity(0.02))
                )
        } else {
            shape
                .fill(.ultraThinMaterial)
        }
    }
}

package struct ZenModeSection<Content: View>: View {
    package let title: LocalizedStringKey
    package let content: Content

    package init(_ title: LocalizedStringKey, @ViewBuilder content: () -> Content) {
        self.title = title
        self.content = content()
    }

    package var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text(title)
                .font(.caption.weight(.bold))
                .foregroundStyle(sectionHeaderColor)
                .textCase(.uppercase)

            content
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }

    private var sectionHeaderColor: Color {
        #if os(iOS)
        Color.primary.opacity(0.78)
        #else
        Color.secondary
        #endif
    }
}

package struct ZenModeChoiceChip: View {
    package let title: LocalizedStringKey
    package let systemImage: String
    package let isSelected: Bool
    package let action: () -> Void

    package var body: some View {
        Button(action: action) {
            Label(title, systemImage: systemImage)
                .font(.callout.weight(.medium))
                .frame(maxWidth: .infinity)
                .padding(.vertical, 10)
                .foregroundStyle(foregroundColor)
                .background(
                    Capsule(style: .continuous)
                        .fill(Color.primary.opacity(backgroundOpacity))
                )
        }
        .buttonStyle(.plain)
    }

    private var foregroundColor: Color {
        if isSelected {
            return .primary
        }

        #if os(iOS)
        return Color.primary.opacity(0.8)
        #else
        return Color.primary.opacity(0.72)
        #endif
    }

    private var backgroundOpacity: Double {
        if isSelected {
            return 0.16
        }

        #if os(iOS)
        return 0.1
        #else
        return 0.08
        #endif
    }
}

package struct ZenModeActionButton: View {
    package let title: LocalizedStringKey
    package let systemImage: String
    package var tint: Color = .primary
    package var isDisabled = false
    package let action: () -> Void

    package var body: some View {
        Button(action: action) {
            HStack(spacing: 10) {
                Image(systemName: systemImage)
                    .frame(width: 16)
                Text(title)
                Spacer()
            }
            .padding(.horizontal, 12)
            .padding(.vertical, 10)
            .foregroundStyle(isDisabled ? Color.secondary : tint)
            .background(
                RoundedRectangle(cornerRadius: 12, style: .continuous)
                    .fill(Color.primary.opacity(0.07))
            )
        }
        .buttonStyle(.plain)
        .disabled(isDisabled)
    }
}

package struct ZenModeStatusLine: View {
    package let title: String
    package let subtitle: String
    package let indicatorColor: Color

    package var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack(spacing: 8) {
                Circle()
                    .fill(indicatorColor)
                    .frame(width: 8, height: 8)
                Text(title)
                    .font(.headline)
            }

            Text(subtitle)
                .font(.caption)
                .foregroundStyle(subtitleColor)
        }
    }

    private var subtitleColor: Color {
        #if os(iOS)
        Color.primary.opacity(0.72)
        #else
        .secondary
        #endif
    }
}


package extension ConnectionState {
    package var statusTintColor: Color {
        switch self {
        case .connected:
            return .green
        case .connecting, .reconnecting:
            return .orange
        case .disconnected, .idle:
            return .secondary
        case .failed:
            return .red
        }
    }
}
