nonisolated enum ServerProRequirement: Equatable, Sendable {
    case unlimitedServers
    case unlimitedWorkspaces
    case moveIntoLockedWorkspace
}

nonisolated enum ServerMoveFailureReason: Equatable, Sendable {
    case destinationUnavailable
    case unavailable
}

nonisolated public enum OrlixError: Error, Equatable, Sendable {
    public case proRequired(ServerProRequirement)
    public case serverLocked(String)
    public case workspaceLocked(String)
    public case moveNotAllowed(ServerMoveFailureReason)
    public case connectionFailed(String)
    public case authenticationFailed
    public case authorizationRequired
    public case serverNotFound
    public case serverDataMutationRecoveryPending
    public case workspaceNotFound
    public case environmentNotFound
    public case environmentDeletionNotAllowed
    public case environmentFallbackUnavailable
    public case workspaceDeletionChanged
    public case timeout

    public var isLockedError: Bool {
        switch self {
        case .serverLocked, .workspaceLocked: return true
        default: return false
        }
    }
}
