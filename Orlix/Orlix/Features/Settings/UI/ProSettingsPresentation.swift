import Foundation

nonisolated enum ProSettingsUserState: Equatable, Sendable {
    case checking
    case free
    case pro
    case lifetime
    case subscription(plan: ProPlanKind, renewalDate: Date?)

    init(snapshot: StoreEntitlementSnapshot) {
        switch snapshot.accessState {
        case .checking:
            self = .checking
        case .free:
            self = .free
        case .pro:
            if snapshot.hasLifetimeAccess {
                self = .lifetime
                return
            }

            guard let status = snapshot.subscriptionStatus,
                  case .verified(let transaction) = status.transaction else {
                self = .pro
                return
            }

            switch transaction.productID {
            case OrlixProducts.proMonthly:
                self = .subscription(plan: .monthly, renewalDate: transaction.expirationDate)
            case OrlixProducts.proYearly:
                self = .subscription(plan: .yearly, renewalDate: transaction.expirationDate)
            case OrlixProducts.proLifetime:
                self = .lifetime
            default:
                self = .pro
            }
        }
    }

    var title: String {
        switch self {
        case .checking:
            String(localized: "Checking...")
        case .free:
            String(localized: "Free Tier")
        case .pro:
            String(localized: "Pro")
        case .lifetime:
            String(localized: "Pro Lifetime")
        case .subscription(let plan, _):
            switch plan {
            case .monthly:
                String(localized: "Pro Monthly")
            case .yearly:
                String(localized: "Pro Yearly")
            case .lifetime:
                String(localized: "Pro Lifetime")
            }
        }
    }

    var renewalDate: Date? {
        guard case .subscription(_, let renewalDate) = self else { return nil }
        return renewalDate
    }

    var primaryAction: ProSettingsPrimaryAction? {
        switch self {
        case .free:
            .viewPlans
        case .pro, .subscription:
            .manageSubscription
        case .checking, .lifetime:
            nil
        }
    }

    var hasProAccess: Bool {
        switch self {
        case .pro, .lifetime, .subscription:
            true
        case .checking, .free:
            false
        }
    }
}

nonisolated enum ProSettingsPrimaryAction: Equatable, Sendable {
    case viewPlans
    case manageSubscription

    var title: String {
        switch self {
        case .viewPlans:
            String(localized: "View Plans")
        case .manageSubscription:
            String(localized: "Manage Subscription")
        }
    }
}
