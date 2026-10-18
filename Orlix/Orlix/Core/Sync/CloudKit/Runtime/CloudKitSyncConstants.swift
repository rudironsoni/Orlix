import Foundation

nonisolated package enum CloudKitSyncConstants {
    static package let appPrefix = "com.rudironsoni.orlix"
    static package let cloudKitContainerIdentifier = "iCloud.com.rudironsoni.orlix"
    static package let recordZoneName = "OrlixZone"
    static package let databaseSubscriptionID = "database-changes"

    static package let syncEnabledKey = "iCloudSyncEnabled"
    static package let pendingCloudKitSyncQueueStorageKey = "\(appPrefix).pendingCloudKitSyncQueue"
    static package let terminalThemeNameKey = "terminalThemeName"
    static package let terminalThemeNameLightKey = "terminalThemeNameLight"
    static package let terminalUsePerAppearanceThemeKey = "terminalUsePerAppearanceTheme"

    static package func changeTokenKey(for zoneName: String = recordZoneName) -> String {
        "\(appPrefix).cloudkit.\(zoneName).token"
    }

    static package func zoneReadyKey(for zoneName: String = recordZoneName) -> String {
        "\(appPrefix).cloudkit.\(zoneName).ready"
    }
}
