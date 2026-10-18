import Foundation

nonisolated package struct StoreProduct: Identifiable, Equatable, Sendable {
    package let id: String
    package let displayName: String
    package let displayPrice: String
}

nonisolated package enum StorePurchaseResult: Equatable, Sendable {
    package case verified(productId: String)
    package case unverified(productId: String)
    package case userCancelled
    package case pending
    package case unknown
}

nonisolated package enum StoreTransactionUpdate: Equatable, Sendable {
    package case verified(productId: String)
    package case unverified(productId: String)
}

nonisolated package enum StoreSubscriptionState: Equatable, Sendable {
    package case subscribed
    package case inGracePeriod
    package case inBillingRetryPeriod
    package case expired
    package case revoked
    package case unknown
}

nonisolated package struct StoreSubscriptionEntitlement: Equatable, Sendable {
    package let state: StoreSubscriptionState
    package let isVerified: Bool
}

nonisolated package enum StoreVerificationResult<Value: Equatable & Sendable>: Equatable, Sendable {
    package case verified(Value)
    package case unverified
}

nonisolated package struct StoreSubscriptionTransaction: Equatable, Sendable {
    package let productID: String
    package let expirationDate: Date?
}

nonisolated package struct StoreSubscriptionStatus: Equatable, Sendable {
    package let transaction: StoreVerificationResult<StoreSubscriptionTransaction>
    package let state: StoreSubscriptionState

    package var expirationDate: Date? {
        guard case .verified(let transaction) = transaction else { return nil }
        return transaction.expirationDate
    }
}

nonisolated package enum PurchaseState: Equatable {
    package case idle
    package case purchasing
    package case purchased
    package case failed(String)

    static func == (lhs: PurchaseState, rhs: PurchaseState) -> Bool {
        switch (lhs, rhs) {
        case (.idle, .idle), (.purchasing, .purchasing), (.purchased, .purchased):
            return true
        case (.failed(let lhsMessage), .failed(let rhsMessage)):
            return lhsMessage == rhsMessage
        default:
            return false
        }
    }
}

nonisolated package enum RestoreState: Equatable {
    package case idle
    package case restoring
    package case restored(hasAccess: Bool)
    package case failed(String)

    static func == (lhs: RestoreState, rhs: RestoreState) -> Bool {
        switch (lhs, rhs) {
        case (.idle, .idle), (.restoring, .restoring):
            return true
        case (.restored(let lhsHasAccess), .restored(let rhsHasAccess)):
            return lhsHasAccess == rhsHasAccess
        case (.failed(let lhsMessage), .failed(let rhsMessage)):
            return lhsMessage == rhsMessage
        default:
            return false
        }
    }
}

nonisolated package enum PaywallSource: String, Equatable, Sendable {
    package case general
    package case serverLimit = "server_limit"
    package case workspaceLimit = "workspace_limit"
    package case tabLimit = "tab_limit"
    package case fileTabLimit = "file_tab_limit"
    package case splitPane = "split_pane"
    package case customEnvironment = "custom_environment"
    package case snippetLimit = "snippet_limit"
    package case welcome
    package case settings
    package case sidebarBanner = "sidebar_banner"
    package case dockerStats = "docker_stats"
}

nonisolated package enum OrlixProducts {
    static package let proMonthly = "com.rudironsoni.orlix.pro.monthly"
    static package let proYearly = "com.rudironsoni.orlix.pro.yearly"
    static package let proLifetime = "com.rudironsoni.orlix.pro.lifetime"

    static package let subscriptionGroupId = "orlix_pro"
    static package let allProducts = [proMonthly, proYearly, proLifetime]
}

nonisolated package enum StoreError: Error {
    package case verificationFailed
    package case productNotFound
    package case purchaseFailed(String)
}
