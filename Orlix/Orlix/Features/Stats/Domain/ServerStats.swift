import Foundation

nonisolated package struct ServerStats: Sendable {
    // System
    package var hostname: String = ""
    package var osInfo: String = ""
    package var hardware: HardwareProfile = .empty
    package var cpuCores: Int = 0

    // CPU detailed
    package var cpuUsage: Double = 0
    package var cpuUser: Double = 0
    package var cpuSystem: Double = 0
    package var cpuIowait: Double = 0
    package var cpuSteal: Double = 0
    package var cpuIdle: Double = 0
    package var cpuCoreSamples: [CPUCoreSample] = []

    // Memory detailed (in bytes)
    package var memoryTotal: UInt64 = 0
    package var memoryUsed: UInt64 = 0
    package var memoryFree: UInt64 = 0
    package var memoryCached: UInt64 = 0
    package var memoryBuffers: UInt64 = 0

    // Network (speed in bytes/sec, total in bytes)
    package var networkRxSpeed: UInt64 = 0
    package var networkTxSpeed: UInt64 = 0
    package var networkRxTotal: UInt64 = 0
    package var networkTxTotal: UInt64 = 0

    // Volumes
    package var volumes: [VolumeInfo] = []

    // System
    package var loadAverage: (Double, Double, Double) = (0, 0, 0)
    package var uptime: TimeInterval = 0
    package var processCount: Int = 0
    package var topProcesses: [ProcessInfo] = []
    package var gpuSamples: [GPUSample] = []
    package var docker = DockerStats()
    package var timestamp: Date = Date()

    package var memoryPercent: Double {
        guard memoryTotal > 0 else { return 0 }
        return Double(memoryUsed) / Double(memoryTotal) * 100
    }
}

nonisolated package struct CPUCoreSample: Identifiable, Sendable {
    package let identifier: String
    package let displayName: String
    package let usagePercent: Double
    package let userPercent: Double
    package let systemPercent: Double
    package let iowaitPercent: Double
    package let stealPercent: Double
    package let idlePercent: Double

    package var id: String { identifier }
}

nonisolated package struct VolumeInfo: Identifiable, Equatable, Sendable {
    package let identity: VolumeIdentity
    package let mountPoint: String
    package let source: String
    package let fileSystem: String
    package let stableIdentifier: String?
    package let kind: VolumeKind
    package let used: UInt64
    package let total: UInt64

    package var id: VolumeIdentity { identity }

    package init(
        identity: VolumeIdentity? = nil,
        platform: VolumeIdentity.Platform = .unknown,
        mountPoint: String,
        source: String = "",
        fileSystem: String = "",
        stableIdentifier: String? = nil,
        kind: VolumeKind? = nil,
        used: UInt64,
        total: UInt64
    ) {
        self.mountPoint = mountPoint
        self.source = source
        self.fileSystem = fileSystem
        self.stableIdentifier = stableIdentifier
        self.kind = kind ?? VolumeKind.classify(
            source: source,
            mountPoint: mountPoint,
            fileSystem: fileSystem
        )
        self.used = used
        self.total = total
        self.identity = identity ?? VolumeIdentity(
            platform: platform,
            stableIdentifier: stableIdentifier,
            source: source,
            mountPoint: mountPoint,
            fileSystem: fileSystem
        )
    }

    package var normalizationKey: String {
        switch identity {
        case .stable(let platform, _, let mountPoint),
             .fallback(let platform, _, let mountPoint, _):
            return "\(platform.rawValue)|\(VolumeIdentity.normalizedMountPoint(mountPoint, platform: platform))"
        }
    }

    package var percent: Double {
        guard total > 0 else { return 0 }
        return Double(used) / Double(total) * 100
    }
}

nonisolated package struct ProcessInfo: Identifiable, Sendable {
    package var id: Int { pid }
    package let pid: Int
    package let name: String
    /// Share of total logical CPU capacity used during the latest sample interval.
    /// A value of 100 means the process saturated the whole machine, not one core.
    package let cpuPercent: Double
    /// Resident physical memory divided by total visible physical memory.
    package let memoryPercent: Double
    /// Resident physical memory in bytes when the platform exposes it.
    package let memoryBytes: UInt64?
    package let user: String
    package let command: String

    package init(
        pid: Int,
        name: String,
        cpuPercent: Double,
        memoryPercent: Double,
        memoryBytes: UInt64? = nil,
        user: String = "",
        command: String = ""
    ) {
        self.pid = pid
        self.name = name
        self.cpuPercent = cpuPercent
        self.memoryPercent = memoryPercent
        self.memoryBytes = memoryBytes
        self.user = user
        self.command = command.isEmpty ? name : command
    }
}

nonisolated package struct StatsPoint: Identifiable, Sendable {
    package let timestamp: Date
    package let value: Double

    package var id: TimeInterval { timestamp.timeIntervalSince1970 }
}
