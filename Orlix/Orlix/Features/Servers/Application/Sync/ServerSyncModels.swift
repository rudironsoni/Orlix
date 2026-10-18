import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package struct ServerRemoteChangeCheckpoint: Equatable, Sendable {
    package let id: UUID
}

nonisolated package struct ServerRemoteChanges: Equatable, Sendable {
    package let servers: [Server]
    package let workspaces: [Workspace]
    package let deletedServerIDs: [UUID]
    package let deletedWorkspaceIDs: [UUID]
    package let isFullFetch: Bool
    package let checkpoint: ServerRemoteChangeCheckpoint

    package init(
        servers: [Server],
        workspaces: [Workspace],
        deletedServerIDs: [UUID],
        deletedWorkspaceIDs: [UUID],
        isFullFetch: Bool,
        checkpoint: ServerRemoteChangeCheckpoint
    ) {
        self.servers = servers
        self.workspaces = workspaces
        self.deletedServerIDs = deletedServerIDs
        self.deletedWorkspaceIDs = deletedWorkspaceIDs
        self.isFullFetch = isFullFetch
        self.checkpoint = checkpoint
    }
}

nonisolated package enum AmbiguousCloudRecoveryChoice: Equatable, Sendable {
    case keepLocal
    case uploadLocal
    case replaceWithCloud
}

nonisolated package struct AmbiguousCloudRecoveryBackup: Codable, Equatable, Sendable {
    package let servers: [Server]
    package let workspaces: [Workspace]
}

nonisolated package enum AmbiguousCloudRecoveryState: Equatable, Sendable {
    case decisionRequired
}

nonisolated package enum AmbiguousCloudRecoveryError: LocalizedError, Equatable, Sendable {
    case fullFetchNeedsDecision
    case remoteUnavailable
    case incompleteSnapshot

    package var errorDescription: String? {
        switch self {
        case .fullFetchNeedsDecision:
            return String(
                localized: "Cloud data is missing local items. Choose how Orlix should recover."
            )
        case .remoteUnavailable:
            return String(localized: "iCloud is not available. Try again when sync is available.")
        case .incompleteSnapshot:
            return String(localized: "iCloud did not return a complete server snapshot.")
        }
    }
}

nonisolated package struct ServerPendingMutation: Codable, Equatable, Identifiable, Sendable {
    nonisolated package enum Payload: Codable, Equatable, Sendable {
        case serverUpsert(Server)
        case serverDelete(Server)
        case workspaceUpsert(Workspace)
        case workspaceDelete(Workspace)

        private enum Kind: String, Codable {
            case serverUpsert
            case serverDelete
            case workspaceUpsert
            case workspaceDelete
        }

        private enum CodingKeys: String, CodingKey {
            case kind
            case payload
        }

        package init(from decoder: Decoder) throws {
            let container = try decoder.container(keyedBy: CodingKeys.self)
            switch try container.decode(Kind.self, forKey: .kind) {
            case .serverUpsert:
                self = .serverUpsert(try container.decode(Server.self, forKey: .payload))
            case .serverDelete:
                self = .serverDelete(try container.decode(Server.self, forKey: .payload))
            case .workspaceUpsert:
                self = .workspaceUpsert(try container.decode(Workspace.self, forKey: .payload))
            case .workspaceDelete:
                self = .workspaceDelete(try container.decode(Workspace.self, forKey: .payload))
            }
        }

        package func encode(to encoder: Encoder) throws {
            var container = encoder.container(keyedBy: CodingKeys.self)
            switch self {
            case .serverUpsert(let server):
                try container.encode(Kind.serverUpsert, forKey: .kind)
                try container.encode(server, forKey: .payload)
            case .serverDelete(let server):
                try container.encode(Kind.serverDelete, forKey: .kind)
                try container.encode(server, forKey: .payload)
            case .workspaceUpsert(let workspace):
                try container.encode(Kind.workspaceUpsert, forKey: .kind)
                try container.encode(workspace, forKey: .payload)
            case .workspaceDelete(let workspace):
                try container.encode(Kind.workspaceDelete, forKey: .kind)
                try container.encode(workspace, forKey: .payload)
            }
        }
    }

    package let id: UUID
    package let payload: Payload
    package let createdAt: Date

    package init(id: UUID, payload: Payload, createdAt: Date) {
        self.id = id
        self.payload = payload
        self.createdAt = createdAt
    }
}
