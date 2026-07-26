import Foundation
#if os(iOS)
import UIKit
#endif

/// Compatibility surface for Orlix feature call sites.
///
/// Orlix does not send Orlix's Umami analytics. Product telemetry is owned by
/// `OrlixTelemetry`, is separately configured, and remains disabled unless the
/// Orlix build explicitly enables it.
@MainActor
final class AnalyticsTracker {
    static let shared = AnalyticsTracker()

    static let enabledKey = "Orlix.analytics.enabled"

    private let defaults = UserDefaults.standard
    private var hasTrackedLaunch = false

    private init() {
        defaults.register(defaults: [Self.enabledKey: false])
    }

    var isEnabled: Bool {
        defaults.bool(forKey: Self.enabledKey)
    }

    func trackAppLaunched(isPro: Bool) {
        guard !hasTrackedLaunch else { return }
        hasTrackedLaunch = true
        send(.appLaunched, properties: ["pro": String(isPro)])
    }

    func trackConnectionSucceeded(transport: String) {
        send(.connectionSucceeded, properties: ["transport": transport])
    }

    func trackPaywallViewed(source: String) {
        send(.paywallViewed, properties: ["source": source])
    }

    func trackPaywallCTATapped(source: String, productId: String) {
        send(.paywallCTATapped, source: source, productId: productId)
    }

    func trackPurchaseStarted(source: String, productId: String) {
        send(.purchaseStarted, source: source, productId: productId)
    }

    func trackPurchase(source: String, productId: String) {
        send(.purchased, source: source, productId: productId)
    }

    func trackPurchaseSucceeded(source: String, productId: String) {
        send(.purchaseSucceeded, source: source, productId: productId)
    }

    func trackPurchaseCancelled(source: String, productId: String) {
        send(.purchaseCancelled, source: source, productId: productId)
    }

    func trackPurchasePending(source: String, productId: String) {
        send(.purchasePending, source: source, productId: productId)
    }

    func trackPurchaseFailed(source: String, productId: String, reason: String) {
        send(.purchaseFailed, properties: ["source": source, "product": productId, "reason": reason])
    }

    func trackLimitHit(source: String, generation: String, current: Int, limit: Int) {
        send(.limitHit, properties: [
            "source": source,
            "generation": generation,
            "current": String(current),
            "limit": String(limit),
        ])
    }

    func trackFreePlanGenerationAssigned(generation: String, serverCount: Int, reason: String) {
        send(.freePlanGenerationAssigned, properties: [
            "generation": generation,
            "server_count": String(serverCount),
            "reason": reason,
        ])
    }

    func trackWelcomeCompleted() { send(.welcomeCompleted) }
    func trackCustomActionCreated(kind: String) { send(.customActionCreated, properties: ["kind": kind]) }
    func trackSplitPaneCreated() { send(.splitPaneCreated) }
    func trackReviewPromptRequested() { send(.reviewPromptRequested) }
    func trackAnalyticsDisabled() { send(.analyticsDisabled) }

    private func send(_ event: OrlixAnalyticsEvent, properties: [String: String] = [:]) {
        guard isEnabled else { return }
        var properties = properties
        properties["platform"] = Self.platform
        OrlixTelemetry.shared.track(event, properties: properties)
    }

    private func send(_ event: OrlixAnalyticsEvent, source: String, productId: String) {
        send(event, properties: ["source": source, "product": productId])
    }

    private static let platform: String = {
        #if os(macOS)
        return "macos"
        #else
        return UIDevice.current.userInterfaceIdiom == .pad ? "ipados" : "ios"
        #endif
    }()
}
