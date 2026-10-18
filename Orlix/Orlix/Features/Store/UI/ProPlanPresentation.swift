import Foundation

package extension ProPlanKind {
    package var title: String {
        switch self {
        case .monthly:
            return String(localized: "Monthly")
        case .yearly:
            return String(localized: "Yearly")
        case .lifetime:
            return String(localized: "Lifetime")
        }
    }

    package var detail: String {
        switch self {
        case .monthly:
            return String(localized: "Flexible access to every Pro feature.")
        case .yearly:
            return String(localized: "Best value for ongoing terminal work.")
        case .lifetime:
            return String(localized: "Pay once and keep Pro access forever.")
        }
    }

    package var badge: String? {
        self == .yearly ? String(localized: "Best value") : nil
    }
}

package struct ProPlanPresentation {
    package let plan: ProPlanKind
    package let displayPrice: String
    package let introductoryOfferState: ProPlanIntroductoryOfferState

    package init(
        plan: ProPlanKind,
        displayPrice: String,
        introductoryOfferState: ProPlanIntroductoryOfferState = .unavailable
    ) {
        self.plan = plan
        self.displayPrice = displayPrice
        self.introductoryOfferState = plan == .yearly ? introductoryOfferState : .unavailable
    }

    package var priceLine: String {
        if advertisesFreeTrial {
            return String(localized: "7 days free")
        }

        switch plan {
        case .monthly:
            return String(format: String(localized: "%@ per month"), displayPrice)
        case .yearly:
            return String(format: String(localized: "%@ per year"), displayPrice)
        case .lifetime:
            return String(format: String(localized: "%@ one time"), displayPrice)
        }
    }

    package var detail: String {
        advertisesFreeTrial
            ? String(format: String(localized: "Then %@ per year."), displayPrice)
            : plan.detail
    }

    package var purchaseButtonTitle: String {
        if plan == .lifetime {
            return String(format: String(localized: "Buy %@"), displayPrice)
        }
        if advertisesFreeTrial {
            return String(localized: "Start 7-Day Free Trial")
        }
        return String(format: String(localized: "Subscribe for %@"), displayPrice)
    }

    package var renewalDisclosure: String {
        if plan == .lifetime {
            return String(localized: "One-time purchase. No subscription renewal.")
        }
        if advertisesFreeTrial {
            return String(
                format: String(localized: "7 days free, then %@ per year. Auto-renews until canceled."),
                displayPrice
            )
        }
        return String(localized: "Auto-renews until canceled.")
    }

    package var planAccessibilityLabel: String {
        [plan.title, priceLine, detail].joined(separator: ". ")
    }

    package var purchaseButtonAccessibilityLabel: String {
        advertisesFreeTrial
            ? [purchaseButtonTitle, renewalDisclosure].joined(separator: ". ")
            : purchaseButtonTitle
    }

    private var advertisesFreeTrial: Bool {
        introductoryOfferState == .eligibleForSevenDayFreeTrial
    }
}
