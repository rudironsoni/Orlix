import Foundation

package struct RemoteSessionSelectionInfo: Identifiable, Equatable {
    package let attachment: RemoteSessionAttachment
    package let attachedClientCount: Int?
    package let containerCount: Int?

    package var id: RemoteSessionIdentifier { attachment.identifier }
    package var displayName: String { id.rawValue }


    package init(

        attachment: RemoteSessionAttachment,

        attachedClientCount: Int? = nil,

        containerCount: Int? = nil

    ) {

        self.attachment = attachment

        self.attachedClientCount = attachedClientCount

        self.containerCount = containerCount

    }
}

package struct RemoteSessionAttachPrompt: Identifiable, Equatable {
    /// Unique shell-start request that owns this prompt.
    package let id: UUID
    package let paneId: UUID
    package let backendName: String
    package let existingSessions: [RemoteSessionSelectionInfo]


    package init(

        id: UUID,

        paneId: UUID,

        backendName: String,

        existingSessions: [RemoteSessionSelectionInfo]

    ) {

        self.id = id

        self.paneId = paneId

        self.backendName = backendName

        self.existingSessions = existingSessions

    }
}

package enum RemoteSessionAttachSelection: Equatable {
    case createManaged
    case attachExisting(RemoteSessionAttachment)
    case plainShell
}
