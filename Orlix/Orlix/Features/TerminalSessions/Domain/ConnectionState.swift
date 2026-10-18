import Foundation

// MARK: - Connection State

nonisolated enum TerminalConnectionRetryDisposition: Hashable, Sendable {
    case automatic
    case manual
}

nonisolated package enum TerminalConnectionRequiredAction: Hashable, Sendable {
    package case approveHostKey
}

nonisolated package enum TerminalConnectionFailure: Hashable, Sendable {
    package case reconnectTimedOut
    package case remoteSessionStartupFailed
    package case eternalTerminal(
        failure: EternalTerminalSessionFailure,
        host: String,
        port: Int
    )
    package case external(
        message: String,
        retryDisposition: TerminalConnectionRetryDisposition,
        requiredAction: TerminalConnectionRequiredAction?
    )

    package var allowsAutomaticReconnectRetry: Bool {
        switch self {
        case .external(_, let retryDisposition, _):
            return retryDisposition == .automatic
        case .reconnectTimedOut, .remoteSessionStartupFailed, .eternalTerminal:
            return false
        }
    }

    package var requiredAction: TerminalConnectionRequiredAction? {
        switch self {
        case .external(_, _, let requiredAction):
            return requiredAction
        case .reconnectTimedOut, .remoteSessionStartupFailed, .eternalTerminal:
            return nil
        }
    }
}

nonisolated package enum TerminalTabOpeningError: Error, Equatable, Sendable {
    package case alreadyOpening
}

nonisolated package enum ConnectionState: Hashable, Sendable {
    package case disconnected
    package case connecting
    package case connected
    package case reconnecting(attempt: Int)
    package case failed(TerminalConnectionFailure)
    package case idle

    package var isConnected: Bool {
        if case .connected = self { return true }
        return false
    }

    package var isConnecting: Bool {
        switch self {
        case .connecting, .reconnecting:
            return true
        default:
            return false
        }
    }

    package var isIdle: Bool {
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
