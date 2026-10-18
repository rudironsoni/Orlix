import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package struct ServerDataLoadState: Equatable, Sendable {
    nonisolated package enum Phase: Equatable, Sendable {
        case idle
        case loading(operationID: UUID)
        case failed(message: String)
    }

    private(set) var phase: Phase = .idle

    package var isLoading: Bool {
        if case .loading = phase {
            return true
        }
        return false
    }

    package var errorMessage: String? {
        if case .failed(let message) = phase {
            return message
        }
        return nil
    }

    mutating package func start(operationID: UUID) -> UUID {
        phase = .loading(operationID: operationID)
        return operationID
    }

    @discardableResult
    mutating package func finish(operationID: UUID) -> Bool {
        guard case .loading(operationID) = phase else { return false }
        phase = .idle
        return true
    }

    @discardableResult
    mutating package func fail(operationID: UUID, message: String) -> Bool {
        guard case .loading(operationID) = phase else { return false }
        phase = .failed(message: message)
        return true
    }

    mutating package func reset() {
        phase = .idle
    }
}

nonisolated package struct ServerManagerSnapshot: Equatable, Sendable {
    package var servers: [Server]
    package var workspaces: [Workspace]
    package var loadState: ServerDataLoadState
    package var localStorageIssues: [ServerLocalStorageIssue]
    package var ambiguousCloudRecovery: AmbiguousCloudRecoveryState?
    package var freePlanGeneration: FreePlanGeneration

    package init(
        servers: [Server] = [],
        workspaces: [Workspace] = [],
        loadState: ServerDataLoadState = ServerDataLoadState(),
        localStorageIssues: [ServerLocalStorageIssue] = [],
        ambiguousCloudRecovery: AmbiguousCloudRecoveryState? = nil,
        freePlanGeneration: FreePlanGeneration
    ) {
        self.servers = servers
        self.workspaces = workspaces
        self.loadState = loadState
        self.localStorageIssues = localStorageIssues
        self.ambiguousCloudRecovery = ambiguousCloudRecovery
        self.freePlanGeneration = freePlanGeneration
    }
}
