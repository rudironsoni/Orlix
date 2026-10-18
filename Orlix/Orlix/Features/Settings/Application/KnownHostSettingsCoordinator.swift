import Combine
import Foundation

nonisolated package struct KnownHostSettingsItem: Identifiable, Equatable, Sendable {
    package let host: String
    package let port: Int
    package let lastSeenAt: Date

    package var endpoint: String { "\(host):\(port)" }
    package var id: String { endpoint }


    package init(

        host: String,

        port: Int,

        lastSeenAt: Date

    ) {

        self.host = host

        self.port = port

        self.lastSeenAt = lastSeenAt

    }
}

@MainActor
package protocol KnownHostSettingsRepository: AnyObject {
    func loadKnownHosts() -> [KnownHostSettingsItem]
    func removeKnownHost(host: String, port: Int)
    func removeAllKnownHosts()
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
