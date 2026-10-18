import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package struct WorkspaceDeletionPlan: Codable, Equatable, Identifiable, Sendable {
    package let id: UUID
    package let workspace: Workspace
    package let deletedServers: [Server]
    package let previousServers: [Server]
    package let previousWorkspaces: [Workspace]
    package let remainingServers: [Server]
    package let remainingWorkspaces: [Workspace]
    package let pendingMutations: [ServerPendingMutation]

    package init?(
        workspaceID: UUID,
        servers: [Server],
        workspaces: [Workspace],
        id: UUID,
        mutationIDs: [UUID],
        mutationDate: Date
    ) {
        guard let workspace = workspaces.first(where: { $0.id == workspaceID }) else {
            return nil
        }

        let deletedServers = servers.filter { $0.workspaceId == workspaceID }
        guard let workspaceMutationID = mutationIDs.last,
              mutationIDs.dropLast().count == deletedServers.count else {
            return nil
        }
        self.id = id
        self.workspace = workspace
        self.deletedServers = deletedServers
        self.previousServers = servers
        self.previousWorkspaces = workspaces
        self.remainingServers = servers.filter { $0.workspaceId != workspaceID }
        self.remainingWorkspaces = workspaces.filter { $0.id != workspaceID }
        self.pendingMutations = deletedServers.enumerated().map { index, server in
            ServerPendingMutation(
                id: mutationIDs[index],
                payload: .serverDelete(server),
                createdAt: mutationDate.addingTimeInterval(TimeInterval(index))
            )
        } + [
            ServerPendingMutation(
                id: workspaceMutationID,
                payload: .workspaceDelete(workspace),
                createdAt: mutationDate.addingTimeInterval(TimeInterval(deletedServers.count))
            )
        ]
    }

    package func hasSameDeletionSnapshot(as other: WorkspaceDeletionPlan) -> Bool {
        workspace == other.workspace &&
        deletedServers == other.deletedServers &&
        remainingServers == other.remainingServers &&
        remainingWorkspaces == other.remainingWorkspaces
    }
}
