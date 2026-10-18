import Foundation

package struct RemoteSessionSelectionInfo: Identifiable, Equatable {
    package let attachment: RemoteSessionAttachment
    package let attachedClientCount: Int?
    package let containerCount: Int?

    package var id: RemoteSessionIdentifier { attachment.identifier }
    package var displayName: String { id.rawValue }
}

package struct RemoteSessionAttachPrompt: Identifiable, Equatable {
    /// Unique shell-start request that owns this prompt.
    package let id: UUID
    package let paneId: UUID
    package let backendName: String
    package let existingSessions: [RemoteSessionSelectionInfo]
}

package enum RemoteSessionAttachSelection: Equatable {
    package case createManaged
    package case attachExisting(RemoteSessionAttachment)
    package case plainShell
}
