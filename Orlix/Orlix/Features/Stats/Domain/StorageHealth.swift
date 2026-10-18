import Foundation

/// Opaque identity for a physical storage device.
///
/// The value is for correlation only. It must never be rendered as a device path
/// or sent to analytics.
nonisolated package struct StorageDeviceIdentity: Hashable, Sendable {
    package let namespace: String
    package let opaqueValue: String

    package init(namespace: String, opaqueValue: String) {
        self.namespace = namespace.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        self.opaqueValue = opaqueValue.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
    }
}

nonisolated public enum StorageHealthUnavailableReason: String, Hashable, Sendable {
    public case unsupported
    public case toolMissing
    public case permissionDenied
    public case unmapped
    public case virtualDevice
    public case networkVolume
    public case invalidResponse
}

nonisolated enum StorageHealthCapability: Hashable, Sendable {
    case supported
    case unavailable(StorageHealthUnavailableReason)
}

nonisolated public enum StorageHealthState: Int, Hashable, Sendable {
    public case unknown = 0
    public case healthy = 1
    public case warning = 2
    public case failing = 3
}

nonisolated public enum StorageHealthSource: String, Hashable, Sendable {
    public case smartctl
    public case emmc
    public case btrfs
    public case zfs
    public case darwinDiskUtility
    public case windowsStorage
    public case bsdNative
}

nonisolated public enum StorageHealthFindingSeverity: Int, Hashable, Sendable {
    public case information = 0
    public case warning = 1
    public case critical = 2
}

nonisolated enum StorageHealthFindingTiming: Hashable, Sendable {
    case current
    case historical
}

/// Closed set of evidence that can explain a non-healthy storage state.
/// Associated values are bounded, privacy-safe supporting values only.
nonisolated enum StorageHealthFindingKind: Hashable, Sendable {
    case sourceReportedHealth(String)
    case smartOverallFailure
    case smartCurrentPrefailThreshold
    case smartPastThreshold
    case smartErrorLog
    case smartSelfTestLog
    case nvmeCriticalWarning(bit: UInt8)
    case ataAttribute(name: String)
    case scsiErrorHistory(read: UInt64, write: UInt64, media: UInt64)
    case emmcPreEOL(EMMCPreEOLStatus)
    case emmcLifetimeExceeded
    case poolState(String)
    case deviceErrors(read: UInt64, write: UInt64, checksum: UInt64)
    case missingMember
    case partialCoverage

    var stableID: String {
        switch self {
        case .sourceReportedHealth(let value): "source-health:\(value.lowercased())"
        case .smartOverallFailure: "smart-overall-failure"
        case .smartCurrentPrefailThreshold: "smart-current-prefail"
        case .smartPastThreshold: "smart-past-threshold"
        case .smartErrorLog: "smart-error-log"
        case .smartSelfTestLog: "smart-self-test-log"
        case .nvmeCriticalWarning(let bit): "nvme-critical:\(bit)"
        case .ataAttribute(let name): "ata-attribute:\(name.lowercased())"
        case .scsiErrorHistory(let read, let write, let media):
            "scsi-errors:\(read):\(write):\(media)"
        case .emmcPreEOL(let status): "emmc-pre-eol:\(status.rawValue)"
        case .emmcLifetimeExceeded: "emmc-lifetime-exceeded"
        case .poolState(let state): "pool-state:\(state.lowercased())"
        case .deviceErrors(let read, let write, let checksum):
            "device-errors:\(read):\(write):\(checksum)"
        case .missingMember: "missing-member"
        case .partialCoverage: "partial-coverage"
        }
    }
}

nonisolated package struct StorageHealthFinding: Hashable, Sendable, Identifiable {
    package let kind: StorageHealthFindingKind
    package let severity: StorageHealthFindingSeverity
    package let timing: StorageHealthFindingTiming
    package let source: StorageHealthSource

    package var id: String {
        "\(kind.stableID)|\(source.rawValue)|\(timing == .current ? "current" : "historical")"
    }

    package init(
        kind: StorageHealthFindingKind,
        severity: StorageHealthFindingSeverity,
        timing: StorageHealthFindingTiming = .current,
        source: StorageHealthSource
    ) {
        self.kind = kind
        self.severity = severity
        self.timing = timing
        self.source = source
    }
}

nonisolated public enum StorageHealthAttributeValue: Hashable, Sendable {
    public case integer(UInt64)
    public case decimal(Double)
    public case text(String)
    public case boolean(Bool)
}

/// A whitelisted platform-specific value that is safe to display.
/// Parsers intentionally omit serial numbers, device paths, and raw payloads.
nonisolated package struct StorageHealthAttribute: Hashable, Sendable {
    package let key: String
    package let label: String
    package let value: StorageHealthAttributeValue
    package let unit: String?

    package init(
        key: String,
        label: String,
        value: StorageHealthAttributeValue,
        unit: String? = nil
    ) {
        self.key = key
        self.label = label
        self.value = value
        self.unit = unit
    }
}

nonisolated public enum EMMCPreEOLStatus: UInt8, Hashable, Sendable {
    public case unknown = 0
    public case normal = 1
    public case warning = 2
    public case urgent = 3
}

/// JEDEC reports lifetime consumption as a coarse bucket, not an exact
/// remaining-life percentage.
nonisolated enum EMMCLifetimeBucket: Hashable, Sendable {
    case unknown
    case estimatedUsage(lowerPercent: Int, upperPercent: Int)
    case exceededMaximumEstimate
    case reserved(UInt8)
}

nonisolated package struct EMMCLifetimeEstimate: Hashable, Sendable {
    package let rawValue: UInt8
    package let bucket: EMMCLifetimeBucket

    package init(rawValue: UInt8) {
        self.rawValue = rawValue
        switch rawValue {
        case 0:
            bucket = .unknown
        case 1...10:
            let lower = Int(rawValue - 1) * 10
            bucket = .estimatedUsage(lowerPercent: lower, upperPercent: lower + 10)
        case 11:
            bucket = .exceededMaximumEstimate
        default:
            bucket = .reserved(rawValue)
        }
    }
}

nonisolated package struct EMMCHealth: Hashable, Sendable {
    package let preEOL: EMMCPreEOLStatus
    package let lifetimeTypeA: EMMCLifetimeEstimate?
    package let lifetimeTypeB: EMMCLifetimeEstimate?
}

nonisolated package struct StorageHealthMetrics: Hashable, Sendable {
    package var temperatureCelsius: Double?
    package var maximumTemperatureCelsius: Double?
    /// Consumed endurance. This is deliberately not named "remaining".
    package var percentageUsed: Double?
    package var availableSparePercent: Double?
    package var availableSpareThresholdPercent: Double?
    package var powerOnHours: UInt64?
    package var powerCycleCount: UInt64?
    package var unsafeShutdownCount: UInt64?
    package var mediaErrorCount: UInt64?
    package var errorLogEntryCount: UInt64?
    package var readErrorsCorrected: UInt64?
    package var readErrorsUncorrected: UInt64?
    package var writeErrorsCorrected: UInt64?
    package var writeErrorsUncorrected: UInt64?
    package var startStopCycleCount: UInt64?
    package var loadUnloadCycleCount: UInt64?
    package var emmc: EMMCHealth?
}

nonisolated package struct StorageHealthReport: Hashable, Sendable {
    package let deviceID: StorageDeviceIdentity
    package var sourceState: StorageHealthState
    package var metrics: StorageHealthMetrics
    package var sources: Set<StorageHealthSource>
    package var findings: [StorageHealthFinding]
    package var attributes: [StorageHealthAttribute]

    package var state: StorageHealthState {
        findings.reduce(sourceState) { state, finding in
            let findingState: StorageHealthState
            switch finding.severity {
            case .information:
                findingState = .unknown
            case .warning:
                findingState = .warning
            case .critical:
                findingState = .failing
            }
            return state.rawValue >= findingState.rawValue ? state : findingState
        }
    }

    package init(
        deviceID: StorageDeviceIdentity,
        state: StorageHealthState = .unknown,
        metrics: StorageHealthMetrics = StorageHealthMetrics(),
        sources: Set<StorageHealthSource>,
        findings: [StorageHealthFinding] = [],
        attributes: [StorageHealthAttribute] = []
    ) {
        self.deviceID = deviceID
        sourceState = state
        self.metrics = metrics
        self.sources = sources
        self.findings = findings
        self.attributes = attributes
    }
}

nonisolated public enum StorageDeviceHealthResult: Hashable, Sendable {
    public case report(StorageHealthReport)
    public case unavailable(StorageHealthUnavailableReason)

    public var capability: StorageHealthCapability {
        switch self {
        case .report:
            return .supported
        case .unavailable(let reason):
            return .unavailable(reason)
        }
    }
}

nonisolated public enum StorageTopologyKind: Hashable, Sendable {
    public case physicalDevice
    public case btrfs
    public case zfs
}

nonisolated public enum StorageHealthMemberRole: Hashable, Sendable {
    public case data
    public case cache
    public case log
    public case spare
    public case special
}

nonisolated public enum StorageHealthCoverage: Hashable, Sendable {
    public case complete
    public case partial
}

nonisolated package struct StorageHealthMemberReport: Hashable, Sendable, Identifiable {
    package let id: StorageDeviceIdentity
    package let role: StorageHealthMemberRole
    package let ordinal: Int
    package let result: StorageDeviceHealthResult
    package let findings: [StorageHealthFinding]

    package init(
        id: StorageDeviceIdentity,
        role: StorageHealthMemberRole,
        ordinal: Int,
        result: StorageDeviceHealthResult,
        findings: [StorageHealthFinding] = []
    ) {
        self.id = id
        self.role = role
        self.ordinal = ordinal
        self.result = result
        self.findings = findings
    }

    package var state: StorageHealthState {
        let deviceState: StorageHealthState
        switch result {
        case .report(let report): deviceState = report.state
        case .unavailable: deviceState = .unknown
        }
        return findings.reduce(deviceState) { state, finding in
            let findingState: StorageHealthState
            switch finding.severity {
            case .information: findingState = .unknown
            case .warning: findingState = .warning
            case .critical: findingState = .failing
            }
            return state.rawValue >= findingState.rawValue ? state : findingState
        }
    }
}

nonisolated package struct StorageHealthVolumeReport: Hashable, Sendable {
    package let topology: StorageTopologyKind
    package let name: String?
    package let coverage: StorageHealthCoverage
    package let findings: [StorageHealthFinding]
    package let members: [StorageHealthMemberReport]

    package var state: StorageHealthState {
        let poolState = findings.reduce(StorageHealthState.unknown) { state, finding in
            let findingState: StorageHealthState
            switch finding.severity {
            case .information: findingState = .unknown
            case .warning: findingState = .warning
            case .critical: findingState = .failing
            }
            return state.rawValue >= findingState.rawValue ? state : findingState
        }
        return members.reduce(poolState) { state, member in
            state.rawValue >= member.state.rawValue ? state : member.state
        }
    }
}

nonisolated public enum StorageHealthResult: Hashable, Sendable {
    public case report(StorageHealthVolumeReport)
    public case unavailable(StorageHealthUnavailableReason)

    public var capability: StorageHealthCapability {
        switch self {
        case .report:
            return .supported
        case .unavailable(let reason):
            return .unavailable(reason)
        }
    }
}
