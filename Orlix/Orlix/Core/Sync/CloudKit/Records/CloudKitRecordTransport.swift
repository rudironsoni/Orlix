import CloudKit

nonisolated struct CloudKitRecordChangeFetchIdentity: Equatable, Sendable {
    let desiredKeys: Set<String>
    let forceFullFetch: Bool

    init(forceFullFetch: Bool, desiredKeys: [String]) {
        self.desiredKeys = Set(desiredKeys)
        self.forceFullFetch = forceFullFetch
    }
}

nonisolated enum CloudKitRecordChangeStreamError: Error, Equatable, Sendable {
    case incompatibleRequestInFlight
    case invalidCheckpoint
    case checkpointPersistenceFailed
}

nonisolated package struct CloudKitRecordChangeCheckpoint: Equatable, Sendable {
    package let id: UUID
}

nonisolated enum CloudKitRecordChangeRequestDecision: Equatable, Sendable {
    case start
    case coalesce
}

nonisolated enum CloudKitRecordChangeRequestPolicy {
    static func decision(
        for request: CloudKitRecordChangeFetchIdentity,
        inFlight: CloudKitRecordChangeFetchIdentity?
    ) throws -> CloudKitRecordChangeRequestDecision {
        guard let inFlight else { return .start }
        guard request == inFlight else {
            throw CloudKitRecordChangeStreamError.incompatibleRequestInFlight
        }
        return .coalesce
    }

    static func requiresCancellationTeardown(activeWaiterCount: Int) -> Bool {
        activeWaiterCount == 0
    }
}

nonisolated enum CloudKitRecordChangeCheckpointPolicy {
    static func validate(
        _ checkpoint: CloudKitRecordChangeCheckpoint,
        pending: CloudKitRecordChangeCheckpoint?
    ) throws {
        guard checkpoint == pending else {
            throw CloudKitRecordChangeStreamError.invalidCheckpoint
        }
    }
}

@MainActor
enum CloudKitRawRecordChange {
    case record(CKRecord)
    case deletion(recordID: CKRecord.ID, recordType: String)
}

@MainActor
package struct CloudKitRawRecordChanges: @unchecked Sendable {
    package let changes: [CloudKitRawRecordChange]
    package let isFullFetch: Bool
    package let checkpoint: CloudKitRecordChangeCheckpoint
}

@MainActor
package protocol CloudKitRecordTransport: AnyObject {
    package var cloudKitRecordZoneID: CKRecordZone.ID { get }

    package func performCloudKitRecordMutation<T>(
        _ operation: () async throws -> T
    ) async throws -> T
    package func fetchCloudKitRecords(
        matchingRecordTypes recordTypes: Set<String>,
        desiredKeys: [String]
    ) async throws -> [CKRecord]
    package func fetchCloudKitRecord(_ recordID: CKRecord.ID) async throws -> CKRecord
    package func upsertCloudKitRecord(_ record: CKRecord) async throws
    package func saveCloudKitRecordIfUnchanged(_ record: CKRecord) async throws
    package func cloudKitServerRecord(from error: Error) -> CKRecord?
    package func isCloudKitRecordMissing(_ error: Error) -> Bool
}

package extension CloudKitManager: CloudKitRecordTransport {}

@MainActor
package protocol CloudKitRecordChangeTransport: CloudKitRecordTransport {
    package var isCloudKitAvailable: Bool { get }
    package var cloudKitSyncGeneration: UUID { get }

    /// Reads the zone's single primary change stream without advancing its shared token.
    /// One consumer owns this stream. Every call must use that consumer's normalized keys.
    /// Concurrent calls coalesce only when their keys and fetch mode match exactly.
    package func fetchCloudKitRecordChanges(
        forceFullFetch: Bool,
        desiredKeys: [String]
    ) async throws -> CloudKitRawRecordChanges
    package func commitCloudKitRecordChanges(
        _ checkpoint: CloudKitRecordChangeCheckpoint
    ) throws
    package func deleteCloudKitRecord(_ recordID: CKRecord.ID) async throws
}

package extension CloudKitManager: CloudKitRecordChangeTransport {}
