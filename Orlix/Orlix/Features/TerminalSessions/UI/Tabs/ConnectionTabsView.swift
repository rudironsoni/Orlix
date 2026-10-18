//
//  ConnectionTabsView.swift
//  Orlix
//

import SwiftUI

#if ORLIX_MODULAR_APP
import OrlixFiles
import OrlixModels
import OrlixSSH
import OrlixSettings
import OrlixStats
#endif
package struct TerminalServerToolbarProjectionHost<Content: View>: View {
    @StateObject private var projection: TerminalServerToolbarProjection
    private let content: (TerminalServerToolbarProjection) -> Content

    package init(
        serverId: UUID,
        tabManager: TerminalTabManager,
        @ViewBuilder content: @escaping (TerminalServerToolbarProjection) -> Content
    ) {
        _projection = StateObject(
            wrappedValue: TerminalServerToolbarProjection(
                serverId: serverId,
                tabManager: tabManager
            )
        )
        self.content = content
    }

    package var body: some View {
        content(projection)
    }
}

// MARK: - Connection Terminal Container

package struct ConnectionTerminalContainer: View {
    package let tabManager: TerminalTabManager
    package let terminalToolbarProjection: TerminalServerToolbarProjection
    @ObservedObject package var terminalContent: TerminalServerContentProjection
    @ObservedObject private var remoteSessionCoordinator: TerminalRemoteSessionCoordinator
    @ObservedObject package var fileTabManager: RemoteFileTabManager
    package let serverManager: ServerManager
    package let fileBrowser: RemoteFileBrowserStore
    package let makeLocalDiscoveryManager: LocalSSHDiscoveryManagerFactory
    package let statsDependencies: ServerStatsScreenDependencies
    package let terminalSecurityActions: TerminalSecurityActions
    package let serverFormDependencies: ServerFormDependencies
    package let voiceInputRuntimeStore: VoiceInputRuntimeStore
    package let server: Server
    @Binding package var isZenModeEnabled: Bool
    package let isSidebarVisible: Bool
    package let onToggleSidebar: () -> Void
    package let onOpenSettings: (() -> Void)?
    package let onLeaveRoute: (() -> Void)?
    package let onDisconnectRoute: (() -> Void)?

    @EnvironmentObject package var ghosttyApp: GhosttyRuntime
    @EnvironmentObject package var storeManager: StoreManager
    @EnvironmentObject private var terminalThemeManager: TerminalThemeManager
    @Environment(\.colorScheme) private var colorScheme
    @EnvironmentObject private var viewTabConfig: ViewTabConfigurationManager

    /// Disconnect confirmation
    @State package var showingDisconnectConfirmation = false
    /// Confirmation before closing the focused split pane via a command/panel
    /// (the in-pane close button has its own confirmation in TerminalTabView).
    @State package var showingPaneCloseConfirmation = false
    @State package var serverFormIntent: ServerFormIntent?

    /// Tab limit alert
    @State private var showingTabLimitAlert = false
    @State package var isShowingDockerUpgrade = false
    @State package var showingFileTabLimitAlert = false
    @State package var showingSplitPaneUpgradeAlert = false
    @State package var showingZenPanel = false

    package init(
        tabManager: TerminalTabManager,
        terminalToolbarProjection: TerminalServerToolbarProjection,
        fileTabManager: RemoteFileTabManager,
        serverManager: ServerManager,
        fileBrowser: RemoteFileBrowserStore,
        makeLocalDiscoveryManager: @escaping LocalSSHDiscoveryManagerFactory,
        statsDependencies: ServerStatsScreenDependencies,
        terminalSecurityActions: TerminalSecurityActions,
        serverFormDependencies: ServerFormDependencies,
        voiceInputRuntimeStore: VoiceInputRuntimeStore,
        server: Server,
        isZenModeEnabled: Binding<Bool>,
        isSidebarVisible: Bool,
        onToggleSidebar: @escaping () -> Void,
        onOpenSettings: (() -> Void)?,
        onLeaveRoute: (() -> Void)?,
        onDisconnectRoute: (() -> Void)?
    ) {
        self.tabManager = tabManager
        self.terminalToolbarProjection = terminalToolbarProjection
        _terminalContent = ObservedObject(wrappedValue: terminalToolbarProjection.content)
        _remoteSessionCoordinator = ObservedObject(wrappedValue: tabManager.remoteSessionCoordinator)
        self.fileTabManager = fileTabManager
        self.serverManager = serverManager
        self.fileBrowser = fileBrowser
        self.makeLocalDiscoveryManager = makeLocalDiscoveryManager
        self.statsDependencies = statsDependencies
        self.terminalSecurityActions = terminalSecurityActions
        self.serverFormDependencies = serverFormDependencies
        self.voiceInputRuntimeStore = voiceInputRuntimeStore
        self.server = server
        _isZenModeEnabled = isZenModeEnabled
        self.isSidebarVisible = isSidebarVisible
        self.onToggleSidebar = onToggleSidebar
        self.onOpenSettings = onOpenSettings
        self.onLeaveRoute = onLeaveRoute
        self.onDisconnectRoute = onDisconnectRoute
    }

    /// Selected view type - persisted per server
    package var selectedView: ConnectionViewTabID {
        viewTabConfig.effectiveView(
            for: terminalContent.state.selectedView
        )
    }

    package var visibleViewTabs: [ConnectionViewTabID] {
        viewTabConfig.currentVisibleTabs
    }

    package var shouldShowViewPicker: Bool {
        visibleViewTabs.count > 1
    }

    package var terminalAppearance: TerminalColorAppearance {
        colorScheme == .dark ? .dark : .light
    }

    package var terminalAppearanceSnapshot: TerminalAppearanceSnapshot {
        terminalThemeManager.appearanceSnapshot(for: terminalAppearance)
    }

    package var selectedViewBinding: Binding<ConnectionViewTabID> {
        Binding(
            get: {
                viewTabConfig.effectiveView(
                    for: terminalContent.state.selectedView
                )
            },
            set: { newValue in
                let current = viewTabConfig.effectiveView(
                    for: tabManager.connectionViewSelections.selection(for: server.id)
                )
                guard current != newValue else { return }
                DispatchQueue.main.async {
                    tabManager.sessionState.selectView(
                        viewTabConfig.effectiveView(for: newValue),
                        for: server.id
                    )
                }
            }
        )
    }

    /// Tabs for THIS server only
    package var serverTabs: [TerminalTab] {
        terminalContent.state.tabs
    }

    /// Effective selected tab ID for this server.
    package var selectedTabId: UUID? {
        if let selectedId = terminalContent.state.selectedTabId,
           serverTabs.contains(where: { $0.id == selectedId }) {
            return selectedId
        }
        return serverTabs.first?.id
    }

    package var selectedTabIdBinding: Binding<UUID?> {
        Binding(
            get: { selectedTabId },
            set: { newValue in
                let validId = newValue.flatMap { requestedId in
                    serverTabs.contains(where: { $0.id == requestedId }) ? requestedId : serverTabs.first?.id
                }
                guard tabManager.sessionState.selectedTabId(for: server.id) != validId else { return }
                tabManager.sessionState.selectTab(validId, for: server.id)
            }
        )
    }

    /// Currently selected tab
    package var selectedTab: TerminalTab? {
        guard let id = selectedTabId else { return serverTabs.first }
        return serverTabs.first { $0.id == id } ?? serverTabs.first
    }

    package var serverFileTabs: [RemoteFileTab] {
        fileTabManager.tabs(for: server.id)
    }

    package var selectedFileTabId: UUID? {
        fileTabManager.selectedTab(for: server.id)?.id
    }

    package var selectedFileTabIdBinding: Binding<UUID?> {
        Binding(
            get: { selectedFileTabId },
            set: { newValue in
                guard let newValue,
                      let tab = serverFileTabs.first(where: { $0.id == newValue }) else {
                    return
                }
                DispatchQueue.main.async {
                    fileTabManager.selectTab(tab)
                }
            }
        )
    }

    package var selectedFileTab: RemoteFileTab? {
        fileTabManager.selectedTab(for: server.id)
    }

    private var remoteSessionAttachPromptBinding: Binding<RemoteSessionAttachPrompt?> {
        Binding(
            get: {
                guard let prompt = remoteSessionCoordinator.attachPrompt else { return nil }
                guard tabManager.sessionState.paneState(for: prompt.paneId)?.serverId == server.id else { return nil }
                return prompt
            },
            set: { newValue in
                guard newValue == nil,
                      let prompt = remoteSessionCoordinator.attachPrompt else { return }
                guard tabManager.sessionState.paneState(for: prompt.paneId)?.serverId == server.id else { return }
                remoteSessionCoordinator.cancelPrompt(requestID: prompt.id)
            }
        )
    }

    private var liveTerminalBackgroundColor: Color {
        Color.fromHex(terminalAppearanceSnapshot.activeTheme.palette.backgroundHex)
    }

    package var sharedBody: some View {
        let backgroundColor = liveTerminalBackgroundColor

        return platformChrome(backgroundColor: backgroundColor)
            .onAppear {
                voiceInputRuntimeStore.synchronize(
                    tabIDs: Set(serverTabs.map(\.id)),
                    for: server.id
                )
                repairSelectedTabSelectionIfNeeded()
                platformHandleSelectedViewChange(selectedView)
                ensureInitialFileTabIfNeeded()
            }
            .task(id: terminalAppearanceSnapshot) {
                let snapshot = terminalThemeManager.activateAppearance(terminalAppearance)
                ghosttyApp.applyAppearance(snapshot)
            }
            .onChange(of: selectedView) { newValue in
                platformHandleSelectedViewChange(newValue)
                ensureInitialFileTabIfNeeded()
            }
            .onChange(of: serverTabs.map(\.id)) { tabIDs in
                voiceInputRuntimeStore.synchronize(
                    tabIDs: Set(tabIDs),
                    for: server.id
                )
                repairSelectedTabSelectionIfNeeded()
            }
            .onChange(of: isZenModeEnabled) { newValue in
                if !newValue {
                    showingZenPanel = false
                }
            }
            .onDisappear {
                statsDependencies.runtimeStore.releaseCollector(for: server.id)
            }
            .limitReachedAlert(.tabs, isPresented: $showingTabLimitAlert)
            .limitReachedAlert(.fileTabs, isPresented: $showingFileTabLimitAlert)
            .splitPaneProFeatureAlert(isPresented: $showingSplitPaneUpgradeAlert)
    }

    @ViewBuilder
    package var filesLayer: some View {
        if let selectedFileTab {
            RemoteFileBrowserScreen(
                browser: fileBrowser,
                server: server,
                fileTab: selectedFileTab,
                appearance: terminalAppearanceSnapshot,
                initialPath: selectedFileTab.seedPath
            ) { currentPath in
                fileTabManager.updateLastKnownPath(currentPath, for: selectedFileTab.id)
            }
            .id(selectedFileTab.id)
            .zIndex(1)
        } else {
            RemoteFileTabsEmptyState(server: server) {
                openNewFileTab(selectFilesViewOnSuccess: false)
            }
            .zIndex(1)
        }
    }

    package var body: some View {
        platformBody
            .sheet(item: remoteSessionAttachPromptBinding) { prompt in
                RemoteSessionAttachPromptSheet(
                    prompt: prompt,
                    onConfirm: { selection in
                        tabManager.remoteSessionCoordinator.resolvePrompt(
                            requestID: prompt.id,
                            selection: selection
                        )
                    }
                )
                .adaptiveSoftScrollEdges()
            }
    }

    package func handleNewTabCommand() {
        if selectedView == .files {
            openNewFileTab(selectFilesViewOnSuccess: true)
        } else {
            openNewTab(selectTerminalViewOnSuccess: true)
        }
    }

    private func ensureInitialFileTabIfNeeded() {
        guard selectedView == .files else { return }

        let seedPath = selectedTab.flatMap {
            tabManager.sessionState.paneState(for: $0.focusedPaneId)?.workingDirectory
        }
        DispatchQueue.main.async {
            guard selectedView == .files else { return }
            guard let fileTab = fileTabManager.ensureInitialTab(
                for: server,
                seedPath: seedPath,
                hasProAccess: storeManager.allowsProFeatures
            ) else { return }
            fileBrowser.prepareNewTab(fileTab, duplicating: nil)
        }
    }

    private func repairSelectedTabSelectionIfNeeded() {
        let currentId = tabManager.sessionState.selectedTabId(for: server.id)
        let repairedId = selectedTabId
        guard currentId != repairedId else { return }
        tabManager.sessionState.selectTab(repairedId, for: server.id)
    }

    package func openNewTab(selectTerminalViewOnSuccess: Bool = false) {
        guard tabManager.sessionState.canOpenNewTab(hasProAccess: storeManager.allowsProFeatures) else {
            showingTabLimitAlert = true
            return
        }

        Task {
            do {
                let tab = try await tabManager.openTab(for: server)
                await MainActor.run {
                    if selectTerminalViewOnSuccess {
                        tabManager.sessionState.selectView(
                            viewTabConfig.effectiveView(for: .terminal),
                            for: server.id
                        )
                    }
                    selectedTabIdBinding.wrappedValue = tab.id
                }
            } catch {
                // No-op: user cancelled biometric auth or open failed.
            }
        }
    }

    package func openNewFileTab(selectFilesViewOnSuccess: Bool = false) {
        guard fileTabManager.canOpenNewTab(
            for: server.id,
            hasProAccess: storeManager.allowsProFeatures
        ) else {
            showingFileTabLimitAlert = true
            return
        }

        let sourceTab = selectedFileTab
        let seedPath = sourceTab.flatMap { fileBrowser.lastVisitedPath(for: $0) }
            ?? selectedTab.flatMap {
                tabManager.sessionState.paneState(for: $0.focusedPaneId)?.workingDirectory
            }
        let newTab = sourceTab.flatMap {
            fileTabManager.duplicateTab(
                $0,
                seedPath: seedPath,
                hasProAccess: storeManager.allowsProFeatures
            )
        } ?? fileTabManager.openTab(
            for: server,
            seedPath: seedPath,
            hasProAccess: storeManager.allowsProFeatures
        )

        guard let newTab else { return }
        fileBrowser.prepareNewTab(newTab, duplicating: sourceTab)

        if selectFilesViewOnSuccess {
            tabManager.sessionState.selectView(
                viewTabConfig.effectiveView(for: .files),
                for: server.id
            )
        }
    }

    package func selectPreviousTab() {
        guard let currentId = selectedTabId,
              let currentIndex = serverTabs.firstIndex(where: { $0.id == currentId }),
              currentIndex > 0 else { return }
        selectedTabIdBinding.wrappedValue = serverTabs[currentIndex - 1].id
    }

    package func selectNextTab() {
        guard let currentId = selectedTabId,
              let currentIndex = serverTabs.firstIndex(where: { $0.id == currentId }),
              currentIndex < serverTabs.count - 1 else { return }
        selectedTabIdBinding.wrappedValue = serverTabs[currentIndex + 1].id
    }

    package func selectPreviousFileTab() {
        fileTabManager.selectPreviousTab(for: server.id)
    }

    package func selectNextFileTab() {
        fileTabManager.selectNextTab(for: server.id)
    }

    private func baseFileTabTitle(for tab: RemoteFileTab) -> String {
        let candidatePath = fileBrowser.lastVisitedPath(for: tab)
            ?? tab.lastKnownPath
            ?? tab.seedPath

        guard let candidatePath else {
            return server.name.nonEmptyString ?? "/"
        }

        let normalizedPath = RemoteFilePath.normalize(candidatePath)
        guard normalizedPath != "/" else {
            return server.name.nonEmptyString ?? "/"
        }

        return RemoteFilePath.breadcrumbs(for: normalizedPath).last?.title ?? (server.name.nonEmptyString ?? "/")
    }

    package func displayedFileTabTitle(for tab: RemoteFileTab) -> String {
        let baseTitles = Dictionary(
            uniqueKeysWithValues: serverFileTabs.map { ($0.id, baseFileTabTitle(for: $0)) }
        )
        let titleCounts = Dictionary(grouping: baseTitles.values, by: { $0 }).mapValues(\.count)
        var seenCounts: [String: Int] = [:]
        var resolvedTitles: [UUID: String] = [:]

        for tab in serverFileTabs {
            let baseTitle = baseTitles[tab.id] ?? (server.name.nonEmptyString ?? "/")
            guard (titleCounts[baseTitle] ?? 0) > 1 else {
                resolvedTitles[tab.id] = baseTitle
                continue
            }

            seenCounts[baseTitle, default: 0] += 1
            resolvedTitles[tab.id] = "\(baseTitle) (\(seenCounts[baseTitle, default: 0]))"
        }

        return resolvedTitles[tab.id] ?? baseFileTabTitle(for: tab)
    }

    private func closeSelectedFileTab() {
        guard let selectedFileTab,
              let removedTab = fileTabManager.closeTab(selectedFileTab) else {
            return
        }
        fileBrowser.removeState(for: removedTab.id)
    }

    package func serverViewTabActions() -> ServerViewTabActions {
        ServerViewTabActions(
            openNew: handleNewTabCommand,
            closeSelected: {
                if selectedView == .files {
                    closeSelectedFileTab()
                } else if let selectedTab {
                    // Close the focused split pane first (with confirmation,
                    // since it terminates an SSH connection); only close the
                    // whole tab once it's the last remaining pane.
                    if selectedTab.paneCount > 1 {
                        requestCloseFocusedPane()
                    } else {
                        tabManager.closeTab(selectedTab)
                    }
                }
            },
            selectPrevious: {
                if selectedView == .files {
                    selectPreviousFileTab()
                } else {
                    selectPreviousTab()
                }
            },
            selectNext: {
                if selectedView == .files {
                    selectNextFileTab()
                } else {
                    selectNextTab()
                }
            },
            selectIndex: { index in
                if selectedView == .files {
                    selectFileTab(at: index)
                } else {
                    selectTab(at: index)
                }
            }
        )
    }

    private func selectTab(at index: Int) {
        guard serverTabs.indices.contains(index) else { return }
        selectedTabIdBinding.wrappedValue = serverTabs[index].id
    }

    private func selectFileTab(at index: Int) {
        guard serverFileTabs.indices.contains(index) else { return }
        fileTabManager.selectTab(serverFileTabs[index])
    }

    /// Ask before closing the focused pane (terminates its SSH connection),
    /// matching the in-pane close button's confirmation.
    package func requestCloseFocusedPane() {
        guard selectedTab != nil else { return }
        platformPrepareForPaneClose()
        showingPaneCloseConfirmation = true
    }

    package func closeFocusedPaneConfirmed() {
        guard let selectedTab else { return }
        tabManager.closePane(tab: selectedTab, paneId: selectedTab.focusedPaneId)
    }

}
