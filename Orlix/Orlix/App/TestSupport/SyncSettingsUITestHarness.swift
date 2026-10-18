#if DEBUG
import Combine
import SwiftUI

#if ORLIX_MODULAR_APP
import OrlixModels
import OrlixSettings
#endif
@MainActor
private final class SyncSettingsUITestCloud: SyncSettingsCloudSyncing {
    private let states: CurrentValueSubject<SyncSettingsCloudState, Never>

    package init(isEnabled: Bool) {
        states = CurrentValueSubject(
            SyncSettingsCloudState(
                status: isEnabled ? .idle : .disabled,
                isAvailable: isEnabled,
                accountState: isEnabled ? .available : .disabled,
                pendingOperationCount: 0,
                hasPendingFailure: false,
                lastSuccessfulSyncDate: nil
            )
        )
    }

    package var currentState: SyncSettingsCloudState { states.value }
    package var stateUpdates: AnyPublisher<SyncSettingsCloudState, Never> {
        states.eraseToAnyPublisher()
    }

    package func setSyncEnabled(_ enabled: Bool) {
        states.send(
            SyncSettingsCloudState(
                status: enabled ? .idle : .disabled,
                isAvailable: enabled,
                accountState: enabled ? .available : .disabled,
                pendingOperationCount: 0,
                hasPendingFailure: false,
                lastSuccessfulSyncDate: nil
            )
        )
    }

    package func checkAccountStatus() async {}
}

@MainActor
private final class SyncSettingsUITestCredentials: SyncSettingsCredentialSyncing {
    private(set) var currentState: SyncSettingsCredentialState

    package init(isEnabled: Bool) {
        currentState = isEnabled ? .storedInICloudKeychain : .storedOnThisDevice
    }

    package func prepareCredentialStorage(isSyncEnabled: Bool) throws {
        currentState = isSyncEnabled ? .storedInICloudKeychain : .storedOnThisDevice
    }

    package func removeCredentialsFromICloud() throws {
        currentState = .storedOnThisDevice
    }
}

@MainActor
private final class SyncSettingsUITestData: SyncSettingsDataRefreshing {
    package func handleSyncDisabled() {}
    package func syncNow() async throws {}
}

@MainActor
private final class SyncSettingsUITestContent: SyncSettingsContentSummarizing {
    package let currentSummary = SyncSettingsContentSummary(
        workspaceCount: 2,
        serverCount: 7,
        customThemeCount: 3,
        customFontCount: 2,
        serverCredentialCount: 6,
        reusableSSHKeyCount: 4
    )
}

@MainActor
private final class SyncSettingsUITestHistory: SyncSettingsHistoryStoring {
    private(set) var lastSuccessfulSyncDate: Date? = Date()

    package func recordSuccessfulSync(at date: Date) throws {
        lastSuccessfulSyncDate = date
    }
}

package struct SyncSettingsUITestHarness: View {
    @StateObject private var coordinator: SyncSettingsCoordinator

    package init() {
        let isEnabled = !Foundation.ProcessInfo.processInfo.arguments.contains(
            "--orlix-ui-test-sync-settings-disabled"
        )
        try? SyncSettings.persistEnabled(isEnabled)
        _coordinator = StateObject(
            wrappedValue: SyncSettingsCoordinator(
                cloud: SyncSettingsUITestCloud(isEnabled: isEnabled),
                credentials: SyncSettingsUITestCredentials(isEnabled: isEnabled),
                data: SyncSettingsUITestData(),
                content: SyncSettingsUITestContent(),
                history: SyncSettingsUITestHistory(),
                runtime: SyncSettingsRuntimeInfo(
                    appVersion: "UI Test",
                    buildVersion: "1",
                    platform: "UI Test"
                )
            )
        )
    }

    package var body: some View {
        NavigationView {
            SyncSettingsView()
                .navigationTitle("iCloud Sync")
        }
        .navigationViewStyle(.stack)
        .environmentObject(coordinator)
        .accessibilityIdentifier("orlix.syncSettingsTest.root")
    }
}
#endif
