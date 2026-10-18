import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated public enum ServerFormIntent: Equatable, Hashable, Identifiable, Sendable {
    public case create(prefill: ServerFormPrefill?)
    public case edit(Server)
    public case duplicate(Server)

    public var id: String {
        switch self {
        case .create:
            return "create"
        case .edit(let server):
            return "edit-\(server.id.uuidString)"
        case .duplicate(let server):
            return "duplicate-\(server.id.uuidString)"
        }
    }

    public var sourceServer: Server? {
        switch self {
        case .create:
            return nil
        case .edit(let server), .duplicate(let server):
            return server
        }
    }

    public var editedServer: Server? {
        guard case .edit(let server) = self else { return nil }
        return server
    }

    public var prefill: ServerFormPrefill? {
        guard case .create(let prefill) = self else { return nil }
        return prefill
    }

    public var isEditing: Bool {
        editedServer != nil
    }

    public func serverID(makeID: () -> UUID) -> UUID {
        editedServer?.id ?? makeID()
    }

    public func createdAt(now: () -> Date) -> Date {
        editedServer?.createdAt ?? now()
    }

    public func mutation(for server: Server) -> ServerMutation {
        isEditing ? .update(server) : .create(server)
    }

    public func preservingSourceMetadata(in server: Server) -> Server {
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
