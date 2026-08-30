import SwiftUI
import UIKit

#if os(iOS) && canImport(OrlixOS)
import OrlixOS

struct DefaultLocalInstanceTerminalView: View {
    @EnvironmentObject private var ghosttyApp: GhosttyRuntime
    @EnvironmentObject private var terminalAccessoryPreferencesManager: TerminalAccessoryPreferencesManager
    @Environment(\.colorScheme) private var colorScheme
    @AppStorage(TerminalThemeUserDefaultsKeys.live.darkTheme) private var terminalThemeName = "Orlix Dark"
    @AppStorage(TerminalThemeUserDefaultsKeys.live.lightTheme) private var terminalThemeNameLight = "Orlix Light"
    @AppStorage(TerminalThemeUserDefaultsKeys.live.usesPerAppearanceTheme) private var usePerAppearanceTheme = true
    @AppStorage("terminalKeyboardDismissButtonEnabled") private var keyboardDismissButtonEnabled = true

    private var effectiveThemeName: String {
        guard usePerAppearanceTheme else { return terminalThemeName }
        return colorScheme == .dark ? terminalThemeName : terminalThemeNameLight
    }

    var body: some View {
        GeometryReader { geometry in
            DefaultLocalInstanceTerminalRepresentable(
                size: geometry.size,
                terminalAccessoryInputSnapshot: TerminalAccessoryInputSnapshot(
                    profile: terminalAccessoryPreferencesManager.profile,
                    showsDismissKeyboardButton: keyboardDismissButtonEnabled
                )
            )
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
    @EnvironmentObject private var ghosttyApp: GhosttyRuntime

    let size: CGSize
    let terminalAccessoryInputSnapshot: TerminalAccessoryInputSnapshot

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
            terminalAccessoryInputSnapshot: terminalAccessoryInputSnapshot,
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
            appWrapper: GhosttyRuntime,
            terminalAccessoryInputSnapshot: TerminalAccessoryInputSnapshot,
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
                terminalAccessoryInputSnapshot: terminalAccessoryInputSnapshot,
                useCustomIO: true
            )
            terminal.isAccessibilityElement = false
            terminal.imeProxyTextView.accessibilityIdentifier = "orlix.local-instance.terminal"
            terminal.imeProxyTextView.accessibilityLabel = "Orlix local terminal"
            terminal.imeProxyTextView.accessibilityValue = "initializing"
            terminal.imeProxyTextView.isAccessibilityElement = true
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
        private var session: OrlixTerminalSession?
        private var output: OrlixTerminalOutput?
        private var started = false
        private var isTerminalReady = false
        private var latestGridSize: (rows: UInt32, columns: UInt32)?
        private var lastForwardedGridSize: (rows: UInt32, columns: UInt32)?

        @MainActor
        func attach(to terminal: GhosttyTerminalView) {
            self.terminal = terminal
            terminal.writeCallback = { [weak self] data in
                self?.session?.send(data)
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
                session.resize(rows: rows, columns: columns)
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

            let machine = OrlixOS.shared.defaultMachine
            guard let session = OrlixOS.shared.openTerminal(
                for: machine,
                bootConfig: OrlixBootConfig(
                    profile: profile,
                    rootImageIdentifier: rootImageIdentifier,
                    terminalIdentifier: "orlix.local.default"
                )
            ) else {
                showError("OrlixOS could not open a terminal for the default machine.")
                return
            }
            self.session = session

            session.resize(
                rows: latestGridSize.rows,
                columns: latestGridSize.columns
            )
            lastForwardedGridSize = latestGridSize

            output = session.attachOutput { [weak self] data in
                DispatchQueue.main.async {
                    self?.terminal?.feedData(data)
                    self?.terminal?.imeProxyTextView.accessibilityValue = "output"
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
            if let session {
                session.send(Data([0x04]))
                OrlixOS.shared.closeTerminal(
                    session,
                    for: OrlixOS.shared.defaultMachine
                )
            }
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
            terminal?.imeProxyTextView.accessibilityValue = "failed"
            terminal?.feedData(Data("\r\n\u{001B}[31m\(message)\u{001B}[0m\r\n".utf8))
        }
    }
}
#endif
