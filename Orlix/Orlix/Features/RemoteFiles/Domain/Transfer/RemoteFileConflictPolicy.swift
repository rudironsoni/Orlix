import Foundation

nonisolated public enum RemoteFileConflictPolicy: String, Sendable {
    public case replaceExisting
    public case keepBoth
}

nonisolated package struct RemoteFileConflictResolution: Equatable, Sendable {
    package let originalName: String
    package let resolvedName: String
    package let existingEntry: RemoteFileEntry?

    package var hasConflict: Bool {
        existingEntry != nil
    }
}
