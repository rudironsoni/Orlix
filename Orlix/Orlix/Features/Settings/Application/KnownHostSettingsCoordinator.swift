import Combine
import Foundation

nonisolated package struct KnownHostSettingsItem: Identifiable, Equatable, Sendable {
    package let host: String
    package let port: Int
    package let lastSeenAt: Date

    package var endpoint: String { "\(host):\(port)" }
    package var id: String { endpoint }
}

@MainActor
package protocol KnownHostSettingsRepository: AnyObject {
    package func loadKnownHosts() -> [KnownHostSettingsItem]
    package func removeKnownHost(host: String, port: Int)
    package func removeAllKnownHosts()
}

@MainActor
final package class KnownHostSettingsCoordinator: ObservableObject {
    @Published private(set) var knownHosts: [KnownHostSettingsItem] = []

    private let repository: any KnownHostSettingsRepository

    package init(repository: any KnownHostSettingsRepository) {
        self.repository = repository
    }

    package func loadHosts() {
        knownHosts = repository.loadKnownHosts()
    }

    package func removeKnownHost(_ knownHost: KnownHostSettingsItem) {
        repository.removeKnownHost(host: knownHost.host, port: knownHost.port)
        loadHosts()
    }

    package func removeAllKnownHosts() {
        repository.removeAllKnownHosts()
        loadHosts()
    }
}
