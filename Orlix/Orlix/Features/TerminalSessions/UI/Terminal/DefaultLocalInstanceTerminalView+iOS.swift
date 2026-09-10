import os
import OrlixOS
import SwiftUI
import UIKit

#if os(iOS)
struct DefaultLocalInstanceTerminalView: View {
    let tabManager: TerminalTabManager
    @ObservedObject private var keyboardCoordinator: TerminalKeyboardCoordinator
    @EnvironmentObject private var ghosttyApp: GhosttyRuntime
    @EnvironmentObject private var terminalThemeManager: TerminalThemeManager
    @EnvironmentObject private var terminalAccessoryPreferencesManager: TerminalAccessoryPreferencesManager
    @Environment(\.colorScheme) private var colorScheme
    @Environment(\.scenePhase) private var scenePhase
    @AppStorage("terminalKeyboardDismissButtonEnabled") private var keyboardDismissButtonEnabled = true
    @State private var paneId = UUID()
    @State private var isVisible = false
    @State private var surfaceChange: TerminalSurfaceStoreChange?

    init(tabManager: TerminalTabManager) {
        self.tabManager = tabManager
        _keyboardCoordinator = ObservedObject(wrappedValue: tabManager.keyboardCoordinator)
    }

    private var appearance: TerminalAppearanceSnapshot {
        terminalThemeManager.appearanceSnapshot(for: colorScheme == .dark ? .dark : .light)
    }

    var body: some View {
        GeometryReader { geometry in
            ZStack {
                Color.fromHex(appearance.activeTheme.palette.backgroundHex)
                if ghosttyApp.readiness == .ready {
                    DefaultLocalInstanceTerminalRepresentable(
                        paneId: paneId,
                        tabManager: tabManager,
                        size: geometry.size,
                        terminalAccessoryInputSnapshot: TerminalAccessoryInputSnapshot(
                            profile: terminalAccessoryPreferencesManager.profile,
                            showsDismissKeyboardButton: keyboardDismissButtonEnabled
                        )
                    )
                }
            }
        }
        .terminalKeyboardAvoidance(
            focusedPaneId: paneId,
            paneIds: [paneId],
            terminalSurfaceChange: surfaceChange,
            terminalProvider: { tabManager.terminalSurfaceStore.ghosttySurface(for: $0) },
            keyboardCoordinator: keyboardCoordinator
        )
        .overlay(alignment: .bottom) {
            if !keyboardCoordinator.isSoftwareKeyboardVisible {
                TerminalFloatingControlButton.keyboard(showsTitle: true) {
                    keyboardCoordinator.userRequestedShow()
                }
                .padding(.bottom, 4)
            }
        }
        .navigationTitle("Orlix")
        .navigationBarTitleDisplayMode(.inline)
        .onReceive(tabManager.terminalSurfaceStore.changes) { surfaceChange = $0 }
        .task(id: appearance) {
            Logger(subsystem: "com.rudironsoni.orlix", category: "LocalInstance")
                .info("local instance terminal appeared; starting shared Ghostty surface")
            ghosttyApp.startIfNeeded(appearance: terminalThemeManager.activateAppearance(
                colorScheme == .dark ? .dark : .light
            ))
        }
        .onAppear {
            isVisible = true
            keyboardCoordinator.setActivePane(paneId)
            keyboardCoordinator.setViewActive(scenePhase == .active)
        }
        .onChange(of: scenePhase) { phase in
            keyboardCoordinator.setViewActive(isVisible && phase == .active)
        }
        .onDisappear {
            isVisible = false
            keyboardCoordinator.setViewActive(false)
            keyboardCoordinator.setActivePane(nil)
        }
    }
}

private struct DefaultLocalInstanceTerminalRepresentable: View {
    let paneId: UUID
    let tabManager: TerminalTabManager
    let size: CGSize
    let terminalAccessoryInputSnapshot: TerminalAccessoryInputSnapshot

    var body: some View {
        TerminalPaneSurface(
            paneId: paneId.uuidString,
            size: size,
            isActive: true,
            presentationOverrides: .empty,
            terminalAccessoryInputSnapshot: terminalAccessoryInputSnapshot,
            makeBackend: { Coordinator(paneId: paneId, tabManager: tabManager) },
            reusableTerminal: { _ in nil },
            configure: { terminal, coordinator, _ in
                terminal.accessibilityIdentifier = "orlix.local-instance.terminal"
                terminal.accessibilityLabel = "Orlix local terminal"
                terminal.accessibilityValue = "initializing"
                coordinator.attach(to: terminal)
                terminal.onReady = { [weak coordinator, weak terminal] in
                    guard let terminal else { return }
                    coordinator?.terminalDidBecomeReady(terminal)
                }
            },
            update: { _, _ in },
            dismantle: { _, coordinator in coordinator.stop() }
        )
    }

    final class Coordinator: @unchecked Sendable {
        private let paneId: UUID
        private let tabManager: TerminalTabManager
        private weak var terminal: GhosttyTerminalView?
        private var session: OrlixTerminalSession?
        private var output: OrlixTerminalOutput?
        private var started = false
        private var isTerminalReady = false
        private var latestGridSize: (rows: UInt32, columns: UInt32)?
        private var lastForwardedGridSize: (rows: UInt32, columns: UInt32)?

        @MainActor
        init(paneId: UUID, tabManager: TerminalTabManager) {
            self.paneId = paneId
            self.tabManager = tabManager
        }

        @MainActor
        func attach(to terminal: GhosttyTerminalView) {
            self.terminal = terminal
            tabManager.registerTerminalSurface(terminal, for: paneId, inputEligible: true)
            terminal.writeCallback = { [weak self] data in
                Logger(subsystem: "com.rudironsoni.orlix", category: "LocalInstance")
                    .debug("local instance input bytes=\(data.count, privacy: .public)")
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
                Logger(subsystem: "com.rudironsoni.orlix", category: "LocalInstance")
                    .error("local instance boot failed: missing OrlixOS payload metadata")
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
                Logger(subsystem: "com.rudironsoni.orlix", category: "LocalInstance")
                    .error("local instance boot failed: openTerminal returned nil")
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
                if let text = String(data: data, encoding: .utf8),
                   text.contains("orlix-init: process started pid=") {
                    Logger(subsystem: "com.rudironsoni.orlix", category: "LocalInstance")
                        .info("local instance init reported child process start")
                }
                DispatchQueue.main.async {
                    self?.terminal?.feedData(data)
                    self?.terminal?.accessibilityValue = "output"
                }
            }

            Logger(subsystem: "com.rudironsoni.orlix", category: "LocalInstance")
                .info("local instance session opened; booting profile=\(String(describing: profile), privacy: .public)")
            DispatchQueue.global(qos: .userInitiated).async { [weak self, session] in
                let status = session.boot()
                guard status != .ok else {
                    Logger(subsystem: "com.rudironsoni.orlix", category: "LocalInstance")
                        .info("local instance boot ok")
                    return
                }
                Logger(subsystem: "com.rudironsoni.orlix", category: "LocalInstance")
                    .error("local instance boot failed: \(status.message, privacy: .public)")
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
            if let terminal {
                tabManager.unregisterTerminalSurface(terminal, for: paneId)
            }
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
