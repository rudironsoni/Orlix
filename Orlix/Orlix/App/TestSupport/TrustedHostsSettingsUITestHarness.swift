#if DEBUG
import Foundation
import SwiftUI

#if ORLIX_MODULAR_APP
import OrlixSettings
#endif
@MainActor
private final class TrustedHostsSettingsUITestRepository: KnownHostSettingsRepository {
    private var knownHosts = [
        KnownHostSettingsItem(
            host: "trusted.example.com",
            port: 22,
            lastSeenAt: Date(timeIntervalSince1970: 1_577_934_245)
        ),
        KnownHostSettingsItem(
            host: "second.example.com",
            port: 2222,
            lastSeenAt: Date(timeIntervalSince1970: 1_609_459_200)
        ),
    ]

    func loadKnownHosts() -> [KnownHostSettingsItem] {
        knownHosts
    }

    func removeKnownHost(host: String, port: Int) {
        knownHosts.removeAll { $0.host == host && $0.port == port }
    }

    func removeAllKnownHosts() {
        knownHosts.removeAll()
    }
}

package struct TrustedHostsSettingsUITestHarness: View {
    @StateObject private var coordinator = KnownHostSettingsCoordinator(
        repository: TrustedHostsSettingsUITestRepository()
    )

    package var body: some View {
        NavigationView {
            TrustedHostsSettingsView()
                .navigationTitle("Trusted Hosts")
        }
        .environmentObject(coordinator)
        .accessibilityIdentifier("orlix.trustedHostsSettingsTest.root")
    }
}
#endif
