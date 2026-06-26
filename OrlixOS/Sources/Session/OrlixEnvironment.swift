import Foundation

@_spi(OrlixPrivateTesting)
public enum OrlixEnvironmentSource: Codable, Equatable, Sendable {
    case defaultRoot
    case copiedEnvironment(parentID: String)
    case rootfsTar
    case ociLayout
}

public enum OrlixEnvironmentRootPropagation: String, Codable, Equatable, Sendable {
    case `private`
    case shared
    case slave
    case unbindable
}

@_spi(OrlixPrivateTesting)
public struct OrlixEnvironmentDescriptor: Codable, Equatable, Sendable {
    public static let defaultEnvironmentID = "default"

    public let id: String
    public let source: OrlixEnvironmentSource
    public let platform: String
    public let rootImageIdentifier: String
    public let defaultCommand: [String]
    public let defaultEnvironment: [String: String]
    public let defaultWorkingDirectory: String
    public let defaultUserID: UInt32
    public let defaultGroupID: UInt32
    public let defaultSupplementaryGroups: [UInt32]
	public let defaultCapabilities: OrlixEnvironmentCapabilities?
	public let defaultNoNewPrivileges: Bool
	public let defaultCloseAdditionalFds: Bool
	public let defaultTerminal: Bool?
	public let defaultTerminalRows: UInt32?
	public let defaultTerminalColumns: UInt32?
	public let defaultOOMScoreAdjustment: Int32?
    public let defaultScheduler: OrlixEnvironmentScheduler?
    public let defaultIOPriority: OrlixEnvironmentIOPriority?
    public let defaultCPUAffinity: OrlixEnvironmentCPUAffinity?
    public let defaultUmask: UInt32?
    public let defaultRlimits: [OrlixEnvironmentRlimit]
    public let defaultPersonalityDomain: String?
    public let hostname: String?
    public let domainname: String?
    public let rootMount: OrlixEnvironmentRootMount
    public let rootReadonly: Bool
    public let rootPropagation: OrlixEnvironmentRootPropagation
    public let sysctls: [String: String]
    public let maskedPaths: [String]
	public let readonlyPaths: [String]
	public let cgroupsPath: String?
public let cgroupPidsLimit: Int64?
	public let cgroupCPUMax: OrlixEnvironmentCgroupCPUMax?
	public let cgroupCPUWeight: UInt64?
	public let cgroupMemoryMax: Int64?
    public let cgroupIOWeight: UInt64?
    public let cgroupUnified: [OrlixEnvironmentCgroupUnifiedEntry]
    public let deviceNodes: [OrlixEnvironmentDeviceNode]
    public let timeOffsets: [OrlixEnvironmentTimeOffset]
    public let uidMappings: [OrlixEnvironmentIDMapping]
	public let gidMappings: [OrlixEnvironmentIDMapping]
	public let namespaces: [String]
	public let namespacePaths: [String: String]
	public let tmpfsMounts: [OrlixEnvironmentTmpfsMount]
	public let mounts: [OrlixEnvironmentMount]
	public let annotations: [String: String]

    public static func defaultEnvironment(
        rootImageIdentifier: String =
            OrlixOSDistribution.productRootImageIdentifier ?? ""
    ) -> OrlixEnvironmentDescriptor {
        OrlixEnvironmentDescriptor(
            id: defaultEnvironmentID,
            source: .defaultRoot,
            platform: "linux/arm64",
            rootImageIdentifier: rootImageIdentifier,
            defaultCommand: ["/bin/sh"],
            defaultEnvironment: [
                "HOME": "/root",
                "PATH": "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
                "TERM": "xterm-256color"
            ],
            defaultWorkingDirectory: "/",
            defaultUserID: 0,
            defaultGroupID: 0,
            defaultUmask: nil,
            defaultRlimits: [],
            hostname: nil,
            domainname: nil,
            rootMount: .defaultOverlay,
            mounts: []
        )
    }

    public init(
        id: String,
        source: OrlixEnvironmentSource,
        platform: String,
        rootImageIdentifier: String,
        defaultCommand: [String],
        defaultEnvironment: [String: String],
        defaultWorkingDirectory: String,
        defaultUserID: UInt32,
        defaultGroupID: UInt32,
        defaultSupplementaryGroups: [UInt32] = [],
		defaultCapabilities: OrlixEnvironmentCapabilities? = nil,
		defaultNoNewPrivileges: Bool = false,
		defaultCloseAdditionalFds: Bool = false,
		defaultTerminal: Bool? = nil,
		defaultTerminalRows: UInt32? = nil,
		defaultTerminalColumns: UInt32? = nil,
		defaultOOMScoreAdjustment: Int32? = nil,
        defaultScheduler: OrlixEnvironmentScheduler? = nil,
        defaultIOPriority: OrlixEnvironmentIOPriority? = nil,
        defaultCPUAffinity: OrlixEnvironmentCPUAffinity? = nil,
        defaultUmask: UInt32? = nil,
        defaultRlimits: [OrlixEnvironmentRlimit] = [],
        defaultPersonalityDomain: String? = nil,
        hostname: String? = nil,
        domainname: String? = nil,
        rootMount: OrlixEnvironmentRootMount = .defaultOverlay,
        rootReadonly: Bool = false,
        rootPropagation: OrlixEnvironmentRootPropagation = .private,
        sysctls: [String: String] = [:],
        maskedPaths: [String] = [],
		readonlyPaths: [String] = [],
cgroupsPath: String? = nil,
cgroupPidsLimit: Int64? = nil,
		cgroupCPUMax: OrlixEnvironmentCgroupCPUMax? = nil,
		cgroupCPUWeight: UInt64? = nil,
		cgroupMemoryMax: Int64? = nil,
        cgroupIOWeight: UInt64? = nil,
        cgroupUnified: [OrlixEnvironmentCgroupUnifiedEntry] = [],
        deviceNodes: [OrlixEnvironmentDeviceNode] = [],
        timeOffsets: [OrlixEnvironmentTimeOffset] = [],
        uidMappings: [OrlixEnvironmentIDMapping] = [],
		gidMappings: [OrlixEnvironmentIDMapping] = [],
		namespaces: [String] = [],
		namespacePaths: [String: String] = [:],
		tmpfsMounts: [OrlixEnvironmentTmpfsMount] = [],
		mounts: [OrlixEnvironmentMount] = [],
		annotations: [String: String] = [:]
	) {
        self.id = id
        self.source = source
        self.platform = platform
        self.rootImageIdentifier = rootImageIdentifier
        self.defaultCommand = defaultCommand
        self.defaultEnvironment = defaultEnvironment
        self.defaultWorkingDirectory = defaultWorkingDirectory
        self.defaultUserID = defaultUserID
        self.defaultGroupID = defaultGroupID
        self.defaultSupplementaryGroups = defaultSupplementaryGroups
		self.defaultCapabilities = defaultCapabilities
		self.defaultNoNewPrivileges = defaultNoNewPrivileges
		self.defaultCloseAdditionalFds = defaultCloseAdditionalFds
		self.defaultTerminal = defaultTerminal
		self.defaultTerminalRows = defaultTerminalRows
		self.defaultTerminalColumns = defaultTerminalColumns
		self.defaultOOMScoreAdjustment = defaultOOMScoreAdjustment
        self.defaultScheduler = defaultScheduler
        self.defaultIOPriority = defaultIOPriority
        self.defaultCPUAffinity = defaultCPUAffinity
        self.defaultUmask = defaultUmask
        self.defaultRlimits = defaultRlimits
        self.defaultPersonalityDomain = defaultPersonalityDomain
        self.hostname = hostname
        self.domainname = domainname
        self.rootMount = rootMount
        self.rootReadonly = rootReadonly
        self.rootPropagation = rootPropagation
        self.sysctls = sysctls
        self.maskedPaths = maskedPaths
		self.readonlyPaths = readonlyPaths
self.cgroupsPath = cgroupsPath
self.cgroupPidsLimit = cgroupPidsLimit
		self.cgroupCPUMax = cgroupCPUMax
		self.cgroupCPUWeight = cgroupCPUWeight
		self.cgroupMemoryMax = cgroupMemoryMax
        self.cgroupIOWeight = cgroupIOWeight
        self.cgroupUnified = cgroupUnified
        self.deviceNodes = deviceNodes
        self.timeOffsets = timeOffsets
        self.uidMappings = uidMappings
		self.gidMappings = gidMappings
		self.namespaces = namespaces
		self.namespacePaths = namespacePaths
		self.tmpfsMounts = tmpfsMounts
		self.mounts = mounts
		self.annotations = annotations
	}

    private enum CodingKeys: String, CodingKey {
        case id
        case source
        case platform
        case rootImageIdentifier
        case defaultCommand
        case defaultEnvironment
        case defaultWorkingDirectory
        case defaultUserID
        case defaultGroupID
        case defaultSupplementaryGroups
		case defaultCapabilities
		case defaultNoNewPrivileges
		case defaultCloseAdditionalFds
		case defaultTerminal
		case defaultTerminalRows
		case defaultTerminalColumns
		case defaultOOMScoreAdjustment
        case defaultScheduler
        case defaultIOPriority
        case defaultCPUAffinity
        case defaultUmask
        case defaultRlimits
        case defaultPersonalityDomain
        case hostname
        case domainname
        case rootMount
        case rootReadonly
        case rootPropagation
        case sysctls
        case maskedPaths
		case readonlyPaths
		case cgroupsPath
case cgroupPidsLimit
case cgroupCPUMax
		case cgroupCPUWeight
		case cgroupMemoryMax
        case cgroupIOWeight
        case cgroupUnified
        case deviceNodes
        case timeOffsets
        case uidMappings
        case gidMappings
		case namespaces
		case namespacePaths
		case tmpfsMounts
		case mounts
		case annotations
	}

    public init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        self.id = try container.decode(String.self, forKey: .id)
        self.source = try container.decode(
            OrlixEnvironmentSource.self,
            forKey: .source
        )
        self.platform = try container.decode(String.self, forKey: .platform)
        self.rootImageIdentifier = try container.decode(
            String.self,
            forKey: .rootImageIdentifier
        )
        self.defaultCommand = try container.decode(
            [String].self,
            forKey: .defaultCommand
        )
        self.defaultEnvironment = try container.decode(
            [String: String].self,
            forKey: .defaultEnvironment
        )
        self.defaultWorkingDirectory = try container.decode(
            String.self,
            forKey: .defaultWorkingDirectory
        )
        self.defaultUserID = try container.decode(
            UInt32.self,
            forKey: .defaultUserID
        )
        self.defaultGroupID = try container.decode(
            UInt32.self,
            forKey: .defaultGroupID
        )
        self.defaultSupplementaryGroups = try container.decodeIfPresent(
            [UInt32].self,
            forKey: .defaultSupplementaryGroups
        ) ?? []
        self.defaultCapabilities = try container.decodeIfPresent(
            OrlixEnvironmentCapabilities.self,
            forKey: .defaultCapabilities
        )
        self.defaultNoNewPrivileges = try container.decodeIfPresent(
            Bool.self,
            forKey: .defaultNoNewPrivileges
        ) ?? false
		self.defaultCloseAdditionalFds = try container.decodeIfPresent(
			Bool.self,
			forKey: .defaultCloseAdditionalFds
		) ?? false
		self.defaultTerminal = try container.decodeIfPresent(
			Bool.self,
			forKey: .defaultTerminal
		)
		self.defaultTerminalRows = try container.decodeIfPresent(
			UInt32.self,
			forKey: .defaultTerminalRows
		)
		self.defaultTerminalColumns = try container.decodeIfPresent(
			UInt32.self,
			forKey: .defaultTerminalColumns
		)
		self.defaultOOMScoreAdjustment = try container.decodeIfPresent(
			Int32.self,
			forKey: .defaultOOMScoreAdjustment
        )
        self.defaultScheduler = try container.decodeIfPresent(
            OrlixEnvironmentScheduler.self,
            forKey: .defaultScheduler
        )
        self.defaultIOPriority = try container.decodeIfPresent(
            OrlixEnvironmentIOPriority.self,
            forKey: .defaultIOPriority
        )
        self.defaultCPUAffinity = try container.decodeIfPresent(
            OrlixEnvironmentCPUAffinity.self,
            forKey: .defaultCPUAffinity
        )
        self.defaultUmask = try container.decodeIfPresent(
            UInt32.self,
            forKey: .defaultUmask
        )
        self.defaultRlimits = try container.decodeIfPresent(
            [OrlixEnvironmentRlimit].self,
            forKey: .defaultRlimits
        ) ?? []
        self.defaultPersonalityDomain = try container.decodeIfPresent(
            String.self,
            forKey: .defaultPersonalityDomain
        )
        self.hostname = try container.decodeIfPresent(
            String.self,
            forKey: .hostname
        )
        self.domainname = try container.decodeIfPresent(
            String.self,
            forKey: .domainname
        )
        self.rootMount = try container.decodeIfPresent(
            OrlixEnvironmentRootMount.self,
            forKey: .rootMount
        ) ?? .defaultOverlay
        self.rootReadonly = try container.decodeIfPresent(
            Bool.self,
            forKey: .rootReadonly
        ) ?? false
        self.rootPropagation = try container.decodeIfPresent(
            OrlixEnvironmentRootPropagation.self,
            forKey: .rootPropagation
        ) ?? .private
        self.sysctls = try container.decodeIfPresent(
            [String: String].self,
            forKey: .sysctls
        ) ?? [:]
        self.maskedPaths = try container.decodeIfPresent(
            [String].self,
            forKey: .maskedPaths
        ) ?? []
        self.readonlyPaths = try container.decodeIfPresent(
            [String].self,
            forKey: .readonlyPaths
        ) ?? []
        self.cgroupsPath = try container.decodeIfPresent(
            String.self,
            forKey: .cgroupsPath
        )
		self.cgroupPidsLimit = try container.decodeIfPresent(
			Int64.self,
			forKey: .cgroupPidsLimit
		)
self.cgroupCPUMax = try container.decodeIfPresent(
OrlixEnvironmentCgroupCPUMax.self,
forKey: .cgroupCPUMax
)
self.cgroupCPUWeight = try container.decodeIfPresent(
UInt64.self,
forKey: .cgroupCPUWeight
)
		self.cgroupMemoryMax = try container.decodeIfPresent(
			Int64.self,
			forKey: .cgroupMemoryMax
		)
		self.cgroupIOWeight = try container.decodeIfPresent(
			UInt64.self,
			forKey: .cgroupIOWeight
		)
		self.cgroupUnified = try container.decodeIfPresent(
			[OrlixEnvironmentCgroupUnifiedEntry].self,
			forKey: .cgroupUnified
		) ?? []
        self.deviceNodes = try container.decodeIfPresent(
            [OrlixEnvironmentDeviceNode].self,
            forKey: .deviceNodes
        ) ?? []
        self.timeOffsets = try container.decodeIfPresent(
            [OrlixEnvironmentTimeOffset].self,
            forKey: .timeOffsets
        ) ?? []
        self.uidMappings = try container.decodeIfPresent(
            [OrlixEnvironmentIDMapping].self,
            forKey: .uidMappings
        ) ?? []
        self.gidMappings = try container.decodeIfPresent(
            [OrlixEnvironmentIDMapping].self,
            forKey: .gidMappings
        ) ?? []
        self.namespaces = try container.decodeIfPresent(
            [String].self,
            forKey: .namespaces
        ) ?? []
		self.namespacePaths = try container.decodeIfPresent(
			[String: String].self,
			forKey: .namespacePaths
		) ?? [:]
		self.tmpfsMounts = try container.decodeIfPresent(
			[OrlixEnvironmentTmpfsMount].self,
			forKey: .tmpfsMounts
		) ?? []
		self.mounts = try container.decodeIfPresent(
			[OrlixEnvironmentMount].self,
			forKey: .mounts
		) ?? []
		self.annotations = try container.decodeIfPresent(
			[String: String].self,
			forKey: .annotations
		) ?? [:]
	}

    public func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(id, forKey: .id)
        try container.encode(source, forKey: .source)
        try container.encode(platform, forKey: .platform)
        try container.encode(rootImageIdentifier, forKey: .rootImageIdentifier)
        try container.encode(defaultCommand, forKey: .defaultCommand)
        try container.encode(defaultEnvironment, forKey: .defaultEnvironment)
        try container.encode(defaultWorkingDirectory, forKey: .defaultWorkingDirectory)
        try container.encode(defaultUserID, forKey: .defaultUserID)
        try container.encode(defaultGroupID, forKey: .defaultGroupID)
        try container.encode(
            defaultSupplementaryGroups,
            forKey: .defaultSupplementaryGroups
        )
        if defaultNoNewPrivileges {
            try container.encode(defaultNoNewPrivileges, forKey: .defaultNoNewPrivileges)
        }
		if defaultCloseAdditionalFds {
			try container.encode(defaultCloseAdditionalFds, forKey: .defaultCloseAdditionalFds)
		}
		try container.encodeIfPresent(defaultTerminal, forKey: .defaultTerminal)
		try container.encodeIfPresent(defaultTerminalRows, forKey: .defaultTerminalRows)
		try container.encodeIfPresent(defaultTerminalColumns, forKey: .defaultTerminalColumns)
		try container.encodeIfPresent(
			defaultOOMScoreAdjustment,
			forKey: .defaultOOMScoreAdjustment
        )
        try container.encodeIfPresent(defaultScheduler, forKey: .defaultScheduler)
        try container.encodeIfPresent(defaultIOPriority, forKey: .defaultIOPriority)
        try container.encodeIfPresent(defaultCPUAffinity, forKey: .defaultCPUAffinity)
        try container.encodeIfPresent(defaultUmask, forKey: .defaultUmask)
        if !defaultRlimits.isEmpty {
            try container.encode(defaultRlimits, forKey: .defaultRlimits)
        }
        try container.encodeIfPresent(
            defaultPersonalityDomain,
            forKey: .defaultPersonalityDomain
        )
        try container.encodeIfPresent(hostname, forKey: .hostname)
        try container.encodeIfPresent(domainname, forKey: .domainname)
        try container.encode(rootMount, forKey: .rootMount)
        try container.encode(rootReadonly, forKey: .rootReadonly)
        try container.encode(rootPropagation, forKey: .rootPropagation)
        if !sysctls.isEmpty {
            try container.encode(sysctls, forKey: .sysctls)
        }
        if !maskedPaths.isEmpty {
            try container.encode(maskedPaths, forKey: .maskedPaths)
        }
        if !readonlyPaths.isEmpty {
            try container.encode(readonlyPaths, forKey: .readonlyPaths)
        }
try container.encodeIfPresent(cgroupsPath, forKey: .cgroupsPath)
try container.encodeIfPresent(cgroupPidsLimit, forKey: .cgroupPidsLimit)
		try container.encodeIfPresent(cgroupCPUMax, forKey: .cgroupCPUMax)
		try container.encodeIfPresent(cgroupCPUWeight, forKey: .cgroupCPUWeight)
		try container.encodeIfPresent(cgroupMemoryMax, forKey: .cgroupMemoryMax)
		try container.encodeIfPresent(cgroupIOWeight, forKey: .cgroupIOWeight)
		if !cgroupUnified.isEmpty {
			try container.encode(cgroupUnified, forKey: .cgroupUnified)
		}
        if !deviceNodes.isEmpty {
            try container.encode(deviceNodes, forKey: .deviceNodes)
        }
        if !timeOffsets.isEmpty {
            try container.encode(timeOffsets, forKey: .timeOffsets)
        }
        if !uidMappings.isEmpty {
            try container.encode(uidMappings, forKey: .uidMappings)
        }
        if !gidMappings.isEmpty {
            try container.encode(gidMappings, forKey: .gidMappings)
        }
        if !namespaces.isEmpty {
            try container.encode(namespaces, forKey: .namespaces)
        }
		if !namespacePaths.isEmpty {
			try container.encode(namespacePaths, forKey: .namespacePaths)
		}
		if !tmpfsMounts.isEmpty {
			try container.encode(tmpfsMounts, forKey: .tmpfsMounts)
		}
		try container.encode(mounts, forKey: .mounts)
		if !annotations.isEmpty {
			try container.encode(annotations, forKey: .annotations)
		}
	}
}

public struct OrlixEnvironmentRlimit: Codable, Equatable, Sendable {
	public let type: String
	public let soft: UInt64
	public let hard: UInt64

    public init(type: String, soft: UInt64, hard: UInt64) {
        self.type = type
        self.soft = soft
        self.hard = hard
	}
}

public struct OrlixEnvironmentCgroupCPUMax: Codable, Equatable, Sendable {
public static let defaultPeriodMicros: UInt64 = 100_000

public let quotaMicros: Int64
public let periodMicros: UInt64

	public init(
		quotaMicros: Int64,
		periodMicros: UInt64 = OrlixEnvironmentCgroupCPUMax.defaultPeriodMicros
	) {
		self.quotaMicros = quotaMicros
self.periodMicros = periodMicros
}
}

public struct OrlixEnvironmentCgroupUnifiedEntry: Codable, Equatable, Sendable {
public let file: String
public let value: String

public init(file: String, value: String) {
self.file = file
self.value = value
}
}

public struct OrlixEnvironmentDeviceNode: Codable, Equatable, Sendable {
    public let path: String
    public let type: String
public let major: UInt32
public let minor: UInt32
public let fileMode: UInt32
public let uid: UInt32
public let gid: UInt32

public init(
path: String,
type: String,
major: UInt32 = 0,
minor: UInt32 = 0,
fileMode: UInt32 = 0o666,
uid: UInt32 = 0,
gid: UInt32 = 0
) {
self.path = path
self.type = type
self.major = major
self.minor = minor
self.fileMode = fileMode
self.uid = uid
        self.gid = gid
    }
}

public struct OrlixEnvironmentTimeOffset: Codable, Equatable, Sendable {
    public let clock: String
    public let secs: Int64
    public let nanosecs: Int64

    public init(clock: String, secs: Int64, nanosecs: Int64) {
        self.clock = clock
        self.secs = secs
        self.nanosecs = nanosecs
    }
}

public struct OrlixEnvironmentIDMapping: Codable, Equatable, Sendable {
    public let containerID: UInt32
    public let hostID: UInt32
    public let size: UInt32

    public init(containerID: UInt32, hostID: UInt32, size: UInt32) {
        self.containerID = containerID
        self.hostID = hostID
        self.size = size
    }
}

public struct OrlixEnvironmentCapabilities: Codable, Equatable, Sendable {
	static let supportedLinuxNames: Set<String> = [
		"CAP_CHOWN",
		"CAP_DAC_OVERRIDE",
		"CAP_DAC_READ_SEARCH",
		"CAP_FOWNER",
		"CAP_FSETID",
		"CAP_KILL",
		"CAP_SETGID",
		"CAP_SETUID",
		"CAP_SETPCAP",
		"CAP_LINUX_IMMUTABLE",
		"CAP_NET_BIND_SERVICE",
		"CAP_NET_BROADCAST",
		"CAP_NET_ADMIN",
		"CAP_NET_RAW",
		"CAP_IPC_LOCK",
		"CAP_IPC_OWNER",
		"CAP_SYS_MODULE",
		"CAP_SYS_RAWIO",
		"CAP_SYS_CHROOT",
		"CAP_SYS_PTRACE",
		"CAP_SYS_PACCT",
		"CAP_SYS_ADMIN",
		"CAP_SYS_BOOT",
		"CAP_SYS_NICE",
		"CAP_SYS_RESOURCE",
		"CAP_SYS_TIME",
		"CAP_SYS_TTY_CONFIG",
		"CAP_MKNOD",
		"CAP_LEASE",
		"CAP_AUDIT_WRITE",
		"CAP_AUDIT_CONTROL",
		"CAP_SETFCAP",
		"CAP_MAC_OVERRIDE",
		"CAP_MAC_ADMIN",
		"CAP_SYSLOG",
		"CAP_WAKE_ALARM",
		"CAP_BLOCK_SUSPEND",
		"CAP_AUDIT_READ",
		"CAP_PERFMON",
		"CAP_BPF",
		"CAP_CHECKPOINT_RESTORE",
	]

	public let bounding: [String]
	public let permitted: [String]
    public let inheritable: [String]
    public let effective: [String]
    public let ambient: [String]

    public init(
        bounding: [String] = [],
        permitted: [String] = [],
        inheritable: [String] = [],
        effective: [String] = [],
        ambient: [String] = []
    ) {
        self.bounding = bounding
        self.permitted = permitted
        self.inheritable = inheritable
        self.effective = effective
        self.ambient = ambient
    }
}

public struct OrlixEnvironmentScheduler: Codable, Equatable, Sendable {
    public let policy: String
    public let priority: Int32

    public init(policy: String, priority: Int32) {
        self.policy = policy
        self.priority = priority
    }
}

public struct OrlixEnvironmentIOPriority: Codable, Equatable, Sendable {
    public let `class`: String
    public let priority: Int32

    public init(class: String, priority: Int32) {
        self.class = `class`
        self.priority = priority
    }
}

public struct OrlixEnvironmentCPUAffinity: Codable, Equatable, Sendable {
    public let mask: String

    public init(mask: String) {
        self.mask = mask
    }
}

public enum OrlixEnvironmentMountSource: Codable, Equatable, Sendable {
	case documents
	case securityScopedExternal(bookmarkID: String)
	case hostPath(String)
}

public struct OrlixEnvironmentMount: Codable, Equatable, Sendable {
	public let source: OrlixEnvironmentMountSource
	public let targetPath: String
	public let readOnly: Bool
	public let noExec: Bool

	public static func documents(
		targetPath: String,
		readOnly: Bool = false,
		noExec: Bool = false
	) throws -> OrlixEnvironmentMount {
		try validateLinuxMountTarget(targetPath)
		return OrlixEnvironmentMount(
			source: .documents,
			targetPath: targetPath,
			readOnly: readOnly,
			noExec: noExec
		)
	}

	public static func securityScopedExternal(
		bookmarkID: String,
		targetPath: String,
		readOnly: Bool = false,
		noExec: Bool = false
	) throws -> OrlixEnvironmentMount {
		try validateSecurityScopedBookmarkID(bookmarkID)
		try validateLinuxMountTarget(targetPath)
		return OrlixEnvironmentMount(
			source: .securityScopedExternal(bookmarkID: bookmarkID),
			targetPath: targetPath,
			readOnly: readOnly,
			noExec: noExec
		)
	}

	public static func hostPath(
		_ hostPath: String,
		targetPath: String,
		readOnly: Bool = false,
		noExec: Bool = false
	) throws -> OrlixEnvironmentMount {
		try validateHostMountPath(hostPath)
		try validateLinuxMountTarget(targetPath)
		return OrlixEnvironmentMount(
			source: .hostPath(hostPath),
			targetPath: targetPath,
			readOnly: readOnly,
			noExec: noExec
		)
	}

	private init(
		source: OrlixEnvironmentMountSource,
		targetPath: String,
		readOnly: Bool,
		noExec: Bool
	) {
		self.source = source
		self.targetPath = targetPath
		self.readOnly = readOnly
		self.noExec = noExec
	}

	private enum CodingKeys: String, CodingKey {
		case source
		case targetPath
		case readOnly
		case noExec
	}

    public init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        let source = try container.decode(
            OrlixEnvironmentMountSource.self,
            forKey: .source
		)
		let targetPath = try container.decode(String.self, forKey: .targetPath)
		let readOnly = try container.decode(Bool.self, forKey: .readOnly)
		let noExec = try container.decodeIfPresent(Bool.self, forKey: .noExec) ?? false

		switch source {
		case .documents:
			self = try .documents(
				targetPath: targetPath,
				readOnly: readOnly,
				noExec: noExec
			)
		case let .securityScopedExternal(bookmarkID):
			self = try .securityScopedExternal(
				bookmarkID: bookmarkID,
				targetPath: targetPath,
				readOnly: readOnly,
				noExec: noExec
			)
		case let .hostPath(hostPath):
			self = try .hostPath(
				hostPath,
				targetPath: targetPath,
				readOnly: readOnly,
				noExec: noExec
			)
		}
	}

    public func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
		try container.encode(source, forKey: .source)
		try container.encode(targetPath, forKey: .targetPath)
		try container.encode(readOnly, forKey: .readOnly)
		try container.encode(noExec, forKey: .noExec)
	}
}

public struct OrlixEnvironmentTmpfsMount: Codable, Equatable, Sendable {
	public let targetPath: String
	public let readOnly: Bool
	public let noSuid: Bool
	public let noDev: Bool
	public let noExec: Bool
	public let data: String?

	public init(
		targetPath: String,
		readOnly: Bool = false,
		noSuid: Bool = false,
		noDev: Bool = false,
		noExec: Bool = false,
		data: String? = nil
	) throws {
		try validateLinuxTmpfsTarget(targetPath)
		try validateTmpfsMountData(data)
		self.targetPath = targetPath
		self.readOnly = readOnly
		self.noSuid = noSuid
		self.noDev = noDev
		self.noExec = noExec
		self.data = data
	}
}

@_spi(OrlixPrivateTesting)
public struct OrlixHostDirectoryRegistration: Equatable, Sendable {
    public let identifier: String
    public let hostPath: String
    public let readOnly: Bool

    public init(
        identifier: String,
        hostPath: String,
        readOnly: Bool
    ) {
        self.identifier = identifier
        self.hostPath = hostPath
        self.readOnly = readOnly
    }
}

@_spi(OrlixPrivateTesting)
public enum OrlixEnvironmentMountError: Error, Equatable, Sendable {
	case invalidSourceIdentifier(String)
	case invalidTargetPath(String)
	case reservedTargetPath(String)
	case invalidTmpfsData(String)
}

private func validateSecurityScopedBookmarkID(_ bookmarkID: String) throws {
	guard !bookmarkID.isEmpty,
		!bookmarkID.contains("/"),
          !bookmarkID.contains("\\"),
          !bookmarkID.contains("\u{0}"),
          bookmarkID != ".",
          bookmarkID != "..",
          !bookmarkID.hasPrefix("."),
          !bookmarkID.contains("..")
    else {
		throw OrlixEnvironmentMountError.invalidSourceIdentifier(bookmarkID)
	}
}

private func validateHostMountPath(_ hostPath: String) throws {
	guard hostPath.hasPrefix("/"),
		!hostPath.contains("\u{0}"),
		!hostPath.contains("//"),
		!hostPath.split(separator: "/").contains("."),
		!hostPath.split(separator: "/").contains("..")
	else {
		throw OrlixEnvironmentMountError.invalidSourceIdentifier(hostPath)
	}
}

private func validateLinuxMountTarget(_ targetPath: String) throws {
	guard targetPath.hasPrefix("/"),
		!targetPath.contains("\u{0}"),
          !targetPath.contains("//"),
          !targetPath.split(separator: "/").contains("..")
    else {
        throw OrlixEnvironmentMountError.invalidTargetPath(targetPath)
    }

    let reservedTargets = [
        "/",
        "/dev",
        "/proc",
        "/run",
        "/sys",
        "/tmp"
    ]
    if reservedTargets.contains(targetPath) ||
        reservedTargets.contains(where: { targetPath.hasPrefix($0 + "/") }) {
        throw OrlixEnvironmentMountError.reservedTargetPath(targetPath)
    }
}

private func validateLinuxTmpfsTarget(_ targetPath: String) throws {
	guard targetPath.hasPrefix("/"),
	      targetPath != "/",
	      !targetPath.contains("\u{0}"),
	      !targetPath.contains("//"),
	      !targetPath.split(separator: "/").contains(where: { $0 == "." || $0 == ".." })
	else {
		throw OrlixEnvironmentMountError.invalidTargetPath(targetPath)
	}
	let reserved = ["/proc", "/sys", "/dev", "/sys/fs/cgroup"]
	for path in reserved {
		if targetPath == path || targetPath.hasPrefix("\(path)/") {
			throw OrlixEnvironmentMountError.reservedTargetPath(targetPath)
		}
	}
}

private func validateTmpfsMountData(_ data: String?) throws {
	guard let data else {
		return
	}
	guard !data.isEmpty,
	      !data.contains("\u{0}"),
	      !data.contains("\n"),
	      !data.contains("\r"),
	      data.count <= 512
	else {
		throw OrlixEnvironmentMountError.invalidTmpfsData(data)
	}
}

@_spi(OrlixPrivateTesting)
public struct OrlixEnvironmentRootMount: Codable, Equatable, Sendable {
    public let baseDevicePath: String
    public let stateDevicePath: String
    public let lowerMountPath: String
    public let stateMountPath: String
    public let overlayMountPath: String
    public let upperDirectoryPath: String
    public let workDirectoryPath: String
    public let finalRootPath: String
    public let filesystemType: String
    public let overlayFilesystemType: String
    public let baseReadOnly: Bool

    public static let defaultOverlay = OrlixEnvironmentRootMount(
        baseDevicePath: "/dev/vda",
        stateDevicePath: "/dev/vdb",
        lowerMountPath: "/lower",
        stateMountPath: "/state",
        overlayMountPath: "/newroot",
        upperDirectoryPath: "/state/upper",
        workDirectoryPath: "/state/work",
        finalRootPath: "/",
        filesystemType: "ext4",
        overlayFilesystemType: "overlay",
        baseReadOnly: true
    )

    public init(
        baseDevicePath: String,
        stateDevicePath: String,
        lowerMountPath: String,
        stateMountPath: String,
        overlayMountPath: String,
        upperDirectoryPath: String,
        workDirectoryPath: String,
        finalRootPath: String,
        filesystemType: String,
        overlayFilesystemType: String,
        baseReadOnly: Bool
    ) {
        self.baseDevicePath = baseDevicePath
        self.stateDevicePath = stateDevicePath
        self.lowerMountPath = lowerMountPath
        self.stateMountPath = stateMountPath
        self.overlayMountPath = overlayMountPath
        self.upperDirectoryPath = upperDirectoryPath
        self.workDirectoryPath = workDirectoryPath
        self.finalRootPath = finalRootPath
        self.filesystemType = filesystemType
        self.overlayFilesystemType = overlayFilesystemType
        self.baseReadOnly = baseReadOnly
    }
}

@_spi(OrlixPrivateTesting)
public struct OrlixEnvironmentStorageLayout: Equatable, Sendable {
    public let environmentID: String
    public let rootDirectory: URL
    public let baseImageURL: URL
    public let stateImageURL: URL
    public let importScratchDirectory: URL
    public let downloadCacheDirectory: URL

	public static func layout(
		forEnvironmentID environmentID: String,
		policy: OrlixStoragePolicy = .current,
		fileManager: FileManager = .default
    ) throws -> OrlixEnvironmentStorageLayout {
        let storageID = try storageSafeID(environmentID)
        let linuxStateRoot = try policy.linuxStateDirectory(fileManager: fileManager)
        let cacheRoot = try policy.cacheDirectory(fileManager: fileManager)
        let scratchRoot = policy.scratchDirectory(fileManager: fileManager)
        return layout(
            forEnvironmentID: environmentID,
            storageID: storageID,
            linuxStateRoot: linuxStateRoot,
            cacheRoot: cacheRoot,
            scratchRoot: scratchRoot
        )
    }

    public static func layout(
        forEnvironmentID environmentID: String,
        linuxStateRoot: URL,
        cacheRoot: URL,
        scratchRoot: URL
    ) throws -> OrlixEnvironmentStorageLayout {
        try layout(
            forEnvironmentID: environmentID,
            storageID: storageSafeID(environmentID),
            linuxStateRoot: linuxStateRoot,
            cacheRoot: cacheRoot,
            scratchRoot: scratchRoot
        )
    }

    private static func layout(
        forEnvironmentID environmentID: String,
        storageID: String,
        linuxStateRoot: URL,
        cacheRoot: URL,
        scratchRoot: URL
    ) -> OrlixEnvironmentStorageLayout {
        let environmentRoot = linuxStateRoot
            .appendingPathComponent("environments", isDirectory: true)
            .appendingPathComponent(storageID, isDirectory: true)

        return OrlixEnvironmentStorageLayout(
            environmentID: environmentID,
            rootDirectory: environmentRoot,
            baseImageURL: environmentRoot
                .appendingPathComponent("base.ext4", isDirectory: false),
            stateImageURL: environmentRoot
                .appendingPathComponent("state.ext4", isDirectory: false),
            importScratchDirectory: scratchRoot
                .appendingPathComponent("imports", isDirectory: true)
                .appendingPathComponent(storageID, isDirectory: true),
            downloadCacheDirectory: cacheRoot
                .appendingPathComponent("downloads", isDirectory: true)
		)
	}

	public static func validateEnvironmentID(_ environmentID: String) throws {
		_ = try storageSafeID(environmentID)
	}

    static func storageSafeID(_ id: String) throws -> String {
		guard !id.isEmpty,
              !id.contains("/"),
              !id.contains("\\"),
              !id.utf8.contains(0)
        else {
            throw OrlixEnvironmentStorageLayoutError.invalidEnvironmentID(id)
        }

        var result = ""
        result.reserveCapacity(id.count)
        for character in id.unicodeScalars {
            switch character.value {
            case 48...57, 65...90, 97...122:
                result.unicodeScalars.append(character)
            case 45, 46, 95:
                result.unicodeScalars.append(character)
            default:
                result.append("-")
            }
        }

        guard result != "." && result != "..",
              !result.hasPrefix("."),
              !result.contains(".."),
              !result.contains("/")
        else {
            throw OrlixEnvironmentStorageLayoutError.invalidEnvironmentID(id)
        }
        return result
    }
}

@_spi(OrlixPrivateTesting)
public enum OrlixEnvironmentStorageLayoutError:
    Error,
    Equatable,
    Sendable
{
    case invalidEnvironmentID(String)
}

@_spi(OrlixPrivateTesting)
public struct OrlixEnvironmentRootImage: Equatable, Sendable {
    public static let defaultKernelCommandLine =
        "console=ttyS0 console=hvc0 rdinit=/init orlix.root=overlay orlix.profile=development"
    public static let defaultExecCommandLineKey = "orlix.exec"
    public static let defaultArgumentCommandLineKeyPrefix = "orlix.argv"
    public static let defaultEnvironmentCommandLineKeyPrefix = "orlix.env"
    public static let defaultWorkingDirectoryCommandLineKey = "orlix.cwd"
    public static let defaultUserIDCommandLineKey = "orlix.uid"
    public static let defaultGroupIDCommandLineKey = "orlix.gid"
    public static let defaultSupplementaryGroupCommandLineKeyPrefix = "orlix.suppgid"
    public static let defaultCapabilitiesBoundingCommandLineKey = "orlix.cap.bounding"
    public static let defaultCapabilitiesPermittedCommandLineKey = "orlix.cap.permitted"
    public static let defaultCapabilitiesInheritableCommandLineKey = "orlix.cap.inheritable"
    public static let defaultCapabilitiesEffectiveCommandLineKey = "orlix.cap.effective"
	public static let defaultCapabilitiesAmbientCommandLineKey = "orlix.cap.ambient"
	public static let defaultNoNewPrivilegesCommandLineKey = "orlix.nonewprivs"
	public static let defaultCloseAdditionalFdsCommandLineKey = "orlix.closefds"
	public static let defaultTerminalCommandLineKey = "orlix.terminal"
	public static let defaultTerminalRowsCommandLineKey = "orlix.terminal.rows"
	public static let defaultTerminalColumnsCommandLineKey = "orlix.terminal.cols"
	public static let defaultOOMScoreAdjustmentCommandLineKey = "orlix.oomscoreadj"
    public static let defaultSchedulerPolicyCommandLineKey = "orlix.scheduler.policy"
    public static let defaultSchedulerPriorityCommandLineKey = "orlix.scheduler.priority"
    public static let defaultIOPriorityClassCommandLineKey = "orlix.ioprio.class"
    public static let defaultIOPriorityPriorityCommandLineKey = "orlix.ioprio.priority"
    public static let defaultCPUAffinityCommandLineKey = "orlix.cpuaffinity"
    public static let defaultPersonalityCommandLineKey = "orlix.personality"
    public static let defaultUmaskCommandLineKey = "orlix.umask"
public static let defaultRlimitCommandLineKeyPrefix = "orlix.rlimit"
	public static let defaultHostMountTargetCommandLineKey = "orlix.mount.host0.target"
	public static let defaultHostMountReadOnlyCommandLineKey = "orlix.mount.host0.readonly"
	public static let defaultHostMountNoExecCommandLineKey = "orlix.mount.host0.noexec"
	public static let hostMountCommandLineKeyPrefix = "orlix.mount.host"
	public static let tmpfsMountCommandLineKeyPrefix = "orlix.mount.tmpfs"
	public static let hostnameCommandLineKey = "orlix.hostname"
    public static let domainnameCommandLineKey = "orlix.domainname"
    public static let rootReadonlyCommandLineKey = "orlix.root.readonly"
    public static let rootPropagationCommandLineKey = "orlix.root.propagation"
    public static let sysctlCommandLineKeyPrefix = "orlix.sysctl"
    public static let maskedPathCommandLineKeyPrefix = "orlix.maskedpath"
	public static let readonlyPathCommandLineKeyPrefix = "orlix.readonlypath"
	public static let cgroupsPathCommandLineKey = "orlix.cgroups.path"
public static let cgroupPidsMaxCommandLineKey = "orlix.cgroups.pids.max"
public static let cgroupCPUMaxCommandLineKey = "orlix.cgroups.cpu.max"
public static let cgroupCPUWeightCommandLineKey = "orlix.cgroups.cpu.weight"
public static let cgroupMemoryMaxCommandLineKey = "orlix.cgroups.memory.max"
public static let cgroupIOWeightCommandLineKey = "orlix.cgroups.io.weight"
public static let cgroupUnifiedCommandLineKeyPrefix = "orlix.cgroups.unified"
public static let deviceNodePathCommandLineKeyPrefix = "orlix.device.path"
public static let deviceNodeTypeCommandLineKeyPrefix = "orlix.device.type"
public static let deviceNodeMajorCommandLineKeyPrefix = "orlix.device.major"
public static let deviceNodeMinorCommandLineKeyPrefix = "orlix.device.minor"
    public static let deviceNodeModeCommandLineKeyPrefix = "orlix.device.mode"
    public static let deviceNodeUIDCommandLineKeyPrefix = "orlix.device.uid"
    public static let deviceNodeGIDCommandLineKeyPrefix = "orlix.device.gid"
    public static let timeOffsetCommandLineKeyPrefix = "orlix.timeoffset"
    public static let uidMappingCommandLineKeyPrefix = "orlix.uidmap"
    public static let gidMappingCommandLineKeyPrefix = "orlix.gidmap"
    public static let namespaceCommandLineKeyPrefix = "orlix.namespace"
public static let namespacePathCommandLineKeyPrefix = "orlix.namespacepath"
public static let defaultHostDirectoryIdentifier = "orlix-host0"
public static let hostDirectoryIdentifierPrefix = "orlix-host"

    public let environmentID: String
    public let rootImageIdentifier: String
    public let baseImageURL: URL
    public let stateImageURL: URL
    public let bootConfig: OrlixBootConfig
    public let hostDirectories: [OrlixHostDirectoryRegistration]
    public let hostDirectoryExtendedAttributes: [OrlixHostDirectoryExtendedAttribute]

    public static func materialized(
        descriptor: OrlixEnvironmentDescriptor,
        layout: OrlixEnvironmentStorageLayout,
        bootProfile: OrlixBootProfile = .development,
        kernelCommandLine: String? = defaultKernelCommandLine,
		documentsDirectory: URL? = nil,
		securityScopedExternalDirectories: [String: URL] = [:],
        hostDirectoryExtendedAttributes: [OrlixHostDirectoryExtendedAttribute] = [],
        fileManager: FileManager = .default
    ) throws -> OrlixEnvironmentRootImage {
        guard descriptor.id == layout.environmentID else {
            throw OrlixEnvironmentRootImageError.environmentMismatch(
                descriptorID: descriptor.id,
                layoutID: layout.environmentID
            )
        }
        try validateImageURL(
            layout.baseImageURL,
            expectedRoot: layout.rootDirectory,
            fileManager: fileManager
        )
        try validateImageURL(
            layout.stateImageURL,
            expectedRoot: layout.rootDirectory,
            fileManager: fileManager
        )
		let hostDirectories = try hostDirectoryRegistrations(
			mounts: descriptor.mounts,
			documentsDirectory: documentsDirectory,
			securityScopedExternalDirectories: securityScopedExternalDirectories,
            fileManager: fileManager
        )
        let resolvedCommandLine = try materializedKernelCommandLine(
            descriptor: descriptor,
            kernelCommandLine: kernelCommandLine
        )
        return OrlixEnvironmentRootImage(
            environmentID: descriptor.id,
            rootImageIdentifier: descriptor.rootImageIdentifier,
            baseImageURL: layout.baseImageURL,
            stateImageURL: layout.stateImageURL,
            bootConfig: OrlixBootConfig(
                profile: bootProfile,
                kernelCommandLine: resolvedCommandLine,
                rootImageIdentifier: descriptor.rootImageIdentifier
            ),
            hostDirectories: hostDirectories,
            hostDirectoryExtendedAttributes: hostDirectoryExtendedAttributes
        )
    }

    public func registerWithHostAdapter() -> Bool {
        OrlixOSPayload.registerMaterializedRootImage(self)
    }

    @_spi(OrlixPrivateTesting)
    public func registerWithHostAdapterForTesting(
        payloadBundlePath: String,
        initrdResource: String,
        baseBlockDevice: UInt32,
        stateBlockDevice: UInt32,
        stateBlockMinimumBytes: UInt64
    ) -> Bool {
        OrlixOSPayload.registerMaterializedRootImage(
            self,
            payloadBundlePath: payloadBundlePath,
            productResources: OrlixOSPayload.ProductRootResources(
                initrdResource: initrdResource,
                baseBlockResource: "",
                stateBlockResource: "",
                baseBlockDevice: baseBlockDevice,
                stateBlockDevice: stateBlockDevice,
                stateBlockMinimumBytes: stateBlockMinimumBytes
            )
        )
    }

    private static func validateImageURL(
        _ url: URL,
        expectedRoot: URL,
        fileManager: FileManager
    ) throws {
        let root = expectedRoot.standardizedFileURL
        let image = url.standardizedFileURL
        let rootPath = root.path.hasSuffix("/") ? root.path : root.path + "/"
        guard image.path.hasPrefix(rootPath) else {
            throw OrlixEnvironmentRootImageError.imageOutsideEnvironmentRoot(
                image.path
            )
        }

        var isDirectory = ObjCBool(false)
        guard fileManager.fileExists(
            atPath: image.path,
            isDirectory: &isDirectory
        ) else {
            throw OrlixEnvironmentRootImageError.missingImage(image.path)
        }
        if isDirectory.boolValue {
            throw OrlixEnvironmentRootImageError.imageIsDirectory(image.path)
        }
    }

    static func materializedKernelCommandLine(
        descriptor: OrlixEnvironmentDescriptor,
        kernelCommandLine: String?
    ) throws -> String? {
        guard kernelCommandLine == defaultKernelCommandLine else {
            return kernelCommandLine
        }

        guard let command = descriptor.defaultCommand.first else {
            throw OrlixEnvironmentRootImageError.invalidDefaultCommand("")
        }
        try validateExecCommand(command)
        let executionTokens = try materializedExecutionTokens(descriptor)
        return ([defaultKernelCommandLine] + executionTokens).joined(separator: " ")
    }

    private static func validateExecCommand(_ command: String) throws {
        guard !command.isEmpty,
              !command.contains("\u{0}"),
              command.hasPrefix("/") || !command.contains("/")
        else {
            throw OrlixEnvironmentRootImageError.invalidDefaultCommand(command)
        }
    }

    private static func validateArgument(_ argument: String) throws {
        guard !argument.contains("\u{0}")
        else {
            throw OrlixEnvironmentRootImageError.invalidDefaultArgument(argument)
        }
    }

    private static func validateEnvironmentEntry(
        key: String,
        value: String
    ) throws -> String {
        guard !key.isEmpty,
              !key.contains("="),
              !key.contains("\u{0}"),
              !value.contains("\u{0}")
        else {
            throw OrlixEnvironmentRootImageError.invalidDefaultEnvironment(key)
        }
        return "\(key)=\(value)"
    }

    private static func validateWorkingDirectory(_ path: String) throws {
        guard path.hasPrefix("/"),
              !path.isEmpty,
              !path.contains("\u{0}")
        else {
            throw OrlixEnvironmentRootImageError.invalidDefaultWorkingDirectory(path)
        }
    }

    private static func validateRlimit(_ rlimit: OrlixEnvironmentRlimit) throws {
        guard !rlimit.type.isEmpty,
            !rlimit.type.contains(":"),
            !rlimit.type.contains("\u{0}"),
            rlimit.soft <= rlimit.hard
        else {
            throw OrlixEnvironmentRootImageError.invalidDefaultRlimit(rlimit.type)
        }
    }

    private static func validateSysctl(key: String, value: String) throws -> String {
        let allowedScalars = CharacterSet.alphanumerics
            .union(CharacterSet(charactersIn: "._-"))
        guard !key.isEmpty,
            key.unicodeScalars.allSatisfy({ allowedScalars.contains($0) }),
            key.first != ".",
            key.last != ".",
            !key.contains(".."),
            !value.contains("\u{0}"),
            !value.contains("\n"),
            !value.contains("\r")
        else {
            throw OrlixEnvironmentRootImageError.invalidDefaultSysctl(key)
        }

        return "\(key)=\(value)"
    }

    private static func validateRuntimePath(_ path: String) throws -> String {
        let components = path.split(separator: "/", omittingEmptySubsequences: false)
        guard path.hasPrefix("/"),
            path != "/",
            !path.contains("\u{0}"),
            !components.contains(where: { $0 == ".." }),
            !components.contains(where: { $0 == "." })
        else {
            throw OrlixEnvironmentRootImageError.invalidRuntimePath(path)
        }

        return path
    }

	private static func validateCgroupPidsLimit(_ limit: Int64) throws -> String {
		guard limit >= -1 else {
			throw OrlixEnvironmentRootImageError.invalidCgroupPidsLimit(limit)
		}

		return limit == -1 ? "max" : String(limit)
	}

	private static func validateCgroupMemoryMax(_ limit: Int64) throws -> String {
		guard limit >= -1 else {
			throw OrlixEnvironmentRootImageError.invalidCgroupMemoryMax(limit)
		}

		return limit == -1 ? "max" : String(limit)
	}

private static func validateCgroupCPUMax(_ cpuMax: OrlixEnvironmentCgroupCPUMax) throws -> String {
guard cpuMax.quotaMicros == -1 || cpuMax.quotaMicros > 0 else {
throw OrlixEnvironmentRootImageError.invalidCgroupCPUMax(cpuMax)
		}
		guard cpuMax.periodMicros > 0 else {
			throw OrlixEnvironmentRootImageError.invalidCgroupCPUMax(cpuMax)
		}

let quota = cpuMax.quotaMicros == -1 ? "max" : String(cpuMax.quotaMicros)
return "\(quota) \(cpuMax.periodMicros)"
}

private static func validateCgroupCPUWeight(_ weight: UInt64) throws -> String {
guard (1...10_000).contains(weight) else {
throw OrlixEnvironmentRootImageError.invalidCgroupCPUWeight(weight)
}

return String(weight)
}

private static func validateCgroupIOWeight(_ weight: UInt64) throws -> String {
guard (1...10_000).contains(weight) else {
throw OrlixEnvironmentRootImageError.invalidCgroupIOWeight(weight)
}
return String(weight)
}

private static func validateCgroupUnified(
_ entry: OrlixEnvironmentCgroupUnifiedEntry
) throws -> String {
let supportedFiles = Set([
"pids.max",
"cpu.max",
"cpu.weight",
"memory.max",
"io.weight",
"io.max"
])
guard supportedFiles.contains(entry.file),
!entry.value.isEmpty,
!entry.file.contains("\u{0}"),
!entry.file.contains("/"),
!entry.value.contains("\u{0}"),
!entry.value.contains("\n"),
!entry.value.contains("\r")
else {
throw OrlixEnvironmentRootImageError.invalidCgroupUnified(entry.file)
}
return "\(entry.file)=\(entry.value)"
}

private static func validateDeviceNode(
_ node: OrlixEnvironmentDeviceNode
) throws -> OrlixEnvironmentDeviceNode {
let supportedTypes = Set(["c", "b", "u", "p"])
guard supportedTypes.contains(node.type),
node.fileMode <= 0o7777
else {
throw OrlixEnvironmentRootImageError.invalidDeviceNode(node.path)
}
_ = try validateRuntimePath(node.path)
if node.type == "p" {
return node
}
return node
}

    private static func validateNamespace(_ namespace: String) throws -> String {
        let supportedNamespaces = Set(["mount", "ipc", "uts", "network", "cgroup", "pid", "time", "user"])
        guard supportedNamespaces.contains(namespace) else {
            throw OrlixEnvironmentRootImageError.invalidNamespace(namespace)
        }

        return namespace
    }

    private static func validateIDMapping(_ mapping: OrlixEnvironmentIDMapping) throws -> String {
        guard mapping.size > 0 else {
            throw OrlixEnvironmentRootImageError.invalidIDMapping(mapping)
        }
        return "\(mapping.containerID):\(mapping.hostID):\(mapping.size)"
    }

    private static func validateTimeOffset(_ offset: OrlixEnvironmentTimeOffset) throws -> String {
        let supportedClocks = Set(["monotonic", "boottime"])
        guard supportedClocks.contains(offset.clock),
              offset.nanosecs >= 0,
              offset.nanosecs < 1_000_000_000
        else {
            throw OrlixEnvironmentRootImageError.invalidTimeOffset(offset.clock)
        }
        return "\(offset.clock):\(offset.secs):\(offset.nanosecs)"
    }

    private static func validateNamespaceJoin(type: String, path: String) throws -> String {
        "\(try validateNamespace(type))=\(try validateRuntimePath(path))"
    }

    private static func validatePersonalityDomain(_ domain: String) throws -> String {
        let supportedDomains = Set(["LINUX", "LINUX32"])
        guard supportedDomains.contains(domain) else {
            throw OrlixEnvironmentRootImageError.invalidDefaultPersonalityDomain(domain)
        }
        return domain
    }

	private static func hostDirectoryRegistrations(
		mounts: [OrlixEnvironmentMount],
		documentsDirectory: URL?,
		securityScopedExternalDirectories: [String: URL],
		fileManager: FileManager
	) throws -> [OrlixHostDirectoryRegistration] {
		var resolvedDocumentsDirectory: URL?
		return try mounts.enumerated().map { index, mount in
			let directory: URL
			switch mount.source {
			case .documents:
				if let resolvedDocumentsDirectory {
					directory = resolvedDocumentsDirectory
				} else {
					let resolved = try documentsDirectory ?? fileManager.url(
						for: .documentDirectory,
						in: .userDomainMask,
						appropriateFor: nil,
						create: false
					)
					resolvedDocumentsDirectory = resolved
					directory = resolved
				}
		case let .securityScopedExternal(bookmarkID):
			guard let resolved = securityScopedExternalDirectories[bookmarkID] else {
				throw OrlixEnvironmentRootImageError.missingLinuxMountBackend(mount)
			}
			directory = resolved
		case let .hostPath(hostPath):
			directory = URL(fileURLWithPath: hostPath, isDirectory: true)
		}
		guard directory.isFileURL else {
			throw OrlixEnvironmentRootImageError.missingLinuxMountBackend(mount)
		}
		var isDirectory = ObjCBool(false)
		guard fileManager.fileExists(atPath: directory.path, isDirectory: &isDirectory),
			isDirectory.boolValue
		else {
			throw OrlixEnvironmentRootImageError.missingLinuxMountBackend(mount)
		}
		return OrlixHostDirectoryRegistration(
				identifier: "\(hostDirectoryIdentifierPrefix)\(index)",
				hostPath: directory.path,
				readOnly: mount.readOnly
			)
		}
	}

 private static func materializedExecutionTokens(
        _ descriptor: OrlixEnvironmentDescriptor
    ) throws -> [String] {
        var tokens = [
            "\(defaultExecCommandLineKey)=\(percentEncoded(descriptor.defaultCommand[0]))"
        ]
        for (index, argument) in descriptor.defaultCommand.enumerated() {
            try validateArgument(argument)
            tokens.append(
                "\(defaultArgumentCommandLineKeyPrefix)\(index)=\(percentEncoded(argument))"
            )
        }
        for (index, entry) in descriptor.defaultEnvironment
            .sorted(by: { $0.key < $1.key })
            .enumerated()
        {
            let assignment = try validateEnvironmentEntry(
                key: entry.key,
                value: entry.value
            )
            tokens.append(
                "\(defaultEnvironmentCommandLineKeyPrefix)\(index)=\(percentEncoded(assignment))"
            )
        }
        try validateWorkingDirectory(descriptor.defaultWorkingDirectory)
        tokens.append(
            "\(defaultWorkingDirectoryCommandLineKey)=\(percentEncoded(descriptor.defaultWorkingDirectory))"
        )
        tokens.append("\(defaultUserIDCommandLineKey)=\(descriptor.defaultUserID)")
        tokens.append("\(defaultGroupIDCommandLineKey)=\(descriptor.defaultGroupID)")
        for (index, groupID) in descriptor.defaultSupplementaryGroups.enumerated() {
            tokens.append("\(defaultSupplementaryGroupCommandLineKeyPrefix)\(index)=\(groupID)")
        }
        if let capabilities = descriptor.defaultCapabilities {
            tokens.append("\(defaultCapabilitiesBoundingCommandLineKey)=\(capabilities.bounding.joined(separator: ","))")
            tokens.append("\(defaultCapabilitiesPermittedCommandLineKey)=\(capabilities.permitted.joined(separator: ","))")
            tokens.append("\(defaultCapabilitiesInheritableCommandLineKey)=\(capabilities.inheritable.joined(separator: ","))")
            tokens.append("\(defaultCapabilitiesEffectiveCommandLineKey)=\(capabilities.effective.joined(separator: ","))")
            tokens.append("\(defaultCapabilitiesAmbientCommandLineKey)=\(capabilities.ambient.joined(separator: ","))")
        }
        if descriptor.defaultNoNewPrivileges {
            tokens.append("\(defaultNoNewPrivilegesCommandLineKey)=1")
        }
		if descriptor.defaultCloseAdditionalFds {
			tokens.append("\(defaultCloseAdditionalFdsCommandLineKey)=1")
		}
		if let defaultTerminal = descriptor.defaultTerminal {
			tokens.append(
				"\(defaultTerminalCommandLineKey)=\(defaultTerminal ? 1 : 0)"
			)
		}
		if let rows = descriptor.defaultTerminalRows,
		   let columns = descriptor.defaultTerminalColumns,
		   rows > 0,
		   columns > 0 {
			tokens.append("\(defaultTerminalRowsCommandLineKey)=\(rows)")
			tokens.append("\(defaultTerminalColumnsCommandLineKey)=\(columns)")
		}
		if let defaultOOMScoreAdjustment = descriptor.defaultOOMScoreAdjustment {
			tokens.append("\(defaultOOMScoreAdjustmentCommandLineKey)=\(defaultOOMScoreAdjustment)")
		}
        if let defaultScheduler = descriptor.defaultScheduler {
            tokens.append("\(defaultSchedulerPolicyCommandLineKey)=\(defaultScheduler.policy)")
            tokens.append("\(defaultSchedulerPriorityCommandLineKey)=\(defaultScheduler.priority)")
        }
        if let defaultIOPriority = descriptor.defaultIOPriority {
            tokens.append("\(defaultIOPriorityClassCommandLineKey)=\(defaultIOPriority.class)")
            tokens.append("\(defaultIOPriorityPriorityCommandLineKey)=\(defaultIOPriority.priority)")
        }
        if let defaultCPUAffinity = descriptor.defaultCPUAffinity {
            tokens.append("\(defaultCPUAffinityCommandLineKey)=\(defaultCPUAffinity.mask)")
        }
        if let defaultPersonalityDomain = descriptor.defaultPersonalityDomain {
            tokens.append(
                "\(defaultPersonalityCommandLineKey)=\(try validatePersonalityDomain(defaultPersonalityDomain))"
            )
        }
        if let defaultUmask = descriptor.defaultUmask {
            tokens.append("\(defaultUmaskCommandLineKey)=\(defaultUmask)")
        }
        for (index, rlimit) in descriptor.defaultRlimits.enumerated() {
            try validateRlimit(rlimit)
            tokens.append(
                "\(defaultRlimitCommandLineKeyPrefix)\(index)=\(rlimit.type):\(rlimit.soft):\(rlimit.hard)"
            )
        }
        if let hostname = descriptor.hostname, !hostname.isEmpty {
            tokens.append("\(hostnameCommandLineKey)=\(percentEncoded(hostname))")
        }
        if let domainname = descriptor.domainname, !domainname.isEmpty {
            tokens.append("\(domainnameCommandLineKey)=\(percentEncoded(domainname))")
        }
        if descriptor.rootReadonly {
            tokens.append("\(rootReadonlyCommandLineKey)=1")
        }
        if descriptor.rootPropagation != .private {
            tokens.append("\(rootPropagationCommandLineKey)=\(descriptor.rootPropagation.rawValue)")
        }
        for (index, entry) in descriptor.sysctls
            .sorted(by: { $0.key < $1.key })
            .enumerated()
        {
            let assignment = try validateSysctl(key: entry.key, value: entry.value)
            tokens.append(
                "\(sysctlCommandLineKeyPrefix)\(index)=\(percentEncoded(assignment))"
            )
        }
        for (index, path) in descriptor.maskedPaths.sorted().enumerated() {
            tokens.append(
                "\(maskedPathCommandLineKeyPrefix)\(index)=\(percentEncoded(try validateRuntimePath(path)))"
            )
        }
        for (index, path) in descriptor.readonlyPaths.sorted().enumerated() {
            tokens.append(
                "\(readonlyPathCommandLineKeyPrefix)\(index)=\(percentEncoded(try validateRuntimePath(path)))"
            )
        }
        if let cgroupsPath = descriptor.cgroupsPath, !cgroupsPath.isEmpty {
            tokens.append(
                "\(cgroupsPathCommandLineKey)=\(percentEncoded(try validateRuntimePath(cgroupsPath)))"
            )
        }
		if let cgroupPidsLimit = descriptor.cgroupPidsLimit {
			guard descriptor.cgroupsPath != nil else {
				throw OrlixEnvironmentRootImageError.invalidCgroupPidsLimit(cgroupPidsLimit)
			}
			tokens.append(
				"\(cgroupPidsMaxCommandLineKey)=\(try validateCgroupPidsLimit(cgroupPidsLimit))"
			)
		}
		if let cgroupCPUMax = descriptor.cgroupCPUMax {
			guard descriptor.cgroupsPath != nil else {
				throw OrlixEnvironmentRootImageError.invalidCgroupCPUMax(cgroupCPUMax)
			}
			tokens.append(
				"\(cgroupCPUMaxCommandLineKey)=\(percentEncoded(try validateCgroupCPUMax(cgroupCPUMax)))"
			)
		}
		if let cgroupCPUWeight = descriptor.cgroupCPUWeight {
			guard descriptor.cgroupsPath != nil else {
				throw OrlixEnvironmentRootImageError.invalidCgroupCPUWeight(cgroupCPUWeight)
			}
			tokens.append(
				"\(cgroupCPUWeightCommandLineKey)=\(try validateCgroupCPUWeight(cgroupCPUWeight))"
			)
		}
		if let cgroupMemoryMax = descriptor.cgroupMemoryMax {
			guard descriptor.cgroupsPath != nil else {
				throw OrlixEnvironmentRootImageError.invalidCgroupMemoryMax(cgroupMemoryMax)
			}
			tokens.append(
				"\(cgroupMemoryMaxCommandLineKey)=\(try validateCgroupMemoryMax(cgroupMemoryMax))"
			)
		}
	if let cgroupIOWeight = descriptor.cgroupIOWeight {
		guard descriptor.cgroupsPath != nil else {
			throw OrlixEnvironmentRootImageError.invalidCgroupIOWeight(cgroupIOWeight)
		}
		tokens.append(
			"\(cgroupIOWeightCommandLineKey)=\(try validateCgroupIOWeight(cgroupIOWeight))"
		)
	}
	for (index, entry) in descriptor.cgroupUnified.enumerated() {
		guard descriptor.cgroupsPath != nil else {
			throw OrlixEnvironmentRootImageError.invalidCgroupUnified(entry.file)
		}
		tokens.append(
			"\(cgroupUnifiedCommandLineKeyPrefix)\(index)=\(percentEncoded(try validateCgroupUnified(entry)))"
		)
	}
	for (index, node) in descriptor.deviceNodes.enumerated() {
		let validated = try validateDeviceNode(node)
		tokens.append(
			"\(deviceNodePathCommandLineKeyPrefix)\(index)=\(percentEncoded(validated.path))"
		)
		tokens.append("\(deviceNodeTypeCommandLineKeyPrefix)\(index)=\(validated.type)")
		tokens.append("\(deviceNodeMajorCommandLineKeyPrefix)\(index)=\(validated.major)")
		tokens.append("\(deviceNodeMinorCommandLineKeyPrefix)\(index)=\(validated.minor)")
            tokens.append("\(deviceNodeModeCommandLineKeyPrefix)\(index)=\(validated.fileMode)")
            tokens.append("\(deviceNodeUIDCommandLineKeyPrefix)\(index)=\(validated.uid)")
            tokens.append("\(deviceNodeGIDCommandLineKeyPrefix)\(index)=\(validated.gid)")
        }
        if !descriptor.timeOffsets.isEmpty && !descriptor.namespaces.contains("time") {
            throw OrlixEnvironmentRootImageError.invalidTimeOffset("time")
        }
        for (index, offset) in descriptor.timeOffsets.enumerated() {
            tokens.append(
                "\(timeOffsetCommandLineKeyPrefix)\(index)=\(try validateTimeOffset(offset))"
            )
        }
        if (!descriptor.uidMappings.isEmpty || !descriptor.gidMappings.isEmpty) &&
            !descriptor.namespaces.contains("user")
        {
            throw OrlixEnvironmentRootImageError.invalidIDMapping(
                descriptor.uidMappings.first ?? descriptor.gidMappings[0]
            )
        }
        for (index, mapping) in descriptor.uidMappings.enumerated() {
            tokens.append(
                "\(uidMappingCommandLineKeyPrefix)\(index)=\(try validateIDMapping(mapping))"
            )
        }
        for (index, mapping) in descriptor.gidMappings.enumerated() {
            tokens.append(
                "\(gidMappingCommandLineKeyPrefix)\(index)=\(try validateIDMapping(mapping))"
            )
        }
        for (index, namespace) in descriptor.namespaces.sorted().enumerated() {
            tokens.append(
                "\(namespaceCommandLineKeyPrefix)\(index)=\(try validateNamespace(namespace))"
            )
        }
        for (index, entry) in descriptor.namespacePaths
            .sorted(by: { $0.key < $1.key })
            .enumerated()
        {
            let assignment = try validateNamespaceJoin(type: entry.key, path: entry.value)
		tokens.append(
			"\(namespacePathCommandLineKeyPrefix)\(index)=\(percentEncoded(assignment))"
		)
	}
	for (index, mount) in descriptor.tmpfsMounts.enumerated() {
		tokens.append(
			"\(tmpfsMountCommandLineKeyPrefix)\(index).target=\(percentEncoded(mount.targetPath))"
		)
		if mount.readOnly {
			tokens.append("\(tmpfsMountCommandLineKeyPrefix)\(index).readonly=1")
		}
		if mount.noSuid {
			tokens.append("\(tmpfsMountCommandLineKeyPrefix)\(index).nosuid=1")
		}
		if mount.noDev {
			tokens.append("\(tmpfsMountCommandLineKeyPrefix)\(index).nodev=1")
		}
		if mount.noExec {
			tokens.append("\(tmpfsMountCommandLineKeyPrefix)\(index).noexec=1")
		}
		if let data = mount.data {
			tokens.append(
				"\(tmpfsMountCommandLineKeyPrefix)\(index).data=\(percentEncoded(data))"
			)
		}
	}
for (index, mount) in descriptor.mounts.enumerated() {
tokens.append(
"\(hostMountCommandLineKeyPrefix)\(index).target=\(percentEncoded(mount.targetPath))"
)
		if mount.readOnly {
			tokens.append("\(hostMountCommandLineKeyPrefix)\(index).readonly=1")
		}
		if mount.noExec {
			tokens.append("\(hostMountCommandLineKeyPrefix)\(index).noexec=1")
		}
	}
	return tokens
}

    private static func percentEncoded(_ value: String) -> String {
        var encoded = ""
        for byte in value.utf8 {
            if isKernelCommandLineTokenByte(byte) {
                encoded.append(Character(UnicodeScalar(byte)))
            } else {
                encoded += String(format: "%%%02X", byte)
            }
        }
        return encoded
    }

    private static func isKernelCommandLineTokenByte(_ byte: UInt8) -> Bool {
        switch byte {
        case UInt8(ascii: "A")...UInt8(ascii: "Z"),
             UInt8(ascii: "a")...UInt8(ascii: "z"),
             UInt8(ascii: "0")...UInt8(ascii: "9"),
             UInt8(ascii: "/"),
             UInt8(ascii: "."),
             UInt8(ascii: "_"),
             UInt8(ascii: "-"),
             UInt8(ascii: ":"),
             UInt8(ascii: "="),
             UInt8(ascii: ","),
             UInt8(ascii: "+"),
             UInt8(ascii: "@"):
            return true
        default:
            return false
        }
    }
}

@_spi(OrlixPrivateTesting)
public enum OrlixEnvironmentRootImageError:
    Error,
    Equatable,
    Sendable
{
    case environmentMismatch(descriptorID: String, layoutID: String)
    case imageOutsideEnvironmentRoot(String)
    case missingImage(String)
    case imageIsDirectory(String)
    case invalidDefaultCommand(String)
    case invalidDefaultArgument(String)
    case invalidDefaultEnvironment(String)
    case invalidDefaultWorkingDirectory(String)
    case invalidDefaultRlimit(String)
    case invalidDefaultPersonalityDomain(String)
    case invalidDefaultSysctl(String)
	case invalidRuntimePath(String)
	case invalidCgroupPidsLimit(Int64)
	case invalidCgroupCPUMax(OrlixEnvironmentCgroupCPUMax)
	case invalidCgroupCPUWeight(UInt64)
	case invalidCgroupMemoryMax(Int64)
    case invalidCgroupIOWeight(UInt64)
    case invalidCgroupUnified(String)
    case invalidDeviceNode(String)
    case invalidTimeOffset(String)
    case invalidIDMapping(OrlixEnvironmentIDMapping)
    case invalidNamespace(String)
    case missingLinuxMountBackend(OrlixEnvironmentMount)
}

@_spi(OrlixPrivateTesting)
public struct OrlixEnvironmentRegistry: Sendable {
    public let linuxStateRoot: URL
    public let cacheRoot: URL
    public let scratchRoot: URL

    public init(
        policy: OrlixStoragePolicy = .current,
        fileManager: FileManager = .default
    ) throws {
        self.init(
            linuxStateRoot: try policy.linuxStateDirectory(
                fileManager: fileManager
            ),
            cacheRoot: try policy.cacheDirectory(fileManager: fileManager),
            scratchRoot: policy.scratchDirectory(fileManager: fileManager)
        )
    }

    public init(
        linuxStateRoot: URL,
        cacheRoot: URL,
        scratchRoot: URL
    ) {
        self.linuxStateRoot = linuxStateRoot
        self.cacheRoot = cacheRoot
        self.scratchRoot = scratchRoot
    }

    public func layout(
        forEnvironmentID environmentID: String
    ) throws -> OrlixEnvironmentStorageLayout {
        try OrlixEnvironmentStorageLayout.layout(
            forEnvironmentID: environmentID,
            linuxStateRoot: linuxStateRoot,
            cacheRoot: cacheRoot,
            scratchRoot: scratchRoot
        )
    }

    public func descriptorURL(
        forEnvironmentID environmentID: String
    ) throws -> URL {
        try layout(forEnvironmentID: environmentID).rootDirectory
            .appendingPathComponent("environment.json", isDirectory: false)
    }

    public func prepareStorage(
        forEnvironmentID environmentID: String,
        fileManager: FileManager = .default
    ) throws -> OrlixEnvironmentStorageLayout {
        let layout = try layout(forEnvironmentID: environmentID)
        try fileManager.createDirectory(
            at: layout.rootDirectory,
            withIntermediateDirectories: true
        )
        try fileManager.createDirectory(
            at: layout.importScratchDirectory,
            withIntermediateDirectories: true
        )
        try fileManager.createDirectory(
            at: layout.downloadCacheDirectory,
            withIntermediateDirectories: true
        )
        return layout
    }

    public func prepareStorage(
        for descriptor: OrlixEnvironmentDescriptor,
        fileManager: FileManager = .default
    ) throws -> OrlixEnvironmentStorageLayout {
        try prepareStorage(
            forEnvironmentID: descriptor.id,
            fileManager: fileManager
        )
    }

    public func save(
        _ descriptor: OrlixEnvironmentDescriptor,
        fileManager: FileManager = .default
    ) throws {
        let layout = try prepareStorage(
            for: descriptor,
            fileManager: fileManager
        )
        let data = try JSONEncoder.orlixEnvironmentEncoder.encode(descriptor)
        try data.write(
            to: layout.rootDirectory
                .appendingPathComponent("environment.json", isDirectory: false),
            options: [.atomic]
        )
    }

	public func load(
		environmentID: String,
		fileManager: FileManager = .default
	) throws -> OrlixEnvironmentDescriptor {
        let url = try descriptorURL(forEnvironmentID: environmentID)
		let data = try Data(contentsOf: url)
		return try JSONDecoder().decode(OrlixEnvironmentDescriptor.self, from: data)
	}

	public func delete(
		environmentID: String,
		fileManager: FileManager = .default
	) throws {
		let layout = try layout(forEnvironmentID: environmentID)
		if fileManager.fileExists(atPath: layout.rootDirectory.path) {
			try fileManager.removeItem(at: layout.rootDirectory)
		}
		if fileManager.fileExists(atPath: layout.importScratchDirectory.path) {
			try fileManager.removeItem(at: layout.importScratchDirectory)
		}
	}

	public func list(
		fileManager: FileManager = .default
	) throws -> [OrlixEnvironmentDescriptor] {
        let environmentsRoot = linuxStateRoot
            .appendingPathComponent("environments", isDirectory: true)
        guard let contents = try? fileManager.contentsOfDirectory(
            at: environmentsRoot,
            includingPropertiesForKeys: [.isDirectoryKey],
            options: [.skipsHiddenFiles]
        ) else {
            return []
        }

        var descriptors: [OrlixEnvironmentDescriptor] = []
        for directory in contents.sorted(by: { $0.path < $1.path }) {
            let descriptorURL = directory.appendingPathComponent(
                "environment.json",
                isDirectory: false
            )
            guard fileManager.fileExists(atPath: descriptorURL.path) else {
                continue
            }
            let data = try Data(contentsOf: descriptorURL)
            descriptors.append(
                try JSONDecoder().decode(
                    OrlixEnvironmentDescriptor.self,
                    from: data
                )
            )
        }
        return descriptors
    }

    public func materializedRootImage(
        forEnvironmentID environmentID: String,
        kernelCommandLine: String? = OrlixEnvironmentRootImage.defaultKernelCommandLine,
        fileManager: FileManager = .default
    ) throws -> OrlixEnvironmentRootImage {
        try OrlixEnvironmentRootImage.materialized(
            descriptor: load(environmentID: environmentID, fileManager: fileManager),
            layout: layout(forEnvironmentID: environmentID),
            kernelCommandLine: kernelCommandLine,
            fileManager: fileManager
        )
    }

    public func copyEnvironment(
        from parentID: String,
        to environmentID: String,
        rootImageIdentifier: String,
        fileManager: FileManager = .default
    ) throws -> OrlixEnvironmentDescriptor {
        let parent = try load(environmentID: parentID, fileManager: fileManager)
        let parentLayout = try layout(forEnvironmentID: parentID)
        let destinationLayout = try layout(forEnvironmentID: environmentID)

        guard fileManager.fileExists(atPath: parentLayout.baseImageURL.path) else {
            throw OrlixEnvironmentCopyError.missingParentImage(
                parentLayout.baseImageURL.path
            )
        }
        guard fileManager.fileExists(atPath: parentLayout.stateImageURL.path) else {
            throw OrlixEnvironmentCopyError.missingParentImage(
                parentLayout.stateImageURL.path
            )
        }
        guard !fileManager.fileExists(atPath: destinationLayout.rootDirectory.path)
        else {
            throw OrlixEnvironmentCopyError.destinationExists(environmentID)
        }

        try fileManager.createDirectory(
            at: destinationLayout.rootDirectory,
            withIntermediateDirectories: true
        )
        try fileManager.createDirectory(
            at: destinationLayout.importScratchDirectory,
            withIntermediateDirectories: true
        )
        try fileManager.createDirectory(
            at: destinationLayout.downloadCacheDirectory,
            withIntermediateDirectories: true
        )
        do {
            try fileManager.copyItem(
                at: parentLayout.baseImageURL,
                to: destinationLayout.baseImageURL
            )
            try fileManager.copyItem(
                at: parentLayout.stateImageURL,
                to: destinationLayout.stateImageURL
            )
            let descriptor = OrlixEnvironmentDescriptor(
                id: environmentID,
                source: .copiedEnvironment(parentID: parentID),
                platform: parent.platform,
                rootImageIdentifier: rootImageIdentifier,
                defaultCommand: parent.defaultCommand,
                defaultEnvironment: parent.defaultEnvironment,
                defaultWorkingDirectory: parent.defaultWorkingDirectory,
                defaultUserID: parent.defaultUserID,
                defaultGroupID: parent.defaultGroupID,
                defaultSupplementaryGroups: parent.defaultSupplementaryGroups,
					defaultCapabilities: parent.defaultCapabilities,
					defaultNoNewPrivileges: parent.defaultNoNewPrivileges,
					defaultCloseAdditionalFds: parent.defaultCloseAdditionalFds,
					defaultTerminal: parent.defaultTerminal,
					defaultTerminalRows: parent.defaultTerminalRows,
					defaultTerminalColumns: parent.defaultTerminalColumns,
					defaultOOMScoreAdjustment: parent.defaultOOMScoreAdjustment,
                defaultScheduler: parent.defaultScheduler,
                defaultIOPriority: parent.defaultIOPriority,
                defaultCPUAffinity: parent.defaultCPUAffinity,
                defaultUmask: parent.defaultUmask,
                defaultRlimits: parent.defaultRlimits,
                defaultPersonalityDomain: parent.defaultPersonalityDomain,
                rootMount: parent.rootMount,
                rootReadonly: parent.rootReadonly,
                rootPropagation: parent.rootPropagation,
                sysctls: parent.sysctls,
                maskedPaths: parent.maskedPaths,
				readonlyPaths: parent.readonlyPaths,
cgroupsPath: parent.cgroupsPath,
cgroupPidsLimit: parent.cgroupPidsLimit,
			cgroupCPUMax: parent.cgroupCPUMax,
				cgroupCPUWeight: parent.cgroupCPUWeight,
				cgroupMemoryMax: parent.cgroupMemoryMax,
                cgroupIOWeight: parent.cgroupIOWeight,
                cgroupUnified: parent.cgroupUnified,
                deviceNodes: parent.deviceNodes,
                timeOffsets: parent.timeOffsets,
                uidMappings: parent.uidMappings,
					gidMappings: parent.gidMappings,
					namespaces: parent.namespaces,
					namespacePaths: parent.namespacePaths,
					tmpfsMounts: parent.tmpfsMounts,
					mounts: parent.mounts
				)
            try save(descriptor, fileManager: fileManager)
            return descriptor
        } catch {
            try? fileManager.removeItem(at: destinationLayout.rootDirectory)
            throw error
        }
    }
}

private extension JSONEncoder {
    static var orlixEnvironmentEncoder: JSONEncoder {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        return encoder
    }
}

@_spi(OrlixPrivateTesting)
public enum OrlixEnvironmentCopyError: Error, Equatable, Sendable {
    case destinationExists(String)
    case missingParentImage(String)
}
