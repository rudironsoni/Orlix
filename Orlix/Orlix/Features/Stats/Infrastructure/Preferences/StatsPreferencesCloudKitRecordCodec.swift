import CloudKit
import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package enum StatsPreferencesCloudKitRecordCodecError: Error, Equatable, Sendable {
    case encodingFailed
}

nonisolated package enum StatsPreferencesCloudKitRecordCodec {
    static package let recordType = "UserPreference"
    static package let recordName = StatsPreferences.recordName

    static package func recordID(in zoneID: CKRecordZone.ID) -> CKRecord.ID {
        CKRecord.ID(recordName: recordName, zoneID: zoneID)
    }

    static package func preferences(from record: CKRecord) -> StatsPreferences? {
        guard let payload = record["payload"] as? Data,
              var preferences = try? JSONDecoder().decode(
                  StatsPreferences.self,
                  from: payload
              ) else {
            return nil
        }

        if let schemaVersion = record["schemaVersion"] as? Int, schemaVersion > 0 {
            preferences.schemaVersion = schemaVersion
        }
        if let updatedAt = record["updatedAt"] as? Date,
           updatedAt > preferences.updatedAt {
            preferences.updatedAt = updatedAt
        }
        if let writerDeviceID = record["lastWriterDeviceId"] as? String,
           !writerDeviceID.isEmpty {
            preferences.lastWriterDeviceId = writerDeviceID
        }

        return preferences.normalized()
    }

    static package func record(
        for preferences: StatsPreferences,
        recordID: CKRecord.ID,
        existingRecord: CKRecord? = nil
    ) throws -> CKRecord {
        let normalized = preferences.normalized()
        let payload: Data
        do {
            payload = try JSONEncoder().encode(normalized)
        } catch {
            throw StatsPreferencesCloudKitRecordCodecError.encodingFailed
        }

        let record = existingRecord ?? CKRecord(recordType: recordType, recordID: recordID)
        record["schemaVersion"] = normalized.schemaVersion
        record["payload"] = payload
        record["updatedAt"] = normalized.updatedAt
        record["lastWriterDeviceId"] = normalized.lastWriterDeviceId
        return record
    }

    static package func merge(
        local: StatsPreferences,
        remote: StatsPreferences
    ) -> StatsPreferences {
        StatsPreferences.merged(local: local, remote: remote).normalized()
    }
}
