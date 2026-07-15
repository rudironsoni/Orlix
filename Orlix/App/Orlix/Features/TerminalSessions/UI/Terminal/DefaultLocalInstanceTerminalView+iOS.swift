import OrlixOS
import SwiftUI
import UIKit

#if os(iOS)
struct DefaultLocalInstanceTerminalView: View {
    @EnvironmentObject private var ghosttyApp: Ghostty.App
    @Environment(\.colorScheme) private var colorScheme
    @AppStorage(CloudKitSyncConstants.terminalThemeNameKey) private var terminalThemeName = "Orlix Dark"
    @AppStorage(CloudKitSyncConstants.terminalThemeNameLightKey) private var terminalThemeNameLight = "Orlix Light"
    @AppStorage(CloudKitSyncConstants.terminalUsePerAppearanceThemeKey) private var usePerAppearanceTheme = true

    private var effectiveThemeName: String {
        guard usePerAppearanceTheme else { return terminalThemeName }
        return colorScheme == .dark ? terminalThemeName : terminalThemeNameLight
    }

    var body: some View {
        GeometryReader { geometry in
            DefaultLocalInstanceTerminalRepresentable(size: geometry.size)
        }
        .background(ThemeColorParser.backgroundColor(for: effectiveThemeName)!)
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
            coordinator: context.coordinator
        )
        uiView.updateAvailableSize(size)
    }

    static func dismantleUIView(_ uiView: LocalTerminalContainerView, coordinator: Coordinator) {
        coordinator.stop()
    }

    final class LocalTerminalContainerView: UIView {
        private(set) weak var terminal: GhosttyTerminalView?
        private var lastReportedSize: CGSize?

        override init(frame: CGRect) {
            super.init(frame: frame)
            backgroundColor = .clear
        }

        @available(*, unavailable)
        required init?(coder: NSCoder) {
            fatalError("init(coder:) has not been implemented")
        }

        override func layoutSubviews() {
            super.layoutSubviews()
            synchronizeTerminalGeometry()
        }

        func installTerminalIfNeeded(
            app: ghostty_app_t?,
            appWrapper: Ghostty.App,
            coordinator: Coordinator
        ) {
            guard terminal == nil, let app else { return }

            let initialSize = bounds.width > 0 && bounds.height > 0
                ? bounds.size
                : CGSize(width: 800, height: 600)
            let terminal = GhosttyTerminalView(
                frame: CGRect(origin: .zero, size: initialSize),
                worktreePath: NSHomeDirectory(),
                ghosttyApp: app,
                appWrapper: appWrapper,
                paneId: "orlix.local.default",
                useCustomIO: true
            )
            terminal.accessibilityIdentifier = "orlix.local-instance.terminal"
            terminal.accessibilityLabel = "Orlix local terminal"
            terminal.accessibilityValue = "initializing"
            terminal.isAccessibilityElement = true
            terminal.acceptsTerminalInput = true
            terminal.autoresizingMask = [.flexibleWidth, .flexibleHeight]
            coordinator.attach(to: terminal)
            terminal.onReady = { [weak self, weak coordinator, weak terminal] in
                guard let self, let terminal else { return }
                self.synchronizeTerminalGeometry(force: true)
                coordinator?.terminalDidBecomeReady(terminal)
            }
            addSubview(terminal)
            self.terminal = terminal
            synchronizeTerminalGeometry(force: true)
        }

        func updateAvailableSize(_ size: CGSize) {
            guard size.width > 0, size.height > 0 else { return }
            if bounds.size != size {
                setNeedsLayout()
            }
            synchronizeTerminalGeometry()
        }

        private func synchronizeTerminalGeometry(force: Bool = false) {
            guard bounds.width > 0, bounds.height > 0, let terminal else { return }
            let size = bounds.size
            terminal.frame = bounds
            guard force || size != lastReportedSize else { return }
            lastReportedSize = size
            terminal.sizeDidChange(size)
        }
    }

    final class Coordinator: @unchecked Sendable {
        private weak var terminal: GhosttyTerminalView?
        private var session: OrlixLinuxSession?
        private var output: OrlixTerminalOutput?
        private var started = false
        private var isTerminalReady = false
        private var latestGridSize: (rows: UInt32, columns: UInt32)?
        private var lastForwardedGridSize: (rows: UInt32, columns: UInt32)?

        @MainActor
        func attach(to terminal: GhosttyTerminalView) {
            self.terminal = terminal
            terminal.writeCallback = { [weak self] data in
                self?.session?.terminal.send(data)
            }
            terminal.setupWriteCallback()
            terminal.onResize = { [weak self] columns, rows in
                guard columns > 0, rows > 0 else { return }
                DispatchQueue.main.async {
                    self?.recordGridSize(rows: UInt32(rows), columns: UInt32(columns))
                }
            }
        }

        @MainActor
        func terminalDidBecomeReady(_ terminal: GhosttyTerminalView) {
            isTerminalReady = true
            if let size = terminal.terminalSize(), size.rows > 0, size.columns > 0 {
                recordGridSize(rows: UInt32(size.rows), columns: UInt32(size.columns))
            } else {
                startIfReady()
            }
        }

        @MainActor
        private func recordGridSize(rows: UInt32, columns: UInt32) {
            let size = (rows: rows, columns: columns)
            latestGridSize = size
            if let session,
               lastForwardedGridSize?.rows != rows || lastForwardedGridSize?.columns != columns {
                session.terminal.resize(rows: rows, columns: columns)
                lastForwardedGridSize = size
            }
            startIfReady()
        }

        @MainActor
        private func startIfReady() {
            guard isTerminalReady, let latestGridSize, !started else { return }
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

            session.terminal.resize(
                rows: latestGridSize.rows,
                columns: latestGridSize.columns
            )
            lastForwardedGridSize = latestGridSize

            output = session.terminal.attachOutput { [weak self] data in
                DispatchQueue.main.async {
                    self?.terminal?.feedData(data)
                    self?.terminal?.accessibilityValue = "output"
                }
            }

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
            latestGridSize = nil
            lastForwardedGridSize = nil
            isTerminalReady = false
        }

        @MainActor
        private func showError(_ message: String) {
            terminal?.accessibilityValue = "failed"
            terminal?.feedData(Data("\r\n\u{001B}[31m\(message)\u{001B}[0m\r\n".utf8))
        }
    }
}
#endif
