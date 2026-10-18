import Foundation

nonisolated package struct TerminalFont: Identifiable, Codable, Equatable, Sendable {
    package typealias ID = String

    package enum FileFormat: String, CaseIterable, Sendable {
        case ttf
        case otf
        case ttc
        case otc

        package init?(fileExtension: String) {
            self.init(rawValue: fileExtension.lowercased())
        }
    }

    package let familyNames: [String]
    package let originalFilename: String
    package let fileSize: Int64
    package let sha256: String
    package var updatedAt: Date
    package var deletedAt: Date?

    package init(
        familyNames: [String],
        originalFilename: String,
        fileSize: Int64,
        sha256: String,
        updatedAt: Date = Date(),
        deletedAt: Date? = nil
    ) {
        self.familyNames = familyNames
        self.originalFilename = originalFilename
        self.fileSize = fileSize
        self.sha256 = sha256.lowercased()
        self.updatedAt = updatedAt
        self.deletedAt = deletedAt
    }

    package var displayName: String {
        familyNames.first ?? originalFilename
    }

    package var id: String {
        sha256
    }

    package var isDeleted: Bool {
        deletedAt != nil
    }
}
