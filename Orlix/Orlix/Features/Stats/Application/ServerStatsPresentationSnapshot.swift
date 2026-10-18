import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated struct ServerStatsPresentationSnapshot: Sendable {
    let stats: ServerStats
    let cpuHistory: [StatsPoint]
    let memoryHistory: [StatsPoint]
    let gpuHistories: [String: [StatsPoint]]
    let networkRxHistory: [StatsPoint]
    let networkTxHistory: [StatsPoint]
    let dockerCPUHistory: [StatsPoint]
    let dockerMemoryHistory: [StatsPoint]
}
