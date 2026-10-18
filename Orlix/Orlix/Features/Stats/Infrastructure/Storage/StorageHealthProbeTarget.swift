/// Infrastructure-only addressing for a health probe. Locators are never
/// persisted, rendered, or included in a health report.
#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package struct StorageHealthProbeTarget: Hashable, Sendable {
    nonisolated package enum Kind: Hashable, Sendable {
        case linux(devicePath: String, isEMMC: Bool)
        case darwin(nativeIdentifier: String, smartctlDevicePath: String?)
        case windows(diskNumber: UInt32)
        case freeBSD(devicePath: String)
        case openBSD(devicePath: String)
        case netBSD(devicePath: String)
    }

    package let deviceID: StorageDeviceIdentity
    package let kind: Kind
}

nonisolated package enum StorageHealthResolvedMember: Hashable, Sendable {
    case target(
        role: StorageHealthMemberRole,
        StorageHealthProbeTarget,
        findings: [StorageHealthFinding]
    )
    case unresolved(
        id: StorageDeviceIdentity,
        role: StorageHealthMemberRole,
        reason: StorageHealthUnavailableReason
    )

    package var id: StorageDeviceIdentity {
        switch self {
        case .target(_, let target, _): target.deviceID
        case .unresolved(let id, _, _): id
        }
    }

    package var role: StorageHealthMemberRole {
        switch self {
        case .target(let role, _, _), .unresolved(_, let role, _): role
        }
    }

}

nonisolated package struct StorageHealthResolvedTopology: Hashable, Sendable {
    package let kind: StorageTopologyKind
    package let name: String?
    package let findings: [StorageHealthFinding]
    package let members: [StorageHealthResolvedMember]

    package var coverage: StorageHealthCoverage {
        members.contains {
            if case .unresolved = $0 { return true }
            return false
        } ? .partial : .complete
    }
}
