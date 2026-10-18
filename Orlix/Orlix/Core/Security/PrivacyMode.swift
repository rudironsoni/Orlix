import SwiftUI

nonisolated package enum PrivacyModeSettings {
    static package let enabledKey = "security.privacyModeEnabled"
}

nonisolated package enum AppContentProtectionPolicy {
    static package func shouldPrepareForSceneDeactivation(
        fullAppLockEnabled: Bool,
        privacyModeEnabled: Bool,
        isAppLocked: Bool
    ) -> Bool {
        shouldObscureContent(
            sceneIsActive: false,
            fullAppLockEnabled: fullAppLockEnabled,
            privacyModeEnabled: privacyModeEnabled,
            isAppLocked: isAppLocked
        )
    }

    static package func shouldObscureContent(
        sceneIsActive: Bool,
        fullAppLockEnabled: Bool,
        privacyModeEnabled: Bool,
        isAppLocked: Bool
    ) -> Bool {
        isAppLocked
            || (!sceneIsActive && (fullAppLockEnabled || privacyModeEnabled))
    }
}

nonisolated enum SensitiveContentMask {
    static let placeholder = "••••••••"

    static func value(_ value: String, privacyModeEnabled: Bool) -> String {
        privacyModeEnabled ? placeholder : value
    }
}

private struct PrivacyModeEnabledEnvironmentKey: EnvironmentKey {
    static let defaultValue = false
}

extension EnvironmentValues {
    var privacyModeEnabled: Bool {
        get { self[PrivacyModeEnabledEnvironmentKey.self] }
        set { self[PrivacyModeEnabledEnvironmentKey.self] = newValue }
    }
}

nonisolated package extension Server {
    package var displayAddressWithPort: String {
        "\(username)@\(host):\(port)"
    }

    package func visibleHost(privacyModeEnabled: Bool) -> String {
        SensitiveContentMask.value(host, privacyModeEnabled: privacyModeEnabled)
    }

    package func visibleAddress(privacyModeEnabled: Bool) -> String {
        privacyModeEnabled ? SensitiveContentMask.placeholder : displayAddressWithPort
    }
}

nonisolated package extension DiscoveredSSHHost {
    package var displayEndpoint: String {
        "\(host):\(port)"
    }

    package func visibleDisplayName(privacyModeEnabled: Bool) -> String {
        SensitiveContentMask.value(displayName, privacyModeEnabled: privacyModeEnabled)
    }

    package func visibleEndpoint(privacyModeEnabled: Bool) -> String {
        SensitiveContentMask.value(displayEndpoint, privacyModeEnabled: privacyModeEnabled)
    }
}
