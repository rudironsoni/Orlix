import Foundation

@MainActor
final package class UserDefaultsEngagementPersistence: EngagementPersisting {
    private enum Keys {
        static package let successfulConnectionCount = "engagement.successfulConnectionCount"
        static package let usageDayCount = "engagement.usageDayCount"
        static package let lastUsageDay = "engagement.lastUsageDay"
        static package let lastReviewRequest = "engagement.lastReviewRequest"
    }

    private let defaults: UserDefaults

    package init(defaults: UserDefaults) {
        self.defaults = defaults
    }

    package func loadHistory() -> EngagementHistory {
        EngagementHistory(
            successfulConnectionCount: defaults.integer(
                forKey: Keys.successfulConnectionCount
            ),
            usageDayCount: defaults.integer(forKey: Keys.usageDayCount),
            lastUsageDay: defaults.object(forKey: Keys.lastUsageDay) as? Date,
            lastReviewRequest: defaults.object(forKey: Keys.lastReviewRequest) as? Date
        )
    }

    package func saveHistory(_ history: EngagementHistory) {
        defaults.set(
            history.successfulConnectionCount,
            forKey: Keys.successfulConnectionCount
        )
        defaults.set(history.usageDayCount, forKey: Keys.usageDayCount)
        save(history.lastUsageDay, forKey: Keys.lastUsageDay)
        save(history.lastReviewRequest, forKey: Keys.lastReviewRequest)
    }

    private func save(_ date: Date?, forKey key: String) {
        if let date {
            defaults.set(date, forKey: key)
        } else {
            defaults.removeObject(forKey: key)
        }
    }
}
