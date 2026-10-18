import Foundation

nonisolated package enum RemoteFileType: String, Codable, CaseIterable, Sendable {
    package case file
    package case directory
    package case symlink
    package case other

    package var displayName: String {
        switch self {
        case .file:
            return String(localized: "File")
        case .directory:
            return String(localized: "Directory")
        case .symlink:
            return String(localized: "Symlink")
        case .other:
            return String(localized: "Other")
        }
    }
}
