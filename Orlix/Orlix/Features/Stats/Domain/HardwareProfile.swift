import Foundation

nonisolated package struct HardwareProfile: Equatable, Sendable {
    package var hostname: String
    package var osInfo: String
    package var architecture: String
    package var kernelVersion: String
    package var cpuModel: String
    package var cpuVendor: String
    package var cpuCores: Int
    package var cpuThreads: Int
    package var memoryTotal: UInt64
    package var gpus: [GPUDevice]
    package var collectedAt: Date

    package init(
        hostname: String,
        osInfo: String,
        architecture: String,
        kernelVersion: String,
        cpuModel: String,
        cpuVendor: String,
        cpuCores: Int,
        cpuThreads: Int,
        memoryTotal: UInt64,
        gpus: [GPUDevice],
        collectedAt: Date
    ) {
        self.hostname = hostname
        self.osInfo = osInfo
        self.architecture = architecture
        self.kernelVersion = kernelVersion
        self.cpuModel = cpuModel
        self.cpuVendor = cpuVendor
        self.cpuCores = cpuCores
        self.cpuThreads = cpuThreads
        self.memoryTotal = memoryTotal
        self.gpus = gpus
        self.collectedAt = collectedAt
    }

    static package let empty = HardwareProfile(
        hostname: "",
        osInfo: "",
        architecture: "",
        kernelVersion: "",
        cpuModel: "",
        cpuVendor: "",
        cpuCores: 0,
        cpuThreads: 0,
        memoryTotal: 0,
        gpus: [],
        collectedAt: .distantPast
    )

    package var isEmpty: Bool {
        hostname.isEmpty
            && osInfo.isEmpty
            && architecture.isEmpty
            && kernelVersion.isEmpty
            && cpuModel.isEmpty
            && cpuVendor.isEmpty
            && cpuCores == 0
            && cpuThreads == 0
            && memoryTotal == 0
            && gpus.isEmpty
    }
}
