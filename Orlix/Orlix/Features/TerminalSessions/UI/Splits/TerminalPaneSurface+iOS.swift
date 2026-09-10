#if os(iOS)
import SwiftUI
import UIKit

struct TerminalPaneSurface<Backend: AnyObject>: UIViewRepresentable {
    let paneId: String
    let size: CGSize
    let isActive: Bool
    let presentationOverrides: TerminalPresentationOverrides
    let terminalAccessoryInputSnapshot: TerminalAccessoryInputSnapshot
    let makeBackend: () -> Backend
    let reusableTerminal: (Backend) -> GhosttyTerminalView?
    let configure: (GhosttyTerminalView, Backend, Bool) -> Void
    let update: (GhosttyTerminalView, Backend) -> Void
    let dismantle: (GhosttyTerminalView, Backend) -> Void

    @EnvironmentObject private var ghosttyApp: GhosttyRuntime
    @Environment(\.scenePhase) private var scenePhase

    final class Coordinator {
        let backend: Backend
        var lastReportedSize: CGSize = .zero
        var dismantle: (GhosttyTerminalView, Backend) -> Void

        init(backend: Backend, dismantle: @escaping (GhosttyTerminalView, Backend) -> Void) {
            self.backend = backend
            self.dismantle = dismantle
        }
    }

    func makeCoordinator() -> Coordinator {
        Coordinator(backend: makeBackend(), dismantle: dismantle)
    }

    func makeUIView(context: Context) -> UIView {
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

    func updateUIView(_ view: UIView, context: Context) {
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

    static func dismantleUIView(_ view: UIView, coordinator: Coordinator) {
        guard let terminal = view as? GhosttyTerminalView else { return }
        coordinator.dismantle(terminal, coordinator.backend)
    }
}

struct TerminalFloatingControlButton: View {
    let title: LocalizedStringKey
    let systemImage: String
    let accessibilityLabel: LocalizedStringKey
    let accessibilityIdentifier: String
    let showsTitle: Bool
    var isPrimary: Bool = false
    let action: () -> Void

    @Environment(\.colorScheme) private var colorScheme

    static func keyboard(showsTitle: Bool, action: @escaping () -> Void) -> Self {
        Self(
            title: "Keyboard",
            systemImage: "keyboard",
            accessibilityLabel: "Show Keyboard",
            accessibilityIdentifier: "orlix.terminal.floating.keyboard",
            showsTitle: showsTitle,
            action: action
        )
    }

    var body: some View {
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
}

private struct FloatingTerminalControlButtonStyle: ViewModifier {
    let isPrimary: Bool
    let colorScheme: ColorScheme

    @ViewBuilder
    func body(content: Content) -> some View {
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
