import XCTest
import StoreKit
import StoreKitTest
@testable import Orlix

@MainActor
final class StoreStateTests: XCTestCase {
    func testEntitlementSnapshotDerivesFreeAndProAccess() {
        let free = StoreEntitlementSnapshot.free
        let paid = StoreEntitlementSnapshot(
            accessState: .pro,
            hasLifetimeAccess: false,
            subscriptionStatus: nil
        )

        XCTAssertEqual(StoreEntitlementSnapshot.checking.accessState, .checking)
        XCTAssertFalse(free.hasStoreAccess)
        XCTAssertTrue(paid.hasStoreAccess)
    }

    func testPurchaseStateEqualityMatchesAssociatedMessage() {
        XCTAssertEqual(PurchaseState.failed("A"), PurchaseState.failed("A"))
        XCTAssertNotEqual(PurchaseState.failed("A"), PurchaseState.failed("B"))
    }

    func testRestoreStateEqualityMatchesAssociatedValues() {
        XCTAssertEqual(RestoreState.restored(hasAccess: true), RestoreState.restored(hasAccess: true))
        XCTAssertNotEqual(RestoreState.restored(hasAccess: true), RestoreState.restored(hasAccess: false))
    }

    func testStoreErrorFormatsPurchaseFailureMessage() {
        let error = StoreError.purchaseFailed("network")

        XCTAssertEqual(error.errorDescription, "Purchase failed: network")
    }

    func testEligibleYearlyPresentationAdvertisesSevenDayFreeTrial() {
        let presentation = ProPlanPresentation(
            plan: .yearly,
            displayPrice: "$24.99",
            introductoryOfferState: .eligibleForSevenDayFreeTrial
        )

        XCTAssertEqual(presentation.priceLine, "7 days free")
        XCTAssertEqual(presentation.detail, "Then $24.99 per year.")
        XCTAssertEqual(presentation.purchaseButtonTitle, "Start 7-Day Free Trial")
        XCTAssertEqual(
            presentation.renewalDisclosure,
            "7 days free, then $24.99 per year. Auto-renews until canceled."
        )
        XCTAssertTrue(presentation.planAccessibilityLabel.contains("$24.99"))
        XCTAssertTrue(presentation.purchaseButtonAccessibilityLabel.contains("7 days free"))
    }

    func testIneligibleYearlyPresentationKeepsStandardSubscriptionCopy() {
        let presentation = ProPlanPresentation(
            plan: .yearly,
            displayPrice: "$24.99",
            introductoryOfferState: .ineligible
        )

        XCTAssertEqual(presentation.priceLine, "$24.99 per year")
        XCTAssertEqual(presentation.detail, "Best value for ongoing terminal work.")
        XCTAssertEqual(presentation.purchaseButtonTitle, "Subscribe for $24.99")
        XCTAssertEqual(presentation.renewalDisclosure, "Auto-renews until canceled.")
    }

    func testUnavailableYearlyOfferMetadataKeepsStandardSubscriptionCopy() {
        let presentation = ProPlanPresentation(
            plan: .yearly,
            displayPrice: "$24.99",
            introductoryOfferState: .unavailable
        )

        XCTAssertEqual(presentation.priceLine, "$24.99 per year")
        XCTAssertEqual(presentation.purchaseButtonTitle, "Subscribe for $24.99")
        XCTAssertEqual(presentation.renewalDisclosure, "Auto-renews until canceled.")
    }

    func testTrialStateCannotLeakIntoOtherPlans() {
        let presentation = ProPlanPresentation(
            plan: .monthly,
            displayPrice: "$6.49",
            introductoryOfferState: .eligibleForSevenDayFreeTrial
        )

        XCTAssertEqual(presentation.introductoryOfferState, .unavailable)
        XCTAssertEqual(presentation.priceLine, "$6.49 per month")
        XCTAssertEqual(presentation.purchaseButtonTitle, "Subscribe for $6.49")
    }

    func testStoreKitConfigurationDefinesSevenDayYearlyTrial() throws {
        let configurationURL = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent()
            .deletingLastPathComponent()
            .deletingLastPathComponent()
            .deletingLastPathComponent()
            .appendingPathComponent("OrlixStoreKit.storekit")
        let data = try Data(contentsOf: configurationURL)
        let root = try XCTUnwrap(
            JSONSerialization.jsonObject(with: data) as? [String: Any]
        )
        let groups = try XCTUnwrap(root["subscriptionGroups"] as? [[String: Any]])
        let subscriptions = groups.flatMap { group in
            group["subscriptions"] as? [[String: Any]] ?? []
        }
        let yearly = try XCTUnwrap(subscriptions.first { subscription in
            subscription["productID"] as? String == OrlixProducts.proYearly
        })
        let offers = try XCTUnwrap(yearly["introductoryOffers"] as? [[String: Any]])
        let offer = try XCTUnwrap(offers.first)

        XCTAssertEqual(yearly["recurringSubscriptionPeriod"] as? String, "P1Y")
        XCTAssertEqual(offer["paymentMode"] as? String, "free")
        XCTAssertEqual(offer["subscriptionPeriod"] as? String, "P1W")
    }

    func testStoreKitConfigurationProvidesEligibleSevenDayYearlyTrial() async throws {
        let session = try SKTestSession(configurationFileNamed: "OrlixStoreKit")
        session.disableDialogs = true
        session.clearTransactions()

        let products = try await Product.products(for: [OrlixProducts.proYearly])
        let product = try XCTUnwrap(products.first)
        let subscription = try XCTUnwrap(product.subscription)
        let offer = try XCTUnwrap(subscription.introductoryOffer)

        XCTAssertEqual(offer.paymentMode, .freeTrial)
        switch offer.period.unit {
        case .day:
            XCTAssertEqual(offer.period.value, 7)
        case .week:
            XCTAssertEqual(offer.period.value, 1)
        default:
            XCTFail("The introductory offer is not seven days")
        }
        XCTAssertEqual(offer.periodCount, 1)
        let isEligible = await subscription.isEligibleForIntroOffer
        XCTAssertTrue(isEligible)
    }
}
