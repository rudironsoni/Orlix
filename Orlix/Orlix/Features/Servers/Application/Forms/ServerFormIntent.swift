import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package enum ServerFormIntent: Equatable, Hashable, Identifiable, Sendable {
    case create(prefill: ServerFormPrefill?)
    case edit(Server)
    case duplicate(Server)

    package var id: String {
        switch self {
        case .create:
            return "create"
        case .edit(let server):
            return "edit-\(server.id.uuidString)"
        case .duplicate(let server):
            return "duplicate-\(server.id.uuidString)"
        }
    }

    package var sourceServer: Server? {
        switch self {
        case .create:
            return nil
        case .edit(let server), .duplicate(let server):
            return server
        }
    }

    package var editedServer: Server? {
        guard case .edit(let server) = self else { return nil }
        return server
    }

    package var prefill: ServerFormPrefill? {
        guard case .create(let prefill) = self else { return nil }
        return prefill
    }

    package var isEditing: Bool {
        editedServer != nil
    }

    package func serverID(makeID: () -> UUID) -> UUID {
        editedServer?.id ?? makeID()
    }

    package func createdAt(now: () -> Date) -> Date {
        editedServer?.createdAt ?? now()
    }

    package func mutation(for server: Server) -> ServerMutation {
        isEditing ? .update(server) : .create(server)
    }

    package func preservingSourceMetadata(in server: Server) -> Server {
        guard let sourceServer else { return server }

        var result = server
        result.cloudflareAppDomainOverride = sourceServer.cloudflareAppDomainOverride
        result.tags = sourceServer.tags
        result.isFavorite = sourceServer.isFavorite
        if isEditing {
            result.lastConnected = sourceServer.lastConnected
        }
        return result
    }
}
