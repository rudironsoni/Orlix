import Foundation

nonisolated enum CloudKitSyncConstants {
    static let appPrefix = "com.rudironsoni.orlix"
    static let cloudKitContainerIdentifier = "iCloud.com.rudironsoni.orlix"
    static let recordZoneName = "OrlixZone"
    static let databaseSubscriptionID = "database-changes"

    static let syncEnabledKey = "iCloudSyncEnabled"
    static let pendingCloudKitSyncQueueStorageKey = "\(appPrefix).pendingCloudKitSyncQueue"

    static func changeTokenKey(for zoneName: String = recordZoneName) -> String {
        "\(appPrefix).cloudkit.\(zoneName).token"
    }

    static func zoneReadyKey(for zoneName: String = recordZoneName) -> String {
        "\(appPrefix).cloudkit.\(zoneName).ready"
    }
}
