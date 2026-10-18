import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
import OrlixSSH
#endif
extension TerminalTabOpeningError: LocalizedError {
    nonisolated package var errorDescription: String? {
        switch self {
        case .alreadyOpening:
            return String(
                format: String(localized: "Connection failed: %@"),
                String(localized: "A tab is already opening for this server.")
            )
        }
    }
}

nonisolated package enum TerminalConnectionFailurePresentation {
    static package func message(for failure: TerminalConnectionFailure) -> String {
        switch failure {
        case .reconnectTimedOut:
            return String(localized: "Connection timed out. Please retry.")
        case .remoteSessionStartupFailed:
            return String(localized: "Unable to start the remote session.")
        case .eternalTerminal(let failure, let host, let port):
            return failure.userFacingMessage(host: host, port: port)
        case .external(let message, _, _):
            return message
        }
    }

    static package func ansiSSHErrorData(for failure: TerminalConnectionFailure) -> Data? {
        let line = "\r\n\u{001B}[31mSSH Error: \(message(for: failure))\u{001B}[0m\r\n"
        return line.data(using: .utf8)
    }

}

package extension TerminalDisconnectReason {
    package var statusMessage: String? {
        switch self {
        case .transportInterrupted, .startupActionCompleted:
            return nil
        case .remoteSessionDetached:
            return String(localized: "The remote session is still running on the server.")
        case .externalRemoteSessionTerminated:
            return String(localized: "The remote session has ended.")
        }
    }
}

package enum TerminalConnectionStatusPresentation: Hashable {
    case hidden
    case connecting(serverName: String)
    case disconnected(message: String?)
    case failed(message: String, allowsHostKeyReplacement: Bool)

    static package func resolve(
        credentialLoadErrorMessage: String?,
        connectionState: ConnectionState,
        serverName: String,
        hasEstablishedConnection: Bool,
        automaticReconnectAllowed: Bool,
        isReconnectPreparationInFlight: Bool,
        isAwaitingRemoteSessionSelection: Bool,
        terminalExists: Bool,
        isReady: Bool,
        disconnectedMessage: String?
    ) -> Self {
        if let credentialLoadErrorMessage {
            return .failed(
                message: credentialLoadErrorMessage,
                allowsHostKeyReplacement: false
            )
        }

        if isAwaitingRemoteSessionSelection {
            return .hidden
        }

        if TerminalConnectionPresentationPolicy.usesReconnectBanner(
            connectionState: connectionState,
            hasEstablishedConnection: hasEstablishedConnection,
            automaticReconnectAllowed: automaticReconnectAllowed,
            isReconnectPreparationInFlight: isReconnectPreparationInFlight
        ) {
            return .hidden
        }

        switch connectionState {
        case .connecting:
            return .connecting(serverName: serverName)
        case .reconnecting:
            return .hidden
        case .disconnected:
            return .disconnected(message: disconnectedMessage)
        case .failed(let failure):
            return .failed(
                message: TerminalConnectionFailurePresentation.message(for: failure),
                allowsHostKeyReplacement: failure.requiredAction == .approveHostKey
            )
        case .connected, .idle:
            return !isReady && !terminalExists ? .connecting(serverName: serverName) : .hidden
        }
    }
}

package struct TerminalConnectionStatusPresentationIdentity: Hashable {
    package let presentation: TerminalConnectionStatusPresentation
    package let connectionAttemptID: UUID
}

package enum TerminalConnectionStatusDismissalPolicy {
    static package func identity(
        for presentation: TerminalConnectionStatusPresentation,
        connectionAttemptID: UUID
    ) -> TerminalConnectionStatusPresentationIdentity? {
        switch presentation {
        case .hidden, .connecting:
            return nil
        case .disconnected, .failed:
            return TerminalConnectionStatusPresentationIdentity(
                presentation: presentation,
                connectionAttemptID: connectionAttemptID
            )
        }
    }

    static package func shouldPresent(
        identity: TerminalConnectionStatusPresentationIdentity?,
        dismissedIdentity: TerminalConnectionStatusPresentationIdentity?,
        isActive: Bool
    ) -> Bool {
        isActive && identity != nil && identity != dismissedIdentity
    }

    static package func retainedDismissedIdentity(
        currentIdentity: TerminalConnectionStatusPresentationIdentity?,
        dismissedIdentity: TerminalConnectionStatusPresentationIdentity?
    ) -> TerminalConnectionStatusPresentationIdentity? {
        currentIdentity == dismissedIdentity ? dismissedIdentity : nil
    }
}

package enum TerminalConnectionPresentationPolicy {
    static package func usesReconnectBanner(
        connectionState: ConnectionState,
        hasEstablishedConnection: Bool,
        automaticReconnectAllowed: Bool,
        isReconnectPreparationInFlight: Bool
    ) -> Bool {
        if isReconnectPreparationInFlight {
            return true
        }

        if case .reconnecting = connectionState {
            return true
        }

        guard hasEstablishedConnection else { return false }

        if connectionState.isConnecting {
            return true
        }

        switch connectionState {
        case .disconnected, .failed:
            return automaticReconnectAllowed
        case .idle, .connecting, .reconnecting, .connected:
            return false
        }
    }
}

package enum TerminalConnectionWatchdogPolicy {
    static package func shouldMonitor(
        connectionState: ConnectionState,
        isReady: Bool,
        terminalExists: Bool,
        isAwaitingUserSelection: Bool
    ) -> Bool {
        guard !isAwaitingUserSelection else { return false }

        return connectionState.isConnecting
            || (connectionState.isConnected && !isReady && !terminalExists)
    }
}

package enum TerminalConnectionStartPolicy {
    static package func shouldStart(connectionState: ConnectionState) -> Bool {
        switch connectionState {
        case .connecting, .reconnecting, .connected:
            return true
        case .disconnected, .failed, .idle:
            return false
        }
    }
}

package enum TerminalSceneActivityPolicy {
    static package func isActive(
        environmentIsActive: Bool,
        windowSceneIsActive: Bool?
    ) -> Bool {
        windowSceneIsActive ?? environmentIsActive
    }
}

package enum RemoteSessionInstallPromptPolicy {
    static package func shouldPresent(
        for status: RemoteSessionStatus?,
        installation: RemoteSessionBackendMetadata.Installation?
    ) -> Bool {
        guard status == .missing else { return false }
        switch installation {
        case .automatic, .documentation:
            return true
        case nil:
            return false
        }
    }
}
