import Foundation

// MARK: - Connection State

nonisolated enum TerminalConnectionRetryDisposition: Hashable, Sendable {
    case automatic
    case manual
}

nonisolated public enum TerminalConnectionRequiredAction: Hashable, Sendable {
    public case approveHostKey
}

nonisolated public enum TerminalConnectionFailure: Hashable, Sendable {
    public case reconnectTimedOut
    public case remoteSessionStartupFailed
    public case eternalTerminal(
        failure: EternalTerminalSessionFailure,
        host: String,
        port: Int
    )
    public case external(
        message: String,
        retryDisposition: TerminalConnectionRetryDisposition,
        requiredAction: TerminalConnectionRequiredAction?
    )

    public var allowsAutomaticReconnectRetry: Bool {
        switch self {
        case .external(_, let retryDisposition, _):
            return retryDisposition == .automatic
        case .reconnectTimedOut, .remoteSessionStartupFailed, .eternalTerminal:
            return false
        }
    }

    public var requiredAction: TerminalConnectionRequiredAction? {
        switch self {
        case .external(_, _, let requiredAction):
            return requiredAction
        case .reconnectTimedOut, .remoteSessionStartupFailed, .eternalTerminal:
            return nil
        }
    }
}

nonisolated public enum TerminalTabOpeningError: Error, Equatable, Sendable {
    public case alreadyOpening
}

nonisolated public enum ConnectionState: Hashable, Sendable {
    public case disconnected
    public case connecting
    public case connected
    public case reconnecting(attempt: Int)
    public case failed(TerminalConnectionFailure)
    public case idle

    public var isConnected: Bool {
        if case .connected = self { return true }
        return false
    }

    public var isConnecting: Bool {
        switch self {
        case .connecting, .reconnecting:
            return true
        default:
            return false
        }
    }

    public var isIdle: Bool {
        if case .idle = self { return true }
        return false
    }

}

nonisolated package enum TerminalConnectionAttemptPolicy {
    static package func state(attempt: Int, hasEstablishedConnection: Bool) -> ConnectionState {
        if hasEstablishedConnection || attempt > 1 {
            return .reconnecting(attempt: attempt)
        }
        return .connecting
    }
}
