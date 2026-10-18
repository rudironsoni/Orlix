import Foundation

nonisolated package struct RemoteFileDragPayload: Codable, Equatable, Sendable {
    package let serverId: UUID
    package let entries: [RemoteFileEntry]

    package init(serverId: UUID, entry: RemoteFileEntry) {
        self.init(serverId: serverId, entries: [entry])
    }

    package init(serverId: UUID, entries: [RemoteFileEntry]) {
        self.serverId = serverId
        self.entries = entries
    }
}
