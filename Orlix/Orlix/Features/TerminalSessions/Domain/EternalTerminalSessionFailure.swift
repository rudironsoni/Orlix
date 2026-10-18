import Foundation

nonisolated package enum EternalTerminalSessionFailure: Error, Hashable, Sendable {
    case bootstrapSSH
    case bootstrapResponse(String)
    case malformedBootstrapCredentials
    case resumeState(message: String, discardStoredState: Bool)
    case transport
    case invalidKey
    case protocolMismatch
    case disconnectedBufferFull
    case connectionInProgress
    case connectionClosed
    case applicationSuspended
    case sessionUnrecoverable
    case client
    case unknown

    package func userFacingMessage(host: String, port: Int) -> String {
        switch self {
        case .bootstrapSSH:
            return String(localized: "Eternal Terminal could not start through SSH. Verify the SSH credentials and that etterminal is installed on the host.")
        case .bootstrapResponse(let excerpt):
            let excerpt = excerpt.trimmingCharacters(in: .whitespacesAndNewlines)
            if excerpt.contains("VVTERM_ET_UNSUPPORTED_NATIVE_WINDOWS") {
                return String(localized: "Eternal Terminal does not run as a native Windows PowerShell or Command Prompt service. Configure this SSH connection to open inside WSL with Eternal Terminal installed, or use SSH with psmux instead.")
            }
            if excerpt.contains("VVTERM_ET_REQUIRES_POSIX_SHELL") {
                return String(localized: "Eternal Terminal requires a POSIX login shell with /bin/sh. Configure this SSH connection to open a supported Linux, macOS, BSD, or WSL environment, then try again.")
            }
            if excerpt.localizedCaseInsensitiveContains("communicating with et daemon") {
                return String(localized: "Eternal Terminal is installed, but its server daemon is not running or uses a different socket. On Linux, run “sudo systemctl enable --now et”. On macOS with Homebrew, run “brew services start et”. Then try again. If it still fails, ensure etterminal and etserver use the same server FIFO.")
            }
            guard !excerpt.isEmpty else {
                return String(localized: "etterminal did not return valid connection details. Verify the Eternal Terminal installation on the host.")
            }
            return String(
                format: String(localized: "etterminal did not return valid connection details. Host response: %@"),
                excerpt
            )
        case .malformedBootstrapCredentials:
            return String(localized: "etterminal returned malformed connection details. Update Eternal Terminal on the host and try again.")
        case .resumeState(let message, _):
            return message
        case .transport:
            return String(
                format: String(localized: "Could not reach etserver at %@:%d. Verify etserver is running and TCP port %d is open."),
                host,
                port,
                port
            )
        case .invalidKey:
            return String(localized: "etserver rejected the session key. Reconnect to start a new Eternal Terminal session.")
        case .protocolMismatch:
            return String(localized: "The Eternal Terminal client and server protocol versions do not match. Update Eternal Terminal on the host.")
        case .disconnectedBufferFull:
            return String(localized: "Eternal Terminal could not buffer more input while offline. Reconnect and try again.")
        case .connectionInProgress:
            return String(localized: "An Eternal Terminal connection is already starting.")
        case .connectionClosed:
            return String(localized: "The Eternal Terminal session closed. Reconnect to start a new session.")
        case .applicationSuspended:
            return String(localized: "Eternal Terminal input is paused while Orlix is in the background.")
        case .sessionUnrecoverable:
            return String(localized: "The Eternal Terminal session can no longer recover. Reconnect to start a new session.")
        case .client:
            return String(localized: "Eternal Terminal could not establish the session. Verify the server installation and try again.")
        case .unknown:
            return String(localized: "Eternal Terminal could not connect. Verify etserver is running and the configured ET port is reachable.")
        }
    }
}
