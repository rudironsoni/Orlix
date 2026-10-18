import Foundation
import StoreKit

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package enum AnalyticsMode: Equatable, Sendable {
    case disabled
    case production
}

nonisolated package enum AnalyticsEnvironmentPolicy {
    static package func mode(
        userEnabled: Bool,
        isDevelopmentBuild: Bool,
        isSimulator: Bool,
        isPreview: Bool,
        isRunningTests: Bool,
        isVerifiedProductionInstall: Bool
    ) -> AnalyticsMode {
        guard userEnabled,
              !isDevelopmentBuild,
              !isSimulator,
              !isPreview,
              !isRunningTests,
              isVerifiedProductionInstall else {
            return .disabled
        }
        return .production
    }

    static package func currentInstallMode() async -> AnalyticsMode {
        #if DEBUG || targetEnvironment(simulator)
        return .disabled
        #else
        let environment = Foundation.ProcessInfo.processInfo.environment
        let isPreview = environment["XCODE_RUNNING_FOR_PREVIEWS"] == "1"
        let isRunningTests = environment["XCTestConfigurationFilePath"] != nil
            || environment["XCTestBundlePath"] != nil

        guard !isPreview, !isRunningTests else {
            return .disabled
        }

        guard let appTransaction = try? await AppTransaction.shared else {
            return .disabled
        }
        let isVerifiedProductionInstall: Bool
        switch appTransaction {
        case .verified(let transaction):
            isVerifiedProductionInstall = transaction.environment == .production
        case .unverified:
            isVerifiedProductionInstall = false
        }

        return mode(
            userEnabled: true,
            isDevelopmentBuild: false,
            isSimulator: false,
            isPreview: isPreview,
            isRunningTests: isRunningTests,
            isVerifiedProductionInstall: isVerifiedProductionInstall
        )
        #endif
    }
}

package actor AnalyticsModeResolver {
    package typealias Provider = @Sendable () async -> AnalyticsMode

    private let provider: Provider
    private var resolution: Task<AnalyticsMode, Never>?

    package init(provider: @escaping Provider) {
        self.provider = provider
    }

    package func mode() async -> AnalyticsMode {
        if let resolution {
            return await resolution.value
        }

        let provider = provider
        let resolution = Task { await provider() }
        self.resolution = resolution
        return await resolution.value
    }
}
