import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package struct StoreEntitlementResult: Equatable, Sendable {
    static package let free = StoreEntitlementResult(
        verifiedProductIds: [],
        subscriptionEntitlements: [],
        subscriptionStatus: nil
    )

    package let verifiedProductIds: Set<String>
    package let subscriptionEntitlements: [StoreSubscriptionEntitlement]
    package let subscriptionStatus: StoreSubscriptionStatus?
}

@MainActor
package protocol StoreClient: AnyObject, Sendable {
    func products(for identifiers: [String]) async throws -> [StoreProduct]
    func purchase(productId: String) async throws -> StorePurchaseResult
    func sync() async throws
    func entitlements(subscriptionProductIds: [String]) async -> StoreEntitlementResult
    func introductoryOfferState(productId: String) async -> ProPlanIntroductoryOfferState
    func transactionUpdates() -> AsyncStream<StoreTransactionUpdate>
}
