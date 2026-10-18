import Foundation

nonisolated package enum BiometryKind: Equatable, Sendable {
    case none
    case touchID
    case faceID
}

nonisolated package enum BiometricUnavailability: Equatable, Sendable {
    case notEnrolled
    case notAvailable
    case locked
    case passcodeNotSet
    case system(message: String)
}

nonisolated package enum BiometricAvailability: Equatable, Sendable {
    case available(BiometryKind)
    case unavailable(BiometricUnavailability)
}

nonisolated package enum BiometricAuthenticationFailure: Error, Equatable, Sendable {
    case cancelled
    case notEnrolled
    case notAvailable
    case locked
    case passcodeNotSet
    case authenticationFailed
    case system(message: String)

    package var isCancellation: Bool {
        if case .cancelled = self {
            return true
        }
        return false
    }
}

nonisolated package enum BiometricAuthenticationReason: Equatable, Sendable {
    case enableAppLock(biometry: BiometryKind)
    case disableAppLock
    case unlockApp(biometry: BiometryKind)
    case unlockServer(name: String)
    case protectedServerAction(
        action: AppLockAuthenticationState.ProtectedServerAction,
        serverName: String
    )
}

nonisolated package enum AppLockFailure: Equatable, Sendable {
    case biometryUnavailable(BiometricUnavailability)
    case authentication(BiometricAuthenticationFailure)
}

nonisolated package enum AppLockState: Equatable, Sendable {
    case unlocked(at: Date?)
    case locked(generation: UUID)

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

    case idle
    case authenticating(attemptID: UUID, purpose: Purpose)

    package var isAuthenticating: Bool {
        if case .authenticating = self { return true }
        return false
    }

    package func accepts(attemptID: UUID, purpose: Purpose) -> Bool {
        self == .authenticating(attemptID: attemptID, purpose: purpose)
    }
}
