import Foundation

extension AnalyticsTracker: EngagementAnalytics {}

package extension EngagementTrackerDependencies {
    static package func live(
        defaults: UserDefaults,
        analytics: any EngagementAnalytics,
        now: @escaping () -> Date,
        calendar: Calendar,
        applicationIsActive: @escaping @MainActor @Sendable () -> Bool
    ) -> Self {
        EngagementTrackerDependencies(
            persistence: UserDefaultsEngagementPersistence(defaults: defaults),
            analytics: analytics,
            now: now,
            startOfDay: { calendar.startOfDay(for: $0) },
            applicationIsActive: applicationIsActive
        )
    }
}
