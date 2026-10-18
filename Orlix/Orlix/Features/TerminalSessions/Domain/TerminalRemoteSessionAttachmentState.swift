import Foundation

nonisolated package struct TerminalRemoteSessionAttachmentState: Codable, Equatable, Sendable {
    package let attachment: RemoteSessionAttachment
    package var managedSessionConfirmed: Bool


    package init(

        attachment: RemoteSessionAttachment,

        managedSessionConfirmed: Bool

    ) {

        self.attachment = attachment

        self.managedSessionConfirmed = managedSessionConfirmed

    }
}
