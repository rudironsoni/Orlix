import CloudKit
import os.log

@MainActor
extension CloudKitManager {
    // MARK: - Subscriptions

    func subscribeToChanges() async {
        await subscribeToChanges(generation: cloudKitSyncGeneration)
    }

    func subscribeToChanges(generation: UUID) async {
        do {
            try await ensureAccountStatusChecked(for: generation)
        } catch {
            return
        }
        guard isCurrentGeneration(generation), isAvailable else { return }

        let subscriptionID = CloudKitSyncConstants.databaseSubscriptionID

        let notification = CKSubscription.NotificationInfo()
        notification.shouldSendContentAvailable = true

        let subscription = CKDatabaseSubscription(subscriptionID: subscriptionID)
        subscription.notificationInfo = notification

        do {
            if let existing = try? await cloudDatabase().subscription(for: subscriptionID) as? CKDatabaseSubscription,
               existing.notificationInfo?.shouldSendContentAvailable == true {
                guard isCurrentGeneration(generation) else { return }
                logger.debug("CloudKit database subscription already configured")
                return
            }

            guard isCurrentGeneration(generation) else { return }
            _ = try await cloudDatabase().save(subscription)
            guard isCurrentGeneration(generation) else { return }
            logger.info("Subscribed to database changes")
        } catch {
            logger.error("Failed to subscribe to database changes: \(error.localizedDescription)")
        }
    }
}
