import SwiftUI
#if ORLIX_MODULAR_APP
import OrlixModels
import OrlixSSH
#endif

// MARK: - Server Stats View

package struct ServerStatsView: View {
    @Environment(\.colorScheme) private var colorScheme
    @AppStorage(StatsResolvedAppearance.storageKey) private var appearanceMode = "system"

    package let server: Server
    package let backgroundColor: Color
    package var sharedClientProvider: () -> SSHClient? = { nil }

    @ObservedObject private var statsCollector: ServerStatsCollector
    @ObservedObject private var preferencesStore: PreferencesStore
    @ObservedObject private var volumeVisibilityStore: ServerVolumeVisibilityStore
    private let securityApprovalActions: ServerStatsSecurityApprovalActions
    private let isDockerUnlocked: Bool
    private let presentDockerUpgrade: () -> Void
    @State private var isShowingAppearanceSettings = false

    package init(
        server: Server,
        backgroundColor: Color,
        sharedClientProvider: @escaping () -> SSHClient? = { nil },
        dependencies: ServerStatsScreenDependencies,
        isDockerUnlocked: Bool,
        presentDockerUpgrade: @escaping () -> Void = {}
    ) {
        self.server = server
        self.backgroundColor = backgroundColor
        self.sharedClientProvider = sharedClientProvider
        _statsCollector = ObservedObject(
            wrappedValue: dependencies.runtimeStore.collector(for: server.id)
        )
        _preferencesStore = ObservedObject(wrappedValue: dependencies.preferencesStore)
        _volumeVisibilityStore = ObservedObject(wrappedValue: dependencies.volumeVisibilityStore)
        self.securityApprovalActions = dependencies.securityApprovalActions
        self.isDockerUnlocked = isDockerUnlocked
        self.presentDockerUpgrade = presentDockerUpgrade
    }

    package var body: some View {
        let currentPreferences = preferencesStore.preferences
        let resolvedColorScheme = StatsResolvedAppearance.colorScheme(from: appearanceMode, fallback: colorScheme)

        ServerStatsDashboard(
            server: server,
            backgroundColor: backgroundColor,
            sharedClientProvider: sharedClientProvider,
            statsCollector: statsCollector,
            preferences: currentPreferences,
            volumeVisibilityStore: volumeVisibilityStore,
            securityApprovalActions: securityApprovalActions,
            isDockerUnlocked: isDockerUnlocked
        ) {
            isShowingAppearanceSettings = true
        } showDockerUpgrade: {
            presentDockerUpgrade()
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(
            StatsBlocksContent.pageBackground(
                for: currentPreferences.style,
                backgroundColor: backgroundColor,
                colorScheme: resolvedColorScheme
            )
        )
        .statsDetailPresentation(isPresented: $isShowingAppearanceSettings, size: StatsPresentationSize.large) {
            StatsAppearanceSettingsSheet(store: preferencesStore)
        }
    }
}
