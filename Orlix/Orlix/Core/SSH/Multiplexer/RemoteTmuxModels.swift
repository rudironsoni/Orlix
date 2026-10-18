import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package struct RemoteTmuxSession: Hashable, Sendable {
    package let name: String
    package let attachedClients: Int
    package let windowCount: Int
    package let ownership: RemoteSessionOwnership
}

nonisolated package struct RemoteTmuxBackend: Hashable, Sendable {
    package enum Variant: Hashable, Sendable {
        case unixTmux
        case windowsPsmux(
            shellFamily: RemoteShellFamily,
            powerShellExecutable: String?
        )
    }

    static package let unixTmux = Self(
        variant: .unixTmux,
        executablePath: "tmux",
        rawVersion: ""
    )

    package let variant: Variant
    package let executablePath: String
    package let rawVersion: String

    static package func unixTmux(executablePath: String, rawVersion: String) -> Self {
        Self(
            variant: .unixTmux,
            executablePath: executablePath,
            rawVersion: rawVersion
        )
    }

    static package func windowsPsmux(
        commandName: String,
        shellFamily: RemoteShellFamily,
        powerShellExecutable: String?,
        executablePath: String? = nil,
        rawVersion: String = ""
    ) -> Self {
        Self(
            variant: .windowsPsmux(
                shellFamily: shellFamily,
                powerShellExecutable: powerShellExecutable
            ),
            executablePath: executablePath ?? commandName,
            rawVersion: rawVersion
        )
    }

    package var isWindows: Bool {
        if case .windowsPsmux = variant {
            return true
        }
        return false
    }

    package var commandName: String {
        executablePath
    }

    package var shellFamily: RemoteShellFamily? {
        guard case .windowsPsmux(let shellFamily, _) = variant else { return nil }
        return shellFamily
    }

    package var powerShellExecutable: String? {
        guard case .windowsPsmux(_, let executable) = variant else { return nil }
        return executable
    }
}

nonisolated package enum RemoteTmuxProbeFailure: Hashable, Sendable {
    case cancelled
    case timeout
    case disconnected
    case transport(String)
    case channelOpenFailed
    case shellRequestFailed
    case invalidResponse
    case commandFailed(String)

    package var retryError: Error {
        switch self {
        case .cancelled:
            return CancellationError()
        case .timeout:
            return SSHError.timeout
        case .disconnected:
            return SSHError.notConnected
        case .transport(let message):
            return SSHError.socketError(message)
        case .channelOpenFailed:
            return SSHError.channelOpenFailed
        case .shellRequestFailed:
            return SSHError.shellRequestFailed
        case .invalidResponse:
            return SSHError.unknown("Unable to verify tmux availability")
        case .commandFailed(let message):
            return SSHError.unknown(message)
        }
    }

    static package func resolve(_ error: Error) -> Self {
        if error is CancellationError {
            return .cancelled
        }
        guard let sshError = error as? SSHError else {
            return .commandFailed(error.localizedDescription)
        }
        switch sshError {
        case .timeout:
            return .timeout
        case .notConnected:
            return .disconnected
        case .connectionFailed(let message), .socketError(let message):
            return .transport(message)
        case .channelOpenFailed:
            return .channelOpenFailed
        case .shellRequestFailed:
            return .shellRequestFailed
        default:
            return .commandFailed(sshError.localizedDescription)
        }
    }

    package var logDescription: String {
        switch self {
        case .cancelled: return "cancelled"
        case .timeout: return "timeout"
        case .disconnected: return "disconnected"
        case .transport: return "transport failure"
        case .channelOpenFailed: return "channel open failure"
        case .shellRequestFailed: return "shell request failure"
        case .invalidResponse: return "invalid response"
        case .commandFailed: return "command failure"
        }
    }
}

nonisolated package enum RemoteTmuxAvailability: Hashable, Sendable {
    case unsupported
    case available(RemoteTmuxBackend)
    case confirmedMissing
    case indeterminate(RemoteTmuxProbeFailure)

    package var backend: RemoteTmuxBackend? {
        guard case .available(let backend) = self else { return nil }
        return backend
    }

    package var logDescription: String {
        switch self {
        case .unsupported: return "unsupported"
        case .available(let backend): return "available (\(String(describing: backend)))"
        case .confirmedMissing: return "confirmed missing"
        case .indeterminate(let failure): return "indeterminate (\(failure.logDescription))"
        }
    }
}
