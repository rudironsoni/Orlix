import Foundation

nonisolated package enum RemoteSessionStatus: String, Codable, Hashable, Sendable {
    package case foreground
    package case background
    package case off
    package case missing
    package case installing
    package case unknown

    package var indicatesPersistentSession: Bool {
        switch self {
        case .foreground, .background, .unknown:
            true
        case .off, .missing, .installing:
            false
        }
    }
}
