import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package enum LocalSSHDiscoverySourceStatus: Sendable {
    case bonjourStarted
    case bonjourFinished
    case probeStarted
    case probeFinished
}

nonisolated package enum LocalSSHDiscoveryEvent: Sendable {
    case scanningStarted
    case sourceStatus(LocalSSHDiscoverySourceStatus)
    case hostFound(DiscoveredSSHHost)
    case permissionDenied
    case scanningFinished
}

nonisolated package enum LocalSSHDiscoveryNetworkAvailability: Equatable, Sendable {
    case supported
    case unsupported
}

nonisolated package struct LocalSSHDiscoveryStopRequest: Sendable {
    private let stop: @MainActor @Sendable () -> Void

    package init(stop: @escaping @MainActor @Sendable () -> Void) {
        self.stop = stop
    }

    package func perform() {
        Task { @MainActor in
            stop()
        }
    }
}

@MainActor
package protocol LocalSSHDiscovering: AnyObject {
    var ownerReleaseStopRequest: LocalSSHDiscoveryStopRequest { get }

    func startScan() -> AsyncStream<LocalSSHDiscoveryEvent>
    func stopScan()
}

@MainActor
package struct LocalSSHDiscoveryDependencies {
    package let service: any LocalSSHDiscovering
    package let networkAvailability: () -> LocalSSHDiscoveryNetworkAvailability
    package let makeScanID: () -> UUID
}
