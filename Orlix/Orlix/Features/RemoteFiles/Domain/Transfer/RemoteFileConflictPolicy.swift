import Foundation

nonisolated package enum RemoteFileConflictPolicy: String, Sendable {
    case replaceExisting
    case keepBoth
}

nonisolated package struct RemoteFileConflictResolution: Equatable, Sendable {
    package let originalName: String
    package let resolvedName: String
    package let existingEntry: RemoteFileEntry?

    package var hasConflict: Bool {
        existingEntry != nil
    }
}
