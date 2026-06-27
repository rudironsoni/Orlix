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
            case "ociTerminal":
                output = try OrlixOCIDerivedStdioRuntimeProof(terminal: true).run()
            case "ociSignal":
                output = try OrlixOCIDerivedSignalRuntimeProof().run()
            case "ociRun":
                output = try OrlixOCIDerivedRunCommandRuntimeProof().run()
            case "ociRunLiveRegistry":
                output = try OrlixOCIDerivedRunCommandRuntimeProof(
                    registryMode: .live
                ).run()
            case "ociNetwork":
                output = try OrlixOCIDerivedNetworkRuntimeProof().run()
            case "ociVirtioFS":
                output = try OrlixOCIDerivedVirtioFSRuntimeProof().run()
            case "ociHostMountTarget":
                output = try OrlixOCIHostMountTargetRuntimeProof().run()
            case "ociHostMountTargetReadOnly":
                output = try OrlixOCIHostMountTargetRuntimeProof(readOnly: true).run()
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
    private static let timeout: TimeInterval = 240
    private static let stdioRequiredMarkers = [
        "ORLIX_ENV_STDIO_BEGIN",
        "ORLIX_ENV_STDIO_STDOUT_OK",
        "ORLIX_ENV_STDIO_STDERR_OK",
        "ORLIX_ENV_STDIO_NOT_PTY_OK",
        "ORLIX_ENV_STDIO_DONE",
    ]
    private static let terminalRequiredMarkers = [
        "ORLIX_ENV_TERMINAL_BEGIN",
        "ORLIX_ENV_TERMINAL_STDOUT_OK",
        "ORLIX_ENV_TERMINAL_STDERR_OK",
        "ORLIX_ENV_TERMINAL_PTY_OK",
        "ORLIX_ENV_TERMINAL_DONE",
    ]

    private let fileManager = FileManager.default
    private let recorder = OrlixRuntimeProofOutputRecorder()
    private let terminalMode: Bool

    init(terminal: Bool = false) {
        self.terminalMode = terminal
    }

    func run() throws -> String {
        let sourceRoot = OrlixAppLaunchRuntimeRunner.fixtureRoot()
        let ready = sourceRoot.appendingPathComponent(".ready", isDirectory: false)
        guard fileManager.fileExists(atPath: ready.path) else {
            throw OrlixOCIDerivedStdioRuntimeProofError.missingFixture(ready.path)
        }

        let copiedRoot = FileManager.default.temporaryDirectory
            .appendingPathComponent("orlix-oci-\(terminalMode ? "terminal" : "stdio")-\(UUID().uuidString)", isDirectory: true)
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
            defaultCommand: ["/bin/sh", "-c", Self.executionScript(terminal: terminalMode)],
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
            terminal: terminalMode,
            script: Self.executionScript(terminal: terminalMode),
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
        let run: OrlixOCIEnvironmentRunResult
        do {
            run = try installer.run(
                id: descriptor.id,
                terminal: terminal,
                observationTimeout: Self.timeout
            )
        } catch {
            let text = Self.normalized(recorder.text)
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "run failed: \(error)\n\(text)"
            )
        }
        let finalState = try installer.state(id: descriptor.id)
        let lifecycleRecordURL = try runtime.lifecycleStore.recordURL(
            forID: descriptor.id
        )
        let environmentDirectoryURL = layout.rootDirectory
        let deletedEnvironment = try installer.delete(id: descriptor.id)

        var text = Self.normalized(recorder.text)
        try Self.validate(text, terminal: terminalMode)
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

    private static var terminalExecutionScript: String {
        [
            "printf '%s%s\\n' ORLIX_ENV_ TERMINAL_BEGIN",
            "printf '%s%s\\n' ORLIX_ENV_TERMINAL_ STDOUT_OK",
            "printf '%s%s\\n' ORLIX_ENV_TERMINAL_ STDERR_OK >&2",
            "if /bin/test -t 0 && /bin/test -t 1 && /bin/test -t 2; then printf '%s%s\\n' ORLIX_ENV_TERMINAL_ TTY_FDS_OK; else printf '%s%s\\n' ORLIX_ENV_TERMINAL_PROOF_ FAILED_TTY_FDS; fi",
            "if command -v tty >/dev/null 2>&1; then tty_path=$(tty); elif /bin/test -x /bin/tty; then tty_path=$(/bin/tty); elif /bin/test -x /usr/bin/tty; then tty_path=$(/usr/bin/tty); else tty_path=missing-tty-command; fi",
            "printf 'terminal_tty=%s\\n' \"$tty_path\"",
            "case \"$tty_path\" in /dev/pts/*) printf '%s%s\\n' ORLIX_ENV_TERMINAL_ PTY_OK;; *) printf '%s%s\\n' ORLIX_ENV_TERMINAL_PROOF_ FAILED_NOT_PTY;; esac",
            "printf '%s%s\\n' ORLIX_ENV_TERMINAL_ DONE",
        ].joined(separator: "\n")
    }

    private static func executionScript(terminal: Bool) -> String {
        terminal ? terminalExecutionScript : stdioExecutionScript
    }

    private static func hasAllRequiredMarkers(in text: String) -> Bool {
        stdioRequiredMarkers.allSatisfy { text.contains($0) }
    }

    private static func validate(_ text: String, terminal: Bool) throws {
        if text.contains("ORLIX_ENV_STDIO_PROOF_FAILED_PTY") ||
            text.contains("ORLIX_ENV_TERMINAL_PROOF_FAILED_TTY_FDS") ||
            text.contains("ORLIX_ENV_TERMINAL_PROOF_FAILED_NOT_PTY") {
            throw OrlixOCIDerivedStdioRuntimeProofError.unexpectedPTY(text)
        }
        let requiredMarkers = terminal ? terminalRequiredMarkers : stdioRequiredMarkers
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

    fileprivate static func writeOCIRuntimeConfig(
        terminal: Bool,
        script: String,
        rootPath: String,
        to bundleRoot: URL
    ) throws {
        let process: [String: Any] = [
            "terminal": terminal,
            "args": ["/bin/sh", "-c", script],
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

private final class OrlixOCIDerivedRunCommandRuntimeProof: @unchecked Sendable {
    enum RegistryMode: Sendable {
        case deterministic
        case live
    }

    private static let timeout: TimeInterval = 120
    private static let fixtureEnvironmentID = "oci-imported-runtime-test-fixture"
    private static let runEnvironmentID = "oci-run-runtime-test-fixture"
    private static let deterministicImageReference =
        "registry.example.org/library/orlix-fixture:latest"
    private static let liveImageReference = "registry.k8s.io/pause:3.10"
    private static let requiredMarkers = [
        "ORLIX_ENV_ORLIX_RUN_BEGIN",
        "ORLIX_ENV_ORLIX_RUN_STDOUT_OK",
        "ORLIX_ENV_ORLIX_RUN_STDERR_OK",
        "ORLIX_ENV_ORLIX_RUN_DONE",
    ]

    private let fileManager = FileManager.default
    private let recorder = OrlixRuntimeProofOutputRecorder()
    private let registryMode: RegistryMode

    init(registryMode: RegistryMode = .deterministic) {
        self.registryMode = registryMode
    }

    func run() throws -> String {
        let resultBox = OrlixAsyncRuntimeProofResultBox<String>()
        let completion = DispatchSemaphore(value: 0)
        Task {
            do {
                resultBox.set(.success(try await self.runAsync()))
            } catch {
                resultBox.set(.failure(error))
            }
            completion.signal()
        }

        guard completion.wait(timeout: .now() + .seconds(Int(Self.timeout)))
            == .success
        else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "orlix run proof timed out"
            )
        }

        guard let result = resultBox.value else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "orlix run proof completed without result"
            )
        }
        return try result.get()
    }

    private func runAsync() async throws -> String {
        let imageReference = Self.imageReference(for: registryMode)
        let sourceRoot = OrlixAppLaunchRuntimeRunner.fixtureRoot()
        let ready = sourceRoot.appendingPathComponent(".ready", isDirectory: false)
        guard fileManager.fileExists(atPath: ready.path) else {
            throw OrlixOCIDerivedStdioRuntimeProofError.missingFixture(ready.path)
        }

        let copiedRoot = FileManager.default.temporaryDirectory
            .appendingPathComponent("orlix-oci-run-\(UUID().uuidString)", isDirectory: true)
        try fileManager.copyItem(at: sourceRoot, to: copiedRoot)
        defer {
            try? fileManager.removeItem(at: copiedRoot)
        }

        let fixture = OrlixRuntimeEnvironmentFixture(root: copiedRoot)
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: fixture.linuxStateRoot,
            cacheRoot: fixture.cacheRoot,
            scratchRoot: fixture.scratchRoot
        )
        let sourceLayout = try OrlixEnvironmentStorageLayout.layout(
            forEnvironmentID: Self.fixtureEnvironmentID,
            linuxStateRoot: fixture.linuxStateRoot,
            cacheRoot: fixture.cacheRoot,
            scratchRoot: fixture.scratchRoot
        )
        let runLayout = try OrlixEnvironmentStorageLayout.layout(
            forEnvironmentID: Self.runEnvironmentID,
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

        let installer = OrlixOCIEnvironmentInstaller(registry: registry)
        let driver = OrlixOCIRuntimeLinuxSessionObservationDriver(
            timeout: Self.timeout
        )
        let result = try await installer.run(
            arguments: [
                "orlix",
                "run",
                "--id",
                Self.runEnvironmentID,
                "--rm",
                imageReference,
                "--",
                "/bin/sh",
                "-c",
                Self.executionScript,
            ],
            tools: OrlixOCIEnvironmentMaterializationTools(
                mke2fs: URL(fileURLWithPath: "/usr/bin/orlix-mke2fs"),
                truncate: URL(fileURLWithPath: "/usr/bin/orlix-truncate"),
                debugfs: URL(fileURLWithPath: "/usr/bin/orlix-debugfs")
            ),
            puller: try Self.registryPuller(for: registryMode),
            terminal: terminal,
            using: driver,
            fileManager: fileManager
        ) { executable, arguments in
            try Self.runFixtureMaterializationCommand(
                executable: executable,
                arguments: arguments,
                sourceBaseImageURL: sourceLayout.baseImageURL,
                sourceStateImageURL: sourceLayout.stateImageURL
            )
        }

        var text = Self.normalized(recorder.text)
        try Self.validateText(text)
        try Self.validateLifecycle(
            result: result,
            lifecycleRecordURL: try OrlixOCIRuntime(registry: registry)
                .lifecycleStore.recordURL(forID: Self.runEnvironmentID),
            environmentDirectoryURL: runLayout.rootDirectory
        )
        text += "\nORLIX_OCI_RUN_COMMAND_STARTED_OK\n"
        text += "ORLIX_OCI_RUN_COMMAND_STOPPED_OK\n"
        text += "ORLIX_OCI_RUN_COMMAND_DELETE_OK\n"
        if registryMode == .live {
            text += "ORLIX_OCI_RUN_LIVE_REGISTRY_PULL_OK\n"
        }
        return text
    }

    private static func imageReference(for mode: RegistryMode) -> String {
        switch mode {
        case .deterministic:
            return deterministicImageReference
        case .live:
            return liveImageReference
        }
    }

    private static var executionScript: String {
        [
            "printf '%s%s\\n' ORLIX_ENV_ ORLIX_RUN_BEGIN",
            "printf '%s%s\\n' ORLIX_ENV_ORLIX_RUN_ STDOUT_OK",
            "printf '%s%s\\n' ORLIX_ENV_ORLIX_RUN_ STDERR_OK >&2",
            "printf '%s%s\\n' ORLIX_ENV_ORLIX_RUN_ DONE",
        ].joined(separator: "\n")
    }

    private static func registryPuller(for mode: RegistryMode) throws -> OrlixOCIRegistryPuller {
        switch mode {
        case .deterministic:
            return try deterministicRegistryPuller()
        case .live:
            return OrlixOCIRegistryPuller()
        }
    }

    private static func deterministicRegistryPuller() throws -> OrlixOCIRegistryPuller {
        let image = try OrlixOCIRegistryImageReference(deterministicImageReference)
        let configData = Data(
            """
            {
              "config": {
                "Env": ["PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", "TERM=xterm-256color"],
                "Entrypoint": ["/bin/sh"],
                "Cmd": ["-c", "printf registry-default\\n"],
                "WorkingDir": "/",
                "User": "0"
              },
              "rootfs": {
                "type": "layers",
                "diff_ids": []
              }
            }
            """.utf8
        )
        let configDigest = "sha256:\(OrlixOCIDigest.sha256Hex(configData))"
        let manifestData = Data(
            """
            {
              "schemaVersion": 2,
              "mediaType": "application/vnd.oci.image.manifest.v1+json",
              "config": {
                "mediaType": "application/vnd.oci.image.config.v1+json",
                "digest": "\(configDigest)",
                "size": \(configData.count)
              },
              "layers": []
            }
            """.utf8
        )
        let manifestDigest = "sha256:\(OrlixOCIDigest.sha256Hex(manifestData))"
        let responses = [
            try image.manifestURL().absoluteString:
                OrlixOCIRegistryFetchResponse(
                    statusCode: 200,
                    headers: [
                        "Content-Type": "application/vnd.oci.image.manifest.v1+json",
                        "Docker-Content-Digest": manifestDigest,
                    ],
                    body: manifestData
                ),
            try image.blobURL(digest: configDigest).absoluteString:
                OrlixOCIRegistryFetchResponse(
                    statusCode: 200,
                    headers: ["Docker-Content-Digest": configDigest],
                    body: configData
                ),
        ]
        return OrlixOCIRegistryPuller { request in
            guard let response = responses[request.url.absoluteString] else {
                throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                    "unexpected registry request \(request.url.absoluteString)"
                )
            }
            return response
        }
    }

    private static func runFixtureMaterializationCommand(
        executable: URL,
        arguments: [String],
        sourceBaseImageURL: URL,
        sourceStateImageURL: URL
    ) throws {
        guard let targetPath = arguments.last else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "materialization command missing target: \(executable.path)"
            )
        }
        let targetURL = URL(fileURLWithPath: targetPath)
        try FileManager.default.createDirectory(
            at: targetURL.deletingLastPathComponent(),
            withIntermediateDirectories: true
        )

        switch executable.lastPathComponent {
        case "orlix-truncate":
            FileManager.default.createFile(atPath: targetURL.path, contents: Data())
        case "orlix-mke2fs":
            if FileManager.default.fileExists(atPath: targetURL.path) {
                try FileManager.default.removeItem(at: targetURL)
            }
            let sourceURL = targetURL.lastPathComponent == "base.ext4"
                ? sourceBaseImageURL
                : sourceStateImageURL
            try FileManager.default.copyItem(at: sourceURL, to: targetURL)
        case "orlix-debugfs":
            break
        default:
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "unexpected materialization executable \(executable.path)"
            )
        }
    }

    private static func validateText(_ text: String) throws {
        for marker in requiredMarkers where !text.contains(marker) {
            throw OrlixOCIDerivedStdioRuntimeProofError.missingMarker(marker, text)
        }
        if text.contains("ORLIX-APP-RUNTIME-RUNNER-ERROR") {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(text)
        }
    }

    private static func validateLifecycle(
        result: OrlixOCIRegistryEnvironmentInstallRunResult,
        lifecycleRecordURL: URL,
        environmentDirectoryURL: URL
    ) throws {
        guard result.installResult.id == Self.runEnvironmentID else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected orlix run install id \(Self.runEnvironmentID)"
            )
        }
        guard result.runResult.startedStateReport.status == .running,
              result.runResult.startedStateReport.pid != nil else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected orlix run started lifecycle state running"
            )
        }
        guard result.runResult.completedStateReport.status == .stopped,
              result.runResult.completedStateReport.exitStatus == 0 else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected orlix run completed lifecycle state stopped exit 0"
            )
        }
        guard result.deleteResult?.lifecycleState == .deleted else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected orlix run --rm delete result"
            )
        }
        guard !FileManager.default.fileExists(atPath: lifecycleRecordURL.path) else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected orlix run lifecycle record cleanup"
            )
        }
        guard !FileManager.default.fileExists(atPath: environmentDirectoryURL.path) else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected orlix run environment directory cleanup"
            )
        }
    }

    private static func normalized(_ text: String) -> String {
        text.replacingOccurrences(of: "\r\n", with: "\n")
            .replacingOccurrences(of: "\r", with: "\n")
    }
}

private final class OrlixOCIDerivedNetworkRuntimeProof: @unchecked Sendable {
    private static let timeout: TimeInterval = 120
    private static let environmentID = "oci-imported-runtime-test-fixture"
    private static let rootImageIdentifier =
        "orlix.test.environment.oci-runtime-test-fixture"
    private static let requiredMarkers = [
        "1..12",
        "ok 1 - procfs exposes network state",
        "ok 2 - rtnetlink sockets open in the current network namespace",
        "ok 3 - RTM_GETLINK reports loopback interface",
        "ok 4 - loopback interface accepts Linux address configuration",
        "ok 5 - RTM_GETADDR reports loopback IPv4 address",
        "ok 6 - RTM_GETROUTE reports loopback IPv4 route",
        "ok 7 - network namespace child enters isolated net namespace",
        "ok 8 - new network namespace keeps procfs network state readable",
        "ok 9 - new network namespace keeps rtnetlink socket local",
        "ok 10 - new network namespace rejects incomplete route with Linux error",
        "ok 11 - loopback TCP accepts local connections",
        "ok 12 - loopback UDP exchanges local datagrams",
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
            .appendingPathComponent("orlix-oci-network-\(UUID().uuidString)", isDirectory: true)
        try fileManager.copyItem(at: sourceRoot, to: copiedRoot)
        defer {
            try? fileManager.removeItem(at: copiedRoot)
        }

        let fixture = OrlixRuntimeEnvironmentFixture(root: copiedRoot)
        let descriptor = OrlixEnvironmentDescriptor(
            id: Self.environmentID,
            source: .ociLayout,
            platform: "linux/arm64",
            rootImageIdentifier: Self.rootImageIdentifier,
            defaultCommand: ["/orlix/network_namespace_probe"],
            defaultEnvironment: [
                "HOME": "/root",
                "PATH": "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
                "TERM": "xterm-256color",
            ],
            defaultWorkingDirectory: "/",
            defaultUserID: 0,
            defaultGroupID: 0,
            hostname: "oci-network-host",
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
        try OrlixOCIDerivedStdioRuntimeProof.writeOCIRuntimeConfig(
            terminal: false,
            script: "/orlix/network_namespace_probe",
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
        try waitForRequiredMarkers()
        let finalState = try installer.state(id: descriptor.id)
        let lifecycleRecordURL = try runtime.lifecycleStore.recordURL(
            forID: descriptor.id
        )
        let environmentDirectoryURL = layout.rootDirectory
        let deletedEnvironment = try installer.delete(id: descriptor.id)

        var text = Self.normalized(recorder.text)
        try Self.validateText(text)
        try Self.validateLifecycle(
            run: run,
            finalState: finalState,
            deletedEnvironment: deletedEnvironment,
            lifecycleRecordURL: lifecycleRecordURL,
            environmentDirectoryURL: environmentDirectoryURL
        )
        text += "\nORLIX_OCI_NETWORK_RUNTIME_STARTED_OK\n"
        text += "ORLIX_OCI_NETWORK_RUNTIME_STOPPED_OK\n"
        text += "ORLIX_OCI_NETWORK_RUNTIME_DELETE_OK\n"
        return text
    }

    private static func validateText(_ text: String) throws {
        for marker in requiredMarkers where !text.contains(marker) {
            throw OrlixOCIDerivedStdioRuntimeProofError.missingMarker(marker, text)
        }
        if text.contains("\nnot ok ") || text.contains("ORLIX-APP-RUNTIME-RUNNER-ERROR") {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(text)
        }
    }

    private func waitForRequiredMarkers() throws {
        let deadline = Date().addingTimeInterval(Self.timeout)
        while Date() < deadline {
            let text = Self.normalized(recorder.text)
            if Self.requiredMarkers.allSatisfy({ text.contains($0) }) {
                return
            }
            Thread.sleep(forTimeInterval: 0.05)
        }
        let text = Self.normalized(recorder.text)
        if let missingMarker = Self.requiredMarkers.first(where: { !text.contains($0) }) {
            throw OrlixOCIDerivedStdioRuntimeProofError.missingMarker(
                missingMarker,
                text
            )
        }
        throw OrlixOCIDerivedStdioRuntimeProofError.missingMarker(
            "OCI network runtime markers",
            text
        )
    }

    private static func validateLifecycle(
        run: OrlixOCIEnvironmentRunResult,
        finalState: OrlixOCIRuntimeStateReport,
        deletedEnvironment: OrlixOCIEnvironmentDeleteResult,
        lifecycleRecordURL: URL,
        environmentDirectoryURL: URL
    ) throws {
        guard run.startedStateReport.status == .running,
              run.startedStateReport.pid != nil else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI network proof started lifecycle state running"
            )
        }
        guard run.completedStateReport.status == .stopped,
              run.completedStateReport.exitStatus == 0 else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI network proof stopped exit 0"
            )
        }
        guard finalState.status == .stopped,
              finalState.exitStatus == 0 else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI network final lifecycle state stopped exit 0"
            )
        }
        guard deletedEnvironment.id == Self.environmentID,
              deletedEnvironment.lifecycleState == .deleted else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI network proof delete cleanup"
            )
        }
        guard !FileManager.default.fileExists(atPath: lifecycleRecordURL.path) else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI network lifecycle record cleanup"
            )
        }
        guard !FileManager.default.fileExists(atPath: environmentDirectoryURL.path) else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI network environment directory cleanup"
            )
        }
    }

    private static func normalized(_ text: String) -> String {
        text.replacingOccurrences(of: "\r\n", with: "\n")
            .replacingOccurrences(of: "\r", with: "\n")
    }
}

private final class OrlixOCIDerivedVirtioFSRuntimeProof: @unchecked Sendable {
    private static let timeout: TimeInterval = 120
    private static let environmentID = "oci-imported-runtime-test-fixture"
    private static let rootImageIdentifier =
        "orlix.test.environment.oci-runtime-test-fixture"
private static let requiredMarkers = [
 "1..14",
 "ok 1 - virtio-fs device exposes standard Orlix host-folder tag",
 "ok 2 - virtio-fs mountpoint available",
 "ok 3 - Linux mounts Orlix host folder through virtio-fs",
 "ok 4 - mounted virtio-fs root is directory",
 "ok 5 - mountinfo reports mounted virtio-fs root",
 "ok 6 - mounted virtio-fs root supports readdir",
 "ok 7 - mounted writable virtio-fs root supports create, write, readback",
 "ok 8 - mounted virtio-fs root supports statx",
 "ok 9 - mounted virtio-fs root reports empty xattr list",
 "ok 10 - mounted virtio-fs root reports missing xattrs",
 "ok 11 - mounted virtio-fs regular files support lseek when present",
 "ok 12 - mounted virtio-fs nested paths support readdir, access, statx when present",
 "ok 13 - Linux mounts Orlix host folder read-only through virtio-fs",
 "ok 14 - mounted read-only virtio-fs root rejects create with EROFS",
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
            .appendingPathComponent("orlix-oci-virtiofs-\(UUID().uuidString)", isDirectory: true)
        try fileManager.copyItem(at: sourceRoot, to: copiedRoot)
        defer {
            try? fileManager.removeItem(at: copiedRoot)
        }

        let hostRoot = FileManager.default.temporaryDirectory
            .appendingPathComponent("orlix-oci-virtiofs-host-\(UUID().uuidString)", isDirectory: true)
        try Self.createHostDirectoryFixture(at: hostRoot, fileManager: fileManager)
        defer {
            try? fileManager.removeItem(at: hostRoot)
        }

        let fixture = OrlixRuntimeEnvironmentFixture(root: copiedRoot)
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
            forEnvironmentID: Self.environmentID,
            linuxStateRoot: fixture.linuxStateRoot,
            cacheRoot: fixture.cacheRoot,
            scratchRoot: fixture.scratchRoot
        )
        try Self.writeOCIRuntimeConfig(
            hostDirectory: hostRoot,
            rootPath: "imported-root",
            to: fixture.root
        )
        let runtime = OrlixOCIRuntime(registry: registry)
        let lifecycle = try OrlixOCIRuntimeBundle
            .load(from: fixture.root)
            .lifecycleController(id: Self.environmentID)
            .create()
        let processHandle = try OrlixOCIRuntimeProcessHandle(
            lifecycle: lifecycle,
            rootMount: .defaultOverlay,
            rootImageIdentifier: Self.rootImageIdentifier
        )
        try registry.save(processHandle.sessionDescriptor.environment)
        try runtime.lifecycleStore.save(lifecycle)

        let installer = OrlixOCIEnvironmentInstaller(registry: registry)
        let run = try installer.run(
            id: Self.environmentID,
            terminal: terminal,
            observationTimeout: Self.timeout
        )
let finalState = try installer.state(id: Self.environmentID)
let lifecycleRecordURL = try runtime.lifecycleStore.recordURL(
forID: Self.environmentID
)
let environmentDirectoryURL = layout.rootDirectory
let deletedEnvironment = try installer.delete(id: Self.environmentID)
try Self.validateHostDirectoryProof(at: hostRoot)

var text = Self.normalized(recorder.text)
try Self.validateLifecycle(
run: run,
finalState: finalState,
deletedEnvironment: deletedEnvironment,
lifecycleRecordURL: lifecycleRecordURL,
environmentDirectoryURL: environmentDirectoryURL
)
text += "\nORLIX_OCI_VIRTIOFS_HOST_WRITE_OK\n"
text += "\nORLIX_OCI_VIRTIOFS_RUNTIME_STARTED_OK\n"
text += "ORLIX_OCI_VIRTIOFS_RUNTIME_STOPPED_OK\n"
text += "ORLIX_OCI_VIRTIOFS_RUNTIME_DELETE_OK\n"
return text
    }

    private static func createHostDirectoryFixture(
        at root: URL,
        fileManager: FileManager
    ) throws {
        let nested = root
            .appendingPathComponent("nested", isDirectory: true)
            .appendingPathComponent("deeper", isDirectory: true)
        try fileManager.createDirectory(
            at: nested,
            withIntermediateDirectories: true
        )
        try Data("orlix virtio-fs fixture\n".utf8).write(
            to: root.appendingPathComponent("root-file.txt", isDirectory: false)
        )
try Data("nested fixture\n".utf8).write(
to: nested.appendingPathComponent("nested-file.txt", isDirectory: false)
)
}

private static func validateHostDirectoryProof(at hostRoot: URL) throws {
let proof = hostRoot.appendingPathComponent("orlix-write-probe", isDirectory: false)
let data = try Data(contentsOf: proof)
guard String(decoding: data, as: UTF8.self) == "orlix writable virtiofs\n" else {
throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
"expected OCI virtio-fs host write proof payload"
)
}
}

private static func writeOCIRuntimeConfig(
hostDirectory: URL,
        rootPath: String,
        to bundleRoot: URL
    ) throws {
        let process: [String: Any] = [
            "terminal": true,
            "args": ["/orlix/virtio_fs_mount_probe"],
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
            "hostname": "oci-virtiofs-host",
            "domainname": "oci.example",
            "mounts": [
                [
                    "destination": "/mnt/oci-host",
                    "type": "bind",
                    "source": hostDirectory.path,
                    "options": ["bind", "rw"],
                ],
            ],
        ]
        let data = try JSONSerialization.data(
            withJSONObject: document,
            options: [.prettyPrinted, .sortedKeys]
        )
        try data.write(to: bundleRoot.appendingPathComponent("config.json"))
    }

    private static func validateText(_ text: String) throws {
        for marker in requiredMarkers where !text.contains(marker) {
            throw OrlixOCIDerivedStdioRuntimeProofError.missingMarker(marker, text)
        }
        if text.contains("\nnot ok ") || text.contains("ORLIX-APP-RUNTIME-RUNNER-ERROR") {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(text)
        }
    }

    private func waitForRequiredMarkers() throws {
        let deadline = Date().addingTimeInterval(Self.timeout)
        while Date() < deadline {
            let text = Self.normalized(recorder.text)
            if Self.requiredMarkers.allSatisfy({ text.contains($0) }) {
                return
            }
            Thread.sleep(forTimeInterval: 0.05)
        }
        let text = Self.normalized(recorder.text)
        if let missingMarker = Self.requiredMarkers.first(where: { !text.contains($0) }) {
            throw OrlixOCIDerivedStdioRuntimeProofError.missingMarker(
                missingMarker,
                text
            )
        }
        throw OrlixOCIDerivedStdioRuntimeProofError.missingMarker(
            "OCI virtio-fs runtime markers",
            text
        )
    }

    private static func validateLifecycle(
        run: OrlixOCIEnvironmentRunResult,
        finalState: OrlixOCIRuntimeStateReport,
        deletedEnvironment: OrlixOCIEnvironmentDeleteResult,
        lifecycleRecordURL: URL,
        environmentDirectoryURL: URL
    ) throws {
        guard run.startedStateReport.status == .running,
              run.startedStateReport.pid != nil else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI virtio-fs proof started lifecycle state running"
            )
        }
        guard run.completedStateReport.status == .stopped,
              run.completedStateReport.exitStatus == 0 else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI virtio-fs proof stopped exit 0"
            )
        }
        guard finalState.status == .stopped,
              finalState.exitStatus == 0 else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI virtio-fs final lifecycle state stopped exit 0"
            )
        }
        guard deletedEnvironment.id == Self.environmentID,
              deletedEnvironment.lifecycleState == .deleted else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI virtio-fs proof delete cleanup"
            )
        }
        guard !FileManager.default.fileExists(atPath: lifecycleRecordURL.path) else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI virtio-fs lifecycle record cleanup"
            )
        }
        guard !FileManager.default.fileExists(atPath: environmentDirectoryURL.path) else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI virtio-fs environment directory cleanup"
            )
        }
    }

    private static func normalized(_ text: String) -> String {
        text.replacingOccurrences(of: "\r\n", with: "\n")
            .replacingOccurrences(of: "\r", with: "\n")
    }
}

private final class OrlixOCIHostMountTargetRuntimeProof: @unchecked Sendable {
    private static let timeout: TimeInterval = 120
    private static let environmentID = "oci-imported-runtime-test-fixture"
    private static let rootImageIdentifier =
        "orlix.test.environment.oci-runtime-test-fixture"
    private let fileManager = FileManager.default
    private let readOnly: Bool

    init(readOnly: Bool = false) {
        self.readOnly = readOnly
    }

    func run() throws -> String {
        let text = try runCase(
            environmentID: Self.environmentID,
            readOnly: readOnly
        )
        return text + (readOnly ?
            "\nORLIX_OCI_HOST_MOUNT_TARGET_RO_OK\n" :
            "\nORLIX_OCI_HOST_MOUNT_TARGET_RW_OK\n")
    }

    private func runCase(environmentID: String, readOnly: Bool) throws -> String {
        let sourceRoot = OrlixAppLaunchRuntimeRunner.fixtureRoot()
        let ready = sourceRoot.appendingPathComponent(".ready", isDirectory: false)
        guard fileManager.fileExists(atPath: ready.path) else {
            throw OrlixOCIDerivedStdioRuntimeProofError.missingFixture(ready.path)
        }

        let copiedRoot = FileManager.default.temporaryDirectory
            .appendingPathComponent("orlix-oci-host-mount-\(UUID().uuidString)", isDirectory: true)
        try fileManager.copyItem(at: sourceRoot, to: copiedRoot)
        defer {
            try? fileManager.removeItem(at: copiedRoot)
        }

        let hostRoot = FileManager.default.temporaryDirectory
            .appendingPathComponent("orlix-oci-host-mount-source-\(UUID().uuidString)", isDirectory: true)
        try Self.createHostDirectoryFixture(at: hostRoot, fileManager: fileManager)
        defer {
            try? fileManager.removeItem(at: hostRoot)
        }

        let fixture = OrlixRuntimeEnvironmentFixture(root: copiedRoot)
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: fixture.linuxStateRoot,
            cacheRoot: fixture.cacheRoot,
            scratchRoot: fixture.scratchRoot
        )
        let terminal = OrlixTerminalSession()
        let recorder = OrlixRuntimeProofOutputRecorder()
        let output = terminal.attachOutput { data in
            recorder.append(data)
        }
        defer {
            output.cancel()
        }

        let layout = try OrlixEnvironmentStorageLayout.layout(
            forEnvironmentID: environmentID,
            linuxStateRoot: fixture.linuxStateRoot,
            cacheRoot: fixture.cacheRoot,
            scratchRoot: fixture.scratchRoot
        )
        try Self.writeOCIRuntimeConfig(
            hostDirectory: hostRoot,
            readOnly: readOnly,
            rootPath: "imported-root",
            to: fixture.root
        )

        let runtime = OrlixOCIRuntime(registry: registry)
        let lifecycle = try OrlixOCIRuntimeBundle
            .load(from: fixture.root)
            .lifecycleController(id: environmentID)
            .create()
        let processHandle = try OrlixOCIRuntimeProcessHandle(
            lifecycle: lifecycle,
            rootMount: .defaultOverlay,
            rootImageIdentifier: Self.rootImageIdentifier
        )
        try registry.save(processHandle.sessionDescriptor.environment)
        try runtime.lifecycleStore.save(lifecycle)

        let installer = OrlixOCIEnvironmentInstaller(registry: registry)
        let run = try installer.run(
            id: environmentID,
            terminal: terminal,
            observationTimeout: Self.timeout
        )
        let finalState = try installer.state(id: environmentID)
        let lifecycleRecordURL = try runtime.lifecycleStore.recordURL(forID: environmentID)
        let environmentDirectoryURL = layout.rootDirectory
        let deletedEnvironment = try installer.delete(id: environmentID)

        let text = Self.normalized(recorder.text)
        try Self.validateText(text, readOnly: readOnly)
        try Self.validateLifecycle(
            environmentID: environmentID,
            run: run,
            finalState: finalState,
            deletedEnvironment: deletedEnvironment,
            lifecycleRecordURL: lifecycleRecordURL,
            environmentDirectoryURL: environmentDirectoryURL
        )
        try Self.validateHostDirectoryProof(at: hostRoot, readOnly: readOnly)

        return text
    }

    private static func createHostDirectoryFixture(
        at root: URL,
        fileManager: FileManager
    ) throws {
        let nested = root
            .appendingPathComponent("nested", isDirectory: true)
            .appendingPathComponent("deeper", isDirectory: true)
        try fileManager.createDirectory(
            at: nested,
            withIntermediateDirectories: true
        )
        try Data("orlix virtio-fs fixture\n".utf8).write(
            to: root.appendingPathComponent("root-file.txt", isDirectory: false)
        )
        try Data("nested fixture\n".utf8).write(
            to: nested.appendingPathComponent("nested-file.txt", isDirectory: false)
        )
    }

    private static func validateHostDirectoryProof(at hostRoot: URL, readOnly: Bool) throws {
        let proof = hostRoot.appendingPathComponent(
            "oci-target-write-probe",
            isDirectory: false
        )
        if readOnly {
            guard !FileManager.default.fileExists(atPath: proof.path) else {
                throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                    "expected OCI read-only host mount to reject host write proof"
                )
            }
            return
        }

        let data = try Data(contentsOf: proof)
        guard String(decoding: data, as: UTF8.self) ==
            "orlix configured host mount writable\n" else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI configured host mount write proof payload"
            )
        }
    }

    private static func writeOCIRuntimeConfig(
        hostDirectory: URL,
        readOnly: Bool,
        rootPath: String,
        to bundleRoot: URL
    ) throws {
        var args = ["/orlix/oci_host_mount_target_probe"]
        if readOnly {
            args.append("--readonly")
        }
        let process: [String: Any] = [
            "terminal": true,
            "args": args,
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
            "hostname": readOnly ? "oci-host-mount-ro" : "oci-host-mount-rw",
            "domainname": "oci.example",
            "mounts": [
                [
                    "destination": "/mnt/oci-host",
                    "type": "bind",
                    "source": hostDirectory.path,
                    "options": readOnly ? ["bind", "ro"] : ["bind", "rw"],
                ],
            ],
        ]
        let data = try JSONSerialization.data(
            withJSONObject: document,
            options: [.prettyPrinted, .sortedKeys]
        )
        try data.write(to: bundleRoot.appendingPathComponent("config.json"))
    }

    private static func validateText(_ text: String, readOnly: Bool) throws {
        if text.contains("\nnot ok ") || text.contains("ORLIX-APP-RUNTIME-RUNNER-ERROR") {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(text)
        }
    }

    private static func validateLifecycle(
        environmentID: String,
        run: OrlixOCIEnvironmentRunResult,
        finalState: OrlixOCIRuntimeStateReport,
        deletedEnvironment: OrlixOCIEnvironmentDeleteResult,
        lifecycleRecordURL: URL,
        environmentDirectoryURL: URL
    ) throws {
        guard run.startedStateReport.status == .running,
              run.startedStateReport.pid != nil else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI host mount target proof started lifecycle state running"
            )
        }
        guard run.completedStateReport.status == .stopped,
              run.completedStateReport.exitStatus == 0 else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI host mount target proof stopped exit 0"
            )
        }
        guard finalState.status == .stopped,
              finalState.exitStatus == 0 else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI host mount target final lifecycle state stopped exit 0"
            )
        }
        guard deletedEnvironment.id == environmentID,
              deletedEnvironment.lifecycleState == .deleted else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI host mount target proof delete cleanup"
            )
        }
        guard !FileManager.default.fileExists(atPath: lifecycleRecordURL.path) else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI host mount target lifecycle record cleanup"
            )
        }
        guard !FileManager.default.fileExists(atPath: environmentDirectoryURL.path) else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI host mount target environment directory cleanup"
            )
        }
    }

    private static func normalized(_ text: String) -> String {
        text.replacingOccurrences(of: "\r\n", with: "\n")
            .replacingOccurrences(of: "\r", with: "\n")
    }
}

private final class OrlixOCIDerivedSignalRuntimeProof: @unchecked Sendable {
    private static let timeout: TimeInterval = 120
    private static let requiredMarkers = [
        "ORLIX_ENV_SIGNAL_BEGIN",
        "ORLIX_ENV_SIGNAL_READY",
        "ORLIX_ENV_SIGNAL_SIGINT_TRAP",
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
            .appendingPathComponent("orlix-oci-signal-\(UUID().uuidString)", isDirectory: true)
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
            defaultCommand: ["/bin/sh", "-c", Self.signalExecutionScript],
            defaultEnvironment: [
                "HOME": "/root",
                "PATH": "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
                "TERM": "xterm-256color",
            ],
            defaultWorkingDirectory: "/",
            defaultUserID: 0,
            defaultGroupID: 0,
            hostname: "oci-signal-host",
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
            script: Self.signalExecutionScript,
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

        let driver = OrlixOCIRuntimeLinuxSessionObservationDriver(timeout: Self.timeout)
        let processSession = try OrlixOCIRuntimeProcessSession(
            lifecycle: lifecycle,
            rootMount: .defaultOverlay,
            registry: registry,
            terminal: terminal,
            lifecycleStore: runtime.lifecycleStore
        )
        let runningSession = try processSession.start(using: driver)
        try waitForMarker("ORLIX_ENV_SIGNAL_READY")
        let runningState = try runtime.lifecycleStore.stateReport(id: descriptor.id)
        let signaledSession = try runningSession.kill(signal: 2, using: driver)
        let signaledState = try runtime.lifecycleStore.stateReport(id: descriptor.id)
        _ = try signaledSession.wait(using: driver)
        let stoppedState = try runtime.lifecycleStore.stateReport(id: descriptor.id)
        let lifecycleRecordURL = try runtime.lifecycleStore.recordURL(forID: descriptor.id)
        let environmentDirectoryURL = layout.rootDirectory
        let deletedEnvironment = try OrlixOCIEnvironmentInstaller(registry: registry)
            .delete(id: descriptor.id)

        var text = Self.normalized(recorder.text)
        try Self.validateText(text)
        try Self.validateLifecycle(
            runningState: runningState,
            signaledState: signaledState,
            stoppedState: stoppedState,
            deletedEnvironment: deletedEnvironment,
            lifecycleRecordURL: lifecycleRecordURL,
            environmentDirectoryURL: environmentDirectoryURL
        )
        text += "\nORLIX_OCI_LIFECYCLE_SIGNAL_RUNNING_OK\n"
        text += "ORLIX_OCI_LIFECYCLE_SIGNAL_SENT_OK\n"
        text += "ORLIX_OCI_LIFECYCLE_SIGNAL_STOPPED_OK\n"
        text += "ORLIX_OCI_LIFECYCLE_SIGNAL_DELETE_OK\n"
        return text
    }

    private static var signalExecutionScript: String {
        [
            "trap 'printf \"%s%s\\n\" ORLIX_ENV_SIGNAL_ SIGINT_TRAP; exit 130' INT",
            "printf '%s%s\\n' ORLIX_ENV_ SIGNAL_BEGIN",
            "printf '%s%s\\n' ORLIX_ENV_SIGNAL_ READY",
            "while :; do sleep 1; done",
        ].joined(separator: "\n")
    }

    private func waitForMarker(_ marker: String) throws {
        let deadline = Date().addingTimeInterval(Self.timeout)
        while Date() < deadline {
            let text = Self.normalized(recorder.text)
            if text.contains(marker) {
                return
            }
            Thread.sleep(forTimeInterval: 0.05)
        }
        throw OrlixOCIDerivedStdioRuntimeProofError.missingMarker(
            marker,
            Self.normalized(recorder.text)
        )
    }

    private static func validateText(_ text: String) throws {
        for marker in requiredMarkers where !text.contains(marker) {
            throw OrlixOCIDerivedStdioRuntimeProofError.missingMarker(marker, text)
        }
        if text.contains("ORLIX-APP-RUNTIME-RUNNER-ERROR") {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(text)
        }
    }

    private static func validateLifecycle(
        runningState: OrlixOCIRuntimeStateReport,
        signaledState: OrlixOCIRuntimeStateReport,
        stoppedState: OrlixOCIRuntimeStateReport,
        deletedEnvironment: OrlixOCIEnvironmentDeleteResult,
        lifecycleRecordURL: URL,
        environmentDirectoryURL: URL
    ) throws {
        guard runningState.status == .running, runningState.pid != nil else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected running lifecycle state before signal"
            )
        }
        guard signaledState.status == .running, signaledState.pid == runningState.pid else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected lifecycle state to remain running until wait observes signal completion"
            )
        }
        guard stoppedState.status == .stopped, stoppedState.exitStatus == 130 else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected stopped lifecycle state exit 130 after SIGINT"
            )
        }
        guard deletedEnvironment.id == "oci-imported-runtime-test-fixture" else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected delete to remove stopped signaled lifecycle record"
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
        script: String,
        rootPath: String,
        to bundleRoot: URL
    ) throws {
        let process: [String: Any] = [
            "terminal": true,
            "args": ["/bin/sh", "-c", script],
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
            "hostname": "oci-signal-host",
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

private final class OrlixAsyncRuntimeProofResultBox<Success>: @unchecked Sendable {
    private let lock = NSLock()
    private var storage: Result<Success, Error>?

    var value: Result<Success, Error>? {
        lock.lock()
        defer {
            lock.unlock()
        }
        return storage
    }

    func set(_ result: Result<Success, Error>) {
        lock.lock()
        storage = result
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
