import SwiftUI

#if ORLIX_MODULAR_APP
import OrlixModels
import OrlixSSH
import OrlixSettings
import OrlixStats
#endif
package struct SettingsView: View {
    package let statsPreferencesStore: PreferencesStore
    package let voiceModelManagers: VoiceSettingsModelManagerOwner
    package let analyticsOptOutAction: AnalyticsOptOutAction
    package let remoteSessionBackends: [RemoteSessionBackendMetadata]

    @AppStorage(TerminalDefaults.fontSizeKey) package var terminalFontSize = TerminalDefaults.defaultFontSize
    @AppStorage(SettingsRoutePersistence.selectedRouteKey)
    package var selectedRouteRaw = SettingsRoute.defaultRoute.rawValue
    @State package var searchText = ""

    @EnvironmentObject package var storeManager: StoreManager
    @Environment(\.dismiss) package var dismiss

    package var body: some View {
        platformBody
    }

    package var selectedRoute: SettingsRoute {
        SettingsRoutePersistence.route(for: selectedRouteRaw)
    }

    package func visibleRoutes(in group: SettingsGroup) -> [SettingsRoute] {
        SettingsRouteCatalog.visibleRoutes(
            from: SettingsRouteCatalog.routes(in: group),
            matching: searchText
        )
    }

    package func visibleRoutes(from routes: [SettingsRoute]) -> [SettingsRoute] {
        SettingsRouteCatalog.visibleRoutes(from: routes, matching: searchText)
    }

    package var visibleDetailRoute: SettingsRoute? {
        SettingsRouteCatalog.visibleDetailRoute(
            selectedRoute: selectedRoute,
            matching: searchText
        )
    }

    @ViewBuilder
    package func destination(for route: SettingsRoute) -> some View {
        switch route {
        case .pro:
            ProSettingsView()
        case .appearanceAndLanguage:
            AppearanceLanguageSettingsView()
        case .navigationAndStats:
            NavigationStatsSettingsView(statsPreferencesStore: statsPreferencesStore)
        case .privacyAndAppLock:
            PrivacyAppLockSettingsView(analyticsOptOutAction: analyticsOptOutAction)
        case .terminalAppearance:
            TerminalAppearanceSettingsView(fontSize: $terminalFontSize)
        case .keyboardAndInput:
            TerminalKeyboardInputSettingsView()
        case .sessionsAndConnections:
            TerminalSessionsConnectionsSettingsView(backends: remoteSessionBackends)
        case .clipboardAndPaste:
            TerminalClipboardPasteSettingsView()
        case .sshKeys:
            KeychainSettingsView()
        case .trustedHosts:
            TrustedHostsSettingsView()
        case .iCloudSync:
            SyncSettingsView()
        case .transcription:
            TranscriptionSettingsView(modelManagers: voiceModelManagers)
        case .aboutAndSupport:
            AboutSettingsView()
        }
    }

    @ViewBuilder
    package func routeLabel(for route: SettingsRoute) -> some View {
        HStack(spacing: 10) {
            Label(route.title, systemImage: route.icon)
            Spacer(minLength: 8)
            if route == .pro {
                Text(storeStatusLabel)
                    .font(.caption2.weight(.semibold))
                    .foregroundStyle(storeManager.accessState == .pro ? .green : .secondary)
            }
        }
        .accessibilityElement(children: .combine)
        .accessibilityIdentifier("orlix.settings.route.\(route.rawValue)")
    }

    package var storeStatusLabel: String {
        switch storeManager.accessState {
        case .checking:
            String(localized: "Checking...")
        case .free:
            String(localized: "FREE_PLAN")
        case .pro:
            String(localized: "PRO")
        }
    }
}
