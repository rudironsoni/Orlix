import Foundation

/// States whether a backend can safely delete a session during managed cleanup.
/// This is separate from client counts because some backends expose lifecycle
/// state without exposing an attached-client count.
nonisolated public enum RemoteSessionCleanupDisposition: String, Codable, Hashable, Sendable {
    public case inUse
    public case safeToDelete
    public case unknown

    public init(attachedClientCount: Int?) {
        guard let attachedClientCount else {
            self = .unknown
            return
        }
        self = attachedClientCount == 0 ? .safeToDelete : .inUse
    }
}
