import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package extension RemoteSessionStatus {
    package func shortLabel(backendName: String) -> String {
        switch self {
        case .foreground: return backendName
        case .background: return backendName
        case .off: return "off"
        case .missing: return "\(backendName) missing"
        case .installing: return "\(backendName) install"
        case .unknown: return backendName
        }
    }

    package var displayName: String {
        switch self {
        case .foreground: return "Foreground"
        case .background: return "Background"
        case .off: return "Off"
        case .missing: return "Unavailable"
        case .installing: return "Installing"
        case .unknown: return "Unknown"
        }
    }
}
