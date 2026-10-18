import Foundation

nonisolated package struct ServerFormPrefill: Equatable, Hashable, Sendable {
    package var name: String
    package var host: String
    package var port: Int
    package var username: String?

    package init(
        name: String,
        host: String,
        port: Int = 22,
        username: String? = nil
    ) {
        self.name = name
        self.host = host
        self.port = port
        self.username = username
    }
}
