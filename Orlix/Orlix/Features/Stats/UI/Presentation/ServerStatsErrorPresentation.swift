import Foundation

nonisolated package extension ServerStatsCollectionState {
    package var errorMessage: String? {
        switch phase {
        case .approvalRequired:
            return String(localized: "SSH host key approval is required before authentication.")
        case .failed(let failure):
            return failure.errorMessage
        case .idle, .starting, .startingPaused, .collecting, .paused:
            return nil
        }
    }
}

nonisolated package extension ServerStatsCollectionFailure {
    package var errorMessage: String {
        switch self {
        case .securityApprovalCancelled:
            String(localized: "Security approval was cancelled.")
        case .securityApprovalExpired:
            String(localized: "Security approval expired. Try again.")
        case .securityApprovalUnavailable:
            String(localized: "Security approval is no longer available. Try again.")
        case .external(let detail):
            detail
        }
    }
}

@MainActor
package extension ServerStatsCollector {
    package var connectionError: String? {
        collectionState.errorMessage
    }
}

nonisolated extension ProcessControlError: LocalizedError {
    package var errorDescription: String? {
        switch self {
        case .notConnected:
            return String(localized: "Stats is not connected to the server.")
        case .protectedProcess:
            return String(localized: "This process cannot be killed from Stats.")
        }
    }
}
