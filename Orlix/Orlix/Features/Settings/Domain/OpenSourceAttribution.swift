import Foundation

nonisolated package struct OpenSourceAttribution: Decodable, Equatable, Identifiable, Sendable {
    package enum Role: String, Decodable, Sendable {
        case terminalEngine
        case networkTransport
        case securityLibrary
        case archiveLibrary
        case machineLearningLibrary
        case utilityLibrary
        case sessionProtocol
        case speechModel
        case fontCollection
        case themeCollection
        case artwork
    }

    package struct Document: Equatable, Identifiable, Sendable {
        let attribution: OpenSourceAttribution
        let legalText: String

        var id: String { attribution.id }
    }

    package let id: String
    package let name: String
    package let role: Role
    package let projectURL: URL
    package let licenseName: String
    package let licenseResource: String
}
