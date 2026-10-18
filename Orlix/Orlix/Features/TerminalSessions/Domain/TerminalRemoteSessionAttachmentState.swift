import Foundation

nonisolated package struct TerminalRemoteSessionAttachmentState: Codable, Equatable, Sendable {
    package let attachment: RemoteSessionAttachment
    package var managedSessionConfirmed: Bool
}
