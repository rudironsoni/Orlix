import Foundation

nonisolated public enum ImagePasteBehavior: String, CaseIterable, Identifiable, Sendable {
    public case disabled
    public case askOnce
    public case automatic

    static public let userDefaultsKey = "terminalImagePasteBehavior"

    public var id: String { rawValue }

    public var settingsTitle: String {
        switch self {
        case .automatic:
            return String(localized: "Automatically")
        case .askOnce:
            return String(localized: "Ask Before Upload")
        case .disabled:
            return String(localized: "Off")
        }
    }
}

nonisolated package struct RichClipboardSettings: Sendable {
    static package let maximumImageBytes = 50 * 1024 * 1024

    package let imagePasteBehavior: ImagePasteBehavior
    package let maximumImageBytes: Int

    package init(defaults: UserDefaults = .standard) {
        self.imagePasteBehavior = Self.resolvedImagePasteBehavior(defaults: defaults)
        self.maximumImageBytes = Self.maximumImageBytes
    }

    package var isImagePasteEnabled: Bool {
        imagePasteBehavior != .disabled
    }

    static package func resolvedImagePasteBehavior(defaults: UserDefaults = .standard) -> ImagePasteBehavior {
        if let rawValue = defaults.string(forKey: ImagePasteBehavior.userDefaultsKey),
           let behavior = ImagePasteBehavior(rawValue: rawValue) {
            return behavior
        }

        return .askOnce
    }

    static package func persistImagePasteBehavior(
        _ behavior: ImagePasteBehavior,
        defaults: UserDefaults = .standard
    ) {
        defaults.set(behavior.rawValue, forKey: ImagePasteBehavior.userDefaultsKey)
    }
}
