import CoreGraphics
import Foundation

/// Selects one application transport coordinator per pane. Transport state is
/// retained by the app-scoped transport owner so view reconstruction cannot
/// replace a live session.
@MainActor
final class TerminalPaneConnectionCoordinator {
    private enum Backend {
        case ssh(TerminalPaneSSHCoordinator)
        case eternalTerminal(EternalTerminalPaneCoordinator)
        case tssh(TSSHPaneCoordinator)
    }

    let tabManager: TerminalTabManager
    weak var terminal: (any TerminalSurface)?
    var isTerminalReady = false
    var preservePane = false
    var lastReportedSize: CGSize = .zero
    private let paneID: UUID
    private let sshFailureOutput: @MainActor @Sendable (TerminalConnectionFailure) -> Data?
    private var connectionMode: SSHConnectionMode
    private var backend: Backend
    private var backendGeneration = UUID()
    private var backendReplacementTask: Task<Void, Never>?

    init(
        paneId: UUID,
        server: Server,
        credentials: ServerCredentials,
        tabManager: TerminalTabManager,
        sshFailureOutput: @escaping @MainActor @Sendable (TerminalConnectionFailure) -> Data?
    ) {
        paneID = paneId
        self.tabManager = tabManager
        self.sshFailureOutput = sshFailureOutput
        connectionMode = server.connectionMode
        backend = Self.makeBackend(
            paneId: paneId,
            server: server,
            credentials: credentials,
            tabManager: tabManager,
            sshFailureOutput: sshFailureOutput
        )
    }

    private static func makeBackend(
        paneId: UUID,
        server: Server,
        credentials: ServerCredentials,
        tabManager: TerminalTabManager,
        sshFailureOutput: @escaping @MainActor @Sendable (TerminalConnectionFailure) -> Data?
    ) -> Backend {
        if server.connectionMode == .tssh {
            return .tssh(TSSHPaneCoordinator(
                paneId: paneId,
                server: server,
                credentials: credentials,
                tabManager: tabManager
            ))
        } else if server.connectionMode == .eternalTerminal {
            return .eternalTerminal(EternalTerminalPaneCoordinator(
                paneId: paneId,
                server: server,
                credentials: credentials,
                tabManager: tabManager
            ))
        }
        return .ssh(TerminalPaneSSHCoordinator(
            paneId: paneId,
            server: server,
            credentials: credentials,
            sshClient: tabManager.transportCoordinator.makeSSHClient(),
            tabManager: tabManager,
            failureOutput: sshFailureOutput
        ))
    }

    var paneId: UUID {
        switch backend {
        case .ssh(let coordinator): coordinator.paneId
        case .eternalTerminal(let coordinator): coordinator.paneId
        case .tssh(let coordinator): coordinator.paneId
        }
    }

    var hasLiveConnection: Bool {
        tabManager.transportCoordinator.hasLiveTransport(for: paneId)
    }

    var isConnectionStartInFlight: Bool {
        tabManager.transportCoordinator.isTransportStartInFlight(for: paneId)
    }

    func installRichPasteInterception(on terminal: any TerminalSurface) {
        guard case .ssh(let coordinator) = backend else { return }
        coordinator.installRichPasteInterception(on: terminal)
    }

    func sendToTransport(_ data: Data) {
        switch backend {
        case .ssh(let coordinator): coordinator.sendToSSH(data)
        case .eternalTerminal(let coordinator): coordinator.send(data)
        case .tssh(let coordinator): coordinator.send(data)
        }
    }

    func handleResize(cols: Int, rows: Int) {
        let pixelSize = terminal?.terminalGeometry?.pixelSize
        switch backend {
        case .ssh(let coordinator):
            coordinator.handleResize(cols: cols, rows: rows, pixelSize: pixelSize)
        case .eternalTerminal(let coordinator):
            coordinator.handleResize(cols: cols, rows: rows, pixelSize: pixelSize)
        case .tssh(let coordinator):
            coordinator.handleResize(cols: cols, rows: rows, pixelSize: pixelSize)
        }
    }

    func startConnection(terminal: any TerminalSurface) {
        self.terminal = terminal
        switch backend {
        case .ssh(let coordinator): coordinator.startSSHConnection(terminal: terminal)
        case .eternalTerminal(let coordinator): coordinator.start(terminal: terminal)
        case .tssh(let coordinator): coordinator.start(terminal: terminal)
        }
    }

    func updateSecurityBinding(server: Server, credentials: ServerCredentials) {
        guard connectionMode != server.connectionMode else {
            guard case .tssh(let coordinator) = backend else { return }
            coordinator.updateSecurityBinding(server: server, credentials: credentials)
            return
        }

        let previousBackend = backend
        let generation = UUID()
        connectionMode = server.connectionMode
        backendGeneration = generation
        backend = Self.makeBackend(
            paneId: paneID,
            server: server,
            credentials: credentials,
            tabManager: tabManager,
            sshFailureOutput: sshFailureOutput
        )
        cancel(previousBackend)
        let previousReplacementTask = backendReplacementTask
        backendReplacementTask = Task { @MainActor [weak self] in
            await previousReplacementTask?.value
            guard let self else { return }
            await self.unregister(previousBackend)
            guard self.backendGeneration == generation,
                  let terminal = self.terminal else { return }
            self.installRichPasteInterception(on: terminal)
            if self.isTerminalReady {
                self.startConnection(terminal: terminal)
            }
        }
    }

    func cancelConnection() {
        terminal = nil
        cancel(backend)
    }

    private func cancel(_ backend: Backend) {
        switch backend {
        case .ssh:
            break
        case .eternalTerminal(let coordinator): coordinator.cancel()
        case .tssh(let coordinator): coordinator.cancel()
        }
    }

    private func unregister(_ backend: Backend) async {
        switch backend {
        case .ssh:
            await tabManager.transportCoordinator.unregisterSSHClient(for: paneID)
        case .eternalTerminal:
            await tabManager.transportCoordinator.unregisterEternalTerminalRuntime(for: paneID)
        case .tssh:
            await tabManager.transportCoordinator.unregisterTSSHRuntime(for: paneID)
        }
    }
}

@MainActor
private final class TSSHPaneCoordinator {
    let paneId: UUID
    private var server: Server
    private var credentials: ServerCredentials
    let tabManager: TerminalTabManager
    private var startTask: Task<Void, Never>?

    init(
        paneId: UUID,
        server: Server,
        credentials: ServerCredentials,
        tabManager: TerminalTabManager
    ) {
        self.paneId = paneId
        self.server = server
        self.credentials = credentials
        self.tabManager = tabManager
    }

    func start(terminal: any TerminalSurface) {
        startTask?.cancel()
        startTask = Task { @MainActor [weak self, weak terminal] in
            guard let self, let terminal,
                  !Task.isCancelled,
                  tabManager.sessionState.containsPane(paneId) else { return }
            guard let runtime = await tabManager.transportCoordinator.tsshRuntime(
                for: paneId,
                server: server,
                credentials: credentials
            ) else { return }
            guard !Task.isCancelled,
                  tabManager.sessionState.containsPane(paneId) else {
                await tabManager.transportCoordinator.unregisterTSSHRuntime(
                    for: paneId,
                    ifOwnedByToken: runtime.identityToken
                )
                return
            }
            runtime.attach(to: terminal)
            if let geometry = terminal.terminalGeometry {
                runtime.resize(
                    cols: geometry.columns,
                    rows: geometry.rows,
                    pixelSize: geometry.pixelSize
                )
            }
            runtime.startIfNeeded()
        }
    }

    func updateSecurityBinding(server: Server, credentials: ServerCredentials) {
        self.server = server
        self.credentials = credentials
        tabManager.transportCoordinator.invalidateTSSHRuntime(
            for: paneId,
            unlessBoundTo: server,
            credentials: credentials
        )
    }

    func send(_ data: Data) {
        tabManager.transportCoordinator.sendTSSHInput(data, for: paneId)
    }

    func handleResize(cols: Int, rows: Int, pixelSize: TerminalPixelSize?) {
        tabManager.transportCoordinator.resizeTSSH(
            for: paneId,
            cols: cols,
            rows: rows,
            pixelSize: pixelSize
        )
    }

    func cancel() {
        startTask?.cancel()
        startTask = nil
        tabManager.transportCoordinator.unregisterTSSHRuntimeIfPaneWasRemoved(for: paneId)
    }
}

@MainActor
private final class EternalTerminalPaneCoordinator {
    let paneId: UUID
    let server: Server
    let credentials: ServerCredentials
    let tabManager: TerminalTabManager

    init(
        paneId: UUID,
        server: Server,
        credentials: ServerCredentials,
        tabManager: TerminalTabManager
    ) {
        self.paneId = paneId
        self.server = server
        self.credentials = credentials
        self.tabManager = tabManager
    }

    func start(terminal: any TerminalSurface) {
        let runtime = tabManager.transportCoordinator.eternalTerminalRuntime(
            for: paneId,
            server: server,
            credentials: credentials
        )
        runtime.attach(to: terminal)
        guard let geometry = terminal.terminalGeometry else { return }
        runtime.resize(
            cols: geometry.columns,
            rows: geometry.rows,
            pixelSize: geometry.pixelSize
        )
        runtime.startIfNeeded()
    }

    func send(_ data: Data) {
        tabManager.transportCoordinator.sendEternalTerminalInput(data, for: paneId)
    }

    func handleResize(cols: Int, rows: Int, pixelSize: TerminalPixelSize?) {
        tabManager.transportCoordinator.resizeEternalTerminal(
            for: paneId,
            cols: cols,
            rows: rows,
            pixelSize: pixelSize
        )
    }

    func cancel() {
        tabManager.transportCoordinator.unregisterEternalTerminalRuntimeIfPaneWasRemoved(
            for: paneId
        )
    }
}
