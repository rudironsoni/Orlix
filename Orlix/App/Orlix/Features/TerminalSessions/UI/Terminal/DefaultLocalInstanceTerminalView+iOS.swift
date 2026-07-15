import OrlixOS
import SwiftUI
import UIKit

#if os(iOS)
struct DefaultLocalInstanceTerminalView: View {
    @EnvironmentObject private var ghosttyApp: Ghostty.App

    var body: some View {
        GeometryReader { geometry in
            DefaultLocalInstanceTerminalRepresentable(size: geometry.size)
        }
        .background(Color.black)
        .navigationTitle("Orlix")
        .navigationBarTitleDisplayMode(.inline)
        .task {
            ghosttyApp.startIfNeeded()
        }
    }
}

private struct DefaultLocalInstanceTerminalRepresentable: UIViewRepresentable {
    @EnvironmentObject private var ghosttyApp: Ghostty.App

    let size: CGSize

    func makeCoordinator() -> Coordinator {
        Coordinator()
    }

    func makeUIView(context: Context) -> LocalTerminalContainerView {
        LocalTerminalContainerView()
    }

    func updateUIView(_ uiView: LocalTerminalContainerView, context: Context) {
        uiView.installTerminalIfNeeded(
            app: ghosttyApp.app,
            appWrapper: ghosttyApp,
            coordinator: context.coordinator,
            size: size
        )
        uiView.resizeTerminal(to: size)
    }

    static func dismantleUIView(_ uiView: LocalTerminalContainerView, coordinator: Coordinator) {
        coordinator.stop()
    }

    final class LocalTerminalContainerView: UIView {
        private(set) weak var terminal: GhosttyTerminalView?

        override init(frame: CGRect) {
            super.init(frame: frame)
            backgroundColor = .black
            accessibilityIdentifier = "orlix.local-instance.terminal"
            accessibilityLabel = "Orlix local terminal"
            accessibilityValue = "initializing"
            isAccessibilityElement = true
        }

        @available(*, unavailable)
        required init?(coder: NSCoder) {
            fatalError("init(coder:) has not been implemented")
        }

        func installTerminalIfNeeded(
            app: ghostty_app_t?,
            appWrapper: Ghostty.App,
            coordinator: Coordinator,
            size: CGSize
        ) {
            guard terminal == nil, let app else { return }

            let initialSize = size.width > 0 && size.height > 0
                ? size
                : CGSize(width: 800, height: 600)
            let terminal = GhosttyTerminalView(
                frame: CGRect(origin: .zero, size: initialSize),
                worktreePath: NSHomeDirectory(),
                ghosttyApp: app,
                appWrapper: appWrapper,
                paneId: "orlix.local.default",
                useCustomIO: true
            )
            terminal.autoresizingMask = [.flexibleWidth, .flexibleHeight]
            coordinator.attach(to: terminal, container: self)
            terminal.onReady = { [weak coordinator, weak terminal] in
                guard let terminal else { return }
                coordinator?.start(on: terminal)
            }
            addSubview(terminal)
            self.terminal = terminal
            terminal.sizeDidChange(initialSize)
        }

        func resizeTerminal(to size: CGSize) {
            guard size.width > 0, size.height > 0, let terminal else { return }
            terminal.frame = CGRect(origin: .zero, size: size)
            terminal.sizeDidChange(size)
        }
    }

    final class Coordinator: @unchecked Sendable {
        private weak var terminal: GhosttyTerminalView?
        private weak var container: LocalTerminalContainerView?
        private var session: OrlixLinuxSession?
        private var output: OrlixTerminalOutput?
        private var started = false

        @MainActor
        func attach(to terminal: GhosttyTerminalView, container: LocalTerminalContainerView) {
            self.terminal = terminal
            self.container = container
            terminal.writeCallback = { [weak self] data in
                self?.session?.terminal.send(data)
            }
            terminal.setupWriteCallback()
            terminal.onResize = { [weak self] columns, rows in
                guard columns > 0, rows > 0 else { return }
                self?.session?.terminal.resize(
                    rows: UInt32(rows),
                    columns: UInt32(columns)
                )
            }
        }

        @MainActor
        func start(on terminal: GhosttyTerminalView) {
            guard !started else { return }
            started = true

            guard let profile = OrlixOSDistribution.bundledBootProfile,
                  let rootImageIdentifier = OrlixOSDistribution.productRootImageIdentifier
            else {
                showError("OrlixOS payload metadata is missing.")
                return
            }

            let session = OrlixLinuxSession(
                bootConfig: OrlixBootConfig(
                    profile: profile,
                    rootImageIdentifier: rootImageIdentifier,
                    terminalIdentifier: "orlix.local.default"
                )
            )
            self.session = session

            output = session.terminal.attachOutput { [weak self] data in
                DispatchQueue.main.async {
                    self?.terminal?.feedData(data)
                    self?.container?.accessibilityValue = "output"
                }
            }

            let terminalSize = terminal.terminalSize()
            session.terminal.resize(
                rows: UInt32(terminalSize?.rows ?? 24),
                columns: UInt32(terminalSize?.columns ?? 80)
            )

            DispatchQueue.global(qos: .userInitiated).async { [weak self, session] in
                let status = session.boot()
                guard status != .ok else { return }
                DispatchQueue.main.async {
                    self?.showError("Orlix failed to start: \(status.message)")
                }
            }
        }

        @MainActor
        func stop() {
            session?.terminal.send(Data([0x04]))
            output?.cancel()
            output = nil
            session = nil
            terminal = nil
            container = nil
        }

        @MainActor
        private func showError(_ message: String) {
            container?.accessibilityValue = "failed"
            terminal?.feedData(Data("\r\n\u{001B}[31m\(message)\u{001B}[0m\r\n".utf8))
        }
    }
}
#endif
