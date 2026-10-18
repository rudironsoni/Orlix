#if os(iOS)
import SwiftUI
import UIKit

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package struct TerminalPaneSurface<Backend: AnyObject>: UIViewRepresentable {
    package let paneId: String
    package let size: CGSize
    package let isActive: Bool
    package let presentationOverrides: TerminalPresentationOverrides
    package let terminalAccessoryInputSnapshot: TerminalAccessoryInputSnapshot
    package let makeBackend: () -> Backend
    package let reusableTerminal: (Backend) -> GhosttyTerminalView?
    package let configure: (GhosttyTerminalView, Backend, Bool) -> Void
    package let update: (GhosttyTerminalView, Backend) -> Void
    package let dismantle: (GhosttyTerminalView, Backend) -> Void

    @EnvironmentObject private var ghosttyApp: GhosttyRuntime
    @Environment(\.scenePhase) private var scenePhase

    final package class Coordinator {
        package let backend: Backend
        package var lastReportedSize: CGSize = .zero
        package var dismantle: (GhosttyTerminalView, Backend) -> Void

        package init(backend: Backend, dismantle: @escaping (GhosttyTerminalView, Backend) -> Void) {
            self.backend = backend
            self.dismantle = dismantle
        }
    }

    package func makeCoordinator() -> Coordinator {
        Coordinator(backend: makeBackend(), dismantle: dismantle)
    }

    package func makeUIView(context: Context) -> UIView {
        guard let app = ghosttyApp.app else { return UIView(frame: .zero) }
        let existing = reusableTerminal(context.coordinator.backend)
        let initialSize = size.width > 0 && size.height > 0
            ? size : CGSize(width: 800, height: 600)
        let terminal = existing ?? GhosttyTerminalView(
            frame: CGRect(origin: .zero, size: initialSize),
            worktreePath: NSHomeDirectory(),
            ghosttyApp: app,
            appWrapper: ghosttyApp,
            paneId: paneId,
            terminalAccessoryInputSnapshot: terminalAccessoryInputSnapshot,
            useCustomIO: true
        )
        if terminal.superview != nil {
            terminal.removeFromSuperview()
        }
        configure(terminal, context.coordinator.backend, existing != nil)
        terminal.acceptsTerminalInput = isActive
        terminal.applyPresentationOverrides(presentationOverrides)
        terminal.applyTerminalAccessoryInputSnapshot(terminalAccessoryInputSnapshot)
        context.coordinator.lastReportedSize = initialSize
        if size.width > 0 && size.height > 0 {
            terminal.frame = CGRect(origin: .zero, size: size)
            terminal.sizeDidChange(size)
        }
        if !isActive {
            terminal.pauseRendering()
        }
        return terminal
    }

    package func updateUIView(_ view: UIView, context: Context) {
        guard let terminal = view as? GhosttyTerminalView else { return }
        context.coordinator.dismantle = dismantle
        let windowScene = terminal.window?.windowScene
        let sceneIsActive = TerminalSceneActivityPolicy.isActive(
            environmentIsActive: scenePhase == .active,
            windowSceneIsActive: windowScene.map {
                $0.activationState == .foregroundActive
            }
        )
        terminal.acceptsTerminalInput = isActive
        if terminal.surfacePresentationOverrides != presentationOverrides {
            terminal.applyPresentationOverrides(presentationOverrides)
        }
        terminal.applyTerminalAccessoryInputSnapshot(terminalAccessoryInputSnapshot)
        if size.width > 0 && size.height > 0 && size != context.coordinator.lastReportedSize {
            context.coordinator.lastReportedSize = size
            terminal.sizeDidChange(size)
        }
        if terminal.didSignalReady {
            switch TerminalRenderingPolicy.transition(
                terminalIsActive: isActive,
                sceneIsActive: sceneIsActive,
                renderingIsPaused: terminal.isRenderingPaused
            ) {
            case .resume:
                terminal.resumeRendering()
            case .pause:
                terminal.pauseRendering()
            case .none:
                break
            }
        }
        update(terminal, context.coordinator.backend)
    }

    static package func dismantleUIView(_ view: UIView, coordinator: Coordinator) {
        guard let terminal = view as? GhosttyTerminalView else { return }
        coordinator.dismantle(terminal, coordinator.backend)
    }
}

package struct TerminalFloatingControlButton: View {
    package let title: LocalizedStringKey
    package let systemImage: String
    package let accessibilityLabel: LocalizedStringKey
    package let accessibilityIdentifier: String
    package let showsTitle: Bool
    package var isPrimary: Bool = false
    package let action: () -> Void

    @Environment(\.colorScheme) private var colorScheme

    static package func keyboard(showsTitle: Bool, action: @escaping () -> Void) -> Self {
        Self(
            title: "Keyboard",
            systemImage: "keyboard",
            accessibilityLabel: "Show Keyboard",
            accessibilityIdentifier: "orlix.terminal.floating.keyboard",
            showsTitle: showsTitle,
            action: action
        )
    }

    package var body: some View {
        Button(action: action) {
            HStack(spacing: showsTitle ? 6 : 0) {
                Image(systemName: systemImage)
                if showsTitle {
                    Text(title)
                        .lineLimit(1)
                        .minimumScaleFactor(0.75)
                }
            }
            .font(.system(size: 15, weight: .semibold, design: .rounded))
            .foregroundStyle(isPrimary ? Color.accentColor : Color.primary)
            .padding(.horizontal, showsTitle ? 2 : 0)
        }
        .accessibilityLabel(Text(accessibilityLabel))
        .accessibilityIdentifier(accessibilityIdentifier)
        .modifier(
            FloatingTerminalControlButtonStyle(
                isPrimary: isPrimary,
                colorScheme: colorScheme
            )
        )
    }


    package init(

        title: LocalizedStringKey,

        systemImage: String,

        accessibilityLabel: LocalizedStringKey,

        accessibilityIdentifier: String,

        showsTitle: Bool,

        isPrimary: Bool = false,

        action: @escaping () -> Void

    ) {

        self.title = title

        self.systemImage = systemImage

        self.accessibilityLabel = accessibilityLabel

        self.accessibilityIdentifier = accessibilityIdentifier

        self.showsTitle = showsTitle

        self.isPrimary = isPrimary

        self.action = action

    }
}

private struct FloatingTerminalControlButtonStyle: ViewModifier {
    package let isPrimary: Bool
    package let colorScheme: ColorScheme

    @ViewBuilder
    package func body(content: Content) -> some View {
        if #available(iOS 26, *) {
            if isPrimary {
                content
                    .tint(Color.accentColor)
                    .buttonStyle(SwiftUI.GlassButtonStyle())
                    .buttonBorderShape(.capsule)
                    .controlSize(.large)
            } else {
                content
                    .buttonStyle(SwiftUI.GlassButtonStyle())
                    .buttonBorderShape(.capsule)
                    .controlSize(.large)
            }
        } else {
            content
                .buttonStyle(
                    .glass(
                        tint: Color.accentColor.opacity(
                            isPrimary ? 0.5 : (colorScheme == .dark ? 0.24 : 0.14)
                        )
                    )
                )
        }
    }
}
#endif
