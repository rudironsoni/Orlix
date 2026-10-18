import Foundation

package typealias PendingCloudKitMutationPayload = PendingCloudKitPayloadEnvelope

nonisolated package protocol PendingCloudKitLegacyMutationMigrating {
    package func migrate(
        recordData: Data
    ) -> Result<PendingCloudKitMutation, PendingCloudKitMutationQuarantineReason>?
}
