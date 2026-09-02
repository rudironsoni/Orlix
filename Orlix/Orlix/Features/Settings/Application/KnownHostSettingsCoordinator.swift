import Combine
import Foundation

nonisolated struct KnownHostSettingsItem: Identifiable, Equatable, Sendable {
    let host: String
    let port: Int
    let lastSeenAt: Date

    var endpoint: String { "\(host):\(port)" }
    var id: String { endpoint }
}

nonisolated enum KnownHostTrustReset: Equatable, Sendable {
    case host(String, Int)
    case all
}

@MainActor
protocol KnownHostSettingsRepository: AnyObject {
    func loadKnownHosts() -> [KnownHostSettingsItem]
    func removeKnownHost(host: String, port: Int)
    func removeAllKnownHosts()
}

@MainActor
final class KnownHostSettingsCoordinator: ObservableObject {
    @Published private(set) var knownHosts: [KnownHostSettingsItem] = []

    private let repository: any KnownHostSettingsRepository
    private let invalidateLiveTSSHTrust: (KnownHostTrustReset) -> Void

    init(
        repository: any KnownHostSettingsRepository,
        invalidateLiveTSSHTrust: @escaping (KnownHostTrustReset) -> Void = { _ in }
    ) {
        self.repository = repository
        self.invalidateLiveTSSHTrust = invalidateLiveTSSHTrust
    }

    func loadHosts() {
        knownHosts = repository.loadKnownHosts()
    }

    func removeKnownHost(_ knownHost: KnownHostSettingsItem) {
        repository.removeKnownHost(host: knownHost.host, port: knownHost.port)
        invalidateLiveTSSHTrust(.host(knownHost.host, knownHost.port))
        loadHosts()
    }

    func removeAllKnownHosts() {
        repository.removeAllKnownHosts()
        invalidateLiveTSSHTrust(.all)
        loadHosts()
    }
}
