import SwiftUI

package struct SyncSettingsStatusHero: View {
    package let state: SyncSettingsUserState
    package let lastSuccessfulSyncDate: Date?
    package let primaryAction: SyncSettingsPrimaryAction?
    package let onPrimaryAction: () -> Void

    package var body: some View {
        platformBody
    }
}

package struct SyncSettingsPrimaryActionButton: View {
    package let action: SyncSettingsPrimaryAction
    package let perform: () -> Void

    package var body: some View {
        Button(action: perform) {
            if action.isRunning {
                HStack(spacing: 7) {
                    ProgressView()
                        .controlSize(.small)
                    Text(action.title)
                }
            } else {
                Text(action.title)
            }
        }
        .buttonStyle(.borderedProminent)
        .controlSize(.large)
        .disabled(action.isRunning)
        .accessibilityIdentifier("orlix.settings.sync.action.primary")
    }
}

package extension SyncSettingsUserState {
    package var statusHeroSystemImage: String {
        switch self {
        case .readyToSync, .upToDate:
            "icloud.fill"
        case .syncing:
            "icloud.and.arrow.up.fill"
        case .waitingForNetwork, .disabled:
            "icloud.slash.fill"
        case .signInToICloud:
            "person.crop.circle.badge.exclamationmark"
        case .needsAttention:
            "exclamationmark.icloud.fill"
        }
    }

    package var statusHeroTint: Color {
        switch self {
        case .readyToSync, .upToDate, .syncing:
            .blue
        case .waitingForNetwork, .signInToICloud:
            .orange
        case .needsAttention:
            .red
        case .disabled:
            .secondary
        }
    }
}

package struct SyncSettingsStatusDetail: View {
    package let state: SyncSettingsUserState
    package let lastSuccessfulSyncDate: Date?

    package var body: some View {
        Group {
            switch state {
            case .readyToSync:
                Text("No successful sync yet.")
            case .upToDate:
                if let lastSuccessfulSyncDate {
                    Text(
                        "Last synced \(lastSuccessfulSyncDate, format: .relative(presentation: .named))"
                    )
                }
            case .syncing:
                Text("Sync in progress")
            case .waitingForNetwork:
                Text("Changes will sync later.")
            case .signInToICloud:
                Text("Sign in to continue.")
            case .needsAttention:
                Text("Your data is safe.")
            case .disabled:
                Text("Data stays on this device.")
            }
        }
        .foregroundStyle(.secondary)
    }
}
