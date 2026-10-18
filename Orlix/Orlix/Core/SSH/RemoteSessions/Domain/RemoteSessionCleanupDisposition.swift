import Foundation

/// States whether a backend can safely delete a session during managed cleanup.
/// This is separate from client counts because some backends expose lifecycle
/// state without exposing an attached-client count.
nonisolated package enum RemoteSessionCleanupDisposition: String, Codable, Hashable, Sendable {
    case inUse
    case safeToDelete
    case unknown

    package init(attachedClientCount: Int?) {
        guard let attachedClientCount else {
            self = .unknown
            return
        }
        self = attachedClientCount == 0 ? .safeToDelete : .inUse
    }
}
