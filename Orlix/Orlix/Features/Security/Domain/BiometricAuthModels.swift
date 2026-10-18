import Foundation

nonisolated package enum BiometryKind: Equatable, Sendable {
    package case none
    package case touchID
    package case faceID
}

nonisolated package enum BiometricUnavailability: Equatable, Sendable {
    package case notEnrolled
    package case notAvailable
    package case locked
    package case passcodeNotSet
    package case system(message: String)
}

nonisolated package enum BiometricAvailability: Equatable, Sendable {
    package case available(BiometryKind)
    package case unavailable(BiometricUnavailability)
}

nonisolated package enum BiometricAuthenticationFailure: Error, Equatable, Sendable {
    package case cancelled
    package case notEnrolled
    package case notAvailable
    package case locked
    package case passcodeNotSet
    package case authenticationFailed
    package case system(message: String)

    package var isCancellation: Bool {
        if case .cancelled = self {
            return true
        }
        return false
    }
}

nonisolated package enum BiometricAuthenticationReason: Equatable, Sendable {
    package case enableAppLock(biometry: BiometryKind)
    package case disableAppLock
    package case unlockApp(biometry: BiometryKind)
    package case unlockServer(name: String)
    package case protectedServerAction(
        action: AppLockAuthenticationState.ProtectedServerAction,
        serverName: String
    )
}

nonisolated package enum AppLockFailure: Equatable, Sendable {
    package case biometryUnavailable(BiometricUnavailability)
    package case authentication(BiometricAuthenticationFailure)
}

nonisolated package enum AppLockState: Equatable, Sendable {
    package case unlocked(at: Date?)
    package case locked(generation: UUID)

    package var isLocked: Bool {
        if case .locked = self { return true }
        return false
    }

    package var lastUnlockAt: Date? {
        guard case .unlocked(let date) = self else { return nil }
        return date
    }
}

nonisolated package enum AppLockAuthenticationState: Equatable, Sendable {
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

    package case idle
    package case authenticating(attemptID: UUID, purpose: Purpose)

    package var isAuthenticating: Bool {
        if case .authenticating = self { return true }
        return false
    }

    package func accepts(attemptID: UUID, purpose: Purpose) -> Bool {
        self == .authenticating(attemptID: attemptID, purpose: purpose)
    }
}
