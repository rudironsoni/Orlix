import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
@MainActor
package protocol ServerRemoteRepository: AnyObject {
    var isAvailable: Bool { get }
    func fetchServerChanges(forceFullFetch: Bool) async throws -> ServerRemoteChanges
    func acceptServerChanges(_ checkpoint: ServerRemoteChangeCheckpoint) throws
    func saveServer(_ server: Server) async throws
    func saveWorkspace(_ workspace: Workspace) async throws
}

@MainActor
package protocol ServerRemoteMutationClient: AnyObject {
    func saveServer(_ server: Server) async throws
    func deleteServer(_ server: Server) async throws
    func saveWorkspace(_ workspace: Workspace) async throws
    func deleteWorkspace(_ workspace: Workspace) async throws
}

@MainActor
package protocol ServerSyncRepository:
    ServerDataMutationEnqueuing,
    AnyObject {
    func pendingServerMutations() throws -> [ServerPendingMutation]
    func clearPendingServerAndWorkspaceMutations() throws
    func removePendingServerMutation(_ mutationID: UUID) throws
    func drainPendingMutations() async
}

@MainActor
package protocol ServerManagerCredentialRepository:
    ServerCredentialTransactionRepository,
    ServerMutationCredentialTransacting,
    AnyObject {
    func deleteCredentials(for serverId: UUID) throws
}

nonisolated package enum ServerProtectedAction: Equatable, Sendable {
    case delete
}

@MainActor
package protocol ProtectedServerActionAuthorizing: AnyObject {
    func authorize(
        _ server: Server,
        for action: ServerProtectedAction
    ) async -> Bool
}

@MainActor
package protocol ServerKnownHostRepository: AnyObject {
    func remove(host: String, port: Int)
}

@MainActor
package protocol FreePlanAssignmentTracking: AnyObject {
    func trackFreePlanGenerationAssigned(
        generation: String,
        serverCount: Int,
        reason: String
    )
}

@MainActor
package struct ServerManagerDependencies {
    package let stateStore: ServerStateStore
    package let remoteSyncCoordinator: ServerRemoteSyncCoordinator
    package let syncRepository: any ServerSyncRepository
    package let credentialRepository: any ServerManagerCredentialRepository
    package let actionAuthorizer: any ProtectedServerActionAuthorizing
    package let didDeleteServerLocalData: (UUID) -> Void
    package let now: () -> Date
    package let makeID: () -> UUID

    package init(
        stateStore: ServerStateStore,
        remoteRepository: any ServerRemoteRepository,
        syncRepository: any ServerSyncRepository,
        credentialRepository: any ServerManagerCredentialRepository,
        actionAuthorizer: any ProtectedServerActionAuthorizing,
        knownHosts: any ServerKnownHostRepository,
        didDeleteServerLocalData: @escaping (UUID) -> Void,
        isRemoteSchemaError: @escaping (Error) -> Bool,
        now: @escaping () -> Date,
        makeID: @escaping () -> UUID
    ) {
        self.stateStore = stateStore
        self.syncRepository = syncRepository
        self.credentialRepository = credentialRepository
        self.actionAuthorizer = actionAuthorizer
        self.didDeleteServerLocalData = didDeleteServerLocalData
        self.now = now
        self.makeID = makeID
        remoteSyncCoordinator = ServerRemoteSyncCoordinator(
            dependencies: ServerRemoteSyncCoordinatorDependencies(
                stateStore: stateStore,
                remoteRepository: remoteRepository,
                syncRepository: syncRepository,
                credentialRepository: credentialRepository,
                knownHosts: knownHosts,
                didDeleteServerLocalData: didDeleteServerLocalData,
                isRemoteSchemaError: isRemoteSchemaError,
                now: now,
                makeID: makeID
            )
        )
    }
}
