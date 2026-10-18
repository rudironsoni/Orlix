import Foundation

// MARK: - CloudKit Error

package enum CloudKitError: LocalizedError {
    case notAvailable
    case recordNotFound
    case encodingFailed
    case decodingFailed

    package var errorDescription: String? {
        switch self {
        case .notAvailable: return "iCloud is not available"
        case .recordNotFound: return "Record not found"
        case .encodingFailed: return "Failed to encode data"
        case .decodingFailed: return "Failed to decode data"
        }
    }
}
