import Foundation

package typealias PendingCloudKitMutationPayload = PendingCloudKitPayloadEnvelope

nonisolated package protocol PendingCloudKitLegacyMutationMigrating {
    func migrate(
        recordData: Data
    ) -> Result<PendingCloudKitMutation, PendingCloudKitMutationQuarantineReason>?
}
