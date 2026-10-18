import Foundation

nonisolated public enum FreePlanGeneration: String, Equatable, Sendable {
    public case legacyThreeServers = "legacy_three_servers"
    public case currentOneServer = "current_one_server"

    public var serverLimit: Int {
        switch self {
        case .legacyThreeServers: return FreeTierLimits.legacyMaxServers
        case .currentOneServer: return FreeTierLimits.currentMaxServers
        }
    }
}

nonisolated package struct ServerFreeTierPolicy: Equatable, Sendable {
    package let generation: FreePlanGeneration

    package var serverLimit: Int {
        generation.serverLimit
    }

    package var isLegacyPlan: Bool {
        generation == .legacyThreeServers
    }

    package func canAddServer(serverCount: Int, hasProAccess: Bool) -> Bool {
        hasProAccess || serverCount < serverLimit
    }

    package func canAddWorkspace(workspaceCount: Int, hasProAccess: Bool) -> Bool {
        hasProAccess || workspaceCount < FreeTierLimits.maxWorkspaces
    }

    package func unlockedServerIDs(servers: [Server], hasProAccess: Bool) -> Set<UUID> {
        if hasProAccess {
            return Set(servers.map(\.id))
        }
        let prioritized = servers.sorted { $0.createdAt < $1.createdAt }
        return Set(prioritized.prefix(serverLimit).map(\.id))
    }

    package func unlockedWorkspaceIDs(workspaces: [Workspace], hasProAccess: Bool) -> Set<UUID> {
        if hasProAccess {
            return Set(workspaces.map(\.id))
        }
        let prioritized = workspaces.sorted { $0.order < $1.order }
        return Set(prioritized.prefix(FreeTierLimits.maxWorkspaces).map(\.id))
    }

    package func lockedServerCount(serverCount: Int, hasProAccess: Bool) -> Int {
        hasProAccess ? 0 : max(0, serverCount - serverLimit)
    }

    package func lockedWorkspaceCount(workspaceCount: Int, hasProAccess: Bool) -> Int {
        hasProAccess ? 0 : max(0, workspaceCount - FreeTierLimits.maxWorkspaces)
    }
}
