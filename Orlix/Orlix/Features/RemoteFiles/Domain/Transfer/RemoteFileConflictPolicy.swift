import Foundation

nonisolated package enum RemoteFileConflictPolicy: String, Sendable {
    package case replaceExisting
    package case keepBoth
}

nonisolated package struct RemoteFileConflictResolution: Equatable, Sendable {
    package let originalName: String
    package let resolvedName: String
    package let existingEntry: RemoteFileEntry?

    package var hasConflict: Bool {
        existingEntry != nil
    }
}
