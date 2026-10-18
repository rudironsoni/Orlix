import Foundation

nonisolated package struct StoreProduct: Identifiable, Equatable, Sendable {
    package let id: String
    package let displayName: String
    package let displayPrice: String
}

nonisolated package enum StorePurchaseResult: Equatable, Sendable {
    case verified(productId: String)
    case unverified(productId: String)
    case userCancelled
    case pending
    case unknown
}

nonisolated package enum StoreTransactionUpdate: Equatable, Sendable {
    case verified(productId: String)
    case unverified(productId: String)
}

nonisolated package enum StoreSubscriptionState: Equatable, Sendable {
    case subscribed
    case inGracePeriod
    case inBillingRetryPeriod
    case expired
    case revoked
    case unknown
}

nonisolated package struct StoreSubscriptionEntitlement: Equatable, Sendable {
    package let state: StoreSubscriptionState
    package let isVerified: Bool
}

nonisolated package enum StoreVerificationResult<Value: Equatable & Sendable>: Equatable, Sendable {
    case verified(Value)
    case unverified
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
    case idle
    case purchasing
    case purchased
    case failed(String)

    package static func == (lhs: PurchaseState, rhs: PurchaseState) -> Bool {
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
    case idle
    case restoring
    case restored(hasAccess: Bool)
    case failed(String)

    package static func == (lhs: RestoreState, rhs: RestoreState) -> Bool {
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
    case general
    case serverLimit = "server_limit"
    case workspaceLimit = "workspace_limit"
    case tabLimit = "tab_limit"
    case fileTabLimit = "file_tab_limit"
    case splitPane = "split_pane"
    case customEnvironment = "custom_environment"
    case snippetLimit = "snippet_limit"
    case welcome
    case settings
    case sidebarBanner = "sidebar_banner"
    case dockerStats = "docker_stats"
}

nonisolated package enum OrlixProducts {
    static package let proMonthly = "com.rudironsoni.orlix.pro.monthly"
    static package let proYearly = "com.rudironsoni.orlix.pro.yearly"
    static package let proLifetime = "com.rudironsoni.orlix.pro.lifetime"

    static package let subscriptionGroupId = "orlix_pro"
    static package let allProducts = [proMonthly, proYearly, proLifetime]
}

nonisolated package enum StoreError: Error {
    case verificationFailed
    case productNotFound
    case purchaseFailed(String)
}
