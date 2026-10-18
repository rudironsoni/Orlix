import CloudKit
import Foundation
import os.log

// MARK: - CloudKit Manager

@MainActor
final package class CloudKitManager {
    static package let shared = CloudKitManager()

    package typealias SyncStatus = CloudKitSyncState.Status

    package let statusStore = CloudKitSyncStatusStore()

    package var syncState: CloudKitSyncState { statusStore.syncState }
    package var lastSyncDate: Date? { statusStore.lastSyncDate }
    package var accountState: CloudKitAccountState { statusStore.accountState }
    package var syncStatus: SyncStatus { statusStore.syncState.status }
    package var isAvailable: Bool { statusStore.syncState.isAvailable }
    package var cloudKitSyncGeneration = UUID()

    package let container: CKContainer?
    package let database: CKDatabase?
    package let logger = Logger(subsystem: Bundle.main.bundleIdentifier!, category: "CloudKit")
    package let recordZoneName = CloudKitSyncConstants.recordZoneName
    lazy package var recordZone = CKRecordZone(zoneName: recordZoneName)
    package var recordZoneID: CKRecordZone.ID { recordZone.zoneID }
    package var changeTokenKey: String { CloudKitSyncConstants.changeTokenKey(for: recordZoneName) }
    package var zoneReadyKey: String { CloudKitSyncConstants.zoneReadyKey(for: recordZoneName) }

    package let syncEnabled: @MainActor @Sendable () -> Bool
    package let fetchAccountStatus: @MainActor @Sendable () async throws -> CKAccountStatus
    package var isSyncEnabled: Bool { syncEnabled() }
    package struct AccountStatusCheck {
        let id: UUID
        let generation: UUID
        let task: Task<CKAccountStatus, Error>
    }

    package struct InFlightRecordChanges {
        let id: UUID
        let identity: CloudKitRecordChangeFetchIdentity
        let task: Task<CloudKitRawRecordChanges, Error>
        var waiters: [UUID: CloudKitTaskContinuation<CloudKitRawRecordChanges>]
        var teardownWaiters: [UUID: CloudKitTaskContinuation<Void>]
    }

    package struct PendingRecordChanges {
        let identity: CloudKitRecordChangeFetchIdentity
        let changes: CloudKitRawRecordChanges
        let token: CKServerChangeToken?
    }

    package struct FetchedRecordChanges {
        let changes: [CloudKitRawRecordChange]
        let isFullFetch: Bool
        let token: CKServerChangeToken?
    }

    package var inFlightRecordChanges: InFlightRecordChanges?
    package var pendingRecordChanges: PendingRecordChanges?
    package var accountStatusCheck: AccountStatusCheck?
    package var ensureZoneTask: Task<Void, Error>?
    package var zoneReady: Bool

    private convenience init() {
        if Self.hasICloudContainerEntitlement() {
            let container = CKContainer(
                identifier: CloudKitSyncConstants.cloudKitContainerIdentifier
            )
            self.init(
                container: container,
                syncEnabled: { SyncSettings.isEnabled },
                accountStatus: { try await container.accountStatus() }
            )
            return
        }
        self.init(
            container: nil,
            syncEnabled: { false },
            accountStatus: { .couldNotDetermine }
        )
    }

    convenience package init(
        container: CKContainer,
        syncEnabled: @escaping @MainActor @Sendable () -> Bool,
        accountStatus: @escaping @MainActor @Sendable () async throws -> CKAccountStatus,
        initialZoneReady: Bool = UserDefaults.standard.bool(
            forKey: CloudKitSyncConstants.zoneReadyKey()
        )
    ) {
        self.init(
            container: Optional(container),
            syncEnabled: syncEnabled,
            accountStatus: accountStatus,
            initialZoneReady: initialZoneReady
        )
    }

    package init(
        container: CKContainer?,
        syncEnabled: @escaping @MainActor @Sendable () -> Bool,
        accountStatus: @escaping @MainActor @Sendable () async throws -> CKAccountStatus,
        initialZoneReady: Bool = UserDefaults.standard.bool(
            forKey: CloudKitSyncConstants.zoneReadyKey()
        )
    ) {
        self.container = container
        database = container?.privateCloudDatabase
        self.syncEnabled = syncEnabled
        fetchAccountStatus = accountStatus
        zoneReady = initialZoneReady
        if container == nil || !isSyncEnabled {
            applySyncDisabledState()
        } else {
            let generation = cloudKitSyncGeneration
            Task { [weak self] in
                await self?.checkAccountStatus(for: generation)
            }
        }
    }

    static package func hasICloudContainerEntitlement() -> Bool {
        #if targetEnvironment(simulator)
        return true
        #else
        guard let url = Bundle.main.url(forResource: "embedded", withExtension: "mobileprovision"),
              let data = try? Data(contentsOf: url),
              let raw = String(data: data, encoding: .ascii) ?? String(data: data, encoding: .utf8)
        else {
            return false
        }
        return raw.contains("icloud-services") || raw.contains("icloud-container-identifiers")
        #endif
    }

    package func cloudDatabase() throws -> CKDatabase {
        guard let database else {
            throw CloudKitError.notAvailable
        }
        return database
    }
}
