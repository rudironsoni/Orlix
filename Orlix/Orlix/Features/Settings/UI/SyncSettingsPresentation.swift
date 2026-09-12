import SwiftUI

enum SyncSettingsPrimaryAction: Equatable {
    case syncNow
    case tryAgain
    case checkAgain
    case syncing

    var title: String {
        switch self {
        case .syncNow: String(localized: "Sync Now")
        case .tryAgain: String(localized: "Try Again")
        case .checkAgain: String(localized: "Check Again")
        case .syncing: String(localized: "Syncing")
        }
    }

    var isRunning: Bool {
        self == .syncing
    }
}

enum SyncSettingsContentSyncState: Equatable {
    case synced
    case included
    case notSyncing

    init(
        syncEnabled: Bool,
        userState: SyncSettingsUserState,
        lastSuccessfulSyncDate: Date?
    ) {
        if !syncEnabled {
            self = .notSyncing
        } else if userState == .upToDate, lastSuccessfulSyncDate != nil {
            self = .synced
        } else {
            self = .included
        }
    }

    var sectionTitle: String {
        switch self {
        case .synced: String(localized: "Synced with iCloud")
        case .included: String(localized: "Included in iCloud Sync")
        case .notSyncing: String(localized: "iCloud Sync Includes")
        }
    }

    var rowTitle: String {
        switch self {
        case .synced: String(localized: "Synced")
        case .included: String(localized: "Included")
        case .notSyncing: String(localized: "Not Syncing")
        }
    }
}

extension SyncSettingsUserState {
    var title: String {
        switch self {
        case .readyToSync: String(localized: "Ready to Sync")
        case .upToDate: String(localized: "Up to Date")
        case .syncing: String(localized: "Syncing")
        case .waitingForNetwork: String(localized: "Waiting for Network")
        case .signInToICloud: String(localized: "Sign In to iCloud")
        case .needsAttention: String(localized: "Sync Needs Attention")
        case .disabled: String(localized: "Sync is Off")
        }
    }

    var recoveryGuidance: String? {
        switch self {
        case .waitingForNetwork:
            String(localized: "Check your network connection.")
        case .signInToICloud:
            String(localized: "Sign in to iCloud and turn on iCloud Drive.")
        case .needsAttention:
            String(localized: "Try syncing again. Your changes are safe.")
        case .readyToSync, .upToDate, .syncing, .disabled:
            nil
        }
    }
}

extension SyncSettingsCredentialState {
    var statusTitle: String {
        switch self {
        case .storedInICloudKeychain: String(localized: "iCloud Keychain")
        case .storedOnThisDevice: String(localized: "On This Device")
        case .needsAttention: String(localized: "Needs Attention")
        }
    }
}

extension SyncSettingsErrorCategory {
    var title: String {
        switch self {
        case .account: String(localized: "Account")
        case .cloudData: String(localized: "App Data")
        case .credentials: String(localized: "Credentials")
        case .network: String(localized: "Network")
        }
    }
}

extension SyncSettingsManualSyncState {
    var announcement: String? {
        switch self {
        case .success:
            String(localized: "iCloud Sync completed.")
        case .waitingForNetwork:
            String(localized: "iCloud Sync is waiting for the network.")
        case .accountActionRequired:
            String(localized: "Sign in to iCloud to sync.")
        case .failure:
            String(localized: "iCloud Sync needs attention.")
        case .idle, .running:
            nil
        }
    }
}
