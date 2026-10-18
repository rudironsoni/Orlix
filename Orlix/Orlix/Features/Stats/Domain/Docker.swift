import Foundation

nonisolated package struct DockerStats: Equatable, Sendable {
    package var availability: DockerAvailability = .unknown
    package var containers: [DockerContainer] = []
    package var timestamp: Date = Date()

    package var isAvailable: Bool {
        if case .available = availability {
            return true
        }
        return false
    }

    package var totalCount: Int {
        containers.count
    }

    package var runningCount: Int {
        containers.filter(\.isRunning).count
    }

    package var stoppedCount: Int {
        containers.filter { !$0.isRunning }.count
    }

    package var unhealthyCount: Int {
        containers.filter { $0.health == .unhealthy }.count
    }

    package var restartingCount: Int {
        containers.filter { $0.state == .restarting }.count
    }

    package var aggregateCPUPercent: Double {
        containers.reduce(0) { $0 + $1.cpuPercent }
    }

    package var memoryUsed: UInt64 {
        containers.reduce(0) { $0 + ($1.memoryUsed ?? 0) }
    }

    package var memoryLimit: UInt64 {
        containers.reduce(0) { $0 + ($1.memoryLimit ?? 0) }
    }

    package var memoryPercent: Double {
        guard memoryLimit > 0 else { return 0 }
        return Double(memoryUsed) / Double(memoryLimit) * 100
    }

    package var networkRx: UInt64 {
        containers.reduce(0) { $0 + ($1.networkRx ?? 0) }
    }

    package var networkTx: UInt64 {
        containers.reduce(0) { $0 + ($1.networkTx ?? 0) }
    }

    package var topContainers: [DockerContainer] {
        containers.sorted { lhs, rhs in
            if lhs.cpuPercent == rhs.cpuPercent {
                return (lhs.memoryUsed ?? 0) > (rhs.memoryUsed ?? 0)
            }
            return lhs.cpuPercent > rhs.cpuPercent
        }
    }
}

nonisolated package enum DockerAvailability: Equatable, Sendable {
    case unknown
    case available
    case commandMissing
    case daemonUnavailable(String)
    case permissionDenied(String)
    case unavailable(String)
}

nonisolated package struct DockerContainer: Identifiable, Equatable, Sendable {
    package let id: String
    package let name: String
    package let image: String
    package let command: String
    package let state: DockerContainerState
    package let status: String
    package let health: DockerHealthStatus
    package let createdAt: String
    package let runningFor: String
    package let ports: String
    package let cpuPercent: Double
    package let memoryPercent: Double
    package let memoryUsed: UInt64?
    package let memoryLimit: UInt64?
    package let networkRx: UInt64?
    package let networkTx: UInt64?
    package let blockRead: UInt64?
    package let blockWrite: UInt64?
    package let pids: Int?

    package var shortID: String {
        String(id.prefix(12))
    }

    package var displayName: String {
        name.isEmpty ? shortID : name
    }

    package var isRunning: Bool {
        state == .running
    }
}

nonisolated package enum DockerContainerState: String, Equatable, Sendable {
    case running
    case exited
    case paused
    case restarting
    case created
    case dead
    case removing
    case unknown

    package init(rawState: String) {
        self = DockerContainerState(rawValue: rawState.lowercased()) ?? .unknown
    }
}

nonisolated package enum DockerHealthStatus: String, Equatable, Sendable {
    case healthy
    case unhealthy
    case starting
    case none

    package init(statusText: String) {
        let lowercased = statusText.lowercased()
        if lowercased.contains("unhealthy") {
            self = .unhealthy
        } else if lowercased.contains("healthy") {
            self = .healthy
        } else if lowercased.contains("health: starting") || lowercased.contains("(starting)") {
            self = .starting
        } else {
            self = .none
        }
    }
}

nonisolated package enum DockerContainerAction: String, Equatable, Sendable {
    case start
    case stop
    case restart
}
