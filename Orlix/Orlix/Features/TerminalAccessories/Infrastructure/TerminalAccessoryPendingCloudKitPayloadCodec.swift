import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package enum TerminalAccessoryPendingCloudKitPayloadCodec {
    static package let entityType = "terminalAccessoryProfile"

    static package func encode(
        _ profile: TerminalAccessoryProfile
    ) throws -> PendingCloudKitPayloadEnvelope {
        try PendingCloudKitPayloadEnvelope(
            entityType: entityType,
            entityKey: TerminalAccessoryProfile.recordName,
            operation: .upsert,
            drainPriority: 4,
            value: profile
        )
    }

    static package func decode(
        _ payload: PendingCloudKitPayloadEnvelope
    ) throws -> TerminalAccessoryProfile? {
        guard let profile = try payload.decode(
            TerminalAccessoryProfile.self,
            entityType: entityType,
            operation: .upsert
        ) else {
            return nil
        }
        try payload.validate(
            entityKey: TerminalAccessoryProfile.recordName,
            drainPriority: 4
        )
        return profile
    }

    static package func migrateLegacy(
        kind: String,
        encodedValue: Data
    ) throws -> PendingCloudKitPayloadEnvelope? {
        guard kind == "terminalAccessoryProfileUpsert" else { return nil }
        return try encode(
            JSONDecoder().decode(TerminalAccessoryProfile.self, from: encodedValue)
        )
    }
}

nonisolated package extension PendingCloudKitPayloadEnvelope {
    static package func terminalAccessoryProfileUpsert(
        _ profile: TerminalAccessoryProfile
    ) throws -> Self {
        try TerminalAccessoryPendingCloudKitPayloadCodec.encode(profile)
    }
}
