//
//  SubscriptionManagementRoute.swift
//  Orlix
//

import Foundation

nonisolated package enum SubscriptionManagementRoute: Equatable, Sendable {
    case nativeSheet
    case web(URL)

    static package func resolve(nativeSheetAvailable: Bool) -> Self {
        if nativeSheetAvailable {
            return .nativeSheet
        }

        return .web(URL(string: "https://apps.apple.com/account/subscriptions")!)
    }
}
