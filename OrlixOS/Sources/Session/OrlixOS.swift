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
        try self.init(
            environmentID: ociRuntimeSession.environment.id,
            registry: registry,
            kernelCommandLine: kernelCommandLine,
            terminal: terminal
        )
    }

    convenience init(
        ociRuntimeBundle: OrlixOCIRuntimeBundle,
        id: String,
        rootMount: OrlixEnvironmentRootMount,
        registry: OrlixEnvironmentRegistry,
        kernelCommandLine: String = OrlixEnvironmentRootImage.defaultKernelCommandLine,
        terminal: OrlixTerminalSession
    ) throws {
        let ociRuntimeSession = try ociRuntimeBundle.sessionDescriptor(
            id: id,
            rootMount: rootMount
        )
        try registry.save(ociRuntimeSession.environment)

        let resolvedCommandLine = try OrlixEnvironmentRootImage.materializedKernelCommandLine(
            descriptor: ociRuntimeSession.environment,
            kernelCommandLine: kernelCommandLine
        )
        self.init(
            bootConfig: OrlixBootConfig(
                profile: .development,
                kernelCommandLine: resolvedCommandLine,
                rootImageIdentifier: ociRuntimeSession.environment.rootImageIdentifier
            ),
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
public enum OrlixOCIRuntimeError: Error, Equatable, Sendable {
	case environmentAlreadyExists(String)
	case missingMaterializedRootImage(String)
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

	public func run(
		id: String,
		rootMount: OrlixEnvironmentRootMount = .defaultOverlay,
		kernelCommandLine: String? = OrlixEnvironmentRootImage.defaultKernelCommandLine,
		terminal: OrlixTerminalSession = OrlixTerminalSession(),
		using driver: OrlixOCIRuntimeProcessObservationDriver,
		fileManager: FileManager = .default
	) throws -> OrlixOCIRuntimeRunResult {
		let processSession = try processSession(
			snapshot: try lifecycleStore.load(id: id, fileManager: fileManager),
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
		rootMount: OrlixEnvironmentRootMount,
		kernelCommandLine: String?,
		terminal: OrlixTerminalSession,
		fileManager: FileManager
	) throws -> OrlixOCIRuntimeProcessSession {
		try processSession(
			snapshot: try lifecycleStore.load(id: id, fileManager: fileManager),
			rootMount: rootMount,
			kernelCommandLine: kernelCommandLine,
			terminal: terminal,
			fileManager: fileManager
		)
	}

	private func processSession(
		snapshot: OrlixOCIRuntimeLifecycleSnapshot,
		rootMount: OrlixEnvironmentRootMount,
		kernelCommandLine: String?,
		terminal: OrlixTerminalSession,
		fileManager: FileManager
	) throws -> OrlixOCIRuntimeProcessSession {
		try validateMaterializedRootImages(
			id: snapshot.record.id,
			fileManager: fileManager
		)
		let bundle = try OrlixOCIRuntimeBundle.load(
			from: URL(fileURLWithPath: snapshot.record.bundlePath),
			fileManager: fileManager
		)
		let lifecycle = OrlixOCIRuntimeLifecycleController(
			config: bundle.config,
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
		let processHandle = try OrlixOCIRuntimeProcessHandle(
			lifecycle: lifecycle,
			rootMount: rootMount
		)
		try registry.save(processHandle.sessionDescriptor.environment)
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
