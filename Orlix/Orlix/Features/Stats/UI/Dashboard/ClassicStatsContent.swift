import SwiftUI

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package struct ClassicStatsContent: View {
    package let serverName: String
    package let stats: ServerStats
    package let visibleBlocks: [StatsPreferences.BlockID]
    package let surfaceStyle: ClassicStatsCardSurfaceStyle
    package let isDockerUnlocked: Bool
    package let showsCustomizationEntryPoint: Bool
    package let customizeAction: (() -> Void)?
    package let dockerUpgradeAction: (() -> Void)?
    package let terminateProcess: ((ProcessInfo) async throws -> Void)?
    package let loadProcesses: (() async throws -> [ProcessInfo])?
    package let loadDockerStats: (() async throws -> DockerStats)?
    package let performDockerAction: ((DockerContainerAction, DockerContainer) async throws -> DockerStats)?
    package let loadStorageHealth: ((VolumeInfo) async throws -> StorageHealthResult)?
    package let storageVolumes: [VolumeInfo]
    package let visibleStorageVolumes: [VolumeInfo]
    package let hiddenStorageVolumeIDs: Set<VolumeIdentity>
    package let setStorageVolumeVisibility: (VolumeInfo, Bool) -> Void
    package let setStorageVolumesVisibility: ([VolumeInfo], Bool) -> Void

    package var body: some View {
        LazyVStack(spacing: 16) {
            ForEach(visibleBlocks, id: \.self) { blockID in
                classicBlock(blockID)
            }

            if showsCustomizationEntryPoint, let customizeAction {
                ClassicStatsCustomizeCard(surfaceStyle: surfaceStyle, action: customizeAction)
            }
        }
    }

    @ViewBuilder
    private func classicBlock(_ blockID: StatsPreferences.BlockID) -> some View {
        switch blockID {
        case .system:
            ClassicServerHeaderCard(
                serverName: serverName,
                osInfo: stats.osInfo,
                surfaceStyle: surfaceStyle
            )
        case .cpu:
            ClassicCPUStatsCard(
                usage: stats.cpuUsage,
                user: stats.cpuUser,
                system: stats.cpuSystem,
                iowait: stats.cpuIowait,
                steal: stats.cpuSteal,
                idle: stats.cpuIdle,
                cores: stats.cpuCores,
                uptime: stats.uptime,
                loadAverage: stats.loadAverage,
                surfaceStyle: surfaceStyle
            )
        case .memory:
            ClassicMemoryStatsCard(
                used: stats.memoryUsed,
                free: stats.memoryFree,
                cached: stats.memoryCached,
                total: stats.memoryTotal,
                percent: stats.memoryPercent,
                surfaceStyle: surfaceStyle
            )
        case .gpu:
            if !stats.hardware.gpus.isEmpty || !stats.gpuSamples.isEmpty {
                ClassicGPUStatsCard(
                    device: stats.hardware.gpus.first,
                    sample: stats.gpuSamples.first,
                    surfaceStyle: surfaceStyle
                )
            }
        case .network:
            ClassicNetworkStatsCard(
                txSpeed: stats.networkTxSpeed,
                rxSpeed: stats.networkRxSpeed,
                txTotal: stats.networkTxTotal,
                rxTotal: stats.networkRxTotal,
                surfaceStyle: surfaceStyle
            )
        case .storage:
            ClassicVolumesCard(
                volumes: storageVolumes,
                visibleVolumes: visibleStorageVolumes,
                hiddenVolumeIDs: hiddenStorageVolumeIDs,
                surfaceStyle: surfaceStyle,
                loadStorageHealth: loadStorageHealth,
                setVolumeVisibility: setStorageVolumeVisibility,
                setVolumesVisibility: setStorageVolumesVisibility
            )
        case .processes:
            ClassicProcessesCard(
                processes: stats.topProcesses,
                surfaceStyle: surfaceStyle,
                terminateProcess: terminateProcess,
                loadProcesses: loadProcesses
            )
        case .docker:
            if isDockerUnlocked {
                ClassicDockerStatsCard(
                    docker: stats.docker,
                    surfaceStyle: surfaceStyle,
                    loadDockerStats: loadDockerStats,
                    performDockerAction: performDockerAction
                )
            } else if let dockerUpgradeAction {
                ClassicLockedDockerCard(surfaceStyle: surfaceStyle, action: dockerUpgradeAction)
            }
        }
    }
}

private struct ClassicStatsCustomizeCard: View {
    package let surfaceStyle: ClassicStatsCardSurfaceStyle
    package let action: () -> Void

    package var body: some View {
        Button(action: action) {
            HStack(spacing: 12) {
                Image(systemName: "slider.horizontal.3")
                    .font(.system(size: 18, weight: .semibold))
                    .foregroundStyle(Color.accentColor)
                    .frame(width: 36, height: 36)
                    .background(Color.accentColor.opacity(0.12), in: Circle())

                VStack(alignment: .leading, spacing: 2) {
                    Text("Customize Stats")
                        .font(.headline)
                        .foregroundStyle(.primary)
                    Text("Cards, order, visibility")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }

                Spacer(minLength: 10)

                Image(systemName: "chevron.right")
                    .font(.system(size: 14, weight: .bold))
                    .foregroundStyle(.secondary)
            }
            .padding(16)
            .frame(maxWidth: .infinity, minHeight: 76, alignment: .leading)
            .background(surfaceStyle.fill.opacity(0.45), in: RoundedRectangle(cornerRadius: 12, style: .continuous))
            .overlay {
                RoundedRectangle(cornerRadius: 12, style: .continuous)
                    .stroke(
                        style: StrokeStyle(
                            lineWidth: 1.5,
                            lineCap: .round,
                            lineJoin: .round,
                            dash: [6, 6]
                        )
                    )
                    .foregroundStyle(Color.secondary.opacity(0.55))
            }
        }
        .buttonStyle(.plain)
        .accessibilityLabel(Text("Customize Stats"))
    }
}

private struct ClassicServerHeaderCard: View, Equatable {
    package let serverName: String
    package let osInfo: String
    package let surfaceStyle: ClassicStatsCardSurfaceStyle

    package var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(serverName)
                .font(.title2)
                .fontWeight(.bold)

            if !osInfo.isEmpty {
                Text(osInfo)
                    .font(.subheadline)
                    .foregroundStyle(.secondary)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding()
        .classicStatsCardSurface(surfaceStyle)
    }
}

private struct ClassicCPUStatsCard: View, Equatable {
    package let usage: Double
    package let user: Double
    package let system: Double
    package let iowait: Double
    package let steal: Double
    package let idle: Double
    package let cores: Int
    package let uptime: TimeInterval
    package let loadAverage: (Double, Double, Double)
    package let surfaceStyle: ClassicStatsCardSurfaceStyle

    package static func == (lhs: ClassicCPUStatsCard, rhs: ClassicCPUStatsCard) -> Bool {
        lhs.usage == rhs.usage && lhs.user == rhs.user && lhs.system == rhs.system &&
        lhs.iowait == rhs.iowait && lhs.steal == rhs.steal && lhs.idle == rhs.idle &&
        lhs.cores == rhs.cores && lhs.uptime == rhs.uptime &&
        lhs.loadAverage.0 == rhs.loadAverage.0 && lhs.loadAverage.1 == rhs.loadAverage.1 &&
        lhs.loadAverage.2 == rhs.loadAverage.2 && lhs.surfaceStyle == rhs.surfaceStyle
    }

    package var body: some View {
        VStack(spacing: 12) {
            HStack(spacing: 16) {
                VStack(alignment: .leading, spacing: 8) {
                    HStack(spacing: 16) {
                        ClassicStatLabel(color: .pink, label: String(localized: "SYS"), value: String(format: String(localized: "%lld %%"), Int64(system)))
                        ClassicStatLabel(color: .green, label: String(localized: "USER"), value: String(format: String(localized: "%lld %%"), Int64(user)))
                    }
                    HStack(spacing: 16) {
                        ClassicStatLabel(color: .yellow, label: String(localized: "IOWAIT"), value: String(format: String(localized: "%lld %%"), Int64(iowait)))
                        ClassicStatLabel(color: .purple, label: String(localized: "STEAL"), value: String(format: String(localized: "%lld %%"), Int64(steal)))
                    }
                }

                Spacer()

                ZStack {
                    ClassicCircularGauge(value: usage / 100, color: cpuColor)
                    Text(String(format: String(localized: "%lld%%"), Int64(usage)))
                        .font(.system(size: 12, weight: .semibold, design: .monospaced))
                        .frame(width: 36)
                }
                .frame(width: 50, height: 50)
            }

            Divider()

            HStack(spacing: 0) {
                ClassicStatColumn(label: String(localized: "CORES"), value: "\(cores)")
                ClassicStatColumn(label: String(localized: "IDLE"), value: String(format: String(localized: "%lld %%"), Int64(idle)))
                ClassicStatColumn(label: String(localized: "UPTIME"), value: formatUptime(uptime))
                ClassicStatColumn(label: String(localized: "LOAD"), value: String(format: "%.1f,%.1f,%.1f", loadAverage.0, loadAverage.1, loadAverage.2))
            }
        }
        .padding()
        .classicStatsCardSurface(surfaceStyle)
    }

    private var cpuColor: Color {
        if usage > 90 { return .red }
        if usage > 70 { return .orange }
        return .green
    }

    private func formatUptime(_ seconds: TimeInterval) -> String {
        let days = Int(seconds) / 86400
        let hours = (Int(seconds) % 86400) / 3600
        if days > 0 { return String(format: String(localized: "%lld D"), days) }
        return String(format: String(localized: "%lld H"), hours)
    }
}

private struct ClassicMemoryStatsCard: View, Equatable {
    package let used: UInt64
    package let free: UInt64
    package let cached: UInt64
    package let total: UInt64
    package let percent: Double
    package let surfaceStyle: ClassicStatsCardSurfaceStyle

    package var body: some View {
        HStack(spacing: 16) {
            VStack(alignment: .leading, spacing: 8) {
                HStack(spacing: 16) {
                    ClassicStatLabel(color: .secondary, label: String(localized: "FREE_MEMORY"), value: formatBytes(free))
                    ClassicStatLabel(color: .green, label: String(localized: "USED"), value: formatBytes(used))
                }
                HStack(spacing: 16) {
                    ClassicStatLabel(color: .blue, label: String(localized: "CACHED"), value: formatBytes(cached))
                    ClassicStatLabel(color: .secondary, label: String(localized: "TOTAL"), value: formatBytes(total))
                }
            }

            Spacer()

            ZStack {
                ClassicCircularGauge(value: percent / 100, color: memoryColor)
                Text(String(format: String(localized: "%lld%%"), Int64(percent)))
                    .font(.system(size: 10, weight: .semibold, design: .monospaced))
                    .frame(width: 36)
            }
            .frame(width: 50, height: 50)
        }
        .padding()
        .classicStatsCardSurface(surfaceStyle)
    }

    private var memoryColor: Color {
        if percent > 90 { return .red }
        if percent > 70 { return .orange }
        return .green
    }

    private func formatBytes(_ bytes: UInt64) -> String {
        let gb = Double(bytes) / 1_073_741_824
        if gb >= 1 { return String(format: "%.1f G", gb) }
        let mb = Double(bytes) / 1_048_576
        return String(format: "%.0f M", mb)
    }
}

private struct ClassicGPUStatsCard: View, Equatable {
    package let device: GPUDevice?
    package let sample: GPUSample?
    package let surfaceStyle: ClassicStatsCardSurfaceStyle

    package var body: some View {
        HStack(spacing: 16) {
            VStack(alignment: .leading, spacing: 8) {
                HStack(spacing: 16) {
                    ClassicStatLabel(color: .green, label: String(localized: "UTIL"), value: utilizationLabel)
                    ClassicStatLabel(color: .blue, label: String(localized: "VRAM"), value: memoryLabel)
                }

                HStack(spacing: 16) {
                    ClassicStatLabel(color: .orange, label: String(localized: "TEMP"), value: temperatureLabel)
                    ClassicStatLabel(color: .yellow, label: String(localized: "POWER"), value: powerLabel)
                }

                if let displayName = device?.displayName, !displayName.isEmpty {
                    Text(displayName)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                        .lineLimit(1)
                        .truncationMode(.middle)
                }
            }

            Spacer()

            ZStack {
                ClassicCircularGauge(value: utilizationValue / 100, color: .green)
                Image(systemName: StatsIcon.gpu)
                    .font(.system(size: 17, weight: .semibold))
                    .foregroundStyle(.green)
            }
            .frame(width: 50, height: 50)
        }
        .padding()
        .classicStatsCardSurface(surfaceStyle)
    }

    private var utilizationValue: Double {
        sample?.utilizationPercent ?? 0
    }

    private var utilizationLabel: String {
        guard let value = sample?.utilizationPercent else { return "-" }
        return String(format: "%.0f %%", value)
    }

    private var memoryLabel: String {
        guard let used = sample?.memoryUsed else {
            if let total = sample?.memoryTotal ?? device?.memoryTotal, total > 0 {
                return formatBytes(total)
            }
            return "-"
        }

        let total = sample?.memoryTotal ?? device?.memoryTotal ?? 0
        if total > 0 {
            return String(format: "%@/%@", formatBytes(used), formatBytes(total))
        }
        return formatBytes(used)
    }

    private var temperatureLabel: String {
        guard let temperature = sample?.temperatureCelsius else { return "-" }
        return String(format: "%.0f C", temperature)
    }

    private var powerLabel: String {
        guard let power = sample?.powerWatts else { return "-" }
        return String(format: "%.0f W", power)
    }

    private func formatBytes(_ bytes: UInt64) -> String {
        let gb = Double(bytes) / 1_073_741_824
        if gb >= 1 { return String(format: "%.1f G", gb) }
        let mb = Double(bytes) / 1_048_576
        return String(format: "%.0f M", mb)
    }
}

private struct ClassicNetworkStatsCard: View, Equatable {
    package let txSpeed: UInt64
    package let rxSpeed: UInt64
    package let txTotal: UInt64
    package let rxTotal: UInt64
    package let surfaceStyle: ClassicStatsCardSurfaceStyle

    package var body: some View {
        HStack(spacing: 16) {
            VStack(alignment: .leading, spacing: 8) {
                HStack(spacing: 16) {
                    ClassicStatLabel(color: .green, label: String(localized: "↑/S"), value: formatSpeed(txSpeed))
                    ClassicStatLabel(color: .orange, label: String(localized: "↓/S"), value: formatSpeed(rxSpeed))
                }
                HStack(spacing: 16) {
                    ClassicStatLabel(color: .green, label: String(localized: "↑ TOTAL"), value: formatBytes(txTotal))
                    ClassicStatLabel(color: .orange, label: String(localized: "↓ TOTAL"), value: formatBytes(rxTotal))
                }
            }

            Spacer()

            ZStack {
                Circle()
                    .stroke(Color.orange.opacity(0.3), lineWidth: 4)
                    .frame(width: 50, height: 50)
                Circle()
                    .trim(from: 0, to: min(Double(rxSpeed) / 10_000_000, 1))
                    .stroke(Color.orange, style: StrokeStyle(lineWidth: 4, lineCap: .round))
                    .frame(width: 50, height: 50)
                    .rotationEffect(.degrees(-90))

                Circle()
                    .stroke(Color.green.opacity(0.3), lineWidth: 4)
                    .frame(width: 36, height: 36)
                Circle()
                    .trim(from: 0, to: min(Double(txSpeed) / 10_000_000, 1))
                    .stroke(Color.green, style: StrokeStyle(lineWidth: 4, lineCap: .round))
                    .frame(width: 36, height: 36)
                    .rotationEffect(.degrees(-90))
            }
        }
        .padding()
        .classicStatsCardSurface(surfaceStyle)
    }

    private func formatSpeed(_ bytesPerSec: UInt64) -> String {
        let mbps = Double(bytesPerSec) / 1_048_576
        if mbps >= 1 { return String(format: "%.1f M/s", mbps) }
        let kbps = Double(bytesPerSec) / 1024
        if kbps >= 1 { return String(format: "%.0f K/s", kbps) }
        return "0 B/s"
    }

    private func formatBytes(_ bytes: UInt64) -> String {
        let gb = Double(bytes) / 1_073_741_824
        if gb >= 1 { return String(format: "%.1f G", gb) }
        let mb = Double(bytes) / 1_048_576
        if mb >= 1 { return String(format: "%.0f M", mb) }
        let kb = Double(bytes) / 1024
        return String(format: "%.0f K", kb)
    }
}

private struct ClassicVolumesCard: View {
    package let volumes: [VolumeInfo]
    package let visibleVolumes: [VolumeInfo]
    package let hiddenVolumeIDs: Set<VolumeIdentity>
    package let surfaceStyle: ClassicStatsCardSurfaceStyle
    package let loadStorageHealth: ((VolumeInfo) async throws -> StorageHealthResult)?
    package let setVolumeVisibility: (VolumeInfo, Bool) -> Void
    package let setVolumesVisibility: ([VolumeInfo], Bool) -> Void
    @State private var showingDetails = false

    package var body: some View {
        Button {
            showingDetails = true
        } label: {
            VStack(alignment: .leading, spacing: 12) {
                HStack(spacing: 8) {
                    Text("Volumes")
                        .font(.headline)

                    Spacer(minLength: 8)

                    if !volumes.isEmpty {
                        Text(volumeCountTitle)
                            .font(.caption)
                            .foregroundStyle(.secondary)
                            .monospacedDigit()
                    }

                    Image(systemName: "chevron.right")
                        .font(.caption.weight(.bold))
                        .foregroundStyle(.tertiary)
                }
                .padding(.horizontal)

                if visibleVolumes.isEmpty {
                    HStack(spacing: 10) {
                        Image(systemName: volumes.isEmpty ? "internaldrive" : "eye.slash")
                            .foregroundStyle(.secondary)
                        Text(volumes.isEmpty ? String(localized: "No volumes reported") : String(localized: "All volumes hidden"))
                            .font(.subheadline)
                            .foregroundStyle(.secondary)
                    }
                    .padding(.horizontal)
                    .padding(.vertical, 8)
                } else {
                    ForEach(visibleVolumes) { volume in
                        ClassicVolumeRow(volume: volume)
                    }
                }
            }
            .padding(.vertical)
            .classicStatsCardSurface(surfaceStyle)
        }
        .buttonStyle(.plain)
        .accessibilityIdentifier("orlix.stats.storage.card")
        .statsDetailPresentation(isPresented: $showingDetails, size: StatsPresentationSize.large) {
            StorageDetailsSheet(
                volumes: volumes,
                hiddenVolumeIDs: hiddenVolumeIDs,
                loadStorageHealth: loadStorageHealth,
                setVolumeVisibility: setVolumeVisibility,
                setVolumesVisibility: setVolumesVisibility
            )
        }
    }

    private var volumeCountTitle: String {
        if visibleVolumes.count == 1 { return String(localized: "1 volume") }
        return String(format: String(localized: "%lld volumes"), Int64(visibleVolumes.count))
    }
}

private struct ClassicVolumeRow: View {
    package let volume: VolumeInfo

    package var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Image(systemName: "internaldrive")
                    .foregroundStyle(.secondary)

                Text(volume.mountPoint)
                    .font(.subheadline)
                    .lineLimit(1)

                Spacer()

                Text(String(format: String(localized: "%@/%@"), formatBytes(volume.used), formatBytes(volume.total)))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            ZStack(alignment: .leading) {
                RoundedRectangle(cornerRadius: 4)
                    .fill(Color.secondary.opacity(0.2))

                RoundedRectangle(cornerRadius: 4)
                    .fill(volumeColor)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .scaleEffect(x: min(volume.percent / 100, 1), y: 1, anchor: .leading)
            }
            .frame(height: 8)
            .frame(maxWidth: .infinity)
        }
        .padding(.horizontal)
    }

    private var volumeColor: Color {
        if volume.percent > 90 { return .red }
        if volume.percent > 80 { return .orange }
        return .green
    }

    private func formatBytes(_ bytes: UInt64) -> String {
        let tb = Double(bytes) / 1_099_511_627_776
        if tb >= 1 { return String(format: "%.1fT", tb) }
        let gb = Double(bytes) / 1_073_741_824
        if gb >= 1 { return String(format: "%.0fG", gb) }
        let mb = Double(bytes) / 1_048_576
        return String(format: "%.0fM", mb)
    }
}

private struct ClassicProcessesCard: View {
    package let processes: [ProcessInfo]
    package let surfaceStyle: ClassicStatsCardSurfaceStyle
    package let terminateProcess: ((ProcessInfo) async throws -> Void)?
    package let loadProcesses: (() async throws -> [ProcessInfo])?
    @State private var showingProcesses = false

    package var body: some View {
        if !processes.isEmpty {
            Button {
                showingProcesses = true
            } label: {
                VStack(alignment: .leading, spacing: 8) {
                    HStack {
                        Text("Top Processes")
                            .font(.headline)

                        Spacer()

                        HStack(spacing: 20) {
                            Text("CPU")
                                .font(.caption)
                                .foregroundStyle(.secondary)
                                .frame(width: 50, alignment: .trailing)
                            Text("MEM")
                                .font(.caption)
                                .foregroundStyle(.secondary)
                                .frame(width: 50, alignment: .trailing)
                            Image(systemName: "chevron.right")
                                .font(.caption.weight(.bold))
                                .foregroundStyle(.tertiary)
                        }
                    }

                    Divider()

                    ForEach(processes.prefix(5)) { process in
                        HStack {
                            Text(process.name)
                                .font(.subheadline)
                                .lineLimit(1)

                            Spacer()

                            Text(String(format: "%.1f%%", process.cpuPercent))
                                .font(.caption)
                                .monospacedDigit()
                                .foregroundStyle(process.cpuPercent > 50 ? .orange : .secondary)
                                .frame(width: 50, alignment: .trailing)

                            Text(String(format: "%.1f%%", process.memoryPercent))
                                .font(.caption)
                                .monospacedDigit()
                                .foregroundStyle(process.memoryPercent > 50 ? .orange : .secondary)
                                .frame(width: 50, alignment: .trailing)
                        }
                        .padding(.vertical, 2)
                    }
                }
                .padding()
                .classicStatsCardSurface(surfaceStyle)
            }
            .buttonStyle(.plain)
            .statsDetailPresentation(isPresented: $showingProcesses, size: StatsPresentationSize.large) {
                ProcessesSheet(
                    processes: processes,
                    processCount: processes.count,
                    terminateProcess: terminateProcess,
                    loadProcesses: loadProcesses
                )
            }
        }
    }
}

private struct ClassicStatLabel: View, Equatable {
    package let color: Color
    package let label: String
    package let value: String

    package var body: some View {
        HStack(spacing: 4) {
            Circle()
                .fill(color)
                .frame(width: 6, height: 6)

            Text(label)
                .font(.caption2)
                .foregroundStyle(.secondary)

            Text(value)
                .font(.caption)
                .fontWeight(.medium)
                .monospacedDigit()
                .frame(minWidth: 40, alignment: .leading)
        }
    }
}

private struct ClassicStatColumn: View, Equatable {
    package let label: String
    package let value: String

    package var body: some View {
        VStack(spacing: 2) {
            Text(label)
                .font(.caption2)
                .foregroundStyle(.secondary)
            Text(value)
                .font(.caption)
                .fontWeight(.semibold)
                .monospacedDigit()
                .lineLimit(1)
                .minimumScaleFactor(0.7)
        }
        .frame(maxWidth: .infinity)
    }
}

private struct ClassicCircularGauge: View, Equatable {
    package let value: Double
    package let color: Color
    package var lineWidth: CGFloat = 6

    package static func == (lhs: ClassicCircularGauge, rhs: ClassicCircularGauge) -> Bool {
        lhs.value == rhs.value && lhs.lineWidth == rhs.lineWidth
    }

    package var body: some View {
        ZStack {
            Circle()
                .stroke(color.opacity(0.2), lineWidth: lineWidth)

            Circle()
                .trim(from: 0, to: min(value, 1))
                .stroke(color, style: StrokeStyle(lineWidth: lineWidth, lineCap: .round))
                .rotationEffect(.degrees(-90))
                .animation(.easeInOut(duration: 0.3), value: value)
        }
    }
}

private struct ClassicDockerStatsCard: View {
    package let docker: DockerStats
    package let surfaceStyle: ClassicStatsCardSurfaceStyle
    package let loadDockerStats: (() async throws -> DockerStats)?
    package let performDockerAction: ((DockerContainerAction, DockerContainer) async throws -> DockerStats)?
    @State private var showingDetails = false

    package var body: some View {
        Button {
            showingDetails = true
        } label: {
            HStack(spacing: 16) {
                VStack(alignment: .leading, spacing: 8) {
                    HStack(spacing: 16) {
                        ClassicStatLabel(color: .blue, label: String(localized: "RUNNING"), value: "\(docker.runningCount)")
                        ClassicStatLabel(color: .secondary, label: String(localized: "STOPPED"), value: "\(docker.stoppedCount)")
                    }

                    HStack(spacing: 16) {
                        ClassicStatLabel(color: .pink, label: String(localized: "CPU"), value: formatPercent(docker.aggregateCPUPercent))
                        ClassicStatLabel(color: .blue, label: String(localized: "MEM"), value: formatPercent(docker.memoryPercent))
                    }
                }

                Spacer()

                ZStack {
                    ClassicCircularGauge(value: docker.memoryPercent / 100, color: .blue)
                    Image(systemName: "shippingbox")
                        .font(.system(size: 17, weight: .semibold))
                        .foregroundStyle(.blue)
                }
                .frame(width: 50, height: 50)
            }
            .padding()
            .classicStatsCardSurface(surfaceStyle)
        }
        .buttonStyle(.plain)
        .statsDetailPresentation(isPresented: $showingDetails, size: StatsPresentationSize.large) {
            DockerDetailsSheet(
                docker: docker,
                loadDockerStats: loadDockerStats,
                performDockerAction: performDockerAction
            )
        }
    }
}

private struct ClassicLockedDockerCard: View {
    package let surfaceStyle: ClassicStatsCardSurfaceStyle
    package let action: () -> Void

    package var body: some View {
        Button(action: action) {
            HStack(spacing: 12) {
                Image(systemName: "shippingbox")
                    .font(.system(size: 18, weight: .semibold))
                    .foregroundStyle(.blue)
                    .frame(width: 36, height: 36)
                    .background(Color.blue.opacity(0.12), in: Circle())

                VStack(alignment: .leading, spacing: 2) {
                    HStack(spacing: 6) {
                        Text(String(localized: "Docker"))
                            .font(.headline)
                        ProBadge(compact: true)
                    }
                    Text(String(localized: "Unlock Docker monitoring"))
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }

                Spacer(minLength: 10)

                Image(systemName: "chevron.right")
                    .font(.system(size: 14, weight: .bold))
                    .foregroundStyle(.secondary)
            }
            .padding(16)
            .classicStatsCardSurface(surfaceStyle)
        }
        .buttonStyle(.plain)
    }
}
