import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package struct ServerStatsPresentationSnapshot: Sendable {
    package let stats: ServerStats
    package let cpuHistory: [StatsPoint]
    package let memoryHistory: [StatsPoint]
    package let gpuHistories: [String: [StatsPoint]]
    package let networkRxHistory: [StatsPoint]
    package let networkTxHistory: [StatsPoint]
    package let dockerCPUHistory: [StatsPoint]
    package let dockerMemoryHistory: [StatsPoint]
}
