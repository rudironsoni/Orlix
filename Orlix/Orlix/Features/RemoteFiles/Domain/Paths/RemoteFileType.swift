import Foundation

nonisolated public enum RemoteFileType: String, Codable, CaseIterable, Sendable {
    public case file
    public case directory
    public case symlink
    public case other

    public var displayName: String {
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
