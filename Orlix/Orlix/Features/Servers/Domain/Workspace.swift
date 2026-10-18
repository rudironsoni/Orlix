import Foundation

// MARK: - Workspace Model

nonisolated package struct Workspace: Identifiable, Codable, Hashable, Sendable {
    package let id: UUID
    package var name: String
    package var colorHex: String
    package var icon: String?
    package var order: Int
    package var environments: [ServerEnvironment]
    package var lastSelectedEnvironmentId: UUID?
    package var lastSelectedServerId: UUID?
    package var createdAt: Date
    package var updatedAt: Date

    package init(
        id: UUID = UUID(),
        name: String,
        colorHex: String = "#007AFF",
        icon: String? = nil,
        order: Int = 0,
        environments: [ServerEnvironment] = ServerEnvironment.builtInEnvironments,
        lastSelectedEnvironmentId: UUID? = nil,
        lastSelectedServerId: UUID? = nil,
        createdAt: Date = Date(),
        updatedAt: Date = Date()
    ) {
        self.id = id
        self.name = name
        self.colorHex = colorHex
        self.icon = icon
        self.order = order
        self.environments = environments
        self.lastSelectedEnvironmentId = lastSelectedEnvironmentId
        self.lastSelectedServerId = lastSelectedServerId
        self.createdAt = createdAt
        self.updatedAt = updatedAt
    }

    package func environment(withId id: UUID) -> ServerEnvironment? {
        environments.first { $0.id == id }
    }

    package func containsEnvironment(_ candidate: ServerEnvironment) -> Bool {
        environment(withId: candidate.id) != nil
    }

}
