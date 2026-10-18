import CloudKit
import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package enum TerminalAccessoryCloudKitRecordCodecError: Error, Equatable, Sendable {
    case encodingFailed
}

nonisolated package enum TerminalAccessoryCloudKitRecordCodec {
    static package let recordType = "UserPreference"
    static package let recordName = TerminalAccessoryProfile.recordName
    static package let recordKeys = [
        "schemaVersion", "payload", "updatedAt", "lastWriterDeviceId"
    ]

    static package func recordID(in zoneID: CKRecordZone.ID) -> CKRecord.ID {
        CKRecord.ID(recordName: recordName, zoneID: zoneID)
    }

    static package func profile(from record: CKRecord) -> TerminalAccessoryProfile? {
        guard let payload = record["payload"] as? Data,
              var profile = try? JSONDecoder().decode(
                  TerminalAccessoryProfile.self,
                  from: payload
              ) else {
            return nil
        }

        if let schemaVersion = record["schemaVersion"] as? Int, schemaVersion > 0 {
            profile.schemaVersion = schemaVersion
        }
        if let updatedAt = record["updatedAt"] as? Date, updatedAt > profile.updatedAt {
            profile.updatedAt = updatedAt
        }
        if let writerDeviceID = record["lastWriterDeviceId"] as? String,
           !writerDeviceID.isEmpty {
            profile.lastWriterDeviceId = writerDeviceID
        }

        return profile.normalized()
    }

    static package func record(
        for profile: TerminalAccessoryProfile,
        recordID: CKRecord.ID,
        existingRecord: CKRecord? = nil
    ) throws -> CKRecord {
        let normalized = profile.normalized()
        let payload: Data
        do {
            payload = try JSONEncoder().encode(normalized)
        } catch {
            throw TerminalAccessoryCloudKitRecordCodecError.encodingFailed
        }

        let record = existingRecord ?? CKRecord(recordType: recordType, recordID: recordID)
        record["schemaVersion"] = normalized.schemaVersion
        record["payload"] = payload
        record["updatedAt"] = normalized.updatedAt
        record["lastWriterDeviceId"] = normalized.lastWriterDeviceId
        return record
    }

    static package func merge(
        local: TerminalAccessoryProfile,
        remote: TerminalAccessoryProfile
    ) -> TerminalAccessoryProfile {
        TerminalAccessoryProfile.merged(local: local, remote: remote).normalized()
    }
}
