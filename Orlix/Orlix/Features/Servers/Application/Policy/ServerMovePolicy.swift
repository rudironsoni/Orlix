import Foundation

nonisolated package enum ServerMoveRestriction: Equatable, Sendable {
    case lockedWorkspace
    case unavailable
}

nonisolated package struct ServerMovePolicy: Sendable {
    package let workspaces: [Workspace]
    package let unlockedWorkspaceIDs: Set<UUID>
    package let hasProAccess: Bool

    private var orderedWorkspaces: [Workspace] {
        workspaces.sorted { $0.order < $1.order }
    }

    package func assignmentWorkspaces(for server: Server?) -> [Workspace] {
        if hasProAccess {
            return orderedWorkspaces
        }

        guard let server,
              orderedWorkspaces.contains(where: { $0.id == server.workspaceId }) else {
            return orderedWorkspaces.filter { unlockedWorkspaceIDs.contains($0.id) }
        }

        let destinationIDs = moveDestinationIDs(for: server)
        return orderedWorkspaces.filter {
            $0.id == server.workspaceId || destinationIDs.contains($0.id)
        }
    }

    package func moveDestinations(for server: Server) -> [Workspace] {
        let destinationIDs = moveDestinationIDs(for: server)
        return orderedWorkspaces.filter { destinationIDs.contains($0.id) }
    }

    package func canAssign(_ server: Server, to destination: Workspace) -> Bool {
        server.workspaceId == destination.id
            || moveDestinationIDs(for: server).contains(destination.id)
    }

    package func restriction(
        for server: Server,
        destination: Workspace
    ) -> ServerMoveRestriction? {
        guard server.workspaceId != destination.id else { return nil }
        guard !moveDestinationIDs(for: server).contains(destination.id) else { return nil }
        if !hasProAccess && !unlockedWorkspaceIDs.contains(destination.id) {
            return .lockedWorkspace
        }
        return .unavailable
    }

    private func moveDestinationIDs(for server: Server) -> Set<UUID> {
        ServerMoveSupport.allowedDestinationIDs(
            isPro: hasProAccess,
            sourceWorkspaceId: server.workspaceId,
            workspacesInOrder: orderedWorkspaces,
            unlockedWorkspaceIds: unlockedWorkspaceIDs
        )
    }
}
