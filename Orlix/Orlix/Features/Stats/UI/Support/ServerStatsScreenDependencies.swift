import Foundation

#if ORLIX_MODULAR_APP
import OrlixSSH
#endif
nonisolated enum ServerStatsSecurityApprovalOutcome: Equatable, Sendable {
    case approved
    case failed(ServerSecurityApprovalError)
}

/// Security effects required by the stats approval presentation.
///
/// The app composition root supplies the live authorization, credential, and
/// known-host implementations. Stats UI only owns presentation and retry flow.
@MainActor
package struct ServerStatsSecurityApprovalActions {
    package typealias Approve = @MainActor (
        _ request: ServerSecurityApprovalRequest
    ) async -> ServerStatsSecurityApprovalOutcome
    package typealias Reject = @MainActor (_ request: ServerSecurityApprovalRequest) -> Void

    package let approve: Approve
    package let reject: Reject
}

/// Stable app-owned dependencies for Stats screens.
@MainActor
package struct ServerStatsScreenDependencies {
    package let runtimeStore: ServerStatsRuntimeStore
    package let preferencesStore: PreferencesStore
    package let volumeVisibilityStore: ServerVolumeVisibilityStore
    package let securityApprovalActions: ServerStatsSecurityApprovalActions
}
