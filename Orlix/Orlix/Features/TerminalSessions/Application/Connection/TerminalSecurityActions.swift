import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
import OrlixSSH
#endif
nonisolated package enum TerminalSecurityApprovalFailure: Equatable, Sendable {
    case expired
}

nonisolated package enum TerminalSecurityApprovalOutcome: Equatable, Sendable {
    case approved
    case failed(TerminalSecurityApprovalFailure)
}

/// App-owned security effects used by terminal session presentation.
@MainActor
package struct TerminalSecurityActions {
    package typealias LoadCredentials = @MainActor @Sendable (Server) throws -> ServerCredentials
    package typealias PendingHostKeyApproval = @MainActor @Sendable (
        _ server: Server
    ) -> ServerSecurityApprovalRequest?
    package typealias Approve = @MainActor @Sendable (
        _ request: ServerSecurityApprovalRequest,
        _ server: Server
    ) -> TerminalSecurityApprovalOutcome
    package typealias Reject = @MainActor @Sendable (
        _ request: ServerSecurityApprovalRequest
    ) -> Void

    package let loadCredentials: LoadCredentials
    package let pendingHostKeyApproval: PendingHostKeyApproval
    package let approve: Approve
    package let reject: Reject
}
