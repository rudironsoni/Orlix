import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package enum StatsPreferencesPendingCloudKitPayloadCodec {
    static package let entityType = "statsPreferences"

    static package func encode(_ preferences: StatsPreferences) throws -> PendingCloudKitPayloadEnvelope {
        try PendingCloudKitPayloadEnvelope(
            entityType: entityType,
            entityKey: StatsPreferences.recordName,
            operation: .upsert,
            drainPriority: 5,
            value: preferences
        )
    }

    static package func decode(_ payload: PendingCloudKitPayloadEnvelope) throws -> StatsPreferences? {
        guard let preferences = try payload.decode(
            StatsPreferences.self,
            entityType: entityType,
            operation: .upsert
        ) else {
            return nil
        }
        try payload.validate(entityKey: StatsPreferences.recordName, drainPriority: 5)
        return preferences
    }

    static package func migrateLegacy(
        kind: String,
        encodedValue: Data
    ) throws -> PendingCloudKitPayloadEnvelope? {
        guard kind == "statsPreferencesUpsert" else { return nil }
        return try encode(JSONDecoder().decode(StatsPreferences.self, from: encodedValue))
    }
}

nonisolated package extension PendingCloudKitPayloadEnvelope {
    static package func statsPreferencesUpsert(_ preferences: StatsPreferences) throws -> Self {
        try StatsPreferencesPendingCloudKitPayloadCodec.encode(preferences)
    }
}
