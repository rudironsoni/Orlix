import Combine
import Foundation
import os.log

#if ORLIX_MODULAR_APP
import OrlixModels
import OrlixSSH
import OrlixSettings
#endif
@MainActor
package struct TerminalRemoteSessionShellRegistration {
    package let client: SSHClient
    package let shellID: UUID
    package let serverID: UUID
}

/// Owns persistent remote-session selection, launch, cleanup, and termination.
@MainActor
final package class TerminalRemoteSessionCoordinator: ObservableObject {
    package struct CleanupKey: Hashable {
        package let serverID: UUID
        package let backendIdentifier: RemoteSessionBackendIdentifier
    }

    package var attachPrompt: RemoteSessionAttachPrompt? {
        resolver.currentPrompt
    }

    package var backendMetadata: [RemoteSessionBackendMetadata] {
        remoteSessions.backendMetadata
    }

    package let configuration: TerminalRemoteSessionConfiguration
    package let remoteSessions: any TerminalRemoteSessionServicing
    package let resolver: RemoteSessionAttachResolver
    package let sessionState: TerminalSessionStateStore
    package let transportLifetime: TerminalTransportLifetime
    package let logger = Logger(
        subsystem: Bundle.main.bundleIdentifier ?? "Orlix",
        category: "TerminalRemoteSessionCoordinator"
    )
    package var completedCleanup: Set<CleanupKey> = []
    private var cancellables: Set<AnyCancellable> = []

    package init(
        configuration: TerminalRemoteSessionConfiguration,
        remoteSessions: any TerminalRemoteSessionServicing,
        resolver: RemoteSessionAttachResolver,
        sessionState: TerminalSessionStateStore,
        transportLifetime: TerminalTransportLifetime
    ) {
        self.configuration = configuration
        self.remoteSessions = remoteSessions
        self.resolver = resolver
        self.sessionState = sessionState
        self.transportLifetime = transportLifetime
        resolver.objectWillChange
            .sink { [weak self] in self?.objectWillChange.send() }
            .store(in: &cancellables)
    }

    // MARK: - Settings and attachment state

    package func isEnabled(for serverID: UUID) -> Bool {
        resolver.isEnabled(for: serverID)
    }

    package func backendIdentifier(for serverID: UUID) -> RemoteSessionBackendIdentifier {
        resolver.backendIdentifier(for: serverID)
    }

    package func attachment(for paneID: UUID) -> TerminalRemoteSessionAttachmentState? {
        resolver.attachment(for: paneID)
    }

    package func setAttachment(
        for paneID: UUID,
        identifier: RemoteSessionIdentifier,
        ownership: RemoteSessionOwnership,
        managedSessionConfirmed: Bool = false
    ) {
        resolver.setAttachment(
            TerminalRemoteSessionAttachmentState(
                attachment: RemoteSessionAttachment(
                    identifier: identifier,
                    ownership: ownership
                ),
                managedSessionConfirmed: managedSessionConfirmed
            ),
            for: paneID
        )
    }

    package func clearAttachmentState(for paneID: UUID) {
        resolver.clearAttachmentState(for: paneID)
        clearResumeContext(for: paneID)
    }

    package func confirmManagedSession(for paneID: UUID) {
        resolver.confirmManagedSession(for: paneID)
    }

    package func shouldReattachManagedSession(
        for paneID: UUID,
        backendIdentifier: RemoteSessionBackendIdentifier
    ) -> Bool {
        guard let state = resolver.attachment(for: paneID) else { return false }
        return state.attachment.identifier.backendIdentifier == backendIdentifier
            && state.attachment.ownership == .managed
            && state.managedSessionConfirmed
    }

    // MARK: - Prompt ownership

    package func resolvePrompt(
        requestID: UUID,
        selection: RemoteSessionAttachSelection
    ) {
        resolver.resolvePrompt(requestID: requestID, selection: selection)
    }

    package func cancelPrompt(requestID: UUID) {
        resolver.cancelPrompt(requestID: requestID)
    }

    package func hasPendingPrompt(requestID: UUID) -> Bool {
        resolver.hasPendingPrompt(requestID: requestID)
    }

    package func requestSelection(
        requestID: UUID,
        paneID: UUID,
        backendIdentifier: RemoteSessionBackendIdentifier,
        availableSessions: [RemoteSessionSelectionInfo]
    ) async -> RemoteSessionAttachSelection {
        await resolver.requestSelection(
            requestID: requestID,
            entityID: paneID,
            backendIdentifier: backendIdentifier,
            availableSessions: availableSessions
        )
    }

    package func cancelPrompt(for startContext: SSHShellRegistry.StartContext?) {
        guard let startContext else { return }
        cancelPrompt(requestID: startContext.token.id)
    }

    package func clearRuntimeState(for paneID: UUID) {
        resolver.clearRuntimeState(for: paneID)
        clearResumeContext(for: paneID)
    }

    // MARK: - Pane state

    package func status(for paneID: UUID) -> RemoteSessionStatus? {
        sessionState.paneState(for: paneID)?.remoteSessionStatus
    }

    package func updateStatus(_ status: RemoteSessionStatus, for paneID: UUID) {
        guard let previous = sessionState.paneState(for: paneID)?.remoteSessionStatus,
              previous != status else {
            return
        }
        sessionState.updatePane(paneID) { $0.remoteSessionStatus = status }
        logger.info(
            "Remote session status for pane \(paneID.uuidString, privacy: .public) changed from \(previous.rawValue, privacy: .public) to \(status.rawValue, privacy: .public)"
        )
    }

    package func shouldApplyWorkingDirectory(for paneID: UUID) -> Bool {
        guard let status = status(for: paneID) else { return false }
        return status == .off || status == .missing
    }

    package func updateSelectionStatuses(selectedTabs: [UUID: UUID]) {
        for serverID in sessionState.serverIdsWithTabs {
            for tab in sessionState.tabs(for: serverID) {
                updateFocus(
                    for: tab,
                    isSelectedTab: selectedTabs[serverID] == tab.id
                )
            }
        }
    }

    package func updateFocus(for tab: TerminalTab) {
        updateFocus(
            for: tab,
            isSelectedTab: sessionState.selectedTabId(for: tab.serverId) == tab.id
        )
    }

    private func updateFocus(for tab: TerminalTab, isSelectedTab: Bool) {
        for paneID in tab.allPaneIds {
            guard let state = sessionState.paneState(for: paneID),
                  state.remoteSessionStatus == .foreground
                    || state.remoteSessionStatus == .background else {
                continue
            }
            updateStatus(
                isSelectedTab && tab.focusedPaneId == paneID ? .foreground : .background,
                for: paneID
            )
        }
    }

    package func disable(for serverID: UUID) {
        for state in sessionState.paneStates(forServer: serverID) {
            updateStatus(.off, for: state.paneId)
            clearRuntimeState(for: state.paneId)
        }
    }

}

#if DEBUG
package extension TerminalRemoteSessionCoordinator {
    convenience package init() {
        let configuration = TerminalRemoteSessionConfiguration.testing
        let remoteSessions = UnavailableTerminalRemoteSessionService()
        let resolver = RemoteSessionAttachResolver(
            configuration: configuration,
            remoteSessions: remoteSessions
        )
        let sessionState = TerminalSessionStateStore(
            snapshotStore: EmptyTerminalTabSnapshotStore(),
            connectionViewSelections: ConnectionViewSelectionStore(),
            remoteSessionResolver: resolver
        )
        self.init(
            configuration: configuration,
            remoteSessions: remoteSessions,
            resolver: resolver,
            sessionState: sessionState,
            transportLifetime: TerminalTransportLifetime()
        )
    }
}

@MainActor
private final class EmptyTerminalTabSnapshotStore: TerminalTabSnapshotStoring {
    package func loadSnapshotData() -> Data? { nil }
    package func saveSnapshotData(_ data: Data) {}
    package func removeSnapshotData() {}
}

package extension TerminalRemoteSessionConfiguration {
    static package var testing: Self {
        Self(
            deviceID: "test-device",
            enabledByDefault: { true },
            backendIdentifierByDefault: { .tmux },
            startupBehaviorByDefault: { .ask },
            serverSettings: { _ in nil },
            themeStyle: {
                RemoteSessionThemeStyle(
                    name: "Orlix Dark",
                    modeStyle: "bg=default,fg=default"
                )
            }
        )
    }
}

package actor UnavailableTerminalRemoteSessionService: TerminalRemoteSessionServicing {
    nonisolated package let backendMetadata = [RemoteSessionBackendMetadata(
        identifier: .tmux,
        displayName: "tmux",
        installation: .automatic,
        managedStartupCommandSupport: .supported
    )]

    package func availability(
        for backendIdentifier: RemoteSessionBackendIdentifier,
        using client: SSHClient
    ) async -> RemoteSessionAvailability { .unsupportedEnvironment }

    package func listSessions(
        scope: RemoteSessionListScope,
        using client: SSHClient,
        runtime: RemoteSessionRuntime
    ) async throws -> [RemoteSessionDescriptor] { throw SSHError.notConnected }

    package func prepareManagedSession(
        using client: SSHClient,
        terminalType: RemoteTerminalType,
        themeStyle: RemoteSessionThemeStyle,
        runtime: RemoteSessionRuntime
    ) async {}

    package func launchPlan(
        for request: RemoteSessionLaunchRequest,
        runtime: RemoteSessionRuntime
    ) async throws -> RemoteSessionBackendLaunchPlan { throw SSHError.notConnected }

    package func installScript(
        attachment: RemoteSessionAttachment,
        workingDirectory: String,
        terminalType: RemoteTerminalType,
        themeStyle: RemoteSessionThemeStyle,
        using client: SSHClient,
        attachAfterInstall: Bool
    ) async -> String? { nil }

    package func sendScript(
        _ script: String,
        using client: SSHClient,
        shellId: UUID
    ) async throws { throw SSHError.notConnected }

    package func killSession(
        _ identifier: RemoteSessionIdentifier,
        using client: SSHClient,
        runtime: RemoteSessionRuntime?
    ) async {}

    package func cleanupSessions(
        keeping identifiers: Set<RemoteSessionIdentifier>,
        using client: SSHClient,
        runtime: RemoteSessionRuntime
    ) async {}

    package func currentWorkingDirectory(
        for attachment: RemoteSessionAttachment,
        using client: SSHClient,
        runtime: RemoteSessionRuntime?
    ) async -> String? { nil }
}
#endif
