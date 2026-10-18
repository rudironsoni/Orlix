import Foundation

nonisolated enum SSHKeepAlivePolicy: Equatable, Sendable {
    case disabled
    case enabled(intervalSeconds: Int)
}

nonisolated package struct SSHRuntimeSettings: Equatable, Sendable {
    static package let keepAliveEnabledKey = "sshKeepAliveEnabled"
    static package let keepAliveIntervalKey = "sshKeepAliveInterval"
    static package let defaultKeepAliveIntervalSeconds = 30
    static package let keepAliveIntervalRange = 10...120

    package let keepAlive: SSHKeepAlivePolicy

    package init(keepAliveEnabled: Bool, keepAliveIntervalSeconds: Int) {
        guard keepAliveEnabled else {
            keepAlive = .disabled
            return
        }

        keepAlive = .enabled(
            intervalSeconds: min(
                max(keepAliveIntervalSeconds, Self.keepAliveIntervalRange.lowerBound),
                Self.keepAliveIntervalRange.upperBound
            )
        )
    }

    package init(defaults: UserDefaults) {
        let keepAliveEnabled = (defaults.object(forKey: Self.keepAliveEnabledKey) as? NSNumber)?
            .boolValue ?? true
        let storedInterval = (defaults.object(forKey: Self.keepAliveIntervalKey) as? NSNumber)?
            .intValue ?? Self.defaultKeepAliveIntervalSeconds

        self.init(
            keepAliveEnabled: keepAliveEnabled,
            keepAliveIntervalSeconds: storedInterval
        )
    }
}
