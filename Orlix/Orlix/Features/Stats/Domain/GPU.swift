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
}
