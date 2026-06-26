import Foundation
@_spi(OrlixPrivateTesting) import OrlixOS

enum OrlixUpstreamTestSuite: String, Sendable {
    case kernel
    case mlibc
    case coreutils
}

struct OrlixUpstreamTestRunSpec: Equatable, Sendable {
    let suite: OrlixUpstreamTestSuite
    let completionMarker: String
    let expectedCoreutilsTotal: Int?
    let timeout: TimeInterval
    let kernelCommandLineSuffix: String?
    let hostDirectoryFixture: Bool

    init(
        suite: OrlixUpstreamTestSuite,
        completionMarker: String,
        expectedCoreutilsTotal: Int?,
        timeout: TimeInterval,
        kernelCommandLineSuffix: String?,
        hostDirectoryFixture: Bool = false
    ) {
        self.suite = suite
        self.completionMarker = completionMarker
        self.expectedCoreutilsTotal = expectedCoreutilsTotal
        self.timeout = timeout
        self.kernelCommandLineSuffix = kernelCommandLineSuffix
        self.hostDirectoryFixture = hostDirectoryFixture
    }

    func bootConfig(rootImage: OrlixRootImageDescriptor) throws
        -> OrlixBootConfig
    {
        guard let profile = OrlixOSDistribution.bundledBootProfile else {
            throw OrlixUpstreamTestRunError.missingBundledBootProfile
        }

        let commandLine: String?
        if let kernelCommandLineSuffix {
            let baseCommandLine = rootImage.kernelCommandLine ?? ""
            commandLine = [
                baseCommandLine,
                kernelCommandLineSuffix
            ].filter { !$0.isEmpty }.joined(separator: " ")
        } else {
            commandLine = rootImage.kernelCommandLine
        }

        let terminalIdentifierSuffix = String((kernelCommandLineSuffix ?? "default").map {
            $0.isLetter || $0.isNumber ? $0 : "."
        })

        return OrlixBootConfig(
            profile: profile,
            kernelCommandLine: commandLine,
            rootImageIdentifier: rootImage.identifier,
            terminalIdentifier: "orlix.test.\(suite.rawValue).\(terminalIdentifierSuffix).terminal"
        )
    }

    func rootImageDescriptor() throws -> OrlixRootImageDescriptor {
        guard let descriptor = OrlixOSDistribution.rootImageDescriptor(
            forRole: suite.rawValue
        ) else {
            throw OrlixUpstreamTestRunError.missingRootImageDescriptor(
                suite.rawValue
            )
        }

        return descriptor
    }

    static let kernel = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: nil
    )

    static let kernelMountNamespace = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=mount_namespace_probe"
    )

    static let kernelNamespace = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=namespace_probe"
    )

    static let kernelEnvironmentEntry = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=environment_entry_probe"
    )

    static let kernelInitExec = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=init_exec_probe"
    )

    static let kernelFDExec = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=fd_exec_probe"
    )

    static let kernelFDAlias = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=fd_alias_probe"
    )

    static let kernelSignalWait = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=signal_wait_probe"
    )

    static let kernelPipePoll = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=pipe_poll_probe"
    )

    static let kernelPipeSelect = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=pipe_select_probe"
    )

    static let kernelPipeEpoll = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=pipe_epoll_probe"
    )

    static let kernelPseudoFS = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=pseudo_fs_probe"
    )

    static let kernelCgroupV2 = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=cgroup_v2_probe"
    )

    static let kernelCgroupPids = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=cgroup_pids_probe"
    )

    static let kernelCgroupNamespace = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=cgroup_namespace_probe"
    )

    static let kernelUserNamespace = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=user_namespace_probe"
    )

    static let kernelOverlayFS = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=overlayfs_probe"
    )

    static let kernelTimeNamespace = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=time_namespace_probe"
    )

    static let kernelIPCNamespace = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=ipc_namespace_probe"
    )

    static let kernelPathErrno = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=path_errno_probe"
    )

    static let kernelCloneThread = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=clone_thread_probe"
    )

    static let kernelBootProfile = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=boot_profile_contract"
    )

    static let kernelRandomDevice = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=random_device_probe"
    )

    static let kernelVirtioBlockEnvironment = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=virtio_blk_environment_probe"
    )

    static let kernelVirtioMMIOContract = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=virtio_mmio_probe_contract"
    )

    static let kernelVirtioNetDevice = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=virtio_net_device_probe"
    )

    static let kernelVirtioFSMount = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=virtio_fs_mount_probe",
        hostDirectoryFixture: true
    )

    static let kernelNetworkNamespace = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=network_namespace_probe"
    )

    static let kernelRlimit = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=rlimit_probe"
    )

    static let kernelProcessCapability = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=process_capability_probe"
    )

    static let kernelProcessLifecycle = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=process_lifecycle_probe"
    )

    static let kernelUmask = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=umask_probe"
    )

    static let kernelReadonlyRoot = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=readonly_root_probe orlix.root.readonly=1"
    )

    static let kernelHostnameDomainname = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=hostname_domainname_probe orlix.hostname=oci-host orlix.domainname=oci.example"
    )

    static let kernelEnvironmentStateWriteback = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=environment_state_writeback_probe"
    )

    static let kernelEnvironmentStateCrossbootWrite = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix:
            "orlix.kselftest=environment_state_crossboot_write_probe"
    )

    static let kernelEnvironmentStateCrossbootVerify = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 300,
        kernelCommandLineSuffix:
            "orlix.kselftest=environment_state_crossboot_verify_probe"
    )

    static let mlibc = OrlixUpstreamTestRunSpec(
        suite: .mlibc,
        completionMarker: "ORLIX-MLIBC-TEST-END",
        expectedCoreutilsTotal: nil,
        timeout: 1_200,
        kernelCommandLineSuffix: nil
    )

    static let coreutils = OrlixUpstreamTestRunSpec(
        suite: .coreutils,
        completionMarker: "ORLIX-COREUTILS-TEST-END",
        expectedCoreutilsTotal: 733,
        timeout: 14_400,
        kernelCommandLineSuffix: nil
    )
}

enum OrlixUpstreamTestRunError: Error, Equatable, CustomStringConvertible {
    case missingRootImageDescriptor(String)
    case missingRootfsBundle(String)
    case missingBundledBootProfile
    case bootFailed(OrlixBootStatus)
    case bootAlreadyStarted
    case timeout(TimeInterval)
    case crashReport(String)
    case kernelPanic(String)
    case oom(String)
    case upstreamFailure(String, outputTail: String)
    case missingCompletionMarker(String)
    case malformedUpstreamOutput(String)
    case malformedCoreutilsCompletion(String)
    case coreutilsSummaryFailed(failures: Int, skips: Int, total: Int, expectedTotal: Int)

    var description: String {
        switch self {
        case let .missingRootImageDescriptor(role):
            return "missing OrlixOS root image metadata for role: \(role)"
        case let .missingRootfsBundle(bundle):
            return "missing upstream test rootfs bundle: \(bundle)"
        case .missingBundledBootProfile:
            return "missing OrlixOS payload boot profile metadata"
        case let .bootFailed(status):
            return "Orlix boot failed: \(status.message)"
        case .bootAlreadyStarted:
            return "Orlix boot already started in this process; run upstream runtime suites in process-isolated invocations."
        case let .timeout(timeout):
            return "timed out after \(Int(timeout)) seconds waiting for upstream test output"
        case let .crashReport(marker):
            return "host crash report marker found: \(marker)"
        case let .kernelPanic(marker):
            return "kernel panic marker found: \(marker)"
        case let .oom(marker):
            return "out-of-memory marker found: \(marker)"
        case let .upstreamFailure(line, outputTail):
            return "upstream failure marker found: \(line)\nrecent upstream output:\n\(outputTail)"
        case let .missingCompletionMarker(marker):
            return "missing upstream completion marker: \(marker)"
        case let .malformedUpstreamOutput(reason):
            return "malformed upstream output: \(reason)"
        case let .malformedCoreutilsCompletion(line):
            return "malformed Coreutils completion marker: \(line)"
        case let .coreutilsSummaryFailed(failures, skips, total, expectedTotal):
            return "Coreutils summary failed: failures=\(failures) skips=\(skips) total=\(total) expectedTotal=\(expectedTotal)"
        }
    }
}

final class OrlixUpstreamTestOutputParser {
    func containsTerminalCondition(
        _ rawOutput: String,
        for spec: OrlixUpstreamTestRunSpec
    ) -> Bool {
        let output = Self.normalized(rawOutput)

        return output.contains(spec.completionMarker) ||
            Self.firstFatalMarker(in: output) != nil ||
            Self.firstUpstreamFailureLine(in: output) != nil
    }

    func validate(
        _ rawOutput: String,
        for spec: OrlixUpstreamTestRunSpec
    ) throws {
        let output = Self.normalized(rawOutput)

        if let marker = Self.firstMarker(in: output, markers: Self.crashMarkers) {
            throw OrlixUpstreamTestRunError.crashReport(marker)
        }
        if let marker = Self.firstMarker(in: output, markers: Self.panicMarkers) {
            throw OrlixUpstreamTestRunError.kernelPanic(marker)
        }
        if let marker = Self.firstMarker(in: output, markers: Self.oomMarkers) {
            throw OrlixUpstreamTestRunError.oom(marker)
        }
        if let failure = Self.firstUpstreamFailureLine(in: output) {
            throw OrlixUpstreamTestRunError.upstreamFailure(
                failure,
                outputTail: Self.outputTail(output)
            )
        }
        guard output.contains(spec.completionMarker) else {
            throw OrlixUpstreamTestRunError.missingCompletionMarker(
                spec.completionMarker
            )
        }

        switch spec.suite {
        case .kernel, .mlibc:
            guard output.contains("TAP version 13") else {
                throw OrlixUpstreamTestRunError.malformedUpstreamOutput(
                    "missing TAP version 13"
                )
            }
            guard Self.containsTAPPlan(in: output) else {
                throw OrlixUpstreamTestRunError.malformedUpstreamOutput(
                    "missing TAP plan"
                )
            }
        case .coreutils:
            try validateCoreutilsCompletion(output, for: spec)
        }
    }

    private func validateCoreutilsCompletion(
        _ output: String,
        for spec: OrlixUpstreamTestRunSpec
    ) throws {
        guard let line = output
            .split(separator: "\n", omittingEmptySubsequences: false)
            .last(where: { $0.contains(spec.completionMarker) })
            .map(String.init)
        else {
            throw OrlixUpstreamTestRunError.missingCompletionMarker(
                spec.completionMarker
            )
        }

        let fields = line.split(separator: " ")
        guard
            fields.count == 4,
            fields[0] == Substring(spec.completionMarker),
            let failures = Self.value(after: "failures=", in: fields[1]),
            let skips = Self.value(after: "skips=", in: fields[2]),
            let total = Self.value(after: "total=", in: fields[3]),
            let expectedTotal = spec.expectedCoreutilsTotal
        else {
            throw OrlixUpstreamTestRunError.malformedCoreutilsCompletion(line)
        }

        guard failures == 0, skips == 0, total == expectedTotal else {
            throw OrlixUpstreamTestRunError.coreutilsSummaryFailed(
                failures: failures,
                skips: skips,
                total: total,
                expectedTotal: expectedTotal
            )
        }
    }

    private static func normalized(_ text: String) -> String {
        stripANSI(
            text.replacingOccurrences(of: "\r\n", with: "\n")
                .replacingOccurrences(of: "\r", with: "\n")
        )
    }

    private static func stripANSI(_ text: String) -> String {
        var output = ""
        var scalars = text.unicodeScalars.makeIterator()

        while let scalar = scalars.next() {
            if scalar.value != 0x1b {
                output.unicodeScalars.append(scalar)
                continue
            }

            guard let introducer = scalars.next() else {
                break
            }
            if introducer.value != 0x5b {
                continue
            }

            while let sequenceScalar = scalars.next() {
                if sequenceScalar.value >= 0x40 && sequenceScalar.value <= 0x7e {
                    break
                }
            }
        }

        return output
    }

    private static func firstMarker(
        in output: String,
        markers: [String]
    ) -> String? {
        markers.first { output.contains($0) }
    }

    private static func outputTail(_ output: String, maxLines: Int = 200) -> String {
        let lines = output.split(separator: "\n", omittingEmptySubsequences: false)
        return lines.suffix(maxLines).joined(separator: "\n")
    }

    private static func firstFatalMarker(in output: String) -> String? {
        firstMarker(in: output, markers: crashMarkers) ??
            firstMarker(in: output, markers: panicMarkers) ??
            firstMarker(in: output, markers: oomMarkers)
    }

    private static func firstUpstreamFailureLine(in output: String) -> String? {
        output
            .split(separator: "\n", omittingEmptySubsequences: false)
            .map(String.init)
            .first { line in
                let trimmed = line.trimmingCharacters(in: .whitespaces)
                return trimmed.hasPrefix("not ok")
            }
    }

    private static func containsTAPPlan(in output: String) -> Bool {
        output
            .split(separator: "\n", omittingEmptySubsequences: false)
            .contains { line in
                line.hasPrefix("1..") &&
                    line.dropFirst(3).allSatisfy(\.isNumber)
            }
    }

    private static func value(
        after prefix: String,
        in field: Substring
    ) -> Int? {
        guard field.hasPrefix(prefix) else {
            return nil
        }
        return Int(field.dropFirst(prefix.count))
    }

    private static let crashMarkers = [
        "Incident Identifier:",
        "Exception Type:",
        "Termination Reason:",
    ]
    private static let panicMarkers = [
        "Kernel panic",
        "kernel panic",
        "panic:",
    ]
    private static let oomMarkers = [
        "Out of memory",
        "oom-kill",
        "Killed process",
    ]
}

final class OrlixUpstreamTestSessionRunner: @unchecked Sendable {
    private let spec: OrlixUpstreamTestRunSpec
    private let injectedSession: OrlixLinuxSession?
    private let parser: OrlixUpstreamTestOutputParser

    init(
        spec: OrlixUpstreamTestRunSpec,
        session: OrlixLinuxSession? = nil,
        parser: OrlixUpstreamTestOutputParser = OrlixUpstreamTestOutputParser()
    ) {
        self.spec = spec
        self.injectedSession = session
        self.parser = parser
    }

    func run() throws -> String {
        let rootImage = try spec.rootImageDescriptor()
        guard let rootBundleResourceName = rootImage.initrdBundleName else {
            throw OrlixUpstreamTestRunError.missingRootfsBundle(
                "metadata:\(spec.suite.rawValue)"
            )
        }
        guard let rootBundleExtension = rootImage.initrdBundleExtension else {
            throw OrlixUpstreamTestRunError.missingRootfsBundle(
                "metadata:\(rootBundleResourceName)"
            )
        }
        guard Bundle.main.url(
            forResource: rootBundleResourceName,
            withExtension: rootBundleExtension
        ) != nil else {
            throw OrlixUpstreamTestRunError.missingRootfsBundle(
            "\(rootBundleResourceName).\(rootBundleExtension)"
            )
        }

        let hostDirectoryFixture = try Self.prepareHostDirectoryFixture(
            ifNeededFor: spec
        )
        defer {
            if let hostDirectoryFixture {
                try? FileManager.default.removeItem(
                    at: hostDirectoryFixture.rootDirectory
                )
            }
        }

        let session: OrlixLinuxSession
        if let injectedSession {
            session = injectedSession
        } else if let hostDirectoryFixture {
            session = OrlixLinuxSession(
                bootConfig: try spec.bootConfig(
                    rootImage: rootImage
                ),
                hostDirectories: hostDirectoryFixture.registrations
            )
        } else {
            session = OrlixLinuxSession(
                bootConfig: try spec.bootConfig(
                    rootImage: rootImage
                )
            )
        }
        let recorder = TerminalOutputRecorder()
        let completion = DispatchSemaphore(value: 0)
        let bootStatus = BootStatusRecorder()
        let output = session.terminal.attachOutput { data in
            recorder.append(data)
            if self.parser.containsTerminalCondition(recorder.text, for: self.spec) {
                completion.signal()
            }
        }
        defer { output.cancel() }

        DispatchQueue.global(qos: .userInitiated).async {
            let status = session.boot()
            bootStatus.set(status)
            if status != .ok {
                completion.signal()
            }
        }

        let deadline = DispatchTime.now() + spec.timeout
        guard completion.wait(timeout: deadline) == .success else {
            let text = recorder.text
            if parser.containsTerminalCondition(text, for: spec) {
                try parser.validate(text, for: spec)
                return text
            }
            throw OrlixUpstreamTestRunError.timeout(spec.timeout)
        }

        if let status = bootStatus.value, status != .ok {
            if status == .alreadyStarted {
                throw OrlixUpstreamTestRunError.bootAlreadyStarted
            }
            throw OrlixUpstreamTestRunError.bootFailed(status)
        }

        let text = recorder.text
        try parser.validate(text, for: spec)
        return text
    }

    private static func prepareHostDirectoryFixture(
        ifNeededFor spec: OrlixUpstreamTestRunSpec
    ) throws -> HostDirectoryFixture? {
        guard spec.hostDirectoryFixture else {
            return nil
        }

        let root = FileManager.default.temporaryDirectory
            .appendingPathComponent(
                "orlix-host-directory-\(UUID().uuidString)",
                isDirectory: true
            )
        let nested = root
            .appendingPathComponent("nested", isDirectory: true)
            .appendingPathComponent("deeper", isDirectory: true)
        try FileManager.default.createDirectory(
            at: nested,
            withIntermediateDirectories: true
        )
        try Data("orlix virtio-fs fixture\n".utf8).write(
            to: root.appendingPathComponent("root-file.txt")
        )
        try Data("nested fixture\n".utf8).write(
            to: nested.appendingPathComponent("nested-file.txt")
        )

        return HostDirectoryFixture(
            rootDirectory: root,
            registrations: [
                    OrlixHostDirectoryRegistration(
                        identifier: OrlixEnvironmentRootImage.defaultHostDirectoryIdentifier,
                        hostPath: root.path,
                        readOnly: false
                    )
                ]
            )
    }
}

private struct HostDirectoryFixture {
    let rootDirectory: URL
    let registrations: [OrlixHostDirectoryRegistration]
}

private final class BootStatusRecorder: @unchecked Sendable {
    private let lock = NSLock()
    private var storage: OrlixBootStatus?

    var value: OrlixBootStatus? {
        lock.lock()
        defer { lock.unlock() }
        return storage
    }

    func set(_ status: OrlixBootStatus) {
        lock.lock()
        storage = status
        lock.unlock()
    }
}

private final class TerminalOutputRecorder: @unchecked Sendable {
    private let lock = NSLock()
    private var storage = Data()

    var text: String {
        lock.lock()
        defer { lock.unlock() }
        return String(decoding: storage, as: UTF8.self)
    }

    func append(_ data: Data) {
        lock.lock()
        storage.append(data)
        lock.unlock()
    }
}

enum OrlixAppLaunchRuntimeRunner {
    private static let environmentKey = "ORLIX_RUNTIME_TEST_SPEC"
    private static let fixtureRootKey = "ORLIX_RUNTIME_FIXTURE_ROOT"

    static func runIfRequested() {
        guard let specName = requestedSpecName() else {
            return
        }

        do {
            let output: String
            switch specName {
            case "ociStdio":
                output = try OrlixOCIDerivedStdioRuntimeProof().run()
            default:
                throw OrlixAppLaunchRuntimeRunnerError.unknownSpec(specName)
            }
            try writeOutputArtifact(output)
            writeText(output, to: .standardOutput)
            exit(EXIT_SUCCESS)
        } catch {
            let message = "ORLIX-APP-RUNTIME-RUNNER-ERROR \(error)\n"
            try? writeOutputArtifact(message)
            NSLog("%@", message)
            writeText(message, to: .standardError)
            exit(EXIT_FAILURE)
        }
    }

    private static func requestedSpecName() -> String? {
        if let specName = ProcessInfo.processInfo.environment[environmentKey] {
            NSLog("ORLIX-APP-RUNTIME-RUNNER requested runtime spec %@", specName)
            return specName
        }

        let arguments = ProcessInfo.processInfo.arguments
        guard let optionIndex = arguments.firstIndex(of: "--orlix-runtime-test-spec") else {
            return nil
        }
        let valueIndex = arguments.index(after: optionIndex)
        guard valueIndex < arguments.endIndex else {
            return nil
        }
        return arguments[valueIndex]
    }

    fileprivate static func fixtureRoot() -> URL {
        if let override = ProcessInfo.processInfo.environment[fixtureRootKey],
           !override.isEmpty {
            return URL(fileURLWithPath: override, isDirectory: true)
        }

        if let resourceRoot = Bundle.main.resourceURL?
            .appendingPathComponent("EnvironmentRuntimeTestFixtures", isDirectory: true)
            .appendingPathComponent("oci-imported", isDirectory: true),
            FileManager.default.fileExists(
                atPath: resourceRoot.appendingPathComponent(".ready").path
            ) {
            return resourceRoot
        }

        let repoRoot = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent()
            .deletingLastPathComponent()
            .deletingLastPathComponent()
        return repoRoot
            .appendingPathComponent("Build", isDirectory: true)
            .appendingPathComponent("OrlixOS", isDirectory: true)
            .appendingPathComponent("environment-runtime-test-fixtures", isDirectory: true)
            .appendingPathComponent("oci-imported", isDirectory: true)
    }

    private static func writeOutputArtifact(_ text: String) throws {
        let url = FileManager.default.temporaryDirectory
            .appendingPathComponent("orlix-runtime-test-output.txt")
        try Data(text.utf8).write(to: url, options: [.atomic])
    }

    private static func writeText(_ text: String, to handle: FileHandle) {
        handle.write(Data(text.utf8))
    }
}

private enum OrlixAppLaunchRuntimeRunnerError: Error, CustomStringConvertible {
    case unknownSpec(String)

    var description: String {
        switch self {
        case let .unknownSpec(name):
            return "unknown runtime test spec '\(name)'"
        }
    }
}

private final class OrlixOCIDerivedStdioRuntimeProof: @unchecked Sendable {
    private static let timeout: TimeInterval = 300
    private static let requiredMarkers = [
        "ORLIX_ENV_STDIO_BEGIN",
        "ORLIX_ENV_STDIO_STDOUT_OK",
        "ORLIX_ENV_STDIO_STDERR_OK",
        "ORLIX_ENV_STDIO_NOT_PTY_OK",
        "ORLIX_ENV_STDIO_DONE",
    ]

    private let fileManager = FileManager.default
    private let recorder = OrlixRuntimeProofOutputRecorder()

    func run() throws -> String {
        let sourceRoot = OrlixAppLaunchRuntimeRunner.fixtureRoot()
        let ready = sourceRoot.appendingPathComponent(".ready", isDirectory: false)
        guard fileManager.fileExists(atPath: ready.path) else {
            throw OrlixOCIDerivedStdioRuntimeProofError.missingFixture(ready.path)
        }

        let copiedRoot = FileManager.default.temporaryDirectory
            .appendingPathComponent("orlix-oci-stdio-\(UUID().uuidString)", isDirectory: true)
        try fileManager.copyItem(at: sourceRoot, to: copiedRoot)
        defer {
            try? fileManager.removeItem(at: copiedRoot)
        }

        let fixture = OrlixRuntimeEnvironmentFixture(root: copiedRoot)
        let descriptor = OrlixEnvironmentDescriptor(
            id: "oci-imported-runtime-test-fixture",
            source: .ociLayout,
            platform: "linux/arm64",
            rootImageIdentifier: "orlix.test.environment.oci-runtime-test-fixture",
            defaultCommand: ["/bin/sh", "-c", Self.stdioExecutionScript],
            defaultEnvironment: [
                "HOME": "/root",
                "PATH": "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
                "TERM": "xterm-256color",
            ],
            defaultWorkingDirectory: "/",
            defaultUserID: 0,
            defaultGroupID: 0,
            hostname: "oci-host",
            domainname: "oci.example",
            rootMount: .defaultOverlay
        )
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: fixture.linuxStateRoot,
            cacheRoot: fixture.cacheRoot,
            scratchRoot: fixture.scratchRoot
        )
        let terminal = OrlixTerminalSession()
        let output = terminal.attachOutput { [recorder] data in
            recorder.append(data)
        }
        defer {
            output.cancel()
        }

        let layout = try OrlixEnvironmentStorageLayout.layout(
            forEnvironmentID: descriptor.id,
            linuxStateRoot: fixture.linuxStateRoot,
            cacheRoot: fixture.cacheRoot,
            scratchRoot: fixture.scratchRoot
        )
        try Self.writeOCIRuntimeConfig(
            terminal: false,
            rootPath: "imported-root",
            to: fixture.root
        )
        let runtime = OrlixOCIRuntime(registry: registry)
        let lifecycle = try OrlixOCIRuntimeBundle
            .load(from: fixture.root)
            .lifecycleController(id: descriptor.id)
            .create()
        let processHandle = try OrlixOCIRuntimeProcessHandle(
            lifecycle: lifecycle,
            rootMount: .defaultOverlay,
            rootImageIdentifier: descriptor.rootImageIdentifier
        )
        try registry.save(processHandle.sessionDescriptor.environment)
        try runtime.lifecycleStore.save(lifecycle)

        let installer = OrlixOCIEnvironmentInstaller(registry: registry)
        let run = try installer.run(
            id: descriptor.id,
            terminal: terminal,
            observationTimeout: Self.timeout
        )
        let finalState = try installer.state(id: descriptor.id)
        let lifecycleRecordURL = try runtime.lifecycleStore.recordURL(
            forID: descriptor.id
        )
        let environmentDirectoryURL = layout.rootDirectory
        let deletedEnvironment = try installer.delete(id: descriptor.id)

        var text = Self.normalized(recorder.text)
        try Self.validate(text)
        try Self.validateLifecycle(
            run: run,
            finalState: finalState,
            deletedEnvironment: deletedEnvironment,
            lifecycleRecordURL: lifecycleRecordURL,
            environmentDirectoryURL: environmentDirectoryURL
        )
        text += "\nORLIX_OCI_LIFECYCLE_RUNNING_OK\n"
        text += "ORLIX_OCI_LIFECYCLE_STOPPED_OK\n"
        text += "ORLIX_OCI_LIFECYCLE_DELETE_OK\n"
        return text
    }

    private static var stdioExecutionScript: String {
        [
            "printf '%s%s\\n' ORLIX_ENV_ STDIO_BEGIN",
            "printf '%s%s\\n' ORLIX_ENV_STDIO_ STDOUT_OK",
            "printf '%s%s\\n' ORLIX_ENV_STDIO_ STDERR_OK >&2",
            "if command -v tty >/dev/null 2>&1; then tty_path=$(tty); elif /bin/test -x /bin/tty; then tty_path=$(/bin/tty); elif /bin/test -x /usr/bin/tty; then tty_path=$(/usr/bin/tty); else tty_path=missing-tty-command; fi",
            "printf 'stdio_tty=%s\\n' \"$tty_path\"",
            "case \"$tty_path\" in /dev/pts/*) printf '%s%s\\n' ORLIX_ENV_STDIO_PROOF_ FAILED_PTY;; *) printf '%s%s\\n' ORLIX_ENV_STDIO_ NOT_PTY_OK;; esac",
            "printf '%s%s\\n' ORLIX_ENV_STDIO_ DONE",
        ].joined(separator: "\n")
    }

    private static func hasAllRequiredMarkers(in text: String) -> Bool {
        requiredMarkers.allSatisfy { text.contains($0) }
    }

    private static func validate(_ text: String) throws {
        if text.contains("ORLIX_ENV_STDIO_PROOF_FAILED_PTY") {
            throw OrlixOCIDerivedStdioRuntimeProofError.unexpectedPTY(text)
        }
        for marker in requiredMarkers where !text.contains(marker) {
            throw OrlixOCIDerivedStdioRuntimeProofError.missingMarker(marker, text)
        }
    }

    private static func validateLifecycle(
        run: OrlixOCIEnvironmentRunResult,
        finalState: OrlixOCIRuntimeStateReport,
        deletedEnvironment: OrlixOCIEnvironmentDeleteResult,
        lifecycleRecordURL: URL,
        environmentDirectoryURL: URL
    ) throws {
        guard run.startedStateReport.status == .running else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected started lifecycle state running"
            )
        }
        guard run.startedStateReport.pid != nil else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected running lifecycle pid"
            )
        }
        guard run.completedStateReport.status == .stopped,
              run.completedStateReport.exitStatus == 0 else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected completed lifecycle state stopped exit 0"
            )
        }
        guard finalState.status == .stopped,
              finalState.exitStatus == 0 else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected final lifecycle state stopped exit 0"
            )
        }
        guard deletedEnvironment.lifecycleState == .deleted else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected deleted lifecycle state"
            )
        }
        guard !FileManager.default.fileExists(atPath: lifecycleRecordURL.path) else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected lifecycle record cleanup"
            )
        }
        guard !FileManager.default.fileExists(atPath: environmentDirectoryURL.path) else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected environment directory cleanup"
            )
        }
    }

    private static func writeOCIRuntimeConfig(
        terminal: Bool,
        rootPath: String,
        to bundleRoot: URL
    ) throws {
        let process: [String: Any] = [
            "terminal": terminal,
            "args": ["/bin/sh", "-c", stdioExecutionScript],
            "env": [
                "HOME=/root",
                "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
                "TERM=xterm-256color",
            ],
            "cwd": "/",
            "user": [
                "uid": 0,
                "gid": 0,
            ],
        ]
        let document: [String: Any] = [
            "ociVersion": "1.1.0",
            "process": process,
            "root": [
                "path": rootPath,
            ],
            "hostname": "oci-host",
            "domainname": "oci.example",
        ]
        let data = try JSONSerialization.data(
            withJSONObject: document,
            options: [.prettyPrinted, .sortedKeys]
        )
        try data.write(to: bundleRoot.appendingPathComponent("config.json"))
    }

    private static func normalized(_ text: String) -> String {
        text.replacingOccurrences(of: "\r\n", with: "\n")
            .replacingOccurrences(of: "\r", with: "\n")
    }
}

private struct OrlixRuntimeEnvironmentFixture {
    let root: URL

    var linuxStateRoot: URL {
        root.appendingPathComponent("state", isDirectory: true)
    }

    var cacheRoot: URL {
        root.appendingPathComponent("cache", isDirectory: true)
    }

    var scratchRoot: URL {
        root.appendingPathComponent("scratch", isDirectory: true)
    }
}

private final class OrlixRuntimeProofOutputRecorder: @unchecked Sendable {
    private let lock = NSLock()
    private var storage = Data()

    var text: String {
        lock.lock()
        defer {
            lock.unlock()
        }
        return String(decoding: storage, as: UTF8.self)
    }

    func append(_ data: Data) {
        lock.lock()
        storage.append(data)
        lock.unlock()
    }
}

private enum OrlixOCIDerivedStdioRuntimeProofError: Error, CustomStringConvertible {
    case missingFixture(String)
    case missingMarker(String, String)
    case unexpectedPTY(String)
    case lifecycle(String)

    var description: String {
        switch self {
        case let .missingFixture(path):
            return "missing OCI runtime fixture marker: \(path)"
        case let .missingMarker(marker, output):
            return "OCI runtime stdio proof missing marker \(marker)\n\(output)"
        case let .unexpectedPTY(output):
            return "OCI runtime stdio proof unexpectedly used a PTY\n\(output)"
        case let .lifecycle(message):
            return "OCI runtime lifecycle proof failed: \(message)"
        }
    }
}
