nonisolated enum ServerProRequirement: Equatable, Sendable {
    case unlimitedServers
    case unlimitedWorkspaces
    case moveIntoLockedWorkspace
}

nonisolated enum ServerMoveFailureReason: Equatable, Sendable {
    case destinationUnavailable
    case unavailable
}

nonisolated package enum OrlixError: Error, Equatable, Sendable {
    package case proRequired(ServerProRequirement)
    package case serverLocked(String)
    package case workspaceLocked(String)
    package case moveNotAllowed(ServerMoveFailureReason)
    package case connectionFailed(String)
    package case authenticationFailed
    package case authorizationRequired
    package case serverNotFound
    package case serverDataMutationRecoveryPending
    package case workspaceNotFound
    package case environmentNotFound
    package case environmentDeletionNotAllowed
    package case environmentFallbackUnavailable
    package case workspaceDeletionChanged
    package case timeout

    package var isLockedError: Bool {
        switch self {
        case .serverLocked, .workspaceLocked: return true
        default: return false
        }
    }
}
