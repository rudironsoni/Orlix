import Foundation

nonisolated package struct EngagementHistory: Equatable, Sendable {
    static package let empty = EngagementHistory(
        successfulConnectionCount: 0,
        usageDayCount: 0,
        lastUsageDay: nil,
        lastReviewRequest: nil
    )

    package var successfulConnectionCount: Int
    package var usageDayCount: Int
    package var lastUsageDay: Date?
    package var lastReviewRequest: Date?
}

@MainActor
package protocol EngagementPersisting: AnyObject {
    func loadHistory() -> EngagementHistory
    func saveHistory(_ history: EngagementHistory)
}

@MainActor
package protocol EngagementAnalytics: AnyObject {
    func trackConnectionSucceeded(transport: String)
    func trackReviewPromptRequested()
}

@MainActor
package struct EngagementTrackerDependencies {
    package let persistence: any EngagementPersisting
    package let analytics: any EngagementAnalytics
    package let now: () -> Date
    package let startOfDay: (Date) -> Date
    package let applicationIsActive: @MainActor @Sendable () -> Bool
}
