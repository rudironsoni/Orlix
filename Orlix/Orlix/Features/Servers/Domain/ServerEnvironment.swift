import Foundation

nonisolated package struct ServerEnvironment: Identifiable, Codable, Hashable, Sendable {
    package let id: UUID
    package var name: String
    package var shortName: String
    package var colorHex: String
    package var isBuiltIn: Bool

    package init(
        id: UUID = UUID(),
        name: String,
        shortName: String,
        colorHex: String,
        isBuiltIn: Bool = false
    ) {
        self.id = id
        self.name = name
        self.shortName = shortName
        self.colorHex = colorHex
        self.isBuiltIn = isBuiltIn
    }

    static package let production = ServerEnvironment(
        id: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!,
        name: "Production",
        shortName: "Prod",
        colorHex: "#34C759",
        isBuiltIn: true
    )

    static package let staging = ServerEnvironment(
        id: UUID(uuidString: "00000000-0000-0000-0000-000000000002")!,
        name: "Staging",
        shortName: "Stag",
        colorHex: "#FF9500",
        isBuiltIn: true
    )

    static package let development = ServerEnvironment(
        id: UUID(uuidString: "00000000-0000-0000-0000-000000000003")!,
        name: "Development",
        shortName: "Dev",
        colorHex: "#007AFF",
        isBuiltIn: true
    )

    static package let builtInEnvironments: [ServerEnvironment] = [.production, .staging, .development]
}
