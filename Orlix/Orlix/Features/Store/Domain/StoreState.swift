import Foundation

nonisolated package struct StoreProduct: Identifiable, Equatable, Sendable {
    package let id: String
    package let displayName: String
    package let displayPrice: String
}

nonisolated public enum StorePurchaseResult: Equatable, Sendable {
    public case verified(productId: String)
    public case unverified(productId: String)
    public case userCancelled
    public case pending
    public case unknown
}

nonisolated public enum StoreTransactionUpdate: Equatable, Sendable {
    public case verified(productId: String)
    public case unverified(productId: String)
}

nonisolated public enum StoreSubscriptionState: Equatable, Sendable {
    public case subscribed
    public case inGracePeriod
    public case inBillingRetryPeriod
    public case expired
    public case revoked
    public case unknown
}

nonisolated package struct StoreSubscriptionEntitlement: Equatable, Sendable {
    package let state: StoreSubscriptionState
    package let isVerified: Bool
}

nonisolated public enum StoreVerificationResult<Value: Equatable & Sendable>: Equatable, Sendable {
    public case verified(Value)
    public case unverified
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

nonisolated public enum PurchaseState: Equatable {
    public case idle
    public case purchasing
    public case purchased
    public case failed(String)

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

nonisolated public enum RestoreState: Equatable {
    public case idle
    public case restoring
    public case restored(hasAccess: Bool)
    public case failed(String)

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

nonisolated public enum PaywallSource: String, Equatable, Sendable {
    public case general
    public case serverLimit = "server_limit"
    public case workspaceLimit = "workspace_limit"
    public case tabLimit = "tab_limit"
    public case fileTabLimit = "file_tab_limit"
    public case splitPane = "split_pane"
    public case customEnvironment = "custom_environment"
    public case snippetLimit = "snippet_limit"
    public case welcome
    public case settings
    public case sidebarBanner = "sidebar_banner"
    public case dockerStats = "docker_stats"
}

nonisolated package enum OrlixProducts {
    static package let proMonthly = "com.rudironsoni.orlix.pro.monthly"
    static package let proYearly = "com.rudironsoni.orlix.pro.yearly"
    static package let proLifetime = "com.rudironsoni.orlix.pro.lifetime"

    static package let subscriptionGroupId = "orlix_pro"
    static package let allProducts = [proMonthly, proYearly, proLifetime]
}

nonisolated public enum StoreError: Error {
    public case verificationFailed
    public case productNotFound
    public case purchaseFailed(String)
}
