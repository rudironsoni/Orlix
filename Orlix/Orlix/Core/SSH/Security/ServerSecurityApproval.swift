import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated public enum ServerSecurityApprovalRequest: Identifiable, Equatable, Sendable {
    public case hostKey(KnownHostsManager.Challenge)

    public var id: String {
        switch self {
        case .hostKey(let challenge):
            "host-key:\(challenge.id.uuidString)"
        }
    }

    static public func detect(
        _ error: Error,
        host: String,
        port: Int,
        knownHosts: KnownHostsManager
    ) -> Self? {
        if let sshError = error as? SSHError,
           case .hostKeyApprovalRequired = sshError,
           let challenge = knownHosts.pendingChallenge(
               for: host,
               port: port
           ) {
            return .hostKey(challenge)
        }
        return nil
    }
}

nonisolated public enum ServerSecurityApprovalError: LocalizedError, Equatable, Sendable {
    public case cancelled
    public case expired
    public case unavailable

    public var errorDescription: String? {
        switch self {
        case .cancelled:
            String(localized: "Security approval was cancelled.")
        case .expired:
            String(localized: "Security approval expired. Try again.")
        case .unavailable:
            String(localized: "Security approval is no longer available. Try again.")
        }
    }
}
