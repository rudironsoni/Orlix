#if ORLIX_MODULAR_APP
import OrlixSSH
#endif
@MainActor
private final class AppKnownHostSettingsRepository: KnownHostSettingsRepository {
    private let knownHosts: KnownHostsManager

    package init(knownHosts: KnownHostsManager) {
        self.knownHosts = knownHosts
    }

    package func loadKnownHosts() -> [KnownHostSettingsItem] {
        knownHosts.entries().map { entry in
            KnownHostSettingsItem(
                host: entry.host,
                port: entry.port,
                lastSeenAt: entry.lastSeenAt
            )
        }
    }

    package func removeKnownHost(host: String, port: Int) {
        knownHosts.remove(host: host, port: port)
    }

    package func removeAllKnownHosts() {
        knownHosts.removeAll()
    }
}

@MainActor
package enum KnownHostSettingsLiveComposition {
    static package func makeCoordinator(
        knownHosts: KnownHostsManager
    ) -> KnownHostSettingsCoordinator {
        KnownHostSettingsCoordinator(
            repository: AppKnownHostSettingsRepository(knownHosts: knownHosts)
        )
    }
}
