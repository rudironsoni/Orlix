import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package extension DockerAvailability {
    package var message: String {
        switch self {
        case .unknown:
            return String(localized: "Waiting for Docker")
        case .available:
            return ""
        case .commandMissing:
            return String(localized: "Docker command not found")
        case .daemonUnavailable(let message),
             .permissionDenied(let message),
             .unavailable(let message):
            return message
        }
    }
}
