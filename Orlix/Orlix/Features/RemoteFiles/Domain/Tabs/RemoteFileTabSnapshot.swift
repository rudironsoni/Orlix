import Foundation

nonisolated package struct RemoteFileTabSnapshot: Codable, Equatable, Sendable {
    package var tabsByServer: [String: [RemoteFileTab]]
    package var selectedTabByServer: [String: UUID]
    package var schemaVersion: Int

    package init(
        tabsByServer: [String: [RemoteFileTab]] = [:],
        selectedTabByServer: [String: UUID] = [:],
        schemaVersion: Int = Self.currentSchemaVersion
    ) {
        self.tabsByServer = tabsByServer
        self.selectedTabByServer = selectedTabByServer
        self.schemaVersion = schemaVersion
    }

    static package let currentSchemaVersion = 1
}
