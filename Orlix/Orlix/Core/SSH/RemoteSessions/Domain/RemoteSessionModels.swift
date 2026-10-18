import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package struct RemoteSessionBackendMetadata: Hashable, Sendable {
    package enum Installation: Hashable, Sendable {
        case automatic
        case documentation(URL)
    }

    package let identifier: RemoteSessionBackendIdentifier
    package let displayName: String
    package let installation: Installation
    package let managedStartupCommandSupport: ManagedStartupCommandSupport
}

nonisolated package enum ManagedStartupCommandSupport: Hashable, Sendable {
    case supported
    case unsupported
}

nonisolated package struct RemoteSessionThemeStyle: Hashable, Sendable {
    package let name: String
    package let modeStyle: String
}

nonisolated package struct RemoteSessionSemanticVersion: Codable, Comparable, Hashable, Sendable {
    package let major: Int
    package let minor: Int
    package let patch: Int

    package init(major: Int, minor: Int, patch: Int) {
        self.major = max(0, major)
        self.minor = max(0, minor)
        self.patch = max(0, patch)
    }

    package init?(_ rawValue: String) {
        let trimmed = rawValue.trimmingCharacters(in: .whitespacesAndNewlines)
        guard let start = trimmed.firstIndex(where: \.isNumber) else { return nil }
        let versionPrefix = trimmed[start...].prefix { $0.isNumber || $0 == "." }
        let components = versionPrefix.split(separator: ".", omittingEmptySubsequences: false)
        guard (2...3).contains(components.count),
              let major = Int(components[0]),
              let minor = Int(components[1]),
              let patch = components.count == 3 ? Int(components[2]) : 0,
              major >= 0, minor >= 0, patch >= 0 else {
            return nil
        }
        self.init(major: major, minor: minor, patch: patch)
    }

    package static func < (lhs: Self, rhs: Self) -> Bool {
        (lhs.major, lhs.minor, lhs.patch) < (rhs.major, rhs.minor, rhs.patch)
    }
}

nonisolated package struct RemoteSessionExecutable: Codable, Hashable, Sendable {
    package enum ValidationError: Error, Equatable, Sendable {
        case empty
        case notAbsolute
        case tooLong
        case containsControlCharacters
    }

    static package let maximumLength = 4_096

    private enum CodingKeys: String, CodingKey {
        case path
    }

    package let path: String

    package init(validating path: String) throws {
        guard !path.isEmpty else { throw ValidationError.empty }
        guard path.utf8.count <= Self.maximumLength else { throw ValidationError.tooLong }
        guard path.rangeOfCharacter(from: .controlCharacters) == nil else {
            throw ValidationError.containsControlCharacters
        }
        let isPOSIXAbsolute = path.hasPrefix("/")
        let isWindowsDriveAbsolute: Bool
        if path.count >= 3 {
            let drive = path[path.startIndex]
            let colon = path[path.index(path.startIndex, offsetBy: 1)]
            let separator = path[path.index(path.startIndex, offsetBy: 2)]
            isWindowsDriveAbsolute = drive.isLetter
                && colon == ":"
                && (separator == "\\" || separator == "/")
        } else {
            isWindowsDriveAbsolute = false
        }
        let isWindowsUNC = path.hasPrefix("\\\\")
        guard isPOSIXAbsolute || isWindowsDriveAbsolute || isWindowsUNC else {
            throw ValidationError.notAbsolute
        }
        self.path = path
    }

    package init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        let path = try container.decode(String.self, forKey: .path)
        do {
            try self.init(validating: path)
        } catch {
            throw DecodingError.dataCorruptedError(
                forKey: .path,
                in: container,
                debugDescription: "Invalid remote session executable path"
            )
        }
    }

    package func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(path, forKey: .path)
    }
}

nonisolated package struct RemoteSessionProbe: Codable, Hashable, Sendable {
    package let backendIdentifier: RemoteSessionBackendIdentifier
    package let executable: RemoteSessionExecutable
    package let implementationVariant: String
    package let rawVersion: String
    package let semanticVersion: RemoteSessionSemanticVersion?
    package let shellFamily: RemoteShellFamily
    package let shellExecutable: String?
}

nonisolated package enum RemoteSessionProbeFailure: Hashable, Sendable {
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
            CancellationError()
        case .timeout:
            SSHError.timeout
        case .disconnected:
            SSHError.notConnected
        case .transport(let message):
            SSHError.socketError(message)
        case .channelOpenFailed:
            SSHError.channelOpenFailed
        case .shellRequestFailed:
            SSHError.shellRequestFailed
        case .invalidResponse:
            SSHError.unknown("Unable to verify the remote session backend")
        case .commandFailed(let message):
            SSHError.unknown(message)
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
        case .cancelled: "cancelled"
        case .timeout: "timeout"
        case .disconnected: "disconnected"
        case .transport: "transport failure"
        case .channelOpenFailed: "channel open failure"
        case .shellRequestFailed: "shell request failure"
        case .invalidResponse: "invalid response"
        case .commandFailed: "command failure"
        }
    }
}

nonisolated package enum RemoteSessionAvailability: Hashable, Sendable {
    case unsupportedEnvironment
    case available(RemoteSessionProbe)
    case confirmedMissing
    case incompatible(RemoteSessionProbe)
    case indeterminate(RemoteSessionProbeFailure)
}

nonisolated package struct RemoteSessionRuntime: Codable, Hashable, Sendable {
    package let probe: RemoteSessionProbe

    package var backendIdentifier: RemoteSessionBackendIdentifier {
        probe.backendIdentifier
    }
}

nonisolated package struct RemoteSessionIdentifier: Codable, Hashable, Sendable {
    package enum ValidationError: Error, Equatable, Sendable {
        case empty
        case tooLong
        case containsControlCharacters
    }

    static package let maximumRawValueLength = 128

    private enum CodingKeys: String, CodingKey {
        case backendIdentifier
        case rawValue
    }

    package let backendIdentifier: RemoteSessionBackendIdentifier
    package let rawValue: String

    package init(
        backendIdentifier: RemoteSessionBackendIdentifier,
        validating rawValue: String
    ) throws {
        guard !rawValue.isEmpty else { throw ValidationError.empty }
        guard rawValue.utf8.count <= Self.maximumRawValueLength else {
            throw ValidationError.tooLong
        }
        guard rawValue.rangeOfCharacter(from: .controlCharacters) == nil else {
            throw ValidationError.containsControlCharacters
        }
        self.backendIdentifier = backendIdentifier
        self.rawValue = rawValue
    }

    package init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        let backendIdentifier = try container.decode(
            RemoteSessionBackendIdentifier.self,
            forKey: .backendIdentifier
        )
        let rawValue = try container.decode(String.self, forKey: .rawValue)
        do {
            try self.init(backendIdentifier: backendIdentifier, validating: rawValue)
        } catch {
            throw DecodingError.dataCorruptedError(
                forKey: .rawValue,
                in: container,
                debugDescription: "Invalid remote session identifier"
            )
        }
    }

    package func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(backendIdentifier, forKey: .backendIdentifier)
        try container.encode(rawValue, forKey: .rawValue)
    }
}

nonisolated package enum RemoteSessionOwnership: String, Codable, Hashable, Sendable {
    case managed
    case external
}

nonisolated package struct RemoteSessionAttachment: Codable, Hashable, Sendable {
    package let identifier: RemoteSessionIdentifier
    package let ownership: RemoteSessionOwnership
}

nonisolated package struct RemoteSessionDescriptor: Codable, Hashable, Identifiable, Sendable {
    package let attachment: RemoteSessionAttachment
    package let attachedClientCount: Int?
    package let containerCount: Int?
    package let cleanupDisposition: RemoteSessionCleanupDisposition

    package var id: RemoteSessionIdentifier { attachment.identifier }
}

nonisolated package enum RemoteSessionListScope: Hashable, Sendable {
    case userVisible
    case managedCleanup
}

nonisolated package enum RemoteSessionLaunchIntent: Hashable, Sendable {
    case attach(RemoteSessionAttachment)
    case ensureManaged(
        identifier: RemoteSessionIdentifier,
        initialCommand: String?
    )

    package var attachment: RemoteSessionAttachment {
        switch self {
        case .attach(let attachment):
            return attachment
        case .ensureManaged(let identifier, _):
            return RemoteSessionAttachment(
                identifier: identifier,
                ownership: .managed
            )
        }
    }

}

nonisolated package struct RemoteSessionLaunchRequest: Hashable, Sendable {
    package let intent: RemoteSessionLaunchIntent
    package let workingDirectory: String
    package let lifecycleEnvelope: RemoteSessionLifecycleEnvelope
    package let transport: ShellTransport
    package let themeStyle: RemoteSessionThemeStyle

    package var attachment: RemoteSessionAttachment { intent.attachment }
}

nonisolated package struct RemoteSessionPresenceProbe: Codable, Hashable, Sendable {
    package let command: String
    package let existsMarker: String
    package let missingMarker: String

    package func sessionExists(in output: String) -> Bool? {
        let reportsExists = output.contains(existsMarker)
        let reportsMissing = output.contains(missingMarker)
        switch (reportsExists, reportsMissing) {
        case (true, false): return true
        case (false, true): return false
        case (false, false), (true, true): return nil
        }
    }
}

nonisolated package struct RemoteSessionBackendLaunchPlan: Hashable, Sendable {
    package let command: String
    package let presenceProbe: RemoteSessionPresenceProbe
}
