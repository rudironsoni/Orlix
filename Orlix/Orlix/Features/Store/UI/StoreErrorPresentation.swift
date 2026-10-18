import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
extension StoreError: LocalizedError {
    nonisolated package var errorDescription: String? {
        switch self {
        case .verificationFailed:
            return String(localized: "Purchase verification failed")
        case .productNotFound:
            return String(localized: "Product not found")
        case .purchaseFailed(let message):
            return String(format: String(localized: "Purchase failed: %@"), message)
        }
    }
}
