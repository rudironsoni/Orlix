import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package struct RemoteFileTransferLimits: Sendable {
    package let maxDepth: Int
    package let maxEntries: Int
    package let maxEntriesPerDirectory: Int
    package let maxFileBytes: UInt64
    package let maxAggregateBytes: UInt64
    package let maxElapsed: Duration
    package let minimumFreeBytes: UInt64

    // Int64.max is a representation safety ceiling, not a product file-size limit.
    // It keeps byte counts compatible with Foundation and POSIX file APIs.
    static package let standard = RemoteFileTransferLimits(
        maxDepth: 64,
        maxEntries: 1_000_000,
        maxEntriesPerDirectory: 100_000,
        maxFileBytes: UInt64(Int64.max),
        maxAggregateBytes: UInt64(Int64.max),
        maxElapsed: .seconds(86_400),
        minimumFreeBytes: 64 * 1_024 * 1_024
    )
}

nonisolated package enum RemoteFileTransferError: LocalizedError, Equatable, Sendable {
    case depthLimit(maximum: Int)
    case entryLimit(maximum: Int)
    case directoryEntryLimit(maximum: Int)
    case elapsedTimeLimit
    case fileSizeLimit(maximumBytes: UInt64)
    case aggregateSizeLimit(maximumBytes: UInt64)
    case insufficientCapacity(requiredBytes: UInt64, availableBytes: UInt64)
    case byteCountOverflow

    package var errorDescription: String? {
        switch self {
        case .depthLimit(let maximum):
            return String(
                format: String(localized: "The folder depth exceeds the safety limit of %lld levels."),
                Int64(maximum)
            )
        case .entryLimit(let maximum):
            return String(
                format: String(localized: "The transfer exceeds the safety limit of %lld items."),
                Int64(maximum)
            )
        case .directoryEntryLimit(let maximum):
            return String(
                format: String(localized: "A folder exceeds the safety limit of %lld direct items."),
                Int64(maximum)
            )
        case .elapsedTimeLimit:
            return String(localized: "The transfer plan exceeded its 24-hour safety limit.")
        case .fileSizeLimit(let maximumBytes):
            return String(
                format: String(localized: "The file exceeds the safety limit of %@."),
                Self.formattedByteCount(maximumBytes)
            )
        case .aggregateSizeLimit(let maximumBytes):
            return String(
                format: String(localized: "The transfer exceeds the safety limit of %@."),
                Self.formattedByteCount(maximumBytes)
            )
        case .insufficientCapacity(let requiredBytes, let availableBytes):
            return String(
                format: String(localized: "The transfer needs %@, but only %@ is available after the free-space reserve."),
                Self.formattedByteCount(requiredBytes),
                Self.formattedByteCount(availableBytes)
            )
        case .byteCountOverflow:
            return String(localized: "The transfer size cannot be represented safely.")
        }
    }

    private static func formattedByteCount(_ byteCount: UInt64) -> String {
        ByteCountFormatter.string(fromByteCount: Int64(clamping: byteCount), countStyle: .file)
    }
}

nonisolated package struct RemoteFileTransferPlanNode: Sendable {
    package let entry: RemoteFileEntry
    package let children: [RemoteFileTransferPlanNode]

    package var unitCount: Int {
        children.reduce(1) { $0 + $1.unitCount }
    }
}

nonisolated package struct LocalFileIdentity: Hashable, Sendable {
    package let device: UInt64
    package let inode: UInt64
}

nonisolated package struct LocalUploadPlanNode: Sendable {
    package enum Kind: Sendable {
        case regularFile(byteCount: UInt64)
        case directory
    }

    package let sourceURL: URL
    package let name: String
    package let identity: LocalFileIdentity
    package let kind: Kind
    package let children: [LocalUploadPlanNode]

    package var unitCount: Int {
        children.reduce(1) { $0 + $1.unitCount }
    }
}

nonisolated package struct RemoteFileTraversalBudget: Sendable {
    private(set) var admittedEntries = 0
    package let limits: RemoteFileTransferLimits
    private let deadline: ContinuousClock.Instant

    package init(limits: RemoteFileTransferLimits = .standard) {
        self.limits = limits
        deadline = ContinuousClock.now.advanced(by: limits.maxElapsed)
    }

    mutating package func admit(depth: Int) throws {
        try checkTime()
        guard depth <= limits.maxDepth else {
            throw RemoteFileTransferError.depthLimit(maximum: limits.maxDepth)
        }
        guard admittedEntries < limits.maxEntries else {
            throw RemoteFileTransferError.entryLimit(maximum: limits.maxEntries)
        }
        admittedEntries += 1
    }

    mutating package func directoryReadLimit() throws -> Int {
        try checkTime()
        let remaining = limits.maxEntries - admittedEntries
        guard remaining > 0 else {
            throw RemoteFileTransferError.entryLimit(maximum: limits.maxEntries)
        }
        return min(remaining, limits.maxEntriesPerDirectory)
    }

    mutating package func checkTime() throws {
        guard ContinuousClock.now < deadline else {
            throw RemoteFileTransferError.elapsedTimeLimit
        }
    }
}

nonisolated package struct RemoteFileTransferByteBudget: Sendable {
    private(set) var consumedBytes: UInt64 = 0
    package let limits: RemoteFileTransferLimits

    package init(limits: RemoteFileTransferLimits = .standard) {
        self.limits = limits
    }

    package func downloadLimit(
        reportedBytes: UInt64?,
        availableCapacity: UInt64
    ) throws -> UInt64 {
        guard consumedBytes <= limits.maxAggregateBytes else {
            throw RemoteFileTransferError.byteCountOverflow
        }
        let remaining = limits.maxAggregateBytes - min(consumedBytes, limits.maxAggregateBytes)
        let storageCapacity = availableCapacity > limits.minimumFreeBytes
            ? availableCapacity - limits.minimumFreeBytes
            : 0

        if let reportedBytes {
            guard reportedBytes <= limits.maxFileBytes else {
                throw RemoteFileTransferError.fileSizeLimit(maximumBytes: limits.maxFileBytes)
            }
            guard reportedBytes <= remaining else {
                throw RemoteFileTransferError.aggregateSizeLimit(maximumBytes: limits.maxAggregateBytes)
            }
            guard reportedBytes <= storageCapacity else {
                throw RemoteFileTransferError.insufficientCapacity(
                    requiredBytes: reportedBytes,
                    availableBytes: storageCapacity
                )
            }
            return reportedBytes
        }

        let limit = min(limits.maxFileBytes, remaining, storageCapacity)
        guard limit > 0 else {
            throw RemoteFileTransferError.insufficientCapacity(
                requiredBytes: 1,
                availableBytes: storageCapacity
            )
        }
        return limit
    }

    mutating package func record(_ byteCount: UInt64) throws {
        guard byteCount <= limits.maxFileBytes else {
            throw RemoteFileTransferError.fileSizeLimit(maximumBytes: limits.maxFileBytes)
        }
        guard consumedBytes <= limits.maxAggregateBytes,
              byteCount <= limits.maxAggregateBytes - consumedBytes else {
            throw RemoteFileTransferError.aggregateSizeLimit(maximumBytes: limits.maxAggregateBytes)
        }
        consumedBytes += byteCount
    }

    package func validateUploadCapacity(_ capacity: RemoteFileFilesystemCapacity) throws {
        guard case .known(let status) = capacity else { return }
        let usableBytes = status.availableBytes > limits.minimumFreeBytes
            ? status.availableBytes - limits.minimumFreeBytes
            : 0
        guard consumedBytes <= usableBytes else {
            throw RemoteFileTransferError.insufficientCapacity(
                requiredBytes: consumedBytes,
                availableBytes: usableBytes
            )
        }
    }
}
