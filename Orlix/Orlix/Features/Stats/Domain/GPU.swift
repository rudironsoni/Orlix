import Foundation

nonisolated package enum GPUKind: String, Codable, Equatable, Sendable {
    case nvidia
    case amd
    case intel
    case apple
    case unknown
}

nonisolated package enum GPUSource: String, Codable, Equatable, Sendable {
    case nvidiaSMI
    case rocmSMI
    case intelGPU
    case systemProfiler
    case powerMetrics
    case wmi
    case unknown
}

nonisolated package struct GPUDevice: Identifiable, Codable, Equatable, Sendable {
    package var id: String
    package var name: String
    package var vendor: String
    package var kind: GPUKind
    package var driverVersion: String
    package var memoryTotal: UInt64
    package var source: GPUSource

    package init(
        id: String,
        name: String,
        vendor: String,
        kind: GPUKind,
        driverVersion: String,
        memoryTotal: UInt64,
        source: GPUSource
    ) {
        self.id = id
        self.name = name
        self.vendor = vendor
        self.kind = kind
        self.driverVersion = driverVersion
        self.memoryTotal = memoryTotal
        self.source = source
    }

    package var displayName: String {
        name.isEmpty ? vendor : name
    }
}

nonisolated package struct GPUSample: Identifiable, Codable, Equatable, Sendable {
    package var id: String { deviceID }
    package var deviceID: String
    package var utilizationPercent: Double?
    package var memoryUsed: UInt64?
    package var memoryTotal: UInt64?
    package var temperatureCelsius: Double?
    package var powerWatts: Double?
    package var processes: [GPUProcess]
    package var source: GPUSource
    package var timestamp: Date

    package init(
        deviceID: String,
        utilizationPercent: Double? = nil,
        memoryUsed: UInt64? = nil,
        memoryTotal: UInt64? = nil,
        temperatureCelsius: Double? = nil,
        powerWatts: Double? = nil,
        processes: [GPUProcess],
        source: GPUSource,
        timestamp: Date
    ) {
        self.deviceID = deviceID
        self.utilizationPercent = utilizationPercent
        self.memoryUsed = memoryUsed
        self.memoryTotal = memoryTotal
        self.temperatureCelsius = temperatureCelsius
        self.powerWatts = powerWatts
        self.processes = processes
        self.source = source
        self.timestamp = timestamp
    }

    package var memoryPercent: Double? {
        guard let memoryUsed,
              let memoryTotal,
              memoryTotal > 0 else {
            return nil
        }
        return min(max(Double(memoryUsed) / Double(memoryTotal) * 100, 0), 100)
    }
}

nonisolated package struct GPUProcess: Identifiable, Codable, Equatable, Sendable {
    package var id: Int { pid }
    package var pid: Int
    package var name: String
    package var memoryUsed: UInt64?
    package var utilizationPercent: Double?

    package init(
        pid: Int,
        name: String,
        memoryUsed: UInt64? = nil,
        utilizationPercent: Double? = nil
    ) {
        self.pid = pid
        self.name = name
        self.memoryUsed = memoryUsed
        self.utilizationPercent = utilizationPercent
    }
}
