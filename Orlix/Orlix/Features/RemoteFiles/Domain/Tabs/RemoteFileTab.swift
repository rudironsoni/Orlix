import Foundation

nonisolated package struct RemoteFileTab: Identifiable, Codable, Equatable, Sendable {
    package let id: UUID
    package let serverId: UUID
    package let createdAt: Date
    package var seedPath: String?
    package var lastKnownPath: String?

    package init(
        id: UUID = UUID(),
        serverId: UUID,
        createdAt: Date = Date(),
        seedPath: String? = nil,
        lastKnownPath: String? = nil
    ) {
        self.id = id
        self.serverId = serverId
        self.createdAt = createdAt
        self.seedPath = Self.normalizeOptionalPath(seedPath)
        self.lastKnownPath = Self.normalizeOptionalPath(lastKnownPath)
    }

    mutating package func updateLastKnownPath(_ path: String?) {
        let normalized = Self.normalizeOptionalPath(path)
        guard let normalized else { return }
        lastKnownPath = normalized
    }

    private static func normalizeOptionalPath(_ path: String?) -> String? {
        guard let path else { return nil }
        let trimmed = path.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return nil }
        return RemoteFilePath.normalize(trimmed)
    }
}
