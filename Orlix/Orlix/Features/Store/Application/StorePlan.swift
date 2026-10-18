import Foundation

nonisolated package enum ProPlanKind: String, Identifiable, Equatable, Sendable {
    case monthly
    case yearly
    case lifetime

    static package let displayOrder: [ProPlanKind] = [.monthly, .yearly, .lifetime]

    package var id: String { rawValue }
}

nonisolated package enum ProPlanIntroductoryOfferState: Equatable, Sendable {
    case unavailable
    case ineligible
    case eligibleForSevenDayFreeTrial
}
