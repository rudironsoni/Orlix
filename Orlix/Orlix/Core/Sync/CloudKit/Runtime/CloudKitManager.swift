import CloudKit
import Foundation
import os.log

// MARK: - CloudKit Manager

@MainActor
final class CloudKitManager {
    static let shared = CloudKitManager()

    typealias SyncStatus = CloudKitSyncState.Status

    let statusStore = CloudKitSyncStatusStore()

    var syncState: CloudKitSyncState { statusStore.syncState }
    var lastSyncDate: Date? { statusStore.lastSyncDate }
    var accountState: CloudKitAccountState { statusStore.accountState }
    var syncStatus: SyncStatus { statusStore.syncState.status }
    var isAvailable: Bool { statusStore.syncState.isAvailable }
    var cloudKitSyncGeneration = UUID()

    let container: CKContainer?
    let database: CKDatabase?
    let logger = Logger(subsystem: Bundle.main.bundleIdentifier!, category: "CloudKit")
    let recordZoneName = CloudKitSyncConstants.recordZoneName
    lazy var recordZone = CKRecordZone(zoneName: recordZoneName)
    var recordZoneID: CKRecordZone.ID { recordZone.zoneID }
    var changeTokenKey: String { CloudKitSyncConstants.changeTokenKey(for: recordZoneName) }
    var zoneReadyKey: String { CloudKitSyncConstants.zoneReadyKey(for: recordZoneName) }

    let syncEnabled: @MainActor @Sendable () -> Bool
    let fetchAccountStatus: @MainActor @Sendable () async throws -> CKAccountStatus
    var isSyncEnabled: Bool { syncEnabled() }
    struct AccountStatusCheck {
        let id: UUID
        let generation: UUID
        let task: Task<CKAccountStatus, Error>
    }

    struct InFlightRecordChanges {
        let id: UUID
        let identity: CloudKitRecordChangeFetchIdentity
        let task: Task<CloudKitRawRecordChanges, Error>
        var waiters: [UUID: CloudKitTaskContinuation<CloudKitRawRecordChanges>]
        var teardownWaiters: [UUID: CloudKitTaskContinuation<Void>]
    }

    struct PendingRecordChanges {
        let identity: CloudKitRecordChangeFetchIdentity
        let changes: CloudKitRawRecordChanges
        let token: CKServerChangeToken?
    }

    struct FetchedRecordChanges {
        let changes: [CloudKitRawRecordChange]
        let isFullFetch: Bool
        let token: CKServerChangeToken?
    }

    var inFlightRecordChanges: InFlightRecordChanges?
    var pendingRecordChanges: PendingRecordChanges?
    var accountStatusCheck: AccountStatusCheck?
    var ensureZoneTask: Task<Void, Error>?
    var zoneReady: Bool

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

    convenience init(
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

    init(
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

    static func hasICloudContainerEntitlement() -> Bool {
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

    func cloudDatabase() throws -> CKDatabase {
        guard let database else {
            throw CloudKitError.notAvailable
        }
        return database
    }
}
