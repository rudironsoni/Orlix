import Combine
import Foundation
import os.log

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
@MainActor
final package class RemoteFileBrowserStore: ObservableObject {
    package typealias ServerProvider = @MainActor (UUID) -> Server?
    package typealias WorkingDirectoryProvider = @MainActor (UUID) -> String?

    package enum ToolbarCommandAction: Sendable {
        case upload(destinationPath: String)
        case createFolder(destinationPath: String)
    }

    package struct ToolbarCommand: Identifiable, Sendable {
        let id = UUID()
        let serverId: UUID
        let tabId: UUID
        let action: ToolbarCommandAction
    }

    package struct TransferProgress: Sendable {
        let completedUnitCount: Int
        let totalUnitCount: Int
        let currentItemName: String
    }

    package struct BrowserState: Sendable {
        let serverId: UUID
        var currentPath: String?
        var entries: [RemoteFileEntry]
        var sort: RemoteFileSort
        var sortDirection: RemoteFileSortDirection
        var showHiddenFiles: Bool
        var hasCustomizedHiddenFiles: Bool
        var directoryPhase: RemoteFileDirectoryPhase
        var viewerPhase: RemoteFileViewerPhase
        var isDirectoryTruncated: Bool
        var filesystemStatus: RemoteFileFilesystemStatus?

        init(serverId: UUID, persisted: RemoteFileBrowserPersistedState) {
            self.serverId = serverId
            currentPath = persisted.lastVisitedPath.map { RemoteFilePath.normalize($0) }
            entries = []
            sort = persisted.sort
            sortDirection = persisted.sortDirection
            showHiddenFiles = persisted.showHiddenFiles
            hasCustomizedHiddenFiles = persisted.hasCustomizedHiddenFiles
            directoryPhase = .notLoaded
            viewerPhase = .idle
            isDirectoryTruncated = false
            filesystemStatus = nil
        }

        var breadcrumbs: [RemoteFileBreadcrumb] {
            guard let currentPath else { return [] }
            return RemoteFilePath.breadcrumbs(for: currentPath)
        }

        var hasLoadedDirectory: Bool { directoryPhase.hasLoadedDirectory }
        var isLoadingDirectory: Bool { directoryPhase.isLoading }
        var isLoadingViewer: Bool { viewerPhase.isLoading }
        var error: RemoteFileBrowserError? { directoryPhase.error }
        var viewerError: RemoteFileBrowserError? { viewerPhase.error }
        var viewerPayload: RemoteFileViewerPayload? { viewerPhase.payload }
        var selectedEntryPath: String? { viewerPhase.selectedEntryPath }
    }

    package struct DirectorySnapshot: Sendable {
        let path: String
        let entries: [RemoteFileEntry]
        let isTruncated: Bool
        let filesystemStatus: RemoteFileFilesystemStatus?
    }

    @Published private(set) var states: [UUID: BrowserState] = [:]
    @Published package var pendingToolbarCommand: ToolbarCommand?

    package let defaults: UserDefaults
    package let persistenceKey = "remoteFileBrowserState.v2"
    package let legacyPersistenceKey = "remoteFileBrowserState.v1"
    package let logger = Logger(subsystem: Bundle.main.bundleIdentifier ?? "Orlix", category: "RemoteFiles")
    package let remoteFileServiceAdapter: SSHSFTPAdapter?
    package let temporaryStorage: RemoteFileTemporaryStorage
    package let previewLoader: RemoteFilePreviewLoader
    package let conflictResolver: RemoteFileConflictResolver
    package let securityApprovalActions: RemoteFileSecurityApprovalActions
    package let serverProvider: ServerProvider
    package let workingDirectoryProvider: WorkingDirectoryProvider

    package var persistedStates: [String: RemoteFileBrowserPersistedState] = [:]
    private var operationCoordinatorsByTabID: [UUID: RemoteFileOperationCoordinator] = [:]
    private var temporaryTransferURLsByTabID: [UUID: Set<URL>] = [:]
    private(set) var activeDragPayload: RemoteFileDragPayload?
    nonisolated static package let directoryEntryLimit = 2_000
    static package let defaultPreviewBytes = 512 * 1_024
    static package let hardPreviewBytes = 2 * 1_024 * 1_024
    static package let previewConfirmationBytes = 1 * 1_024 * 1_024
    static package let maxMediaPreviewBytes = 64 * 1_024 * 1_024

    package init(
        defaults: UserDefaults = .standard,
        remoteFileServiceAdapter: SSHSFTPAdapter? = nil,
        temporaryStorage: RemoteFileTemporaryStorage = RemoteFileTemporaryStorage(),
        previewLoader: RemoteFilePreviewLoader = RemoteFilePreviewLoader(),
        conflictResolver: RemoteFileConflictResolver = RemoteFileConflictResolver(),
        securityApprovalActions: RemoteFileSecurityApprovalActions = .unavailable,
        serverProvider: @escaping ServerProvider = { _ in nil },
        workingDirectoryProvider: @escaping WorkingDirectoryProvider = { _ in nil }
    ) {
        self.defaults = defaults
        self.remoteFileServiceAdapter = remoteFileServiceAdapter
        self.temporaryStorage = temporaryStorage
        self.previewLoader = previewLoader
        self.conflictResolver = conflictResolver
        self.securityApprovalActions = securityApprovalActions
        self.serverProvider = serverProvider
        self.workingDirectoryProvider = workingDirectoryProvider
        loadPersistedStates()
    }

    isolated deinit {
        operationCoordinatorsByTabID.values.forEach { $0.cancelAll() }
        temporaryTransferURLsByTabID.values
            .flatMap(Array.init)
            .forEach(temporaryStorage.removeItem)
    }

    package func prepareNewTab(_ tab: RemoteFileTab, duplicating sourceTab: RemoteFileTab?) {
        if let sourceTab {
            let sourceState = state(for: sourceTab)
            let sourcePersistedState = persistedState(for: sourceTab.id)
            let sourcePath = sourceState.currentPath
                ?? sourcePersistedState.lastVisitedPath
                ?? sourceTab.lastKnownPath
                ?? sourceTab.seedPath

            persistedStates[tab.id.uuidString] = RemoteFileBrowserPersistedState(
                lastVisitedPath: sourcePath,
                sort: sourceState.sort,
                sortDirection: sourceState.sortDirection,
                showHiddenFiles: sourceState.showHiddenFiles,
                hasCustomizedHiddenFiles: sourceState.hasCustomizedHiddenFiles
            )
            states[tab.id] = BrowserState(serverId: tab.serverId, persisted: persistedState(for: tab.id))
            persistStates()
            return
        }

        guard persistedStates[tab.id.uuidString] == nil else { return }
        persistedStates[tab.id.uuidString] = RemoteFileBrowserPersistedState(
            lastVisitedPath: tab.seedPath ?? tab.lastKnownPath
        )
        persistStates()
    }

    package func beginDrag(_ payload: RemoteFileDragPayload) {
        activeDragPayload = payload
    }

    package func endDrag() {
        activeDragPayload = nil
    }

    package func state(for tab: RemoteFileTab) -> BrowserState {
        states[tab.id] ?? BrowserState(serverId: tab.serverId, persisted: persistedState(for: tab.id))
    }

    package func currentPathValue(for tab: RemoteFileTab) -> String? {
        state(for: tab).currentPath
    }

    package func lastVisitedPath(for tab: RemoteFileTab) -> String? {
        state(for: tab).currentPath
            ?? persistedState(for: tab.id).lastVisitedPath
            ?? tab.lastKnownPath
            ?? tab.seedPath
    }

    package func currentPath(for tab: RemoteFileTab) -> String {
        lastVisitedPath(for: tab) ?? "/"
    }

    package func displayedEntries(for tab: RemoteFileTab) -> [RemoteFileEntry] {
        let state = state(for: tab)
        let visibleEntries = state.showHiddenFiles
            ? state.entries
            : state.entries.filter { !$0.isHidden }
        return visibleEntries.sortedForBrowser(using: state.sort, direction: state.sortDirection)
    }

    package func entries(for tab: RemoteFileTab) -> [RemoteFileEntry] {
        displayedEntries(for: tab)
    }

    package func selectedEntryPath(for tab: RemoteFileTab) -> String? {
        state(for: tab).selectedEntryPath
    }

    package func viewerPayload(for tab: RemoteFileTab) -> RemoteFileViewerPayload? {
        state(for: tab).viewerPayload
    }

    package func error(for tab: RemoteFileTab) -> RemoteFileBrowserError? {
        state(for: tab).error
    }

    package func viewerError(for tab: RemoteFileTab) -> RemoteFileBrowserError? {
        state(for: tab).viewerError
    }

    package func isLoading(for tab: RemoteFileTab) -> Bool {
        state(for: tab).isLoadingDirectory
    }

    package func isLoadingViewer(for tab: RemoteFileTab) -> Bool {
        state(for: tab).isLoadingViewer
    }

    package func isTruncated(for tab: RemoteFileTab) -> Bool {
        state(for: tab).isDirectoryTruncated
    }

    package func filesystemStatus(for tab: RemoteFileTab) -> RemoteFileFilesystemStatus? {
        state(for: tab).filesystemStatus
    }

    package func sort(for tab: RemoteFileTab) -> RemoteFileSort {
        state(for: tab).sort
    }

    package func sortDirection(for tab: RemoteFileTab) -> RemoteFileSortDirection {
        state(for: tab).sortDirection
    }

    package func showHiddenFiles(for tab: RemoteFileTab) -> Bool {
        state(for: tab).showHiddenFiles
    }

    package func breadcrumbs(for tab: RemoteFileTab) -> [RemoteFileBreadcrumb] {
        state(for: tab).breadcrumbs
    }

    package func loadInitialPath(for server: Server, tab: RemoteFileTab, initialPath: String? = nil) async {
        guard tab.serverId == server.id else { return }

        let currentState = state(for: tab)
        guard !currentState.isLoadingDirectory else { return }
        guard !currentState.hasLoadedDirectory else { return }

        let requestID = UUID()

        updateState(for: tab) { state in
            state.directoryPhase.begin(requestID: requestID)
        }

        do {
            let snapshot = try await resolveInitialDirectorySnapshot(for: server, tab: tab, initialPath: initialPath)
            applyDirectorySnapshot(snapshot, to: tab, requestID: requestID)
        } catch {
            guard failDirectoryRequest(requestID, for: tab, error: error) else { return }
            logger.error("Initial file browser load failed [server: \(server.name, privacy: .private(mask: .hash))] [error: \(LogPrivacy.errorClass(error), privacy: .public)]")
        }
    }

    package func refresh(server: Server, tab: RemoteFileTab) async {
        guard tab.serverId == server.id else { return }
        let targetPath = lastVisitedPath(for: tab)
            ?? bestWorkingDirectory(for: server.id)
            ?? "/"
        await loadDirectory(path: targetPath, in: tab, server: server)
    }

    package func openBreadcrumb(_ breadcrumb: RemoteFileBreadcrumb, in tab: RemoteFileTab, server: Server) async {
        guard tab.serverId == server.id else { return }
        await loadDirectory(path: breadcrumb.path, in: tab, server: server)
    }

    package func openDirectory(_ entry: RemoteFileEntry, in tab: RemoteFileTab, server: Server) async {
        guard tab.serverId == server.id else { return }
        await loadDirectory(path: entry.path, in: tab, server: server)
    }

    package func activate(_ entry: RemoteFileEntry, in tab: RemoteFileTab, server: Server) async {
        guard tab.serverId == server.id else { return }

        switch entry.type {
        case .directory:
            await openDirectory(entry, in: tab, server: server)
        case .symlink:
            do {
                let resolvedEntry = try await withRemoteFileService(for: server) { service in
                    try await service.stat(at: entry.path)
                }
                if resolvedEntry.type == .directory {
                    await loadDirectory(path: entry.path, in: tab, server: server)
                } else {
                    selectFile(entry, in: tab)
                }
            } catch {
                selectFile(entry, in: tab)
            }
        case .file, .other:
            selectFile(entry, in: tab)
        }
    }

    package func focus(_ entry: RemoteFileEntry, in tab: RemoteFileTab) {
        cleanupPreviewArtifact(for: state(for: tab).viewerPayload)
        updateState(for: tab) { state in
            state.viewerPhase.select(path: entry.path)
        }
    }

    package func updateSort(_ sort: RemoteFileSort, for tab: RemoteFileTab) {
        updateSort(sort, direction: sort.defaultDirection, for: tab)
    }

    package func updateSort(_ sort: RemoteFileSort, direction: RemoteFileSortDirection, for tab: RemoteFileTab) {
        updateState(for: tab) { state in
            state.sort = sort
            state.sortDirection = direction
        }
        persistState(for: tab.id)
    }

    package func setShowHiddenFiles(_ showHiddenFiles: Bool, for tab: RemoteFileTab) {
        updateState(for: tab) { state in
            state.showHiddenFiles = showHiddenFiles
            state.hasCustomizedHiddenFiles = true
        }
        persistState(for: tab.id)
    }

    package func removeState(for tabId: UUID) {
        removeRuntimeState(for: tabId)
        persistedStates.removeValue(forKey: tabId.uuidString)
        persistStates()
    }

    package func removeRuntimeState(for tabId: UUID) {
        operationCoordinatorsByTabID.removeValue(forKey: tabId)?.cancelAll()
        temporaryTransferURLsByTabID.removeValue(forKey: tabId)?
            .forEach(temporaryStorage.removeItem)
        temporaryStorage.removePreviewArtifact(for: states[tabId]?.viewerPayload)
        states.removeValue(forKey: tabId)

        if pendingToolbarCommand?.tabId == tabId {
            pendingToolbarCommand = nil
        }
    }

    package func disconnect(serverId: UUID) {
        let affectedTabIDs = Set(
            states.compactMap { tabId, state in
                state.serverId == serverId ? tabId : nil
            }
        )

        for tabId in affectedTabIDs {
            removeRuntimeState(for: tabId)
        }

        remoteFileServiceAdapter?.disconnect(serverId: serverId)
    }

    package func operationCoordinator(
        for tab: RemoteFileTab,
        server: Server
    ) -> RemoteFileOperationCoordinator {
        precondition(tab.serverId == server.id)
        if let coordinator = operationCoordinatorsByTabID[tab.id] {
            return coordinator
        }
        let coordinator = RemoteFileOperationCoordinator(
            server: server,
            securityApprovalActions: securityApprovalActions
        )
        operationCoordinatorsByTabID[tab.id] = coordinator
        return coordinator
    }

    package func makeTemporaryTransferFileURL(
        for entry: RemoteFileEntry,
        in tab: RemoteFileTab
    ) throws -> URL {
        let url = try temporaryStorage.makeTransferFileURL(for: entry)
        temporaryTransferURLsByTabID[tab.id, default: []].insert(url)
        return url
    }

    package func removeTemporaryTransferFile(at url: URL, in tab: RemoteFileTab) {
        temporaryTransferURLsByTabID[tab.id]?.remove(url)
        if temporaryTransferURLsByTabID[tab.id]?.isEmpty == true {
            temporaryTransferURLsByTabID[tab.id] = nil
        }
        temporaryStorage.removeItem(at: url)
    }

    package func prepareDragExport(for entry: RemoteFileEntry, server: Server) async throws -> URL {
        try await temporaryStorage.prepareDragExport(for: entry) { [weak self] temporaryURL in
            guard let self else { throw CancellationError() }
            try await downloadItem(entry, to: temporaryURL, server: server)
        }
    }

    package func goUp(in tab: RemoteFileTab, server: Server) async {
        guard tab.serverId == server.id else { return }
        let currentPath = currentPath(for: tab)
        let parentPath = RemoteFilePath.parent(of: currentPath)
        guard parentPath != currentPath else { return }
        await loadDirectory(path: parentPath, in: tab, server: server)
    }

    package func loadDirectory(path: String, in tab: RemoteFileTab, server: Server) async {
        guard tab.serverId == server.id else { return }

        let normalizedPath = RemoteFilePath.normalize(path)
        let requestID = UUID()
        cleanupPreviewArtifact(for: state(for: tab).viewerPayload)

        updateState(for: tab) { state in
            state.directoryPhase.begin(requestID: requestID)
            state.viewerPhase = .idle
        }

        do {
            let snapshot = try await directorySnapshot(path: normalizedPath, for: server)
            applyDirectorySnapshot(snapshot, to: tab, requestID: requestID)
        } catch {
            guard failDirectoryRequest(requestID, for: tab, error: error) else { return }
            logger.error("Directory load failed [path: \(normalizedPath, privacy: .private(mask: .hash))] [error: \(LogPrivacy.errorClass(error), privacy: .public)]")
        }
    }

    package func resolveInitialDirectorySnapshot(
        for server: Server,
        tab: RemoteFileTab,
        initialPath: String?
    ) async throws -> DirectorySnapshot {
        for path in initialDirectoryCandidates(for: server, tab: tab, initialPath: initialPath) {
            do {
                return try await directorySnapshot(path: path, for: server)
            } catch {
                logger.debug("Skipping initial browser path [path: \(path, privacy: .private(mask: .hash))] [error: \(LogPrivacy.errorClass(error), privacy: .public)]")
            }
        }

        let homePath = try await withRemoteFileService(for: server) { service in
            try await service.resolveHomeDirectory()
        }
        return try await directorySnapshot(path: homePath, for: server)
    }

    package func initialDirectoryCandidates(
        for server: Server,
        tab: RemoteFileTab,
        initialPath: String?
    ) -> [String] {
        let persistedPath = persistedState(for: tab.id).lastVisitedPath
        let workingDirectory = bestWorkingDirectory(for: server.id)
        var seenPaths = Set<String>()

        return [
            persistedPath,
            tab.lastKnownPath,
            initialPath,
            tab.seedPath,
            workingDirectory
        ]
        .compactMap { $0?.trimmingCharacters(in: .whitespacesAndNewlines) }
        .filter { !$0.isEmpty }
        .map { RemoteFilePath.normalize($0) }
        .filter { seenPaths.insert($0).inserted }
    }

    private func directorySnapshot(path: String, for server: Server) async throws -> DirectorySnapshot {
        let normalizedPath = RemoteFilePath.normalize(path)
        let listedEntries = try await withRemoteFileService(for: server) { service in
            try await service.listDirectory(at: normalizedPath, maxEntries: Self.directoryEntryLimit + 1)
        }
        let listing = Self.cappedDirectoryListing(listedEntries)
        let filesystemStatus = try? await withRemoteFileService(for: server) { service in
            try await service.fileSystemCapacity(at: normalizedPath).status
        }
        return DirectorySnapshot(
            path: normalizedPath,
            entries: listing.entries,
            isTruncated: listing.isTruncated,
            filesystemStatus: filesystemStatus
        )
    }

    static package func cappedDirectoryListing(
        _ entries: [RemoteFileEntry],
        limit: Int = directoryEntryLimit
    ) -> (entries: [RemoteFileEntry], isTruncated: Bool) {
        guard limit > 0 else {
            return ([], !entries.isEmpty)
        }

        return (Array(entries.prefix(limit)), entries.count > limit)
    }

    package func applyDirectorySnapshot(_ snapshot: DirectorySnapshot, to tab: RemoteFileTab, requestID: UUID) {
        var didApply = false
        guard updateExistingState(for: tab, mutation: { state in
            guard state.directoryPhase.complete(requestID: requestID) else { return }
            state.currentPath = snapshot.path
            state.entries = snapshot.entries
            state.isDirectoryTruncated = snapshot.isTruncated
            state.filesystemStatus = snapshot.filesystemStatus
            didApply = true
        }) else { return }
        if didApply {
            persistState(for: tab.id)
        }
    }

    @discardableResult
    private func failDirectoryRequest(_ requestID: UUID, for tab: RemoteFileTab, error: Error) -> Bool {
        var didFail = false
        guard updateExistingState(for: tab, mutation: { state in
            didFail = state.directoryPhase.fail(
                requestID: requestID,
                error: RemoteFileBrowserError.map(error)
            )
        }) else { return false }
        return didFail
    }

    package func selectFile(_ entry: RemoteFileEntry, in tab: RemoteFileTab) {
        focus(entry, in: tab)
    }

    package func withRemoteFileService<T: Sendable>(
        for server: Server,
        operation: @MainActor @escaping @Sendable (any RemoteFileService) async throws -> T
    ) async throws -> T {
        guard let remoteFileServiceAdapter else {
            throw RemoteFileBrowserError.disconnected
        }
        return try await remoteFileServiceAdapter.withService(for: server, operation: operation)
    }

    package func bestWorkingDirectory(for serverId: UUID) -> String? {
        workingDirectoryProvider(serverId)
    }

    package func updateState(for tab: RemoteFileTab, mutation: (inout BrowserState) -> Void) {
        updateState(for: tab.id, serverId: tab.serverId, mutation: mutation)
    }

    package func updateState(for tabId: UUID, serverId: UUID, mutation: (inout BrowserState) -> Void) {
        var state = states[tabId] ?? BrowserState(serverId: serverId, persisted: persistedState(for: tabId))
        mutation(&state)
        states[tabId] = state
    }

    @discardableResult
    package func updateExistingState(for tab: RemoteFileTab, mutation: (inout BrowserState) -> Void) -> Bool {
        guard var state = states[tab.id], state.serverId == tab.serverId else { return false }
        mutation(&state)
        states[tab.id] = state
        return true
    }

    package func server(for serverId: UUID) -> Server? {
        serverProvider(serverId)
    }

    package func setPendingToolbarCommand(_ command: ToolbarCommand?) {
        pendingToolbarCommand = command
    }
}

extension String {
    var nonEmptyString: String? {
        isEmpty ? nil : self
    }
}
