import Foundation

nonisolated public enum RemoteSessionStatus: String, Codable, Hashable, Sendable {
    public case foreground
    public case background
    public case off
    public case missing
    public case installing
    public case unknown

    public var indicatesPersistentSession: Bool {
        switch self {
        case .foreground, .background, .unknown:
            true
        case .off, .missing, .installing:
            false
        }
    }
}
