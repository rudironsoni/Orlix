import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package extension DiscoverySource {
    package var label: String {
        switch self {
        case .bonjour:
            return String(localized: "Bonjour")
        case .portScan:
            return String(localized: "Port Scan")
        }
    }
}
