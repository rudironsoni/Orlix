import Foundation

@_silgen_name("OrlixBoot")
private func OrlixBoot(_ config: UnsafePointer<COrlixBootConfig>) -> CInt

private struct COrlixBootProgressEvent {
    var sequence: UInt64
    var monotonicNS: UInt64
    var stage: UInt32
    var status: Int32
    var machKernReturn: Int32
    var posixErrno: Int32
}

@_silgen_name("orlix_host_boot_progress_reset")
private func orlix_host_boot_progress_reset()

@_silgen_name("orlix_host_boot_progress_record")
private func orlix_host_boot_progress_record(
    _ stage: UInt32,
    _ status: Int32,
    _ machKernReturn: Int32,
    _ posixErrno: Int32
)

@_silgen_name("orlix_host_boot_progress_snapshot")
private func orlix_host_boot_progress_snapshot(
    _ events: UnsafeMutablePointer<COrlixBootProgressEvent>?,
    _ capacity: UInt32
) -> UInt32

@_silgen_name("orlix_host_boot_progress_latest")
private func orlix_host_boot_progress_latest(
    _ event: UnsafeMutablePointer<COrlixBootProgressEvent>?
) -> CInt

private enum COrlixBootStage {
    static let sessionCreated: UInt32 = 10
    static let payloadRegistering: UInt32 = 20
    static let payloadRegistered: UInt32 = 30
    static let bootloaderEntered: UInt32 = 40
    static let kernelHandoff: UInt32 = 70
    static let failed: UInt32 = 1000
}

@_silgen_name("orlix_host_console_set_output_fd")
private func orlix_host_console_set_output_fd(_ source: UInt32, _ fd: CInt)

@_silgen_name("orlix_host_console_enqueue_input")
private func orlix_host_console_enqueue_input(
	_ source: UInt32,
    _ bytes: UnsafeRawPointer?,
    _ length: UInt
) -> UInt

@_silgen_name("orlix_host_console_clear_input")
private func orlix_host_console_clear_input(_ source: UInt32)

@_silgen_name("orlix_host_console_pending_input")
private func orlix_host_console_pending_input(_ source: UInt32) -> UInt

@_silgen_name("orlix_host_console_recent_output_clear")
private func orlix_host_console_recent_output_clear(_ source: UInt32)

@_silgen_name("orlix_host_console_recent_output_snapshot")
private func orlix_host_console_recent_output_snapshot(
    _ source: UInt32,
    _ bytes: UnsafeMutableRawPointer?,
    _ capacity: UInt
) -> UInt

private enum COrlixHostConsoleSource {
    static let serial: UInt32 = 0
    static let virtio: UInt32 = 1
}

@_spi(OrlixPrivateTesting)
public enum OrlixPaneTransportDiagnostics {
	public static let serialSource: UInt32 = COrlixHostConsoleSource.serial
	public static let virtioSource: UInt32 = COrlixHostConsoleSource.virtio

	public static func setOutputFD(source: UInt32, fd: Int32) {
		orlix_host_console_set_output_fd(source, fd)
	}

	@discardableResult
	public static func enqueueInput(source: UInt32, data: Data) -> UInt {
		data.withUnsafeBytes {
			orlix_host_console_enqueue_input(
				source, $0.baseAddress, UInt($0.count)
			)
		}
	}

	public static func clearInput(source: UInt32) {
		orlix_host_console_clear_input(source)
	}

	public static func pendingInput(source: UInt32) -> UInt {
		orlix_host_console_pending_input(source)
	}
}

@_silgen_name("orlix_host_resources_set_payload_root_path")
private func orlix_host_resources_set_payload_root_path(
    _ path: UnsafePointer<CChar>
) -> CInt

@_silgen_name("orlix_host_resources_clear_root_images")
private func orlix_host_resources_clear_root_images() -> CInt

@_silgen_name("orlix_host_resources_register_root_image")
private func orlix_host_resources_register_root_image(
    _ identifier: UnsafePointer<CChar>,
    _ initrdBundleName: UnsafePointer<CChar>?,
    _ initrdBundleExtension: UnsafePointer<CChar>?,
    _ initrdResource: UnsafePointer<CChar>,
    _ baseBlockResource: UnsafePointer<CChar>,
    _ stateBlockResource: UnsafePointer<CChar>,
    _ baseBlockDevice: UInt32,
    _ stateBlockDevice: UInt32,
    _ stateBlockMinimumBytes: UInt64
) -> CInt

@_silgen_name("orlix_host_resources_register_root_image_files")
private func orlix_host_resources_register_root_image_files(
    _ identifier: UnsafePointer<CChar>,
    _ initrdBundleName: UnsafePointer<CChar>?,
    _ initrdBundleExtension: UnsafePointer<CChar>?,
    _ initrdResource: UnsafePointer<CChar>,
    _ baseBlockPath: UnsafePointer<CChar>,
    _ stateBlockPath: UnsafePointer<CChar>,
    _ baseBlockDevice: UInt32,
    _ stateBlockDevice: UInt32,
    _ stateBlockMinimumBytes: UInt64
) -> CInt

@_silgen_name("orlix_host_resources_register_host_directory_xattr")
private func orlix_host_resources_register_host_directory_xattr(
    _ identifier: UnsafePointer<CChar>,
    _ relativePath: UnsafePointer<CChar>,
    _ name: UnsafePointer<CChar>,
    _ value: UnsafeRawPointer?,
    _ valueLength: UInt32
) -> CInt

@_silgen_name("orlix_host_resources_clear_host_directories")
private func orlix_host_resources_clear_host_directories() -> CInt

@_silgen_name("orlix_host_resources_register_host_directory")
private func orlix_host_resources_register_host_directory(
    _ identifier: UnsafePointer<CChar>,
    _ hostPath: UnsafePointer<CChar>,
    _ readOnly: UInt32
) -> CInt

private struct COrlixBootConfig {
    var profile: CInt
    var kernelCommandLine: UnsafePointer<CChar>?
    var rootImageIdentifier: UnsafePointer<CChar>?
    var terminalIdentifier: UnsafePointer<CChar>?
}

public enum OrlixBootProfile: Sendable {
    case release
    case development

    fileprivate var cValue: CInt {
        switch self {
        case .release:
            return 0
        case .development:
            return 1
        }
    }
}

public enum OrlixBootStatus: Equatable, Sendable {
    case ok
    case invalidConfig
    case unavailable
    case alreadyStarted
    case unknown(CInt)

    init(rawStatus: CInt) {
        switch rawStatus {
        case 0:
            self = .ok
        case -1:
            self = .invalidConfig
        case -2:
            self = .unavailable
        case -3:
            self = .alreadyStarted
        default:
            self = .unknown(rawStatus)
        }
    }

    public var message: String {
        switch self {
        case .ok:
            return "Orlix boot entered the Linux kernel path."
        case .invalidConfig:
            return "Orlix bootloader rejected the boot config."
        case .unavailable:
            return "Orlix could not reserve the hosted Linux boot address space."
        case .alreadyStarted:
            return "Orlix boot already started in this process."
        case .unknown:
            return "Orlix bootloader returned an unknown status."
        }
    }
}

public enum OrlixBootStage: UInt32, Sendable {
    case unknown = 0
    case sessionCreated = 10
    case payloadRegistering = 20
    case payloadRegistered = 30
    case bootloaderEntered = 40
    case bootConfigValidated = 50
    case hostResourcesReady = 60
    case kernelHandoff = 70
    case archEntry = 80
    case earlyConsoleReady = 90
    case linuxStartKernel = 100
    case firstConsoleOutput = 110
    case failed = 1000

    fileprivate init(cStage: UInt32) {
        self = OrlixBootStage(rawValue: cStage) ?? .unknown
    }
}

public struct OrlixBootProgressEvent: Equatable, Sendable {
    public let sequence: UInt64
    public let stage: OrlixBootStage
    public let statusCode: Int32
    public let machKernReturn: Int32?
    public let posixErrno: Int32?

    fileprivate init(cEvent: COrlixBootProgressEvent) {
        self.sequence = cEvent.sequence
        self.stage = OrlixBootStage(cStage: cEvent.stage)
        self.statusCode = cEvent.status
        self.machKernReturn = cEvent.machKernReturn == 0
            ? nil
            : cEvent.machKernReturn
        self.posixErrno = cEvent.posixErrno == 0 ? nil : cEvent.posixErrno
    }

    init(
        sequence: UInt64,
        rawStage: UInt32,
        statusCode: Int32,
        machKernReturn: Int32,
        posixErrno: Int32
    ) {
        self.sequence = sequence
        self.stage = OrlixBootStage(cStage: rawStage)
        self.statusCode = statusCode
        self.machKernReturn = machKernReturn == 0 ? nil : machKernReturn
        self.posixErrno = posixErrno == 0 ? nil : posixErrno
    }
}

public enum OrlixMachineState: Equatable, Sendable {
    case idle
    case preparing
    case booting
    case running
    case failed(OrlixBootProgressEvent?)
    case stopped
}

public struct OrlixMachineSnapshot: Equatable, Sendable {
    public let state: OrlixMachineState
    public let latestBootProgress: OrlixBootProgressEvent?
    public let hasConsoleOutput: Bool
}

public struct OrlixBootConfig: Equatable, Sendable {
    public var profile: OrlixBootProfile
    public var kernelCommandLine: String?
    public var rootImageIdentifier: String
    public var terminalIdentifier: String

    public init(
        profile: OrlixBootProfile,
        kernelCommandLine: String? =
            OrlixOSDistribution.bundledKernelCommandLine,
        rootImageIdentifier: String =
            OrlixOSDistribution.productRootImageIdentifier ?? "",
        terminalIdentifier: String = "orlix.terminal.main"
    ) {
        self.profile = profile
        self.kernelCommandLine = kernelCommandLine
        self.rootImageIdentifier = rootImageIdentifier
        self.terminalIdentifier = terminalIdentifier
    }
}

public enum OrlixOSDistribution {
    public static var bundledBootProfile: OrlixBootProfile? {
        OrlixOSResources.selectedBootProfile
    }

    public static var bundledKernelCommandLine: String? {
        OrlixOSResources.kernelCommandLine
    }

    public static var productRootImageIdentifier: String? {
        OrlixOSResources.productRootImageIdentifier
    }

    @_spi(OrlixPrivateTesting)
    public static func rootImageDescriptor(
        forRole role: String
    ) -> OrlixRootImageDescriptor? {
        OrlixOSResources.rootImageDescriptors.first { $0.role == role }
    }
}

@_spi(OrlixPrivateTesting)
public struct OrlixRootImageDescriptor: Equatable, Sendable {
    public let role: String
    public let identifier: String
    public let kernelCommandLine: String?
    public let initrdBundleName: String?
    public let initrdBundleExtension: String?
    public let initrdResource: String?
}

private final class OrlixOSBundleAnchor {}

enum OrlixOSResources {
    private static let manifestName = "OrlixOSManifest"
    private static let selectedProfileKey = "OrlixSelectedProfile"
    private static let kernelCommandLineKey = "OrlixKernelCommandLine"
    private static let rootInitrdResourceKey = "OrlixRootInitramfs"
    private static let baseRootImageResourceKey = "OrlixBaseRootImage"
    private static let stateRootImageResourceKey = "OrlixStateRootImage"
    private static let baseRootDeviceKey = "OrlixBaseRootDevice"
    private static let stateRootDeviceKey = "OrlixStateRootDevice"
    private static let baseRootHostBlockDeviceKey =
        "OrlixBaseRootHostBlockDevice"
    private static let stateRootHostBlockDeviceKey =
        "OrlixStateRootHostBlockDevice"
    private static let stateRootMinimumBytesKey = "OrlixStateRootMinimumBytes"
    private static let productRootImageIdentifierKey =
        "OrlixOSProductRootImageIdentifier"
    private static let rootImageInitrdBundleExtensionKey =
        "OrlixOSRootImageInitrdBundleExtension"
    private static let rootImagesKey = "OrlixOSRootImages"
    private static let rootImageRoleKey = "OrlixRootImageRole"
    private static let rootImageIdentifierKey = "OrlixRootImageIdentifier"
    private static let rootImageKernelCommandLineKey =
        "OrlixRootImageKernelCommandLine"
    private static let rootImageInitrdBundleNameKey =
        "OrlixRootImageInitrdBundleName"
    private static let rootImageInitrdResourceKey =
        "OrlixRootImageInitrdResource"

    struct ProductRootResources {
        let initrdResource: String
        let baseBlockResource: String
        let stateBlockResource: String
        let baseBlockDevice: UInt32
        let stateBlockDevice: UInt32
        let stateBlockMinimumBytes: UInt64
    }

    static var resourceRootURL: URL {
        frameworkBundle.resourceURL ?? frameworkBundle.bundleURL
    }

    static var manifestURL: URL? {
        frameworkBundle.url(
            forResource: manifestName,
            withExtension: "plist"
        )
    }

    static var selectedBootProfile: OrlixBootProfile? {
        guard let profile = bundleMetadataValue(for: selectedProfileKey)
        else {
            return nil
        }

        return bootProfile(forPayloadProfile: profile)
    }

    static func bootProfile(forPayloadProfile profile: String) -> OrlixBootProfile? {
        switch profile {
        case "release":
            return .release
        case "development":
            return .development
        default:
            return nil
        }
    }

    static var kernelCommandLine: String? {
        bundleMetadataValue(for: kernelCommandLineKey)
    }

    static var productRootImageIdentifier: String? {
        bundleMetadataValue(for: productRootImageIdentifierKey)
    }

    static var rootImageDescriptors: [OrlixRootImageDescriptor] {
        guard let entries = frameworkBundle.object(
            forInfoDictionaryKey: rootImagesKey
        ) as? [[String: Any]] else {
            return []
        }

        return entries.compactMap { entry in
            guard let role = metadataString(rootImageRoleKey, in: entry),
                  let identifier = metadataString(rootImageIdentifierKey, in: entry)
            else {
                return nil
            }

            let initrdBundleName = metadataString(
                rootImageInitrdBundleNameKey,
                in: entry
            )
            let initrdBundleExtension = initrdBundleName == nil ? nil :
                bundleMetadataValue(for: rootImageInitrdBundleExtensionKey)

            return OrlixRootImageDescriptor(
                role: role,
                identifier: identifier,
                kernelCommandLine: metadataString(
                    rootImageKernelCommandLineKey,
                    in: entry
                ),
                initrdBundleName: initrdBundleName,
                initrdBundleExtension: initrdBundleExtension,
                initrdResource: metadataString(
                    rootImageInitrdResourceKey,
                    in: entry
                )
            )
        }
    }

    static func registerWithHostAdapter() -> Bool {
        let resourceRootPath = resourceRootURL.path
        guard let productResources = productResources,
              let productRootImageIdentifier = productRootImageIdentifier,
              rootImageDescriptors.contains(
                where: { $0.identifier == productRootImageIdentifier }
              )
        else {
            return false
        }

        guard resourceRootPath.withCString({ path in
            orlix_host_resources_set_payload_root_path(path) == 0
        }) else {
            return false
        }
        guard orlix_host_resources_clear_root_images() == 0 else {
            return false
        }
        guard orlix_host_resources_clear_host_directories() == 0 else {
            _ = orlix_host_resources_clear_root_images()
            return false
        }

        for descriptor in rootImageDescriptors {
            guard registerRootImage(
                descriptor,
                productResources: productResources
            ) else {
                _ = orlix_host_resources_clear_root_images()
                _ = orlix_host_resources_clear_host_directories()
                return false
            }
        }
        return true
    }

    static func registerMaterializedRootImage(
        _ rootImage: OrlixEnvironmentRootImage
    ) -> Bool {
        guard let productResources = productResources
        else {
            return false
        }
        return registerMaterializedRootImage(
            rootImage,
            resourceRootPath: resourceRootURL.path,
            productResources: productResources
        )
    }

    static func registerAdditionalHostDirectoriesForTesting(
        _ directories: [OrlixHostDirectoryRegistration]
    ) -> Bool {
        guard !directories.isEmpty else {
            return true
        }
        return registerHostDirectories(directories)
    }

    static func registerMaterializedRootImage(
        _ rootImage: OrlixEnvironmentRootImage,
        resourceRootPath: String,
        productResources: ProductRootResources
    ) -> Bool {
        guard resourceRootPath.withCString({ path in
            orlix_host_resources_set_payload_root_path(path) == 0
        }) else {
            return false
        }
        guard orlix_host_resources_clear_root_images() == 0 else {
            return false
        }

        let initrdBundleName = ""
        let initrdBundleExtension = ""
        let registered = rootImage.rootImageIdentifier.withCString { identifier in
            initrdBundleName.withCString { bundleName in
                initrdBundleExtension.withCString { bundleExtension in
                    productResources.initrdResource.withCString { initrd in
                        rootImage.baseImageURL.path.withCString { base in
                            rootImage.stateImageURL.path.withCString { state in
                                orlix_host_resources_register_root_image_files(
                                    identifier,
                                    bundleName,
                                    bundleExtension,
                                    initrd,
                                    base,
                                    state,
                                    productResources.baseBlockDevice,
                                    productResources.stateBlockDevice,
                                    productResources.stateBlockMinimumBytes
                                ) == 0
                            }
                        }
                    }
                }
            }
        }
        let resourcesRegistered = registered
            && registerHostDirectories(rootImage.hostDirectories)
            && registerHostDirectoryExtendedAttributes(rootImage.hostDirectoryExtendedAttributes)
        if !resourcesRegistered {
            _ = orlix_host_resources_clear_root_images()
            _ = orlix_host_resources_clear_host_directories()
        }
        return resourcesRegistered
    }

    private static func registerHostDirectories(
        _ directories: [OrlixHostDirectoryRegistration]
    ) -> Bool {
        guard orlix_host_resources_clear_host_directories() == 0 else {
            return false
        }

        for directory in directories {
            let registered = directory.identifier.withCString { identifier in
                directory.hostPath.withCString { hostPath in
                    orlix_host_resources_register_host_directory(
                        identifier,
                        hostPath,
                        directory.readOnly ? 1 : 0
                    ) == 0
                }
            }
            guard registered else {
                return false
            }
        }

        return true
    }

    private static func registerHostDirectoryExtendedAttributes(
        _ attributes: [OrlixHostDirectoryExtendedAttribute]
    ) -> Bool {
        for attribute in attributes {
            guard attribute.value.count <= Int(UInt32.max) else {
                return false
            }

            let registered = attribute.identifier.withCString { identifier in
                attribute.relativePath.withCString { relativePath in
                    attribute.name.withCString { name in
                        attribute.value.withUnsafeBytes { bytes in
                            orlix_host_resources_register_host_directory_xattr(
                                identifier,
                                relativePath,
                                name,
                                bytes.baseAddress,
                                UInt32(attribute.value.count)
                            )
                        }
                    }
                }
            }

            guard registered == 0 else {
                return false
            }
        }

        return true
    }

    private static var productResources: ProductRootResources? {
        guard let initrdResource = bundleMetadataValue(
                for: rootInitrdResourceKey
              ),
              let baseBlockResource = bundleMetadataValue(
                for: baseRootImageResourceKey
              ),
              let stateBlockResource = bundleMetadataValue(
                for: stateRootImageResourceKey
              ),
              bundleMetadataValue(for: baseRootDeviceKey) != nil,
              bundleMetadataValue(for: stateRootDeviceKey) != nil,
              let baseBlockDevice = bundleMetadataUInt32(
                for: baseRootHostBlockDeviceKey
              ),
              let stateBlockDevice = bundleMetadataUInt32(
                for: stateRootHostBlockDeviceKey
              ),
              let stateBlockMinimumBytes = bundleMetadataUInt64(
                for: stateRootMinimumBytesKey
              )
        else {
            return nil
        }

        return ProductRootResources(
            initrdResource: initrdResource,
            baseBlockResource: baseBlockResource,
            stateBlockResource: stateBlockResource,
            baseBlockDevice: baseBlockDevice,
            stateBlockDevice: stateBlockDevice,
            stateBlockMinimumBytes: stateBlockMinimumBytes
        )
    }

    private static var frameworkBundle: Bundle {
        Bundle(for: OrlixOSBundleAnchor.self)
    }

    private static func bundleMetadataValue(for key: String) -> String? {
        guard let value = frameworkBundle.object(forInfoDictionaryKey: key)
                as? String,
              !value.isEmpty
        else {
            return nil
        }

        return value
    }

    private static func bundleMetadataUInt32(for key: String) -> UInt32? {
        guard let value = bundleMetadataUInt64(for: key),
              value <= UInt64(UInt32.max)
        else {
            return nil
        }

        return UInt32(value)
    }

    private static func bundleMetadataUInt64(for key: String) -> UInt64? {
        if let value = frameworkBundle.object(
            forInfoDictionaryKey: key
        ) as? NSNumber {
            let intValue = value.int64Value
            guard intValue >= 0 else {
                return nil
            }
            return UInt64(intValue)
        }
        guard let text = bundleMetadataValue(for: key) else {
            return nil
        }

        return UInt64(text)
    }

    private static func metadataString(
        _ key: String,
        in metadata: [String: Any]
    ) -> String? {
        guard let value = metadata[key] as? String,
              !value.isEmpty
        else {
            return nil
        }

        return value
    }

    private static func registerRootImage(
        _ descriptor: OrlixRootImageDescriptor,
        productResources: ProductRootResources
    ) -> Bool {
        let initrdBundleName = descriptor.initrdBundleName ?? ""
        let initrdBundleExtension = descriptor.initrdBundleExtension ?? ""
        let initrdResource =
            descriptor.initrdResource ?? productResources.initrdResource

        return descriptor.identifier.withCString { identifier in
            initrdBundleName.withCString { bundleName in
                initrdBundleExtension.withCString { bundleExtension in
                    initrdResource.withCString { initrd in
                        productResources.baseBlockResource.withCString { base in
                            productResources.stateBlockResource.withCString { state in
                                orlix_host_resources_register_root_image(
                                    identifier,
                                    bundleName,
                                    bundleExtension,
                                    initrd,
                                    base,
                                    state,
                                    productResources.baseBlockDevice,
                                    productResources.stateBlockDevice,
                                    productResources.stateBlockMinimumBytes
                                ) == 0
                            }
                        }
                    }
                }
            }
        }
    }
}

public protocol OrlixTerminalInput: AnyObject {
    func send(_ data: Data)
    func resize(rows: UInt32, columns: UInt32)
}

protocol OrlixPaneTransport: AnyObject {
	func attachOutput(
		_ handler: @escaping @Sendable (Data) -> Void
	) -> OrlixTerminalOutput
	func send(_ data: Data)
	func resize(rows: UInt32, columns: UInt32)
	func configureOutputSource(_ source: UInt32) -> Bool
	func clearRecentOutput()
	func recentOutput() -> Data
}

public final class OrlixTerminalOutput: @unchecked Sendable {
    private let cancelHandler: @Sendable () -> Void
    private let lock = NSLock()
    private var isCancelled = false

    init(cancel: @escaping @Sendable () -> Void) {
        self.cancelHandler = cancel
    }

    public func cancel() {
        lock.lock()
        let shouldCancel = !isCancelled
        isCancelled = true
        lock.unlock()

        if shouldCancel {
            cancelHandler()
        }
    }

    deinit {
        cancel()
    }
}

public final class OrlixTerminalSession: OrlixTerminalInput, @unchecked Sendable {
    public enum BackendState: Equatable, Sendable {
        case unbound
    }

    public let id: String
    public let backendState: BackendState

    private let transport: OrlixPaneTransport
    private let geometryLock = NSLock()
    private var geometry: (rows: UInt32, columns: UInt32)?
    private weak var kernelSession: OrlixKernelSession?

    public convenience init() {
        self.init(transport: HostConsolePaneTransport())
    }

    init(
        id: String = UUID().uuidString,
        transport: OrlixPaneTransport
    ) {
        self.id = id
        self.transport = transport
        backendState = .unbound
    }

    @discardableResult
    public func attachOutput(
        _ handler: @escaping @Sendable (Data) -> Void
    ) -> OrlixTerminalOutput {
        transport.attachOutput(handler)
    }

    public func send(_ data: Data) {
        transport.send(data)
    }

    public func resize(rows: UInt32, columns: UInt32) {
        guard rows > 0, rows <= UInt32(UInt16.max),
              columns > 0, columns <= UInt32(UInt16.max)
        else {
            return
        }
        geometryLock.lock()
        geometry = (rows, columns)
        geometryLock.unlock()
        transport.resize(rows: rows, columns: columns)
    }

    public func boot() -> OrlixBootStatus {
        kernelSession?.boot() ?? .invalidConfig
    }

    func bind(kernelSession: OrlixKernelSession) {
        self.kernelSession = kernelSession
    }

    var latestGeometry: (rows: UInt32, columns: UInt32)? {
        geometryLock.lock()
        defer { geometryLock.unlock() }
        return geometry
    }

    func configureOutputSource(_ source: UInt32) -> Bool {
        transport.configureOutputSource(source)
    }

    func clearRecentOutput() {
        transport.clearRecentOutput()
    }

    func recentOutput() -> Data {
        transport.recentOutput()
    }

    func transportDidBecomeReady(rows: UInt32, columns: UInt32) {
        transport.resize(rows: rows, columns: columns)
    }
}

@_spi(OrlixPrivateTesting)
public final class OrlixKernelSession: @unchecked Sendable {
    public let terminal: OrlixTerminalSession
    public let bootConfig: OrlixBootConfig
    private let materializedRootImage: OrlixEnvironmentRootImage?
    private let hostDirectories: [OrlixHostDirectoryRegistration]

    public init(
        bootConfig: OrlixBootConfig,
        terminal: OrlixTerminalSession = OrlixTerminalSession()
    ) {
        self.bootConfig = bootConfig
        self.terminal = terminal
        self.materializedRootImage = nil
        self.hostDirectories = []
    }

    @_spi(OrlixPrivateTesting)
    public init(
        bootConfig: OrlixBootConfig,
        hostDirectories: [OrlixHostDirectoryRegistration],
        terminal: OrlixTerminalSession = OrlixTerminalSession()
    ) {
        self.bootConfig = bootConfig
        self.terminal = terminal
        self.materializedRootImage = nil
        self.hostDirectories = hostDirectories
    }

    public convenience init(
        environmentID: String,
        terminal: OrlixTerminalSession = OrlixTerminalSession()
    ) throws {
        try self.init(
            environmentID: environmentID,
            registry: OrlixEnvironmentRegistry(),
            terminal: terminal
        )
    }

    @_spi(OrlixPrivateTesting)
    public init(
        materializedRootImage: OrlixEnvironmentRootImage,
        terminal: OrlixTerminalSession = OrlixTerminalSession()
    ) {
        self.bootConfig = materializedRootImage.bootConfig
        self.terminal = terminal
        self.materializedRootImage = materializedRootImage
        self.hostDirectories = []
    }

    @_spi(OrlixPrivateTesting)
    public var materializedRootImageForTesting: OrlixEnvironmentRootImage? {
        materializedRootImage
    }

    public var latestBootProgress: OrlixBootProgressEvent? {
        var cEvent = COrlixBootProgressEvent(
            sequence: 0,
            monotonicNS: 0,
            stage: 0,
            status: 0,
            machKernReturn: 0,
            posixErrno: 0
        )
        guard orlix_host_boot_progress_latest(&cEvent) != 0 else {
            return nil
        }
        return OrlixBootProgressEvent(cEvent: cEvent)
    }

    public var bootProgressSnapshot: [OrlixBootProgressEvent] {
        let capacity = 64
        var cEvents = Array(
            repeating: COrlixBootProgressEvent(
                sequence: 0,
                monotonicNS: 0,
                stage: 0,
                status: 0,
                machKernReturn: 0,
                posixErrno: 0
            ),
            count: capacity
        )
        let count = cEvents.withUnsafeMutableBufferPointer { buffer in
            orlix_host_boot_progress_snapshot(buffer.baseAddress, UInt32(capacity))
        }
        return cEvents.prefix(Int(count)).map(OrlixBootProgressEvent.init(cEvent:))
    }

    public var recentConsoleOutput: Data {
		terminal.recentOutput()
    }

    public var recentConsoleOutputText: String {
        String(decoding: recentConsoleOutput, as: UTF8.self)
    }

    public var machineSnapshot: OrlixMachineSnapshot {
        let snapshot = bootProgressSnapshot
        let latest = snapshot.last
        let hasConsoleOutput = snapshot.contains { $0.stage == .firstConsoleOutput }
        let state: OrlixMachineState

        switch latest?.stage {
        case nil:
            state = .idle
        case .failed:
            state = .failed(latest)
        case .payloadRegistering:
            state = .preparing
        default:
            state = .booting
        }

        return OrlixMachineSnapshot(
            state: state,
            latestBootProgress: latest,
            hasConsoleOutput: hasConsoleOutput
        )
    }

    @_spi(OrlixPrivateTesting)
    public convenience init(
        environmentID: String,
        registry: OrlixEnvironmentRegistry,
        kernelCommandLine: String? = OrlixEnvironmentRootImage.defaultKernelCommandLine,
        terminal: OrlixTerminalSession = OrlixTerminalSession()
    ) throws {
        let rootImage = try registry.materializedRootImage(
            forEnvironmentID: environmentID,
            kernelCommandLine: kernelCommandLine
        )
        self.init(materializedRootImage: rootImage, terminal: terminal)
    }

    @_spi(OrlixPrivateTesting)
	public convenience init(
		ociRuntimeSession: OrlixOCIRuntimeSessionDescriptor,
		registry: OrlixEnvironmentRegistry,
		kernelCommandLine: String? = OrlixEnvironmentRootImage.defaultKernelCommandLine,
		terminal: OrlixTerminalSession = OrlixTerminalSession()
	) throws {
		let resolvedKernelCommandLine = try Self.ociRuntimeKernelCommandLine(
			for: ociRuntimeSession,
			kernelCommandLine: kernelCommandLine
		)
		try self.init(
			environmentID: ociRuntimeSession.environment.id,
			registry: registry,
			kernelCommandLine: resolvedKernelCommandLine,
			terminal: terminal
		)
		if let consoleSize = ociRuntimeSession.consoleSize {
			terminal.resize(rows: consoleSize.height, columns: consoleSize.width)
		}
	}

	private static func ociRuntimeKernelCommandLine(
		for session: OrlixOCIRuntimeSessionDescriptor,
		kernelCommandLine: String?
	) throws -> String? {
		guard kernelCommandLine == OrlixEnvironmentRootImage.defaultKernelCommandLine else {
			return kernelCommandLine
		}
		let base = try OrlixEnvironmentRootImage.materializedKernelCommandLine(
			descriptor: session.environment,
			kernelCommandLine: kernelCommandLine
		)
		let terminalTokens = [session.terminal ? "orlix.terminal=1" : "orlix.terminal=0"]
		let terminalToken = terminalTokens.joined(separator: " ")
		guard let base, !base.isEmpty else {
			return terminalToken
		}
		return "\(terminalToken) \(base)"
	}

	public convenience init(
		ociRuntimeBundle: OrlixOCIRuntimeBundle,
		id: String,
		terminal: OrlixTerminalSession = OrlixTerminalSession()
	) throws {
		try self.init(
			ociRuntimeBundle: ociRuntimeBundle,
			id: id,
			rootMount: .defaultOverlay,
			registry: try OrlixEnvironmentRegistry(),
			kernelCommandLine: OrlixEnvironmentRootImage.defaultKernelCommandLine,
			terminal: terminal
		)
	}

	@_spi(OrlixPrivateTesting) public convenience init(
		ociRuntimeBundle: OrlixOCIRuntimeBundle,
		id: String,
		rootMount: OrlixEnvironmentRootMount,
		registry: OrlixEnvironmentRegistry,
		kernelCommandLine: String? = OrlixEnvironmentRootImage.defaultKernelCommandLine,
		terminal: OrlixTerminalSession
	) throws {
		let ociRuntimeSession = try ociRuntimeBundle.sessionDescriptor(
			id: id,
			rootMount: rootMount
		)
		try registry.save(ociRuntimeSession.environment)
		try self.init(
			ociRuntimeSession: ociRuntimeSession,
			registry: registry,
			kernelCommandLine: kernelCommandLine,
			terminal: terminal
		)
	}

    public func boot() -> OrlixBootStatus {
		guard terminal.latestGeometry != nil else {
			return .invalidConfig
		}
		guard configureInteractiveConsole() else {
			return .invalidConfig
		}
        orlix_host_boot_progress_reset()
		terminal.clearRecentOutput()
        orlix_host_boot_progress_record(COrlixBootStage.sessionCreated, 0, 0, 0)
        orlix_host_boot_progress_record(COrlixBootStage.payloadRegistering, 0, 0, 0)

        guard registerRootImagesForBoot() else {
            orlix_host_boot_progress_record(COrlixBootStage.failed, -1, 0, 0)
            return .invalidConfig
        }

        orlix_host_boot_progress_record(COrlixBootStage.payloadRegistered, 0, 0, 0)
        return bootConfig.rootImageIdentifier.withCString { rootImageIdentifier in
            bootConfig.terminalIdentifier.withCString { terminalIdentifier in
                let boot = { (kernelCommandLine: UnsafePointer<CChar>?) in
                    var cConfig = COrlixBootConfig(
                        profile: self.bootConfig.profile.cValue,
                        kernelCommandLine: kernelCommandLine,
                        rootImageIdentifier: rootImageIdentifier,
                        terminalIdentifier: terminalIdentifier
                    )
                    orlix_host_boot_progress_record(
                        COrlixBootStage.bootloaderEntered,
                        0,
                        0,
                        0
                    )
                    let rawStatus = OrlixBoot(&cConfig)
                    let status = OrlixBootStatus(rawStatus: rawStatus)
                    switch status {
                    case .ok:
                        orlix_host_boot_progress_record(
                            COrlixBootStage.kernelHandoff,
                            rawStatus,
                            0,
                            0
                        )
                    default:
                        orlix_host_boot_progress_record(
                            COrlixBootStage.failed,
                            rawStatus,
                            0,
                            0
                        )
                    }
                    return status
                }

				guard let kernelCommandLine = bootConfig.kernelCommandLine,
                      !kernelCommandLine.isEmpty
                else {
                    return boot(nil)
                }

                return kernelCommandLine.withCString { boot($0) }
            }
        }
    }


	static func interactiveConsoleSource(kernelCommandLine: String?) -> UInt32? {
		guard let kernelCommandLine else { return nil }
		guard let token = kernelCommandLine
			.split(whereSeparator: { $0.isWhitespace })
			.last(where: { $0.hasPrefix("console=") }) else { return nil }
		let name = token.dropFirst("console=".count).split(separator: ",").first
		switch name {
		case "ttyS0": return COrlixHostConsoleSource.serial
		case "hvc0": return COrlixHostConsoleSource.virtio
		default: return nil
		}
	}

	func configureInteractiveConsole() -> Bool {
		guard let source = Self.interactiveConsoleSource(
			kernelCommandLine: bootConfig.kernelCommandLine
		) else { return false }
		guard terminal.configureOutputSource(source) else { return false }
		guard let geometry = terminal.latestGeometry else { return false }
		terminal.transportDidBecomeReady(
			rows: geometry.rows, columns: geometry.columns
		)
		return true
	}

    private func registerRootImagesForBoot() -> Bool {
        if let materializedRootImage {
            return materializedRootImage.registerWithHostAdapter()
        }
        guard OrlixOSResources.registerWithHostAdapter() else {
            return false
        }
        return OrlixOSResources.registerAdditionalHostDirectoriesForTesting(
            hostDirectories
        )
    }
}

@_spi(OrlixPrivateTesting)
public protocol OrlixOCIRuntimeProcessObservationDriver: Sendable {
	func start(processSession: OrlixOCIRuntimeProcessSession) throws -> OrlixOCIRuntimeProcessStartObservation
	func signal(processSession: OrlixOCIRuntimeProcessSession, signal: Int32) throws
	func wait(processSession: OrlixOCIRuntimeProcessSession) throws -> OrlixOCIRuntimeProcessCompletionObservation
}

@_spi(OrlixPrivateTesting)
public enum OrlixOCIRuntimeKernelSessionObservationError: Error, Equatable, Sendable {
	case bootFailed(OrlixBootStatus)
	case timedOutWaitingForStart
	case timedOutWaitingForCompletion
	case signalUnsupported
}

@_spi(OrlixPrivateTesting)
public final class OrlixOCIRuntimeKernelSessionObservationDriver:
	OrlixOCIRuntimeProcessObservationDriver,
	@unchecked Sendable
{
	private let timeout: TimeInterval
	private let bootSession: @Sendable (OrlixKernelSession) -> OrlixBootStatus
	private let condition = NSCondition()
	private var output: OrlixTerminalOutput?
	private var text = ""
	private var bootStatus: OrlixBootStatus?
	private var startObservation: OrlixOCIRuntimeProcessStartObservation?
	private var completionObservation: OrlixOCIRuntimeProcessCompletionObservation?

	public convenience init(timeout: TimeInterval = 600) {
		self.init(timeout: timeout) { session in
			session.boot()
		}
	}

	init(
		timeout: TimeInterval = 600,
		bootSession: @escaping @Sendable (OrlixKernelSession) -> OrlixBootStatus
	) {
		self.timeout = timeout
		self.bootSession = bootSession
	}

	public func start(
		processSession: OrlixOCIRuntimeProcessSession
	) throws -> OrlixOCIRuntimeProcessStartObservation {
		condition.lock()
		text = ""
		bootStatus = nil
		startObservation = nil
		completionObservation = nil
		condition.unlock()

		output = processSession.kernelSession.terminal.attachOutput { [weak self] data in
			self?.append(data)
		}

		DispatchQueue.global(qos: .userInitiated).async { [bootSession] in
			let status = bootSession(processSession.kernelSession)
			self.condition.lock()
			self.bootStatus = status
			self.condition.broadcast()
			self.condition.unlock()
		}

		return try waitForStartObservation()
	}

    public func signal(
        processSession: OrlixOCIRuntimeProcessSession,
        signal: Int32
    ) throws {
        guard processSession.processHandle.sessionDescriptor.terminal,
              let controlCharacter = Self.terminalControlCharacter(forLinuxSignal: signal)
        else {
            throw OrlixOCIRuntimeKernelSessionObservationError.signalUnsupported
        }

        processSession.kernelSession.terminal.send(Data([controlCharacter]))
    }

    private static func terminalControlCharacter(forLinuxSignal signal: Int32) -> UInt8? {
        switch signal {
        case 2:
            return 0x03
        case 3:
            return 0x1c
        case 20:
            return 0x1a
        default:
            return nil
        }
    }

	public func wait(
		processSession: OrlixOCIRuntimeProcessSession
	) throws -> OrlixOCIRuntimeProcessCompletionObservation {
		do {
			let observation = try waitForCompletionObservation()
			output?.cancel()
			output = nil
			return observation
		} catch {
			output?.cancel()
			output = nil
			throw error
		}
	}

	private func append(_ data: Data) {
		condition.lock()
		text += String(decoding: data, as: UTF8.self)
		parseObservations()
		condition.broadcast()
		condition.unlock()
	}

	private func parseObservations() {
		if startObservation == nil,
		   let match = firstMatch(
			#"orlix-init: process started pid=([0-9]+)"#
		   ),
		   let pid = Int32(String(match[1])) {
			startObservation = try? OrlixOCIRuntimeProcessStartObservation(pid: pid)
		}

		if completionObservation == nil,
		   let match = firstMatch(
			#"orlix-init: process exited pid=([0-9]+) status=([0-9]+)"#
		   ),
		   let pid = Int32(String(match[1])),
		   let status = Int32(String(match[2])),
		   let observation = try? OrlixOCIRuntimeProcessExitObservation(
			pid: pid,
			exitStatus: status
		   ) {
			completionObservation = .exited(observation)
		}

		if completionObservation == nil,
		   let match = firstMatch(
			#"orlix-init: process signaled pid=([0-9]+) signal=([0-9]+)"#
		   ),
		   let pid = Int32(String(match[1])),
		   let signal = Int32(String(match[2])),
		   let observation = try? OrlixOCIRuntimeProcessSignalObservation(
			pid: pid,
			signal: signal
		   ) {
			completionObservation = .signaled(observation)
		}
	}

	private func firstMatch(_ pattern: String) -> [Substring]? {
		guard let expression = try? NSRegularExpression(pattern: pattern) else {
			return nil
		}
		let range = NSRange(text.startIndex..<text.endIndex, in: text)
		guard let match = expression.firstMatch(in: text, range: range) else {
			return nil
		}
		return (0..<match.numberOfRanges).compactMap { index in
			guard let range = Range(match.range(at: index), in: text) else {
				return nil
			}
			return text[range]
		}
	}

	private func waitForStartObservation() throws -> OrlixOCIRuntimeProcessStartObservation {
		let deadline = Date().addingTimeInterval(timeout)
		condition.lock()
		defer { condition.unlock() }
		while startObservation == nil {
			if let bootStatus, bootStatus != .ok {
				throw OrlixOCIRuntimeKernelSessionObservationError
					.bootFailed(bootStatus)
			}
			if !condition.wait(until: deadline) {
				throw OrlixOCIRuntimeKernelSessionObservationError
					.timedOutWaitingForStart
			}
		}
		return startObservation!
	}

	private func waitForCompletionObservation() throws -> OrlixOCIRuntimeProcessCompletionObservation {
		let deadline = Date().addingTimeInterval(timeout)
		condition.lock()
		defer { condition.unlock() }
		while completionObservation == nil {
			if !condition.wait(until: deadline) {
				throw OrlixOCIRuntimeKernelSessionObservationError
					.timedOutWaitingForCompletion
			}
		}
		let drainDeadline = Date().addingTimeInterval(1)
		while Date() < drainDeadline {
			condition.wait(until: drainDeadline)
		}
		return completionObservation!
	}
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIRuntimeProcessRunResult: Sendable {
	public let startObservation: OrlixOCIRuntimeProcessStartObservation
	public let runningSession: OrlixOCIRuntimeProcessSession
	public let completionObservation: OrlixOCIRuntimeProcessCompletionObservation
	public let completedProcess: OrlixOCIRuntimeCompletedProcess
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIRuntimeCreatedEnvironment: Sendable {
	public let importPlan: OrlixOCIRuntimeBundleImportPlan
	public let lifecycleStore: OrlixOCIRuntimeLifecycleStore
	public let lifecycle: OrlixOCIRuntimeLifecycleController
	public let stateReport: OrlixOCIRuntimeStateReport

	public var environment: OrlixEnvironmentDescriptor {
		importPlan.environment
	}
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIRuntimeMaterializedCreatedEnvironment: Sendable {
	public let createdEnvironment: OrlixOCIRuntimeCreatedEnvironment
	public let materializationResult: OrlixEnvironmentImageMaterializationResult

	public var importPlan: OrlixOCIRuntimeBundleImportPlan {
		createdEnvironment.importPlan
	}

	public var lifecycleStore: OrlixOCIRuntimeLifecycleStore {
		createdEnvironment.lifecycleStore
	}

	public var lifecycle: OrlixOCIRuntimeLifecycleController {
		createdEnvironment.lifecycle
	}

	public var stateReport: OrlixOCIRuntimeStateReport {
		createdEnvironment.stateReport
	}

	public var environment: OrlixEnvironmentDescriptor {
		createdEnvironment.environment
	}
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIRuntimeDeletedEnvironment: Sendable {
	public let id: String
	public let deletedRecord: OrlixOCIRuntimeLifecycleRecord
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIRuntimeStartedEnvironment: Sendable {
	public let processSession: OrlixOCIRuntimeProcessSession
	public let stateReport: OrlixOCIRuntimeStateReport
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIRuntimeSignaledEnvironment: Sendable {
	public let processSession: OrlixOCIRuntimeProcessSession
	public let signal: Int32
	public let stateReport: OrlixOCIRuntimeStateReport
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIRuntimeCompletedEnvironment: Sendable {
	public let completedProcess: OrlixOCIRuntimeCompletedProcess
	public let stateReport: OrlixOCIRuntimeStateReport
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIRuntimeRunResult: Sendable {
	public let startedEnvironment: OrlixOCIRuntimeStartedEnvironment
	public let completedEnvironment: OrlixOCIRuntimeCompletedEnvironment
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIRuntimeMaterializedRunResult: Sendable {
	public let createdEnvironment: OrlixOCIRuntimeMaterializedCreatedEnvironment
	public let runResult: OrlixOCIRuntimeRunResult

	public var startedEnvironment: OrlixOCIRuntimeStartedEnvironment {
		runResult.startedEnvironment
	}

	public var completedEnvironment: OrlixOCIRuntimeCompletedEnvironment {
		runResult.completedEnvironment
	}
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIRuntimeEphemeralRunResult: Sendable {
	public let materializedRunResult: OrlixOCIRuntimeMaterializedRunResult
	public let deletedEnvironment: OrlixOCIRuntimeDeletedEnvironment

	public var createdEnvironment: OrlixOCIRuntimeMaterializedCreatedEnvironment {
		materializedRunResult.createdEnvironment
	}

	public var startedEnvironment: OrlixOCIRuntimeStartedEnvironment {
		materializedRunResult.startedEnvironment
	}

	public var completedEnvironment: OrlixOCIRuntimeCompletedEnvironment {
		materializedRunResult.completedEnvironment
	}
}

@_spi(OrlixPrivateTesting)
public enum OrlixOCIRuntimeError: Error, Equatable, Sendable {
	case environmentAlreadyExists(String)
	case missingMaterializedRootImage(String)
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIRuntimeEphemeralRunFailure: Error {
	public let id: String
	public let originalError: Error
	public let cleanupError: Error?
	public let deletedEnvironment: OrlixOCIRuntimeDeletedEnvironment?
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIEnvironmentMaterializationTools: Equatable, Sendable {
    public let mke2fs: URL
    public let truncate: URL
    public let debugfs: URL
    public let e2fsck: URL

    public init(mke2fs: URL, truncate: URL, debugfs: URL, e2fsck: URL) {
        self.mke2fs = mke2fs
        self.truncate = truncate
        self.debugfs = debugfs
        self.e2fsck = e2fsck
    }
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIEnvironmentInstallResult: Sendable {
	public let id: String
	public let bundleURL: URL
	public let stateReport: OrlixOCIRuntimeStateReport
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIRegistryRootfsImportReport: Sendable {
	public let stagingRootDirectory: URL
	public let baseTreeDirectory: URL
	public let layerDigests: [String]
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIRegistryEnvironmentInstallResult: Sendable {
	public let id: String
	public let image: OrlixOCIRegistryImageReference
	public let pullResult: OrlixOCIRegistryPullResult
	public let rootfsImport: OrlixOCIRegistryRootfsImportReport
	public let stateReport: OrlixOCIRuntimeStateReport
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIEnvironmentRunResult: Sendable {
    public let id: String
    public let startedStateReport: OrlixOCIRuntimeStateReport
    public let completedStateReport: OrlixOCIRuntimeStateReport
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIEnvironmentExecResult: Sendable {
    public let id: String
    public let command: [String]
    public let runResult: OrlixOCIEnvironmentRunResult
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIEnvironmentHealthcheckResult: Sendable {
    public let id: String
    public let command: [String]
    public let runResult: OrlixOCIEnvironmentRunResult
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIEnvironmentStartResult: Sendable {
	public let id: String
	public let stateReport: OrlixOCIRuntimeStateReport
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIEnvironmentWaitResult: Sendable {
    public let id: String
    public let stateReport: OrlixOCIRuntimeStateReport
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIEnvironmentSignalResult: Sendable {
    public let id: String
    public let signal: Int32
    public let stateReport: OrlixOCIRuntimeStateReport
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIEnvironmentInstallRunResult: Sendable {
    public let installResult: OrlixOCIEnvironmentInstallResult
    public let runResult: OrlixOCIEnvironmentRunResult
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIRegistryEnvironmentInstallRunResult: Sendable {
	public let installResult: OrlixOCIRegistryEnvironmentInstallResult
	public let runResult: OrlixOCIEnvironmentRunResult
	public let deleteResult: OrlixOCIEnvironmentDeleteResult?
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIRegistryEnvironmentTerminalSessionResult: Sendable {
	public let installResult: OrlixOCIRegistryEnvironmentInstallResult
	let kernelSession: OrlixKernelSession
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIEnvironmentTerminalSessionResult: Sendable {
    public let id: String
    public let image: OrlixOCIRegistryImageReference
    public let command: [String]?
    let kernelSession: OrlixKernelSession
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIEnvironmentPreparedState: Sendable {
    public let id: String
    public let platform: String
    public let defaultCommand: [String]
    public let lifecycleState: OrlixOCIRuntimeLifecycleState
    public let stateReport: OrlixOCIRuntimeStateReport?
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIEnvironmentInspectResult: Sendable {
    public let id: String
    public let platform: String
    public let rootImageIdentifier: String
    public let defaultCommand: [String]
    public let defaultEnvironment: [String: String]
    public let defaultWorkingDirectory: String
    public let defaultUserID: UInt32
    public let defaultGroupID: UInt32
    public let hostname: String?
    public let domainname: String?
    public let annotations: [String: String]
    public let lifecycleState: OrlixOCIRuntimeLifecycleState
    public let stateReport: OrlixOCIRuntimeStateReport
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIEnvironmentDeleteResult: Sendable {
    public let id: String
    public let lifecycleState: OrlixOCIRuntimeLifecycleState
}

@_spi(OrlixPrivateTesting)
public enum OrlixOCIEnvironmentListArgumentsError: Error, Equatable, Sendable {
	case missingListCommand
	case missingOptionValue(String)
	case unknownOption(String)
	case unexpectedArgument(String)
	case invalidState(String)
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIEnvironmentListArguments: Equatable, Sendable {
	public let states: [OrlixOCIRuntimeLifecycleState]

	public init(_ arguments: [String]) throws {
		var values = arguments
		if values.first == "orlix" {
			values.removeFirst()
		}
		guard values.first == "list" || values.first == "ps" else {
			throw OrlixOCIEnvironmentListArgumentsError.missingListCommand
		}
		values.removeFirst()

		var parsedStates: [OrlixOCIRuntimeLifecycleState] = []
		while !values.isEmpty {
			let value = values.removeFirst()
			if value == "--state" || value == "--status" {
				guard let state = values.first else {
					throw OrlixOCIEnvironmentListArgumentsError
						.missingOptionValue(value)
				}
				try Self.addState(state, to: &parsedStates)
				values.removeFirst()
				continue
			}
			if value.hasPrefix("--state=") || value.hasPrefix("--status=") {
				let separator = value.firstIndex(of: "=")!
				let option = String(value[..<separator])
				let state = String(value[value.index(after: separator)...])
				guard !state.isEmpty else {
					throw OrlixOCIEnvironmentListArgumentsError
						.missingOptionValue(option)
				}
				try Self.addState(state, to: &parsedStates)
				continue
			}
			if value.hasPrefix("-") {
				throw OrlixOCIEnvironmentListArgumentsError.unknownOption(value)
			}
			throw OrlixOCIEnvironmentListArgumentsError.unexpectedArgument(value)
		}

		self.states = parsedStates
	}

	private static func addState(
		_ value: String,
		to states: inout [OrlixOCIRuntimeLifecycleState]
	) throws {
		guard let state = OrlixOCIRuntimeLifecycleState(rawValue: value) else {
			throw OrlixOCIEnvironmentListArgumentsError.invalidState(value)
		}
		if !states.contains(state) {
			states.append(state)
		}
	}
}

@_spi(OrlixPrivateTesting)
public enum OrlixOCIEnvironmentStateArgumentsError: Error, Equatable, Sendable {
	case missingStateCommand
	case missingID
	case missingOptionValue(String)
	case unknownOption(String)
	case unexpectedArgument(String)
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIEnvironmentStateArguments: Equatable, Sendable {
	public let id: String

	public init(_ arguments: [String]) throws {
		var values = arguments
		if values.first == "orlix" {
			values.removeFirst()
		}
		guard values.first == "state" else {
			throw OrlixOCIEnvironmentStateArgumentsError.missingStateCommand
		}
		values.removeFirst()

		var parsedID: String?
		while !values.isEmpty {
			let value = values.removeFirst()
			if value == "--id" || value == "--name" {
				guard let id = values.first else {
					throw OrlixOCIEnvironmentStateArgumentsError
						.missingOptionValue(value)
				}
				parsedID = try Self.acceptID(id, current: parsedID)
				values.removeFirst()
				continue
			}
			if value.hasPrefix("--id=") || value.hasPrefix("--name=") {
				let separator = value.firstIndex(of: "=")!
				let option = String(value[..<separator])
				let id = String(value[value.index(after: separator)...])
				guard !id.isEmpty else {
					throw OrlixOCIEnvironmentStateArgumentsError
						.missingOptionValue(option)
				}
				parsedID = try Self.acceptID(id, current: parsedID)
				continue
			}
			if value.hasPrefix("-") {
				throw OrlixOCIEnvironmentStateArgumentsError.unknownOption(value)
			}
			parsedID = try Self.acceptID(value, current: parsedID)
		}

		guard let id = parsedID else {
			throw OrlixOCIEnvironmentStateArgumentsError.missingID
		}
		try OrlixEnvironmentStorageLayout.validateEnvironmentID(id)
		self.id = id
	}

	private static func acceptID(_ id: String, current: String?) throws -> String {
		guard current == nil else {
			throw OrlixOCIEnvironmentStateArgumentsError.unexpectedArgument(id)
		}
		return id
	}
}

@_spi(OrlixPrivateTesting)
public enum OrlixOCIEnvironmentLifecycleArgumentsError: Error, Equatable, Sendable {
	case missingCommand(String)
	case missingID
	case missingOptionValue(String)
	case unknownOption(String)
	case unexpectedArgument(String)
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIEnvironmentLifecycleArguments: Equatable, Sendable {
    public let id: String

    public init(_ arguments: [String], command: String) throws {
        var values = arguments
		if values.first == "orlix" {
			values.removeFirst()
		}
		guard values.first == command else {
			throw OrlixOCIEnvironmentLifecycleArgumentsError
				.missingCommand(command)
		}
		values.removeFirst()

		var parsedID: String?
		while !values.isEmpty {
			let value = values.removeFirst()
			if value == "--id" || value == "--name" {
				guard let id = values.first else {
					throw OrlixOCIEnvironmentLifecycleArgumentsError
						.missingOptionValue(value)
				}
				parsedID = try Self.acceptID(id, current: parsedID)
				values.removeFirst()
				continue
			}
			if value.hasPrefix("--id=") || value.hasPrefix("--name=") {
				let separator = value.firstIndex(of: "=")!
				let option = String(value[..<separator])
				let id = String(value[value.index(after: separator)...])
				guard !id.isEmpty else {
					throw OrlixOCIEnvironmentLifecycleArgumentsError
						.missingOptionValue(option)
				}
				parsedID = try Self.acceptID(id, current: parsedID)
				continue
			}
			if value.hasPrefix("-") {
				throw OrlixOCIEnvironmentLifecycleArgumentsError.unknownOption(value)
			}
			parsedID = try Self.acceptID(value, current: parsedID)
		}

		guard let id = parsedID else {
			throw OrlixOCIEnvironmentLifecycleArgumentsError.missingID
		}
		try OrlixEnvironmentStorageLayout.validateEnvironmentID(id)
		self.id = id
	}

	private static func acceptID(_ id: String, current: String?) throws -> String {
		guard current == nil else {
			throw OrlixOCIEnvironmentLifecycleArgumentsError
				.unexpectedArgument(id)
		}
		return id
    }
}

@_spi(OrlixPrivateTesting)
public enum OrlixOCIEnvironmentExecArgumentsError: Error, Equatable, Sendable {
    case missingExecCommand
    case missingID
    case missingCommand
    case missingOptionValue(String)
    case unknownOption(String)
    case unexpectedArgument(String)
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIEnvironmentExecArguments: Equatable, Sendable {
    public let id: String
    public let command: [String]

    public init(_ arguments: [String]) throws {
        var values = arguments
        if values.first == "orlix" {
            values.removeFirst()
        }
        guard values.first == "exec" else {
            throw OrlixOCIEnvironmentExecArgumentsError.missingExecCommand
        }
        values.removeFirst()

        var parsedID: String?
        var parsedCommand: [String] = []
        while !values.isEmpty {
            let value = values.removeFirst()
            if value == "--" {
                parsedCommand = values
                values.removeAll()
                break
            }
            if parsedID == nil, value == "--id" || value == "--name" {
                guard let id = values.first else {
                    throw OrlixOCIEnvironmentExecArgumentsError
                        .missingOptionValue(value)
                }
                parsedID = try Self.acceptID(id, current: parsedID)
                values.removeFirst()
                continue
            }
            if parsedID == nil,
                value.hasPrefix("--id=") || value.hasPrefix("--name=")
            {
                let separator = value.firstIndex(of: "=")!
                let option = String(value[..<separator])
                let id = String(value[value.index(after: separator)...])
                guard !id.isEmpty else {
                    throw OrlixOCIEnvironmentExecArgumentsError
                        .missingOptionValue(option)
                }
                parsedID = try Self.acceptID(id, current: parsedID)
                continue
            }
            if parsedID == nil {
                if value.hasPrefix("-") {
                    throw OrlixOCIEnvironmentExecArgumentsError
                        .unknownOption(value)
                }
                parsedID = value
                continue
            }
            parsedCommand = [value] + values
            values.removeAll()
        }

        guard let id = parsedID else {
            throw OrlixOCIEnvironmentExecArgumentsError.missingID
        }
        guard !parsedCommand.isEmpty else {
            throw OrlixOCIEnvironmentExecArgumentsError.missingCommand
        }
        try OrlixEnvironmentStorageLayout.validateEnvironmentID(id)
        self.id = id
        self.command = parsedCommand
    }

    private static func acceptID(_ id: String, current: String?) throws -> String {
        guard current == nil else {
            throw OrlixOCIEnvironmentExecArgumentsError.unexpectedArgument(id)
        }
        return id
    }
}

@_spi(OrlixPrivateTesting)
public enum OrlixOCIEnvironmentKillArgumentsError: Error, Equatable, Sendable {
    case missingKillCommand
    case missingID
    case missingOptionValue(String)
	case unknownOption(String)
	case unexpectedArgument(String)
	case invalidSignal(String)
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIEnvironmentKillArguments: Equatable, Sendable {
	public let id: String
	public let signal: Int32
	public let signalSpecified: Bool

	public init(_ arguments: [String]) throws {
		var values = arguments
		if values.first == "orlix" {
			values.removeFirst()
		}
		guard values.first == "kill" else {
			throw OrlixOCIEnvironmentKillArgumentsError.missingKillCommand
		}
		values.removeFirst()

		var parsedID: String?
		var parsedSignal: Int32 = 15
		var parsedSignalPresent = false
		while !values.isEmpty {
			let value = values.removeFirst()
			if value == "--id" || value == "--name" {
				guard let id = values.first else {
					throw OrlixOCIEnvironmentKillArgumentsError
						.missingOptionValue(value)
				}
				parsedID = try Self.acceptID(id, current: parsedID)
				values.removeFirst()
				continue
			}
			if value.hasPrefix("--id=") || value.hasPrefix("--name=") {
				let separator = value.firstIndex(of: "=")!
				let option = String(value[..<separator])
				let id = String(value[value.index(after: separator)...])
				guard !id.isEmpty else {
					throw OrlixOCIEnvironmentKillArgumentsError
						.missingOptionValue(option)
				}
				parsedID = try Self.acceptID(id, current: parsedID)
				continue
			}
			if value == "--signal" || value == "-s" {
				guard let signal = values.first else {
					throw OrlixOCIEnvironmentKillArgumentsError
						.missingOptionValue(value)
				}
				parsedSignal = try Self.parseSignal(signal)
				parsedSignalPresent = true
				values.removeFirst()
				continue
			}
			if value.hasPrefix("--signal=") {
				let separator = value.firstIndex(of: "=")!
				let signal = String(value[value.index(after: separator)...])
				guard !signal.isEmpty else {
					throw OrlixOCIEnvironmentKillArgumentsError
						.missingOptionValue("--signal")
				}
				parsedSignal = try Self.parseSignal(signal)
				parsedSignalPresent = true
				continue
			}
			if value.hasPrefix("-") {
				throw OrlixOCIEnvironmentKillArgumentsError.unknownOption(value)
			}
			if parsedID == nil {
				parsedID = value
			} else if !parsedSignalPresent {
				parsedSignal = try Self.parseSignal(value)
				parsedSignalPresent = true
			} else {
				throw OrlixOCIEnvironmentKillArgumentsError
					.unexpectedArgument(value)
			}
		}

		guard let id = parsedID else {
			throw OrlixOCIEnvironmentKillArgumentsError.missingID
		}
		try OrlixEnvironmentStorageLayout.validateEnvironmentID(id)
		self.id = id
		self.signal = parsedSignal
		self.signalSpecified = parsedSignalPresent
	}

	private static func acceptID(_ id: String, current: String?) throws -> String {
		guard current == nil else {
			throw OrlixOCIEnvironmentKillArgumentsError.unexpectedArgument(id)
		}
		return id
	}

	private static func parseSignal(_ value: String) throws -> Int32 {
		if let signal = OrlixLinuxSignal.number(value) {
			return signal
		}
		throw OrlixOCIEnvironmentKillArgumentsError.invalidSignal(value)
	}
}

@_spi(OrlixPrivateTesting)
public enum OrlixOCIEnvironmentRunArgumentsError: Error, Equatable, Sendable {
	case missingRunCommand
	case missingImage
	case missingOptionValue(String)
	case unknownOption(String)
	case removeAfterRunRequiresObservedRun
}

@_spi(OrlixPrivateTesting)
public enum OrlixOCIEnvironmentHealthcheckError: Error, Equatable, Sendable {
	case missingHealthcheck(String)
	case disabledHealthcheck(String)
	case invalidHealthcheckTest(String, [String])
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIEnvironmentRunArguments: Equatable, Sendable {
    public let image: String
    public let id: String
	public let platform: String
	public let entrypoint: [String]?
	public let environment: [String: String]
	public let workingDirectory: String?
	public let userID: UInt32?
	public let groupID: UInt32?
	public let supplementaryGroupIDs: [UInt32]
	public let capabilities: OrlixEnvironmentCapabilities?
    public let hostname: String?
    public let domainname: String?
    public let terminal: Bool?
    public let terminalRows: UInt32?
    public let terminalColumns: UInt32?
    public let rootReadonly: Bool?
    public let rootPropagation: OrlixEnvironmentRootPropagation?
    public let rlimits: [OrlixEnvironmentRlimit]
    public let sysctls: [String: String]
    public let maskedPaths: [String]
    public let readonlyPaths: [String]
    public let umask: UInt32?
	public let oomScoreAdjustment: Int32?
	public let scheduler: OrlixEnvironmentScheduler?
	public let ioPriority: OrlixEnvironmentIOPriority?
	public let cpuAffinity: OrlixEnvironmentCPUAffinity?
	public let personalityDomain: String?
	public let noNewPrivileges: Bool?
	public let closeAdditionalFds: Bool?
	public let cgroupsPath: String?
	public let cgroupPidsLimit: Int64?
	public let cgroupCPUMax: OrlixEnvironmentCgroupCPUMax?
    public let cgroupCPUWeight: UInt64?
    public let cgroupMemoryMax: Int64?
    public let cgroupIOWeight: UInt64?
	public let cgroupUnified: [OrlixEnvironmentCgroupUnifiedEntry]
	public let tmpfsMounts: [OrlixEnvironmentTmpfsMount]
	public let mounts: [OrlixEnvironmentMount]
	public let publishedPorts: [OrlixEnvironmentPublishedPort]
	public let deviceNodes: [OrlixEnvironmentDeviceNode]
    public let namespaces: [String]
    public let namespacePaths: [String: String]
	public let timeOffsets: [OrlixEnvironmentTimeOffset]
	public let uidMappings: [OrlixEnvironmentIDMapping]
	public let gidMappings: [OrlixEnvironmentIDMapping]
	public let annotations: [String: String]
	public let command: [String]?
	public let removeAfterRun: Bool

    var resolvedCommandOverride: [String]? {
        guard let entrypoint else {
            return command
        }
        return entrypoint + (command ?? [])
    }

    public init(_ arguments: [String]) throws {
		var values = arguments
		if values.first == "orlix" {
			values.removeFirst()
		}
		guard values.first == "run" else {
			throw OrlixOCIEnvironmentRunArgumentsError.missingRunCommand
		}
		values.removeFirst()

		var parsedID: String?
		var parsedPlatform = "linux/arm64"
		var parsedEntrypoint: [String]?
		var parsedEnvironment: [String: String] = [:]
		var parsedWorkingDirectory: String?
		var parsedUserID: UInt32?
		var parsedGroupID: UInt32?
		var parsedSupplementaryGroupIDs: [UInt32] = []
		var parsedCapabilities = OrlixEnvironmentCapabilities()
		var parsedCapabilitiesPresent = false
        var parsedHostname: String?
        var parsedDomainname: String?
        var parsedTerminal: Bool?
        var parsedTerminalRows: UInt32?
        var parsedTerminalColumns: UInt32?
        var parsedRootReadonly: Bool?
        var parsedRootPropagation: OrlixEnvironmentRootPropagation?
        var parsedRlimits: [OrlixEnvironmentRlimit] = []
        var parsedSysctls: [String: String] = [:]
        var parsedMaskedPaths: [String] = []
        var parsedReadonlyPaths: [String] = []
        var parsedUmask: UInt32?
		var parsedOOMScoreAdjustment: Int32?
		var parsedScheduler: OrlixEnvironmentScheduler?
		var parsedIOPriority: OrlixEnvironmentIOPriority?
		var parsedCPUAffinity: OrlixEnvironmentCPUAffinity?
		var parsedPersonalityDomain: String?
		var parsedNoNewPrivileges: Bool?
		var parsedCloseAdditionalFds: Bool?
		var parsedCgroupsPath: String?
		var parsedCgroupPidsLimit: Int64?
		var parsedCgroupCPUMax: OrlixEnvironmentCgroupCPUMax?
        var parsedCgroupCPUWeight: UInt64?
        var parsedCgroupMemoryMax: Int64?
        var parsedCgroupIOWeight: UInt64?
	var parsedCgroupUnified: [OrlixEnvironmentCgroupUnifiedEntry] = []
	var parsedTmpfsMounts: [OrlixEnvironmentTmpfsMount] = []
	var parsedMounts: [OrlixEnvironmentMount] = []
	var parsedPublishedPorts: [OrlixEnvironmentPublishedPort] = []
	var parsedDeviceNodes: [OrlixEnvironmentDeviceNode] = []
        var parsedNamespaces: [String] = []
        var parsedNamespacePaths: [String: String] = [:]
		var parsedTimeOffsets: [OrlixEnvironmentTimeOffset] = []
		var parsedUIDMappings: [OrlixEnvironmentIDMapping] = []
		var parsedGIDMappings: [OrlixEnvironmentIDMapping] = []
		var parsedAnnotations: [String: String] = [:]
		var parsedImage: String?
		var parsedCommand: [String] = []
		var parsedRemoveAfterRun = false

		while !values.isEmpty {
			let value = values.removeFirst()
			if value == "--" {
				parsedCommand = values
				values.removeAll()
				break
			}
			if parsedImage == nil, value == "--rm" {
				parsedRemoveAfterRun = true
				continue
			}
			if parsedImage == nil, value == "--tty" || value == "-t" {
				parsedTerminal = true
				continue
			}
            if parsedImage == nil,
               value == "--no-tty" || value == "--no-terminal" {
            parsedTerminal = false
            continue
        }
        if parsedImage == nil, value == "--terminal-size" {
            guard let terminalSize = values.first else {
                throw OrlixOCIEnvironmentRunArgumentsError
                    .missingOptionValue(value)
            }
            (parsedTerminalRows, parsedTerminalColumns) =
                try Self.parseTerminalSize(terminalSize)
            values.removeFirst()
            continue
        }
        if parsedImage == nil, value.hasPrefix("--terminal-size=") {
            let separator = value.firstIndex(of: "=")!
            let terminalSize = String(value[value.index(after: separator)...])
            guard !terminalSize.isEmpty else {
                throw OrlixOCIEnvironmentRunArgumentsError
                    .missingOptionValue("--terminal-size")
            }
            (parsedTerminalRows, parsedTerminalColumns) =
                try Self.parseTerminalSize(terminalSize)
            continue
        }
        if parsedImage == nil, value == "--read-only" {
            parsedRootReadonly = true
            continue
            }
            if parsedImage == nil, value == "--read-write" {
                parsedRootReadonly = false
                continue
            }
            if parsedImage == nil, value == "--root-propagation" {
                guard let rootPropagation = values.first else {
                    throw OrlixOCIEnvironmentRunArgumentsError
                        .missingOptionValue(value)
                }
                parsedRootPropagation = try Self.parseRootPropagation(
                    rootPropagation
                )
                values.removeFirst()
                continue
            }
            if parsedImage == nil, value.hasPrefix("--root-propagation=") {
                let separator = value.firstIndex(of: "=")!
                let rootPropagation = String(value[value.index(after: separator)...])
                guard !rootPropagation.isEmpty else {
                    throw OrlixOCIEnvironmentRunArgumentsError
                        .missingOptionValue("--root-propagation")
                }
                parsedRootPropagation = try Self.parseRootPropagation(
                    rootPropagation
                )
                continue
            }
            if parsedImage == nil, value == "--no-new-privileges" {
                parsedNoNewPrivileges = true
                continue
			}
			if parsedImage == nil, value == "--close-fds" {
				parsedCloseAdditionalFds = true
				continue
			}
			if parsedImage == nil, value == "--cgroups-path" {
				guard let cgroupsPath = values.first else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue(value)
				}
				parsedCgroupsPath = try Self.parseCgroupsPath(cgroupsPath)
				values.removeFirst()
				continue
			}
			if parsedImage == nil, value.hasPrefix("--cgroups-path=") {
				let separator = value.firstIndex(of: "=")!
				let cgroupsPath = String(value[value.index(after: separator)...])
				guard !cgroupsPath.isEmpty else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue("--cgroups-path")
				}
				parsedCgroupsPath = try Self.parseCgroupsPath(cgroupsPath)
				continue
			}
			if parsedImage == nil, value == "--pids-limit" {
				guard let pidsLimit = values.first else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue(value)
				}
				parsedCgroupPidsLimit = try Self.parseCgroupPidsLimit(pidsLimit)
				values.removeFirst()
				continue
			}
			if parsedImage == nil, value.hasPrefix("--pids-limit=") {
				let separator = value.firstIndex(of: "=")!
				let pidsLimit = String(value[value.index(after: separator)...])
				guard !pidsLimit.isEmpty else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue("--pids-limit")
				}
				parsedCgroupPidsLimit = try Self.parseCgroupPidsLimit(pidsLimit)
				continue
			}
			if parsedImage == nil, value == "--cpu-max" {
				guard let cpuMax = values.first else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue(value)
				}
				parsedCgroupCPUMax = try Self.parseCgroupCPUMax(cpuMax)
				values.removeFirst()
				continue
			}
			if parsedImage == nil, value.hasPrefix("--cpu-max=") {
				let separator = value.firstIndex(of: "=")!
				let cpuMax = String(value[value.index(after: separator)...])
				guard !cpuMax.isEmpty else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue("--cpu-max")
				}
				parsedCgroupCPUMax = try Self.parseCgroupCPUMax(cpuMax)
				continue
			}
			if parsedImage == nil, value == "--cpu-weight" {
				guard let cpuWeight = values.first else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue(value)
				}
				parsedCgroupCPUWeight = try Self.parseCgroupCPUWeight(cpuWeight)
				values.removeFirst()
				continue
			}
			if parsedImage == nil, value.hasPrefix("--cpu-weight=") {
				let separator = value.firstIndex(of: "=")!
				let cpuWeight = String(value[value.index(after: separator)...])
				guard !cpuWeight.isEmpty else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue("--cpu-weight")
				}
				parsedCgroupCPUWeight = try Self.parseCgroupCPUWeight(cpuWeight)
				continue
			}
			if parsedImage == nil, value == "--memory-max" {
				guard let memoryMax = values.first else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue(value)
				}
				parsedCgroupMemoryMax = try Self.parseCgroupMemoryMax(memoryMax)
				values.removeFirst()
				continue
			}
			if parsedImage == nil, value.hasPrefix("--memory-max=") {
				let separator = value.firstIndex(of: "=")!
				let memoryMax = String(value[value.index(after: separator)...])
				guard !memoryMax.isEmpty else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue("--memory-max")
				}
				parsedCgroupMemoryMax = try Self.parseCgroupMemoryMax(memoryMax)
				continue
			}
			if parsedImage == nil, value == "--io-weight" {
				guard let ioWeight = values.first else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue(value)
				}
				parsedCgroupIOWeight = try Self.parseCgroupIOWeight(ioWeight)
				values.removeFirst()
				continue
			}
            if parsedImage == nil, value.hasPrefix("--io-weight=") {
                let separator = value.firstIndex(of: "=")!
                let ioWeight = String(value[value.index(after: separator)...])
                guard !ioWeight.isEmpty else {
                    throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue("--io-weight")
				}
                parsedCgroupIOWeight = try Self.parseCgroupIOWeight(ioWeight)
                continue
            }
            if parsedImage == nil, value == "--cgroup-unified" {
                guard let unified = values.first else {
                    throw OrlixOCIEnvironmentRunArgumentsError
                        .missingOptionValue(value)
                }
                try Self.addCgroupUnifiedEntry(
                    unified,
                    to: &parsedCgroupUnified
                )
                values.removeFirst()
                continue
            }
            if parsedImage == nil, value.hasPrefix("--cgroup-unified=") {
                let separator = value.firstIndex(of: "=")!
                let unified = String(value[value.index(after: separator)...])
                guard !unified.isEmpty else {
                    throw OrlixOCIEnvironmentRunArgumentsError
                        .missingOptionValue("--cgroup-unified")
                }
                try Self.addCgroupUnifiedEntry(
                    unified,
                    to: &parsedCgroupUnified
                )
                continue
            }
            if parsedImage == nil, value == "--tmpfs" {
                guard let tmpfs = values.first else {
                    throw OrlixOCIEnvironmentRunArgumentsError
                        .missingOptionValue(value)
                }
                try Self.addTmpfsMount(tmpfs, to: &parsedTmpfsMounts)
                values.removeFirst()
                continue
            }
            if parsedImage == nil, value.hasPrefix("--tmpfs=") {
                let separator = value.firstIndex(of: "=")!
                let tmpfs = String(value[value.index(after: separator)...])
            guard !tmpfs.isEmpty else {
                throw OrlixOCIEnvironmentRunArgumentsError
                        .missingOptionValue("--tmpfs")
                }
                try Self.addTmpfsMount(tmpfs, to: &parsedTmpfsMounts)
                continue
            }
            if parsedImage == nil, value == "--mount" {
                guard let mount = values.first else {
                    throw OrlixOCIEnvironmentRunArgumentsError
                        .missingOptionValue(value)
                }
                try Self.addMount(mount, to: &parsedMounts)
                values.removeFirst()
                continue
            }
            if parsedImage == nil, value.hasPrefix("--mount=") {
                let separator = value.firstIndex(of: "=")!
                let mount = String(value[value.index(after: separator)...])
                guard !mount.isEmpty else {
                    throw OrlixOCIEnvironmentRunArgumentsError
                        .missingOptionValue("--mount")
                }
		try Self.addMount(mount, to: &parsedMounts)
		continue
	}
	if parsedImage == nil, value == "--volume" || value == "-v" {
		guard let volume = values.first else {
			throw OrlixOCIEnvironmentRunArgumentsError
				.missingOptionValue(value)
		}
		try Self.addVolume(volume, to: &parsedMounts)
		values.removeFirst()
		continue
	}
			if parsedImage == nil, value.hasPrefix("--volume=") {
				let separator = value.firstIndex(of: "=")!
				let volume = String(value[value.index(after: separator)...])
				guard !volume.isEmpty else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue("--volume")
				}
				try Self.addVolume(volume, to: &parsedMounts)
				continue
			}
			if parsedImage == nil, value == "--publish" || value == "-p" {
				guard let publish = values.first else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue(value)
				}
				try Self.addPublishedPort(publish, to: &parsedPublishedPorts)
				values.removeFirst()
				continue
			}
			if parsedImage == nil, value.hasPrefix("--publish=") {
				let separator = value.firstIndex(of: "=")!
				let publish = String(value[value.index(after: separator)...])
				guard !publish.isEmpty else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue("--publish")
				}
				try Self.addPublishedPort(publish, to: &parsedPublishedPorts)
				continue
			}
			if parsedImage == nil, value == "--device-node" {
		guard let deviceNode = values.first else {
                    throw OrlixOCIEnvironmentRunArgumentsError
                    .missingOptionValue(value)
            }
            try Self.addDeviceNode(deviceNode, to: &parsedDeviceNodes)
            values.removeFirst()
            continue
        }
        if parsedImage == nil, value.hasPrefix("--device-node=") {
            let separator = value.firstIndex(of: "=")!
            let deviceNode = String(value[value.index(after: separator)...])
            guard !deviceNode.isEmpty else {
                throw OrlixOCIEnvironmentRunArgumentsError
                    .missingOptionValue("--device-node")
            }
            try Self.addDeviceNode(deviceNode, to: &parsedDeviceNodes)
            continue
        }
        if parsedImage == nil, value == "--namespace" {
            guard let namespace = values.first else {
                throw OrlixOCIEnvironmentRunArgumentsError
                    .missingOptionValue(value)
            }
            try Self.addNamespace(
                namespace,
                to: &parsedNamespaces,
                namespacePaths: parsedNamespacePaths
            )
            values.removeFirst()
            continue
        }
        if parsedImage == nil, value.hasPrefix("--namespace=") {
            let separator = value.firstIndex(of: "=")!
            let namespace = String(value[value.index(after: separator)...])
            guard !namespace.isEmpty else {
                throw OrlixOCIEnvironmentRunArgumentsError
                    .missingOptionValue("--namespace")
            }
            try Self.addNamespace(
                namespace,
                to: &parsedNamespaces,
                namespacePaths: parsedNamespacePaths
            )
            continue
        }
        if parsedImage == nil, value == "--namespace-path" {
            guard let namespacePath = values.first else {
                throw OrlixOCIEnvironmentRunArgumentsError
                    .missingOptionValue(value)
            }
            try Self.addNamespacePath(
                namespacePath,
                to: &parsedNamespacePaths,
                namespaces: parsedNamespaces
            )
            values.removeFirst()
            continue
        }
        if parsedImage == nil, value.hasPrefix("--namespace-path=") {
            let separator = value.firstIndex(of: "=")!
            let namespacePath = String(value[value.index(after: separator)...])
            guard !namespacePath.isEmpty else {
                throw OrlixOCIEnvironmentRunArgumentsError
                    .missingOptionValue("--namespace-path")
            }
            try Self.addNamespacePath(
                namespacePath,
                to: &parsedNamespacePaths,
                namespaces: parsedNamespaces
            )
            continue
        }
        if parsedImage == nil, value == "--time-offset" {
            guard let offset = values.first else {
                throw OrlixOCIEnvironmentRunArgumentsError
                    .missingOptionValue(value)
            }
            try Self.addTimeOffset(offset, to: &parsedTimeOffsets)
            values.removeFirst()
            continue
        }
        if parsedImage == nil, value.hasPrefix("--time-offset=") {
            let separator = value.firstIndex(of: "=")!
            let offset = String(value[value.index(after: separator)...])
            guard !offset.isEmpty else {
                throw OrlixOCIEnvironmentRunArgumentsError
                    .missingOptionValue("--time-offset")
            }
            try Self.addTimeOffset(offset, to: &parsedTimeOffsets)
            continue
        }
        if parsedImage == nil, value == "--uid-map" {
            guard let mapping = values.first else {
                throw OrlixOCIEnvironmentRunArgumentsError
                    .missingOptionValue(value)
            }
            try Self.addIDMapping(
                mapping,
                to: &parsedUIDMappings,
                feature: "linux.uidMappings"
            )
            values.removeFirst()
            continue
        }
        if parsedImage == nil, value.hasPrefix("--uid-map=") {
            let separator = value.firstIndex(of: "=")!
            let mapping = String(value[value.index(after: separator)...])
            guard !mapping.isEmpty else {
                throw OrlixOCIEnvironmentRunArgumentsError
                    .missingOptionValue("--uid-map")
            }
            try Self.addIDMapping(
                mapping,
                to: &parsedUIDMappings,
                feature: "linux.uidMappings"
            )
            continue
        }
        if parsedImage == nil, value == "--gid-map" {
            guard let mapping = values.first else {
                throw OrlixOCIEnvironmentRunArgumentsError
                    .missingOptionValue(value)
            }
            try Self.addIDMapping(
                mapping,
                to: &parsedGIDMappings,
                feature: "linux.gidMappings"
            )
            values.removeFirst()
            continue
        }
        if parsedImage == nil, value.hasPrefix("--gid-map=") {
            let separator = value.firstIndex(of: "=")!
            let mapping = String(value[value.index(after: separator)...])
            guard !mapping.isEmpty else {
                throw OrlixOCIEnvironmentRunArgumentsError
                    .missingOptionValue("--gid-map")
            }
            try Self.addIDMapping(
                mapping,
                to: &parsedGIDMappings,
                feature: "linux.gidMappings"
            )
            continue
        }
        if parsedImage == nil, value == "--sysctl" {
            guard let sysctl = values.first else {
                throw OrlixOCIEnvironmentRunArgumentsError
                        .missingOptionValue(value)
                }
                try Self.addSysctlEntry(sysctl, to: &parsedSysctls)
                values.removeFirst()
                continue
            }
			if parsedImage == nil, value.hasPrefix("--sysctl=") {
				let separator = value.firstIndex(of: "=")!
				let sysctl = String(value[value.index(after: separator)...])
				guard !sysctl.isEmpty else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue("--sysctl")
				}
				try Self.addSysctlEntry(sysctl, to: &parsedSysctls)
				continue
			}
			if parsedImage == nil, value == "--annotation" {
				guard let annotation = values.first else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue(value)
				}
				try Self.addAnnotationEntry(annotation, to: &parsedAnnotations)
				values.removeFirst()
				continue
			}
			if parsedImage == nil, value.hasPrefix("--annotation=") {
				let separator = value.firstIndex(of: "=")!
				let annotation = String(value[value.index(after: separator)...])
				guard !annotation.isEmpty else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue("--annotation")
				}
				try Self.addAnnotationEntry(annotation, to: &parsedAnnotations)
				continue
			}
			if parsedImage == nil, value == "--mask" {
				guard let path = values.first else {
					throw OrlixOCIEnvironmentRunArgumentsError
                        .missingOptionValue(value)
                }
                try Self.addRuntimePath(
                    path,
                    to: &parsedMaskedPaths,
                    feature: "linux.maskedPaths"
                )
                values.removeFirst()
                continue
            }
            if parsedImage == nil, value.hasPrefix("--mask=") {
                let separator = value.firstIndex(of: "=")!
                let path = String(value[value.index(after: separator)...])
                guard !path.isEmpty else {
                    throw OrlixOCIEnvironmentRunArgumentsError
                        .missingOptionValue("--mask")
                }
                try Self.addRuntimePath(
                    path,
                    to: &parsedMaskedPaths,
                    feature: "linux.maskedPaths"
                )
                continue
            }
            if parsedImage == nil, value == "--readonly" {
                guard let path = values.first else {
                    throw OrlixOCIEnvironmentRunArgumentsError
                        .missingOptionValue(value)
                }
                try Self.addRuntimePath(
                    path,
                    to: &parsedReadonlyPaths,
                    feature: "linux.readonlyPaths"
                )
                values.removeFirst()
                continue
            }
            if parsedImage == nil, value.hasPrefix("--readonly=") {
                let separator = value.firstIndex(of: "=")!
                let path = String(value[value.index(after: separator)...])
                guard !path.isEmpty else {
                    throw OrlixOCIEnvironmentRunArgumentsError
                        .missingOptionValue("--readonly")
                }
                try Self.addRuntimePath(
                    path,
                    to: &parsedReadonlyPaths,
                    feature: "linux.readonlyPaths"
                )
                continue
            }
            if parsedImage == nil, value == "--ulimit" {
                guard let rlimit = values.first else {
                    throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue(value)
				}
				try Self.addRlimit(rlimit, to: &parsedRlimits)
				values.removeFirst()
				continue
			}
			if parsedImage == nil, value.hasPrefix("--ulimit=") {
				let separator = value.firstIndex(of: "=")!
				let rlimit = String(value[value.index(after: separator)...])
				guard !rlimit.isEmpty else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue("--ulimit")
				}
				try Self.addRlimit(rlimit, to: &parsedRlimits)
				continue
			}
			if parsedImage == nil, value == "--umask" {
				guard let umask = values.first else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue(value)
				}
				parsedUmask = try Self.parseUmask(umask)
				values.removeFirst()
				continue
			}
			if parsedImage == nil, value.hasPrefix("--umask=") {
				let separator = value.firstIndex(of: "=")!
				let umask = String(value[value.index(after: separator)...])
				guard !umask.isEmpty else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue("--umask")
				}
				parsedUmask = try Self.parseUmask(umask)
				continue
			}
			if parsedImage == nil, value == "--oom-score-adj" {
				guard let oomScoreAdjustment = values.first else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue(value)
				}
				parsedOOMScoreAdjustment = try Self.parseOOMScoreAdjustment(
					oomScoreAdjustment
				)
				values.removeFirst()
				continue
			}
			if parsedImage == nil, value.hasPrefix("--oom-score-adj=") {
				let separator = value.firstIndex(of: "=")!
				let oomScoreAdjustment = String(value[value.index(after: separator)...])
				guard !oomScoreAdjustment.isEmpty else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue("--oom-score-adj")
				}
				parsedOOMScoreAdjustment = try Self.parseOOMScoreAdjustment(
					oomScoreAdjustment
				)
				continue
			}
			if parsedImage == nil, value == "--scheduler" {
				guard let scheduler = values.first else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue(value)
				}
				parsedScheduler = try Self.parseScheduler(scheduler)
				values.removeFirst()
				continue
			}
			if parsedImage == nil, value.hasPrefix("--scheduler=") {
				let separator = value.firstIndex(of: "=")!
				let scheduler = String(value[value.index(after: separator)...])
				guard !scheduler.isEmpty else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue("--scheduler")
				}
				parsedScheduler = try Self.parseScheduler(scheduler)
				continue
			}
			if parsedImage == nil, value == "--io-priority" {
				guard let ioPriority = values.first else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue(value)
				}
				parsedIOPriority = try Self.parseIOPriority(ioPriority)
				values.removeFirst()
				continue
			}
			if parsedImage == nil, value.hasPrefix("--io-priority=") {
				let separator = value.firstIndex(of: "=")!
				let ioPriority = String(value[value.index(after: separator)...])
				guard !ioPriority.isEmpty else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue("--io-priority")
				}
				parsedIOPriority = try Self.parseIOPriority(ioPriority)
				continue
			}
			if parsedImage == nil, value == "--cpu-affinity" {
				guard let cpuAffinity = values.first else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue(value)
				}
				parsedCPUAffinity = try Self.parseCPUAffinity(cpuAffinity)
				values.removeFirst()
				continue
			}
			if parsedImage == nil, value.hasPrefix("--cpu-affinity=") {
				let separator = value.firstIndex(of: "=")!
				let cpuAffinity = String(value[value.index(after: separator)...])
				guard !cpuAffinity.isEmpty else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue("--cpu-affinity")
				}
				parsedCPUAffinity = try Self.parseCPUAffinity(cpuAffinity)
				continue
			}
			if parsedImage == nil, value == "--personality" {
				guard let personality = values.first else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue(value)
				}
				parsedPersonalityDomain = try Self.parsePersonalityDomain(personality)
				values.removeFirst()
				continue
			}
			if parsedImage == nil, value.hasPrefix("--personality=") {
				let separator = value.firstIndex(of: "=")!
				let personality = String(value[value.index(after: separator)...])
				guard !personality.isEmpty else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue("--personality")
				}
				parsedPersonalityDomain = try Self.parsePersonalityDomain(personality)
				continue
			}
			if parsedImage == nil, value == "--cap-set" {
				guard let capabilities = values.first else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue(value)
				}
				try Self.addCapabilitySet(
					capabilities,
					to: &parsedCapabilities
				)
				parsedCapabilitiesPresent = true
				values.removeFirst()
				continue
			}
			if parsedImage == nil, value.hasPrefix("--cap-set=") {
				let separator = value.firstIndex(of: "=")!
				let capabilities = String(value[value.index(after: separator)...])
				guard !capabilities.isEmpty else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue("--cap-set")
				}
				try Self.addCapabilitySet(
					capabilities,
					to: &parsedCapabilities
				)
				parsedCapabilitiesPresent = true
				continue
			}
			if parsedImage == nil, value == "--entrypoint" {
				guard let entrypoint = values.first else {
					throw OrlixOCIEnvironmentRunArgumentsError
                        .missingOptionValue(value)
                }
                parsedEntrypoint = [entrypoint]
                values.removeFirst()
                continue
            }
            if parsedImage == nil, value.hasPrefix("--entrypoint=") {
                let separator = value.firstIndex(of: "=")!
                let entrypoint = String(value[value.index(after: separator)...])
                guard !entrypoint.isEmpty else {
                    throw OrlixOCIEnvironmentRunArgumentsError
                        .missingOptionValue("--entrypoint")
                }
				parsedEntrypoint = [entrypoint]
				continue
			}
			if parsedImage == nil, value == "--env" || value == "-e" {
				guard let environmentEntry = values.first else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue(value)
				}
				try Self.addEnvironmentEntry(
					environmentEntry,
					to: &parsedEnvironment
				)
				values.removeFirst()
				continue
			}
			if parsedImage == nil, value.hasPrefix("--env=") {
				let separator = value.firstIndex(of: "=")!
				let environmentEntry = String(
					value[value.index(after: separator)...]
				)
				guard !environmentEntry.isEmpty else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue("--env")
				}
				try Self.addEnvironmentEntry(
					environmentEntry,
					to: &parsedEnvironment
				)
				continue
			}
			if parsedImage == nil, value == "--workdir" || value == "-w" {
				guard let workingDirectory = values.first else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue(value)
				}
				parsedWorkingDirectory = try Self.validatedWorkingDirectory(
					workingDirectory
				)
				values.removeFirst()
				continue
			}
			if parsedImage == nil, value.hasPrefix("--workdir=") {
				let separator = value.firstIndex(of: "=")!
				let workingDirectory = String(
					value[value.index(after: separator)...]
				)
				guard !workingDirectory.isEmpty else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue("--workdir")
				}
				parsedWorkingDirectory = try Self.validatedWorkingDirectory(
					workingDirectory
				)
				continue
			}
			if parsedImage == nil, value == "--user" || value == "-u" {
				guard let user = values.first else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue(value)
				}
				let parsedUser = try Self.parseUser(user)
				parsedUserID = parsedUser.uid
				parsedGroupID = parsedUser.gid
				values.removeFirst()
				continue
			}
			if parsedImage == nil, value.hasPrefix("--user=") {
				let separator = value.firstIndex(of: "=")!
				let user = String(value[value.index(after: separator)...])
				guard !user.isEmpty else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue("--user")
				}
				let parsedUser = try Self.parseUser(user)
				parsedUserID = parsedUser.uid
				parsedGroupID = parsedUser.gid
				continue
			}
			if parsedImage == nil, value == "--group-add" {
				guard let group = values.first else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue(value)
				}
				try Self.addSupplementaryGroup(
					group,
					to: &parsedSupplementaryGroupIDs
				)
				values.removeFirst()
				continue
			}
			if parsedImage == nil, value.hasPrefix("--group-add=") {
				let separator = value.firstIndex(of: "=")!
				let group = String(value[value.index(after: separator)...])
				guard !group.isEmpty else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue("--group-add")
				}
				try Self.addSupplementaryGroup(
					group,
					to: &parsedSupplementaryGroupIDs
				)
				continue
			}
			if parsedImage == nil, value == "--hostname" || value == "-h" {
				guard let hostname = values.first else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue(value)
				}
				parsedHostname = try Self.validatedUTSName(
					hostname,
					feature: "hostname"
				)
				values.removeFirst()
				continue
			}
			if parsedImage == nil, value.hasPrefix("--hostname=") {
				let separator = value.firstIndex(of: "=")!
				let hostname = String(value[value.index(after: separator)...])
				guard !hostname.isEmpty else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue("--hostname")
				}
				parsedHostname = try Self.validatedUTSName(
					hostname,
					feature: "hostname"
				)
				continue
			}
			if parsedImage == nil, value == "--domainname" {
				guard let domainname = values.first else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue(value)
				}
				parsedDomainname = try Self.validatedUTSName(
					domainname,
					feature: "domainname"
				)
				values.removeFirst()
				continue
			}
			if parsedImage == nil, value.hasPrefix("--domainname=") {
				let separator = value.firstIndex(of: "=")!
				let domainname = String(value[value.index(after: separator)...])
				guard !domainname.isEmpty else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue("--domainname")
				}
				parsedDomainname = try Self.validatedUTSName(
					domainname,
					feature: "domainname"
				)
				continue
			}
			if parsedImage == nil, value == "--id" || value == "--name" {
				guard let id = values.first else {
					throw OrlixOCIEnvironmentRunArgumentsError.missingOptionValue(value)
				}
				parsedID = id
				values.removeFirst()
				continue
			}
			if parsedImage == nil,
			   value.hasPrefix("--id=") || value.hasPrefix("--name=")
			{
				let separator = value.firstIndex(of: "=")!
				let option = String(value[..<separator])
				let id = String(value[value.index(after: separator)...])
				guard !id.isEmpty else {
					throw OrlixOCIEnvironmentRunArgumentsError.missingOptionValue(option)
				}
				parsedID = id
				continue
			}
			if parsedImage == nil, value == "--platform" {
				guard let platform = values.first else {
					throw OrlixOCIEnvironmentRunArgumentsError.missingOptionValue(value)
				}
				parsedPlatform = platform
				values.removeFirst()
				continue
			}
			if parsedImage == nil, value.hasPrefix("--platform=") {
				let separator = value.firstIndex(of: "=")!
				let platform = String(value[value.index(after: separator)...])
				guard !platform.isEmpty else {
					throw OrlixOCIEnvironmentRunArgumentsError
						.missingOptionValue("--platform")
				}
				parsedPlatform = platform
				continue
			}
			if parsedImage == nil, value.hasPrefix("-") {
				throw OrlixOCIEnvironmentRunArgumentsError.unknownOption(value)
			}
			if parsedImage == nil {
				parsedImage = value
			} else {
				parsedCommand.append(value)
			}
		}

		guard let image = parsedImage else {
			throw OrlixOCIEnvironmentRunArgumentsError.missingImage
		}
		let reference = try OrlixOCIRegistryImageReference(image)
		let id = try parsedID ?? Self.defaultEnvironmentID(for: reference)
		try OrlixEnvironmentStorageLayout.validateEnvironmentID(id)

        self.image = image
		self.id = id
		self.platform = parsedPlatform
		self.entrypoint = parsedEntrypoint
		self.environment = parsedEnvironment
		self.workingDirectory = parsedWorkingDirectory
		self.userID = parsedUserID
		self.groupID = parsedGroupID
		self.supplementaryGroupIDs = parsedSupplementaryGroupIDs
		self.capabilities = parsedCapabilitiesPresent ? parsedCapabilities : nil
        self.hostname = parsedHostname
        self.domainname = parsedDomainname
        self.terminal = parsedTerminal
        self.terminalRows = parsedTerminalRows
        self.terminalColumns = parsedTerminalColumns
        self.rootReadonly = parsedRootReadonly
        self.rootPropagation = parsedRootPropagation
        self.rlimits = parsedRlimits
        self.sysctls = parsedSysctls
        self.maskedPaths = parsedMaskedPaths
        self.readonlyPaths = parsedReadonlyPaths
        self.umask = parsedUmask
		self.oomScoreAdjustment = parsedOOMScoreAdjustment
		self.scheduler = parsedScheduler
		self.ioPriority = parsedIOPriority
		self.cpuAffinity = parsedCPUAffinity
		self.personalityDomain = parsedPersonalityDomain
		self.noNewPrivileges = parsedNoNewPrivileges
		self.closeAdditionalFds = parsedCloseAdditionalFds
		self.cgroupsPath = parsedCgroupsPath
		self.cgroupPidsLimit = parsedCgroupPidsLimit
        self.cgroupCPUMax = parsedCgroupCPUMax
        self.cgroupCPUWeight = parsedCgroupCPUWeight
        self.cgroupMemoryMax = parsedCgroupMemoryMax
        self.cgroupIOWeight = parsedCgroupIOWeight
		self.cgroupUnified = parsedCgroupUnified
		self.tmpfsMounts = parsedTmpfsMounts
		self.mounts = parsedMounts
		self.publishedPorts = parsedPublishedPorts
		self.deviceNodes = parsedDeviceNodes
        self.namespaces = parsedNamespaces
        self.namespacePaths = parsedNamespacePaths
		self.timeOffsets = parsedTimeOffsets
		self.uidMappings = parsedUIDMappings
		self.gidMappings = parsedGIDMappings
		self.annotations = parsedAnnotations
		self.command = parsedCommand.isEmpty ? nil : parsedCommand
		self.removeAfterRun = parsedRemoveAfterRun
	}

	private static func addEnvironmentEntry(
		_ value: String,
		to environment: inout [String: String]
	) throws {
		guard let separator = value.firstIndex(of: "=") else {
			throw OrlixOCIRuntimeConfigError.invalidEnvironmentEntry(value)
		}
		let key = String(value[..<separator])
		let variableValue = String(value[value.index(after: separator)...])
		guard !key.isEmpty,
			!key.contains("\u{0}"),
			!variableValue.contains("\u{0}"),
			environment[key] == nil
		else {
			throw OrlixOCIRuntimeConfigError.invalidEnvironmentEntry(value)
		}
		environment[key] = variableValue
	}

    private static func parseRootPropagation(
        _ value: String
    ) throws -> OrlixEnvironmentRootPropagation {
        guard let propagation = OrlixEnvironmentRootPropagation(rawValue: value)
        else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "linux.rootfsPropagation"
            )
        }
        return propagation
    }

	private static func addRlimit(
		_ value: String,
		to rlimits: inout [OrlixEnvironmentRlimit]
	) throws {
		let rlimit = try parseRlimit(value)
		guard !rlimits.contains(where: { $0.type == rlimit.type }) else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
				"process.rlimits"
			)
		}
		rlimits.append(rlimit)
	}

	private static func addAnnotationEntry(
		_ value: String,
		to annotations: inout [String: String]
	) throws {
		guard let separator = value.firstIndex(of: "=") else {
			throw OrlixOCIRuntimeConfigError.invalidAnnotationEntry(value)
		}
		let key = String(value[..<separator])
		let annotationValue = String(value[value.index(after: separator)...])
		guard !key.isEmpty,
			!key.contains("\u{0}"),
			!annotationValue.contains("\u{0}")
		else {
			throw OrlixOCIRuntimeConfigError.invalidAnnotationEntry(value)
		}
		annotations[key] = annotationValue
	}

	private static func parseRlimit(_ value: String)
		throws -> OrlixEnvironmentRlimit
	{
		guard let separator = value.firstIndex(of: "=") else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
				"process.rlimits"
			)
		}
		let rawType = String(value[..<separator])
		let limits = String(value[value.index(after: separator)...])
		let type = try normalizedRlimitType(rawType)
		let pieces = limits.split(separator: ":", omittingEmptySubsequences: false)
		guard pieces.count == 1 || pieces.count == 2,
			let soft = UInt64(pieces[0])
		else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
				"process.rlimits"
			)
		}
		let hard: UInt64
		if pieces.count == 2 {
			guard let parsedHard = UInt64(pieces[1]) else {
				throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
					"process.rlimits"
				)
			}
			hard = parsedHard
		} else {
			hard = soft
		}
		guard soft <= hard else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
				"process.rlimits"
			)
		}
		return OrlixEnvironmentRlimit(type: type, soft: soft, hard: hard)
	}

	private static func normalizedRlimitType(_ value: String) throws -> String {
		guard !value.isEmpty,
			!value.contains("\u{0}"),
			!value.contains(":")
		else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
				"process.rlimits"
			)
		}
		let upper = value.uppercased()
		let type = upper.hasPrefix("RLIMIT_") ? upper : "RLIMIT_\(upper)"
		guard OrlixOCIRuntimeConfigParser.supportedRlimitTypes.contains(type)
		else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
				"process.rlimits"
			)
		}
		return type
	}

	private static func parseUmask(_ value: String) throws -> UInt32 {
		guard !value.isEmpty,
			!value.contains("\u{0}")
		else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
				"process.user.umask"
			)
		}
		let radix = value.hasPrefix("0") ? 8 : 10
		guard let parsed = UInt32(value, radix: radix),
			parsed <= 0o777
		else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
				"process.user.umask"
			)
		}
		return parsed
	}

	private static func validatedWorkingDirectory(
		_ value: String
	) throws -> String {
		guard !value.isEmpty else {
			throw OrlixOCIRuntimeConfigError.missingWorkingDirectory
		}
		guard value.hasPrefix("/"), !value.contains("\u{0}") else {
			throw OrlixOCIRuntimeConfigError.invalidWorkingDirectory(value)
		}
		return value
	}

	private static func parseOOMScoreAdjustment(_ value: String) throws -> Int32 {
		guard let parsed = Int32(value), parsed >= -1000, parsed <= 1000 else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
				"process.oomScoreAdj"
			)
		}
		return parsed
	}

	private static func parseScheduler(
		_ value: String
	) throws -> OrlixEnvironmentScheduler {
		let parts = value.split(separator: ":", omittingEmptySubsequences: false)
		guard parts.count == 1 || parts.count == 2 else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
				"process.scheduler"
			)
		}
		let policy = String(parts[0])
		let supportedPolicies: Set<String> = [
			"SCHED_OTHER",
			"SCHED_BATCH",
			"SCHED_IDLE",
			"SCHED_FIFO",
			"SCHED_RR",
		]
		guard supportedPolicies.contains(policy) else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
				"process.scheduler.policy"
			)
		}
		let priority: Int32
		if parts.count == 2 {
			guard
				let parsedPriority = Int32(parts[1]),
				parsedPriority >= 0
			else {
				throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
					"process.scheduler.priority"
				)
			}
			priority = parsedPriority
		} else {
			priority = 0
		}
		return OrlixEnvironmentScheduler(policy: policy, priority: priority)
	}

	private static func parseIOPriority(
		_ value: String
	) throws -> OrlixEnvironmentIOPriority {
		let parts = value.split(separator: ":", omittingEmptySubsequences: false)
		guard parts.count == 1 || parts.count == 2 else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
				"process.ioPriority"
			)
		}
		let priorityClass = String(parts[0])
		let supportedClasses: Set<String> = [
			"IOPRIO_CLASS_RT",
			"IOPRIO_CLASS_BE",
			"IOPRIO_CLASS_IDLE",
		]
		guard supportedClasses.contains(priorityClass) else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
				"process.ioPriority.class"
			)
		}
		let priority: Int32
		if parts.count == 2 {
			guard
				let parsedPriority = Int32(parts[1]),
				parsedPriority >= 0,
				parsedPriority <= 7
			else {
				throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
					"process.ioPriority.priority"
				)
			}
			priority = parsedPriority
		} else {
			priority = 0
		}
		return OrlixEnvironmentIOPriority(class: priorityClass, priority: priority)
	}

	private static func parseCPUAffinity(
		_ value: String
	) throws -> OrlixEnvironmentCPUAffinity {
		let parts = value.split(separator: ",", omittingEmptySubsequences: false)
		guard !parts.isEmpty else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
				"process.execCPUAffinity"
			)
		}
		for part in parts {
			let range = part.split(separator: "-", omittingEmptySubsequences: false)
			guard range.count == 1 || range.count == 2 else {
				throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
					"process.execCPUAffinity"
				)
			}
			guard let start = Int(range[0]), start >= 0 else {
				throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
					"process.execCPUAffinity"
				)
			}
			if range.count == 2 {
				guard let end = Int(range[1]), end >= start else {
					throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
						"process.execCPUAffinity"
					)
				}
			}
		}
		return OrlixEnvironmentCPUAffinity(mask: value)
	}

	private static func parsePersonalityDomain(_ value: String) throws -> String {
		guard Set(["LINUX", "LINUX32"]).contains(value) else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
				"linux.personality.domain"
			)
		}
		return value
	}

	private static func addCapabilitySet(
		_ value: String,
		to capabilities: inout OrlixEnvironmentCapabilities
	) throws {
		guard let separator = value.firstIndex(of: "=") else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
				"process.capabilities"
			)
		}
		let field = String(value[..<separator])
		let names = String(value[value.index(after: separator)...])
		let parsedNames = try parseCapabilityNames(names, field: field)
		switch field {
		case "bounding":
			capabilities = OrlixEnvironmentCapabilities(
				bounding: parsedNames,
				permitted: capabilities.permitted,
				inheritable: capabilities.inheritable,
				effective: capabilities.effective,
				ambient: capabilities.ambient
			)
		case "permitted":
			capabilities = OrlixEnvironmentCapabilities(
				bounding: capabilities.bounding,
				permitted: parsedNames,
				inheritable: capabilities.inheritable,
				effective: capabilities.effective,
				ambient: capabilities.ambient
			)
		case "inheritable":
			capabilities = OrlixEnvironmentCapabilities(
				bounding: capabilities.bounding,
				permitted: capabilities.permitted,
				inheritable: parsedNames,
				effective: capabilities.effective,
				ambient: capabilities.ambient
			)
		case "effective":
			capabilities = OrlixEnvironmentCapabilities(
				bounding: capabilities.bounding,
				permitted: capabilities.permitted,
				inheritable: capabilities.inheritable,
				effective: parsedNames,
				ambient: capabilities.ambient
			)
		case "ambient":
			capabilities = OrlixEnvironmentCapabilities(
				bounding: capabilities.bounding,
				permitted: capabilities.permitted,
				inheritable: capabilities.inheritable,
				effective: capabilities.effective,
				ambient: parsedNames
			)
		default:
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
				"process.capabilities.\(field)"
			)
		}
	}

	private static func parseCapabilityNames(
		_ value: String,
		field: String
	) throws -> [String] {
		guard !field.isEmpty, !field.contains("\u{0}") else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
				"process.capabilities"
			)
		}
		var seen = Set<String>()
		var result: [String] = []
		for name in value.split(separator: ",", omittingEmptySubsequences: false)
			.map(String.init)
		{
			guard !name.isEmpty else {
				throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
					"process.capabilities.\(field)"
				)
			}
			guard OrlixEnvironmentCapabilities.supportedLinuxNames.contains(name) else {
				throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
					"process.capabilities.\(field)"
				)
			}
			if seen.insert(name).inserted {
				result.append(name)
			}
		}
		return result
	}

	private static func parseUser(
		_ value: String
	) throws -> (uid: UInt32, gid: UInt32?) {
		let parts = value.split(separator: ":", omittingEmptySubsequences: false)
		guard parts.count == 1 || parts.count == 2 else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
				"process.user"
			)
		}
		let uid = try parseID(String(parts[0]), feature: "process.user.uid")
		let gid: UInt32?
		if parts.count == 2 {
			gid = try parseID(String(parts[1]), feature: "process.user.gid")
		} else {
			gid = nil
		}
		return (uid, gid)
	}

	private static func addSupplementaryGroup(
		_ value: String,
		to groups: inout [UInt32]
	) throws {
		let group = try parseID(
			value,
			feature: "process.user.additionalGids"
		)
		if !groups.contains(group) {
			groups.append(group)
		}
	}

    private static func parseTerminalSize(_ value: String) throws -> (UInt32, UInt32) {
        let parts = value.split(separator: "x", omittingEmptySubsequences: false)
        guard parts.count == 2,
            let rows = UInt32(parts[0]),
            let columns = UInt32(parts[1]),
            rows > 0,
            columns > 0
        else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "process.consoleSize"
            )
        }
        return (rows, columns)
    }

    private static func parseCgroupsPath(_ value: String) throws -> String {
		guard !value.isEmpty, !value.contains("\u{0}") else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
				"linux.cgroupsPath"
			)
		}
		if value.hasPrefix("/") {
			return value
		}
		return "/orlix/\(value)"
	}

	private static func parseCgroupPidsLimit(_ value: String) throws -> Int64 {
		guard let limit = Int64(value), limit >= -1 else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
				"linux.resources.pids.limit"
			)
		}
		return limit
	}

	private static func parseCgroupCPUMax(
		_ value: String
	) throws -> OrlixEnvironmentCgroupCPUMax {
		let parts = value.split(separator: ":", omittingEmptySubsequences: false)
		guard parts.count == 1 || parts.count == 2 else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
				"linux.resources.cpu.quota"
			)
		}
		let quota: Int64
		if parts[0] == "max" {
			quota = -1
		} else {
			guard let parsedQuota = Int64(parts[0]), parsedQuota > 0 else {
				throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
					"linux.resources.cpu.quota"
				)
			}
			quota = parsedQuota
		}
		let period: UInt64
		if parts.count == 2 {
			guard let parsedPeriod = UInt64(parts[1]), parsedPeriod > 0 else {
				throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
					"linux.resources.cpu.period"
				)
			}
			period = parsedPeriod
		} else {
			period = OrlixEnvironmentCgroupCPUMax.defaultPeriodMicros
		}
		return OrlixEnvironmentCgroupCPUMax(
			quotaMicros: quota,
			periodMicros: period
		)
	}

	private static func parseCgroupCPUWeight(_ value: String) throws -> UInt64 {
		guard let weight = UInt64(value), (1...10_000).contains(weight) else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
				"linux.resources.cpu.shares"
			)
		}
		return weight
	}

	private static func parseCgroupMemoryMax(_ value: String) throws -> Int64 {
		guard let limit = Int64(value), limit >= -1 else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
				"linux.resources.memory.limit"
			)
		}
		return limit
	}

    private static func parseCgroupIOWeight(_ value: String) throws -> UInt64 {
        guard let weight = UInt64(value), (1...10_000).contains(weight) else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "linux.resources.blockIO.weight"
            )
        }
        return weight
    }

    private static func addCgroupUnifiedEntry(
        _ value: String,
        to entries: inout [OrlixEnvironmentCgroupUnifiedEntry]
    ) throws {
        guard let separator = value.firstIndex(of: "=") else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "linux.resources.unified"
            )
        }
        let file = String(value[..<separator])
        let unifiedValue = String(value[value.index(after: separator)...])
		guard !file.isEmpty,
		      !file.contains("\u{0}"),
              !file.contains("/"),
              !unifiedValue.isEmpty,
              !unifiedValue.contains("\u{0}"),
              !unifiedValue.contains("\n"),
              !unifiedValue.contains("\r"),
              !entries.contains(where: { $0.file == file })
        else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "linux.resources.unified.\(file)"
            )
        }
        entries.append(
            OrlixEnvironmentCgroupUnifiedEntry(
                file: file,
                value: unifiedValue
            )
        )
    }

    private static func addTmpfsMount(
        _ value: String,
        to mounts: inout [OrlixEnvironmentTmpfsMount]
    ) throws {
        let parts = value.split(
            separator: ":",
            maxSplits: 1,
            omittingEmptySubsequences: false
        )
        let targetPath = String(parts[0])
        guard !targetPath.isEmpty,
              !mounts.contains(where: { $0.targetPath == targetPath })
        else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "mounts.destination"
            )
        }

        var readOnly = false
        var noSuid = false
        var noDev = false
        var noExec = false
        var dataOptions: [String] = []
        if parts.count == 2 {
            let optionsText = String(parts[1])
            guard !optionsText.isEmpty else {
                throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                    "mounts.options"
                )
            }
            for option in optionsText.split(
                separator: ",",
                omittingEmptySubsequences: false
            ) {
                let option = String(option)
                switch option {
                case "ro":
                    readOnly = true
                case "rw":
                    readOnly = false
                case "nosuid":
                    noSuid = true
                case "nodev":
                    noDev = true
                case "noexec":
                    noExec = true
                default:
                    if try Self.validatedTmpfsDataOption(option) {
                        dataOptions.append(option)
                    } else {
                        throw OrlixOCIRuntimeConfigError
                            .unsupportedLinuxFeature("mounts.options")
                    }
                }
            }
        }

        do {
            mounts.append(
                try OrlixEnvironmentTmpfsMount(
                    targetPath: targetPath,
                    readOnly: readOnly,
                    noSuid: noSuid,
                    noDev: noDev,
                    noExec: noExec,
                    data: dataOptions.isEmpty
                        ? nil
                        : dataOptions.joined(separator: ",")
                )
            )
        } catch OrlixEnvironmentMountError.invalidTargetPath(_),
                OrlixEnvironmentMountError.reservedTargetPath(_) {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "mounts.destination"
            )
        } catch OrlixEnvironmentMountError.invalidTmpfsData(_) {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "mounts.options"
            )
        }
    }

    private static func addMount(
        _ value: String,
        to mounts: inout [OrlixEnvironmentMount]
    ) throws {
        let mount = try parseMount(value)
        guard !mounts.contains(where: { $0.targetPath == mount.targetPath })
        else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "mounts.destination"
            )
        }
	mounts.append(mount)
	}

	private static func addVolume(
		_ value: String,
		to mounts: inout [OrlixEnvironmentMount]
	) throws {
		let mount = try parseVolume(value)
		guard !mounts.contains(where: { $0.targetPath == mount.targetPath })
		else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
				"mounts.destination"
			)
		}
		mounts.append(mount)
	}

private static func addPublishedPort(
_ value: String,
to publishedPorts: inout [OrlixEnvironmentPublishedPort]
) throws {
let publishedPort = try parsePublishedPort(value)
guard !publishedPorts.contains(publishedPort)
else {
throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
"linux.ports"
)
}
publishedPorts.append(publishedPort)
}

private static func parsePublishedPort(_ value: String) throws
-> OrlixEnvironmentPublishedPort
{
let portAndProtocol = value.split(
separator: "/",
maxSplits: 1,
omittingEmptySubsequences: false
)
guard portAndProtocol.count == 1 || portAndProtocol.count == 2
else {
throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
"linux.ports"
)
}
let proto: String
if portAndProtocol.count == 2 {
proto = String(portAndProtocol[1]).lowercased()
guard ["tcp", "udp", "sctp"].contains(proto)
else {
throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
"linux.ports.protocol"
)
}
} else {
proto = "tcp"
}
let fields = portAndProtocol[0].split(
separator: ":",
maxSplits: 2,
omittingEmptySubsequences: false
)
switch fields.count {
case 1:
return OrlixEnvironmentPublishedPort(
containerPort: try parsePublishedContainerPort(String(fields[0])),
proto: proto
)
case 2:
return OrlixEnvironmentPublishedPort(
containerPort: try parsePublishedContainerPort(String(fields[1])),
proto: proto,
hostPort: try parsePublishedHostPort(String(fields[0]))
)
case 3:
return OrlixEnvironmentPublishedPort(
containerPort: try parsePublishedContainerPort(String(fields[2])),
proto: proto,
hostPort: try parsePublishedHostPort(String(fields[1])),
hostAddress: try parsePublishedHostAddress(String(fields[0]))
)
default:
throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
"linux.ports"
)
}
}

private static func parsePublishedContainerPort(
_ value: String
) throws -> UInt16 {
guard let port = UInt16(value), port > 0
else {
throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
"linux.ports.container"
)
}
return port
}

private static func parsePublishedHostPort(_ value: String) throws -> UInt16 {
guard let port = UInt16(value), port > 0
else {
throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
"linux.ports.host"
)
}
return port
}

private static func parsePublishedHostAddress(_ value: String) throws
-> String
{
guard !value.isEmpty,
!value.contains("\u{0}"),
!value.contains("/")
else {
throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
"linux.ports.hostIP"
)
}
return value
}

private static func parseVolume(_ value: String) throws
		-> OrlixEnvironmentMount
	{
		let parts = value.split(
			separator: ":",
			maxSplits: 2,
			omittingEmptySubsequences: false
		)
		guard parts.count == 2 || parts.count == 3 else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("mounts")
		}
		let rawSource = String(parts[0])
		let target = String(parts[1])
		guard !rawSource.isEmpty, !target.isEmpty else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("mounts")
		}

		let source: String
		switch rawSource {
		case "documents":
			source = "orlix:documents"
		default:
			source = rawSource
		}

		var options = ["bind"]
		if parts.count == 3 {
			let optionsText = String(parts[2])
			guard !optionsText.isEmpty else {
				throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
					"mounts.options"
				)
			}
			for rawOption in optionsText.split(
				separator: ",",
				omittingEmptySubsequences: false
			) {
				let option = String(rawOption)
				guard !option.isEmpty else {
					throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
						"mounts.options"
					)
				}
				options.append(option == "readonly" ? "ro" : option)
			}
		}

		let runtimeMount = OrlixOCIRuntimeMount(
			destination: target,
			type: "bind",
			source: source,
			options: options
		)
		guard let mount = try runtimeMount.environmentMount() else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
				"mounts.type.bind"
			)
		}
		return mount
	}

	private static func parseMount(_ value: String) throws
	-> OrlixEnvironmentMount
	{
        var type: String?
        var source: String?
        var destination: String?
        var options: [String] = []
        var seenKeys = Set<String>()
        let parts = value.split(separator: ",", omittingEmptySubsequences: false)
        guard !parts.isEmpty else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature("mounts")
        }
        for rawPart in parts {
            let part = String(rawPart)
            guard !part.isEmpty else {
                throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                    "mounts.options"
                )
            }
            if let separator = part.firstIndex(of: "=") {
                let key = String(part[..<separator])
                let fieldValue = String(part[part.index(after: separator)...])
                guard !key.isEmpty,
                      !fieldValue.isEmpty,
                      !seenKeys.contains(key)
                else {
                    throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                        "mounts.options"
                    )
                }
                seenKeys.insert(key)
                switch key {
                case "type":
                    type = fieldValue
                case "source", "src":
                    source = fieldValue
                case "target", "destination", "dst":
                    destination = fieldValue
                case "readonly":
                    guard fieldValue == "true" || fieldValue == "1" else {
                        throw OrlixOCIRuntimeConfigError
                            .unsupportedLinuxFeature("mounts.options")
                    }
                    options.append("ro")
                default:
                    throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                        "mounts.options"
                    )
                }
            } else {
                switch part {
                case "readonly":
                    options.append("ro")
                default:
                    options.append(part)
                }
            }
        }
        guard let mountType = type, mountType == "bind" else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "mounts.type.\(type ?? "")"
            )
        }
        guard let destination else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "mounts.destination"
            )
        }
        let runtimeMount = OrlixOCIRuntimeMount(
            destination: destination,
            type: mountType,
            source: source,
            options: options
        )
        guard let mount = try runtimeMount.environmentMount() else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "mounts.type.\(mountType)"
            )
        }
        return mount
    }

    private static func validatedTmpfsDataOption(
        _ option: String
    ) throws -> Bool {
        guard let separator = option.firstIndex(of: "=") else {
            return false
        }
        let key = String(option[..<separator])
        let value = String(option[option.index(after: separator)...])
        switch key {
        case "size":
            guard !value.isEmpty,
                  value.unicodeScalars.allSatisfy({
                      CharacterSet.alphanumerics
                          .union(CharacterSet(charactersIn: "%"))
                          .contains($0)
                  })
            else {
                throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                    "mounts.options"
                )
            }
        case "mode":
            guard !value.isEmpty,
                  value.count <= 4,
                  value.unicodeScalars.allSatisfy({
                      CharacterSet(charactersIn: "01234567").contains($0)
                  })
            else {
                throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                    "mounts.options"
                )
            }
        case "uid", "gid":
            guard !value.isEmpty,
                  value.unicodeScalars.allSatisfy({
                      CharacterSet.decimalDigits.contains($0)
                  })
            else {
                throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                    "mounts.options"
                )
            }
        default:
            return false
        }
        return true
    }

    private static func addSysctlEntry(
        _ value: String,
        to sysctls: inout [String: String]
    ) throws {
        guard let separator = value.firstIndex(of: "=") else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "linux.sysctl"
            )
        }
        let key = String(value[..<separator])
        let sysctlValue = String(value[value.index(after: separator)...])
        let allowedScalars = CharacterSet.alphanumerics
            .union(CharacterSet(charactersIn: "._-"))
        guard !key.isEmpty,
              key.unicodeScalars.allSatisfy({
                  allowedScalars.contains($0)
              }),
              key.first != ".",
              key.last != ".",
              !key.contains(".."),
              !sysctlValue.contains("\u{0}"),
              !sysctlValue.contains("\n"),
              !sysctlValue.contains("\r"),
              sysctls[key] == nil
        else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "linux.sysctl"
            )
        }
        sysctls[key] = sysctlValue
    }

    private static func addRuntimePath(
        _ path: String,
        to paths: inout [String],
        feature: String
    ) throws {
        let components = path.split(
            separator: "/",
            omittingEmptySubsequences: false
        )
        guard path.hasPrefix("/"),
              path != "/",
              !path.contains("\u{0}"),
              !components.contains(where: { $0 == ".." }),
              !components.contains(where: { $0 == "." }),
              !paths.contains(path)
        else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(feature)
        }
        paths.append(path)
    }

    private static func addDeviceNode(
        _ value: String,
        to nodes: inout [OrlixEnvironmentDeviceNode]
    ) throws {
        let node = try parseDeviceNode(value)
        nodes.removeAll { $0.path == node.path }
        nodes.append(node)
    }

    private static func addNamespace(
        _ value: String,
        to namespaces: inout [String],
        namespacePaths: [String: String]
    ) throws {
        let namespace = try parseNamespace(value)
        guard namespacePaths[namespace] == nil,
            !namespaces.contains(namespace)
        else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "linux.namespaces"
            )
        }
        namespaces.append(namespace)
    }

    private static func addNamespacePath(
        _ value: String,
        to namespacePaths: inout [String: String],
        namespaces: [String]
    ) throws {
        let separator = value.firstIndex(of: "=")
        guard let separator else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "linux.namespaces.path"
            )
        }
        let namespace = try parseNamespace(String(value[..<separator]))
        let path = String(value[value.index(after: separator)...])
        guard !namespaces.contains(namespace),
            namespacePaths[namespace] == nil
        else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "linux.namespaces"
            )
        }
        var pathValidation: [String] = []
        try addRuntimePath(
            path,
            to: &pathValidation,
            feature: "linux.namespaces.path"
        )
        namespacePaths[namespace] = path
    }

    private static func parseNamespace(_ value: String) throws -> String {
        let supportedNamespaces = Set([
            "mount", "ipc", "uts", "network", "cgroup", "pid", "time", "user",
        ])
        guard supportedNamespaces.contains(value) else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "linux.namespaces.type"
            )
        }
        return value
    }

    private static func addTimeOffset(
        _ value: String,
        to offsets: inout [OrlixEnvironmentTimeOffset]
    ) throws {
        let offset = try parseTimeOffset(value)
        guard !offsets.contains(where: { $0.clock == offset.clock }) else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "linux.timeOffsets"
            )
        }
        offsets.append(offset)
    }

    private static func parseTimeOffset(
        _ value: String
    ) throws -> OrlixEnvironmentTimeOffset {
        let components = value.split(separator: ":", omittingEmptySubsequences: false)
            .map(String.init)
        guard components.count == 3 else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "linux.timeOffsets"
            )
        }
        let clock = components[0]
        guard Set(["monotonic", "boottime"]).contains(clock) else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "linux.timeOffsets"
            )
        }
        guard let secs = Int64(components[1]),
            let nanosecs = Int64(components[2]),
            nanosecs >= 0,
            nanosecs < 1_000_000_000
        else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "linux.timeOffsets"
            )
        }
        return OrlixEnvironmentTimeOffset(
            clock: clock,
            secs: secs,
            nanosecs: nanosecs
        )
    }

    private static func addIDMapping(
        _ value: String,
        to mappings: inout [OrlixEnvironmentIDMapping],
        feature: String
    ) throws {
        let mapping = try parseIDMapping(value, feature: feature)
        mappings.append(mapping)
    }

    private static func parseIDMapping(
        _ value: String,
        feature: String
    ) throws -> OrlixEnvironmentIDMapping {
        let components = value.split(separator: ":", omittingEmptySubsequences: false)
            .map(String.init)
        guard components.count == 3,
            let containerID = UInt32(components[0]),
            let hostID = UInt32(components[1]),
            let size = UInt32(components[2]),
            size > 0
        else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(feature)
        }
        return OrlixEnvironmentIDMapping(
            containerID: containerID,
            hostID: hostID,
            size: size
        )
    }

    private static func parseDeviceNode(
        _ value: String
    ) throws -> OrlixEnvironmentDeviceNode {
        let components = value.split(separator: ":", omittingEmptySubsequences: false)
            .map(String.init)
        guard components.count >= 2 else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "linux.devices"
            )
        }

        let path = components[0]
        let type = components[1]
        var pathValidation: [String] = []
        try addRuntimePath(path, to: &pathValidation, feature: "linux.devices.path")
        guard Set(["c", "b", "u", "p"]).contains(type) else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "linux.devices.type"
            )
        }

        if type == "p" {
            guard components.count <= 5 else {
                throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                    "linux.devices"
                )
            }
            return OrlixEnvironmentDeviceNode(
                path: path,
                type: type,
                fileMode: try parseDeviceNodeMode(
                    components.count >= 3 ? components[2] : nil
                ),
                uid: try parseDeviceNodeID(
                    components.count >= 4 ? components[3] : nil,
                    feature: "linux.devices.uid"
                ),
                gid: try parseDeviceNodeID(
                    components.count >= 5 ? components[4] : nil,
                    feature: "linux.devices.gid"
                )
            )
        }

        guard components.count >= 4, components.count <= 7 else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "linux.devices"
            )
        }
        return OrlixEnvironmentDeviceNode(
            path: path,
            type: type,
            major: try parseDeviceNodeID(
                components[2],
                feature: "linux.devices.major"
            ),
            minor: try parseDeviceNodeID(
                components[3],
                feature: "linux.devices.minor"
            ),
            fileMode: try parseDeviceNodeMode(
                components.count >= 5 ? components[4] : nil
            ),
            uid: try parseDeviceNodeID(
                components.count >= 6 ? components[5] : nil,
                feature: "linux.devices.uid"
            ),
            gid: try parseDeviceNodeID(
                components.count >= 7 ? components[6] : nil,
                feature: "linux.devices.gid"
            )
        )
    }

    private static func parseDeviceNodeMode(_ value: String?) throws -> UInt32 {
        guard let value, !value.isEmpty else { return 0o666 }
        guard value.count <= 4,
            value.unicodeScalars.allSatisfy({
                CharacterSet(charactersIn: "01234567").contains($0)
            }),
            let mode = UInt32(value, radix: 8),
            mode <= 0o7777
        else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "linux.devices.fileMode"
            )
        }
        return mode
    }

    private static func parseDeviceNodeID(
        _ value: String?,
        feature: String
    ) throws -> UInt32 {
        guard let value, !value.isEmpty else { return 0 }
        guard value.unicodeScalars.allSatisfy({
            CharacterSet.decimalDigits.contains($0)
        }),
            let id = UInt32(value)
        else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(feature)
        }
        return id
    }

    private static func parseID(
        _ value: String,
        feature: String
	) throws -> UInt32 {
		guard !value.isEmpty,
			value.allSatisfy(\.isNumber),
			let id = UInt32(value)
		else {
			throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(feature)
		}
		return id
	}

	private static func validatedUTSName(
		_ value: String,
		feature: String
	) throws -> String {
		guard !value.isEmpty, !value.contains("\u{0}") else {
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

	private static func defaultEnvironmentID(
		for image: OrlixOCIRegistryImageReference
	) throws -> String {
		let input = [
			image.registry,
			image.repository,
			image.manifestReference,
		].joined(separator: "-")
		var value = "oci-"
		for scalar in input.unicodeScalars {
			switch scalar.value {
			case 48...57, 65...90, 97...122:
				value.unicodeScalars.append(scalar)
			default:
				if value.last != "-" {
					value.append("-")
				}
			}
		}
		while value.last == "-" {
			value.removeLast()
		}
		try OrlixEnvironmentStorageLayout.validateEnvironmentID(value)
		return value
	}
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIEnvironmentInstaller: Sendable {
	private let registry: OrlixEnvironmentRegistry

    public init() throws {
        self.registry = try OrlixEnvironmentRegistry()
    }

    @_spi(OrlixPrivateTesting)
    public init(registry: OrlixEnvironmentRegistry) {
        self.registry = registry
    }

	private static func mergedRlimits(
		_ existing: [OrlixEnvironmentRlimit],
		overrides: [OrlixEnvironmentRlimit]
	) throws -> [OrlixEnvironmentRlimit] {
		guard !overrides.isEmpty else {
			return existing
		}
		var result = existing.filter { existingLimit in
			!overrides.contains { $0.type == existingLimit.type }
		}
		for override in overrides {
			guard OrlixOCIRuntimeConfigParser.supportedRlimitTypes
				.contains(override.type),
				override.soft <= override.hard
			else {
				throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
					"process.rlimits"
				)
			}
			result.append(override)
		}
		return result.sorted { $0.type < $1.type }
	}

    private static func mergedSupplementaryGroups(
        _ existing: [UInt32],
        adding added: [UInt32]
    ) -> [UInt32] {
        guard !added.isEmpty else { return existing }
		var result = existing
		for group in added where !result.contains(group) {
			result.append(group)
		}
        return result
    }

    private static func mergedCgroupUnified(
        _ existing: [OrlixEnvironmentCgroupUnifiedEntry],
        overrides: [OrlixEnvironmentCgroupUnifiedEntry]
    ) -> [OrlixEnvironmentCgroupUnifiedEntry] {
        guard !overrides.isEmpty else { return existing }
        let overrideFiles = Set(overrides.map(\.file))
        let retained = existing.filter { !overrideFiles.contains($0.file) }
        return (retained + overrides).sorted { $0.file < $1.file }
    }

    private static func mergedTmpfsMounts(
        _ existing: [OrlixEnvironmentTmpfsMount],
        overrides: [OrlixEnvironmentTmpfsMount]
    ) -> [OrlixEnvironmentTmpfsMount] {
        guard !overrides.isEmpty else { return existing }
        let overrideTargets = Set(overrides.map(\.targetPath))
        let retained = existing.filter {
            !overrideTargets.contains($0.targetPath)
        }
        return (retained + overrides).sorted { $0.targetPath < $1.targetPath }
    }

private static func mergedMounts(
_ existing: [OrlixEnvironmentMount],
overrides: [OrlixEnvironmentMount]
) -> [OrlixEnvironmentMount] {
        guard !overrides.isEmpty else { return existing }
        let overrideTargets = Set(overrides.map(\.targetPath))
        let retained = existing.filter {
            !overrideTargets.contains($0.targetPath)
        }
return (retained + overrides).sorted { $0.targetPath < $1.targetPath }
}

private static func mergedPublishedPorts(
_ existing: [OrlixEnvironmentPublishedPort],
overrides: [OrlixEnvironmentPublishedPort]
) -> [OrlixEnvironmentPublishedPort] {
guard !overrides.isEmpty else { return existing }
let overrideKeys = Set(overrides.map(publishedPortKey))
let retained = existing.filter { !overrideKeys.contains(publishedPortKey($0)) }
return (retained + overrides).sorted {
publishedPortKey($0) < publishedPortKey($1)
}
}

private static func publishedPortKey(
_ publishedPort: OrlixEnvironmentPublishedPort
) -> String {
let hostAddress = publishedPort.hostAddress ?? ""
let hostPort = publishedPort.hostPort.map(String.init) ?? ""
return [
publishedPort.proto,
String(publishedPort.containerPort),
hostAddress,
hostPort,
].joined(separator: "\u{0}")
}

private static func mergedDeviceNodes(
_ existing: [OrlixEnvironmentDeviceNode],
overrides: [OrlixEnvironmentDeviceNode]
) -> [OrlixEnvironmentDeviceNode] {
        guard !overrides.isEmpty else { return existing }
        let overridePaths = Set(overrides.map(\.path))
        let retained = existing.filter { !overridePaths.contains($0.path) }
        return (retained + overrides).sorted { $0.path < $1.path }
    }

    private static func mergedNamespaces(
        _ existing: [String],
        overrides: [String]
    ) -> [String] {
        guard !overrides.isEmpty else { return existing }
        return Array(Set(existing).union(overrides)).sorted()
    }

    private static func mergedNamespacePaths(
        _ existing: [String: String],
        overrides: [String: String]
    ) -> [String: String] {
        guard !overrides.isEmpty else { return existing }
        return existing.merging(overrides) { _, override in override }
    }

    private static func mergedTimeOffsets(
        _ existing: [OrlixEnvironmentTimeOffset],
        overrides: [OrlixEnvironmentTimeOffset]
    ) -> [OrlixEnvironmentTimeOffset] {
        guard !overrides.isEmpty else { return existing }
        let overrideClocks = Set(overrides.map(\.clock))
        let retained = existing.filter { !overrideClocks.contains($0.clock) }
        return (retained + overrides).sorted { $0.clock < $1.clock }
    }

    private static func mergedRuntimePaths(
        _ existing: [String],
        overrides: [String]
    ) -> [String] {
        guard !overrides.isEmpty else { return existing }
        let overridePaths = Set(overrides)
        let retained = existing.filter { !overridePaths.contains($0) }
        return (retained + overrides).sorted()
    }

    private static func defaultCgroupsPath(
		environmentID: String,
		required: Bool
	) throws -> String? {
		guard required else { return nil }
		let storageID = try OrlixEnvironmentStorageLayout.storageSafeID(
			environmentID
		)
		return "/orlix/oci/\(storageID)"
	}

	private static func descriptor(
		_ descriptor: OrlixEnvironmentDescriptor,
		replacingDefaultCommandWith command: [String]?,
		mergingDefaultEnvironmentWith environment: [String: String],
		replacingDefaultWorkingDirectoryWith workingDirectory: String?,
		replacingDefaultUserIDWith userID: UInt32?,
		replacingDefaultGroupIDWith groupID: UInt32?,
		mergingDefaultSupplementaryGroupsWith supplementaryGroups: [UInt32],
		replacingDefaultCapabilitiesWith capabilities: OrlixEnvironmentCapabilities?,
        replacingHostnameWith hostname: String?,
        replacingDomainnameWith domainname: String?,
        replacingDefaultTerminalWith terminal: Bool?,
        replacingDefaultTerminalRowsWith terminalRows: UInt32?,
        replacingDefaultTerminalColumnsWith terminalColumns: UInt32?,
		replacingRootReadonlyWith rootReadonly: Bool?,
		replacingRootPropagationWith rootPropagation:
			OrlixEnvironmentRootPropagation?,
		mergingAnnotationsWith annotations: [String: String],
		mergingDefaultRlimitsWith rlimits: [OrlixEnvironmentRlimit],
        mergingSysctlsWith sysctls: [String: String],
        mergingMaskedPathsWith maskedPaths: [String],
        mergingReadonlyPathsWith readonlyPaths: [String],
        replacingDefaultUmaskWith umask: UInt32?,
		replacingDefaultOOMScoreAdjustmentWith oomScoreAdjustment: Int32?,
		replacingDefaultSchedulerWith scheduler: OrlixEnvironmentScheduler?,
		replacingDefaultIOPriorityWith ioPriority: OrlixEnvironmentIOPriority?,
		replacingDefaultCPUAffinityWith cpuAffinity: OrlixEnvironmentCPUAffinity?,
		replacingDefaultPersonalityDomainWith personalityDomain: String?,
		replacingDefaultNoNewPrivilegesWith noNewPrivileges: Bool?,
		replacingDefaultCloseAdditionalFdsWith closeAdditionalFds: Bool?,
		replacingCgroupsPathWith cgroupsPath: String?,
		replacingCgroupPidsLimitWith cgroupPidsLimit: Int64?,
        replacingCgroupCPUMaxWith cgroupCPUMax: OrlixEnvironmentCgroupCPUMax?,
        replacingCgroupCPUWeightWith cgroupCPUWeight: UInt64?,
        replacingCgroupMemoryMaxWith cgroupMemoryMax: Int64?,
	replacingCgroupIOWeightWith cgroupIOWeight: UInt64?,
mergingCgroupUnifiedWith cgroupUnified: [OrlixEnvironmentCgroupUnifiedEntry],
mergingTmpfsMountsWith tmpfsMounts: [OrlixEnvironmentTmpfsMount],
mergingMountsWith mounts: [OrlixEnvironmentMount],
mergingPublishedPortsWith publishedPorts: [OrlixEnvironmentPublishedPort],
mergingDeviceNodesWith deviceNodes: [OrlixEnvironmentDeviceNode],
mergingNamespacesWith namespaces: [String],
        mergingNamespacePathsWith namespacePaths: [String: String],
        mergingTimeOffsetsWith timeOffsets: [OrlixEnvironmentTimeOffset],
        appendingUIDMappings uidMappings: [OrlixEnvironmentIDMapping],
        appendingGIDMappings gidMappings: [OrlixEnvironmentIDMapping]
    ) throws -> OrlixEnvironmentDescriptor {
		guard command != nil
			|| !environment.isEmpty
			|| workingDirectory != nil
			|| userID != nil
			|| groupID != nil
			|| !supplementaryGroups.isEmpty
			|| capabilities != nil
            || hostname != nil
            || domainname != nil
            || terminal != nil
            || terminalRows != nil
            || terminalColumns != nil
			|| rootReadonly != nil
			|| rootPropagation != nil
			|| !annotations.isEmpty
			|| !rlimits.isEmpty
            || !sysctls.isEmpty
            || !maskedPaths.isEmpty
            || !readonlyPaths.isEmpty
            || umask != nil
			|| oomScoreAdjustment != nil
			|| scheduler != nil
			|| ioPriority != nil
			|| cpuAffinity != nil
			|| personalityDomain != nil
			|| noNewPrivileges != nil
			|| closeAdditionalFds != nil
			|| cgroupsPath != nil
			|| cgroupPidsLimit != nil
            || cgroupCPUMax != nil
            || cgroupCPUWeight != nil
            || cgroupMemoryMax != nil
	|| cgroupIOWeight != nil
|| !cgroupUnified.isEmpty
|| !tmpfsMounts.isEmpty
|| !mounts.isEmpty
|| !publishedPorts.isEmpty
|| !deviceNodes.isEmpty
            || !namespaces.isEmpty
            || !namespacePaths.isEmpty
            || !timeOffsets.isEmpty
            || !uidMappings.isEmpty
            || !gidMappings.isEmpty
        else {
            return descriptor
        }
		if let command {
			guard !command.isEmpty else {
				throw OrlixOCIRuntimeConfigError.emptyProcessArgs
			}
			for argument in command {
				guard !argument.isEmpty, !argument.contains("\u{0}") else {
					throw OrlixOCIRuntimeConfigError.invalidProcessArg(argument)
				}
			}
		}
		for (key, value) in environment {
			guard !key.isEmpty,
				!key.contains("\u{0}"),
				!value.contains("\u{0}")
			else {
				throw OrlixOCIRuntimeConfigError.invalidEnvironmentEntry(
					"\(key)=\(value)"
				)
			}
		}
		for (key, value) in annotations {
			guard !key.isEmpty,
				!key.contains("\u{0}"),
				!value.contains("\u{0}")
			else {
				throw OrlixOCIRuntimeConfigError.invalidAnnotationEntry(key)
			}
		}
		let allowedSysctlScalars = CharacterSet.alphanumerics
			.union(CharacterSet(charactersIn: "._-"))
        for (key, value) in sysctls {
            guard !key.isEmpty,
                key.unicodeScalars.allSatisfy({
                    allowedSysctlScalars.contains($0)
                }),
				key.first != ".",
				key.last != ".",
				!key.contains(".."),
				!value.contains("\u{0}"),
				!value.contains("\n"),
				!value.contains("\r")
			else {
				throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
					"linux.sysctl"
                )
            }
        }
        for path in maskedPaths {
            let components = path.split(
                separator: "/",
                omittingEmptySubsequences: false
            )
            guard path.hasPrefix("/"),
                  path != "/",
                  !path.contains("\u{0}"),
                  !components.contains(where: { $0 == ".." }),
                  !components.contains(where: { $0 == "." })
            else {
                throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                    "linux.maskedPaths"
                )
            }
        }
        for path in readonlyPaths {
            let components = path.split(
                separator: "/",
                omittingEmptySubsequences: false
            )
            guard path.hasPrefix("/"),
                  path != "/",
                  !path.contains("\u{0}"),
                  !components.contains(where: { $0 == ".." }),
                  !components.contains(where: { $0 == "." })
            else {
                throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                    "linux.readonlyPaths"
                )
            }
        }
        if let workingDirectory {
			guard workingDirectory.hasPrefix("/"),
				!workingDirectory.contains("\u{0}")
			else {
				throw OrlixOCIRuntimeConfigError.invalidWorkingDirectory(
					workingDirectory
				)
			}
		}
		if let hostname {
			guard !hostname.isEmpty,
				!hostname.contains("\u{0}"),
				hostname.utf8.count <= 64
			else {
				throw OrlixOCIRuntimeConfigError.invalidHostname(hostname)
			}
		}
		if let domainname {
			guard !domainname.isEmpty,
				!domainname.contains("\u{0}"),
				domainname.utf8.count <= 64
			else {
				throw OrlixOCIRuntimeConfigError.invalidDomainname(domainname)
			}
		}
		let defaultEnvironment = descriptor.defaultEnvironment.merging(
			environment
		) { _, override in override }
		let effectiveAnnotations = descriptor.annotations.merging(
			annotations
		) { _, override in override }
		let effectiveSysctls = descriptor.sysctls.merging(
			sysctls
		) { _, override in override }
        let effectiveMaskedPaths = Self.mergedRuntimePaths(
            descriptor.maskedPaths,
            overrides: maskedPaths
        )
        let effectiveReadonlyPaths = Self.mergedRuntimePaths(
            descriptor.readonlyPaths,
            overrides: readonlyPaths
        )
        let defaultRlimits = try mergedRlimits(
			descriptor.defaultRlimits,
			overrides: rlimits
		)
		let defaultSupplementaryGroups = mergedSupplementaryGroups(
			descriptor.defaultSupplementaryGroups,
			adding: supplementaryGroups
		)
        let effectiveCgroupUnified = Self.mergedCgroupUnified(
            descriptor.cgroupUnified,
            overrides: cgroupUnified
        )
	let effectiveTmpfsMounts = Self.mergedTmpfsMounts(
		descriptor.tmpfsMounts,
		overrides: tmpfsMounts
	)
let effectiveMounts = Self.mergedMounts(
descriptor.mounts,
overrides: mounts
)
let effectivePublishedPorts = Self.mergedPublishedPorts(
descriptor.publishedPorts,
overrides: publishedPorts
)
let effectiveDeviceNodes = Self.mergedDeviceNodes(
descriptor.deviceNodes,
overrides: deviceNodes
)
        let effectiveNamespaces = Self.mergedNamespaces(
            descriptor.namespaces,
            overrides: namespaces
        )
        let effectiveNamespacePaths = Self.mergedNamespacePaths(
            descriptor.namespacePaths,
            overrides: namespacePaths
        )
        let effectiveTimeOffsets = Self.mergedTimeOffsets(
            descriptor.timeOffsets,
            overrides: timeOffsets
        )
        let effectiveUIDMappings = descriptor.uidMappings + uidMappings
        let effectiveGIDMappings = descriptor.gidMappings + gidMappings
        guard Set(effectiveNamespaces)
            .isDisjoint(with: Set(effectiveNamespacePaths.keys))
        else {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "linux.namespaces"
            )
        }
        if !effectiveTimeOffsets.isEmpty && !effectiveNamespaces.contains("time") {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "linux.timeOffsets"
            )
        }
        if (!effectiveUIDMappings.isEmpty || !effectiveGIDMappings.isEmpty)
            && !effectiveNamespaces.contains("user")
        {
            throw OrlixOCIRuntimeConfigError.unsupportedLinuxFeature(
                "linux.uidMappings"
            )
        }
let effectiveCgroupsPath = try cgroupsPath
?? descriptor.cgroupsPath
?? defaultCgroupsPath(
environmentID: descriptor.id,
				required: cgroupPidsLimit != nil
				|| cgroupCPUMax != nil
				|| cgroupCPUWeight != nil
				|| cgroupMemoryMax != nil
|| cgroupIOWeight != nil
|| !cgroupUnified.isEmpty
)
let effectiveDefaultCommand = command ?? descriptor.defaultCommand
let effectiveDefaultWorkingDirectory = workingDirectory
?? descriptor.defaultWorkingDirectory
let effectiveDefaultUserID = userID ?? descriptor.defaultUserID
let effectiveDefaultGroupID = groupID ?? descriptor.defaultGroupID
let effectiveDefaultCapabilities = capabilities ?? descriptor.defaultCapabilities
let effectiveDefaultNoNewPrivileges = noNewPrivileges
?? descriptor.defaultNoNewPrivileges
let effectiveDefaultCloseAdditionalFds = closeAdditionalFds
?? descriptor.defaultCloseAdditionalFds
let effectiveDefaultTerminal = terminal ?? descriptor.defaultTerminal
let effectiveDefaultTerminalRows = terminalRows
?? descriptor.defaultTerminalRows
let effectiveDefaultTerminalColumns = terminalColumns
?? descriptor.defaultTerminalColumns
let effectiveDefaultOOMScoreAdjustment = oomScoreAdjustment
?? descriptor.defaultOOMScoreAdjustment
let effectiveDefaultScheduler = scheduler ?? descriptor.defaultScheduler
let effectiveDefaultIOPriority = ioPriority ?? descriptor.defaultIOPriority
let effectiveDefaultCPUAffinity = cpuAffinity
?? descriptor.defaultCPUAffinity
let effectiveDefaultUmask = umask ?? descriptor.defaultUmask
let effectiveDefaultPersonalityDomain = personalityDomain
?? descriptor.defaultPersonalityDomain
let effectiveHostname = hostname ?? descriptor.hostname
let effectiveDomainname = domainname ?? descriptor.domainname
let effectiveRootReadonly = rootReadonly ?? descriptor.rootReadonly
let effectiveRootPropagation = rootPropagation ?? descriptor.rootPropagation
let effectiveCgroupPidsLimit = cgroupPidsLimit ?? descriptor.cgroupPidsLimit
let effectiveCgroupCPUMax = cgroupCPUMax ?? descriptor.cgroupCPUMax
let effectiveCgroupCPUWeight = cgroupCPUWeight ?? descriptor.cgroupCPUWeight
let effectiveCgroupMemoryMax = cgroupMemoryMax
?? descriptor.cgroupMemoryMax
let effectiveCgroupIOWeight = cgroupIOWeight ?? descriptor.cgroupIOWeight
return OrlixEnvironmentDescriptor(
id: descriptor.id,
source: descriptor.source,
platform: descriptor.platform,
rootImageIdentifier: descriptor.rootImageIdentifier,
defaultCommand: effectiveDefaultCommand,
defaultEnvironment: defaultEnvironment,
defaultWorkingDirectory: effectiveDefaultWorkingDirectory,
defaultUserID: effectiveDefaultUserID,
defaultGroupID: effectiveDefaultGroupID,
defaultSupplementaryGroups: defaultSupplementaryGroups,
defaultCapabilities: effectiveDefaultCapabilities,
defaultNoNewPrivileges: effectiveDefaultNoNewPrivileges,
defaultCloseAdditionalFds: effectiveDefaultCloseAdditionalFds,
defaultTerminal: effectiveDefaultTerminal,
defaultTerminalRows: effectiveDefaultTerminalRows,
defaultTerminalColumns: effectiveDefaultTerminalColumns,
defaultOOMScoreAdjustment: effectiveDefaultOOMScoreAdjustment,
defaultScheduler: effectiveDefaultScheduler,
defaultIOPriority: effectiveDefaultIOPriority,
defaultCPUAffinity: effectiveDefaultCPUAffinity,
defaultUmask: effectiveDefaultUmask,
defaultRlimits: defaultRlimits,
defaultPersonalityDomain: effectiveDefaultPersonalityDomain,
hostname: effectiveHostname,
domainname: effectiveDomainname,
rootMount: descriptor.rootMount,
rootReadonly: effectiveRootReadonly,
rootPropagation: effectiveRootPropagation,
sysctls: effectiveSysctls,
maskedPaths: effectiveMaskedPaths,
readonlyPaths: effectiveReadonlyPaths,
cgroupsPath: effectiveCgroupsPath,
cgroupPidsLimit: effectiveCgroupPidsLimit,
cgroupCPUMax: effectiveCgroupCPUMax,
cgroupCPUWeight: effectiveCgroupCPUWeight,
cgroupMemoryMax: effectiveCgroupMemoryMax,
cgroupIOWeight: effectiveCgroupIOWeight,
cgroupUnified: effectiveCgroupUnified,
deviceNodes: effectiveDeviceNodes,
timeOffsets: effectiveTimeOffsets,
uidMappings: effectiveUIDMappings,
gidMappings: effectiveGIDMappings,
namespaces: effectiveNamespaces,
namespacePaths: effectiveNamespacePaths,
tmpfsMounts: effectiveTmpfsMounts,
mounts: effectiveMounts,
publishedPorts: effectivePublishedPorts,
annotations: effectiveAnnotations
)
	}

	public func listPreparedEnvironments(
		fileManager: FileManager = .default
	) throws -> [OrlixOCIEnvironmentPreparedState] {
		try OrlixOCIRuntime(registry: registry)
			.listPreparedEnvironments(fileManager: fileManager)
	}

	public func listPreparedEnvironments(
		arguments: [String],
		fileManager: FileManager = .default
	) throws -> [OrlixOCIEnvironmentPreparedState] {
		let request = try OrlixOCIEnvironmentListArguments(arguments)
		let prepared = try listPreparedEnvironments(fileManager: fileManager)
		guard !request.states.isEmpty else {
			return prepared
		}
		return prepared.filter { request.states.contains($0.lifecycleState) }
	}

	public func list(
		arguments: [String],
		fileManager: FileManager = .default
	) throws -> [OrlixOCIEnvironmentPreparedState] {
		try listPreparedEnvironments(
			arguments: arguments,
			fileManager: fileManager
		)
	}

	@discardableResult
	public func install(
        bundleURL: URL,
        id: String,
		tools: OrlixOCIEnvironmentMaterializationTools,
		fileManager: FileManager = .default,
		runCommand: @escaping @Sendable (URL, [String]) throws -> Void
	) throws -> OrlixOCIEnvironmentInstallResult {
		let runtime = OrlixOCIRuntime(registry: registry)
		let created = try runtime.createMaterialized(
			bundleURL: bundleURL,
			id: id,
            mke2fsExecutable: tools.mke2fs.path,
            truncateExecutable: tools.truncate.path,
            debugfsExecutable: tools.debugfs.path,
            e2fsckExecutable: tools.e2fsck.path,
            fileManager: fileManager,
            runner: OrlixOCIEnvironmentInstallerCommandRunner(
                runCommand: runCommand
			)
		)
		return OrlixOCIEnvironmentInstallResult(
			id: id,
			bundleURL: bundleURL,
			stateReport: created.stateReport
		)
	}

	@discardableResult
    public func install(
        image: String,
        id: String,
        tools: OrlixOCIEnvironmentMaterializationTools,
		puller: OrlixOCIRegistryPuller = OrlixOCIRegistryPuller(),
		platform: String = "linux/arm64",
		defaultCommandOverride: [String]? = nil,
		defaultEnvironmentOverride: [String: String] = [:],
		defaultWorkingDirectoryOverride: String? = nil,
		defaultUserIDOverride: UInt32? = nil,
		defaultGroupIDOverride: UInt32? = nil,
		defaultSupplementaryGroupOverrides: [UInt32] = [],
		defaultCapabilitiesOverride: OrlixEnvironmentCapabilities? = nil,
		hostnameOverride: String? = nil,
		domainnameOverride: String? = nil,
		terminalOverride: Bool? = nil,
		terminalRowsOverride: UInt32? = nil,
		terminalColumnsOverride: UInt32? = nil,
		rootReadonlyOverride: Bool? = nil,
		rootPropagationOverride: OrlixEnvironmentRootPropagation? = nil,
		annotationOverrides: [String: String] = [:],
		defaultRlimitOverrides: [OrlixEnvironmentRlimit] = [],
		sysctlOverrides: [String: String] = [:],
		maskedPathOverrides: [String] = [],
		readonlyPathOverrides: [String] = [],
		defaultUmaskOverride: UInt32? = nil,
		defaultOOMScoreAdjustmentOverride: Int32? = nil,
		defaultSchedulerOverride: OrlixEnvironmentScheduler? = nil,
		defaultIOPriorityOverride: OrlixEnvironmentIOPriority? = nil,
		defaultPersonalityDomainOverride: String? = nil,
		defaultCPUAffinityOverride: OrlixEnvironmentCPUAffinity? = nil,
		noNewPrivilegesOverride: Bool? = nil,
		closeAdditionalFdsOverride: Bool? = nil,
		cgroupsPathOverride: String? = nil,
		cgroupPidsLimitOverride: Int64? = nil,
		cgroupCPUMaxOverride: OrlixEnvironmentCgroupCPUMax? = nil,
		cgroupCPUWeightOverride: UInt64? = nil,
		cgroupMemoryMaxOverride: Int64? = nil,
	cgroupIOWeightOverride: UInt64? = nil,
	cgroupUnifiedOverrides: [OrlixEnvironmentCgroupUnifiedEntry] = [],
tmpfsMountOverrides: [OrlixEnvironmentTmpfsMount] = [],
mountOverrides: [OrlixEnvironmentMount] = [],
publishedPortOverrides: [OrlixEnvironmentPublishedPort] = [],
deviceNodeOverrides: [OrlixEnvironmentDeviceNode] = [],
namespaceOverrides: [String] = [],
namespacePathOverrides: [String: String] = [:],
timeOffsetOverrides: [OrlixEnvironmentTimeOffset] = [],
uidMappingOverrides: [OrlixEnvironmentIDMapping] = [],
gidMappingOverrides: [OrlixEnvironmentIDMapping] = [],
		fileManager: FileManager = .default,
		runCommand: @escaping @Sendable (URL, [String]) throws -> Void
	) async throws -> OrlixOCIRegistryEnvironmentInstallResult {
		try await install(
			image: OrlixOCIRegistryImageReference(image),
			id: id,
            tools: tools,
			puller: puller,
			platform: platform,
			defaultCommandOverride: defaultCommandOverride,
			defaultEnvironmentOverride: defaultEnvironmentOverride,
			defaultWorkingDirectoryOverride: defaultWorkingDirectoryOverride,
			defaultUserIDOverride: defaultUserIDOverride,
			defaultGroupIDOverride: defaultGroupIDOverride,
			defaultSupplementaryGroupOverrides: defaultSupplementaryGroupOverrides,
			defaultCapabilitiesOverride: defaultCapabilitiesOverride,
			hostnameOverride: hostnameOverride,
			domainnameOverride: domainnameOverride,
			terminalOverride: terminalOverride,
			terminalRowsOverride: terminalRowsOverride,
			terminalColumnsOverride: terminalColumnsOverride,
			rootReadonlyOverride: rootReadonlyOverride,
			rootPropagationOverride: rootPropagationOverride,
			annotationOverrides: annotationOverrides,
			defaultRlimitOverrides: defaultRlimitOverrides,
			sysctlOverrides: sysctlOverrides,
			maskedPathOverrides: maskedPathOverrides,
			readonlyPathOverrides: readonlyPathOverrides,
			defaultUmaskOverride: defaultUmaskOverride,
			defaultOOMScoreAdjustmentOverride: defaultOOMScoreAdjustmentOverride,
			defaultSchedulerOverride: defaultSchedulerOverride,
			defaultIOPriorityOverride: defaultIOPriorityOverride,
			defaultPersonalityDomainOverride: defaultPersonalityDomainOverride,
			defaultCPUAffinityOverride: defaultCPUAffinityOverride,
			noNewPrivilegesOverride: noNewPrivilegesOverride,
			closeAdditionalFdsOverride: closeAdditionalFdsOverride,
			cgroupsPathOverride: cgroupsPathOverride,
			cgroupPidsLimitOverride: cgroupPidsLimitOverride,
			cgroupCPUMaxOverride: cgroupCPUMaxOverride,
			cgroupCPUWeightOverride: cgroupCPUWeightOverride,
			cgroupMemoryMaxOverride: cgroupMemoryMaxOverride,
	cgroupIOWeightOverride: cgroupIOWeightOverride,
	cgroupUnifiedOverrides: cgroupUnifiedOverrides,
tmpfsMountOverrides: tmpfsMountOverrides,
mountOverrides: mountOverrides,
publishedPortOverrides: publishedPortOverrides,
deviceNodeOverrides: deviceNodeOverrides,
namespaceOverrides: namespaceOverrides,
namespacePathOverrides: namespacePathOverrides,
timeOffsetOverrides: timeOffsetOverrides,
uidMappingOverrides: uidMappingOverrides,
gidMappingOverrides: gidMappingOverrides,
			fileManager: fileManager,
			runCommand: runCommand
		)
	}

	@discardableResult
    public func install(
        image: OrlixOCIRegistryImageReference,
        id: String,
        tools: OrlixOCIEnvironmentMaterializationTools,
		puller: OrlixOCIRegistryPuller = OrlixOCIRegistryPuller(),
		platform: String = "linux/arm64",
		defaultCommandOverride: [String]? = nil,
		defaultEnvironmentOverride: [String: String] = [:],
		defaultWorkingDirectoryOverride: String? = nil,
		defaultUserIDOverride: UInt32? = nil,
		defaultGroupIDOverride: UInt32? = nil,
		defaultSupplementaryGroupOverrides: [UInt32] = [],
		defaultCapabilitiesOverride: OrlixEnvironmentCapabilities? = nil,
		hostnameOverride: String? = nil,
		domainnameOverride: String? = nil,
		terminalOverride: Bool? = nil,
		terminalRowsOverride: UInt32? = nil,
		terminalColumnsOverride: UInt32? = nil,
		rootReadonlyOverride: Bool? = nil,
		rootPropagationOverride: OrlixEnvironmentRootPropagation? = nil,
		annotationOverrides: [String: String] = [:],
		defaultRlimitOverrides: [OrlixEnvironmentRlimit] = [],
		sysctlOverrides: [String: String] = [:],
		maskedPathOverrides: [String] = [],
		readonlyPathOverrides: [String] = [],
		defaultUmaskOverride: UInt32? = nil,
		defaultOOMScoreAdjustmentOverride: Int32? = nil,
		defaultSchedulerOverride: OrlixEnvironmentScheduler? = nil,
		defaultIOPriorityOverride: OrlixEnvironmentIOPriority? = nil,
		defaultPersonalityDomainOverride: String? = nil,
		defaultCPUAffinityOverride: OrlixEnvironmentCPUAffinity? = nil,
		noNewPrivilegesOverride: Bool? = nil,
		closeAdditionalFdsOverride: Bool? = nil,
		cgroupsPathOverride: String? = nil,
		cgroupPidsLimitOverride: Int64? = nil,
		cgroupCPUMaxOverride: OrlixEnvironmentCgroupCPUMax? = nil,
		cgroupCPUWeightOverride: UInt64? = nil,
		cgroupMemoryMaxOverride: Int64? = nil,
	cgroupIOWeightOverride: UInt64? = nil,
	cgroupUnifiedOverrides: [OrlixEnvironmentCgroupUnifiedEntry] = [],
tmpfsMountOverrides: [OrlixEnvironmentTmpfsMount] = [],
mountOverrides: [OrlixEnvironmentMount] = [],
publishedPortOverrides: [OrlixEnvironmentPublishedPort] = [],
deviceNodeOverrides: [OrlixEnvironmentDeviceNode] = [],
        namespaceOverrides: [String] = [],
        namespacePathOverrides: [String: String] = [:],
        timeOffsetOverrides: [OrlixEnvironmentTimeOffset] = [],
        uidMappingOverrides: [OrlixEnvironmentIDMapping] = [],
        gidMappingOverrides: [OrlixEnvironmentIDMapping] = [],
		fileManager: FileManager = .default,
		runCommand: @escaping @Sendable (URL, [String]) throws -> Void
	) async throws -> OrlixOCIRegistryEnvironmentInstallResult {
		let layout = try registry.layout(forEnvironmentID: id)
		let pulledLayoutURL = layout.importScratchDirectory
			.appendingPathComponent("registry-layout", isDirectory: true)
		if fileManager.fileExists(atPath: layout.rootDirectory.path) {
			throw OrlixOCIImageLayoutError.destinationExists(id)
		}
		if fileManager.fileExists(atPath: layout.importScratchDirectory.path) {
			try fileManager.removeItem(at: layout.importScratchDirectory)
		}
		try fileManager.createDirectory(
			at: layout.importScratchDirectory,
			withIntermediateDirectories: true
		)
		do {
			let pullResult = try await puller.pull(
				image,
				to: pulledLayoutURL,
				platform: platform,
				fileManager: fileManager
			)
            let importResult = try OrlixOCIImageLayoutImporter().importLayout(
                at: pulledLayoutURL,
                environmentID: id,
                registry: registry,
                rootImageIdentifier: "orlix.env.\(id)",
                platform: platform,
                fileManager: fileManager
            )
			let descriptor = try Self.descriptor(
				importResult.descriptor,
				replacingDefaultCommandWith: defaultCommandOverride,
				mergingDefaultEnvironmentWith: defaultEnvironmentOverride,
				replacingDefaultWorkingDirectoryWith:
					defaultWorkingDirectoryOverride,
				replacingDefaultUserIDWith: defaultUserIDOverride,
				replacingDefaultGroupIDWith: defaultGroupIDOverride,
				mergingDefaultSupplementaryGroupsWith:
					defaultSupplementaryGroupOverrides,
				replacingDefaultCapabilitiesWith: defaultCapabilitiesOverride,
			replacingHostnameWith: hostnameOverride,
			replacingDomainnameWith: domainnameOverride,
			replacingDefaultTerminalWith: terminalOverride,
			replacingDefaultTerminalRowsWith: terminalRowsOverride,
			replacingDefaultTerminalColumnsWith: terminalColumnsOverride,
			replacingRootReadonlyWith: rootReadonlyOverride,
			replacingRootPropagationWith: rootPropagationOverride,
			mergingAnnotationsWith: annotationOverrides,
			mergingDefaultRlimitsWith: defaultRlimitOverrides,
			mergingSysctlsWith: sysctlOverrides,
			mergingMaskedPathsWith: maskedPathOverrides,
			mergingReadonlyPathsWith: readonlyPathOverrides,
				replacingDefaultUmaskWith: defaultUmaskOverride,
				replacingDefaultOOMScoreAdjustmentWith:
					defaultOOMScoreAdjustmentOverride,
				replacingDefaultSchedulerWith: defaultSchedulerOverride,
				replacingDefaultIOPriorityWith: defaultIOPriorityOverride,
				replacingDefaultCPUAffinityWith: defaultCPUAffinityOverride,
				replacingDefaultPersonalityDomainWith:
					defaultPersonalityDomainOverride,
				replacingDefaultNoNewPrivilegesWith: noNewPrivilegesOverride,
				replacingDefaultCloseAdditionalFdsWith:
					closeAdditionalFdsOverride,
				replacingCgroupsPathWith: cgroupsPathOverride,
				replacingCgroupPidsLimitWith: cgroupPidsLimitOverride,
				replacingCgroupCPUMaxWith: cgroupCPUMaxOverride,
				replacingCgroupCPUWeightWith: cgroupCPUWeightOverride,
				replacingCgroupMemoryMaxWith: cgroupMemoryMaxOverride,
	replacingCgroupIOWeightWith: cgroupIOWeightOverride,
	mergingCgroupUnifiedWith: cgroupUnifiedOverrides,
mergingTmpfsMountsWith: tmpfsMountOverrides,
mergingMountsWith: mountOverrides,
mergingPublishedPortsWith: publishedPortOverrides,
mergingDeviceNodesWith: deviceNodeOverrides,
            mergingNamespacesWith: namespaceOverrides,
            mergingNamespacePathsWith: namespacePathOverrides,
            mergingTimeOffsetsWith: timeOffsetOverrides,
            appendingUIDMappings: uidMappingOverrides,
            appendingGIDMappings: gidMappingOverrides
		)
            if descriptor != importResult.descriptor {
                try registry.save(descriptor, fileManager: fileManager)
            }
        _ = try importResult.materializationPlan.materialize(
            mke2fsExecutable: tools.mke2fs.path,
            truncateExecutable: tools.truncate.path,
            debugfsExecutable: tools.debugfs.path,
            e2fsckExecutable: tools.e2fsck.path,
            runner: OrlixOCIEnvironmentInstallerCommandRunner(
                runCommand: runCommand
            )
			)
            let config = try registryLifecycleConfig(for: descriptor)
			let lifecycleStore = OrlixOCIRuntimeLifecycleStore(registry: registry)
			let lifecycle = try OrlixOCIRuntimeLifecycleController(
				config: config,
				id: id,
				bundlePath: "oci://\(image.registry)/\(image.repository)@\(pullResult.manifestDigest)"
			).create()
			try lifecycleStore.save(lifecycle, fileManager: fileManager)
			return OrlixOCIRegistryEnvironmentInstallResult(
				id: id,
				image: image,
				pullResult: pullResult,
				rootfsImport: OrlixOCIRegistryRootfsImportReport(
					stagingRootDirectory: importResult.stagingRootDirectory,
					baseTreeDirectory: importResult.materializationPlan.baseTreeDirectory,
					layerDigests: importResult.image.layers.map(\.digest)
				),
				stateReport: try lifecycleStore.stateReport(
					id: id,
					fileManager: fileManager
				)
			)
		} catch {
			try? registry.delete(environmentID: id, fileManager: fileManager)
			throw error
		}
	}

	@discardableResult
	public func run(
		bundleURL: URL,
		id: String,
		tools: OrlixOCIEnvironmentMaterializationTools,
		command: [String]? = nil,
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		observationTimeout: TimeInterval = 600,
		fileManager: FileManager = .default,
		runCommand: @escaping @Sendable (URL, [String]) throws -> Void
	) throws -> OrlixOCIEnvironmentInstallRunResult {
		let installResult = try install(
			bundleURL: bundleURL,
			id: id,
			tools: tools,
			fileManager: fileManager,
			runCommand: runCommand
		)
		let runResult = try run(
			id: id,
			command: command,
			terminal: terminal,
			observationTimeout: observationTimeout,
			fileManager: fileManager
		)
		return OrlixOCIEnvironmentInstallRunResult(
			installResult: installResult,
			runResult: runResult
		)
	}

	@discardableResult
	public func run(
		arguments: [String],
		tools: OrlixOCIEnvironmentMaterializationTools,
		puller: OrlixOCIRegistryPuller = OrlixOCIRegistryPuller(),
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		observationTimeout: TimeInterval = 600,
		fileManager: FileManager = .default,
		runCommand: @escaping @Sendable (URL, [String]) throws -> Void
	) async throws -> OrlixOCIRegistryEnvironmentInstallRunResult {
		let request = try OrlixOCIEnvironmentRunArguments(arguments)
		let result = try await run(
			image: request.image,
			id: request.id,
            tools: tools,
			puller: puller,
			platform: request.platform,
			command: request.resolvedCommandOverride,
			defaultCommandOverride: request.resolvedCommandOverride,
			defaultEnvironmentOverride: request.environment,
			defaultWorkingDirectoryOverride: request.workingDirectory,
			defaultUserIDOverride: request.userID,
			defaultGroupIDOverride: request.groupID,
			defaultSupplementaryGroupOverrides:
				request.supplementaryGroupIDs,
			defaultCapabilitiesOverride: request.capabilities,
			hostnameOverride: request.hostname,
			domainnameOverride: request.domainname,
			terminalOverride: request.terminal,
			terminalRowsOverride: request.terminalRows,
			terminalColumnsOverride: request.terminalColumns,
			rootReadonlyOverride: request.rootReadonly,
			rootPropagationOverride: request.rootPropagation,
			annotationOverrides: request.annotations,
			defaultRlimitOverrides: request.rlimits,
			sysctlOverrides: request.sysctls,
			maskedPathOverrides: request.maskedPaths,
			readonlyPathOverrides: request.readonlyPaths,
			defaultUmaskOverride: request.umask,
			defaultOOMScoreAdjustmentOverride: request.oomScoreAdjustment,
			defaultSchedulerOverride: request.scheduler,
			defaultIOPriorityOverride: request.ioPriority,
			defaultPersonalityDomainOverride: request.personalityDomain,
			defaultCPUAffinityOverride: request.cpuAffinity,
			noNewPrivilegesOverride: request.noNewPrivileges,
			closeAdditionalFdsOverride: request.closeAdditionalFds,
			cgroupsPathOverride: request.cgroupsPath,
			cgroupPidsLimitOverride: request.cgroupPidsLimit,
			cgroupCPUMaxOverride: request.cgroupCPUMax,
			cgroupCPUWeightOverride: request.cgroupCPUWeight,
			cgroupMemoryMaxOverride: request.cgroupMemoryMax,
	cgroupIOWeightOverride: request.cgroupIOWeight,
	cgroupUnifiedOverrides: request.cgroupUnified,
	tmpfsMountOverrides: request.tmpfsMounts,
	mountOverrides: request.mounts,
	publishedPortOverrides: request.publishedPorts,
	deviceNodeOverrides: request.deviceNodes,
            namespaceOverrides: request.namespaces,
            namespacePathOverrides: request.namespacePaths,
            timeOffsetOverrides: request.timeOffsets,
            uidMappingOverrides: request.uidMappings,
            gidMappingOverrides: request.gidMappings,
            terminal: terminal,
			observationTimeout: observationTimeout,
			fileManager: fileManager,
			runCommand: runCommand
		)
		guard request.removeAfterRun else {
			return result
		}
		let deleteResult = try delete(id: request.id, fileManager: fileManager)
		return OrlixOCIRegistryEnvironmentInstallRunResult(
			installResult: result.installResult,
			runResult: result.runResult,
			deleteResult: deleteResult
		)
	}

	@discardableResult
	@_spi(OrlixPrivateTesting)
	public func prepareTerminalSession(
		arguments: [String],
		tools: OrlixOCIEnvironmentMaterializationTools,
		puller: OrlixOCIRegistryPuller = OrlixOCIRegistryPuller(),
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		fileManager: FileManager = .default,
		runCommand: @escaping @Sendable (URL, [String]) throws -> Void
	) async throws -> OrlixOCIRegistryEnvironmentTerminalSessionResult {
		let request = try OrlixOCIEnvironmentRunArguments(arguments)
		if request.removeAfterRun {
			throw OrlixOCIEnvironmentRunArgumentsError
				.removeAfterRunRequiresObservedRun
		}
		return try await prepareTerminalSession(
			image: try OrlixOCIRegistryImageReference(request.image),
			id: request.id,
			tools: tools,
			puller: puller,
			platform: request.platform,
			command: request.resolvedCommandOverride,
			defaultCommandOverride: request.resolvedCommandOverride,
			defaultEnvironmentOverride: request.environment,
			defaultWorkingDirectoryOverride: request.workingDirectory,
			defaultUserIDOverride: request.userID,
			defaultGroupIDOverride: request.groupID,
			defaultSupplementaryGroupOverrides:
				request.supplementaryGroupIDs,
			defaultCapabilitiesOverride: request.capabilities,
			hostnameOverride: request.hostname,
			domainnameOverride: request.domainname,
			terminalOverride: request.terminal,
			terminalRowsOverride: request.terminalRows,
			terminalColumnsOverride: request.terminalColumns,
			rootReadonlyOverride: request.rootReadonly,
			rootPropagationOverride: request.rootPropagation,
			annotationOverrides: request.annotations,
			defaultRlimitOverrides: request.rlimits,
			sysctlOverrides: request.sysctls,
			maskedPathOverrides: request.maskedPaths,
			readonlyPathOverrides: request.readonlyPaths,
			defaultUmaskOverride: request.umask,
			defaultOOMScoreAdjustmentOverride: request.oomScoreAdjustment,
			defaultSchedulerOverride: request.scheduler,
			defaultIOPriorityOverride: request.ioPriority,
			defaultPersonalityDomainOverride: request.personalityDomain,
			defaultCPUAffinityOverride: request.cpuAffinity,
			noNewPrivilegesOverride: request.noNewPrivileges,
			closeAdditionalFdsOverride: request.closeAdditionalFds,
			cgroupsPathOverride: request.cgroupsPath,
			cgroupPidsLimitOverride: request.cgroupPidsLimit,
			cgroupCPUMaxOverride: request.cgroupCPUMax,
			cgroupCPUWeightOverride: request.cgroupCPUWeight,
			cgroupMemoryMaxOverride: request.cgroupMemoryMax,
	cgroupIOWeightOverride: request.cgroupIOWeight,
	cgroupUnifiedOverrides: request.cgroupUnified,
	tmpfsMountOverrides: request.tmpfsMounts,
	mountOverrides: request.mounts,
	publishedPortOverrides: request.publishedPorts,
	deviceNodeOverrides: request.deviceNodes,
            namespaceOverrides: request.namespaces,
            namespacePathOverrides: request.namespacePaths,
            timeOffsetOverrides: request.timeOffsets,
            uidMappingOverrides: request.uidMappings,
            gidMappingOverrides: request.gidMappings,
            terminal: terminal,
			fileManager: fileManager,
			runCommand: runCommand
		)
	}

	@_spi(OrlixPrivateTesting)
	public func terminalSession(
		arguments: [String],
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		fileManager: FileManager = .default
	) throws -> OrlixOCIEnvironmentTerminalSessionResult {
		let request = try OrlixOCIEnvironmentRunArguments(arguments)
		if request.removeAfterRun {
			throw OrlixOCIEnvironmentRunArgumentsError
				.removeAfterRunRequiresObservedRun
		}
		let image = try OrlixOCIRegistryImageReference(request.image)
		let kernelSession = try OrlixOCIRuntime(registry: registry).kernelSession(
            id: request.id,
            command: request.resolvedCommandOverride,
            terminal: terminal,
            fileManager: fileManager
        )
		return OrlixOCIEnvironmentTerminalSessionResult(
            id: request.id,
            image: image,
            command: request.resolvedCommandOverride,
            kernelSession: kernelSession
        )
	}

	@discardableResult
	@_spi(OrlixPrivateTesting)
	public func prepareTerminalSession(
		image: String,
		id: String,
		tools: OrlixOCIEnvironmentMaterializationTools,
		puller: OrlixOCIRegistryPuller = OrlixOCIRegistryPuller(),
		platform: String = "linux/arm64",
		command: [String]? = nil,
		defaultCommandOverride: [String]? = nil,
		defaultEnvironmentOverride: [String: String] = [:],
		defaultWorkingDirectoryOverride: String? = nil,
		defaultUserIDOverride: UInt32? = nil,
		defaultGroupIDOverride: UInt32? = nil,
		defaultSupplementaryGroupOverrides: [UInt32] = [],
		defaultCapabilitiesOverride: OrlixEnvironmentCapabilities? = nil,
		hostnameOverride: String? = nil,
		domainnameOverride: String? = nil,
		terminalOverride: Bool? = nil,
		terminalRowsOverride: UInt32? = nil,
		terminalColumnsOverride: UInt32? = nil,
		rootReadonlyOverride: Bool? = nil,
		rootPropagationOverride: OrlixEnvironmentRootPropagation? = nil,
		annotationOverrides: [String: String] = [:],
		defaultRlimitOverrides: [OrlixEnvironmentRlimit] = [],
		sysctlOverrides: [String: String] = [:],
		maskedPathOverrides: [String] = [],
		readonlyPathOverrides: [String] = [],
		defaultUmaskOverride: UInt32? = nil,
		defaultOOMScoreAdjustmentOverride: Int32? = nil,
		defaultSchedulerOverride: OrlixEnvironmentScheduler? = nil,
		defaultIOPriorityOverride: OrlixEnvironmentIOPriority? = nil,
		defaultPersonalityDomainOverride: String? = nil,
		defaultCPUAffinityOverride: OrlixEnvironmentCPUAffinity? = nil,
		noNewPrivilegesOverride: Bool? = nil,
		closeAdditionalFdsOverride: Bool? = nil,
		cgroupsPathOverride: String? = nil,
		cgroupPidsLimitOverride: Int64? = nil,
		cgroupCPUMaxOverride: OrlixEnvironmentCgroupCPUMax? = nil,
		cgroupCPUWeightOverride: UInt64? = nil,
		cgroupMemoryMaxOverride: Int64? = nil,
		cgroupIOWeightOverride: UInt64? = nil,
	cgroupUnifiedOverrides: [OrlixEnvironmentCgroupUnifiedEntry] = [],
tmpfsMountOverrides: [OrlixEnvironmentTmpfsMount] = [],
mountOverrides: [OrlixEnvironmentMount] = [],
publishedPortOverrides: [OrlixEnvironmentPublishedPort] = [],
deviceNodeOverrides: [OrlixEnvironmentDeviceNode] = [],
        namespaceOverrides: [String] = [],
        namespacePathOverrides: [String: String] = [:],
        timeOffsetOverrides: [OrlixEnvironmentTimeOffset] = [],
        uidMappingOverrides: [OrlixEnvironmentIDMapping] = [],
        gidMappingOverrides: [OrlixEnvironmentIDMapping] = [],
        terminal: OrlixTerminalSession = OrlixTerminalSession(),
		fileManager: FileManager = .default,
		runCommand: @escaping @Sendable (URL, [String]) throws -> Void
	) async throws -> OrlixOCIRegistryEnvironmentTerminalSessionResult {
		try await prepareTerminalSession(
			image: OrlixOCIRegistryImageReference(image),
			id: id,
			tools: tools,
			puller: puller,
			platform: platform,
			command: command,
			defaultCommandOverride: defaultCommandOverride,
			defaultEnvironmentOverride: defaultEnvironmentOverride,
			defaultWorkingDirectoryOverride: defaultWorkingDirectoryOverride,
			defaultUserIDOverride: defaultUserIDOverride,
			defaultGroupIDOverride: defaultGroupIDOverride,
			defaultSupplementaryGroupOverrides: defaultSupplementaryGroupOverrides,
			defaultCapabilitiesOverride: defaultCapabilitiesOverride,
			hostnameOverride: hostnameOverride,
			domainnameOverride: domainnameOverride,
			terminalOverride: terminalOverride,
			terminalRowsOverride: terminalRowsOverride,
			terminalColumnsOverride: terminalColumnsOverride,
			rootReadonlyOverride: rootReadonlyOverride,
			rootPropagationOverride: rootPropagationOverride,
			annotationOverrides: annotationOverrides,
			defaultRlimitOverrides: defaultRlimitOverrides,
			sysctlOverrides: sysctlOverrides,
			maskedPathOverrides: maskedPathOverrides,
			readonlyPathOverrides: readonlyPathOverrides,
			defaultUmaskOverride: defaultUmaskOverride,
			defaultOOMScoreAdjustmentOverride: defaultOOMScoreAdjustmentOverride,
			defaultSchedulerOverride: defaultSchedulerOverride,
			defaultIOPriorityOverride: defaultIOPriorityOverride,
			defaultPersonalityDomainOverride: defaultPersonalityDomainOverride,
			defaultCPUAffinityOverride: defaultCPUAffinityOverride,
			noNewPrivilegesOverride: noNewPrivilegesOverride,
			closeAdditionalFdsOverride: closeAdditionalFdsOverride,
			cgroupsPathOverride: cgroupsPathOverride,
			cgroupPidsLimitOverride: cgroupPidsLimitOverride,
			cgroupCPUMaxOverride: cgroupCPUMaxOverride,
			cgroupCPUWeightOverride: cgroupCPUWeightOverride,
			cgroupMemoryMaxOverride: cgroupMemoryMaxOverride,
			cgroupIOWeightOverride: cgroupIOWeightOverride,
	cgroupUnifiedOverrides: cgroupUnifiedOverrides,
tmpfsMountOverrides: tmpfsMountOverrides,
mountOverrides: mountOverrides,
publishedPortOverrides: publishedPortOverrides,
deviceNodeOverrides: deviceNodeOverrides,
            namespaceOverrides: namespaceOverrides,
            namespacePathOverrides: namespacePathOverrides,
            timeOffsetOverrides: timeOffsetOverrides,
            uidMappingOverrides: uidMappingOverrides,
            gidMappingOverrides: gidMappingOverrides,
            terminal: terminal,
			fileManager: fileManager,
			runCommand: runCommand
		)
	}

	@discardableResult
	@_spi(OrlixPrivateTesting)
	public func prepareTerminalSession(
		image: OrlixOCIRegistryImageReference,
		id: String,
		tools: OrlixOCIEnvironmentMaterializationTools,
		puller: OrlixOCIRegistryPuller = OrlixOCIRegistryPuller(),
		platform: String = "linux/arm64",
		command: [String]? = nil,
		defaultCommandOverride: [String]? = nil,
		defaultEnvironmentOverride: [String: String] = [:],
		defaultWorkingDirectoryOverride: String? = nil,
		defaultUserIDOverride: UInt32? = nil,
		defaultGroupIDOverride: UInt32? = nil,
		defaultSupplementaryGroupOverrides: [UInt32] = [],
		defaultCapabilitiesOverride: OrlixEnvironmentCapabilities? = nil,
		hostnameOverride: String? = nil,
		domainnameOverride: String? = nil,
		terminalOverride: Bool? = nil,
		terminalRowsOverride: UInt32? = nil,
		terminalColumnsOverride: UInt32? = nil,
		rootReadonlyOverride: Bool? = nil,
		rootPropagationOverride: OrlixEnvironmentRootPropagation? = nil,
		annotationOverrides: [String: String] = [:],
		defaultRlimitOverrides: [OrlixEnvironmentRlimit] = [],
		sysctlOverrides: [String: String] = [:],
		maskedPathOverrides: [String] = [],
		readonlyPathOverrides: [String] = [],
		defaultUmaskOverride: UInt32? = nil,
		defaultOOMScoreAdjustmentOverride: Int32? = nil,
		defaultSchedulerOverride: OrlixEnvironmentScheduler? = nil,
		defaultIOPriorityOverride: OrlixEnvironmentIOPriority? = nil,
		defaultPersonalityDomainOverride: String? = nil,
		defaultCPUAffinityOverride: OrlixEnvironmentCPUAffinity? = nil,
		noNewPrivilegesOverride: Bool? = nil,
		closeAdditionalFdsOverride: Bool? = nil,
		cgroupsPathOverride: String? = nil,
		cgroupPidsLimitOverride: Int64? = nil,
		cgroupCPUMaxOverride: OrlixEnvironmentCgroupCPUMax? = nil,
		cgroupCPUWeightOverride: UInt64? = nil,
		cgroupMemoryMaxOverride: Int64? = nil,
		cgroupIOWeightOverride: UInt64? = nil,
	cgroupUnifiedOverrides: [OrlixEnvironmentCgroupUnifiedEntry] = [],
	tmpfsMountOverrides: [OrlixEnvironmentTmpfsMount] = [],
	mountOverrides: [OrlixEnvironmentMount] = [],
	publishedPortOverrides: [OrlixEnvironmentPublishedPort] = [],
	deviceNodeOverrides: [OrlixEnvironmentDeviceNode] = [],
namespaceOverrides: [String] = [],
namespacePathOverrides: [String: String] = [:],
timeOffsetOverrides: [OrlixEnvironmentTimeOffset] = [],
uidMappingOverrides: [OrlixEnvironmentIDMapping] = [],
gidMappingOverrides: [OrlixEnvironmentIDMapping] = [],
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		fileManager: FileManager = .default,
		runCommand: @escaping @Sendable (URL, [String]) throws -> Void
	) async throws -> OrlixOCIRegistryEnvironmentTerminalSessionResult {
		let installResult = try await install(
			image: image,
			id: id,
            tools: tools,
			puller: puller,
			platform: platform,
			defaultCommandOverride: defaultCommandOverride,
			defaultEnvironmentOverride: defaultEnvironmentOverride,
			defaultWorkingDirectoryOverride: defaultWorkingDirectoryOverride,
			defaultUserIDOverride: defaultUserIDOverride,
			defaultGroupIDOverride: defaultGroupIDOverride,
			defaultSupplementaryGroupOverrides: defaultSupplementaryGroupOverrides,
			defaultCapabilitiesOverride: defaultCapabilitiesOverride,
			hostnameOverride: hostnameOverride,
			domainnameOverride: domainnameOverride,
			terminalOverride: terminalOverride,
			terminalRowsOverride: terminalRowsOverride,
			terminalColumnsOverride: terminalColumnsOverride,
			rootReadonlyOverride: rootReadonlyOverride,
			rootPropagationOverride: rootPropagationOverride,
			annotationOverrides: annotationOverrides,
			defaultRlimitOverrides: defaultRlimitOverrides,
			sysctlOverrides: sysctlOverrides,
			maskedPathOverrides: maskedPathOverrides,
			readonlyPathOverrides: readonlyPathOverrides,
			defaultUmaskOverride: defaultUmaskOverride,
			defaultOOMScoreAdjustmentOverride: defaultOOMScoreAdjustmentOverride,
			defaultSchedulerOverride: defaultSchedulerOverride,
			defaultIOPriorityOverride: defaultIOPriorityOverride,
			defaultPersonalityDomainOverride: defaultPersonalityDomainOverride,
			defaultCPUAffinityOverride: defaultCPUAffinityOverride,
			noNewPrivilegesOverride: noNewPrivilegesOverride,
			closeAdditionalFdsOverride: closeAdditionalFdsOverride,
			cgroupsPathOverride: cgroupsPathOverride,
			cgroupPidsLimitOverride: cgroupPidsLimitOverride,
			cgroupCPUMaxOverride: cgroupCPUMaxOverride,
			cgroupCPUWeightOverride: cgroupCPUWeightOverride,
			cgroupMemoryMaxOverride: cgroupMemoryMaxOverride,
			cgroupIOWeightOverride: cgroupIOWeightOverride,
	cgroupUnifiedOverrides: cgroupUnifiedOverrides,
	tmpfsMountOverrides: tmpfsMountOverrides,
	mountOverrides: mountOverrides,
	publishedPortOverrides: publishedPortOverrides,
	deviceNodeOverrides: deviceNodeOverrides,
namespaceOverrides: namespaceOverrides,
namespacePathOverrides: namespacePathOverrides,
timeOffsetOverrides: timeOffsetOverrides,
uidMappingOverrides: uidMappingOverrides,
gidMappingOverrides: gidMappingOverrides,
fileManager: fileManager,
			runCommand: runCommand
		)
		let kernelSession = try OrlixOCIRuntime(registry: registry).kernelSession(
			id: id,
			command: command,
			terminal: terminal,
			fileManager: fileManager
		)
		return OrlixOCIRegistryEnvironmentTerminalSessionResult(
			installResult: installResult,
			kernelSession: kernelSession
		)
	}

	@discardableResult
	public func run(
		image: String,
		id: String,
		tools: OrlixOCIEnvironmentMaterializationTools,
        puller: OrlixOCIRegistryPuller = OrlixOCIRegistryPuller(),
		platform: String = "linux/arm64",
		command: [String]? = nil,
		defaultCommandOverride: [String]? = nil,
		defaultEnvironmentOverride: [String: String] = [:],
		defaultWorkingDirectoryOverride: String? = nil,
		defaultUserIDOverride: UInt32? = nil,
		defaultGroupIDOverride: UInt32? = nil,
		defaultSupplementaryGroupOverrides: [UInt32] = [],
		defaultCapabilitiesOverride: OrlixEnvironmentCapabilities? = nil,
		hostnameOverride: String? = nil,
		domainnameOverride: String? = nil,
		terminalOverride: Bool? = nil,
		terminalRowsOverride: UInt32? = nil,
		terminalColumnsOverride: UInt32? = nil,
		rootReadonlyOverride: Bool? = nil,
		rootPropagationOverride: OrlixEnvironmentRootPropagation? = nil,
		annotationOverrides: [String: String] = [:],
		defaultRlimitOverrides: [OrlixEnvironmentRlimit] = [],
		sysctlOverrides: [String: String] = [:],
		maskedPathOverrides: [String] = [],
		readonlyPathOverrides: [String] = [],
		defaultUmaskOverride: UInt32? = nil,
		defaultOOMScoreAdjustmentOverride: Int32? = nil,
		defaultSchedulerOverride: OrlixEnvironmentScheduler? = nil,
		defaultIOPriorityOverride: OrlixEnvironmentIOPriority? = nil,
		defaultPersonalityDomainOverride: String? = nil,
		defaultCPUAffinityOverride: OrlixEnvironmentCPUAffinity? = nil,
		noNewPrivilegesOverride: Bool? = nil,
		closeAdditionalFdsOverride: Bool? = nil,
		cgroupsPathOverride: String? = nil,
		cgroupPidsLimitOverride: Int64? = nil,
		cgroupCPUMaxOverride: OrlixEnvironmentCgroupCPUMax? = nil,
		cgroupCPUWeightOverride: UInt64? = nil,
		cgroupMemoryMaxOverride: Int64? = nil,
		cgroupIOWeightOverride: UInt64? = nil,
	cgroupUnifiedOverrides: [OrlixEnvironmentCgroupUnifiedEntry] = [],
	tmpfsMountOverrides: [OrlixEnvironmentTmpfsMount] = [],
	mountOverrides: [OrlixEnvironmentMount] = [],
	publishedPortOverrides: [OrlixEnvironmentPublishedPort] = [],
	deviceNodeOverrides: [OrlixEnvironmentDeviceNode] = [],
namespaceOverrides: [String] = [],
namespacePathOverrides: [String: String] = [:],
timeOffsetOverrides: [OrlixEnvironmentTimeOffset] = [],
uidMappingOverrides: [OrlixEnvironmentIDMapping] = [],
gidMappingOverrides: [OrlixEnvironmentIDMapping] = [],
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		observationTimeout: TimeInterval = 600,
		fileManager: FileManager = .default,
		runCommand: @escaping @Sendable (URL, [String]) throws -> Void
	) async throws -> OrlixOCIRegistryEnvironmentInstallRunResult {
		try await run(
			image: OrlixOCIRegistryImageReference(image),
			id: id,
			tools: tools,
            puller: puller,
			platform: platform,
			command: command,
			defaultCommandOverride: defaultCommandOverride,
			defaultEnvironmentOverride: defaultEnvironmentOverride,
			defaultWorkingDirectoryOverride: defaultWorkingDirectoryOverride,
			defaultUserIDOverride: defaultUserIDOverride,
			defaultGroupIDOverride: defaultGroupIDOverride,
			defaultSupplementaryGroupOverrides: defaultSupplementaryGroupOverrides,
			defaultCapabilitiesOverride: defaultCapabilitiesOverride,
			hostnameOverride: hostnameOverride,
			domainnameOverride: domainnameOverride,
			terminalOverride: terminalOverride,
			terminalRowsOverride: terminalRowsOverride,
			terminalColumnsOverride: terminalColumnsOverride,
			rootReadonlyOverride: rootReadonlyOverride,
			rootPropagationOverride: rootPropagationOverride,
			annotationOverrides: annotationOverrides,
			defaultRlimitOverrides: defaultRlimitOverrides,
			sysctlOverrides: sysctlOverrides,
			maskedPathOverrides: maskedPathOverrides,
			readonlyPathOverrides: readonlyPathOverrides,
			defaultUmaskOverride: defaultUmaskOverride,
			defaultOOMScoreAdjustmentOverride: defaultOOMScoreAdjustmentOverride,
			defaultSchedulerOverride: defaultSchedulerOverride,
			defaultIOPriorityOverride: defaultIOPriorityOverride,
			defaultPersonalityDomainOverride: defaultPersonalityDomainOverride,
			defaultCPUAffinityOverride: defaultCPUAffinityOverride,
			noNewPrivilegesOverride: noNewPrivilegesOverride,
			closeAdditionalFdsOverride: closeAdditionalFdsOverride,
			cgroupsPathOverride: cgroupsPathOverride,
			cgroupPidsLimitOverride: cgroupPidsLimitOverride,
			cgroupCPUMaxOverride: cgroupCPUMaxOverride,
			cgroupCPUWeightOverride: cgroupCPUWeightOverride,
			cgroupMemoryMaxOverride: cgroupMemoryMaxOverride,
			cgroupIOWeightOverride: cgroupIOWeightOverride,
	cgroupUnifiedOverrides: cgroupUnifiedOverrides,
tmpfsMountOverrides: tmpfsMountOverrides,
mountOverrides: mountOverrides,
publishedPortOverrides: publishedPortOverrides,
deviceNodeOverrides: deviceNodeOverrides,
            namespaceOverrides: namespaceOverrides,
            namespacePathOverrides: namespacePathOverrides,
            timeOffsetOverrides: timeOffsetOverrides,
            uidMappingOverrides: uidMappingOverrides,
            gidMappingOverrides: gidMappingOverrides,
            terminal: terminal,
			observationTimeout: observationTimeout,
			fileManager: fileManager,
			runCommand: runCommand
		)
	}

	@discardableResult
	public func run(
		image: OrlixOCIRegistryImageReference,
		id: String,
		tools: OrlixOCIEnvironmentMaterializationTools,
        puller: OrlixOCIRegistryPuller = OrlixOCIRegistryPuller(),
		platform: String = "linux/arm64",
		command: [String]? = nil,
		defaultCommandOverride: [String]? = nil,
		defaultEnvironmentOverride: [String: String] = [:],
		defaultWorkingDirectoryOverride: String? = nil,
		defaultUserIDOverride: UInt32? = nil,
		defaultGroupIDOverride: UInt32? = nil,
		defaultSupplementaryGroupOverrides: [UInt32] = [],
		defaultCapabilitiesOverride: OrlixEnvironmentCapabilities? = nil,
		hostnameOverride: String? = nil,
		domainnameOverride: String? = nil,
		terminalOverride: Bool? = nil,
		terminalRowsOverride: UInt32? = nil,
		terminalColumnsOverride: UInt32? = nil,
		rootReadonlyOverride: Bool? = nil,
		rootPropagationOverride: OrlixEnvironmentRootPropagation? = nil,
		annotationOverrides: [String: String] = [:],
		defaultRlimitOverrides: [OrlixEnvironmentRlimit] = [],
		sysctlOverrides: [String: String] = [:],
		maskedPathOverrides: [String] = [],
		readonlyPathOverrides: [String] = [],
		defaultUmaskOverride: UInt32? = nil,
		defaultOOMScoreAdjustmentOverride: Int32? = nil,
		defaultSchedulerOverride: OrlixEnvironmentScheduler? = nil,
		defaultIOPriorityOverride: OrlixEnvironmentIOPriority? = nil,
		defaultPersonalityDomainOverride: String? = nil,
		defaultCPUAffinityOverride: OrlixEnvironmentCPUAffinity? = nil,
		noNewPrivilegesOverride: Bool? = nil,
		closeAdditionalFdsOverride: Bool? = nil,
		cgroupsPathOverride: String? = nil,
		cgroupPidsLimitOverride: Int64? = nil,
		cgroupCPUMaxOverride: OrlixEnvironmentCgroupCPUMax? = nil,
		cgroupCPUWeightOverride: UInt64? = nil,
		cgroupMemoryMaxOverride: Int64? = nil,
        cgroupIOWeightOverride: UInt64? = nil,
	cgroupUnifiedOverrides: [OrlixEnvironmentCgroupUnifiedEntry] = [],
tmpfsMountOverrides: [OrlixEnvironmentTmpfsMount] = [],
mountOverrides: [OrlixEnvironmentMount] = [],
publishedPortOverrides: [OrlixEnvironmentPublishedPort] = [],
deviceNodeOverrides: [OrlixEnvironmentDeviceNode] = [],
        namespaceOverrides: [String] = [],
        namespacePathOverrides: [String: String] = [:],
        timeOffsetOverrides: [OrlixEnvironmentTimeOffset] = [],
        uidMappingOverrides: [OrlixEnvironmentIDMapping] = [],
        gidMappingOverrides: [OrlixEnvironmentIDMapping] = [],
        terminal: OrlixTerminalSession = OrlixTerminalSession(),
		observationTimeout: TimeInterval = 600,
		fileManager: FileManager = .default,
		runCommand: @escaping @Sendable (URL, [String]) throws -> Void
	) async throws -> OrlixOCIRegistryEnvironmentInstallRunResult {
		let installResult = try await install(
			image: image,
			id: id,
            tools: tools,
			puller: puller,
			platform: platform,
			defaultCommandOverride: defaultCommandOverride,
			defaultEnvironmentOverride: defaultEnvironmentOverride,
			defaultWorkingDirectoryOverride: defaultWorkingDirectoryOverride,
			defaultUserIDOverride: defaultUserIDOverride,
			defaultGroupIDOverride: defaultGroupIDOverride,
			defaultSupplementaryGroupOverrides: defaultSupplementaryGroupOverrides,
			defaultCapabilitiesOverride: defaultCapabilitiesOverride,
			hostnameOverride: hostnameOverride,
			domainnameOverride: domainnameOverride,
			terminalOverride: terminalOverride,
			terminalRowsOverride: terminalRowsOverride,
			terminalColumnsOverride: terminalColumnsOverride,
			rootReadonlyOverride: rootReadonlyOverride,
			rootPropagationOverride: rootPropagationOverride,
			annotationOverrides: annotationOverrides,
			defaultRlimitOverrides: defaultRlimitOverrides,
			sysctlOverrides: sysctlOverrides,
			maskedPathOverrides: maskedPathOverrides,
			readonlyPathOverrides: readonlyPathOverrides,
			defaultUmaskOverride: defaultUmaskOverride,
			defaultOOMScoreAdjustmentOverride: defaultOOMScoreAdjustmentOverride,
			defaultSchedulerOverride: defaultSchedulerOverride,
			defaultIOPriorityOverride: defaultIOPriorityOverride,
			defaultPersonalityDomainOverride: defaultPersonalityDomainOverride,
			defaultCPUAffinityOverride: defaultCPUAffinityOverride,
			noNewPrivilegesOverride: noNewPrivilegesOverride,
			closeAdditionalFdsOverride: closeAdditionalFdsOverride,
			cgroupsPathOverride: cgroupsPathOverride,
			cgroupPidsLimitOverride: cgroupPidsLimitOverride,
			cgroupCPUMaxOverride: cgroupCPUMaxOverride,
			cgroupCPUWeightOverride: cgroupCPUWeightOverride,
			cgroupMemoryMaxOverride: cgroupMemoryMaxOverride,
            cgroupIOWeightOverride: cgroupIOWeightOverride,
	cgroupUnifiedOverrides: cgroupUnifiedOverrides,
	tmpfsMountOverrides: tmpfsMountOverrides,
	mountOverrides: mountOverrides,
	publishedPortOverrides: publishedPortOverrides,
	deviceNodeOverrides: deviceNodeOverrides,
namespaceOverrides: namespaceOverrides,
namespacePathOverrides: namespacePathOverrides,
timeOffsetOverrides: timeOffsetOverrides,
uidMappingOverrides: uidMappingOverrides,
gidMappingOverrides: gidMappingOverrides,
fileManager: fileManager,
			runCommand: runCommand
		)
		let runResult = try run(
			id: id,
			command: command,
			terminal: terminal,
			observationTimeout: observationTimeout,
			fileManager: fileManager
		)
		return OrlixOCIRegistryEnvironmentInstallRunResult(
			installResult: installResult,
			runResult: runResult,
			deleteResult: nil
		)
	}

	@_spi(OrlixPrivateTesting)
	@discardableResult
	public func run(
		arguments: [String],
		tools: OrlixOCIEnvironmentMaterializationTools,
		puller: OrlixOCIRegistryPuller = OrlixOCIRegistryPuller(),
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		using driver: OrlixOCIRuntimeProcessObservationDriver,
		fileManager: FileManager = .default,
		runCommand: @escaping @Sendable (URL, [String]) throws -> Void
	) async throws -> OrlixOCIRegistryEnvironmentInstallRunResult {
		let request = try OrlixOCIEnvironmentRunArguments(arguments)
		let result = try await run(
			image: OrlixOCIRegistryImageReference(request.image),
			id: request.id,
            tools: tools,
			puller: puller,
			platform: request.platform,
			command: request.resolvedCommandOverride,
			defaultCommandOverride: request.resolvedCommandOverride,
			defaultEnvironmentOverride: request.environment,
			defaultWorkingDirectoryOverride: request.workingDirectory,
			defaultUserIDOverride: request.userID,
			defaultGroupIDOverride: request.groupID,
			defaultSupplementaryGroupOverrides:
				request.supplementaryGroupIDs,
			defaultCapabilitiesOverride: request.capabilities,
			hostnameOverride: request.hostname,
			domainnameOverride: request.domainname,
			terminalOverride: request.terminal,
			terminalRowsOverride: request.terminalRows,
			terminalColumnsOverride: request.terminalColumns,
			rootReadonlyOverride: request.rootReadonly,
			rootPropagationOverride: request.rootPropagation,
			annotationOverrides: request.annotations,
			defaultRlimitOverrides: request.rlimits,
			sysctlOverrides: request.sysctls,
			maskedPathOverrides: request.maskedPaths,
			readonlyPathOverrides: request.readonlyPaths,
			defaultUmaskOverride: request.umask,
			defaultOOMScoreAdjustmentOverride: request.oomScoreAdjustment,
			defaultSchedulerOverride: request.scheduler,
			defaultIOPriorityOverride: request.ioPriority,
			defaultPersonalityDomainOverride: request.personalityDomain,
			defaultCPUAffinityOverride: request.cpuAffinity,
			noNewPrivilegesOverride: request.noNewPrivileges,
			closeAdditionalFdsOverride: request.closeAdditionalFds,
			cgroupsPathOverride: request.cgroupsPath,
			cgroupPidsLimitOverride: request.cgroupPidsLimit,
			cgroupCPUMaxOverride: request.cgroupCPUMax,
			cgroupCPUWeightOverride: request.cgroupCPUWeight,
			cgroupMemoryMaxOverride: request.cgroupMemoryMax,
			cgroupIOWeightOverride: request.cgroupIOWeight,
	cgroupUnifiedOverrides: request.cgroupUnified,
tmpfsMountOverrides: request.tmpfsMounts,
mountOverrides: request.mounts,
publishedPortOverrides: request.publishedPorts,
deviceNodeOverrides: request.deviceNodes,
namespaceOverrides: request.namespaces,
namespacePathOverrides: request.namespacePaths,
timeOffsetOverrides: request.timeOffsets,
uidMappingOverrides: request.uidMappings,
gidMappingOverrides: request.gidMappings,
			terminal: terminal,
			using: driver,
			fileManager: fileManager,
			runCommand: runCommand
		)
		guard request.removeAfterRun else {
			return result
		}
		let deleteResult = try delete(id: request.id, fileManager: fileManager)
		return OrlixOCIRegistryEnvironmentInstallRunResult(
			installResult: result.installResult,
			runResult: result.runResult,
			deleteResult: deleteResult
		)
	}

	@_spi(OrlixPrivateTesting)
	@discardableResult
	public func run(
		image: OrlixOCIRegistryImageReference,
		id: String,
		tools: OrlixOCIEnvironmentMaterializationTools,
        puller: OrlixOCIRegistryPuller = OrlixOCIRegistryPuller(),
		platform: String = "linux/arm64",
		command: [String]? = nil,
		defaultCommandOverride: [String]? = nil,
		defaultEnvironmentOverride: [String: String] = [:],
		defaultWorkingDirectoryOverride: String? = nil,
		defaultUserIDOverride: UInt32? = nil,
		defaultGroupIDOverride: UInt32? = nil,
		defaultSupplementaryGroupOverrides: [UInt32] = [],
		defaultCapabilitiesOverride: OrlixEnvironmentCapabilities? = nil,
		hostnameOverride: String? = nil,
		domainnameOverride: String? = nil,
		terminalOverride: Bool? = nil,
		terminalRowsOverride: UInt32? = nil,
		terminalColumnsOverride: UInt32? = nil,
		rootReadonlyOverride: Bool? = nil,
		rootPropagationOverride: OrlixEnvironmentRootPropagation? = nil,
		annotationOverrides: [String: String] = [:],
		defaultRlimitOverrides: [OrlixEnvironmentRlimit] = [],
		sysctlOverrides: [String: String] = [:],
		maskedPathOverrides: [String] = [],
		readonlyPathOverrides: [String] = [],
		defaultUmaskOverride: UInt32? = nil,
		defaultOOMScoreAdjustmentOverride: Int32? = nil,
		defaultSchedulerOverride: OrlixEnvironmentScheduler? = nil,
		defaultIOPriorityOverride: OrlixEnvironmentIOPriority? = nil,
		defaultPersonalityDomainOverride: String? = nil,
		defaultCPUAffinityOverride: OrlixEnvironmentCPUAffinity? = nil,
		noNewPrivilegesOverride: Bool? = nil,
		closeAdditionalFdsOverride: Bool? = nil,
		cgroupsPathOverride: String? = nil,
		cgroupPidsLimitOverride: Int64? = nil,
		cgroupCPUMaxOverride: OrlixEnvironmentCgroupCPUMax? = nil,
		cgroupCPUWeightOverride: UInt64? = nil,
		cgroupMemoryMaxOverride: Int64? = nil,
        cgroupIOWeightOverride: UInt64? = nil,
	cgroupUnifiedOverrides: [OrlixEnvironmentCgroupUnifiedEntry] = [],
tmpfsMountOverrides: [OrlixEnvironmentTmpfsMount] = [],
mountOverrides: [OrlixEnvironmentMount] = [],
publishedPortOverrides: [OrlixEnvironmentPublishedPort] = [],
deviceNodeOverrides: [OrlixEnvironmentDeviceNode] = [],
namespaceOverrides: [String] = [],
namespacePathOverrides: [String: String] = [:],
timeOffsetOverrides: [OrlixEnvironmentTimeOffset] = [],
uidMappingOverrides: [OrlixEnvironmentIDMapping] = [],
gidMappingOverrides: [OrlixEnvironmentIDMapping] = [],
terminal: OrlixTerminalSession = OrlixTerminalSession(),
		using driver: OrlixOCIRuntimeProcessObservationDriver,
		fileManager: FileManager = .default,
		runCommand: @escaping @Sendable (URL, [String]) throws -> Void
	) async throws -> OrlixOCIRegistryEnvironmentInstallRunResult {
		let installResult = try await install(
			image: image,
			id: id,
            tools: tools,
			puller: puller,
			platform: platform,
			defaultCommandOverride: defaultCommandOverride,
			defaultEnvironmentOverride: defaultEnvironmentOverride,
			defaultWorkingDirectoryOverride: defaultWorkingDirectoryOverride,
			defaultUserIDOverride: defaultUserIDOverride,
			defaultGroupIDOverride: defaultGroupIDOverride,
			defaultSupplementaryGroupOverrides: defaultSupplementaryGroupOverrides,
			defaultCapabilitiesOverride: defaultCapabilitiesOverride,
			hostnameOverride: hostnameOverride,
			domainnameOverride: domainnameOverride,
			terminalOverride: terminalOverride,
			terminalRowsOverride: terminalRowsOverride,
			terminalColumnsOverride: terminalColumnsOverride,
			rootReadonlyOverride: rootReadonlyOverride,
			rootPropagationOverride: rootPropagationOverride,
			annotationOverrides: annotationOverrides,
			defaultRlimitOverrides: defaultRlimitOverrides,
			sysctlOverrides: sysctlOverrides,
			maskedPathOverrides: maskedPathOverrides,
			readonlyPathOverrides: readonlyPathOverrides,
			defaultUmaskOverride: defaultUmaskOverride,
			defaultOOMScoreAdjustmentOverride: defaultOOMScoreAdjustmentOverride,
			defaultSchedulerOverride: defaultSchedulerOverride,
			defaultIOPriorityOverride: defaultIOPriorityOverride,
			defaultPersonalityDomainOverride: defaultPersonalityDomainOverride,
            defaultCPUAffinityOverride: defaultCPUAffinityOverride,
            noNewPrivilegesOverride: noNewPrivilegesOverride,
            closeAdditionalFdsOverride: closeAdditionalFdsOverride,
            cgroupsPathOverride: cgroupsPathOverride,
            cgroupPidsLimitOverride: cgroupPidsLimitOverride,
            cgroupCPUMaxOverride: cgroupCPUMaxOverride,
            cgroupCPUWeightOverride: cgroupCPUWeightOverride,
            cgroupMemoryMaxOverride: cgroupMemoryMaxOverride,
            cgroupIOWeightOverride: cgroupIOWeightOverride,
	cgroupUnifiedOverrides: cgroupUnifiedOverrides,
tmpfsMountOverrides: tmpfsMountOverrides,
mountOverrides: mountOverrides,
publishedPortOverrides: publishedPortOverrides,
deviceNodeOverrides: deviceNodeOverrides,
            namespaceOverrides: namespaceOverrides,
            namespacePathOverrides: namespacePathOverrides,
            timeOffsetOverrides: timeOffsetOverrides,
            uidMappingOverrides: uidMappingOverrides,
            gidMappingOverrides: gidMappingOverrides,
            fileManager: fileManager,
            runCommand: runCommand
        )
		let runResult = try run(
			id: id,
			command: command,
			terminal: terminal,
			using: driver,
			fileManager: fileManager
		)
		return OrlixOCIRegistryEnvironmentInstallRunResult(
			installResult: installResult,
			runResult: runResult,
			deleteResult: nil
		)
	}

	@_spi(OrlixPrivateTesting)
	@discardableResult
	public func run(
		bundleURL: URL,
		id: String,
		tools: OrlixOCIEnvironmentMaterializationTools,
		command: [String]? = nil,
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		using driver: OrlixOCIRuntimeProcessObservationDriver,
		fileManager: FileManager = .default,
		runCommand: @escaping @Sendable (URL, [String]) throws -> Void
	) throws -> OrlixOCIEnvironmentInstallRunResult {
		let installResult = try install(
			bundleURL: bundleURL,
			id: id,
			tools: tools,
			fileManager: fileManager,
			runCommand: runCommand
		)
		let runResult = try run(
			id: id,
			command: command,
			terminal: terminal,
			using: driver,
			fileManager: fileManager
		)
		return OrlixOCIEnvironmentInstallRunResult(
			installResult: installResult,
			runResult: runResult
		)
	}

		func kernelSession(
		bundleURL: URL,
		id: String,
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		fileManager: FileManager = .default
	) throws -> OrlixKernelSession {
		try OrlixKernelSession(
			ociRuntimeBundle: OrlixOCIRuntimeBundle.load(
				from: bundleURL,
				fileManager: fileManager
			),
			id: id,
			rootMount: .defaultOverlay,
			registry: registry,
			terminal: terminal
		)
	}

		func kernelSession(
		id: String,
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		fileManager: FileManager = .default
	) throws -> OrlixKernelSession {
		try OrlixKernelSession(
			environmentID: id,
			registry: registry,
			terminal: terminal
		)
	}

	public func state(
		id: String,
		fileManager: FileManager = .default
	) throws -> OrlixOCIRuntimeStateReport {
		try OrlixOCIRuntime(registry: registry).state(
			id: id,
			fileManager: fileManager
		)
	}

    public func state(
        arguments: [String],
        fileManager: FileManager = .default
    ) throws -> OrlixOCIRuntimeStateReport {
        let request = try OrlixOCIEnvironmentStateArguments(arguments)
        return try state(id: request.id, fileManager: fileManager)
    }

    public func inspect(
        id: String,
        fileManager: FileManager = .default
    ) throws -> OrlixOCIEnvironmentInspectResult {
        try OrlixOCIRuntime(registry: registry).inspect(
            id: id,
            fileManager: fileManager
        )
    }

    public func inspect(
        arguments: [String],
        fileManager: FileManager = .default
    ) throws -> OrlixOCIEnvironmentInspectResult {
        let request = try OrlixOCIEnvironmentLifecycleArguments(
            arguments,
            command: "inspect"
        )
        return try inspect(id: request.id, fileManager: fileManager)
    }

    public func start(
        id: String,
        terminal: OrlixTerminalSession = OrlixTerminalSession(),
        observationTimeout: TimeInterval = 600,
        fileManager: FileManager = .default
	) throws -> OrlixOCIEnvironmentStartResult {
		let started = try OrlixOCIRuntime(registry: registry).start(
			id: id,
			terminal: terminal,
			observationTimeout: observationTimeout,
			fileManager: fileManager
		)
		return OrlixOCIEnvironmentStartResult(
			id: id,
			stateReport: started.stateReport
		)
	}

	public func start(
		arguments: [String],
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		observationTimeout: TimeInterval = 600,
		fileManager: FileManager = .default
	) throws -> OrlixOCIEnvironmentStartResult {
		let request = try OrlixOCIEnvironmentLifecycleArguments(
			arguments,
			command: "start"
		)
		return try start(
			id: request.id,
			terminal: terminal,
			observationTimeout: observationTimeout,
			fileManager: fileManager
		)
	}

	@_spi(OrlixPrivateTesting)
	public func start(
		id: String,
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		using driver: OrlixOCIRuntimeProcessObservationDriver,
		fileManager: FileManager = .default
	) throws -> OrlixOCIEnvironmentStartResult {
		let started = try OrlixOCIRuntime(registry: registry).start(
			id: id,
			terminal: terminal,
			using: driver,
			fileManager: fileManager
		)
		return OrlixOCIEnvironmentStartResult(
			id: id,
			stateReport: started.stateReport
		)
	}

	@_spi(OrlixPrivateTesting)
	public func start(
		arguments: [String],
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		using driver: OrlixOCIRuntimeProcessObservationDriver,
		fileManager: FileManager = .default
	) throws -> OrlixOCIEnvironmentStartResult {
		let request = try OrlixOCIEnvironmentLifecycleArguments(
			arguments,
			command: "start"
		)
		return try start(
			id: request.id,
			terminal: terminal,
			using: driver,
			fileManager: fileManager
		)
	}

	public func wait(
		id: String,
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
        observationTimeout: TimeInterval = 600,
        fileManager: FileManager = .default
	) throws -> OrlixOCIEnvironmentWaitResult {
		let completed = try OrlixOCIRuntime(registry: registry).wait(
			id: id,
			terminal: terminal,
			observationTimeout: observationTimeout,
			fileManager: fileManager
		)
		return OrlixOCIEnvironmentWaitResult(
			id: id,
			stateReport: completed.stateReport
		)
	}

	public func wait(
		arguments: [String],
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		observationTimeout: TimeInterval = 600,
		fileManager: FileManager = .default
	) throws -> OrlixOCIEnvironmentWaitResult {
		let request = try OrlixOCIEnvironmentLifecycleArguments(
			arguments,
			command: "wait"
		)
		return try wait(
			id: request.id,
			terminal: terminal,
			observationTimeout: observationTimeout,
			fileManager: fileManager
		)
	}

	@_spi(OrlixPrivateTesting)
	public func wait(
		id: String,
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		using driver: OrlixOCIRuntimeProcessObservationDriver,
		fileManager: FileManager = .default
	) throws -> OrlixOCIEnvironmentWaitResult {
		let completed = try OrlixOCIRuntime(registry: registry).wait(
			id: id,
			terminal: terminal,
			using: driver,
			fileManager: fileManager
		)
		return OrlixOCIEnvironmentWaitResult(
			id: id,
			stateReport: completed.stateReport
		)
	}

	@_spi(OrlixPrivateTesting)
	public func wait(
		arguments: [String],
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		using driver: OrlixOCIRuntimeProcessObservationDriver,
		fileManager: FileManager = .default
	) throws -> OrlixOCIEnvironmentWaitResult {
		let request = try OrlixOCIEnvironmentLifecycleArguments(
			arguments,
			command: "wait"
		)
		return try wait(
			id: request.id,
			terminal: terminal,
			using: driver,
			fileManager: fileManager
		)
	}

	public func kill(
		id: String,
		signal: Int32,
        terminal: OrlixTerminalSession = OrlixTerminalSession(),
        observationTimeout: TimeInterval = 600,
        fileManager: FileManager = .default
    ) throws -> OrlixOCIEnvironmentSignalResult {
        let signaled = try OrlixOCIRuntime(registry: registry).kill(
            id: id,
            signal: signal,
            terminal: terminal,
            using: OrlixOCIRuntimeKernelSessionObservationDriver(
                timeout: observationTimeout
            ),
            fileManager: fileManager
        )
        return OrlixOCIEnvironmentSignalResult(
            id: id,
            signal: signaled.signal,
			stateReport: signaled.stateReport
		)
	}

	public func kill(
		arguments: [String],
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		observationTimeout: TimeInterval = 600,
		fileManager: FileManager = .default
	) throws -> OrlixOCIEnvironmentSignalResult {
		let request = try OrlixOCIEnvironmentKillArguments(arguments)
		let signal = try effectiveKillSignal(
			for: request,
			fileManager: fileManager
		)
		return try kill(
			id: request.id,
			signal: signal,
			terminal: terminal,
			observationTimeout: observationTimeout,
			fileManager: fileManager
		)
	}

	@_spi(OrlixPrivateTesting)
	public func kill(
		id: String,
        signal: Int32,
        terminal: OrlixTerminalSession = OrlixTerminalSession(),
        using driver: OrlixOCIRuntimeProcessObservationDriver,
        fileManager: FileManager = .default
    ) throws -> OrlixOCIEnvironmentSignalResult {
        let signaled = try OrlixOCIRuntime(registry: registry).kill(
            id: id,
            signal: signal,
            terminal: terminal,
            using: driver,
            fileManager: fileManager
        )
        return OrlixOCIEnvironmentSignalResult(
            id: id,
            signal: signaled.signal,
			stateReport: signaled.stateReport
		)
	}

	@_spi(OrlixPrivateTesting)
	public func kill(
		arguments: [String],
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		using driver: OrlixOCIRuntimeProcessObservationDriver,
		fileManager: FileManager = .default
	) throws -> OrlixOCIEnvironmentSignalResult {
		let request = try OrlixOCIEnvironmentKillArguments(arguments)
		let signal = try effectiveKillSignal(
			for: request,
			fileManager: fileManager
		)
		return try kill(
			id: request.id,
			signal: signal,
			terminal: terminal,
			using: driver,
			fileManager: fileManager
		)
	}

	private func effectiveKillSignal(
		for request: OrlixOCIEnvironmentKillArguments,
		fileManager: FileManager
	) throws -> Int32 {
		guard !request.signalSpecified else {
			return request.signal
		}
		return try registry.load(
			environmentID: request.id,
			fileManager: fileManager
		).defaultStopSignal ?? request.signal
	}

    public func run(
        id: String,
        command: [String]? = nil,
        terminal: OrlixTerminalSession = OrlixTerminalSession(),
        observationTimeout: TimeInterval = 600,
		fileManager: FileManager = .default
	) throws -> OrlixOCIEnvironmentRunResult {
		let runtimeResult = try OrlixOCIRuntime(registry: registry).run(
			id: id,
			command: command,
			terminal: terminal,
			observationTimeout: observationTimeout,
			fileManager: fileManager
        )
        return Self.runResult(id: id, runtimeResult: runtimeResult)
    }

    public func exec(
        id: String,
        command: [String],
        terminal: OrlixTerminalSession = OrlixTerminalSession(),
        observationTimeout: TimeInterval = 600,
        fileManager: FileManager = .default
    ) throws -> OrlixOCIEnvironmentExecResult {
        let runResult = try run(
            id: id,
            command: command,
            terminal: terminal,
            observationTimeout: observationTimeout,
            fileManager: fileManager
        )
        return OrlixOCIEnvironmentExecResult(
            id: id,
            command: command,
            runResult: runResult
        )
    }

    public func exec(
        arguments: [String],
        terminal: OrlixTerminalSession = OrlixTerminalSession(),
        observationTimeout: TimeInterval = 600,
        fileManager: FileManager = .default
    ) throws -> OrlixOCIEnvironmentExecResult {
        let request = try OrlixOCIEnvironmentExecArguments(arguments)
        return try exec(
            id: request.id,
            command: request.command,
            terminal: terminal,
            observationTimeout: observationTimeout,
            fileManager: fileManager
        )
    }

    @_spi(OrlixPrivateTesting)
    public func run(
        id: String,
        command: [String]? = nil,
        rootMount: OrlixEnvironmentRootMount = .defaultOverlay,
		kernelCommandLine: String? = OrlixEnvironmentRootImage.defaultKernelCommandLine,
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		using driver: OrlixOCIRuntimeProcessObservationDriver,
		fileManager: FileManager = .default
	) throws -> OrlixOCIEnvironmentRunResult {
		let runtimeResult = try OrlixOCIRuntime(registry: registry).run(
			id: id,
			command: command,
			rootMount: rootMount,
			kernelCommandLine: kernelCommandLine,
			terminal: terminal,
			using: driver,
			fileManager: fileManager
        )
        return Self.runResult(id: id, runtimeResult: runtimeResult)
    }

    @_spi(OrlixPrivateTesting)
    public func exec(
        id: String,
        command: [String],
        rootMount: OrlixEnvironmentRootMount = .defaultOverlay,
        kernelCommandLine: String? = OrlixEnvironmentRootImage
            .defaultKernelCommandLine,
        terminal: OrlixTerminalSession = OrlixTerminalSession(),
        using driver: OrlixOCIRuntimeProcessObservationDriver,
        fileManager: FileManager = .default
    ) throws -> OrlixOCIEnvironmentExecResult {
        let runResult = try run(
            id: id,
            command: command,
            rootMount: rootMount,
            kernelCommandLine: kernelCommandLine,
            terminal: terminal,
            using: driver,
            fileManager: fileManager
        )
        return OrlixOCIEnvironmentExecResult(
            id: id,
            command: command,
            runResult: runResult
        )
    }

    @_spi(OrlixPrivateTesting)
    public func exec(
        arguments: [String],
        rootMount: OrlixEnvironmentRootMount = .defaultOverlay,
        kernelCommandLine: String? = OrlixEnvironmentRootImage
            .defaultKernelCommandLine,
        terminal: OrlixTerminalSession = OrlixTerminalSession(),
        using driver: OrlixOCIRuntimeProcessObservationDriver,
        fileManager: FileManager = .default
    ) throws -> OrlixOCIEnvironmentExecResult {
        let request = try OrlixOCIEnvironmentExecArguments(arguments)
        return try exec(
            id: request.id,
            command: request.command,
            rootMount: rootMount,
            kernelCommandLine: kernelCommandLine,
            terminal: terminal,
            using: driver,
            fileManager: fileManager
        )
    }

    public func healthcheck(
        id: String,
        terminal: OrlixTerminalSession = OrlixTerminalSession(),
        observationTimeout: TimeInterval = 600,
fileManager: FileManager = .default
) throws -> OrlixOCIEnvironmentHealthcheckResult {
let command = try healthcheckCommand(
environmentID: id,
fileManager: fileManager
)
let runResult = try run(
id: id,
command: command,
terminal: terminal,
observationTimeout: observationTimeout,
fileManager: fileManager
)
return OrlixOCIEnvironmentHealthcheckResult(
id: id,
command: command,
runResult: runResult
)
}

public func healthcheck(
arguments: [String],
terminal: OrlixTerminalSession = OrlixTerminalSession(),
observationTimeout: TimeInterval = 600,
fileManager: FileManager = .default
) throws -> OrlixOCIEnvironmentHealthcheckResult {
let request = try OrlixOCIEnvironmentLifecycleArguments(
arguments,
command: "healthcheck"
)
return try healthcheck(
id: request.id,
terminal: terminal,
observationTimeout: observationTimeout,
fileManager: fileManager
)
}

@_spi(OrlixPrivateTesting)
public func healthcheck(
id: String,
rootMount: OrlixEnvironmentRootMount = .defaultOverlay,
kernelCommandLine: String? = OrlixEnvironmentRootImage.defaultKernelCommandLine,
terminal: OrlixTerminalSession = OrlixTerminalSession(),
using driver: OrlixOCIRuntimeProcessObservationDriver,
fileManager: FileManager = .default
) throws -> OrlixOCIEnvironmentHealthcheckResult {
let command = try healthcheckCommand(
environmentID: id,
fileManager: fileManager
)
let runResult = try run(
id: id,
command: command,
rootMount: rootMount,
kernelCommandLine: kernelCommandLine,
terminal: terminal,
using: driver,
fileManager: fileManager
)
return OrlixOCIEnvironmentHealthcheckResult(
id: id,
command: command,
runResult: runResult
)
}

@_spi(OrlixPrivateTesting)
public func healthcheck(
arguments: [String],
terminal: OrlixTerminalSession = OrlixTerminalSession(),
using driver: OrlixOCIRuntimeProcessObservationDriver,
fileManager: FileManager = .default
) throws -> OrlixOCIEnvironmentHealthcheckResult {
let request = try OrlixOCIEnvironmentLifecycleArguments(
arguments,
command: "healthcheck"
)
return try healthcheck(
id: request.id,
terminal: terminal,
using: driver,
fileManager: fileManager
)
}

private func healthcheckCommand(
environmentID: String,
fileManager: FileManager
) throws -> [String] {
let descriptor = try registry.load(
environmentID: environmentID,
fileManager: fileManager
)
guard let healthcheck = descriptor.healthcheck else {
throw OrlixOCIEnvironmentHealthcheckError.missingHealthcheck(environmentID)
}
return try Self.healthcheckCommand(
healthcheck.test,
environmentID: environmentID
)
}

private static func healthcheckCommand(
_ test: [String],
environmentID: String
) throws -> [String] {
guard let kind = test.first else {
throw OrlixOCIEnvironmentHealthcheckError.invalidHealthcheckTest(
environmentID,
test
)
}
switch kind {
case "NONE":
throw OrlixOCIEnvironmentHealthcheckError.disabledHealthcheck(environmentID)
case "CMD":
let command = Array(test.dropFirst())
guard !command.isEmpty else {
throw OrlixOCIEnvironmentHealthcheckError.invalidHealthcheckTest(
environmentID,
test
)
}
return command
case "CMD-SHELL":
guard test.count == 2,
let script = test.dropFirst().first,
!script.isEmpty else {
throw OrlixOCIEnvironmentHealthcheckError.invalidHealthcheckTest(
environmentID,
test
)
}
return ["/bin/sh", "-c", script]
default:
throw OrlixOCIEnvironmentHealthcheckError.invalidHealthcheckTest(
environmentID,
test
)
}
}

private static func runResult(
	id: String,
	runtimeResult: OrlixOCIRuntimeRunResult
	) -> OrlixOCIEnvironmentRunResult {
		OrlixOCIEnvironmentRunResult(
			id: id,
			startedStateReport: runtimeResult.startedEnvironment.stateReport,
			completedStateReport: runtimeResult.completedEnvironment.stateReport
		)
	}

	@discardableResult
	public func delete(
		id: String,
		fileManager: FileManager = .default
	) throws -> OrlixOCIEnvironmentDeleteResult {
		let deletedEnvironment = try OrlixOCIRuntime(registry: registry).delete(
			id: id,
			fileManager: fileManager
		)
		return OrlixOCIEnvironmentDeleteResult(
			id: deletedEnvironment.id,
			lifecycleState: deletedEnvironment.deletedRecord.state
		)
	}

	@discardableResult
	public func delete(
		arguments: [String],
		fileManager: FileManager = .default
	) throws -> OrlixOCIEnvironmentDeleteResult {
		let request = try OrlixOCIEnvironmentLifecycleArguments(
			arguments,
			command: "delete"
		)
		return try delete(id: request.id, fileManager: fileManager)
	}

	private func registryLifecycleConfig(
		for descriptor: OrlixEnvironmentDescriptor
	) throws -> OrlixOCIRuntimeConfigDescriptor {
		try orlixOCIRuntimeConfig(for: descriptor)
	}
}

private func runtimeLifecycleConfig(
	for snapshot: OrlixOCIRuntimeLifecycleSnapshot,
	registry: OrlixEnvironmentRegistry,
	command: [String]? = nil,
	fileManager: FileManager
) throws -> OrlixOCIRuntimeConfigDescriptor {
	let config: OrlixOCIRuntimeConfigDescriptor
	if snapshot.record.bundlePath.hasPrefix("oci://") {
		config = try orlixOCIRuntimeConfig(
			for: registry.load(
				environmentID: snapshot.record.id,
				fileManager: fileManager
			)
		)
	} else {
		config = try OrlixOCIRuntimeBundle.load(
			from: URL(fileURLWithPath: snapshot.record.bundlePath),
			fileManager: fileManager
		).config
	}
	guard let command else {
		return config
	}
	return try config.replacingDefaultCommand(command)
}

private func orlixOCIRuntimeConfig(
	for descriptor: OrlixEnvironmentDescriptor
) throws -> OrlixOCIRuntimeConfigDescriptor {
		let environment = descriptor.defaultEnvironment
			.keys
			.sorted()
			.map { "\($0)=\(descriptor.defaultEnvironment[$0] ?? "")" }
	var user: [String: Any] = [
		"uid": descriptor.defaultUserID,
		"gid": descriptor.defaultGroupID,
	]
	if !descriptor.defaultSupplementaryGroups.isEmpty {
		user["additionalGids"] = descriptor.defaultSupplementaryGroups
	}
	if let umask = descriptor.defaultUmask {
		user["umask"] = umask
	}
	var capabilities: [String: Any]?
	if let descriptorCapabilities = descriptor.defaultCapabilities {
		capabilities = [
			"bounding": descriptorCapabilities.bounding,
			"permitted": descriptorCapabilities.permitted,
			"inheritable": descriptorCapabilities.inheritable,
			"effective": descriptorCapabilities.effective,
			"ambient": descriptorCapabilities.ambient,
		]
	}
	var process: [String: Any] = [
		"terminal": descriptor.defaultTerminal ?? false,
		"args": descriptor.defaultCommand,
		"env": environment,
		"cwd": descriptor.defaultWorkingDirectory,
		"user": user,
	]
	if let capabilities {
		process["capabilities"] = capabilities
	}
	if let oomScoreAdjustment = descriptor.defaultOOMScoreAdjustment {
		process["oomScoreAdj"] = oomScoreAdjustment
	}
	if let scheduler = descriptor.defaultScheduler {
		process["scheduler"] = [
			"policy": scheduler.policy,
			"priority": scheduler.priority,
		]
	}
	if let ioPriority = descriptor.defaultIOPriority {
		process["ioPriority"] = [
			"class": ioPriority.class,
			"priority": ioPriority.priority,
		]
	}
	if let cpuAffinity = descriptor.defaultCPUAffinity {
		process["execCPUAffinity"] = [
			"final": cpuAffinity.mask,
		]
	}
	if let rows = descriptor.defaultTerminalRows,
		let columns = descriptor.defaultTerminalColumns
	{
		process["consoleSize"] = [
			"height": rows,
			"width": columns,
		]
	}
	if descriptor.defaultNoNewPrivileges {
		process["noNewPrivileges"] = true
	}
	if descriptor.defaultCloseAdditionalFds {
		process["closeAdditionalFds"] = true
	}
	if !descriptor.defaultRlimits.isEmpty {
		process["rlimits"] = descriptor.defaultRlimits.map { rlimit in
			[
				"type": rlimit.type,
				"soft": rlimit.soft,
				"hard": rlimit.hard,
			]
		}
	}
	var root: [String: Any] = [
		"path": "rootfs",
	]
	if descriptor.rootReadonly {
		root["readonly"] = true
	}
	var mounts: [[String: Any]] = descriptor.tmpfsMounts.map { mount in
		var options: [String] = []
		if mount.readOnly { options.append("ro") }
		if mount.noSuid { options.append("nosuid") }
		if mount.noDev { options.append("nodev") }
		if mount.noExec { options.append("noexec") }
		if let data = mount.data {
			options.append(contentsOf: data.split(separator: ",").map(String.init))
		}
		var runtimeMount: [String: Any] = [
			"destination": mount.targetPath,
			"type": "tmpfs",
			"source": "tmpfs",
		]
		if !options.isEmpty {
			runtimeMount["options"] = options
		}
		return runtimeMount
	}
	mounts.append(contentsOf: descriptor.mounts.map { mount in
		let source: String
		switch mount.source {
		case .documents:
			source = "orlix:documents"
		case let .securityScopedExternal(bookmarkID):
			source = "orlix:external:\(bookmarkID)"
		case let .hostPath(hostPath):
			source = hostPath
		}
		var options = ["bind"]
		options.append(mount.readOnly ? "ro" : "rw")
		if mount.noExec {
			options.append("noexec")
		}
		return [
			"destination": mount.targetPath,
			"type": "bind",
			"source": source,
			"options": options,
		]
	})
	var linux: [String: Any] = [:]
	if descriptor.rootPropagation != .private {
		linux["rootfsPropagation"] = descriptor.rootPropagation.rawValue
	}
	if !descriptor.sysctls.isEmpty {
		linux["sysctl"] = descriptor.sysctls
	}
	if !descriptor.maskedPaths.isEmpty {
		linux["maskedPaths"] = descriptor.maskedPaths
	}
	if !descriptor.readonlyPaths.isEmpty {
		linux["readonlyPaths"] = descriptor.readonlyPaths
	}
	if let cgroupsPath = descriptor.cgroupsPath {
		linux["cgroupsPath"] = cgroupsPath
	}
	var resources: [String: Any] = [:]
	if let cgroupPidsLimit = descriptor.cgroupPidsLimit {
		resources["pids"] = ["limit": cgroupPidsLimit]
	}
	if descriptor.cgroupCPUMax != nil || descriptor.cgroupCPUWeight != nil {
		var cpu: [String: Any] = [:]
		if let cgroupCPUMax = descriptor.cgroupCPUMax {
			cpu["quota"] = cgroupCPUMax.quotaMicros
			cpu["period"] = cgroupCPUMax.periodMicros
		}
		if let cgroupCPUWeight = descriptor.cgroupCPUWeight {
			cpu["shares"] = try ociCPUShares(forCgroupV2Weight: cgroupCPUWeight)
		}
		resources["cpu"] = cpu
	}
	if let cgroupMemoryMax = descriptor.cgroupMemoryMax {
		resources["memory"] = ["limit": cgroupMemoryMax]
	}
	if let cgroupIOWeight = descriptor.cgroupIOWeight {
		resources["blockIO"] = ["weight": cgroupIOWeight]
	}
	if !descriptor.cgroupUnified.isEmpty {
		resources["unified"] = Dictionary(
			uniqueKeysWithValues: descriptor.cgroupUnified.map {
				($0.file, $0.value)
			}
		)
	}
	if !resources.isEmpty {
		linux["resources"] = resources
	}
	if !descriptor.deviceNodes.isEmpty {
		linux["devices"] = descriptor.deviceNodes.map { device in
			[
				"path": device.path,
				"type": device.type,
				"major": device.major,
				"minor": device.minor,
				"fileMode": device.fileMode,
				"uid": device.uid,
				"gid": device.gid,
			]
		}
	}
	if !descriptor.timeOffsets.isEmpty {
		linux["timeOffsets"] = Dictionary(
			uniqueKeysWithValues: descriptor.timeOffsets.map { offset in
				(
					offset.clock,
					[
						"secs": offset.secs,
						"nanosecs": offset.nanosecs,
					]
				)
			}
		)
	}
	if !descriptor.uidMappings.isEmpty {
		linux["uidMappings"] = descriptor.uidMappings.map { mapping in
			[
				"containerID": mapping.containerID,
				"hostID": mapping.hostID,
				"size": mapping.size,
			]
		}
	}
	if !descriptor.gidMappings.isEmpty {
		linux["gidMappings"] = descriptor.gidMappings.map { mapping in
			[
				"containerID": mapping.containerID,
				"hostID": mapping.hostID,
				"size": mapping.size,
			]
		}
	}
	var namespaces = descriptor.namespaces.map { namespace in
		["type": namespace]
	}
	namespaces.append(contentsOf: descriptor.namespacePaths
		.keys
		.sorted()
		.map { type in
			[
				"type": type,
				"path": descriptor.namespacePaths[type] ?? "",
			]
		})
	if !namespaces.isEmpty {
		linux["namespaces"] = namespaces
	}
	if let personality = descriptor.defaultPersonalityDomain {
		linux["personality"] = ["domain": personality]
	}
	var config = [
		"ociVersion": "1.1.0",
		"annotations": descriptor.annotations,
		"root": root,
		"process": process,
	] as [String: Any]
	if !mounts.isEmpty {
		config["mounts"] = mounts
	}
	if !linux.isEmpty {
		config["linux"] = linux
	}
	if let hostname = descriptor.hostname {
		config["hostname"] = hostname
	}
	if let domainname = descriptor.domainname {
		config["domainname"] = domainname
	}
	let data = try JSONSerialization.data(
			withJSONObject: config,
			options: [.sortedKeys]
		)
		return try OrlixOCIRuntimeConfigParser().parse(data)
	}

	private func ociCPUShares(forCgroupV2Weight weight: UInt64) throws -> UInt64 {
		guard (1...10_000).contains(weight) else {
			throw OrlixEnvironmentRootImageError.invalidCgroupCPUWeight(weight)
		}
		let numerator = (weight - 1) * 262_142
		return 2 + (numerator + 9_998) / 9_999
	}
private struct OrlixOCIEnvironmentInstallerCommandRunner:
	OrlixEnvironmentImageMaterializationCommandRunner
{
	let runCommand: @Sendable (URL, [String]) throws -> Void

	func run(_ command: OrlixEnvironmentImageMaterializationCommand) throws {
		try runCommand(URL(fileURLWithPath: command.executable), command.arguments)
	}
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIRuntime: Sendable {
	public let registry: OrlixEnvironmentRegistry
	public let lifecycleStore: OrlixOCIRuntimeLifecycleStore

	public init(registry: OrlixEnvironmentRegistry) {
		self.registry = registry
		self.lifecycleStore = OrlixOCIRuntimeLifecycleStore(registry: registry)
	}

	public func create(
		bundleURL: URL,
		id: String,
		rootMount: OrlixEnvironmentRootMount = .defaultOverlay,
		fileManager: FileManager = .default
	) throws -> OrlixOCIRuntimeCreatedEnvironment {
		try createEnvironment(
			bundleURL: bundleURL,
			id: id,
			rootMount: rootMount,
			fileManager: fileManager,
			materializationRunner: nil
		).createdEnvironment
	}

	public func createMaterialized(
		bundleURL: URL,
		id: String,
		rootMount: OrlixEnvironmentRootMount = .defaultOverlay,
		mke2fsExecutable: String = "mke2fs",
		truncateExecutable: String = "truncate",
		debugfsExecutable: String = "debugfs",
		e2fsckExecutable: String = "e2fsck",
		fileManager: FileManager = .default,
		runner: OrlixEnvironmentImageMaterializationCommandRunner
	) throws -> OrlixOCIRuntimeMaterializedCreatedEnvironment {
		let result = try createEnvironment(
			bundleURL: bundleURL,
			id: id,
			rootMount: rootMount,
			fileManager: fileManager,
			materializationRunner: { importPlan in
				try importPlan.materialize(
					mke2fsExecutable: mke2fsExecutable,
					truncateExecutable: truncateExecutable,
					debugfsExecutable: debugfsExecutable,
					e2fsckExecutable: e2fsckExecutable,
					fileManager: fileManager,
					runner: runner
				)
			}
		)
		return OrlixOCIRuntimeMaterializedCreatedEnvironment(
			createdEnvironment: result.createdEnvironment,
			materializationResult: result.materializationResult!
		)
	}

	private func createEnvironment(
		bundleURL: URL,
		id: String,
		rootMount: OrlixEnvironmentRootMount,
		fileManager: FileManager,
		materializationRunner: (
			(OrlixOCIRuntimeBundleImportPlan) throws
				-> OrlixEnvironmentImageMaterializationResult
		)?
	) throws -> (
		createdEnvironment: OrlixOCIRuntimeCreatedEnvironment,
		materializationResult: OrlixEnvironmentImageMaterializationResult?
	) {
		if fileManager.fileExists(
			atPath: try lifecycleStore.recordURL(forID: id).path
		) {
			throw OrlixOCIRuntimeError.environmentAlreadyExists(id)
		}
		let bundle = try OrlixOCIRuntimeBundle.load(
			from: bundleURL,
			fileManager: fileManager
		)
		let importPlan = try bundle.importPlan(
			id: id,
			rootMount: rootMount,
			registry: registry,
			fileManager: fileManager
		)
		let materializationResult: OrlixEnvironmentImageMaterializationResult?
		if let materializationRunner {
			materializationResult = try materializationRunner(importPlan)
		} else {
			try importPlan.prepareMaterializationInputs(fileManager: fileManager)
			materializationResult = nil
		}
		try importPlan.saveEnvironment(
			to: registry,
			fileManager: fileManager
		)
		let lifecycle = try bundle.lifecycleController(id: id).create()
		try lifecycleStore.save(lifecycle, fileManager: fileManager)
		let createdEnvironment = OrlixOCIRuntimeCreatedEnvironment(
			importPlan: importPlan,
			lifecycleStore: lifecycleStore,
			lifecycle: lifecycle,
			stateReport: try lifecycleStore.stateReport(
				id: id,
				fileManager: fileManager
			)
		)
		return (createdEnvironment, materializationResult)
	}

    public func state(
        id: String,
        fileManager: FileManager = .default
    ) throws -> OrlixOCIRuntimeStateReport {
        try lifecycleStore.stateReport(id: id, fileManager: fileManager)
    }

    public func inspect(
        id: String,
        fileManager: FileManager = .default
    ) throws -> OrlixOCIEnvironmentInspectResult {
        let descriptor = try registry.load(
            environmentID: id,
            fileManager: fileManager
        )
        let snapshot = try lifecycleStore.load(id: id, fileManager: fileManager)
        return OrlixOCIEnvironmentInspectResult(
            id: descriptor.id,
            platform: descriptor.platform,
            rootImageIdentifier: descriptor.rootImageIdentifier,
            defaultCommand: descriptor.defaultCommand,
            defaultEnvironment: descriptor.defaultEnvironment,
            defaultWorkingDirectory: descriptor.defaultWorkingDirectory,
            defaultUserID: descriptor.defaultUserID,
            defaultGroupID: descriptor.defaultGroupID,
            hostname: descriptor.hostname,
            domainname: descriptor.domainname,
            annotations: descriptor.annotations,
            lifecycleState: snapshot.record.state,
            stateReport: try snapshot.stateReport()
        )
    }

    public func listPreparedEnvironments(
        fileManager: FileManager = .default
    ) throws -> [OrlixOCIEnvironmentPreparedState] {
        try registry.list(fileManager: fileManager).compactMap { descriptor in
            let recordURL = try lifecycleStore.recordURL(forID: descriptor.id)
            guard fileManager.fileExists(atPath: recordURL.path) else {
                return nil
            }
            let snapshot = try lifecycleStore.load(
                id: descriptor.id,
                fileManager: fileManager
            )
            return OrlixOCIEnvironmentPreparedState(
                id: descriptor.id,
                platform: descriptor.platform,
                defaultCommand: descriptor.defaultCommand,
                lifecycleState: snapshot.record.state,
                stateReport: try? snapshot.stateReport()
            )
        }
        .sorted { $0.id < $1.id }
    }

	func kernelSession(
        id: String,
        command: [String]? = nil,
        rootMount: OrlixEnvironmentRootMount = .defaultOverlay,
		kernelCommandLine: String? = OrlixEnvironmentRootImage.defaultKernelCommandLine,
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		fileManager: FileManager = .default
	) throws -> OrlixKernelSession {
		try processSession(
			id: id,
			command: command,
			rootMount: rootMount,
			kernelCommandLine: kernelCommandLine,
			terminal: terminal,
			fileManager: fileManager
		).kernelSession
	}

	public func start(
		id: String,
		rootMount: OrlixEnvironmentRootMount = .defaultOverlay,
		kernelCommandLine: String? = OrlixEnvironmentRootImage.defaultKernelCommandLine,
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		using driver: OrlixOCIRuntimeProcessObservationDriver,
		fileManager: FileManager = .default
	) throws -> OrlixOCIRuntimeStartedEnvironment {
		let processSession = try processSession(
			id: id,
			rootMount: rootMount,
			kernelCommandLine: kernelCommandLine,
			terminal: terminal,
			fileManager: fileManager
		)
		let runningSession = try processSession.start(using: driver)
		return OrlixOCIRuntimeStartedEnvironment(
			processSession: runningSession,
			stateReport: try lifecycleStore.stateReport(
				id: id,
				fileManager: fileManager
			)
		)
	}

	public func start(
		id: String,
		rootMount: OrlixEnvironmentRootMount = .defaultOverlay,
		kernelCommandLine: String? = OrlixEnvironmentRootImage.defaultKernelCommandLine,
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		observationTimeout: TimeInterval = 600,
		fileManager: FileManager = .default
	) throws -> OrlixOCIRuntimeStartedEnvironment {
		try start(
			id: id,
			rootMount: rootMount,
			kernelCommandLine: kernelCommandLine,
			terminal: terminal,
			using: OrlixOCIRuntimeKernelSessionObservationDriver(
				timeout: observationTimeout
			),
			fileManager: fileManager
		)
	}

	public func kill(
		id: String,
		signal: Int32,
		rootMount: OrlixEnvironmentRootMount = .defaultOverlay,
		kernelCommandLine: String? = OrlixEnvironmentRootImage.defaultKernelCommandLine,
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		using driver: OrlixOCIRuntimeProcessObservationDriver,
		fileManager: FileManager = .default
	) throws -> OrlixOCIRuntimeSignaledEnvironment {
		let processSession = try processSession(
			id: id,
			rootMount: rootMount,
			kernelCommandLine: kernelCommandLine,
			terminal: terminal,
			fileManager: fileManager
		)
		let signaledSession = try processSession.kill(
			signal: signal,
			using: driver
		)
		return OrlixOCIRuntimeSignaledEnvironment(
			processSession: signaledSession,
			signal: signal,
			stateReport: try lifecycleStore.stateReport(
				id: id,
				fileManager: fileManager
			)
		)
	}

	public func wait(
		id: String,
		rootMount: OrlixEnvironmentRootMount = .defaultOverlay,
		kernelCommandLine: String? = OrlixEnvironmentRootImage.defaultKernelCommandLine,
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		using driver: OrlixOCIRuntimeProcessObservationDriver,
		fileManager: FileManager = .default
	) throws -> OrlixOCIRuntimeCompletedEnvironment {
		let processSession = try processSession(
			id: id,
			rootMount: rootMount,
			kernelCommandLine: kernelCommandLine,
			terminal: terminal,
			fileManager: fileManager
		)
		let completedProcess = try processSession.wait(using: driver)
		return OrlixOCIRuntimeCompletedEnvironment(
			completedProcess: completedProcess,
			stateReport: try lifecycleStore.stateReport(
				id: id,
				fileManager: fileManager
			)
		)
	}

	public func wait(
		id: String,
		rootMount: OrlixEnvironmentRootMount = .defaultOverlay,
		kernelCommandLine: String? = OrlixEnvironmentRootImage.defaultKernelCommandLine,
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		observationTimeout: TimeInterval = 600,
		fileManager: FileManager = .default
	) throws -> OrlixOCIRuntimeCompletedEnvironment {
		try wait(
			id: id,
			rootMount: rootMount,
			kernelCommandLine: kernelCommandLine,
			terminal: terminal,
			using: OrlixOCIRuntimeKernelSessionObservationDriver(
				timeout: observationTimeout
			),
			fileManager: fileManager
		)
	}

	public func run(
		id: String,
		command: [String]? = nil,
		rootMount: OrlixEnvironmentRootMount = .defaultOverlay,
		kernelCommandLine: String? = OrlixEnvironmentRootImage.defaultKernelCommandLine,
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		using driver: OrlixOCIRuntimeProcessObservationDriver,
		fileManager: FileManager = .default
	) throws -> OrlixOCIRuntimeRunResult {
		let processSession = try processSession(
			snapshot: try lifecycleStore.load(id: id, fileManager: fileManager),
			command: command,
			rootMount: rootMount,
			kernelCommandLine: kernelCommandLine,
			terminal: terminal,
			fileManager: fileManager
		)
		let runningSession = try processSession.start(using: driver)
		let startedEnvironment = OrlixOCIRuntimeStartedEnvironment(
			processSession: runningSession,
			stateReport: try lifecycleStore.stateReport(
				id: id,
				fileManager: fileManager
			)
		)
		let completedProcess = try runningSession.wait(using: driver)
		let completedEnvironment = OrlixOCIRuntimeCompletedEnvironment(
			completedProcess: completedProcess,
			stateReport: try lifecycleStore.stateReport(
				id: id,
				fileManager: fileManager
			)
		)
		return OrlixOCIRuntimeRunResult(
			startedEnvironment: startedEnvironment,
			completedEnvironment: completedEnvironment
		)
	}

	public func run(
		id: String,
		command: [String]? = nil,
		rootMount: OrlixEnvironmentRootMount = .defaultOverlay,
		kernelCommandLine: String? = OrlixEnvironmentRootImage.defaultKernelCommandLine,
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		observationTimeout: TimeInterval = 600,
		fileManager: FileManager = .default
	) throws -> OrlixOCIRuntimeRunResult {
		try run(
			id: id,
			command: command,
			rootMount: rootMount,
			kernelCommandLine: kernelCommandLine,
			terminal: terminal,
			using: OrlixOCIRuntimeKernelSessionObservationDriver(
				timeout: observationTimeout
			),
			fileManager: fileManager
		)
	}

	public func run(
		bundleURL: URL,
		id: String,
		rootMount: OrlixEnvironmentRootMount = .defaultOverlay,
		mke2fsExecutable: String = "mke2fs",
		truncateExecutable: String = "truncate",
		debugfsExecutable: String = "debugfs",
		e2fsckExecutable: String = "e2fsck",
		kernelCommandLine: String? = OrlixEnvironmentRootImage.defaultKernelCommandLine,
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		materializationRunner: OrlixEnvironmentImageMaterializationCommandRunner,
		processDriver: OrlixOCIRuntimeProcessObservationDriver,
		fileManager: FileManager = .default
	) throws -> OrlixOCIRuntimeMaterializedRunResult {
		let createdEnvironment = try createMaterialized(
			bundleURL: bundleURL,
			id: id,
			rootMount: rootMount,
			mke2fsExecutable: mke2fsExecutable,
			truncateExecutable: truncateExecutable,
			debugfsExecutable: debugfsExecutable,
			e2fsckExecutable: e2fsckExecutable,
			fileManager: fileManager,
			runner: materializationRunner
		)
		let runResult = try run(
			id: id,
			rootMount: rootMount,
			kernelCommandLine: kernelCommandLine,
			terminal: terminal,
			using: processDriver,
			fileManager: fileManager
		)
		return OrlixOCIRuntimeMaterializedRunResult(
			createdEnvironment: createdEnvironment,
			runResult: runResult
		)
	}

	public func run(
		bundleURL: URL,
		id: String,
		rootMount: OrlixEnvironmentRootMount = .defaultOverlay,
		mke2fsExecutable: String = "mke2fs",
		truncateExecutable: String = "truncate",
		debugfsExecutable: String = "debugfs",
		e2fsckExecutable: String = "e2fsck",
		kernelCommandLine: String? = OrlixEnvironmentRootImage.defaultKernelCommandLine,
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		materializationRunner: OrlixEnvironmentImageMaterializationCommandRunner,
		observationTimeout: TimeInterval = 600,
		fileManager: FileManager = .default
	) throws -> OrlixOCIRuntimeMaterializedRunResult {
		try run(
			bundleURL: bundleURL,
			id: id,
			rootMount: rootMount,
			mke2fsExecutable: mke2fsExecutable,
			truncateExecutable: truncateExecutable,
			debugfsExecutable: debugfsExecutable,
			e2fsckExecutable: e2fsckExecutable,
			kernelCommandLine: kernelCommandLine,
			terminal: terminal,
			materializationRunner: materializationRunner,
			processDriver: OrlixOCIRuntimeKernelSessionObservationDriver(
				timeout: observationTimeout
			),
			fileManager: fileManager
		)
	}

	public func runEphemeral(
		bundleURL: URL,
		id: String,
		rootMount: OrlixEnvironmentRootMount = .defaultOverlay,
		mke2fsExecutable: String = "mke2fs",
		truncateExecutable: String = "truncate",
		debugfsExecutable: String = "debugfs",
		e2fsckExecutable: String = "e2fsck",
		kernelCommandLine: String? = OrlixEnvironmentRootImage.defaultKernelCommandLine,
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		materializationRunner: OrlixEnvironmentImageMaterializationCommandRunner,
		processDriver: OrlixOCIRuntimeProcessObservationDriver,
		fileManager: FileManager = .default
	) throws -> OrlixOCIRuntimeEphemeralRunResult {
		let materializedRunResult: OrlixOCIRuntimeMaterializedRunResult
		do {
			materializedRunResult = try run(
				bundleURL: bundleURL,
				id: id,
				rootMount: rootMount,
				mke2fsExecutable: mke2fsExecutable,
				truncateExecutable: truncateExecutable,
				debugfsExecutable: debugfsExecutable,
				kernelCommandLine: kernelCommandLine,
				terminal: terminal,
				materializationRunner: materializationRunner,
				processDriver: processDriver,
				fileManager: fileManager
			)
		} catch {
			guard fileManager.fileExists(
				atPath: try lifecycleStore.recordURL(forID: id).path
			) else {
				throw error
			}
			do {
				let deletedEnvironment = try delete(
					id: id,
					fileManager: fileManager
				)
				throw OrlixOCIRuntimeEphemeralRunFailure(
					id: id,
					originalError: error,
					cleanupError: nil,
					deletedEnvironment: deletedEnvironment
				)
			} catch let cleanupError as OrlixOCIRuntimeEphemeralRunFailure {
				throw cleanupError
			} catch let cleanupError {
				throw OrlixOCIRuntimeEphemeralRunFailure(
					id: id,
					originalError: error,
					cleanupError: cleanupError,
					deletedEnvironment: nil
				)
			}
		}
		let deletedEnvironment = try delete(
			id: id,
			fileManager: fileManager
		)
		return OrlixOCIRuntimeEphemeralRunResult(
			materializedRunResult: materializedRunResult,
			deletedEnvironment: deletedEnvironment
		)
	}

	public func runEphemeral(
		bundleURL: URL,
		id: String,
		rootMount: OrlixEnvironmentRootMount = .defaultOverlay,
		mke2fsExecutable: String = "mke2fs",
		truncateExecutable: String = "truncate",
		debugfsExecutable: String = "debugfs",
		e2fsckExecutable: String = "e2fsck",
		kernelCommandLine: String? = OrlixEnvironmentRootImage.defaultKernelCommandLine,
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		materializationRunner: OrlixEnvironmentImageMaterializationCommandRunner,
		observationTimeout: TimeInterval = 600,
		fileManager: FileManager = .default
	) throws -> OrlixOCIRuntimeEphemeralRunResult {
		try runEphemeral(
			bundleURL: bundleURL,
			id: id,
			rootMount: rootMount,
			mke2fsExecutable: mke2fsExecutable,
			truncateExecutable: truncateExecutable,
			debugfsExecutable: debugfsExecutable,
			e2fsckExecutable: e2fsckExecutable,
			kernelCommandLine: kernelCommandLine,
			terminal: terminal,
			materializationRunner: materializationRunner,
			processDriver: OrlixOCIRuntimeKernelSessionObservationDriver(
				timeout: observationTimeout
			),
			fileManager: fileManager
		)
	}

	public func delete(
		id: String,
		fileManager: FileManager = .default
	) throws -> OrlixOCIRuntimeDeletedEnvironment {
		let snapshot = try lifecycleStore.load(id: id, fileManager: fileManager)
		switch snapshot.record.state {
		case .created, .stopped, .configured:
			break
		case .running, .deleted:
			throw OrlixOCIRuntimeLifecycleError.invalidTransition(
				from: snapshot.record.state,
				action: .delete
			)
		}
		let deletedRecord = OrlixOCIRuntimeLifecycleRecord(
			id: snapshot.record.id,
			bundlePath: snapshot.record.bundlePath,
			pid: snapshot.record.pid,
			exitStatus: snapshot.record.exitStatus,
			state: .deleted
		)
		try lifecycleStore.delete(id: id, fileManager: fileManager)
		try registry.delete(environmentID: id, fileManager: fileManager)
		return OrlixOCIRuntimeDeletedEnvironment(
			id: id,
			deletedRecord: deletedRecord
		)
	}

	private func processSession(
		id: String,
		command: [String]? = nil,
		rootMount: OrlixEnvironmentRootMount,
		kernelCommandLine: String?,
		terminal: OrlixTerminalSession,
		fileManager: FileManager
	) throws -> OrlixOCIRuntimeProcessSession {
		try processSession(
			snapshot: try lifecycleStore.load(id: id, fileManager: fileManager),
			command: command,
			rootMount: rootMount,
			kernelCommandLine: kernelCommandLine,
			terminal: terminal,
			fileManager: fileManager
		)
	}

	private func processSession(
		snapshot: OrlixOCIRuntimeLifecycleSnapshot,
		command: [String]? = nil,
		rootMount: OrlixEnvironmentRootMount,
		kernelCommandLine: String?,
		terminal: OrlixTerminalSession,
		fileManager: FileManager
	) throws -> OrlixOCIRuntimeProcessSession {
		try validateMaterializedRootImages(
			id: snapshot.record.id,
			fileManager: fileManager
		)
		let lifecycle = OrlixOCIRuntimeLifecycleController(
			config: try runtimeLifecycleConfig(
				for: snapshot,
				registry: registry,
				command: command,
				fileManager: fileManager
			),
			record: snapshot.record
		)
		return try OrlixOCIRuntimeProcessSession(
			lifecycle: lifecycle,
			rootMount: rootMount,
			registry: registry,
			kernelCommandLine: kernelCommandLine,
			terminal: terminal,
			lifecycleStore: lifecycleStore
		)
	}

	private func validateMaterializedRootImages(
		id: String,
		fileManager: FileManager
	) throws {
		let layout = try registry.layout(forEnvironmentID: id)
		try validateMaterializedRootImage(layout.baseImageURL, fileManager: fileManager)
		try validateMaterializedRootImage(layout.stateImageURL, fileManager: fileManager)
	}

	private func validateMaterializedRootImage(
		_ url: URL,
		fileManager: FileManager
	) throws {
		var isDirectory = ObjCBool(false)
		guard fileManager.fileExists(atPath: url.path, isDirectory: &isDirectory),
		      !isDirectory.boolValue
		else {
			throw OrlixOCIRuntimeError.missingMaterializedRootImage(url.path)
		}
	}
}

@_spi(OrlixPrivateTesting)
public struct OrlixOCIRuntimeProcessSession: Sendable {
	public let processHandle: OrlixOCIRuntimeProcessHandle
	let kernelSession: OrlixKernelSession
	public let lifecycleStore: OrlixOCIRuntimeLifecycleStore?

	init(
		processHandle: OrlixOCIRuntimeProcessHandle,
		kernelSession: OrlixKernelSession,
		lifecycleStore: OrlixOCIRuntimeLifecycleStore? = nil
	) {
		self.processHandle = processHandle
		self.kernelSession = kernelSession
		self.lifecycleStore = lifecycleStore
	}

	public init(
		lifecycle: OrlixOCIRuntimeLifecycleController,
		rootMount: OrlixEnvironmentRootMount,
		registry: OrlixEnvironmentRegistry,
		kernelCommandLine: String? = OrlixEnvironmentRootImage.defaultKernelCommandLine,
	    terminal: OrlixTerminalSession = OrlixTerminalSession(),
	    lifecycleStore: OrlixOCIRuntimeLifecycleStore? = nil
	) throws {
		let rootImageIdentifier = try Self.persistedRootImageIdentifier(
			for: lifecycle.record.id,
			in: registry
		)
		let processHandle = try OrlixOCIRuntimeProcessHandle(
			lifecycle: lifecycle,
			rootMount: rootMount,
			rootImageIdentifier: rootImageIdentifier
		)
		if !FileManager.default.fileExists(
			atPath: try registry.descriptorURL(
				forEnvironmentID: lifecycle.record.id
			).path
		) {
			try registry.save(processHandle.sessionDescriptor.environment)
		}
		let kernelSession = try OrlixKernelSession(
			ociRuntimeSession: processHandle.sessionDescriptor,
			registry: registry,
			kernelCommandLine: kernelCommandLine,
			terminal: terminal
		)
		let resolvedStore = lifecycleStore ?? OrlixOCIRuntimeLifecycleStore(
			registry: registry
		)
		try resolvedStore.save(processHandle.lifecycle)
		self.init(
			processHandle: processHandle,
			kernelSession: kernelSession,
			lifecycleStore: resolvedStore
		)
	}

	private static func persistedRootImageIdentifier(
		for environmentID: String,
		in registry: OrlixEnvironmentRegistry
	) throws -> String? {
		let descriptorURL = try registry.descriptorURL(forEnvironmentID: environmentID)
		guard FileManager.default.fileExists(atPath: descriptorURL.path) else {
			return nil
		}
		return try registry.load(environmentID: environmentID).rootImageIdentifier
	}

	public func start(observedPID pid: Int32) throws -> OrlixOCIRuntimeProcessSession {
        try start(
            observedProcess: OrlixOCIRuntimeProcessStartObservation(pid: pid)
        )
    }

	public func start(observedProcess observation: OrlixOCIRuntimeProcessStartObservation) throws -> OrlixOCIRuntimeProcessSession {
		let startedHandle = try processHandle.start(observedProcess: observation)
		try persist(startedHandle.lifecycle)
		return OrlixOCIRuntimeProcessSession(
			processHandle: startedHandle,
			kernelSession: kernelSession,
			lifecycleStore: lifecycleStore
		)
	}

    public func start(using driver: OrlixOCIRuntimeProcessObservationDriver) throws -> OrlixOCIRuntimeProcessSession {
        guard processHandle.lifecycle.record.state == .created else {
            throw OrlixOCIRuntimeLifecycleError.invalidTransition(
                from: processHandle.lifecycle.record.state,
                action: .start
            )
        }
        return try start(observedProcess: driver.start(processSession: self))
    }

	public func kill(signal: Int32) throws -> OrlixOCIRuntimeProcessSession {
		let signaledHandle = try processHandle.kill(signal: signal)
		try persist(signaledHandle.lifecycle)
		return OrlixOCIRuntimeProcessSession(
			processHandle: signaledHandle,
			kernelSession: kernelSession,
			lifecycleStore: lifecycleStore
		)
	}

	public func kill(signal: Int32, using driver: OrlixOCIRuntimeProcessObservationDriver) throws -> OrlixOCIRuntimeProcessSession {
		let signaledHandle = try processHandle.kill(signal: signal)
		try driver.signal(processSession: self, signal: signal)
		try persist(signaledHandle.lifecycle)
		return OrlixOCIRuntimeProcessSession(
			processHandle: signaledHandle,
			kernelSession: kernelSession,
			lifecycleStore: lifecycleStore
		)
	}

	public func exit(observedProcess observation: OrlixOCIRuntimeProcessExitObservation) throws -> OrlixOCIRuntimeCompletedProcess {
		let completedProcess = try processHandle.exit(observedProcess: observation)
		try persist(completedProcess.lifecycle)
		return completedProcess
	}

	public func exit(observedSignal observation: OrlixOCIRuntimeProcessSignalObservation) throws -> OrlixOCIRuntimeCompletedProcess {
		let completedProcess = try processHandle.exit(observedSignal: observation)
		try persist(completedProcess.lifecycle)
		return completedProcess
	}

	public func exit(observedCompletion observation: OrlixOCIRuntimeProcessCompletionObservation) throws -> OrlixOCIRuntimeCompletedProcess {
		let completedProcess = try processHandle.exit(observedCompletion: observation)
		try persist(completedProcess.lifecycle)
		return completedProcess
	}

    public func wait(using driver: OrlixOCIRuntimeProcessObservationDriver) throws -> OrlixOCIRuntimeCompletedProcess {
        guard processHandle.lifecycle.record.state == .running else {
            throw OrlixOCIRuntimeLifecycleError.invalidTransition(
                from: processHandle.lifecycle.record.state,
                action: .exit
            )
        }
        return try exit(observedCompletion: driver.wait(processSession: self))
    }

    public func run(using driver: OrlixOCIRuntimeProcessObservationDriver) throws -> OrlixOCIRuntimeCompletedProcess {
        try runObserved(using: driver).completedProcess
    }

	public func runObserved(using driver: OrlixOCIRuntimeProcessObservationDriver) throws -> OrlixOCIRuntimeProcessRunResult {
		guard processHandle.lifecycle.record.state == .created else {
			throw OrlixOCIRuntimeLifecycleError.invalidTransition(
				from: processHandle.lifecycle.record.state,
				action: .start
			)
		}
		let startObservation = try driver.start(processSession: self)
		let runningSession = try start(observedProcess: startObservation)
		let completionObservation = try driver.wait(processSession: runningSession)
		let completedProcess = try runningSession.exit(observedCompletion: completionObservation)

		return OrlixOCIRuntimeProcessRunResult(
			startObservation: startObservation,
			runningSession: runningSession,
			completionObservation: completionObservation,
			completedProcess: completedProcess
		)
	}

	private func persist(_ lifecycle: OrlixOCIRuntimeLifecycleController) throws {
		try lifecycleStore?.save(lifecycle)
	}
}

enum OrlixTerminalMuxEncoder {
	static let maximumPayloadSize = 4096

	static func frame(type: UInt8, payload: Data) -> Data? {
		guard payload.count <= maximumPayloadSize else { return nil }
		var frame = Data([1, type, 0, 0])
		let length = UInt32(payload.count)
		frame.append(contentsOf: [
			UInt8((length >> 24) & 0xff), UInt8((length >> 16) & 0xff),
			UInt8((length >> 8) & 0xff), UInt8(length & 0xff),
		])
		frame.append(payload)
		var encoded = Data([0xc0])
		for byte in frame {
			switch byte {
			case 0xc0: encoded.append(contentsOf: [0xdb, 0xdc])
			case 0xdb: encoded.append(contentsOf: [0xdb, 0xdd])
			default: encoded.append(byte)
			}
		}
		encoded.append(0xc0)
		return encoded
	}
}

private final class HostConsolePaneTransport:
    OrlixPaneTransport,
    @unchecked Sendable
{
    private let pipe = Pipe()
    private let lock = NSLock()
    private var outputHandlers: [UUID: @Sendable (Data) -> Void] = [:]
	private var selectedSource: UInt32?

    init() {
        pipe.fileHandleForReading.readabilityHandler = { [weak self] handle in
            let data = handle.availableData
            guard !data.isEmpty else {
                return
            }
            self?.emit(data)
        }
    }

    deinit {
        pipe.fileHandleForReading.readabilityHandler = nil
		if let selectedSource {
			orlix_host_console_set_output_fd(selectedSource, -1)
		}
    }

    func attachOutput(
        _ handler: @escaping @Sendable (Data) -> Void
    ) -> OrlixTerminalOutput {
        let id = UUID()
        lock.lock()
        outputHandlers[id] = handler
        lock.unlock()

        return OrlixTerminalOutput { [weak self] in
            self?.removeOutputHandler(id)
        }
    }

	func send(_ data: Data) {
		if data.isEmpty {
			sendFrame(type: 1, payload: data)
			return
		}
		var offset = 0
		while offset < data.count {
			let end = min(offset + OrlixTerminalMuxEncoder.maximumPayloadSize,
				      data.count)
			sendFrame(type: 1, payload: data.subdata(in: offset..<end))
			offset = end
		}
	}

	func resize(rows: UInt32, columns: UInt32) {
		guard rows > 0, rows <= UInt32(UInt16.max),
		      columns > 0, columns <= UInt32(UInt16.max)
		else {
			return
		}

		let payload = Data([
			UInt8((rows >> 8) & 0xff), UInt8(rows & 0xff),
			UInt8((columns >> 8) & 0xff), UInt8(columns & 0xff),
		])
		sendFrame(type: 2, payload: payload)
	}

	func configureOutputSource(_ source: UInt32) -> Bool {
		guard source == COrlixHostConsoleSource.serial ||
		      source == COrlixHostConsoleSource.virtio else { return false }
		lock.lock()
		let previous = selectedSource
		selectedSource = source
		lock.unlock()
		if let previous, previous != source {
			orlix_host_console_set_output_fd(previous, -1)
		}
		orlix_host_console_set_output_fd(
			source, pipe.fileHandleForWriting.fileDescriptor
		)
		orlix_host_console_clear_input(source)
		return true
	}

	func clearRecentOutput() {
		guard let selectedSource else { return }
		orlix_host_console_recent_output_clear(selectedSource)
	}

	func recentOutput() -> Data {
		guard let selectedSource else { return Data() }
		let capacity = 64 * 1024
		var bytes = [UInt8](repeating: 0, count: capacity)
		let count = bytes.withUnsafeMutableBytes { buffer in
			orlix_host_console_recent_output_snapshot(
				selectedSource, buffer.baseAddress, UInt(capacity)
			)
		}
		return Data(bytes.prefix(Int(count)))
	}

	private func sendFrame(type: UInt8, payload: Data) {
		guard let encoded = OrlixTerminalMuxEncoder.frame(
			type: type, payload: payload
		) else { return }
		enqueue(encoded)
	}

	private func enqueue(_ data: Data) {
		guard let selectedSource else { return }
		data.withUnsafeBytes { buffer in
			guard let baseAddress = buffer.baseAddress else { return }
			var offset = 0
			while offset < buffer.count {
				let written = orlix_host_console_enqueue_input(
					selectedSource,
					baseAddress.advanced(by: offset), UInt(buffer.count - offset)
				)
				guard written > 0 else { return }
				offset += Int(written)
			}
		}
	}

	private func emit(_ data: Data) {
        lock.lock()
        let handlers = Array(outputHandlers.values)
        lock.unlock()

        for handler in handlers {
            handler(data)
        }
    }

    private func removeOutputHandler(_ id: UUID) {
        lock.lock()
        outputHandlers[id] = nil
        lock.unlock()
    }
}
