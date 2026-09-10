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
#endif
