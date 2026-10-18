import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
extension LocalSSHDiscoveryService: LocalSSHDiscovering {}

package extension LocalSSHDiscoveryDependencies {
    static package func live(
        networkConnectionType: @escaping () -> NetworkMonitor.ConnectionType,
        makeScanID: @escaping () -> UUID
    ) -> Self {
        LocalSSHDiscoveryDependencies(
            service: LocalSSHDiscoveryService(),
            networkAvailability: {
                networkConnectionType() == .cellular
                    ? .unsupported
                    : .supported
            },
            makeScanID: makeScanID
        )
    }
}
