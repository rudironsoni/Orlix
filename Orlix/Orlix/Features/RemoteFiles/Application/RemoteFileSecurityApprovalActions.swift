import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
import OrlixSSH
#endif
@MainActor
package struct RemoteFileSecurityApprovalActions {
    package typealias PendingRequest = @MainActor (
        _ error: Error,
        _ server: Server
    ) -> ServerSecurityApprovalRequest?
    package typealias Approve = @MainActor (_ request: ServerSecurityApprovalRequest) -> Bool
    package typealias Reject = @MainActor (_ request: ServerSecurityApprovalRequest) -> Void

    package let pendingRequest: PendingRequest
    package let approve: Approve
    package let reject: Reject

    static package let unavailable = RemoteFileSecurityApprovalActions(
        pendingRequest: { _, _ in nil },
        approve: { _ in false },
        reject: { _ in }
    )
}
