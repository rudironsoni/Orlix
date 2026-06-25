import Foundation
import zlib

@_spi(OrlixPrivateTesting)
public struct OrlixOCIImageLayoutImport: Equatable, Sendable {
    public let manifestDigest: String
    public let configDigest: String
    public let platform: String
    public let layers: [OrlixOCIImageLayer]
    public let rootfsDiffIDs: [String]
    public let processDefaults: OrlixOCIProcessDefaults
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIImageLayer: Equatable, Sendable {
    public let digest: String
    public let mediaType: String
    public let size: UInt64
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIProcessDefaults: Equatable, Sendable {
    public let environment: [String: String]
    public let entrypoint: [String]
    public let command: [String]
    public let workingDirectory: String?
    public let user: String?
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIImageLayoutReader: Sendable {
    public init() {}

    public func readLayout(
        at layoutURL: URL,
        platform: String = "linux/arm64"
    ) throws -> OrlixOCIImageLayoutImport {
        let requestedPlatform = try OrlixOCIPlatform(platform)
        let layoutData = try Data(
            contentsOf: layoutURL.appendingPathComponent("oci-layout")
        )
        let layout = try JSONDecoder().decode(OCILayout.self, from: layoutData)
        guard layout.imageLayoutVersion == "1.0.0" else {
            throw OrlixOCIImageLayoutError.invalidLayoutVersion(
                layout.imageLayoutVersion
            )
        }

        let indexData = try Data(contentsOf: layoutURL.appendingPathComponent("index.json"))
        let index = try JSONDecoder().decode(OCIIndex.self, from: indexData)
        guard let manifestDescriptor = index.manifests.first(
            where: { $0.platform == requestedPlatform }
        ) else {
            throw OrlixOCIImageLayoutError.missingPlatform(platform)
        }
        guard Self.supportedManifestMediaTypes.contains(manifestDescriptor.mediaType)
        else {
            throw OrlixOCIImageLayoutError.unsupportedManifestMediaType(
                manifestDescriptor.mediaType
            )
        }

        let manifestData = try OrlixOCIImageLayoutBlobStore.verifiedBlob(
            manifestDescriptor.digest,
            expectedSize: manifestDescriptor.size,
            under: layoutURL
        )
        let manifest = try JSONDecoder().decode(OCIManifest.self, from: manifestData)
        guard manifest.schemaVersion == 2 else {
            throw OrlixOCIImageLayoutError.invalidManifestSchemaVersion(
                manifest.schemaVersion
            )
        }
        guard Self.supportedConfigMediaTypes.contains(manifest.config.mediaType)
        else {
            throw OrlixOCIImageLayoutError.unsupportedConfigMediaType(
                manifest.config.mediaType
            )
        }
        let configData = try OrlixOCIImageLayoutBlobStore.verifiedBlob(
            manifest.config.digest,
            expectedSize: manifest.config.size,
            under: layoutURL
        )
        let imageConfig = try JSONDecoder().decode(OCIImageConfig.self, from: configData)
        let layers = try manifest.layers.map { layer -> OrlixOCIImageLayer in
            _ = try OrlixOCIImageLayoutBlobStore.verifiedBlob(
                layer.digest,
                expectedSize: layer.size,
                under: layoutURL
            )
            return OrlixOCIImageLayer(
                digest: layer.digest,
                mediaType: layer.mediaType,
                size: layer.size
            )
        }
        let rootfsDiffIDs = try validatedRootfsDiffIDs(
            imageConfig.rootfs,
            layerCount: layers.count
        )

        return OrlixOCIImageLayoutImport(
            manifestDigest: manifestDescriptor.digest,
            configDigest: manifest.config.digest,
            platform: platform,
            layers: layers,
            rootfsDiffIDs: rootfsDiffIDs,
            processDefaults: OrlixOCIProcessDefaults(
                environment: try environmentDictionary(imageConfig.config?.env ?? []),
                entrypoint: try commandVector(imageConfig.config?.entrypoint ?? []),
                command: try commandVector(imageConfig.config?.cmd ?? []),
                workingDirectory: imageConfig.config?.workingDir,
                user: imageConfig.config?.user
            )
        )
    }

    private func validatedRootfsDiffIDs(
        _ rootfs: OCIRootfs?,
        layerCount: Int
    ) throws -> [String] {
        guard let rootfs else {
            return []
        }
        if let type = rootfs.type, type != "layers" {
            throw OrlixOCIImageLayoutError.unsupportedRootfsType(type)
        }
        let values = rootfs.diffIDs ?? []
        guard !values.isEmpty else {
            return []
        }
        guard values.count == layerCount else {
            throw OrlixOCIImageLayoutError.rootfsDiffIDCountMismatch(
                expected: layerCount,
                actual: values.count
            )
        }
        return try values.map { value in
            let digest = try OrlixOCIDigest(value)
            return "\(digest.algorithm):\(digest.hex)"
        }
    }

    private static let supportedManifestMediaTypes: Set<String> = [
        "application/vnd.oci.image.manifest.v1+json",
        "application/vnd.docker.distribution.manifest.v2+json"
    ]

    private static let supportedConfigMediaTypes: Set<String> = [
        "application/vnd.oci.image.config.v1+json",
        "application/vnd.docker.container.image.v1+json"
    ]

    private func environmentDictionary(_ values: [String]) throws -> [String: String] {
        var result: [String: String] = [:]
        for value in values {
            guard let separator = value.firstIndex(of: "=") else {
                throw OrlixOCIImageLayoutError.invalidEnvironmentEntry(value)
            }
            let key = String(value[..<separator])
            guard !key.isEmpty,
                  !key.contains("\u{0}"),
                  !value.contains("\u{0}")
            else {
                throw OrlixOCIImageLayoutError.invalidEnvironmentEntry(value)
            }
			guard result[key] == nil else {
				throw OrlixOCIImageLayoutError.invalidEnvironmentEntry(value)
			}
			result[key] = String(value[value.index(after: separator)...])
        }
        return result
    }

    private func commandVector(_ values: [String]) throws -> [String] {
        for value in values {
            guard !value.contains("\u{0}") else {
                throw OrlixOCIImageLayoutError.invalidCommandEntry(value)
            }
        }
        return values
    }
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIImageLayoutImportResult: Equatable, Sendable {
    public let descriptor: OrlixEnvironmentDescriptor
    public let storageLayout: OrlixEnvironmentStorageLayout
    public let stagingRootDirectory: URL
    public let materializationPlan: OrlixEnvironmentImageMaterializationPlan
    public let image: OrlixOCIImageLayoutImport
    public let manifest: [OrlixRootfsTarManifestEntry]
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIImageLayoutImporter: Sendable {
    public init() {}

    public func importLayout(
        at layoutURL: URL,
        environmentID: String,
        registry: OrlixEnvironmentRegistry,
        rootImageIdentifier: String,
        platform: String = "linux/arm64",
        fileManager: FileManager = .default
    ) throws -> OrlixOCIImageLayoutImportResult {
        let plannedStorageLayout = try registry.layout(forEnvironmentID: environmentID)
        if fileManager.fileExists(atPath: plannedStorageLayout.rootDirectory.path) {
            throw OrlixOCIImageLayoutError.destinationExists(environmentID)
        }
        let image = try OrlixOCIImageLayoutReader().readLayout(
            at: layoutURL,
            platform: platform
        )
        let storageLayout = try registry.prepareStorage(
            forEnvironmentID: environmentID,
            fileManager: fileManager
        )
        var shouldRemoveEnvironmentRootOnFailure = true
        let temporaryRootDirectory = storageLayout.importScratchDirectory
            .appendingPathComponent(
                ".rootfs.oci-import-\(UUID().uuidString)",
                isDirectory: true
            )
        do {
            let stagingRootDirectory = storageLayout.importScratchDirectory
                .appendingPathComponent("rootfs", isDirectory: true)
            try? fileManager.removeItem(at: temporaryRootDirectory)
            try fileManager.createDirectory(
                at: temporaryRootDirectory,
                withIntermediateDirectories: true
            )

            var manifest: [OrlixRootfsTarManifestEntry] = []
            let applicator = OrlixOCIImageLayerApplicator(fileManager: fileManager)
            let decoder = OrlixOCILayerDecoder()
            for (index, layer) in image.layers.enumerated() {
                let data = try OrlixOCIImageLayoutBlobStore.verifiedBlob(
                    layer.digest,
                    expectedSize: layer.size,
                    under: layoutURL
                )
                let tarData = try decoder.decode(
                    layerData: data,
                    mediaType: layer.mediaType
                )
                try verifyDiffID(
                    image.rootfsDiffIDs,
                    layerIndex: index,
                    decodedLayerData: tarData
                )
                let application = try applicator.apply(
                    tarData,
                    to: temporaryRootDirectory
                )
                applyManifestChanges(application, to: &manifest)
            }
            let descriptor = try environmentDescriptor(
                environmentID: environmentID,
                rootImageIdentifier: rootImageIdentifier,
                image: image,
                rootDirectory: temporaryRootDirectory
            )
            if fileManager.fileExists(atPath: stagingRootDirectory.path) {
                try fileManager.removeItem(at: stagingRootDirectory)
            }
            try fileManager.moveItem(
                at: temporaryRootDirectory,
                to: stagingRootDirectory
            )
            let materializationPlan = try OrlixEnvironmentImageMaterializationPlan.plan(
                stagingRootDirectory: stagingRootDirectory,
                storageLayout: storageLayout
            )
            try materializationPlan.prepareInputTrees(fileManager: fileManager)
            try materializationPlan.writeBaseImageMetadataCommands(
                manifest: manifest,
                fileManager: fileManager
            )
            try materializationPlan.writeStateImageMetadataCommands(
                fileManager: fileManager
            )
            try registry.save(descriptor, fileManager: fileManager)
            shouldRemoveEnvironmentRootOnFailure = false
            return OrlixOCIImageLayoutImportResult(
                descriptor: descriptor,
                storageLayout: storageLayout,
                stagingRootDirectory: stagingRootDirectory,
                materializationPlan: materializationPlan,
                image: image,
                manifest: manifest
            )
        } catch {
            try? fileManager.removeItem(at: temporaryRootDirectory)
            if shouldRemoveEnvironmentRootOnFailure {
                try? fileManager.removeItem(at: storageLayout.rootDirectory)
            }
            throw error
        }
    }

    private func applyManifestChanges(
        _ application: OrlixOCIImageLayerApplication,
        to manifest: inout [OrlixRootfsTarManifestEntry]
    ) {
        for operation in application.operations {
            switch operation {
            case let .opaqueDirectory(directory):
                applyOpaqueDirectory(directory, to: &manifest)
            case let .removed(path):
                removePath(path, from: &manifest)
            case let .applied(entry):
                applyEntry(entry, to: &manifest)
            }
        }
    }

    private func applyOpaqueDirectory(
        _ directory: String,
        to manifest: inout [OrlixRootfsTarManifestEntry]
    ) {
        if directory.isEmpty {
            manifest.removeAll()
            return
        }
        let prefix = directory + "/"
        manifest.removeAll { entry in
            entry.path.hasPrefix(prefix)
        }
        if !manifest.contains(where: { $0.path == directory }) {
            manifest.append(opaqueDirectoryManifestEntry(directory))
        }
    }

    private func removePath(
        _ path: String,
        from manifest: inout [OrlixRootfsTarManifestEntry]
    ) {
        let prefix = path + "/"
        manifest.removeAll { entry in
            entry.path == path || entry.path.hasPrefix(prefix)
        }
    }

    private func applyEntry(
        _ entry: OrlixRootfsTarManifestEntry,
        to manifest: inout [OrlixRootfsTarManifestEntry]
    ) {
        let prefix = entry.path + "/"
        manifest.removeAll { existing in
            existing.path == entry.path ||
                (entry.type != .directory && existing.path.hasPrefix(prefix))
        }
        manifest.append(entry)
    }

    private func opaqueDirectoryManifestEntry(
        _ path: String
    ) -> OrlixRootfsTarManifestEntry {
        OrlixRootfsTarManifestEntry(
            path: path,
            size: 0,
            mode: 0o755,
            uid: 0,
            gid: 0,
            type: .directory,
            linkName: nil
        )
    }

    private func environmentDescriptor(
        environmentID: String,
        rootImageIdentifier: String,
        image: OrlixOCIImageLayoutImport,
        rootDirectory: URL
    ) throws -> OrlixEnvironmentDescriptor {
        var command = image.processDefaults.entrypoint + image.processDefaults.command
        if command.isEmpty {
            command = ["/bin/sh"]
        }
        let user = try userAndGroup(
            image.processDefaults.user,
            rootDirectory: rootDirectory
        )
        let workingDirectory = try validatedWorkingDirectory(
            image.processDefaults.workingDirectory
        )
        return OrlixEnvironmentDescriptor(
            id: environmentID,
            source: .ociLayout,
            platform: image.platform,
            rootImageIdentifier: rootImageIdentifier,
            defaultCommand: command,
            defaultEnvironment: image.processDefaults.environment,
            defaultWorkingDirectory: workingDirectory,
            defaultUserID: user.uid,
            defaultGroupID: user.gid
        )
    }

    private func validatedWorkingDirectory(_ value: String?) throws -> String {
        guard let value, !value.isEmpty else {
            return "/"
        }
        guard value.hasPrefix("/"),
              !value.contains("\u{0}")
        else {
            throw OrlixOCIImageLayoutError.invalidWorkingDirectory(value)
        }
        return value
    }

    private func userAndGroup(
        _ value: String?,
        rootDirectory: URL
    ) throws -> (uid: UInt32, gid: UInt32) {
        guard let value, !value.isEmpty else {
            return (0, 0)
        }
        let parts = value.split(
            separator: ":",
            maxSplits: 1,
            omittingEmptySubsequences: false
        )
        guard parts.count == 1 || parts.count == 2,
              !parts[0].isEmpty
        else {
            throw OrlixOCIImageLayoutError.unsupportedUser(value)
        }
        let account = try OrlixLinuxAccountDatabase(rootDirectory: rootDirectory)
        let uid: UInt32
        let defaultGID: UInt32
        if let numericUID = UInt32(parts[0]) {
            uid = numericUID
            defaultGID = 0
        } else if let user = account.user(named: String(parts[0])) {
            uid = user.uid
            defaultGID = user.gid
        } else {
            throw OrlixOCIImageLayoutError.unsupportedUser(value)
        }
        guard parts.count == 2 else {
            return (uid, defaultGID)
        }

        guard !parts[1].isEmpty else {
            throw OrlixOCIImageLayoutError.unsupportedUser(value)
        }
        if let numericGID = UInt32(parts[1]) {
            return (uid, numericGID)
        }
        if let group = account.group(named: String(parts[1])) {
            return (uid, group.gid)
        }
        throw OrlixOCIImageLayoutError.unsupportedUser(value)
    }
}

private struct OrlixLinuxAccountDatabase {
    struct User {
        let name: String
        let uid: UInt32
        let gid: UInt32
    }

    struct Group {
        let name: String
        let gid: UInt32
    }

    private let usersByName: [String: User]
    private let groupsByName: [String: Group]

    init(rootDirectory: URL) throws {
        self.usersByName = try Self.users(
            at: rootDirectory
                .appendingPathComponent("etc", isDirectory: true)
                .appendingPathComponent("passwd", isDirectory: false)
        )
        self.groupsByName = try Self.groups(
            at: rootDirectory
                .appendingPathComponent("etc", isDirectory: true)
                .appendingPathComponent("group", isDirectory: false)
        )
    }

    func user(named name: String) -> User? {
        usersByName[name]
    }

    func group(named name: String) -> Group? {
        groupsByName[name]
    }

    private static func users(at url: URL) throws -> [String: User] {
        guard FileManager.default.fileExists(atPath: url.path) else {
            return [:]
        }
        let contents = try String(contentsOf: url, encoding: .utf8)
        var users: [String: User] = [:]
        for line in contents.split(separator: "\n", omittingEmptySubsequences: false) {
            let fields = line.split(separator: ":", omittingEmptySubsequences: false)
            guard fields.count >= 4,
                  !fields[0].isEmpty,
                  let uid = UInt32(fields[2]),
                  let gid = UInt32(fields[3])
            else {
                continue
            }
            let name = String(fields[0])
            users[name] = User(name: name, uid: uid, gid: gid)
        }
        return users
    }

    private static func groups(at url: URL) throws -> [String: Group] {
        guard FileManager.default.fileExists(atPath: url.path) else {
            return [:]
        }
        let contents = try String(contentsOf: url, encoding: .utf8)
        var groups: [String: Group] = [:]
        for line in contents.split(separator: "\n", omittingEmptySubsequences: false) {
            let fields = line.split(separator: ":", omittingEmptySubsequences: false)
            guard fields.count >= 3,
                  !fields[0].isEmpty,
                  let gid = UInt32(fields[2])
            else {
                continue
            }
            let name = String(fields[0])
            groups[name] = Group(name: name, gid: gid)
        }
        return groups
    }
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIDigest: Equatable, Sendable {
    public let algorithm: String
    public let hex: String

    public init(_ value: String) throws {
        let parts = value.split(separator: ":", maxSplits: 1)
        guard parts.count == 2,
              parts[0] == "sha256",
              parts[1].count == 64,
              parts[1].allSatisfy({ $0.isHexDigit })
        else {
            throw OrlixOCIImageLayoutError.invalidDigest(value)
        }
        self.algorithm = String(parts[0])
        self.hex = String(parts[1]).lowercased()
    }

    public static func sha256Hex(_ data: Data) -> String {
        OrlixSHA256.hash(data).map { String(format: "%02x", $0) }.joined()
    }
}

@_spi(OrlixPrivateTesting)
public enum OrlixOCIImageLayoutError:
    Error,
    Equatable,
    Sendable
{
    case invalidDigest(String)
    case invalidLayoutVersion(String)
    case invalidManifestSchemaVersion(Int)
    case missingPlatform(String)
    case digestMismatch(expected: String, actual: String)
    case sizeMismatch(digest: String, expected: UInt64, actual: UInt64)
    case rootfsDiffIDCountMismatch(expected: Int, actual: Int)
    case rootfsDiffIDMismatch(layerIndex: Int, expected: String, actual: String)
    case unsupportedManifestMediaType(String)
    case unsupportedConfigMediaType(String)
    case unsupportedLayerMediaType(String)
    case unsupportedRootfsType(String)
    case unsupportedUser(String)
    case invalidWorkingDirectory(String)
    case invalidEnvironmentEntry(String)
    case invalidCommandEntry(String)
    case invalidWhiteout(String)
    case decompressionFailed(String)
    case decompressedLayerTooLarge(Int)
    case destinationExists(String)
}

private func verifyDiffID(
    _ diffIDs: [String],
    layerIndex: Int,
    decodedLayerData: Data
) throws {
    guard !diffIDs.isEmpty else {
        return
    }
    let actual = "sha256:\(OrlixOCIDigest.sha256Hex(decodedLayerData))"
    guard diffIDs[layerIndex] == actual else {
        throw OrlixOCIImageLayoutError.rootfsDiffIDMismatch(
            layerIndex: layerIndex,
            expected: diffIDs[layerIndex],
            actual: actual
        )
    }
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCILayerDecoder: Sendable {
    public static let defaultMaxDecodedLayerBytes = 8 * 1024 * 1024 * 1024

    public init() {}

    public func decode(
        layerData: Data,
        mediaType: String,
        maxDecodedBytes: Int = defaultMaxDecodedLayerBytes
    ) throws -> Data {
        switch mediaType {
        case "application/vnd.oci.image.layer.v1.tar",
             "application/vnd.oci.image.layer.nondistributable.v1.tar",
             "application/vnd.docker.image.rootfs.diff.tar":
            return layerData
        case "application/vnd.oci.image.layer.v1.tar+gzip",
             "application/vnd.oci.image.layer.nondistributable.v1.tar+gzip",
             "application/vnd.docker.image.rootfs.diff.tar.gzip":
            return try decodeGzip(layerData, maxDecodedBytes: maxDecodedBytes)
        default:
            throw OrlixOCIImageLayoutError.unsupportedLayerMediaType(mediaType)
        }
    }

    private func decodeGzip(
        _ data: Data,
        maxDecodedBytes: Int
    ) throws -> Data {
        guard maxDecodedBytes >= 0 else {
            throw OrlixOCIImageLayoutError.decompressedLayerTooLarge(maxDecodedBytes)
        }

        var stream = z_stream()
        let initStatus = inflateInit2_(
            &stream,
            MAX_WBITS + 32,
            ZLIB_VERSION,
            Int32(MemoryLayout<z_stream>.size)
        )
        guard initStatus == Z_OK else {
            throw OrlixOCIImageLayoutError.decompressionFailed(
                "inflateInit2 failed: \(initStatus)"
            )
        }
        defer {
            inflateEnd(&stream)
        }

        var output = Data()
        var input = Array(data)
        let chunkSize = 64 * 1024
        let status = input.withUnsafeMutableBytes { inputBuffer -> Int32 in
            guard let inputBase = inputBuffer.baseAddress else {
                return Z_DATA_ERROR
            }
            stream.next_in = inputBase.assumingMemoryBound(to: Bytef.self)
            stream.avail_in = uInt(inputBuffer.count)

            var status = Z_OK
            while status != Z_STREAM_END {
                var chunk = Array(repeating: UInt8(0), count: chunkSize)
                status = chunk.withUnsafeMutableBytes { outputBuffer -> Int32 in
                    stream.next_out = outputBuffer.baseAddress!
                        .assumingMemoryBound(to: Bytef.self)
                    stream.avail_out = uInt(outputBuffer.count)
                    return inflate(&stream, Z_NO_FLUSH)
                }

                let produced = chunkSize - Int(stream.avail_out)
                if produced > 0 {
                    output.append(chunk, count: produced)
                    if output.count > maxDecodedBytes {
                        return Z_MEM_ERROR
                    }
                }

                if status == Z_STREAM_END {
                    break
                }
                if status != Z_OK {
                    return status
                }
                if produced == 0 && stream.avail_in == 0 {
                    return Z_DATA_ERROR
                }
            }
            return status
        }

        guard status == Z_STREAM_END else {
            if status == Z_MEM_ERROR {
                throw OrlixOCIImageLayoutError.decompressedLayerTooLarge(
                    maxDecodedBytes
                )
            }
            throw OrlixOCIImageLayoutError.decompressionFailed(
                "inflate failed: \(status)"
            )
        }
        return output
    }
}

private enum OrlixOCIImageLayoutBlobStore {
    static func verifiedBlob(
        _ digest: String,
        expectedSize: UInt64,
        under layoutURL: URL
    ) throws -> Data {
        let parsed = try OrlixOCIDigest(digest)
        let url = layoutURL
            .appendingPathComponent("blobs", isDirectory: true)
            .appendingPathComponent(parsed.algorithm, isDirectory: true)
            .appendingPathComponent(parsed.hex, isDirectory: false)
        let data = try Data(contentsOf: url)
        guard UInt64(data.count) == expectedSize else {
            throw OrlixOCIImageLayoutError.sizeMismatch(
                digest: digest,
                expected: expectedSize,
                actual: UInt64(data.count)
            )
        }
        let actual = OrlixOCIDigest.sha256Hex(data)
        guard parsed.algorithm == "sha256", parsed.hex == actual else {
            throw OrlixOCIImageLayoutError.digestMismatch(
                expected: digest,
                actual: "sha256:\(actual)"
            )
        }
        return data
    }
}

private struct OrlixOCIImageLayerApplication {
    var operations: [OrlixOCIImageLayerOperation] = []
}

private enum OrlixOCIImageLayerOperation {
    case applied(OrlixRootfsTarManifestEntry)
    case removed(String)
    case opaqueDirectory(String)
}

private struct OrlixOCIImageLayerApplicator {
    let fileManager: FileManager

    func apply(
        _ data: Data,
        to rootDirectory: URL
    ) throws -> OrlixOCIImageLayerApplication {
        let root = rootDirectory.standardizedFileURL
        let records = try OrlixRootfsTarManifestReader().readRecords(from: data)
        var application = OrlixOCIImageLayerApplication()
        for record in records {
            if let whiteout = try applyWhiteout(record.entry, under: root) {
                switch whiteout {
                case let .removed(path):
                    application.operations.append(.removed(path))
                case let .opaqueDirectory(path):
                    application.operations.append(.opaqueDirectory(path))
                }
                continue
            }
            try materialize(record, data: data, under: root)
            application.operations.append(.applied(record.entry))
        }
        return application
    }

    private func applyWhiteout(
        _ entry: OrlixRootfsTarManifestEntry,
        under root: URL
    ) throws -> OrlixOCIWhiteout? {
        let components = entry.path.split(separator: "/", omittingEmptySubsequences: true)
        guard let name = components.last else {
            return nil
        }
        let parent = components.dropLast().joined(separator: "/")
        if name == ".wh..wh..opq" {
            let directory = try destination(
                for: parent.isEmpty ? "." : parent,
                under: root
            )
            if fileManager.fileExists(atPath: directory.path) {
                let contents = try fileManager.contentsOfDirectory(
                    at: directory,
                    includingPropertiesForKeys: nil
                )
                for item in contents {
                    try fileManager.removeItem(at: item)
                }
            } else {
                try fileManager.createDirectory(
                    at: directory,
                    withIntermediateDirectories: true
                )
            }
            return .opaqueDirectory(parent)
        }

        guard name.hasPrefix(".wh.") else {
            return nil
        }
        let targetName = name.dropFirst(4)
        guard !targetName.isEmpty else {
            throw OrlixOCIImageLayoutError.invalidWhiteout(entry.path)
        }
        let targetPath = parent.isEmpty
            ? String(targetName)
            : parent + "/" + String(targetName)
        let target = try destination(for: targetPath, under: root)
        if itemExistsIncludingSymbolicLink(at: target) {
            try fileManager.removeItem(at: target)
        }
        return .removed(targetPath)
    }

    private func materialize(
        _ record: OrlixRootfsTarArchiveRecord,
        data: Data,
        under root: URL
    ) throws {
        let entry = record.entry
        let target = try destination(for: entry.path, under: root)
        switch entry.type {
        case .directory:
            if fileManager.fileExists(atPath: target.path) {
                return
            }
            try fileManager.createDirectory(
                at: target,
                withIntermediateDirectories: true
            )
        case .regularFile:
            try fileManager.createDirectory(
                at: target.deletingLastPathComponent(),
                withIntermediateDirectories: true
            )
            try? fileManager.removeItem(at: target)
            if entry.sparseExtents.isEmpty {
                try data.subdata(in: record.payloadRange).write(
                    to: target,
                    options: [.atomic]
                )
            } else {
                try writeSparseFile(
                    entry: entry,
                    archiveData: data,
                    payloadRange: record.payloadRange,
                    to: target
                )
            }
        case .symbolicLink:
            guard let linkName = entry.linkName else {
                throw OrlixRootfsTarMaterializerError.missingLinkName(entry.path)
            }
            try fileManager.createDirectory(
                at: target.deletingLastPathComponent(),
                withIntermediateDirectories: true
            )
            try? fileManager.removeItem(at: target)
            try fileManager.createSymbolicLink(
                atPath: target.path,
                withDestinationPath: linkName
            )
        case .hardLink:
            guard let linkName = entry.linkName else {
                throw OrlixRootfsTarMaterializerError.missingLinkName(entry.path)
            }
            let source = try destination(for: linkName, under: root)
            try fileManager.createDirectory(
                at: target.deletingLastPathComponent(),
                withIntermediateDirectories: true
            )
            try? fileManager.removeItem(at: target)
            try fileManager.linkItem(at: source, to: target)
        case .characterDevice, .blockDevice, .fifo:
            try fileManager.createDirectory(
                at: target.deletingLastPathComponent(),
                withIntermediateDirectories: true
            )
        }
    }

    private func itemExistsIncludingSymbolicLink(at url: URL) -> Bool {
        if fileManager.fileExists(atPath: url.path) {
            return true
        }
        return (try? fileManager.destinationOfSymbolicLink(atPath: url.path)) != nil
    }

    private func destination(
        for path: String,
        under root: URL
    ) throws -> URL {
        let destination = root
            .appendingPathComponent(path, isDirectory: false)
            .standardizedFileURL
        let rootPath = root.path.hasSuffix("/") ? root.path : root.path + "/"
        guard destination.path == root.path || destination.path.hasPrefix(rootPath)
        else {
            throw OrlixRootfsTarMaterializerError.destinationEscapesRoot(path)
        }
        return destination
    }
}

private enum OrlixOCIWhiteout {
    case removed(String)
    case opaqueDirectory(String)
}

private func writeSparseFile(
    entry: OrlixRootfsTarManifestEntry,
    archiveData: Data,
    payloadRange: Range<Int>,
    to destination: URL
) throws {
    guard let logicalSize = entry.logicalSize else {
        throw OrlixRootfsTarManifestError.invalidPAXExtendedHeader
    }
    _ = FileManager.default.createFile(atPath: destination.path, contents: nil)
    let handle = try FileHandle(forWritingTo: destination)
    defer {
        try? handle.close()
    }

    var payloadOffset = payloadRange.lowerBound
    for extent in entry.sparseExtents {
        guard extent.length <= UInt64(Int.max) else {
            throw OrlixRootfsTarManifestError.invalidPAXExtendedHeader
        }
        let extentLength = Int(extent.length)
        guard extent.offset <= logicalSize,
              extent.length <= logicalSize - extent.offset,
              extentLength <= payloadRange.upperBound - payloadOffset
        else {
            throw OrlixRootfsTarManifestError.invalidPAXExtendedHeader
        }
        try handle.seek(toOffset: extent.offset)
        try handle.write(
            contentsOf: archiveData.subdata(
                in: payloadOffset..<(payloadOffset + extentLength)
            )
        )
        payloadOffset += extentLength
    }
    guard payloadOffset == payloadRange.upperBound else {
        throw OrlixRootfsTarManifestError.invalidPAXExtendedHeader
    }
    try handle.truncate(atOffset: logicalSize)
}

private struct OrlixOCIPlatform: Codable, Equatable {
    let os: String
    let architecture: String
    let variant: String?

    init(_ value: String) throws {
        let parts = value.split(separator: "/", omittingEmptySubsequences: false)
        guard (parts.count == 2 || parts.count == 3),
              parts.allSatisfy({ !$0.isEmpty })
        else {
            throw OrlixOCIImageLayoutError.missingPlatform(value)
        }
        self.os = String(parts[0])
        self.architecture = String(parts[1])
        self.variant = parts.count == 3 ? String(parts[2]) : nil
    }
}

private struct OCIIndex: Codable {
    let manifests: [OCIDescriptor]
}

private struct OCILayout: Codable {
    let imageLayoutVersion: String
}

private struct OCIManifest: Codable {
    let schemaVersion: Int
    let config: OCIDescriptor
    let layers: [OCIDescriptor]
}

private struct OCIDescriptor: Codable {
    let mediaType: String
    let digest: String
    let size: UInt64
    let platform: OrlixOCIPlatform?
}

private struct OCIImageConfig: Codable {
    let config: OCIProcessConfig?
    let rootfs: OCIRootfs?
}

private struct OCIRootfs: Codable {
    let type: String?
    let diffIDs: [String]?

    enum CodingKeys: String, CodingKey {
        case type
        case diffIDs = "diff_ids"
    }
}

private struct OCIProcessConfig: Codable {
    let env: [String]?
    let entrypoint: [String]?
    let cmd: [String]?
    let workingDir: String?
    let user: String?

    enum CodingKeys: String, CodingKey {
        case env = "Env"
        case entrypoint = "Entrypoint"
        case cmd = "Cmd"
        case workingDir = "WorkingDir"
        case user = "User"
    }
}

private enum OrlixSHA256 {
    private static let initialHash: [UInt32] = [
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    ]

    private static let roundConstants: [UInt32] = [
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
        0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
        0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
        0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
        0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    ]

    static func hash(_ data: Data) -> [UInt8] {
        var bytes = Array(data)
        let bitLength = UInt64(bytes.count) * 8
        bytes.append(0x80)
        while bytes.count % 64 != 56 {
            bytes.append(0)
        }
        for shift in stride(from: 56, through: 0, by: -8) {
            bytes.append(UInt8((bitLength >> UInt64(shift)) & 0xff))
        }

        var hash = initialHash
        var schedule = Array(repeating: UInt32(0), count: 64)
        for chunkOffset in stride(from: 0, to: bytes.count, by: 64) {
            for index in 0..<16 {
                let offset = chunkOffset + index * 4
                schedule[index] =
                    UInt32(bytes[offset]) << 24 |
                    UInt32(bytes[offset + 1]) << 16 |
                    UInt32(bytes[offset + 2]) << 8 |
                    UInt32(bytes[offset + 3])
            }
            for index in 16..<64 {
                let s0 = rotateRight(schedule[index - 15], by: 7) ^
                    rotateRight(schedule[index - 15], by: 18) ^
                    (schedule[index - 15] >> 3)
                let s1 = rotateRight(schedule[index - 2], by: 17) ^
                    rotateRight(schedule[index - 2], by: 19) ^
                    (schedule[index - 2] >> 10)
                schedule[index] = schedule[index - 16]
                    &+ s0
                    &+ schedule[index - 7]
                    &+ s1
            }

            var a = hash[0]
            var b = hash[1]
            var c = hash[2]
            var d = hash[3]
            var e = hash[4]
            var f = hash[5]
            var g = hash[6]
            var h = hash[7]

            for index in 0..<64 {
                let s1 = rotateRight(e, by: 6) ^
                    rotateRight(e, by: 11) ^
                    rotateRight(e, by: 25)
                let ch = (e & f) ^ (~e & g)
                let temp1 = h
                    &+ s1
                    &+ ch
                    &+ roundConstants[index]
                    &+ schedule[index]
                let s0 = rotateRight(a, by: 2) ^
                    rotateRight(a, by: 13) ^
                    rotateRight(a, by: 22)
                let maj = (a & b) ^ (a & c) ^ (b & c)
                let temp2 = s0 &+ maj

                h = g
                g = f
                f = e
                e = d &+ temp1
                d = c
                c = b
                b = a
                a = temp1 &+ temp2
            }

            hash[0] = hash[0] &+ a
            hash[1] = hash[1] &+ b
            hash[2] = hash[2] &+ c
            hash[3] = hash[3] &+ d
            hash[4] = hash[4] &+ e
            hash[5] = hash[5] &+ f
            hash[6] = hash[6] &+ g
            hash[7] = hash[7] &+ h
        }

        return hash.flatMap { word in
            [
                UInt8((word >> 24) & 0xff),
                UInt8((word >> 16) & 0xff),
                UInt8((word >> 8) & 0xff),
                UInt8(word & 0xff)
            ]
        }
    }

    private static func rotateRight(_ value: UInt32, by bits: UInt32) -> UInt32 {
        (value >> bits) | (value << (32 - bits))
    }
}

public enum OrlixOCIRuntimeFeatureStatus: String, Codable, Equatable, Sendable {
	case implemented
	case recognized
	case deterministicallyRejected
}

public struct OrlixOCIRuntimeFeature: Codable, Equatable, Sendable {
	public let name: String
	public let status: OrlixOCIRuntimeFeatureStatus
	public let proof: String?
	public let reason: String

	public init(name: String,
		    status: OrlixOCIRuntimeFeatureStatus,
		    proof: String? = nil,
		    reason: String)
	{
		self.name = name
		self.status = status
		self.proof = proof
		self.reason = reason
	}
}

public struct OrlixOCIRuntimeFeatureReport: Codable, Equatable, Sendable {
	public let schemaVersion: UInt
	public let platform: String
	public let features: [OrlixOCIRuntimeFeature]

	public init(schemaVersion: UInt = 1,
		    platform: String = "linux/arm64",
		    features: [OrlixOCIRuntimeFeature])
	{
		self.schemaVersion = schemaVersion
		self.platform = platform
		self.features = features.sorted { $0.name < $1.name }
	}

	public static let current = OrlixOCIRuntimeFeatureReport(features: [
		OrlixOCIRuntimeFeature(
			name: "apparmor",
			status: .deterministicallyRejected,
			proof: "orlix:runtime_config_parser",
			reason: "No AppArmor policy loading or enforcement proof exists for Orlix OCI-derived environments."
		),
		OrlixOCIRuntimeFeature(
			name: "cgroupV2BasicLifecycle",
			status: .implemented,
			proof: "orlix:cgroup_v2_probe",
			reason: "Basic cgroup v2 mount, task move, child creation, and child removal are covered by Orlix kselftest."
		),
		OrlixOCIRuntimeFeature(
			name: "cgroupV2PidsController",
			status: .implemented,
			proof: "orlix:cgroup_pids_probe",
			reason: "The cgroup v2 pids controller is exposed, enabled for child cgroups, accepts max limits, and accepts task migration in Orlix kselftest."
		),
		OrlixOCIRuntimeFeature(
			name: "cgroups",
			status: .recognized,
			reason: "Broad OCI cgroup resource policy is recognized but not claimed beyond the basic cgroup v2 lifecycle proof."
		),
		OrlixOCIRuntimeFeature(
			name: "devtmpfs",
			status: .implemented,
			proof: "orlix:pseudo_fs_probe",
			reason: "Linux /dev devtmpfs visibility and core character devices are covered by Orlix kselftest."
		),
		OrlixOCIRuntimeFeature(
			name: "fdAliases",
			status: .implemented,
			proof: "orlix:fd_alias_probe",
			reason: "Linux /dev/fd, /dev/stdin, /dev/stdout, and /dev/stderr alias behavior is covered by Orlix kselftest."
		),
		OrlixOCIRuntimeFeature(
			name: "process.user",
			status: .implemented,
			proof: "orlix:runtime_config_parser",
			reason: "OCI process uid, gid, supplementary groups, umask, and supported rlimits carry into OrlixOS descriptors and init command-line defaults."
		),
		OrlixOCIRuntimeFeature(
			name: "process.capabilities",
			status: .implemented,
			proof: "orlix:process_capability_probe",
			reason: "OCI process capability sets carry into OrlixOS descriptors and init applies Linux capability UAPI through capset and prctl."
		),
		OrlixOCIRuntimeFeature(
			name: "process.noNewPrivileges",
			status: .implemented,
			proof: "orlix:runtime_config_parser",
			reason: "OCI process noNewPrivileges carries into OrlixOS descriptors and init applies PR_SET_NO_NEW_PRIVS."
		),
		OrlixOCIRuntimeFeature(
			name: "process.closeAdditionalFds",
			status: .implemented,
			proof: "orlix:runtime_config_parser",
			reason: "OCI process closeAdditionalFds carries into OrlixOS descriptors and init closes inherited descriptors above stderr."
		),
		OrlixOCIRuntimeFeature(
			name: "process.oomScoreAdj",
			status: .implemented,
			proof: "orlix:runtime_config_parser",
			reason: "OCI process oomScoreAdj values in Linux's -1000...1000 range carry into OrlixOS descriptors and init writes /proc/self/oom_score_adj."
		),
		OrlixOCIRuntimeFeature(
			name: "process.scheduler",
			status: .implemented,
			proof: "orlix:runtime_config_parser",
			reason: "OCI process scheduler policy plus priority carries into OrlixOS descriptors for supported Linux SCHED_* policies and init calls sched_setscheduler."
		),
		OrlixOCIRuntimeFeature(
			name: "process.ioPriority",
			status: .implemented,
			proof: "orlix:runtime_config_parser",
			reason: "OCI process ioPriority class plus priority carries into OrlixOS descriptors for supported Linux IOPRIO classes and init calls ioprio_set."
		),
		OrlixOCIRuntimeFeature(
			name: "process.execCPUAffinity",
			status: .implemented,
			proof: "orlix:runtime_config_parser",
			reason: "OCI process execCPUAffinity CPU lists carry into OrlixOS descriptors and init calls sched_setaffinity."
		),
		OrlixOCIRuntimeFeature(
			name: "process.terminal",
			status: .recognized,
			proof: "orlix:runtime_session_descriptor_unit_tests",
			reason: "OCI process terminal requests carry into Orlix runtime session descriptors, but terminal allocation semantics are not claimed until Linux PTY lifecycle proof covers OCI starts."
		),
		OrlixOCIRuntimeFeature(
			name: "process.consoleSize",
			status: .recognized,
			proof: "orlix:runtime_session_descriptor_unit_tests",
			reason: "OCI process consoleSize is validated and carried with terminal session metadata, but runtime terminal resizing is not claimed until Linux PTY lifecycle proof covers OCI starts."
		),
		OrlixOCIRuntimeFeature(
			name: "loopbackNetworking",
			status: .implemented,
			proof: "orlix:network_namespace_probe",
			reason: "Loopback TCP and UDP behavior is covered by Orlix kselftest."
		),
		OrlixOCIRuntimeFeature(
			name: "netDevices",
			status: .deterministicallyRejected,
			proof: "orlix:runtime_config_parser",
			reason: "OCI netDevices policy remains rejected until Orlix has OCI device selection, external networking, DNS, and NAT proof beyond the internal virtio-net device-plane probe."
		),
		OrlixOCIRuntimeFeature(
			name: "ociLifecycleStateModel",
			status: .implemented,
			proof: "orlix:runtime_lifecycle_unit_tests",
			reason: "OrlixOS models create, start, signal, wait/exit, state, and delete transitions with invalid-transition guards before observation-driver side effects."
		),
		OrlixOCIRuntimeFeature(
			name: "ociRuntimeSpecLifecycle",
			status: .recognized,
			reason: "Full OCI Runtime Spec lifecycle is recognized but not claimed until Linux substrate, process execution, resource setup, and cleanup proofs are complete."
		),
		OrlixOCIRuntimeFeature(
			name: "procfs",
			status: .implemented,
			proof: "orlix:pseudo_fs_probe",
			reason: "Linux /proc mount and /proc/self files are covered by Orlix kselftest."
		),
		OrlixOCIRuntimeFeature(
			name: "root.readonly",
			status: .implemented,
			proof: "orlix:readonly_root_probe",
			reason: "OCI root.readonly carries into OrlixOS descriptors and Linux rootinit mounts the environment root read-only, with mountinfo and EROFS write behavior covered by Orlix kselftest."
		),
		OrlixOCIRuntimeFeature(
			name: "rootfsPropagation",
			status: .implemented,
			proof: "orlix:rootinit_mount_propagation",
			reason: "OCI linux.rootfsPropagation carries into OrlixOS descriptors and Linux rootinit applies recursive mount propagation flags before switch_root."
		),
        OrlixOCIRuntimeFeature(
            name: "maskedPaths",
            status: .implemented,
            proof: "orlix:rootinit_masked_paths",
            reason: "OCI linux.maskedPaths carries into OrlixOS descriptors Linux rootinit masks validated in-root paths with standard Linux mounts before userspace init."
        ),
        OrlixOCIRuntimeFeature(
            name: "readonlyPaths",
            status: .implemented,
            proof: "orlix:rootinit_readonly_paths",
            reason: "OCI linux.readonlyPaths carries into OrlixOS descriptors Linux rootinit bind-remounts validated in-root paths read-only before userspace init."
        ),
        OrlixOCIRuntimeFeature(
            name: "sysctl",
            status: .implemented,
            proof: "orlix:rootinit_procfs_sysctl",
            reason: "OCI linux.sysctl carries into OrlixOS descriptors Linux rootinit writes validated sysctl key/value pairs through /proc/sys before userspace init."
        ),
        OrlixOCIRuntimeFeature(
            name: "virtioNetDevicePlane",
			status: .implemented,
			proof: "orlix:virtio_net_device_probe",
			reason:
			"Linux sees the virtio-net device as a netdev with sysfs, rtnetlink, carrier, AF_PACKET bind, TX counter, RX queue, and procfs interface proof."
		),
		OrlixOCIRuntimeFeature(
			name: "virtioFsHostFolderMount",
			status: .implemented,
			proof: "orlix:virtio_fs_mount_probe",
			reason: "Linux mounts the Orlix host-folder tag orlix-host0 through upstream virtio-fs with mountinfo, readdir, EROFS, statx, xattr, lseek, and nested traversal proof."
		),
		OrlixOCIRuntimeFeature(
			name: "rtnetlink",
			status: .implemented,
			proof: "orlix:network_namespace_probe",
			reason: "Opening rtnetlink sockets is covered by Orlix kselftest."
		),
		OrlixOCIRuntimeFeature(
			name: "userNamespaceMappings",
			status: .deterministicallyRejected,
			proof: "orlix:runtime_config_parser",
			reason: "OCI uidMappings and gidMappings are rejected until Orlix reports Linux-owned user namespace mapping support."
		),
		OrlixOCIRuntimeFeature(
			name: "idmappedMounts",
			status: .deterministicallyRejected,
			proof: "orlix:runtime_config_parser",
			reason: "OCI idmapped mounts are rejected until Orlix reports Linux-owned idmapped mount support."
		),
		OrlixOCIRuntimeFeature(
			name: "seccomp",
			status: .deterministicallyRejected,
			proof: "orlix:runtime_config_parser",
			reason: "No seccomp filter loading or enforcement proof exists for Orlix OCI-derived environments."
		),
		OrlixOCIRuntimeFeature(
			name: "hooks.prestart",
			status: .deterministicallyRejected,
			proof: "orlix:runtime_config_parser",
			reason: "OCI prestart hooks are rejected until Orlix can execute lifecycle hooks through Linux-owned runtime semantics."
		),
		OrlixOCIRuntimeFeature(
			name: "hooks.createRuntime",
			status: .deterministicallyRejected,
			proof: "orlix:runtime_config_parser",
			reason: "OCI createRuntime hooks are rejected until Orlix can execute lifecycle hooks through Linux-owned runtime semantics."
		),
		OrlixOCIRuntimeFeature(
			name: "hooks.createContainer",
			status: .deterministicallyRejected,
			proof: "orlix:runtime_config_parser",
			reason: "OCI createContainer hooks are rejected until Orlix can execute lifecycle hooks through Linux-owned runtime semantics."
		),
		OrlixOCIRuntimeFeature(
			name: "hooks.startContainer",
			status: .deterministicallyRejected,
			proof: "orlix:runtime_config_parser",
			reason: "OCI startContainer hooks are rejected until Orlix can execute lifecycle hooks through Linux-owned runtime semantics."
		),
		OrlixOCIRuntimeFeature(
			name: "hooks.poststart",
			status: .deterministicallyRejected,
			proof: "orlix:runtime_config_parser",
			reason: "OCI poststart hooks are rejected until Orlix can execute lifecycle hooks through Linux-owned runtime semantics."
		),
		OrlixOCIRuntimeFeature(
			name: "hooks.poststop",
			status: .deterministicallyRejected,
			proof: "orlix:runtime_config_parser",
			reason: "OCI poststop hooks are rejected until Orlix can execute lifecycle hooks through Linux-owned runtime semantics."
		),
		OrlixOCIRuntimeFeature(
			name: "intelRdt",
			status: .deterministicallyRejected,
			proof: "orlix:runtime_config_parser",
			reason: "OCI Intel RDT policy is parsed and rejected because Orlix does not expose Intel RDT controls."
		),
        OrlixOCIRuntimeFeature(
            name: "ociCgroupPath",
            status: .implemented,
            proof: "orlix:init_cgroup_path_join",
            reason: "OCI absolute cgroupsPath carries through unchanged; relative cgroupsPath normalizes under /orlix before init creates the requested cgroup v2 path and writes the child PID to cgroup.procs."
        ),
		OrlixOCIRuntimeFeature(
			name: "ociHugepageLimits",
			status: .deterministicallyRejected,
			proof: "orlix:runtime_config_parser",
			reason: "OCI hugepage limits are parsed and rejected because hugepage resource control is not supported."
		),
		OrlixOCIRuntimeFeature(
			name: "ociLinuxDevices",
			status: .implemented,
			proof: "orlix:device_node_probe",
			reason: "OCI Linux device declarations carry into OrlixOS descriptors and init creates character, block, and fifo device nodes through Linux mknod/mkfifo before exec."
		),
		OrlixOCIRuntimeFeature(
			name: "ociLinuxResources",
			status: .recognized,
			proof: "orlix:runtime_config_parser",
			reason: "OCI Linux resources object is parsed. Pids, CPU quota, CPU shares, memory limits, block IO weights/throttles, and allowlisted unified cgroup v2 writes are implemented; unproven device, network, RDMA, and hugepage resources remain rejected."
		),
		OrlixOCIRuntimeFeature(
			name: "ociCPUQuota",
			status: .implemented,
			proof: "orlix:cgroup_cpu_probe",
			reason: "OCI linux.resources.cpu quota and period carry into OrlixOS descriptors and init writes cgroup v2 cpu.max before joining the process cgroup."
		),
		OrlixOCIRuntimeFeature(
			name: "ociCPUShares",
			status: .implemented,
			proof: "orlix:cgroup_cpu_probe",
			reason: "OCI linux.resources.cpu.shares maps to cgroup v2 cpu.weight and init writes it before joining the process cgroup."
		),
		OrlixOCIRuntimeFeature(
			name: "ociPidsLimit",
            status: .implemented,
            proof: "orlix:cgroup_pids_probe",
            reason: "OCI linux.resources.pids.limit carries into OrlixOS descriptors and init writes pids.max in the configured cgroup v2 path before joining the process."
),
		OrlixOCIRuntimeFeature(
			name: "ociMemoryLimit",
			status: .implemented,
			proof: "orlix:cgroup_memory_probe",
			reason: "OCI linux.resources.memory.limit carries into OrlixOS descriptors init writes cgroup v2 memory.max before joining process cgroup."
		),
		OrlixOCIRuntimeFeature(
			name: "ociBlockIOControls",
			status: .implemented,
			proof: "orlix:cgroup_io_probe",
			reason: "OCI linux.resources.blockIO default weight, device weights, and device throttles carry into OrlixOS descriptors and init writes cgroup v2 io.weight/io.max before joining process cgroup."
		),
		OrlixOCIRuntimeFeature(
			name: "ociMaskedPaths",
			status: .implemented,
			proof: "orlix:rootinit_masked_paths",
			reason: "OCI linux.maskedPaths carry into OrlixOS descriptors and Linux rootinit masks validated in-root paths with standard Linux mounts before userspace init."
		),
		OrlixOCIRuntimeFeature(
			name: "ociNamespaces",
			status: .recognized,
			proof: "orlix:runtime_config_parser",
			reason: "OCI Linux namespaces are parsed. Mount, IPC, UTS, network, and cgroup namespace creation and path joins are implemented; PID, user, time, and duplicate namespace declarations remain rejected."
		),
		OrlixOCIRuntimeFeature(
			name: "ociMountIpcUtsNetworkCgroupNamespaces",
			status: .implemented,
			proof: "orlix:mount_namespace_probe,orlix:ipc_namespace_probe,orlix:network_namespace_probe,orlix:cgroup_namespace_probe",
			reason: "OCI mount, IPC, UTS, network, and cgroup namespace requests carry into OrlixOS descriptors and init creates them with Linux unshare before exec."
		),
		OrlixOCIRuntimeFeature(
			name: "ociNamespacePathJoins",
			status: .implemented,
			proof: "orlix:runtime_config_parser",
			reason: "OCI mount, IPC, UTS, network, and cgroup namespace path requests carry into OrlixOS descriptors and init joins them with Linux setns before exec."
		),
		OrlixOCIRuntimeFeature(
			name: "ociReadonlyPaths",
			status: .implemented,
			proof: "orlix:rootinit_readonly_paths",
			reason: "OCI linux.readonlyPaths carry into OrlixOS descriptors and Linux rootinit bind-remounts validated in-root paths read-only before userspace init."
		),
		OrlixOCIRuntimeFeature(
			name: "ociUnifiedCgroupResources",
			status: .implemented,
			proof: "orlix:cgroup_unified_probe",
			reason: "OCI unified cgroup resources carry allowlisted cgroup v2 files into OrlixOS descriptors and init writes them through the Linux cgroup filesystem before joining the process cgroup."
		),
		OrlixOCIRuntimeFeature(
			name: "selinux",
			status: .deterministicallyRejected,
			proof: "orlix:runtime_config_parser",
			reason: "No SELinux policy loading or enforcement proof exists for Orlix OCI-derived environments."
		),
		OrlixOCIRuntimeFeature(
			name: "tmpfs",
			status: .implemented,
			proof: "orlix:pseudo_fs_probe",
			reason: "Linux /tmp tmpfs visibility is covered by Orlix kselftest."
		),
		OrlixOCIRuntimeFeature(
			name: "ociBindMounts",
			status: .implemented,
			proof: "orlix:virtio_fs_mount_probe",
			reason: "OCI bind mount source paths translate to opaque Orlix host-directory registrations and Linux-visible virtiofs mounts; Documents and security-scoped external folders remain app-managed source conveniences."
		),
		OrlixOCIRuntimeFeature(
			name: "ociCgroupMounts",
			status: .implemented,
			proof: "orlix:cgroup_v2_probe",
			reason: "OCI cgroup2 mounts are accepted for the standard /sys/fs/cgroup hierarchy that Orlix init mounts with Linux cgroup2."
		),
	])

	public func feature(named name: String) -> OrlixOCIRuntimeFeature? {
		features.first { $0.name == name }
	}

	public func jsonData() throws -> Data {
		let encoder = JSONEncoder()
		encoder.outputFormatting = [.prettyPrinted, .sortedKeys, .withoutEscapingSlashes]
		return try encoder.encode(self)
	}
}

public enum OrlixOCIRuntimeConfigError: Error, Equatable, Sendable {
	case unsupportedOCIVersion(String)
	case missingProcess
	case emptyProcessArgs
	case invalidProcessArg(String)
	case invalidEnvironmentEntry(String)
	case invalidAnnotationEntry(String)
	case missingRootPath
	case invalidRootPath(String)
	case missingWorkingDirectory
	case invalidWorkingDirectory(String)
	case invalidConsoleSize
	case invalidHostname(String)
	case invalidDomainname(String)
	case unsupportedLinuxFeature(String)
}

public struct OrlixOCIRuntimeConfigDescriptor: Equatable, Sendable {
	public let ociVersion: String
	public let annotations: [String: String]
	public let hostname: String?
	public let domainname: String?
    public let rootPath: String?
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
public let mounts: [OrlixOCIRuntimeMount]
	public let defaultCommand: [String]
	public let defaultEnvironment: [String: String]
	public let defaultWorkingDirectory: String
	public let defaultUserID: UInt32
	public let defaultGroupID: UInt32
	public let defaultSupplementaryGroups: [UInt32]
	public let defaultCapabilities: OrlixEnvironmentCapabilities?
	public let defaultNoNewPrivileges: Bool
	public let defaultCloseAdditionalFds: Bool
	public let defaultOOMScoreAdjustment: Int32?
	public let defaultScheduler: OrlixEnvironmentScheduler?
	public let defaultIOPriority: OrlixEnvironmentIOPriority?
	public let defaultCPUAffinity: OrlixEnvironmentCPUAffinity?
	public let defaultUmask: UInt32?
	public let defaultRlimits: [OrlixEnvironmentRlimit]
    public let terminal: Bool
    public let consoleSize: OrlixOCIRuntimeConsoleSize?
    public let namespaces: [String]
    public let namespacePaths: [String: String]

	@_spi(OrlixPrivateTesting)
	public func environmentDescriptor(id: String,
					 rootMount: OrlixEnvironmentRootMount,
					 mounts: [OrlixEnvironmentMount] = [])
		throws -> OrlixEnvironmentDescriptor
	{
		let ociMounts = try self.mounts.compactMap { try $0.environmentMount() }

		return OrlixEnvironmentDescriptor(
			id: id,
			source: .ociLayout,
			platform: "linux/arm64",
			rootImageIdentifier: id,
			defaultCommand: defaultCommand,
			defaultEnvironment: defaultEnvironment,
			defaultWorkingDirectory: defaultWorkingDirectory,
			defaultUserID: defaultUserID,
			defaultGroupID: defaultGroupID,
			defaultSupplementaryGroups: defaultSupplementaryGroups,
			defaultCapabilities: defaultCapabilities,
			defaultNoNewPrivileges: defaultNoNewPrivileges,
			defaultCloseAdditionalFds: defaultCloseAdditionalFds,
			defaultOOMScoreAdjustment: defaultOOMScoreAdjustment,
			defaultScheduler: defaultScheduler,
			defaultIOPriority: defaultIOPriority,
			defaultCPUAffinity: defaultCPUAffinity,
			defaultUmask: defaultUmask,
			defaultRlimits: defaultRlimits,
			hostname: hostname,
			domainname: domainname,
            rootMount: rootMount,
            rootReadonly: rootReadonly,
            rootPropagation: rootPropagation,
            sysctls: sysctls,
            maskedPaths: maskedPaths,
            readonlyPaths: readonlyPaths,
            cgroupsPath: cgroupsPath,
            cgroupPidsLimit: cgroupPidsLimit,
            cgroupCPUMax: cgroupCPUMax,
			cgroupCPUWeight: cgroupCPUWeight,
			cgroupMemoryMax: cgroupMemoryMax,
			cgroupIOWeight: cgroupIOWeight,
			cgroupUnified: cgroupUnified,
			deviceNodes: deviceNodes,
			namespaces: namespaces,
            namespacePaths: namespacePaths,
            mounts: mounts + ociMounts
        )
    }
}

public struct OrlixOCIRuntimeMount: Equatable, Sendable {
	public let destination: String
	public let type: String
	public let source: String?
	public let options: [String]

	private static let documentsSource = "orlix:documents"
	private static let externalSourcePrefix = "orlix:external:"
	private static let supportedBindOptions = Set(["bind", "rbind", "ro", "rw"])

	func environmentMount() throws -> OrlixEnvironmentMount? {
		guard type == "bind" else {
			return nil
		}
		guard let source, !source.isEmpty, !source.contains("\u{0}") else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("mounts.source")
		}
		let optionSet = Set(options)
		guard optionSet.isSubset(of: Self.supportedBindOptions),
		      !(optionSet.contains("ro") && optionSet.contains("rw")) else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("mounts.options")
		}
		let readOnly = optionSet.contains("ro")
		do {
			if source == Self.documentsSource {
				return try .documents(targetPath: destination, readOnly: readOnly)
			}
			if source.hasPrefix(Self.externalSourcePrefix) {
				let bookmarkID = String(source.dropFirst(Self.externalSourcePrefix.count))
				return try .securityScopedExternal(
					bookmarkID: bookmarkID,
					targetPath: destination,
					readOnly: readOnly
				)
			}
			if source.hasPrefix("/") {
				return try .hostPath(
					source,
					targetPath: destination,
					readOnly: readOnly
				)
			}
		} catch OrlixEnvironmentMountError.invalidTargetPath(_),
		        OrlixEnvironmentMountError.reservedTargetPath(_) {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("mounts.destination")
		} catch OrlixEnvironmentMountError.invalidSourceIdentifier(_) {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("mounts.source")
		}
		throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("mounts.source")
	}
}

public struct OrlixOCIRuntimeConsoleSize: Equatable, Sendable {
	public let height: UInt32
	public let width: UInt32
}

public struct OrlixOCIRuntimeConfigParser: Sendable {
	public init() {}

	public func parse(_ data: Data) throws -> OrlixOCIRuntimeConfigDescriptor {
		let config = try JSONDecoder().decode(OCIRuntimeConfig.self, from: data)
		try Self.validateOCIVersion(config.ociVersion)
		let hostname = try Self.validatedUTSName(config.hostname,
							 feature: "hostname")
		let domainname = try Self.validatedUTSName(config.domainname,
							   feature: "domainname")
		try Self.rejectUnsupportedNonLinuxPlatformConfig(config)

		guard let process = config.process else {
			throw OrlixOCIRuntimeConfigError.missingProcess
		}
		guard !process.args.isEmpty else {
			throw OrlixOCIRuntimeConfigError.emptyProcessArgs
		}

		let args = try process.args.map { arg in
			if arg.contains("\u{0}") {
				throw OrlixOCIRuntimeConfigError.invalidProcessArg(arg)
			}
			return arg
		}
		let environment = try Self.environmentDictionary(from: process.env ?? [])
		guard let cwd = process.cwd else {
			throw OrlixOCIRuntimeConfigError.missingWorkingDirectory
		}
		guard cwd.hasPrefix("/"), !cwd.contains("\u{0}") else {
			throw OrlixOCIRuntimeConfigError.invalidWorkingDirectory(cwd)
		}

		let namespaces = try Self.validatedNamespaces(config.linux?.namespaces ?? [])
		let mounts = try Self.validatedMounts(config.mounts ?? [])
		try Self.rejectUnsupportedProcessFeatures(process)
		try Self.rejectUnsupportedLinuxFeatures(config.linux)
		try Self.rejectUnsupportedHooks(config.hooks)
		let terminal = process.terminal ?? false
		let consoleSize = try Self.validatedConsoleSize(process.consoleSize,
							       terminal: terminal)

		return OrlixOCIRuntimeConfigDescriptor(
			ociVersion: config.ociVersion,
			annotations: try Self.validatedAnnotations(config.annotations ?? [:]),
			hostname: hostname,
			domainname: domainname,
			rootPath: try Self.validatedRootPath(config.root?.path),
			rootReadonly: config.root?.readonly ?? false,
            rootPropagation: try Self.validatedRootPropagation(
                config.linux?.rootfsPropagation
            ),
            sysctls: try Self.validatedSysctls(config.linux?.sysctl ?? [:]),
            maskedPaths: try Self.validatedRuntimePaths(
                config.linux?.maskedPaths ?? [],
                feature: "linux.maskedPaths"
            ),
            readonlyPaths: try Self.validatedRuntimePaths(
                config.linux?.readonlyPaths ?? [],
                feature: "linux.readonlyPaths"
            ),
cgroupsPath: try Self.validatedCgroupsPath(config.linux?.cgroupsPath),
            cgroupPidsLimit: try Self.validatedCgroupPidsLimit(
                config.linux?.resources,
                cgroupsPath: config.linux?.cgroupsPath
            ),
cgroupCPUMax: try Self.validatedCgroupCPUMax(
config.linux?.resources,
cgroupsPath: config.linux?.cgroupsPath
),
cgroupCPUWeight: try Self.validatedCgroupCPUWeight(
config.linux?.resources,
cgroupsPath: config.linux?.cgroupsPath
),
			cgroupMemoryMax: try Self.validatedCgroupMemoryMax(
				config.linux?.resources,
				cgroupsPath: config.linux?.cgroupsPath
			),
			cgroupIOWeight: try Self.validatedCgroupIOWeight(
				config.linux?.resources,
				cgroupsPath: config.linux?.cgroupsPath
			),
			cgroupUnified: try Self.validatedCgroupUnified(
				config.linux?.resources,
				cgroupsPath: config.linux?.cgroupsPath
			) + Self.validatedCgroupBlockIOUnifiedEntries(
				config.linux?.resources,
				cgroupsPath: config.linux?.cgroupsPath
			),
			deviceNodes: try Self.validatedDeviceNodes(config.linux?.devices ?? []),
			mounts: mounts,
			defaultCommand: args,
			defaultEnvironment: environment,
			defaultWorkingDirectory: cwd,
			defaultUserID: process.user?.uid ?? 0,
			defaultGroupID: process.user?.gid ?? 0,
			defaultSupplementaryGroups: process.user?.additionalGids ?? [],
			defaultCapabilities: try Self.validatedCapabilities(process.capabilities),
			defaultNoNewPrivileges: process.noNewPrivileges ?? false,
			defaultCloseAdditionalFds: process.closeAdditionalFds ?? false,
			defaultOOMScoreAdjustment: try Self.validatedOOMScoreAdjustment(process.oomScoreAdj),
			defaultScheduler: try Self.validatedScheduler(process.scheduler),
			defaultIOPriority: try Self.validatedIOPriority(process.ioPriority),
			defaultCPUAffinity: try Self.validatedCPUAffinity(process.execCPUAffinity),
			defaultUmask: try Self.validatedUmask(process.user?.umask),
			defaultRlimits: try Self.validatedRlimits(process.rlimits),
            terminal: terminal,
            consoleSize: consoleSize,
            namespaces: namespaces,
            namespacePaths: try Self.validatedNamespacePaths(config.linux?.namespaces ?? [])
        )
	}

	private static func validateOCIVersion(_ version: String) throws {
		let supportedVersions = Set(["1.0.0", "1.0.1", "1.1.0"])

		if !supportedVersions.contains(version) {
			throw OrlixOCIRuntimeConfigError.unsupportedOCIVersion(version)
		}
	}

	private static func environmentDictionary(from values: [String]) throws -> [String: String] {
		var environment: [String: String] = [:]

		for value in values {
			guard let separator = value.firstIndex(of: "=") else {
				throw OrlixOCIRuntimeConfigError.invalidEnvironmentEntry(value)
			}
			let key = String(value[..<separator])
			let variableValue = String(value[value.index(after: separator)...])
			guard !key.isEmpty,
			      !key.contains("\u{0}"),
			      !variableValue.contains("\u{0}") else {
				throw OrlixOCIRuntimeConfigError.invalidEnvironmentEntry(value)
			}
			guard environment[key] == nil else {
				throw OrlixOCIRuntimeConfigError.invalidEnvironmentEntry(value)
			}
			environment[key] = variableValue
		}

		return environment
	}

	private static func validatedAnnotations(_ annotations: [String: String]) throws
		-> [String: String]
	{
		for (key, value) in annotations {
			guard !key.isEmpty,
			      !key.contains("\u{0}"),
			      !value.contains("\u{0}") else {
				throw OrlixOCIRuntimeConfigError.invalidAnnotationEntry(key)
			}
		}

		return annotations
	}

	private static func validatedRootPath(_ value: String?) throws -> String {
		guard let value else {
			throw OrlixOCIRuntimeConfigError.missingRootPath
		}
		guard !value.isEmpty, !value.contains("\u{0}") else {
			throw OrlixOCIRuntimeConfigError.invalidRootPath(value)
		}
		return value
	}

    private static func validatedRootPropagation(
        _ value: String?
    ) throws -> OrlixEnvironmentRootPropagation {
		guard let value, !value.isEmpty else {
			return .private
		}
		switch value {
		case "private", "rprivate":
			return .private
		case "shared", "rshared":
			return .shared
		case "slave", "rslave":
			return .slave
		case "unbindable", "runbindable":
			return .unbindable
		default:
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
				"linux.rootfsPropagation"
			)
        }
    }

    private static func validatedSysctls(_ values: [String: String]) throws -> [String: String] {
        let allowedScalars = CharacterSet.alphanumerics
            .union(CharacterSet(charactersIn: "._-"))
        for (key, value) in values {
            guard !key.isEmpty,
                key.unicodeScalars.allSatisfy({ allowedScalars.contains($0) }),
                key.first != ".",
                key.last != ".",
                !key.contains(".."),
                !value.contains("\u{0}"),
                !value.contains("\n"),
                !value.contains("\r")
            else {
                throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.sysctl")
            }
        }
        return values
    }

private static func validatedRuntimePaths(
	_ paths: [String],
	feature: String
) throws -> [String] {
        for path in paths {
            let components = path.split(separator: "/", omittingEmptySubsequences: false)
            guard path.hasPrefix("/"),
                path != "/",
                !path.contains("\u{0}"),
                !components.contains(where: { $0 == ".." }),
                !components.contains(where: { $0 == "." })
            else {
                throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(feature)
            }
        }
        return paths
    }

private static func validatedOptionalRuntimePath(
_ path: String?,
feature: String
) throws -> String? {
guard let path else {
return nil
}
return try validatedRuntimePaths([path], feature: feature).first
}

private static func validatedCgroupsPath(_ path: String?) throws -> String? {
	guard let path else { return nil }
	let components = path.split(separator: "/", omittingEmptySubsequences: false)
	guard !path.isEmpty,
	      !path.contains("\u{0}"),
	      !path.contains("//"),
	      !components.contains(where: { $0 == ".." }),
	      !components.contains(where: { $0 == "." })
	else {
		throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.cgroupsPath")
	}
	if path.hasPrefix("/") {
		guard path != "/" else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.cgroupsPath")
		}
		return path
	}
	return "/orlix/\(path)"
}

private static func validatedDeviceNodes(
	_ devices: [OCIRuntimeDevice]
) throws -> [OrlixEnvironmentDeviceNode] {
	try devices.map { device in
		guard let path = device.path else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.devices.path")
		}
		guard let type = device.type else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.devices.type")
		}
		_ = try validatedRuntimePaths([path], feature: "linux.devices.path")
		guard Set(["c", "b", "u", "p"]).contains(type) else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.devices.type")
		}
		let mode = device.fileMode ?? 0o666
		guard mode <= 0o7777 else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.devices.fileMode")
		}
		if type == "p" {
			return OrlixEnvironmentDeviceNode(
				path: path,
				type: type,
				fileMode: mode,
				uid: device.uid ?? 0,
				gid: device.gid ?? 0
			)
		}
		guard let major = device.major else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.devices.major")
		}
		guard let minor = device.minor else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.devices.minor")
		}
		return OrlixEnvironmentDeviceNode(
			path: path,
			type: type,
			major: major,
			minor: minor,
			fileMode: mode,
			uid: device.uid ?? 0,
			gid: device.gid ?? 0
		)
	}
}

private static func validatedCgroupPidsLimit(
	_ resources: OCIRuntimeResources?,
	cgroupsPath: String?
    ) throws -> Int64? {
        guard let resources else {
            return nil
        }

        if resources.devices != nil {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.devices")
        }
	if resources.network != nil {
		throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.network")
	}
        if let hugepageLimits = resources.hugepageLimits, !hugepageLimits.isEmpty {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.hugepageLimits")
        }
        if let rdma = resources.rdma, !rdma.isEmpty {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.rdma")
        }
guard let limit = resources.pids?.limit else {
            return nil
        }
        guard cgroupsPath != nil else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.pids.cgroupsPath")
        }
        guard limit >= -1 else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.pids.limit")
		}
return limit
}

private static func validatedCgroupMemoryMax(
_ resources: OCIRuntimeResources?,
cgroupsPath: String?
) throws -> Int64? {
guard let memory = resources?.memory else {
return nil
}
if memory.reservation != nil {
throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.memory.reservation")
}
if memory.swap != nil {
throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.memory.swap")
}
if memory.kernel != nil {
throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.memory.kernel")
}
if memory.kernelTCP != nil {
throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.memory.kernelTCP")
}
if memory.swappiness != nil {
throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.memory.swappiness")
}
if memory.disableOOMKiller != nil {
throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.memory.disableOOMKiller")
}
if memory.useHierarchy != nil {
throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.memory.useHierarchy")
}
guard let limit = memory.limit else {
return nil
}
guard cgroupsPath != nil else {
throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.memory.cgroupsPath")
}
guard limit >= -1 else {
throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.memory.limit")
}
return limit
}

private static func validatedCgroupCPUMax(
_ resources: OCIRuntimeResources?,
cgroupsPath: String?
	) throws -> OrlixEnvironmentCgroupCPUMax? {
		guard let cpu = resources?.cpu else {
			return nil
		}
    if cpu.realtimeRuntime != nil {
        throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.cpu.realtimeRuntime")
    }
		if cpu.realtimePeriod != nil {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.cpu.realtimePeriod")
		}
		if cpu.cpus != nil {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.cpu.cpus")
		}
		if cpu.mems != nil {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.cpu.mems")
		}
		guard cpu.quota != nil || cpu.period != nil else {
			return nil
		}
		guard cgroupsPath != nil else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.cpu.cgroupsPath")
		}
		let quota = cpu.quota ?? -1
		let period = cpu.period ?? OrlixEnvironmentCgroupCPUMax.defaultPeriodMicros
		guard quota == -1 || quota > 0 else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.cpu.quota")
		}
		guard period > 0 else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.cpu.period")
		}
    return OrlixEnvironmentCgroupCPUMax(quotaMicros: quota, periodMicros: period)
}

private static func validatedCgroupCPUWeight(
	_ resources: OCIRuntimeResources?,
	cgroupsPath: String?
) throws -> UInt64? {
guard let shares = resources?.cpu?.shares else {
return nil
}
guard cgroupsPath != nil else {
throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.cpu.shares.cgroupsPath")
}
guard shares >= 2 && shares <= 262_144 else {
throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.cpu.shares")
	}
	return 1 + ((shares - 2) * 9_999) / 262_142
}

private static func validatedCgroupIOWeight(
	_ resources: OCIRuntimeResources?,
	cgroupsPath: String?
) throws -> UInt64? {
	guard let blockIO = resources?.blockIO else {
		return nil
	}
	if blockIO.leafWeight != nil {
		throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.blockIO.leafWeight")
	}
	guard let weight = blockIO.weight else {
		return nil
	}
	guard cgroupsPath != nil else {
		throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.blockIO.weight.cgroupsPath")
	}
	guard (1...10_000).contains(weight) else {
		throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.blockIO.weight")
	}
	return weight
}

private static func validatedCgroupBlockIOUnifiedEntries(
	_ resources: OCIRuntimeResources?,
	cgroupsPath: String?
) throws -> [OrlixEnvironmentCgroupUnifiedEntry] {
	guard let blockIO = resources?.blockIO else {
		return []
	}
	if blockIO.leafWeight != nil {
		throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.blockIO.leafWeight")
	}
	var entries: [OrlixEnvironmentCgroupUnifiedEntry] = []
	if let weightDevice = blockIO.weightDevice, !weightDevice.isEmpty {
		guard cgroupsPath != nil else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.blockIO.weightDevice.cgroupsPath")
		}
		for device in weightDevice {
			if device.leafWeight != nil {
				throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.blockIO.weightDevice.leafWeight")
			}
			let prefix = try Self.validatedBlockIODevicePrefix(
				major: device.major,
				minor: device.minor,
				feature: "linux.resources.blockIO.weightDevice"
			)
			guard let weight = device.weight,
			      (1...10_000).contains(weight)
			else {
				throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.blockIO.weightDevice.weight")
			}
			entries.append(
				OrlixEnvironmentCgroupUnifiedEntry(
					file: "io.weight",
					value: "\(prefix) \(weight)"
				)
			)
		}
	}
	try Self.appendBlockIOThrottleEntries(
		blockIO.throttleReadBpsDevice,
		token: "rbps",
		feature: "linux.resources.blockIO.throttleReadBpsDevice",
		cgroupsPath: cgroupsPath,
		entries: &entries
	)
	try Self.appendBlockIOThrottleEntries(
		blockIO.throttleWriteBpsDevice,
		token: "wbps",
		feature: "linux.resources.blockIO.throttleWriteBpsDevice",
		cgroupsPath: cgroupsPath,
		entries: &entries
	)
	try Self.appendBlockIOThrottleEntries(
		blockIO.throttleReadIOPSDevice,
		token: "riops",
		feature: "linux.resources.blockIO.throttleReadIOPSDevice",
		cgroupsPath: cgroupsPath,
		entries: &entries
	)
	try Self.appendBlockIOThrottleEntries(
		blockIO.throttleWriteIOPSDevice,
		token: "wiops",
		feature: "linux.resources.blockIO.throttleWriteIOPSDevice",
		cgroupsPath: cgroupsPath,
		entries: &entries
	)
	return entries
}

private static func appendBlockIOThrottleEntries(
	_ devices: [OCIRuntimeResourceBlockIODeviceThrottle]?,
	token: String,
	feature: String,
	cgroupsPath: String?,
	entries: inout [OrlixEnvironmentCgroupUnifiedEntry]
) throws {
	guard let devices, !devices.isEmpty else {
		return
	}
	guard cgroupsPath != nil else {
		throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("\(feature).cgroupsPath")
	}
	for device in devices {
		let prefix = try Self.validatedBlockIODevicePrefix(
			major: device.major,
			minor: device.minor,
			feature: feature
		)
		guard let rate = device.rate, rate > 1 else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("\(feature).rate")
		}
		entries.append(
			OrlixEnvironmentCgroupUnifiedEntry(
				file: "io.max",
				value: "\(prefix) \(token)=\(rate)"
			)
		)
	}
}

private static func validatedBlockIODevicePrefix(
	major: UInt32?,
	minor: UInt32?,
	feature: String
) throws -> String {
	guard let major, let minor else {
		throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(feature)
	}
	return "\(major):\(minor)"
}

private static func validatedCgroupUnified(
	_ resources: OCIRuntimeResources?,
	cgroupsPath: String?
) throws -> [OrlixEnvironmentCgroupUnifiedEntry] {
	guard let unified = resources?.unified, !unified.isEmpty else {
		return []
	}
	guard cgroupsPath != nil else {
		throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.unified.cgroupsPath")
	}
	let supportedFiles = Set([
		"pids.max",
		"cpu.max",
		"cpu.weight",
		"memory.max",
		"io.weight",
		"io.max"
	])
	return try unified
		.sorted(by: { $0.key < $1.key })
		.map { entry in
			let file = entry.key
			let value = entry.value
			guard supportedFiles.contains(file),
			      !file.contains("\u{0}"),
			      !file.contains("/"),
			      !value.isEmpty,
			      !value.contains("\u{0}"),
			      !value.contains("\n"),
			      !value.contains("\r")
			else {
				throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.resources.unified.\(file)")
			}
			return OrlixEnvironmentCgroupUnifiedEntry(file: file, value: value)
		}
}

private static func validatedUTSName(_ value: String?,
		feature: String) throws -> String?
	{
		guard let value, !value.isEmpty else {
			return nil
		}
		guard !value.contains("\u{0}") else {
			if feature == "hostname" {
				throw OrlixOCIRuntimeConfigError.invalidHostname(value)
			}
			throw OrlixOCIRuntimeConfigError.invalidDomainname(value)
		}
		guard value.utf8.count <= 64 else {
			if feature == "hostname" {
				throw OrlixOCIRuntimeConfigError.invalidHostname(value)
			}
			throw OrlixOCIRuntimeConfigError.invalidDomainname(value)
		}
		return value
	}

	private static func validatedUmask(_ value: UInt32?) throws -> UInt32? {
		guard let value else {
			return nil
		}
		guard value <= 0o777 else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("process.user.umask")
		}
		return value
	}

	private static func validatedOOMScoreAdjustment(_ value: Int?) throws -> Int32? {
		guard let value else {
			return nil
		}
		guard value >= -1000 && value <= 1000 else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("process.oomScoreAdj")
		}
		return Int32(value)
	}

	private static func validatedCapabilities(
		_ capabilities: OCIRuntimeCapabilities?
	) throws -> OrlixEnvironmentCapabilities? {
		guard let capabilities else {
			return nil
		}
		return OrlixEnvironmentCapabilities(
			bounding: try validatedCapabilitySet(capabilities.bounding, field: "bounding"),
			permitted: try validatedCapabilitySet(capabilities.permitted, field: "permitted"),
			inheritable: try validatedCapabilitySet(capabilities.inheritable, field: "inheritable"),
			effective: try validatedCapabilitySet(capabilities.effective, field: "effective"),
			ambient: try validatedCapabilitySet(capabilities.ambient, field: "ambient")
		)
	}

	private static func validatedCapabilitySet(
		_ names: [String]?,
		field: String
	) throws -> [String] {
		guard let names else {
			return []
		}
		var seen = Set<String>()
		var result: [String] = []
		for name in names {
			guard supportedLinuxCapabilityNames.contains(name) else {
				throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("process.capabilities.\(field)")
			}
			if seen.insert(name).inserted {
				result.append(name)
			}
		}
		return result
	}

	private static let supportedLinuxCapabilityNames: Set<String> = [
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

	private static func validatedScheduler(
		_ scheduler: OCIRuntimeScheduler?
	) throws -> OrlixEnvironmentScheduler? {
		guard let scheduler else {
			return nil
		}
		if scheduler.nice != nil {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("process.scheduler.nice")
		}
		if let flags = scheduler.flags, !flags.isEmpty {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("process.scheduler.flags")
		}
		guard let policy = scheduler.policy else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("process.scheduler.policy")
		}
		let supportedPolicies: Set<String> = [
			"SCHED_OTHER",
			"SCHED_BATCH",
			"SCHED_IDLE",
			"SCHED_FIFO",
			"SCHED_RR"
		]
		guard supportedPolicies.contains(policy) else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("process.scheduler.policy")
		}
		let priority = scheduler.priority ?? 0
		guard priority >= 0 && priority <= Int(Int32.max) else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("process.scheduler.priority")
		}
		return OrlixEnvironmentScheduler(policy: policy, priority: Int32(priority))
	}

	private static func validatedIOPriority(
		_ ioPriority: OCIRuntimeIOPriority?
	) throws -> OrlixEnvironmentIOPriority? {
		guard let ioPriority else {
			return nil
		}
		guard let priorityClass = ioPriority.class else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("process.ioPriority.class")
		}
		let supportedClasses: Set<String> = [
			"IOPRIO_CLASS_RT",
			"IOPRIO_CLASS_BE",
			"IOPRIO_CLASS_IDLE"
		]
		guard supportedClasses.contains(priorityClass) else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("process.ioPriority.class")
		}
		let priority = ioPriority.priority ?? 0
		guard priority >= 0 && priority <= 7 else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("process.ioPriority.priority")
		}
		return OrlixEnvironmentIOPriority(class: priorityClass, priority: Int32(priority))
	}

	private static func validatedCPUAffinity(
		_ affinity: OCIRuntimeCPUAffinity?
	) throws -> OrlixEnvironmentCPUAffinity? {
		guard let affinity else {
			return nil
		}
		guard let mask = affinity.final ?? affinity.initial, !mask.isEmpty else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("process.execCPUAffinity")
		}
		let parts = mask.split(separator: ",", omittingEmptySubsequences: false)
		guard !parts.isEmpty else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("process.execCPUAffinity")
		}
		for part in parts {
			let range = part.split(separator: "-", omittingEmptySubsequences: false)
			guard range.count == 1 || range.count == 2 else {
				throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("process.execCPUAffinity")
			}
			guard let start = Int(range[0]), start >= 0 else {
				throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("process.execCPUAffinity")
			}
			if range.count == 2 {
				guard let end = Int(range[1]), end >= start else {
					throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("process.execCPUAffinity")
				}
			}
		}
		return OrlixEnvironmentCPUAffinity(mask: mask)
	}

	private static func validatedRlimits(_ values: [OCIRuntimeRlimit]?) throws -> [OrlixEnvironmentRlimit] {
		guard let values else {
			return []
		}
		var seen = Set<String>()
		var rlimits: [OrlixEnvironmentRlimit] = []
		for value in values {
			guard supportedRlimitTypes.contains(value.type),
			      value.soft <= value.hard,
			      seen.insert(value.type).inserted
			else {
				throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("process.rlimits")
			}
			rlimits.append(OrlixEnvironmentRlimit(
				type: value.type,
				soft: value.soft,
				hard: value.hard
			))
		}
		return rlimits
	}

	private static let supportedRlimitTypes: Set<String> = [
		"RLIMIT_AS",
		"RLIMIT_CORE",
		"RLIMIT_CPU",
		"RLIMIT_DATA",
		"RLIMIT_FSIZE",
		"RLIMIT_LOCKS",
		"RLIMIT_MEMLOCK",
		"RLIMIT_MSGQUEUE",
		"RLIMIT_NICE",
		"RLIMIT_NOFILE",
		"RLIMIT_NPROC",
		"RLIMIT_RSS",
		"RLIMIT_RTPRIO",
		"RLIMIT_RTTIME",
		"RLIMIT_SIGPENDING",
		"RLIMIT_STACK"
	]

	private static func rejectUnsupportedNonLinuxPlatformConfig(_ config: OCIRuntimeConfig) throws {
		if config.solaris != nil {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("solaris")
		}
		if config.windows != nil {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("windows")
		}
		if config.vm != nil {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("vm")
		}
		if config.zOS != nil {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("zOS")
		}
	}

	private static func rejectUnsupportedHooks(_ hooks: OCIRuntimeHooks?) throws {
		guard let hooks else {
			return
		}
		if let prestart = hooks.prestart, !prestart.isEmpty {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("hooks.prestart")
		}
		if let createRuntime = hooks.createRuntime, !createRuntime.isEmpty {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("hooks.createRuntime")
		}
		if let createContainer = hooks.createContainer, !createContainer.isEmpty {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("hooks.createContainer")
		}
		if let startContainer = hooks.startContainer, !startContainer.isEmpty {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("hooks.startContainer")
		}
		if let poststart = hooks.poststart, !poststart.isEmpty {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("hooks.poststart")
		}
		if let poststop = hooks.poststop, !poststop.isEmpty {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("hooks.poststop")
		}
	}

    private static func validatedNamespaces(_ namespaces: [OCIRuntimeNamespace]) throws -> [String] {
        var seen = Set<String>()
        var result: [String] = []
        let supportedNamespaces = Set(["mount", "ipc", "uts", "network", "cgroup"])

        for namespace in namespaces {
            guard !seen.contains(namespace.type) else {
                throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                    "linux.namespaces.\(namespace.type).duplicate"
                )
            }
            seen.insert(namespace.type)
            guard supportedNamespaces.contains(namespace.type) else {
                throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                    "linux.namespaces.\(namespace.type)"
                )
            }
            if namespace.path == nil {
                result.append(namespace.type)
            }
        }

        return result.sorted()
    }

    private static func validatedNamespacePaths(
        _ namespaces: [OCIRuntimeNamespace]
    ) throws -> [String: String] {
        var result: [String: String] = [:]
        let supportedNamespaces = Set(["mount", "ipc", "uts", "network", "cgroup"])

        for namespace in namespaces {
            guard let path = namespace.path else {
                continue
            }
            guard supportedNamespaces.contains(namespace.type) else {
                throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                    "linux.namespaces.\(namespace.type).path"
                )
            }
            guard try validatedOptionalRuntimePath(
                path,
                feature: "linux.namespaces.\(namespace.type).path"
            ) != nil else {
                throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                    "linux.namespaces.\(namespace.type).path"
                )
            }
            result[namespace.type] = path
        }

        return result
    }

	private static func validatedMounts(_ mounts: [OCIRuntimeMount]) throws -> [OrlixOCIRuntimeMount] {
		try mounts.map { mount in
			guard mount.destination.hasPrefix("/"),
			      !mount.destination.contains("\u{0}") else {
				throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("mounts.destination")
			}

		let supportedMountTypes = Set(["proc", "sysfs", "devtmpfs", "devpts", "tmpfs", "cgroup2", "bind"])
			guard supportedMountTypes.contains(mount.type) else {
				throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("mounts.type.\(mount.type)")
			}

			if let uidMappings = mount.uidMappings, !uidMappings.isEmpty {
				throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("mounts.uidMappings")
			}
			if let gidMappings = mount.gidMappings, !gidMappings.isEmpty {
				throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("mounts.gidMappings")
			}

			let runtimeMount = OrlixOCIRuntimeMount(
				destination: mount.destination,
				type: mount.type,
				source: mount.source,
				options: mount.options ?? []
			)
			try Self.validateDefaultVirtualMount(runtimeMount)
			_ = try runtimeMount.environmentMount()
			return runtimeMount
		}
	}

	private static func validateDefaultVirtualMount(_ mount: OrlixOCIRuntimeMount) throws {
		guard mount.type != "bind" else { return }

		guard mount.options.isEmpty else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("mounts.options")
		}

		let expectedSource: String?
		let expectedDestination: String
		switch mount.type {
		case "proc":
			expectedSource = "proc"
			expectedDestination = "/proc"
		case "sysfs":
			expectedSource = "sysfs"
			expectedDestination = "/sys"
		case "devtmpfs":
			expectedSource = "devtmpfs"
			expectedDestination = "/dev"
		case "devpts":
			expectedSource = "devpts"
			expectedDestination = "/dev/pts"
		case "tmpfs":
			expectedSource = "tmpfs"
			expectedDestination = "/tmp"
		case "cgroup2":
			expectedSource = "cgroup2"
			expectedDestination = "/sys/fs/cgroup"
		default:
			return
		}

		guard mount.destination == expectedDestination else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("mounts.destination")
		}
		if let source = mount.source, source != expectedSource {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("mounts.source")
		}
	}

	private static func rejectUnsupportedProcessFeatures(_ process: OCIRuntimeProcess) throws {
		if process.apparmorProfile != nil {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("process.apparmorProfile")
		}
		if process.selinuxLabel != nil {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("process.selinuxLabel")
		}
	}

	private static func validatedConsoleSize(_ size: OCIProcessConsoleSize?,
						 terminal: Bool) throws
		-> OrlixOCIRuntimeConsoleSize?
	{
		guard let size else {
			return nil
		}
		guard terminal, size.height > 0, size.width > 0 else {
			throw OrlixOCIRuntimeConfigError.invalidConsoleSize
		}

		return OrlixOCIRuntimeConsoleSize(height: size.height, width: size.width)
	}

	private static func rejectUnsupportedLinuxFeatures(_ linux: OCIRuntimeLinux?) throws {
		guard let linux else {
			return
		}

		if let uidMappings = linux.uidMappings, !uidMappings.isEmpty {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.uidMappings")
		}
		if let gidMappings = linux.gidMappings, !gidMappings.isEmpty {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.gidMappings")
		}
if linux.seccomp != nil {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.seccomp")
        }
        if linux.mountLabel != nil {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.mountLabel")
        }
		if linux.personality != nil {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.personality")
		}
		if let timeOffsets = linux.timeOffsets, !timeOffsets.isEmpty {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.timeOffsets")
		}
		if let unified = linux.unified, !unified.isEmpty {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.unified")
		}
		if linux.intelRdt != nil {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.intelRdt")
		}
		if let hugepageLimits = linux.hugepageLimits, !hugepageLimits.isEmpty {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.hugepageLimits")
		}
		if let rdma = linux.rdma, !rdma.isEmpty {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.rdma")
		}
        if let netDevices = linux.netDevices, !netDevices.isEmpty {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("linux.netDevices")
		}
	}
}

private struct OCIRuntimeConfig: Decodable {
	let ociVersion: String
	let annotations: [String: String]?
	let hostname: String?
	let domainname: String?
	let hooks: OCIRuntimeHooks?
	let process: OCIRuntimeProcess?
	let root: OCIRuntimeRoot?
	let mounts: [OCIRuntimeMount]?
	let linux: OCIRuntimeLinux?
	let solaris: OCIRuntimeUnsupportedPlatform?
	let windows: OCIRuntimeUnsupportedPlatform?
	let vm: OCIRuntimeUnsupportedPlatform?
	let zOS: OCIRuntimeUnsupportedPlatform?
}

private struct OCIRuntimeUnsupportedPlatform: Decodable {}

private struct OCIRuntimeHooks: Decodable {
	let prestart: [OCIRuntimeHook]?
	let createRuntime: [OCIRuntimeHook]?
	let createContainer: [OCIRuntimeHook]?
	let startContainer: [OCIRuntimeHook]?
	let poststart: [OCIRuntimeHook]?
	let poststop: [OCIRuntimeHook]?
}

private struct OCIRuntimeHook: Decodable {
	let path: String
	let args: [String]?
	let env: [String]?
	let timeout: Int?
}

private struct OCIRuntimeProcess: Decodable {
	let terminal: Bool?
	let consoleSize: OCIProcessConsoleSize?
	let args: [String]
	let env: [String]?
	let cwd: String?
	let user: OCIRuntimeUser?
	let rlimits: [OCIRuntimeRlimit]?
	let capabilities: OCIRuntimeCapabilities?
	let apparmorProfile: String?
	let selinuxLabel: String?
	let noNewPrivileges: Bool?
	let oomScoreAdj: Int?
	let scheduler: OCIRuntimeScheduler?
	let ioPriority: OCIRuntimeIOPriority?
	let execCPUAffinity: OCIRuntimeCPUAffinity?
	let closeAdditionalFds: Bool?
}

private struct OCIRuntimeUser: Decodable {
	let uid: UInt32
	let gid: UInt32
	let additionalGids: [UInt32]?
	let umask: UInt32?
}

private struct OCIRuntimeScheduler: Decodable {
	let policy: String?
	let nice: Int?
	let priority: Int?
	let flags: [String]?
}

private struct OCIRuntimeIOPriority: Decodable {
	let `class`: String?
	let priority: Int?
}

private struct OCIRuntimeCPUAffinity: Decodable {
	let initial: String?
	let final: String?
}

private struct OCIRuntimeRoot: Decodable {
	let path: String
	let readonly: Bool?
}

private struct OCIRuntimeMount: Decodable {
	let destination: String
	let type: String
	let source: String?
	let options: [String]?
	let uidMappings: [OCIRuntimeIDMapping]?
	let gidMappings: [OCIRuntimeIDMapping]?
}

private struct OCIRuntimeLinux: Decodable {
	let namespaces: [OCIRuntimeNamespace]?
	let uidMappings: [OCIRuntimeIDMapping]?
	let gidMappings: [OCIRuntimeIDMapping]?
	let devices: [OCIRuntimeDevice]?
	let resources: OCIRuntimeResources?
	let seccomp: OCIRuntimeSeccomp?
	let maskedPaths: [String]?
	let readonlyPaths: [String]?
	let sysctl: [String: String]?
	let mountLabel: String?
	let rootfsPropagation: String?
	let personality: OCIRuntimePersonality?
	let timeOffsets: [String: String]?
	let unified: [String: String]?
	let intelRdt: OCIRuntimeIntelRdt?
	let hugepageLimits: [OCIRuntimeHugepageLimit]?
	let rdma: [String: OCIRuntimeRdmaLimit]?
	let cgroupsPath: String?
	let netDevices: [OCIRuntimeNetDevice]?
}

private struct OCIRuntimePersonality: Decodable {
	let domain: String?
}

private struct OCIRuntimeIntelRdt: Decodable {
	let l3CacheSchema: String?
	let memBwSchema: String?
}

private struct OCIRuntimeHugepageLimit: Decodable {
	let pageSize: String?
	let limit: UInt64?
}

private struct OCIRuntimeRdmaLimit: Decodable {
	let hcaHandles: UInt32?
	let hcaObjects: UInt32?
}

private struct OCIRuntimeNamespace: Decodable {
    let type: String
    let path: String?
}

private struct OCIRuntimeIDMapping: Decodable {
	let containerID: UInt32?
	let hostID: UInt32?
	let size: UInt32?
}

private struct OCIRuntimeDevice: Decodable {
	let path: String?
	let type: String?
	let major: UInt32?
	let minor: UInt32?
	let fileMode: UInt32?
	let uid: UInt32?
	let gid: UInt32?
}
private struct OCIRuntimeResources: Decodable {
    let devices: [OCIRuntimeResourceDevice]?
    let memory: OCIRuntimeResourceMemory?
    let cpu: OCIRuntimeResourceCPU?
    let blockIO: OCIRuntimeResourceBlockIO?
    let hugepageLimits: [OCIRuntimeHugepageLimit]?
    let network: OCIRuntimeResourceNetwork?
    let pids: OCIRuntimeResourcePids?
    let rdma: [String: OCIRuntimeRdmaLimit]?
    let unified: [String: String]?
}
private struct OCIRuntimeResourceDevice: Decodable {}
private struct OCIRuntimeResourceMemory: Decodable {
let limit: Int64?
let reservation: Int64?
let swap: Int64?
let kernel: Int64?
let kernelTCP: Int64?
let swappiness: UInt64?
let disableOOMKiller: Bool?
let useHierarchy: Bool?
}
private struct OCIRuntimeResourceCPU: Decodable {
	let shares: UInt64?
	let quota: Int64?
	let period: UInt64?
	let realtimeRuntime: Int64?
	let realtimePeriod: UInt64?
	let cpus: String?
	let mems: String?
}
private struct OCIRuntimeResourceBlockIO: Decodable {
	let weight: UInt64?
	let leafWeight: UInt64?
	let weightDevice: [OCIRuntimeResourceBlockIODeviceWeight]?
	let throttleReadBpsDevice: [OCIRuntimeResourceBlockIODeviceThrottle]?
	let throttleWriteBpsDevice: [OCIRuntimeResourceBlockIODeviceThrottle]?
	let throttleReadIOPSDevice: [OCIRuntimeResourceBlockIODeviceThrottle]?
	let throttleWriteIOPSDevice: [OCIRuntimeResourceBlockIODeviceThrottle]?
}

private struct OCIRuntimeResourceBlockIODeviceWeight: Decodable {
	let major: UInt32?
	let minor: UInt32?
	let weight: UInt64?
	let leafWeight: UInt64?
}
private struct OCIRuntimeResourceBlockIODeviceThrottle: Decodable {
	let major: UInt32?
	let minor: UInt32?
	let rate: UInt64?
}
private struct OCIRuntimeResourceNetwork: Decodable {}
private struct OCIRuntimeResourcePids: Decodable {
    let limit: Int64?
}
private struct OCIRuntimeSeccomp: Decodable {}
private struct OCIProcessConsoleSize: Decodable {
	let height: UInt32
	let width: UInt32
}
private struct OCIRuntimeRlimit: Decodable {
	let type: String
	let hard: UInt64
	let soft: UInt64
}
private struct OCIRuntimeCapabilities: Decodable {
	let bounding: [String]?
	let permitted: [String]?
	let inheritable: [String]?
	let effective: [String]?
	let ambient: [String]?
}
private struct OCIRuntimeNetDevice: Decodable {}

public enum OrlixOCIRuntimeBundleError: Error, Equatable, Sendable {
	case missingConfig(String)
	case missingRootfs(String)
	case rootfsIsNotDirectory(String)
	case rootfsEscapesBundle(String)
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIRuntimeBundleImportPlan: Equatable, Sendable {
	public let bundle: OrlixOCIRuntimeBundle
	public let ociRuntimeSession: OrlixOCIRuntimeSessionDescriptor
	public let storageLayout: OrlixEnvironmentStorageLayout
	public let materializationPlan: OrlixEnvironmentImageMaterializationPlan

	public var environment: OrlixEnvironmentDescriptor {
		ociRuntimeSession.environment
	}

	@discardableResult
	public func saveEnvironment(
		to registry: OrlixEnvironmentRegistry,
		fileManager: FileManager = .default
	) throws -> OrlixEnvironmentDescriptor {
		try registry.save(environment, fileManager: fileManager)
		return environment
	}

	public func materializationCommands(
		mke2fsExecutable: String = "mke2fs",
		truncateExecutable: String = "truncate",
		debugfsExecutable: String = "debugfs"
	) throws -> [OrlixEnvironmentImageMaterializationCommand] {
		try materializationPlan.commands(
			mke2fsExecutable: mke2fsExecutable,
			truncateExecutable: truncateExecutable,
			debugfsExecutable: debugfsExecutable
		)
	}

	public func prepareMaterializationInputs(
		fileManager: FileManager = .default
	) throws {
		try materializationPlan.prepareInputTrees(fileManager: fileManager)
	}

	@discardableResult
	public func materialize(
		mke2fsExecutable: String = "mke2fs",
		truncateExecutable: String = "truncate",
		debugfsExecutable: String = "debugfs",
		fileManager: FileManager = .default,
		runner: OrlixEnvironmentImageMaterializationCommandRunner
	) throws -> OrlixEnvironmentImageMaterializationResult {
		try prepareMaterializationInputs(fileManager: fileManager)
		try materializationPlan.writeBaseImageMetadataCommands(
			manifest: [],
			fileManager: fileManager
		)
		try materializationPlan.writeStateImageMetadataCommands(fileManager: fileManager)
		return try materializationPlan.materialize(
			mke2fsExecutable: mke2fsExecutable,
			truncateExecutable: truncateExecutable,
			debugfsExecutable: debugfsExecutable,
			runner: runner
		)
	}

	public func materializationToolchainCheck(
		mke2fsExecutable: String = "mke2fs",
		truncateExecutable: String = "truncate",
		debugfsExecutable: String = "debugfs",
		searchPath: [URL]? = nil,
		fileManager: FileManager = .default
	) throws -> OrlixOCIRuntimeBundleMaterializationToolchainCheck {
		let commands = try materializationCommands(
			mke2fsExecutable: mke2fsExecutable,
			truncateExecutable: truncateExecutable,
			debugfsExecutable: debugfsExecutable
		)
		let requiredExecutables = [
			mke2fsExecutable,
			truncateExecutable,
			debugfsExecutable
		]
		let resolvedExecutables = Self.resolveExecutables(
			requiredExecutables,
			searchPath: searchPath ?? Self.defaultExecutableSearchPath(),
			fileManager: fileManager
		)
		let missingExecutables = requiredExecutables.filter {
			resolvedExecutables[$0] == nil
		}
		return OrlixOCIRuntimeBundleMaterializationToolchainCheck(
			commands: commands,
			requiredExecutables: requiredExecutables,
			resolvedExecutables: resolvedExecutables,
			missingExecutables: missingExecutables
		)
	}

	public func materializedRootImage(
		registry: OrlixEnvironmentRegistry,
		kernelCommandLine: String? = OrlixEnvironmentRootImage.defaultKernelCommandLine,
		fileManager: FileManager = .default
	) throws -> OrlixEnvironmentRootImage {
		try saveEnvironment(to: registry, fileManager: fileManager)
		return try registry.materializedRootImage(
			forEnvironmentID: environment.id,
			kernelCommandLine: kernelCommandLine,
			fileManager: fileManager
		)
	}

	public func linuxSession(
		registry: OrlixEnvironmentRegistry,
		kernelCommandLine: String? = OrlixEnvironmentRootImage.defaultKernelCommandLine,
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		fileManager: FileManager = .default
	) throws -> OrlixLinuxSession {
		try OrlixLinuxSession(
			materializedRootImage: materializedRootImage(
				registry: registry,
				kernelCommandLine: kernelCommandLine,
				fileManager: fileManager
			),
			terminal: terminal
		)
	}

	private static func defaultExecutableSearchPath() -> [URL] {
		ProcessInfo.processInfo.environment["PATH", default: ""]
			.split(separator: ":")
			.map { URL(fileURLWithPath: String($0), isDirectory: true) }
	}

	private static func resolveExecutables(
		_ executableNames: [String],
		searchPath: [URL],
		fileManager: FileManager
	) -> [String: URL] {
		var resolved: [String: URL] = [:]
		for executableName in executableNames where resolved[executableName] == nil {
			for directory in searchPath {
				let candidate = directory.appendingPathComponent(executableName)
				guard fileManager.isExecutableFile(atPath: candidate.path) else {
					continue
				}
				resolved[executableName] = candidate
				break
			}
		}
		return resolved
	}
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIRuntimeBundleMaterializationToolchainCheck:
	Equatable,
	Sendable
{
	public let commands: [OrlixEnvironmentImageMaterializationCommand]
	public let requiredExecutables: [String]
	public let resolvedExecutables: [String: URL]
	public let missingExecutables: [String]

	public var isReady: Bool {
		missingExecutables.isEmpty
	}
}

public struct OrlixOCIRuntimeBundle: Equatable, Sendable {
	public let bundleURL: URL
	public let configURL: URL
	public let rootfsURL: URL
	public let config: OrlixOCIRuntimeConfigDescriptor

	public static func load(
		from bundleURL: URL,
		fileManager: FileManager = .default,
		parser: OrlixOCIRuntimeConfigParser = OrlixOCIRuntimeConfigParser()
	) throws -> OrlixOCIRuntimeBundle {
		let configURL = bundleURL.appendingPathComponent("config.json")

		guard fileManager.fileExists(atPath: configURL.path) else {
			throw OrlixOCIRuntimeBundleError.missingConfig(configURL.path)
		}

		let configData = try Data(contentsOf: configURL)
		let config = try parser.parse(configData)
		let rootfsURL = try resolveRootfsURL(
			for: config,
			bundleURL: bundleURL,
			fileManager: fileManager
		)
		var isDirectory: ObjCBool = false
		guard fileManager.fileExists(atPath: rootfsURL.path, isDirectory: &isDirectory) else {
			throw OrlixOCIRuntimeBundleError.missingRootfs(rootfsURL.path)
		}
		guard isDirectory.boolValue else {
			throw OrlixOCIRuntimeBundleError.rootfsIsNotDirectory(rootfsURL.path)
		}

		return OrlixOCIRuntimeBundle(
			bundleURL: bundleURL,
			configURL: configURL,
			rootfsURL: rootfsURL,
			config: config
		)
	}

	private static func resolveRootfsURL(
		for config: OrlixOCIRuntimeConfigDescriptor,
		bundleURL: URL,
		fileManager: FileManager
	) throws -> URL {
		let rootPath = config.rootPath ?? "rootfs"
		let candidateURL: URL
		if rootPath.hasPrefix("/") {
			candidateURL = URL(fileURLWithPath: rootPath, isDirectory: true)
		} else {
			candidateURL = bundleURL.appendingPathComponent(rootPath, isDirectory: true)
		}

		let bundlePath = bundleURL
			.resolvingSymlinksInPath()
			.standardizedFileURL
			.path
		let candidatePath = candidateURL
			.resolvingSymlinksInPath()
			.standardizedFileURL
			.path
		guard candidatePath == bundlePath || candidatePath.hasPrefix(bundlePath + "/") else {
			throw OrlixOCIRuntimeBundleError.rootfsEscapesBundle(rootPath)
		}

		return candidateURL
	}

	public func lifecycleController(id: String) -> OrlixOCIRuntimeLifecycleController {
		OrlixOCIRuntimeLifecycleController(
			config: config,
			id: id,
			bundlePath: bundleURL.path
		)
	}

	@_spi(OrlixPrivateTesting)
	public func sessionDescriptor(
		id: String,
		rootMount: OrlixEnvironmentRootMount
	) throws -> OrlixOCIRuntimeSessionDescriptor {
		try OrlixEnvironmentStorageLayout.validateEnvironmentID(id)
		return try lifecycleController(id: id)
			.create()
			.sessionDescriptor(rootMount: rootMount)
	}

	@_spi(OrlixPrivateTesting)
	public func importPlan(
		id: String,
		rootMount: OrlixEnvironmentRootMount,
		storagePolicy: OrlixStoragePolicy = .current,
		fileManager: FileManager = .default
	) throws -> OrlixOCIRuntimeBundleImportPlan {
		try importPlan(
			id: id,
			rootMount: rootMount,
			storageLayout: OrlixEnvironmentStorageLayout.layout(
				forEnvironmentID: id,
				policy: storagePolicy,
				fileManager: fileManager
			),
			fileManager: fileManager
		)
	}

	@_spi(OrlixPrivateTesting)
	public func importPlan(
		id: String,
		rootMount: OrlixEnvironmentRootMount,
		registry: OrlixEnvironmentRegistry,
		fileManager: FileManager = .default
	) throws -> OrlixOCIRuntimeBundleImportPlan {
		try importPlan(
			id: id,
			rootMount: rootMount,
			storageLayout: registry.layout(forEnvironmentID: id),
			fileManager: fileManager
		)
	}

	private func importPlan(
		id: String,
		rootMount: OrlixEnvironmentRootMount,
		storageLayout: OrlixEnvironmentStorageLayout,
		fileManager: FileManager
	) throws -> OrlixOCIRuntimeBundleImportPlan {
		var isDirectory = ObjCBool(false)
		guard fileManager.fileExists(atPath: rootfsURL.path, isDirectory: &isDirectory) else {
			throw OrlixOCIRuntimeBundleError.missingRootfs(rootfsURL.path)
		}
		guard isDirectory.boolValue else {
			throw OrlixOCIRuntimeBundleError.rootfsIsNotDirectory(rootfsURL.path)
		}
		let ociRuntimeSession = try sessionDescriptor(id: id, rootMount: rootMount)
		let materializationPlan = try OrlixEnvironmentImageMaterializationPlan.plan(
			stagingRootDirectory: rootfsURL,
			storageLayout: storageLayout
		)
		return OrlixOCIRuntimeBundleImportPlan(
			bundle: self,
			ociRuntimeSession: ociRuntimeSession,
			storageLayout: storageLayout,
			materializationPlan: materializationPlan
		)
	}
}

public enum OrlixOCIRuntimeLifecycleState: String, Codable, Equatable, Sendable {
	case configured
	case created
	case running
	case stopped
	case deleted
}

public enum OrlixOCIRuntimeLifecycleAction: String, Codable, Equatable, Sendable {
	case create
	case start
	case exit
	case kill
	case delete
}

public enum OrlixOCIRuntimeLifecycleError: Error, Equatable, Sendable {
	case invalidTransition(from: OrlixOCIRuntimeLifecycleState,
			       action: OrlixOCIRuntimeLifecycleAction)
	case invalidPID(Int32)
	case invalidExitStatus(Int32)
	case invalidSignal(Int32)
	case processPIDMismatch(expected: Int32, observed: Int32)
	case stateUnavailable(OrlixOCIRuntimeLifecycleState)
	case stateReportRequiresPID(OrlixOCIRuntimeStateStatus)
}

public struct OrlixOCIRuntimeProcessStartObservation: Equatable, Sendable {
	public let pid: Int32

	public init(pid: Int32) throws {
		guard pid > 0 else {
			throw OrlixOCIRuntimeLifecycleError.invalidPID(pid)
		}

		self.pid = pid
	}
}

public struct OrlixOCIRuntimeProcessExitObservation: Equatable, Sendable {
	public let pid: Int32
	public let exitStatus: Int32

	public init(pid: Int32, exitStatus: Int32) throws {
		guard pid > 0 else {
			throw OrlixOCIRuntimeLifecycleError.invalidPID(pid)
		}

		guard (0...255).contains(exitStatus) else {
			throw OrlixOCIRuntimeLifecycleError.invalidExitStatus(exitStatus)
		}

		self.pid = pid
		self.exitStatus = exitStatus
	}
}

public struct OrlixOCIRuntimeProcessSignalObservation: Equatable, Sendable {
	public let pid: Int32
	public let signal: Int32

	public init(pid: Int32, signal: Int32) throws {
		guard pid > 0 else {
			throw OrlixOCIRuntimeLifecycleError.invalidPID(pid)
		}

		guard (1...127).contains(signal) else {
			throw OrlixOCIRuntimeLifecycleError.invalidSignal(signal)
		}

		self.pid = pid
		self.signal = signal
	}
}

public enum OrlixOCIRuntimeProcessCompletionObservation: Equatable, Sendable {
	case exited(OrlixOCIRuntimeProcessExitObservation)
	case signaled(OrlixOCIRuntimeProcessSignalObservation)
}

public enum OrlixOCIRuntimeStateStatus: String, Codable, Equatable, Sendable {
	case created
	case running
	case stopped
}

public struct OrlixOCIRuntimeStateReport: Codable, Equatable, Sendable {
	public let ociVersion: String
	public let id: String
	public let status: OrlixOCIRuntimeStateStatus
	public let pid: Int32?
	public let bundle: String
	public let annotations: [String: String]
	public let exitStatus: Int32?

	public func jsonData() throws -> Data {
		let encoder = JSONEncoder()
		encoder.outputFormatting = [.prettyPrinted, .sortedKeys, .withoutEscapingSlashes]
		return try encoder.encode(self)
	}
}

public struct OrlixOCIRuntimeLifecycleRecord: Codable, Equatable, Sendable {
	public let id: String
	public let bundlePath: String
	public let pid: Int32?
	public let exitStatus: Int32?
	public let state: OrlixOCIRuntimeLifecycleState

	public init(id: String,
		    bundlePath: String,
		    pid: Int32? = nil,
		    exitStatus: Int32? = nil,
		    state: OrlixOCIRuntimeLifecycleState = .configured)
	{
		self.id = id
		self.bundlePath = bundlePath
		self.pid = pid
		self.exitStatus = exitStatus
		self.state = state
	}
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIRuntimeLifecycleSnapshot: Codable, Equatable, Sendable {
	public let record: OrlixOCIRuntimeLifecycleRecord
	public let ociVersion: String
	public let annotations: [String: String]

	public init(
		record: OrlixOCIRuntimeLifecycleRecord,
		ociVersion: String,
		annotations: [String: String]
	) {
		self.record = record
		self.ociVersion = ociVersion
		self.annotations = annotations
	}

	public func stateReport() throws -> OrlixOCIRuntimeStateReport {
		let status: OrlixOCIRuntimeStateStatus

		switch record.state {
		case .created:
			status = .created
		case .running:
			status = .running
		case .stopped:
			status = .stopped
		case .configured, .deleted:
			throw OrlixOCIRuntimeLifecycleError.stateUnavailable(record.state)
		}

		if status == .running, record.pid == nil {
			throw OrlixOCIRuntimeLifecycleError.stateReportRequiresPID(status)
		}

		return OrlixOCIRuntimeStateReport(
			ociVersion: ociVersion,
			id: record.id,
			status: status,
			pid: record.pid,
			bundle: record.bundlePath,
			annotations: annotations,
			exitStatus: record.exitStatus
		)
	}
}

@_spi(OrlixPrivateTesting)
public enum OrlixOCIRuntimeLifecycleStoreError: Error, Equatable, Sendable {
	case missingRecord(String)
	case recordIDMismatch(expected: String, actual: String)
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIRuntimeLifecycleStore: Sendable {
	public let registry: OrlixEnvironmentRegistry

	public init(registry: OrlixEnvironmentRegistry) {
		self.registry = registry
	}

	public func recordURL(forID id: String) throws -> URL {
		try registry.layout(forEnvironmentID: id)
			.rootDirectory
			.appendingPathComponent("oci-lifecycle.json", isDirectory: false)
	}

	public func save(
		_ controller: OrlixOCIRuntimeLifecycleController,
		fileManager: FileManager = .default
	) throws {
		try save(
			OrlixOCIRuntimeLifecycleSnapshot(
				record: controller.record,
				ociVersion: controller.config.ociVersion,
				annotations: controller.config.annotations
			),
			fileManager: fileManager
		)
	}

	public func save(
		_ snapshot: OrlixOCIRuntimeLifecycleSnapshot,
		fileManager: FileManager = .default
	) throws {
		let url = try recordURL(forID: snapshot.record.id)
		try fileManager.createDirectory(
			at: url.deletingLastPathComponent(),
			withIntermediateDirectories: true
		)
		let encoder = JSONEncoder()
		encoder.outputFormatting = [.prettyPrinted, .sortedKeys, .withoutEscapingSlashes]
		try encoder.encode(snapshot).write(to: url, options: [.atomic])
	}

	public func load(
		id: String,
		fileManager: FileManager = .default
	) throws -> OrlixOCIRuntimeLifecycleSnapshot {
		let url = try recordURL(forID: id)
		guard fileManager.fileExists(atPath: url.path) else {
			throw OrlixOCIRuntimeLifecycleStoreError.missingRecord(id)
		}
		let snapshot = try JSONDecoder().decode(
			OrlixOCIRuntimeLifecycleSnapshot.self,
			from: try Data(contentsOf: url)
		)
		guard snapshot.record.id == id else {
			throw OrlixOCIRuntimeLifecycleStoreError.recordIDMismatch(
				expected: id,
				actual: snapshot.record.id
			)
		}
		return snapshot
	}

	public func stateReport(
		id: String,
		fileManager: FileManager = .default
	) throws -> OrlixOCIRuntimeStateReport {
		try load(id: id, fileManager: fileManager).stateReport()
	}

	public func delete(
		id: String,
		fileManager: FileManager = .default
	) throws {
		let url = try recordURL(forID: id)
		if fileManager.fileExists(atPath: url.path) {
			try fileManager.removeItem(at: url)
		}
	}
}

public struct OrlixOCIRuntimeLifecycleController: Equatable, Sendable {
	public let config: OrlixOCIRuntimeConfigDescriptor
	public let record: OrlixOCIRuntimeLifecycleRecord

	public init(config: OrlixOCIRuntimeConfigDescriptor,
		    id: String,
		    bundlePath: String)
	{
		self.config = config
		self.record = OrlixOCIRuntimeLifecycleRecord(
			id: id,
			bundlePath: bundlePath
		)
	}

	public init(config: OrlixOCIRuntimeConfigDescriptor,
		    record: OrlixOCIRuntimeLifecycleRecord)
	{
		self.config = config
		self.record = record
	}

	public func create() throws -> OrlixOCIRuntimeLifecycleController {
		guard record.state == .configured else {
			throw OrlixOCIRuntimeLifecycleError.invalidTransition(
				from: record.state,
				action: .create
			)
		}
		return withState(.created)
	}

	public func start(pid: Int32) throws -> OrlixOCIRuntimeLifecycleController {
		guard pid > 0 else {
			throw OrlixOCIRuntimeLifecycleError.invalidPID(pid)
		}
		guard record.state == .created else {
			throw OrlixOCIRuntimeLifecycleError.invalidTransition(
				from: record.state,
				action: .start
			)
		}
		return OrlixOCIRuntimeLifecycleController(
			config: config,
			record: OrlixOCIRuntimeLifecycleRecord(
				id: record.id,
				bundlePath: record.bundlePath,
				pid: pid,
				state: .running
			)
		)
	}

	public func kill(signal: Int32) throws -> OrlixOCIRuntimeLifecycleController {
		guard (1...127).contains(signal) else {
			throw OrlixOCIRuntimeLifecycleError.invalidSignal(signal)
		}
		guard record.state == .running else {
			throw OrlixOCIRuntimeLifecycleError.invalidTransition(
				from: record.state,
				action: .kill
			)
		}
		return self
	}

	public func exit(observedProcess observation: OrlixOCIRuntimeProcessExitObservation) throws -> OrlixOCIRuntimeLifecycleController {
		guard record.state == .running else {
			throw OrlixOCIRuntimeLifecycleError.invalidTransition(
				from: record.state,
				action: .exit
			)
		}

		guard record.pid == observation.pid else {
			throw OrlixOCIRuntimeLifecycleError.processPIDMismatch(
				expected: record.pid ?? 0,
				observed: observation.pid
			)
		}

		return try exit(exitStatus: observation.exitStatus)
	}

	public func exit(observedSignal observation: OrlixOCIRuntimeProcessSignalObservation) throws -> OrlixOCIRuntimeLifecycleController {
		guard record.state == .running else {
			throw OrlixOCIRuntimeLifecycleError.invalidTransition(
				from: record.state,
				action: .exit
			)
		}

		guard record.pid == observation.pid else {
			throw OrlixOCIRuntimeLifecycleError.processPIDMismatch(
				expected: record.pid ?? 0,
				observed: observation.pid
			)
		}

		return try exit(exitStatus: 128 + observation.signal)
	}

	public func exit(exitStatus: Int32) throws -> OrlixOCIRuntimeLifecycleController {
		guard (0...255).contains(exitStatus) else {
			throw OrlixOCIRuntimeLifecycleError.invalidExitStatus(exitStatus)
		}

		guard record.state == .running else {
			throw OrlixOCIRuntimeLifecycleError.invalidTransition(
				from: record.state,
				action: .exit
			)
		}
		return OrlixOCIRuntimeLifecycleController(
			config: config,
			record: OrlixOCIRuntimeLifecycleRecord(
				id: record.id,
				bundlePath: record.bundlePath,
				pid: record.pid,
				exitStatus: exitStatus,
				state: .stopped
			)
		)
	}

	public func delete() throws -> OrlixOCIRuntimeLifecycleController {
		switch record.state {
		case .configured, .created, .stopped:
			return withState(.deleted)
		case .running, .deleted:
			throw OrlixOCIRuntimeLifecycleError.invalidTransition(
				from: record.state,
				action: .delete
			)
		}
	}

	public func stateReport() throws -> OrlixOCIRuntimeStateReport {
		let status: OrlixOCIRuntimeStateStatus

		switch record.state {
		case .created:
			status = .created
		case .running:
			status = .running
		case .stopped:
			status = .stopped
	case .configured, .deleted:
		throw OrlixOCIRuntimeLifecycleError.stateUnavailable(record.state)
	}

		if status == .running {
			guard record.pid != nil else {
				throw OrlixOCIRuntimeLifecycleError.stateReportRequiresPID(status)
			}
		}

	return OrlixOCIRuntimeStateReport(
			ociVersion: config.ociVersion,
			id: record.id,
			status: status,
			pid: record.pid,
			bundle: record.bundlePath,
			annotations: config.annotations,
			exitStatus: record.exitStatus
		)
	}

	private func withState(_ state: OrlixOCIRuntimeLifecycleState)
		-> OrlixOCIRuntimeLifecycleController
	{
		OrlixOCIRuntimeLifecycleController(
			config: config,
			record: OrlixOCIRuntimeLifecycleRecord(
				id: record.id,
				bundlePath: record.bundlePath,
				pid: record.pid,
				exitStatus: record.exitStatus,
				state: state
			)
		)
	}
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIRuntimeSessionDescriptor: Equatable, Sendable {
	public let id: String
	public let lifecycleState: OrlixOCIRuntimeLifecycleState
	public let terminal: Bool
	public let consoleSize: OrlixOCIRuntimeConsoleSize?
	public let environment: OrlixEnvironmentDescriptor

	public init(id: String,
	            lifecycleState: OrlixOCIRuntimeLifecycleState,
	            terminal: Bool,
	            consoleSize: OrlixOCIRuntimeConsoleSize?,
	            environment: OrlixEnvironmentDescriptor)
	{
		self.id = id
		self.lifecycleState = lifecycleState
		self.terminal = terminal
		self.consoleSize = consoleSize
		self.environment = environment
	}
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIRuntimeProcessHandle: Sendable {
	public let lifecycle: OrlixOCIRuntimeLifecycleController
	public let rootMount: OrlixEnvironmentRootMount
	public let sessionDescriptor: OrlixOCIRuntimeSessionDescriptor

	public init(lifecycle: OrlixOCIRuntimeLifecycleController,
		    rootMount: OrlixEnvironmentRootMount) throws
	{
		self.lifecycle = lifecycle
		self.rootMount = rootMount
		self.sessionDescriptor = try lifecycle.sessionDescriptor(rootMount: rootMount)
	}

	public func start(observedPID pid: Int32) throws -> OrlixOCIRuntimeProcessHandle {
		try start(
			observedProcess: OrlixOCIRuntimeProcessStartObservation(pid: pid)
		)
	}

	public func start(observedProcess observation: OrlixOCIRuntimeProcessStartObservation) throws -> OrlixOCIRuntimeProcessHandle {
		try OrlixOCIRuntimeProcessHandle(
			lifecycle: lifecycle.start(pid: observation.pid),
			rootMount: rootMount
		)
	}

	public func kill(signal: Int32) throws -> OrlixOCIRuntimeProcessHandle {
		try OrlixOCIRuntimeProcessHandle(
			lifecycle: lifecycle.kill(signal: signal),
			rootMount: rootMount
		)
	}

	public func delete() throws -> OrlixOCIRuntimeDeletedProcess {
		OrlixOCIRuntimeDeletedProcess(
			lifecycle: try lifecycle.delete()
		)
	}

	public func exit(observedProcess observation: OrlixOCIRuntimeProcessExitObservation) throws -> OrlixOCIRuntimeCompletedProcess {
		OrlixOCIRuntimeCompletedProcess(
			lifecycle: try lifecycle.exit(observedProcess: observation)
		)
	}

	public func exit(observedSignal observation: OrlixOCIRuntimeProcessSignalObservation) throws -> OrlixOCIRuntimeCompletedProcess {
		OrlixOCIRuntimeCompletedProcess(
			lifecycle: try lifecycle.exit(observedSignal: observation)
		)
	}

	public func exit(observedCompletion observation: OrlixOCIRuntimeProcessCompletionObservation) throws -> OrlixOCIRuntimeCompletedProcess {
		switch observation {
		case .exited(let processExit):
			return try exit(observedProcess: processExit)
		case .signaled(let processSignal):
			return try exit(observedSignal: processSignal)
		}
	}
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIRuntimeCompletedProcess: Sendable {
	public let lifecycle: OrlixOCIRuntimeLifecycleController

	public init(lifecycle: OrlixOCIRuntimeLifecycleController) {
		self.lifecycle = lifecycle
	}

	public func stateReport() throws -> OrlixOCIRuntimeStateReport {
		try lifecycle.stateReport()
	}

	public func delete() throws -> OrlixOCIRuntimeDeletedProcess {
		OrlixOCIRuntimeDeletedProcess(
			lifecycle: try lifecycle.delete()
		)
	}
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIRuntimeDeletedProcess: Sendable {
	public let lifecycle: OrlixOCIRuntimeLifecycleController

	public init(lifecycle: OrlixOCIRuntimeLifecycleController) {
		self.lifecycle = lifecycle
	}
}

@_spi(OrlixPrivateTesting)
public extension OrlixOCIRuntimeLifecycleController {
	func sessionDescriptor(rootMount: OrlixEnvironmentRootMount)
		throws -> OrlixOCIRuntimeSessionDescriptor
	{
		switch record.state {
		case .created, .running:
			return OrlixOCIRuntimeSessionDescriptor(
				id: record.id,
				lifecycleState: record.state,
				terminal: config.terminal,
				consoleSize: config.consoleSize,
				environment: try config.environmentDescriptor(
					id: record.id,
					rootMount: rootMount
				)
			)
		case .configured, .stopped, .deleted:
			throw OrlixOCIRuntimeLifecycleError.stateUnavailable(record.state)
		}
	}
}
