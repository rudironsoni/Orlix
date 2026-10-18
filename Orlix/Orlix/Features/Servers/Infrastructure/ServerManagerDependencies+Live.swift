import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
import OrlixSSH
import OrlixStats
#endif
extension CloudKitSyncCoordinator: ServerSyncRepository {
    package func clearPendingServerAndWorkspaceMutations() throws {
        try removeAll { ServerPendingCloudKitPayloadCodec.contains($0.payload) }
    }

    package func enqueueServerUpsert(_ server: Server) throws {
        try enqueue(.serverUpsert(server))
    }

    package func enqueueServerDelete(_ server: Server) throws {
        try enqueue(.serverDelete(server))
    }

    package func enqueueServerDataMutations(_ mutations: [ServerPendingMutation]) throws {
        try enqueueAtomically(try mutations.map(PendingCloudKitMutation.init))
    }

    package func enqueueWorkspaceUpsert(_ workspace: Workspace) throws {
        try enqueue(.workspaceUpsert(workspace))
    }

    package func enqueueWorkspaceDelete(_ workspace: Workspace) throws {
        try enqueue(.workspaceDelete(workspace))
    }

    package func pendingServerMutations() throws -> [ServerPendingMutation] {
        try readySnapshot().compactMap(ServerPendingMutation.init)
    }

    package func removePendingServerMutation(_ mutationID: UUID) throws {
        try remove(mutationID)
    }

}

extension KeychainManager: ServerManagerCredentialRepository {
    package func cleanupCredentials(for server: Server) throws {
        try deleteCredentials(for: server.id)
        guard try !hasCredentials(for: server) else {
            throw ServerCredentialCleanupError.incomplete
        }
    }
}
extension AppLockManager: ProtectedServerActionAuthorizing {
    package func authorize(_ server: Server, for action: ServerProtectedAction) async -> Bool {
        switch action {
        case .delete:
            return await authorizeProtectedServerAction(server, action: .delete)
        }
    }
}
extension KnownHostsManager: ServerKnownHostRepository {}
extension AnalyticsTracker: FreePlanAssignmentTracking {}

package extension ServerManagerDependencies {
    static package func live(
        defaults: UserDefaults,
        serverCloud: ServerCloudKitClient,
        credentialRepository: any ServerManagerCredentialRepository,
        knownHosts: any ServerKnownHostRepository,
        freePlanTracker: any FreePlanAssignmentTracking,
        actionAuthorizer: any ProtectedServerActionAuthorizing,
        syncRepository: any ServerSyncRepository,
        didDeleteServerLocalData: @escaping (UUID) -> Void,
        defaultWorkspaceName: @escaping () -> String,
        canonicalDefaultWorkspaceNames: @escaping () -> Set<String>,
        now: @escaping () -> Date,
        makeID: @escaping () -> UUID
    ) -> Self {
        let stateStore = ServerStateStore(
            dependencies: ServerStateStoreDependencies(
                localRepository: ServerLocalStore(defaults: defaults),
                preferences: ServerManagerUserDefaultsPreferences(defaults: defaults),
                freePlanTracker: freePlanTracker,
                isSyncEnabled: { SyncSettings.isEnabled(in: defaults) },
                now: now,
                makeID: makeID,
                defaultWorkspaceName: defaultWorkspaceName,
                canonicalDefaultWorkspaceNames: canonicalDefaultWorkspaceNames
            )
        )
        return Self(
            stateStore: stateStore,
            remoteRepository: serverCloud,
            syncRepository: syncRepository,
            credentialRepository: credentialRepository,
            actionAuthorizer: actionAuthorizer,
            knownHosts: knownHosts,
            didDeleteServerLocalData: didDeleteServerLocalData,
            isRemoteSchemaError: ServerCloudKitClient.isSchemaError,
            now: now,
            makeID: makeID
        )
    }
}

private extension ServerPendingMutation {
    package init?(_ mutation: PendingCloudKitMutation) {
        guard let payload = try? ServerPendingCloudKitPayloadCodec.decode(mutation.payload) else {
            return nil
        }
        self.init(id: mutation.id, payload: payload, createdAt: mutation.createdAt)
    }
}

private extension PendingCloudKitMutation {
    package init(_ mutation: ServerPendingMutation) throws {
        self.init(
            id: mutation.id,
            payload: try ServerPendingCloudKitPayloadCodec.encode(mutation.payload),
            createdAt: mutation.createdAt
        )
    }
}

private enum ServerCredentialCleanupError: LocalizedError {
    case incomplete

    package var errorDescription: String? {
        "The server credentials are still present after cleanup."
    }
}
