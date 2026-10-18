import Foundation

nonisolated package struct RemoteSessionBackendIdentifier: RawRepresentable, Codable, Hashable, Sendable {
    static package let herdr = Self(rawValue: "herdr")
    static package let tmux = Self(rawValue: "tmux")
    static package let zellij = Self(rawValue: "zellij")
    static package let zmx = Self(rawValue: "zmx")

    package let rawValue: String

    package init(rawValue: String) {
        self.rawValue = rawValue
    }
}

nonisolated package enum RemoteSessionStartupBehavior: String, Codable, CaseIterable, Identifiable, Sendable {
    package case createManaged
    package case ask
    package case plainShell

    package var id: String { rawValue }
    package init?(persistedRawValue: String) {
        switch persistedRawValue {
        case Self.createManaged.rawValue, "orlixManaged":
            self = .createManaged
        case Self.ask.rawValue, "askEveryTime":
            self = .ask
        case Self.plainShell.rawValue, "skipTmux":
            self = .plainShell
        default:
            return nil
        }
    }

    package init(from decoder: Decoder) throws {
        let container = try decoder.singleValueContainer()
        let rawValue = try container.decode(String.self)
        guard let value = Self(persistedRawValue: rawValue) else {
            throw DecodingError.dataCorruptedError(
                in: container,
                debugDescription: "Unknown remote session startup behavior"
            )
        }
        self = value
    }

    package func encode(to encoder: Encoder) throws {
        var container = encoder.singleValueContainer()
        try container.encode(rawValue)
    }
}

package extension RemoteSessionStartupBehavior {
    package var displayName: String {
        switch self {
        case .createManaged:
            return String(localized: "Create Orlix session")
        case .ask:
            return String(localized: "Ask every time")
        case .plainShell:
            return String(localized: "Use a normal shell")
        }
    }

    package var descriptionText: String {
        switch self {
        case .createManaged:
            return String(localized: "Create or reconnect to a Orlix-managed session.")
        case .ask:
            return String(localized: "Ask which session to use for each new tab or split.")
        case .plainShell:
            return String(localized: "Start a normal shell without remote session persistence.")
        }
    }
}
