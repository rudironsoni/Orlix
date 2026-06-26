import Foundation

@_silgen_name("OrlixBoot")
private func OrlixBoot(_ config: UnsafePointer<COrlixBootConfig>) -> CInt

@_silgen_name("orlix_host_console_set_output_fd")
private func orlix_host_console_set_output_fd(_ fd: CInt)

@_silgen_name("orlix_host_console_enqueue_input")
private func orlix_host_console_enqueue_input(
    _ bytes: UnsafeRawPointer?,
    _ length: UInt
) -> UInt

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
            return "Orlix boot handoff is not wired to iOS-hosted Linux execution yet."
        case .alreadyStarted:
            return "Orlix boot already started in this process."
        case .unknown:
            return "Orlix bootloader returned an unknown status."
        }
    }
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
        OrlixOSPayload.selectedBootProfile
    }

    public static var bundledKernelCommandLine: String? {
        OrlixOSPayload.kernelCommandLine
    }

    public static var productRootImageIdentifier: String? {
        OrlixOSPayload.productRootImageIdentifier
    }

    @_spi(OrlixPrivateTesting)
    public static func rootImageDescriptor(
        forRole role: String
    ) -> OrlixRootImageDescriptor? {
        OrlixOSPayload.rootImageDescriptors.first { $0.role == role }
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

enum OrlixOSPayload {
    private static let bundleNameKey = "OrlixOSPayloadBundleName"
    private static let bundleExtensionKey = "OrlixOSPayloadBundleExtension"
    private static let payloadProfileInfoKeyKey =
        "OrlixOSPayloadProfileInfoKey"
    private static let payloadKernelCommandLineInfoKeyKey =
        "OrlixOSPayloadKernelCommandLineInfoKey"
    private static let payloadRootInitrdInfoKeyKey =
        "OrlixOSPayloadRootInitramfsInfoKey"
    private static let payloadBaseRootImageInfoKeyKey =
        "OrlixOSPayloadBaseRootImageInfoKey"
    private static let payloadStateRootImageInfoKeyKey =
        "OrlixOSPayloadStateRootImageInfoKey"
    private static let payloadBaseRootDeviceInfoKeyKey =
        "OrlixOSPayloadBaseRootDeviceInfoKey"
    private static let payloadStateRootDeviceInfoKeyKey =
        "OrlixOSPayloadStateRootDeviceInfoKey"
    private static let payloadBaseRootHostBlockDeviceInfoKeyKey =
        "OrlixOSPayloadBaseRootHostBlockDeviceInfoKey"
    private static let payloadStateRootHostBlockDeviceInfoKeyKey =
        "OrlixOSPayloadStateRootHostBlockDeviceInfoKey"
    private static let payloadStateRootMinimumBytesInfoKeyKey =
        "OrlixOSPayloadStateRootMinimumBytesInfoKey"
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

    private struct PayloadMetadataSchema {
        let selectedProfileKey: String
        let kernelCommandLineKey: String
        let rootInitrdResourceKey: String
        let baseRootImageResourceKey: String
        let stateRootImageResourceKey: String
        let baseRootDeviceKey: String
        let stateRootDeviceKey: String
        let baseRootHostBlockDeviceKey: String
        let stateRootHostBlockDeviceKey: String
        let stateRootMinimumBytesKey: String
    }

    struct ProductRootResources {
        let initrdResource: String
        let baseBlockResource: String
        let stateBlockResource: String
        let baseBlockDevice: UInt32
        let stateBlockDevice: UInt32
        let stateBlockMinimumBytes: UInt64
    }

    static var bundleURL: URL? {
        guard let name = bundleMetadataValue(for: bundleNameKey),
              let extensionName = bundleMetadataValue(for: bundleExtensionKey)
        else {
            return nil
        }

        return frameworkBundle.url(
            forResource: name,
            withExtension: extensionName
        )
    }

    static var selectedBootProfile: OrlixBootProfile? {
        guard let payloadBundleURL = bundleURL,
              let payloadBundle = Bundle(url: payloadBundleURL),
              let schema = payloadMetadataSchema,
              let profile = payloadBundle.object(
                forInfoDictionaryKey: schema.selectedProfileKey
              ) as? String
        else {
            return nil
        }

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
        guard let payloadBundleURL = bundleURL,
              let payloadBundle = Bundle(url: payloadBundleURL),
              let schema = payloadMetadataSchema
        else {
            return nil
        }

        return payloadMetadataValue(schema.kernelCommandLineKey, in: payloadBundle)
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
        guard let payloadBundlePath = bundleURL?.path else {
            return false
        }
        guard let productResources = productResources,
              let productRootImageIdentifier = productRootImageIdentifier,
              rootImageDescriptors.contains(
                where: { $0.identifier == productRootImageIdentifier }
              )
        else {
            return false
        }

        guard payloadBundlePath.withCString({ path in
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
        guard let payloadBundlePath = bundleURL?.path,
              let productResources = productResources
        else {
            return false
        }
        return registerMaterializedRootImage(
            rootImage,
            payloadBundlePath: payloadBundlePath,
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
        payloadBundlePath: String,
        productResources: ProductRootResources
    ) -> Bool {
        guard payloadBundlePath.withCString({ path in
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
        guard let payloadBundleURL = bundleURL,
              let payloadBundle = Bundle(url: payloadBundleURL),
              let schema = payloadMetadataSchema,
              let initrdResource = payloadMetadataValue(
                schema.rootInitrdResourceKey,
                in: payloadBundle
              ),
              let baseBlockResource = payloadMetadataValue(
                schema.baseRootImageResourceKey,
                in: payloadBundle
              ),
              let stateBlockResource = payloadMetadataValue(
                schema.stateRootImageResourceKey,
                in: payloadBundle
              ),
              payloadMetadataValue(schema.baseRootDeviceKey, in: payloadBundle) != nil,
              payloadMetadataValue(schema.stateRootDeviceKey, in: payloadBundle) != nil,
              let baseBlockDevice = payloadMetadataUInt32(
                schema.baseRootHostBlockDeviceKey,
                in: payloadBundle
              ),
              let stateBlockDevice = payloadMetadataUInt32(
                schema.stateRootHostBlockDeviceKey,
                in: payloadBundle
              ),
              let stateBlockMinimumBytes = payloadMetadataUInt64(
                schema.stateRootMinimumBytesKey,
                in: payloadBundle
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

    private static var payloadMetadataSchema: PayloadMetadataSchema? {
        guard let selectedProfileKey = bundleMetadataValue(
                for: payloadProfileInfoKeyKey
              ),
              let kernelCommandLineKey = bundleMetadataValue(
                for: payloadKernelCommandLineInfoKeyKey
              ),
              let rootInitrdResourceKey = bundleMetadataValue(
                for: payloadRootInitrdInfoKeyKey
              ),
              let baseRootImageResourceKey = bundleMetadataValue(
                for: payloadBaseRootImageInfoKeyKey
              ),
              let stateRootImageResourceKey = bundleMetadataValue(
                for: payloadStateRootImageInfoKeyKey
              ),
              let baseRootDeviceKey = bundleMetadataValue(
                for: payloadBaseRootDeviceInfoKeyKey
              ),
              let stateRootDeviceKey = bundleMetadataValue(
                for: payloadStateRootDeviceInfoKeyKey
              ),
              let baseRootHostBlockDeviceKey = bundleMetadataValue(
                for: payloadBaseRootHostBlockDeviceInfoKeyKey
              ),
              let stateRootHostBlockDeviceKey = bundleMetadataValue(
                for: payloadStateRootHostBlockDeviceInfoKeyKey
              ),
              let stateRootMinimumBytesKey = bundleMetadataValue(
                for: payloadStateRootMinimumBytesInfoKeyKey
              )
        else {
            return nil
        }

        return PayloadMetadataSchema(
            selectedProfileKey: selectedProfileKey,
            kernelCommandLineKey: kernelCommandLineKey,
            rootInitrdResourceKey: rootInitrdResourceKey,
            baseRootImageResourceKey: baseRootImageResourceKey,
            stateRootImageResourceKey: stateRootImageResourceKey,
            baseRootDeviceKey: baseRootDeviceKey,
            stateRootDeviceKey: stateRootDeviceKey,
            baseRootHostBlockDeviceKey: baseRootHostBlockDeviceKey,
            stateRootHostBlockDeviceKey: stateRootHostBlockDeviceKey,
            stateRootMinimumBytesKey: stateRootMinimumBytesKey
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

    private static func payloadMetadataValue(
        _ key: String,
        in bundle: Bundle
    ) -> String? {
        guard let value = bundle.object(forInfoDictionaryKey: key) as? String,
              !value.isEmpty
        else {
            return nil
        }

        return value
    }

    private static func payloadMetadataUInt32(
        _ key: String,
        in bundle: Bundle
    ) -> UInt32? {
        guard let value = payloadMetadataUInt64(key, in: bundle),
              value <= UInt64(UInt32.max)
        else {
            return nil
        }

        return UInt32(value)
    }

    private static func payloadMetadataUInt64(
        _ key: String,
        in bundle: Bundle
    ) -> UInt64? {
        if let value = bundle.object(forInfoDictionaryKey: key) as? NSNumber {
            let intValue = value.int64Value
            guard intValue >= 0 else {
                return nil
            }
            return UInt64(intValue)
        }
        guard let text = payloadMetadataValue(key, in: bundle) else {
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
}

protocol OrlixTerminalTransport: AnyObject {
    func attachOutput(
        _ handler: @escaping @Sendable (Data) -> Void
    ) -> OrlixTerminalOutput
    func send(_ data: Data)
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
    private let transport: OrlixTerminalTransport

    public convenience init() {
        self.init(transport: HostConsoleTerminalTransport())
    }

    init(transport: OrlixTerminalTransport) {
        self.transport = transport
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
}

public final class OrlixLinuxSession: @unchecked Sendable {
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
		let terminalToken = session.terminal ? "orlix.terminal=1" : "orlix.terminal=0"
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
        guard registerRootImagesForBoot() else {
            return .invalidConfig
        }

        return bootConfig.rootImageIdentifier.withCString { rootImageIdentifier in
            bootConfig.terminalIdentifier.withCString { terminalIdentifier in
                let boot = { (kernelCommandLine: UnsafePointer<CChar>?) in
                    var cConfig = COrlixBootConfig(
                        profile: self.bootConfig.profile.cValue,
                        kernelCommandLine: kernelCommandLine,
                        rootImageIdentifier: rootImageIdentifier,
                        terminalIdentifier: terminalIdentifier
                    )
                    return OrlixBootStatus(rawStatus: OrlixBoot(&cConfig))
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

    private func registerRootImagesForBoot() -> Bool {
        if let materializedRootImage {
            return materializedRootImage.registerWithHostAdapter()
        }
        guard OrlixOSPayload.registerWithHostAdapter() else {
            return false
        }
        return OrlixOSPayload.registerAdditionalHostDirectoriesForTesting(
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
public enum OrlixOCIRuntimeLinuxSessionObservationError: Error, Equatable, Sendable {
	case bootFailed(OrlixBootStatus)
	case timedOutWaitingForStart
	case timedOutWaitingForCompletion
	case signalUnsupported
}

@_spi(OrlixPrivateTesting)
public final class OrlixOCIRuntimeLinuxSessionObservationDriver:
	OrlixOCIRuntimeProcessObservationDriver,
	@unchecked Sendable
{
	private let timeout: TimeInterval
	private let bootSession: @Sendable (OrlixLinuxSession) -> OrlixBootStatus
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
		bootSession: @escaping @Sendable (OrlixLinuxSession) -> OrlixBootStatus
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

		output = processSession.linuxSession.terminal.attachOutput { [weak self] data in
			self?.append(data)
		}

		DispatchQueue.global(qos: .userInitiated).async { [bootSession] in
			let status = bootSession(processSession.linuxSession)
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
            throw OrlixOCIRuntimeLinuxSessionObservationError.signalUnsupported
        }

        processSession.linuxSession.terminal.send(Data([controlCharacter]))
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
				throw OrlixOCIRuntimeLinuxSessionObservationError
					.bootFailed(bootStatus)
			}
			if !condition.wait(until: deadline) {
				throw OrlixOCIRuntimeLinuxSessionObservationError
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
				throw OrlixOCIRuntimeLinuxSessionObservationError
					.timedOutWaitingForCompletion
			}
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

public struct OrlixOCIEnvironmentMaterializationTools: Equatable, Sendable {
	public let mke2fs: URL
	public let truncate: URL
	public let debugfs: URL

	public init(mke2fs: URL, truncate: URL, debugfs: URL) {
		self.mke2fs = mke2fs
		self.truncate = truncate
		self.debugfs = debugfs
	}
}

public struct OrlixOCIEnvironmentInstallResult: Sendable {
	public let id: String
	public let bundleURL: URL
	public let stateReport: OrlixOCIRuntimeStateReport
}

public struct OrlixOCIRegistryEnvironmentInstallResult: Sendable {
	public let id: String
	public let image: OrlixOCIRegistryImageReference
	public let pullResult: OrlixOCIRegistryPullResult
	public let stateReport: OrlixOCIRuntimeStateReport
}

public struct OrlixOCIEnvironmentRunResult: Sendable {
	public let id: String
	public let startedStateReport: OrlixOCIRuntimeStateReport
	public let completedStateReport: OrlixOCIRuntimeStateReport
}

public struct OrlixOCIEnvironmentStartResult: Sendable {
	public let id: String
	public let stateReport: OrlixOCIRuntimeStateReport
}

public struct OrlixOCIEnvironmentWaitResult: Sendable {
    public let id: String
    public let stateReport: OrlixOCIRuntimeStateReport
}

public struct OrlixOCIEnvironmentSignalResult: Sendable {
    public let id: String
    public let signal: Int32
    public let stateReport: OrlixOCIRuntimeStateReport
}

public struct OrlixOCIEnvironmentInstallRunResult: Sendable {
    public let installResult: OrlixOCIEnvironmentInstallResult
    public let runResult: OrlixOCIEnvironmentRunResult
}

public struct OrlixOCIRegistryEnvironmentInstallRunResult: Sendable {
	public let installResult: OrlixOCIRegistryEnvironmentInstallResult
	public let runResult: OrlixOCIEnvironmentRunResult
}

public struct OrlixOCIRegistryEnvironmentTerminalSessionResult: Sendable {
	public let installResult: OrlixOCIRegistryEnvironmentInstallResult
	public let linuxSession: OrlixLinuxSession
}

public struct OrlixOCIEnvironmentTerminalSessionResult: Sendable {
    public let id: String
    public let image: OrlixOCIRegistryImageReference
    public let command: [String]?
    public let linuxSession: OrlixLinuxSession
}

public struct OrlixOCIEnvironmentPreparedState: Sendable {
    public let id: String
    public let platform: String
    public let defaultCommand: [String]
    public let lifecycleState: OrlixOCIRuntimeLifecycleState
    public let stateReport: OrlixOCIRuntimeStateReport?
}

public struct OrlixOCIEnvironmentDeleteResult: Sendable {
    public let id: String
    public let lifecycleState: OrlixOCIRuntimeLifecycleState
}

public enum OrlixOCIEnvironmentRunArgumentsError: Error, Equatable, Sendable {
	case missingRunCommand
	case missingImage
	case missingOptionValue(String)
	case unknownOption(String)
}

public struct OrlixOCIEnvironmentRunArguments: Equatable, Sendable {
	public let image: String
	public let id: String
	public let platform: String
	public let command: [String]?

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
		var parsedImage: String?
		var parsedCommand: [String] = []

		while !values.isEmpty {
			let value = values.removeFirst()
			if value == "--" {
				parsedCommand = values
				values.removeAll()
				break
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
		self.command = parsedCommand.isEmpty ? nil : parsedCommand
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

public struct OrlixOCIEnvironmentInstaller: Sendable {
	private let registry: OrlixEnvironmentRegistry

    public init() throws {
        self.registry = try OrlixEnvironmentRegistry()
    }

    @_spi(OrlixPrivateTesting)
	public init(registry: OrlixEnvironmentRegistry) {
        self.registry = registry
    }

    public func listPreparedEnvironments(
        fileManager: FileManager = .default
    ) throws -> [OrlixOCIEnvironmentPreparedState] {
        try OrlixOCIRuntime(registry: registry)
            .listPreparedEnvironments(fileManager: fileManager)
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
		fileManager: FileManager = .default,
		runCommand: @escaping @Sendable (URL, [String]) throws -> Void
	) async throws -> OrlixOCIRegistryEnvironmentInstallResult {
		try await install(
			image: OrlixOCIRegistryImageReference(image),
			id: id,
			tools: tools,
			puller: puller,
			platform: platform,
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
			_ = try importResult.materializationPlan.materialize(
				mke2fsExecutable: tools.mke2fs.path,
				truncateExecutable: tools.truncate.path,
				debugfsExecutable: tools.debugfs.path,
				runner: OrlixOCIEnvironmentInstallerCommandRunner(
					runCommand: runCommand
				)
			)
			let config = try registryLifecycleConfig(for: importResult.descriptor)
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
		return try await run(
			image: request.image,
			id: request.id,
			tools: tools,
			puller: puller,
			platform: request.platform,
			command: request.command,
			terminal: terminal,
			observationTimeout: observationTimeout,
			fileManager: fileManager,
			runCommand: runCommand
		)
	}

	@discardableResult
	public func prepareTerminalSession(
		arguments: [String],
		tools: OrlixOCIEnvironmentMaterializationTools,
		puller: OrlixOCIRegistryPuller = OrlixOCIRegistryPuller(),
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		fileManager: FileManager = .default,
		runCommand: @escaping @Sendable (URL, [String]) throws -> Void
	) async throws -> OrlixOCIRegistryEnvironmentTerminalSessionResult {
		let request = try OrlixOCIEnvironmentRunArguments(arguments)
		return try await prepareTerminalSession(
			image: try OrlixOCIRegistryImageReference(request.image),
			id: request.id,
			tools: tools,
			puller: puller,
			platform: request.platform,
			command: request.command,
			terminal: terminal,
			fileManager: fileManager,
			runCommand: runCommand
		)
	}

	public func terminalSession(
		arguments: [String],
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		fileManager: FileManager = .default
	) throws -> OrlixOCIEnvironmentTerminalSessionResult {
		let request = try OrlixOCIEnvironmentRunArguments(arguments)
		let image = try OrlixOCIRegistryImageReference(request.image)
		let linuxSession = try OrlixOCIRuntime(registry: registry).terminalSession(
			id: request.id,
			command: request.command,
			terminal: terminal,
			fileManager: fileManager
		)
		return OrlixOCIEnvironmentTerminalSessionResult(
			id: request.id,
			image: image,
			command: request.command,
			linuxSession: linuxSession
		)
	}

	@discardableResult
	public func prepareTerminalSession(
		image: String,
		id: String,
		tools: OrlixOCIEnvironmentMaterializationTools,
		puller: OrlixOCIRegistryPuller = OrlixOCIRegistryPuller(),
		platform: String = "linux/arm64",
		command: [String]? = nil,
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
			terminal: terminal,
			fileManager: fileManager,
			runCommand: runCommand
		)
	}

	@discardableResult
	public func prepareTerminalSession(
		image: OrlixOCIRegistryImageReference,
		id: String,
		tools: OrlixOCIEnvironmentMaterializationTools,
		puller: OrlixOCIRegistryPuller = OrlixOCIRegistryPuller(),
		platform: String = "linux/arm64",
		command: [String]? = nil,
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
			fileManager: fileManager,
			runCommand: runCommand
		)
		let linuxSession = try OrlixOCIRuntime(registry: registry).terminalSession(
			id: id,
			command: command,
			terminal: terminal,
			fileManager: fileManager
		)
		return OrlixOCIRegistryEnvironmentTerminalSessionResult(
			installResult: installResult,
			linuxSession: linuxSession
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
			runResult: runResult
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
		return try await run(
			image: OrlixOCIRegistryImageReference(request.image),
			id: request.id,
			tools: tools,
			puller: puller,
			platform: request.platform,
			command: request.command,
			terminal: terminal,
			using: driver,
			fileManager: fileManager,
			runCommand: runCommand
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
			runResult: runResult
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

	public func session(
		bundleURL: URL,
		id: String,
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		fileManager: FileManager = .default
	) throws -> OrlixLinuxSession {
		try OrlixLinuxSession(
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

	public func session(
		id: String,
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		fileManager: FileManager = .default
	) throws -> OrlixLinuxSession {
		try OrlixLinuxSession(
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
            using: OrlixOCIRuntimeLinuxSessionObservationDriver(
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
		let config = [
			"ociVersion": "1.1.0",
			"root": [
				"path": "rootfs",
			],
			"process": [
				"terminal": false,
				"args": descriptor.defaultCommand,
				"env": environment,
				"cwd": descriptor.defaultWorkingDirectory,
				"user": [
					"uid": descriptor.defaultUserID,
					"gid": descriptor.defaultGroupID,
				],
			],
		] as [String: Any]
		let data = try JSONSerialization.data(
			withJSONObject: config,
			options: [.sortedKeys]
		)
		return try OrlixOCIRuntimeConfigParser().parse(data)
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

    public func terminalSession(
        id: String,
        command: [String]? = nil,
        rootMount: OrlixEnvironmentRootMount = .defaultOverlay,
		kernelCommandLine: String? = OrlixEnvironmentRootImage.defaultKernelCommandLine,
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		fileManager: FileManager = .default
	) throws -> OrlixLinuxSession {
		try processSession(
			id: id,
			command: command,
			rootMount: rootMount,
			kernelCommandLine: kernelCommandLine,
			terminal: terminal,
			fileManager: fileManager
		).linuxSession
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
			using: OrlixOCIRuntimeLinuxSessionObservationDriver(
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
			using: OrlixOCIRuntimeLinuxSessionObservationDriver(
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
			using: OrlixOCIRuntimeLinuxSessionObservationDriver(
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
			kernelCommandLine: kernelCommandLine,
			terminal: terminal,
			materializationRunner: materializationRunner,
			processDriver: OrlixOCIRuntimeLinuxSessionObservationDriver(
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
			kernelCommandLine: kernelCommandLine,
			terminal: terminal,
			materializationRunner: materializationRunner,
			processDriver: OrlixOCIRuntimeLinuxSessionObservationDriver(
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
	public let linuxSession: OrlixLinuxSession
	public let lifecycleStore: OrlixOCIRuntimeLifecycleStore?

	public init(
		processHandle: OrlixOCIRuntimeProcessHandle,
		linuxSession: OrlixLinuxSession,
		lifecycleStore: OrlixOCIRuntimeLifecycleStore? = nil
	) {
		self.processHandle = processHandle
		self.linuxSession = linuxSession
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
		let linuxSession = try OrlixLinuxSession(
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
			linuxSession: linuxSession,
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
			linuxSession: linuxSession,
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
			linuxSession: linuxSession,
			lifecycleStore: lifecycleStore
		)
	}

	public func kill(signal: Int32, using driver: OrlixOCIRuntimeProcessObservationDriver) throws -> OrlixOCIRuntimeProcessSession {
		let signaledHandle = try processHandle.kill(signal: signal)
		try driver.signal(processSession: self, signal: signal)
		try persist(signaledHandle.lifecycle)
		return OrlixOCIRuntimeProcessSession(
			processHandle: signaledHandle,
			linuxSession: linuxSession,
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

private final class HostConsoleTerminalTransport:
    OrlixTerminalTransport,
    @unchecked Sendable
{
    private let pipe = Pipe()
    private let lock = NSLock()
    private var outputHandlers: [UUID: @Sendable (Data) -> Void] = [:]

    init() {
        pipe.fileHandleForReading.readabilityHandler = { [weak self] handle in
            let data = handle.availableData
            guard !data.isEmpty else {
                return
            }
            self?.emit(data)
        }
        orlix_host_console_set_output_fd(
            pipe.fileHandleForWriting.fileDescriptor
        )
    }

    deinit {
        pipe.fileHandleForReading.readabilityHandler = nil
        orlix_host_console_set_output_fd(-1)
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
        data.withUnsafeBytes { buffer in
            guard let baseAddress = buffer.baseAddress else {
                return
            }
            _ = orlix_host_console_enqueue_input(
                baseAddress,
                UInt(buffer.count)
            )
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
