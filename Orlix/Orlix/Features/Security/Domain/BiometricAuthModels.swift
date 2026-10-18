import Foundation

nonisolated public enum BiometryKind: Equatable, Sendable {
    public case none
    public case touchID
    public case faceID
}

nonisolated public enum BiometricUnavailability: Equatable, Sendable {
    public case notEnrolled
    public case notAvailable
    public case locked
    public case passcodeNotSet
    public case system(message: String)
}

nonisolated public enum BiometricAvailability: Equatable, Sendable {
    public case available(BiometryKind)
    public case unavailable(BiometricUnavailability)
}

nonisolated public enum BiometricAuthenticationFailure: Error, Equatable, Sendable {
    public case cancelled
    public case notEnrolled
    public case notAvailable
    public case locked
    public case passcodeNotSet
    public case authenticationFailed
    public case system(message: String)

    public var isCancellation: Bool {
        if case .cancelled = self {
            return true
        }
        return false
    }
}

nonisolated public enum BiometricAuthenticationReason: Equatable, Sendable {
    public case enableAppLock(biometry: BiometryKind)
    public case disableAppLock
    public case unlockApp(biometry: BiometryKind)
    public case unlockServer(name: String)
    public case protectedServerAction(
        action: AppLockAuthenticationState.ProtectedServerAction,
        serverName: String
    )
}

nonisolated public enum AppLockFailure: Equatable, Sendable {
    public case biometryUnavailable(BiometricUnavailability)
    public case authentication(BiometricAuthenticationFailure)
}

nonisolated public enum AppLockState: Equatable, Sendable {
    public case unlocked(at: Date?)
    public case locked(generation: UUID)

    public var isLocked: Bool {
        if case .locked = self { return true }
        return false
    }

    public var lastUnlockAt: Date? {
        guard case .unlocked(let date) = self else { return nil }
        return date
    }
}

nonisolated public enum AppLockAuthenticationState: Equatable, Sendable {
    nonisolated package enum ProtectedServerAction: Equatable, Sendable {
        case edit
        case testConnection
        case save
        case delete
    }

    nonisolated package enum Purpose: Equatable, Sendable {
        case enableFullAppLock
        case disableFullAppLock
        case unlockApp(lockGeneration: UUID)
        case unlockServer(serverID: UUID)
        case protectedServerAction(serverID: UUID, action: ProtectedServerAction)
    }

    public case idle
    public case authenticating(attemptID: UUID, purpose: Purpose)

    public var isAuthenticating: Bool {
        if case .authenticating = self { return true }
        return false
    }

    public func accepts(attemptID: UUID, purpose: Purpose) -> Bool {
        self == .authenticating(attemptID: attemptID, purpose: purpose)
    }
}
