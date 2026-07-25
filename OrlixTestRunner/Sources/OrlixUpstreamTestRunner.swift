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
    let timeout: TimeInterval
    let kernelCommandLineSuffix: String?
    let expectedKUnitSuite: String?
    let hostDirectoryFixture: Bool

    var selectedKernelTest: String? {
        guard suite == .kernel, let kernelCommandLineSuffix else {
            return nil
        }

        return kernelCommandLineSuffix
            .split(whereSeparator: \.isWhitespace)
            .first { $0.hasPrefix("orlix.kselftest=") }
            .map { String($0.dropFirst("orlix.kselftest=".count)) }
    }

    init(
        suite: OrlixUpstreamTestSuite,
        completionMarker: String,
        timeout: TimeInterval,
        kernelCommandLineSuffix: String?,
        expectedKUnitSuite: String? = nil,
        hostDirectoryFixture: Bool = false
    ) {
        self.suite = suite
        self.completionMarker = completionMarker
        self.timeout = timeout
        self.kernelCommandLineSuffix = kernelCommandLineSuffix
        self.expectedKUnitSuite = expectedKUnitSuite
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
        timeout: 300,
        kernelCommandLineSuffix: nil,
        hostDirectoryFixture: true
    )

    static let kernelTCTIAtomicMemoryDiagnostic = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix:
            "kunit.filter_glob=orlix-tcti-atomic-memory",
        expectedKUnitSuite: "orlix-tcti-atomic-memory",
        hostDirectoryFixture: true
    )

    static let kernelTCTIKthreadHandoffDiagnostic = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix:
            "kunit.filter_glob=orlix-tcti-kthread-handoff",
        expectedKUnitSuite: "orlix-tcti-kthread-handoff",
        hostDirectoryFixture: true
    )

    static let kernelMountNamespace = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=mount_namespace_probe"
    )

    static let kernelNamespace = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=namespace_probe"
    )

    static let kernelEnvironmentEntry = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=environment_entry_probe"
    )

    static let kernelInitExec = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=init_exec_probe"
    )

    static let kernelFDExec = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=fd_exec_probe"
    )

    static let kernelFDAlias = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=fd_alias_probe"
    )

    static let kernelPTYTerminal = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=pty_terminal_probe"
    )

    static let kernelTCTICrypto = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=tcti_crypto_probe"
    )

    static let kernelSignalWait = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=signal_wait_probe"
    )

    static let kernelPipePoll = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=pipe_poll_probe"
    )

    static let kernelPipeSelect = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=pipe_select_probe"
    )

    static let kernelPipeEpoll = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=pipe_epoll_probe"
    )

    static let kernelPseudoFS = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=pseudo_fs_probe"
    )

    static let kernelCgroupV2 = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=cgroup_v2_probe"
    )

    static let kernelCgroupIO = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=cgroup_io_probe"
    )

    static let kernelCgroupPids = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=cgroup_pids_probe"
    )

    static let kernelCgroupNamespace = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=cgroup_namespace_probe"
    )

    static let kernelUserNamespace = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=user_namespace_probe"
    )

    static let kernelOverlayFS = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=overlayfs_probe"
    )

    static let kernelTimeNamespace = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=time_namespace_probe"
    )

    static let kernelTimeSurface = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=time_surface_probe"
    )

    static let kernelIPCNamespace = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=ipc_namespace_probe"
    )

    static let kernelPathErrno = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=path_errno_probe"
    )

    static let kernelCloneThread = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=clone_thread_probe"
    )

    static let kernelBootProfile = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=boot_profile_contract"
    )

    static let kernelRandomDevice = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=random_device_probe"
    )

    static let kernelVirtioBlockEnvironment = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=virtio_blk_environment_probe"
    )

    static let kernelVirtioMMIOContract = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=virtio_mmio_probe_contract"
    )

    static let kernelVirtioNetDevice = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=virtio_net_device_probe"
    )

    static let kernelVirtioFSMount = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=virtio_fs_mount_probe",
        hostDirectoryFixture: true
    )

    static let kernelNetworkNamespace = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=network_namespace_probe"
    )

    static let kernelRlimit = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=rlimit_probe"
    )

    static let kernelStackGrowth = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=stack_growth_probe"
    )

    static let kernelProcessCapability = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=process_capability_probe"
    )

    static let kernelProcessLifecycle = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=process_lifecycle_probe"
    )

    static let kernelUmask = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=umask_probe"
    )

    static let kernelReadonlyRoot = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=readonly_root_probe orlix.root.readonly=1"
    )

    static let kernelHostnameDomainname = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=hostname_domainname_probe orlix.hostname=oci-host orlix.domainname=oci.example"
    )

    static let kernelEnvironmentStateWriteback = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix: "orlix.kselftest=environment_state_writeback_probe"
    )

    static let kernelEnvironmentStateCrossbootWrite = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix:
            "orlix.kselftest=environment_state_crossboot_write_probe"
    )

    static let kernelEnvironmentStateCrossbootVerify = OrlixUpstreamTestRunSpec(
        suite: .kernel,
        completionMarker: "ORLIX-KSELFTEST-END",
        timeout: 300,
        kernelCommandLineSuffix:
            "orlix.kselftest=environment_state_crossboot_verify_probe"
    )

    static let mlibc = OrlixUpstreamTestRunSpec(
        suite: .mlibc,
        completionMarker: "ORLIX-MLIBC-TEST-END",
        timeout: 1_200,
        kernelCommandLineSuffix: nil
    )

    static let coreutils = OrlixUpstreamTestRunSpec(
        suite: .coreutils,
        completionMarker: "ORLIX-COREUTILS-TEST-END",
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
    case kernelPanic(String, outputTail: String)
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
        case let .kernelPanic(marker, outputTail):
            return "kernel panic marker found: \(marker)\n\(outputTail)"
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
        for spec: OrlixUpstreamTestRunSpec,
        expectedCoreutilsManifest: [String]? = nil
    ) throws {
        let output = Self.normalized(rawOutput)

        if let marker = Self.firstMarker(in: output, markers: Self.crashMarkers) {
            throw OrlixUpstreamTestRunError.crashReport(marker)
        }
        if let marker = Self.firstMarker(in: output, markers: Self.panicMarkers) {
            throw OrlixUpstreamTestRunError.kernelPanic(
                marker,
                outputTail: Self.outputTail(output)
            )
        }
        if let marker = Self.firstMarker(in: output, markers: Self.oomMarkers) {
            throw OrlixUpstreamTestRunError.oom(marker)
        }
        if let expectedKUnitSuite = spec.expectedKUnitSuite,
           Self.kunitSuiteResult(named: expectedKUnitSuite, in: output) == .failed {
            throw OrlixUpstreamTestRunError.malformedUpstreamOutput(
                "missing passing KTAP result for selected KUnit suite \(expectedKUnitSuite)"
            )
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
        case .kernel:
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
            if let selectedKernelTest = spec.selectedKernelTest,
               !Self.containsPassingTAPResult(named: selectedKernelTest, in: output) {
                throw OrlixUpstreamTestRunError.malformedUpstreamOutput(
                    "missing passing TAP result for selected kernel test \(selectedKernelTest)"
                )
            }
            if let expectedKUnitSuite = spec.expectedKUnitSuite,
               Self.kunitSuiteResult(named: expectedKUnitSuite, in: output) != .passed {
                throw OrlixUpstreamTestRunError.malformedUpstreamOutput(
                    "missing passing KTAP result for selected KUnit suite \(expectedKUnitSuite)"
                )
            }
        case .mlibc:
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
            try validateCoreutilsCompletion(
                output,
                for: spec,
                expectedManifest: expectedCoreutilsManifest
            )
        }
    }

    private func validateCoreutilsCompletion(
        _ output: String,
        for spec: OrlixUpstreamTestRunSpec,
        expectedManifest: [String]?
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
            let expectedManifest,
            !expectedManifest.isEmpty
        else {
            throw OrlixUpstreamTestRunError.malformedCoreutilsCompletion(line)
        }

        let observedManifest = output
            .split(separator: "\n", omittingEmptySubsequences: false)
            .compactMap { line -> String? in
                let fields = line.split(separator: " ")
                guard fields.count == 3,
                      fields[0] == "ORLIX-COREUTILS-TEST-RUNNING",
                      Int(fields[1]) != nil
                else {
                    return nil
                }
                return "\(fields[1]) \(fields[2])"
            }

        guard observedManifest == expectedManifest else {
            throw OrlixUpstreamTestRunError.malformedUpstreamOutput(
                "Coreutils execution does not match the packaged upstream manifest"
            )
        }

        guard failures == 0, skips == 0, total == observedManifest.count else {
            throw OrlixUpstreamTestRunError.coreutilsSummaryFailed(
                failures: failures,
                skips: skips,
                total: total,
                expectedTotal: observedManifest.count
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

    private static func containsPassingTAPResult(
        named expectedName: String,
        in output: String
    ) -> Bool {
        output
            .split(separator: "\n", omittingEmptySubsequences: false)
            .map { $0.trimmingCharacters(in: .whitespaces) }
            .contains { line in
                let fields = line.split(separator: " ", omittingEmptySubsequences: true)
                guard fields.count >= 3,
                      fields[0] == "ok",
                      Int(fields[1]) != nil else {
                    return false
                }
                return fields.last == Substring(expectedName)
            }
    }

    private enum KUnitSuiteResult: Equatable {
        case absent
        case failed
        case passed
    }

    private static func kunitSuiteResult(
        named expectedName: String,
        in output: String
    ) -> KUnitSuiteResult {
        let lines = output
            .split(separator: "\n", omittingEmptySubsequences: false)
            .map(String.init)
        let subtest = "    # Subtest: \(expectedName)"

        for index in lines.indices where lines[index] == "    KTAP version 1" {
            guard index + 1 < lines.endIndex, lines[index + 1] == subtest else {
                continue
            }

            for resultIndex in lines.index(after: index + 1)..<lines.endIndex {
                let line = lines[resultIndex]

                guard !line.hasPrefix(" ") else {
                    continue
                }
                if line.hasPrefix("ok ") {
                    let fields = line.split(separator: " ", omittingEmptySubsequences: true)

                    guard fields.count == 3,
                          fields[0] == "ok",
                          Int(fields[1]) != nil,
                          fields[2] == Substring(expectedName) else {
                        return .failed
                    }
                    return .passed
                }
                if line.hasPrefix("not ok ") {
                    let fields = line.split(separator: " ", omittingEmptySubsequences: true)

                    guard fields.count >= 4,
                          fields[0] == "not",
                          fields[1] == "ok",
                          Int(fields[2]) != nil,
                          fields[3] == Substring(expectedName) else {
                        return .failed
                    }
                    return .failed
                }
            }
            return .failed
        }

        return .absent
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
		let completion = DispatchSemaphore(value: 0)

		return try run(
			signalCompletion: { completion.signal() },
			waitForCompletion: { timeout in
				completion.wait(timeout: .now() + timeout) == .success
			}
		)
	}

	func run(
		signalCompletion: @escaping () -> Void,
		waitForCompletion: (TimeInterval) -> Bool
	) throws -> String {
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
        guard let rootBundleURL = Bundle.main.url(
            forResource: rootBundleResourceName,
            withExtension: rootBundleExtension
        ) else {
            throw OrlixUpstreamTestRunError.missingRootfsBundle(
                "\(rootBundleResourceName).\(rootBundleExtension)"
            )
        }
        let expectedCoreutilsManifest = try Self.coreutilsManifest(
            for: spec,
            in: rootBundleURL
        )

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
		session.terminal.resize(rows: 24, columns: 80)
		let completion = OrlixUpstreamTestCompletion(signalCompletion)
        let bootStatus = BootStatusRecorder()
        let output = session.terminal.attachOutput { data in
            recorder.append(data)
            let text = Self.combinedUpstreamOutput(
                terminal: recorder.text,
                console: session.recentConsoleOutputText
            )
            if self.parser.containsTerminalCondition(text, for: self.spec) {
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

		guard waitForCompletion(spec.timeout) else {
            let text = Self.combinedUpstreamOutput(
                terminal: recorder.text,
                console: session.recentConsoleOutputText
            )
            if parser.containsTerminalCondition(text, for: spec) {
                try parser.validate(
                    text,
                    for: spec,
                    expectedCoreutilsManifest: expectedCoreutilsManifest
                )
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

        let text = Self.combinedUpstreamOutput(
            terminal: recorder.text,
            console: session.recentConsoleOutputText
        )
        try parser.validate(
            text,
            for: spec,
            expectedCoreutilsManifest: expectedCoreutilsManifest
        )
        return text
    }

    private static func coreutilsManifest(
        for spec: OrlixUpstreamTestRunSpec,
        in rootBundleURL: URL
    ) throws -> [String]? {
        guard spec.suite == .coreutils else {
            return nil
        }

        let manifestURL = rootBundleURL
            .appendingPathComponent("coreutils-test-manifest")
            .appendingPathExtension("txt")
        guard let contents = try? String(contentsOf: manifestURL, encoding: .utf8) else {
            throw OrlixUpstreamTestRunError.malformedUpstreamOutput(
                "missing packaged Coreutils test manifest: \(manifestURL.lastPathComponent)"
            )
        }

        let entries = contents.split(separator: "\n")
        guard !entries.isEmpty,
              entries.allSatisfy({ $0.split(separator: " ").count == 2 })
        else {
            throw OrlixUpstreamTestRunError.malformedUpstreamOutput(
                "malformed packaged Coreutils test manifest"
            )
        }
        return entries.enumerated().map { index, line in
            let test = line.split(separator: " ")[1]
            return "\(index + 1) \(test)"
        }
    }

    static func combinedUpstreamOutput(
        terminal: String,
        console: String
    ) -> String {
        if terminal.isEmpty {
            return console
        }
        return terminal
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

private final class OrlixUpstreamTestCompletion: @unchecked Sendable {
	private let lock = NSLock()
	private let action: () -> Void
	private var signaled = false

	init(_ action: @escaping () -> Void) {
		self.action = action
	}

	func signal() {
		lock.lock()
		guard !signaled else {
			lock.unlock()
			return
		}
		signaled = true
		lock.unlock()
		action()
	}
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
		case "ociStopSignal":
			output = try OrlixOCIDerivedStopSignalRuntimeProof().run()
		case "ociKillSignal":
			output = try OrlixOCIDerivedKillSignalRuntimeProof().run()
		case "ociLifecycleState":
			output = try OrlixOCIDerivedLifecycleStateRuntimeProof().run()
		case "ociHealthcheck":
			output = try OrlixOCIDerivedHealthcheckRuntimeProof().run()
		case "ociRun":
			output = try OrlixOCIDerivedRunCommandRuntimeProof().run()
		case "ociRunLiveRegistry":
			output = try OrlixOCIDerivedRunCommandRuntimeProof(
				registryMode: .live
			).run()
		case "ociRunLiveRegistryBusybox":
			output = try OrlixOCIDerivedLiveRegistryBusyboxRuntimeProof().run()
        case "ociRunLiveRegistryAlpine":
            output = try OrlixOCIDerivedLiveRegistryAlpineRuntimeProof().run()
        case "ociLiveRegistryAlpineRootfsImport":
            output = try OrlixOCIDerivedLiveRegistryAlpineRootfsImportProof().run()
		case "ociLiveRegistryAlpineLinuxMaterialization":
			output = try OrlixOCIDerivedLiveRegistryAlpineRootfsImportProof(
				useLinuxMaterialization: true
			).run()
		case "ociLiveRegistryAlpineMaterializationProbe":
			output = try OrlixOCIDerivedLiveRegistryAlpineRootfsImportProof(
				probeLinuxMaterialization: true
			).run()
		case "ociLiveRegistryAlpineGeneratedRoot":
			output = try OrlixOCIDerivedLiveRegistryAlpineRootfsImportProof
				.validatePreparedLinuxMaterializedAlpine()
		case "orlixPayloadE2fsprogs":
			output = try OrlixPayloadE2fsprogsRuntimeProof().run()
        case "orlixPayloadE2fsprogsMaterialization":
            output = try OrlixPayloadE2fsprogsRuntimeProof(
                script: OrlixPayloadE2fsprogsRuntimeProof
                    .materializationScript,
                requiredMarkers: OrlixPayloadE2fsprogsRuntimeProof
                    .materializationMarkers,
                timeout: 180
            ).run()
        case "ociTerminalLiveRegistry":
            output = try OrlixOCIDerivedLiveRegistryTerminalProof().run()
		case "ociTerminalLiveRegistryAlpine":
			output = try OrlixOCIDerivedLiveRegistryTerminalProof(
				liveImageReference: "alpine:3.20",
				terminalEnvironmentID:
					"oci-live-registry-terminal-alpine-test-fixture",
				successMarker: "ORLIX_OCI_LIVE_REGISTRY_TERMINAL_ALPINE_OK"
			).run()
		case "ociTerminalLiveRegistryAlpineInput":
			output = try OrlixOCIDerivedLiveRegistryTerminalProof(
				liveImageReference: "alpine:3.20",
				terminalEnvironmentID:
					"oci-live-registry-terminal-alpine-input-test-fixture",
				executionScript:
					OrlixOCIDerivedLiveRegistryTerminalProof
					.interactiveExecutionScript,
				requiredMarkers:
					OrlixOCIDerivedLiveRegistryTerminalProof
					.interactiveRequiredMarkers,
				inputAfterMarker:
					"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_INPUT_READY",
input: Data("orlix-interactive-alpine\n".utf8),
successMarker:
"ORLIX_OCI_LIVE_REGISTRY_TERMINAL_ALPINE_INPUT_OK"
).run()
case "ociTerminalLiveRegistryAlpineResize":
output = try OrlixOCIDerivedLiveRegistryTerminalProof(
liveImageReference: "alpine:3.20",
terminalEnvironmentID:
"oci-live-registry-terminal-alpine-resize-test-fixture",
executionScript: OrlixOCIDerivedLiveRegistryTerminalProof
.dynamicResizeExecutionScript,
requiredMarkers: OrlixOCIDerivedLiveRegistryTerminalProof
.dynamicResizeRequiredMarkers,
inputAfterMarker:
"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_RESIZE_READY",
input: Data("\n".utf8),
resizeAfterMarker:
"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_RESIZE_READY",
resizeRows: 41,
resizeColumns: 117,
successMarker:
"ORLIX_OCI_LIVE_REGISTRY_TERMINAL_ALPINE_RESIZE_OK"
).run()
case "ociTerminalLiveRegistryAlpineSIGWINCH":
output = try OrlixOCIDerivedLiveRegistryTerminalProof(
liveImageReference: "alpine:3.20",
terminalEnvironmentID:
"oci-live-registry-terminal-alpine-sigwinch-test-fixture",
executionScript: OrlixOCIDerivedLiveRegistryTerminalProof
.sigwinchExecutionScript,
requiredMarkers: OrlixOCIDerivedLiveRegistryTerminalProof
.sigwinchRequiredMarkers,
resizeAfterMarker:
"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_SIGWINCH_READY",
resizeRows: 43,
resizeColumns: 119,
inputAfterResize: Data("\n".utf8),
successMarker:
"ORLIX_OCI_LIVE_REGISTRY_TERMINAL_ALPINE_SIGWINCH_OK"
).run()
case "ociTerminalLiveRegistryAlpineSIGTSTP":
output = try OrlixOCIDerivedLiveRegistryTerminalProof(
liveImageReference: "alpine:3.20",
terminalEnvironmentID:
"oci-live-registry-terminal-alpine-sigtstp-test-fixture",
executionScript: OrlixOCIDerivedLiveRegistryTerminalProof
.sigtstpExecutionScript,
requiredMarkers: OrlixOCIDerivedLiveRegistryTerminalProof
.sigtstpRequiredMarkers,
inputAfterMarker:
"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_SIGTSTP_READY",
input: Data([0x1a]),
expectedExitStatus: 148,
successMarker:
"ORLIX_OCI_LIVE_REGISTRY_TERMINAL_ALPINE_SIGTSTP_OK"
).run()
case "ociTerminalLiveRegistryBusyboxSize":
output = try OrlixOCIDerivedLiveRegistryTerminalProof(
liveImageReference: "registry.k8s.io/e2e-test-images/busybox:1.29-4",
				terminalEnvironmentID: "oci-live-registry-terminal-busybox-size-test-fixture",
				executionScript: OrlixOCIDerivedLiveRegistryTerminalProof
					.terminalSizeExecutionScript,
				requiredMarkers: OrlixOCIDerivedLiveRegistryTerminalProof
					.terminalSizeRequiredMarkers,
				terminalRows: 37,
				terminalColumns: 132,
				readOnlyRoot: true,
				successMarker: "ORLIX_OCI_LIVE_REGISTRY_TERMINAL_BUSYBOX_SIZE_OK"
			).run()
		case "ociNetwork":
			output = try OrlixOCIDerivedNetworkRuntimeProof().run()
		case "ociVirtioNet":
			output = try OrlixOCIDerivedVirtioNetRuntimeProof().run()
		case "ociCgroupResources":
			output = try OrlixOCIDerivedCgroupResourcesRuntimeProof().run()
        case "ociDeviceNodes":
            output = try OrlixOCIDerivedDeviceNodesRuntimeProof().run()
            case "ociNamespaceIdentity":
                output = try OrlixOCIDerivedNamespaceIdentityRuntimeProof().run()
            case "ociTimeNamespace":
                output = try OrlixOCIDerivedTimeNamespaceRuntimeProof().run()
            case "ociProcessAttributes":
                output = try OrlixOCIDerivedProcessAttributesRuntimeProof().run()
        case "ociRootfsControls":
            output = try OrlixOCIDerivedRootfsControlsRuntimeProof().run()
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

private final class OrlixOCIDerivedHealthcheckRuntimeProof: @unchecked Sendable {
	private static let timeout: TimeInterval = 240
	private static let environmentID = "oci-imported-runtime-test-fixture"
	private static let requiredMarkers = [
		"ORLIX_ENV_HEALTHCHECK_BEGIN",
		"ORLIX_ENV_HEALTHCHECK_PROC_OK",
		"ORLIX_ENV_HEALTHCHECK_DEV_OK",
		"ORLIX_ENV_HEALTHCHECK_DONE",
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
			.appendingPathComponent(
				"orlix-oci-healthcheck-\(UUID().uuidString)",
				isDirectory: true
			)
		try fileManager.copyItem(at: sourceRoot, to: copiedRoot)
		defer { try? fileManager.removeItem(at: copiedRoot) }

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
		defer { output.cancel() }

		try OrlixOCIDerivedStdioRuntimeProof.writeOCIRuntimeConfig(
			terminal: false,
			script: "printf '%s\\n' ORLIX_ENV_BASE_COMMAND_SHOULD_NOT_RUN",
			rootPath: "imported-root",
			to: fixture.root
		)

		let lifecycle = try OrlixOCIRuntimeBundle
			.load(from: fixture.root)
			.lifecycleController(id: Self.environmentID)
			.create()
		let processHandle = try OrlixOCIRuntimeProcessHandle(
			lifecycle: lifecycle,
			rootMount: .defaultOverlay,
			rootImageIdentifier: "orlix.test.environment.oci-runtime-test-fixture"
		)
		let runtime = OrlixOCIRuntime(registry: registry)
		let healthcheck = OrlixEnvironmentHealthcheck(
			test: ["CMD-SHELL", Self.healthcheckScript]
		)
		try registry.save(
			Self.descriptor(
				processHandle.sessionDescriptor.environment,
				healthcheck: healthcheck
			)
		)
		try runtime.lifecycleStore.save(lifecycle)

		let installer = OrlixOCIEnvironmentInstaller(registry: registry)
		let result = try installer.healthcheck(
			id: Self.environmentID,
			terminal: terminal,
			observationTimeout: Self.timeout
		)
		let finalState = try installer.state(id: Self.environmentID)
		let lifecycleRecordURL = try runtime.lifecycleStore.recordURL(
			forID: Self.environmentID
		)
		let layout = try OrlixEnvironmentStorageLayout.layout(
			forEnvironmentID: Self.environmentID,
			linuxStateRoot: fixture.linuxStateRoot,
			cacheRoot: fixture.cacheRoot,
			scratchRoot: fixture.scratchRoot
		)
		let deletedEnvironment = try installer.delete(id: Self.environmentID)

		var text = Self.normalized(recorder.text)
		try Self.validateText(text)
		guard result.command == ["/bin/sh", "-c", Self.healthcheckScript] else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"unexpected healthcheck command \(result.command)"
			)
		}
		guard result.runResult.completedStateReport.status == .stopped,
			result.runResult.completedStateReport.exitStatus == 0,
			finalState.status == .stopped,
			finalState.exitStatus == 0
		else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected healthcheck lifecycle stopped exit 0"
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
		guard !FileManager.default.fileExists(atPath: layout.rootDirectory.path) else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected environment directory cleanup"
			)
		}
		text += "\nORLIX_OCI_HEALTHCHECK_COMMAND_OK\n"
		text += "ORLIX_OCI_HEALTHCHECK_RUNTIME_STOPPED_OK\n"
		text += "ORLIX_OCI_HEALTHCHECK_RUNTIME_DELETE_OK\n"
		return text
	}

	private static var healthcheckScript: String {
		[
			"printf '%s\\n' ORLIX_ENV_HEALTHCHECK_BEGIN",
			"if /bin/test -r /proc/self/status; then printf '%s\\n' ORLIX_ENV_HEALTHCHECK_PROC_OK; else exit 42; fi",
			"if /bin/test -c /dev/null; then printf '%s\\n' ORLIX_ENV_HEALTHCHECK_DEV_OK; else exit 43; fi",
			"printf '%s\\n' ORLIX_ENV_HEALTHCHECK_DONE",
		].joined(separator: "\n")
	}

	private static func validateText(_ text: String) throws {
		for marker in requiredMarkers where !text.contains(marker) {
			throw OrlixOCIDerivedStdioRuntimeProofError.missingMarker(marker, text)
		}
		if text.contains("\nnot ok ")
			|| text.contains("ORLIX-APP-RUNTIME-RUNNER-ERROR")
			|| text.contains("ORLIX_ENV_BASE_COMMAND_SHOULD_NOT_RUN")
		{
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(text)
		}
	}

	private static func normalized(_ text: String) -> String {
		text.replacingOccurrences(of: "\r\n", with: "\n")
			.replacingOccurrences(of: "\r", with: "\n")
	}

	private static func descriptor(
		_ descriptor: OrlixEnvironmentDescriptor,
		healthcheck: OrlixEnvironmentHealthcheck
	) -> OrlixEnvironmentDescriptor {
		OrlixEnvironmentDescriptor(
			id: descriptor.id,
			source: descriptor.source,
			platform: descriptor.platform,
			rootImageIdentifier: descriptor.rootImageIdentifier,
			defaultCommand: descriptor.defaultCommand,
			defaultEnvironment: descriptor.defaultEnvironment,
			defaultWorkingDirectory: descriptor.defaultWorkingDirectory,
			defaultUserID: descriptor.defaultUserID,
			defaultGroupID: descriptor.defaultGroupID,
			defaultSupplementaryGroups: descriptor.defaultSupplementaryGroups,
			defaultCapabilities: descriptor.defaultCapabilities,
			defaultNoNewPrivileges: descriptor.defaultNoNewPrivileges,
			defaultCloseAdditionalFds: descriptor.defaultCloseAdditionalFds,
			defaultStopSignal: descriptor.defaultStopSignal,
			defaultTerminal: descriptor.defaultTerminal,
			defaultTerminalRows: descriptor.defaultTerminalRows,
			defaultTerminalColumns: descriptor.defaultTerminalColumns,
			defaultOOMScoreAdjustment: descriptor.defaultOOMScoreAdjustment,
			defaultScheduler: descriptor.defaultScheduler,
			defaultIOPriority: descriptor.defaultIOPriority,
			defaultCPUAffinity: descriptor.defaultCPUAffinity,
			defaultUmask: descriptor.defaultUmask,
			defaultRlimits: descriptor.defaultRlimits,
			defaultPersonalityDomain: descriptor.defaultPersonalityDomain,
			hostname: descriptor.hostname,
			domainname: descriptor.domainname,
			rootMount: descriptor.rootMount,
			rootReadonly: descriptor.rootReadonly,
			rootPropagation: descriptor.rootPropagation,
			sysctls: descriptor.sysctls,
			maskedPaths: descriptor.maskedPaths,
			readonlyPaths: descriptor.readonlyPaths,
			cgroupsPath: descriptor.cgroupsPath,
			cgroupPidsLimit: descriptor.cgroupPidsLimit,
			cgroupCPUMax: descriptor.cgroupCPUMax,
			cgroupCPUWeight: descriptor.cgroupCPUWeight,
			cgroupMemoryMax: descriptor.cgroupMemoryMax,
			cgroupIOWeight: descriptor.cgroupIOWeight,
			cgroupUnified: descriptor.cgroupUnified,
			deviceNodes: descriptor.deviceNodes,
			timeOffsets: descriptor.timeOffsets,
			uidMappings: descriptor.uidMappings,
			gidMappings: descriptor.gidMappings,
			namespaces: descriptor.namespaces,
			namespacePaths: descriptor.namespacePaths,
			tmpfsMounts: descriptor.tmpfsMounts,
			mounts: descriptor.mounts,
			exposedPorts: descriptor.exposedPorts,
			publishedPorts: descriptor.publishedPorts,
			imageVolumes: descriptor.imageVolumes,
			healthcheck: healthcheck,
			annotations: descriptor.annotations
		)
	}
}

private final class OrlixOCIDerivedRunCommandRuntimeProof: @unchecked Sendable {
    enum RegistryMode: Sendable {
        case deterministic
        case live
    }

    private static let timeout: TimeInterval = 120
    private static let fixtureEnvironmentID = "oci-imported-runtime-test-fixture"
	private static let defaultRunEnvironmentID = "oci-run-runtime-test-fixture"
	private static let deterministicImageReference =
		"registry.example.org/library/orlix-fixture:latest"
	private static let defaultLiveImageReference = "registry.k8s.io/pause:3.10"
    private static let requiredMarkers = [
        "ORLIX_ENV_ORLIX_RUN_BEGIN",
        "ORLIX_ENV_ORLIX_RUN_STDOUT_OK",
        "ORLIX_ENV_ORLIX_RUN_STDERR_OK",
        "ORLIX_ENV_ORLIX_RUN_DONE",
    ]

	private let fileManager = FileManager.default
	private let recorder = OrlixRuntimeProofOutputRecorder()
	private let registryMode: RegistryMode
	private let liveImageReference: String
	private let runEnvironmentID: String

	init(
		registryMode: RegistryMode = .deterministic,
		liveImageReference: String = defaultLiveImageReference,
		runEnvironmentID: String = defaultRunEnvironmentID
	) {
		self.registryMode = registryMode
		self.liveImageReference = liveImageReference
		self.runEnvironmentID = runEnvironmentID
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
		let imageReference = Self.imageReference(
			for: registryMode,
			liveImageReference: liveImageReference
		)
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
			forEnvironmentID: runEnvironmentID,
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
			runEnvironmentID,
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
                debugfs: URL(fileURLWithPath: "/usr/bin/orlix-debugfs"),
                e2fsck: URL(fileURLWithPath: "/usr/bin/orlix-e2fsck")
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
			expectedEnvironmentID: runEnvironmentID,
			lifecycleRecordURL: try OrlixOCIRuntime(registry: registry)
				.lifecycleStore.recordURL(forID: runEnvironmentID),
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

	private static func imageReference(
		for mode: RegistryMode,
		liveImageReference: String
	) -> String {
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

    fileprivate static func runFixtureMaterializationCommand(
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
        case "orlix-debugfs", "orlix-e2fsck":
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
expectedEnvironmentID: String,
lifecycleRecordURL: URL,
environmentDirectoryURL: URL
) throws {
guard result.installResult.id == expectedEnvironmentID else {
throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
"expected orlix run install id \(expectedEnvironmentID)"
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

private final class OrlixOCIDerivedLiveRegistryBusyboxRuntimeProof:
	@unchecked Sendable
{
	func run() throws -> String {
		var text = try OrlixOCIDerivedRunCommandRuntimeProof(
			registryMode: .live,
			liveImageReference: "registry.k8s.io/e2e-test-images/busybox:1.29-4",
			runEnvironmentID: "oci-run-live-registry-busybox-test-fixture"
		).run()
		text += "ORLIX_OCI_RUN_LIVE_REGISTRY_BUSYBOX_OK\n"
		return text
	}
}

private final class OrlixOCIDerivedLiveRegistryAlpineRuntimeProof:
    @unchecked Sendable
{
    func run() throws -> String {
		var text = try OrlixOCIDerivedRunCommandRuntimeProof(
			registryMode: .live,
			liveImageReference: "alpine:3.20",
			runEnvironmentID: "oci-run-live-registry-alpine-test-fixture"
		).run()
		text += "ORLIX_OCI_RUN_LIVE_REGISTRY_ALPINE_OK\n"
		return text
    }
}

private final class OrlixPayloadE2fsprogsRuntimeProof: @unchecked Sendable {
	private static let defaultTimeout: TimeInterval = 120
	private static let versionMarkers = [
		"ORLIX_PAYLOAD_E2FSPROGS_MKE2FS_OK",
		"ORLIX_PAYLOAD_E2FSPROGS_MKFS_EXT4_OK",
		"ORLIX_PAYLOAD_E2FSPROGS_DEBUGFS_OK",
		"ORLIX_PAYLOAD_E2FSPROGS_DONE",
	]
	static let materializationMarkers = [
		"ORLIX_MKE2FS_D_BEGIN",
		"ORLIX_MKE2FS_D_TRUNCATE_OK",
		"ORLIX_MKE2FS_D_OK",
		"ORLIX_MKE2FS_D_DEBUGFS_OK",
	]
	private static let versionScript = [
		"set -eu",
        "test -x /bin/mke2fs",
        "test -x /bin/mkfs.ext4",
        "test -x /bin/debugfs",
        "test -x /bin/e2fsck",
        "/bin/mke2fs -V >/tmp/orlix-mke2fs-version.txt 2>&1",
        "/bin/mkfs.ext4 -V >/tmp/orlix-mkfs-ext4-version.txt 2>&1",
        "/bin/debugfs -V >/tmp/orlix-debugfs-version.txt 2>&1",
        "/bin/e2fsck -V >/tmp/orlix-e2fsck-version.txt 2>&1",
        "printf 'ORLIX_PAYLOAD_E2FSPROGS_%s_OK\\n' MKE2FS",
        "printf 'ORLIX_PAYLOAD_E2FSPROGS_%s_OK\\n' MKFS_EXT4",
        "printf 'ORLIX_PAYLOAD_E2FSPROGS_%s_OK\\n' DEBUGFS",
        "printf 'ORLIX_PAYLOAD_E2FSPROGS_%s_OK\\n' E2FSCK",
		"printf 'ORLIX_PAYLOAD_E2FSPROGS_%s\\n' DONE",
	].joined(separator: "; ")
	static let materializationScript = [
		"set -eu",
		"printf 'ORLIX_%s\\n' MKE2FS_D_BEGIN",
		"mkdir -p /tmp/s",
		"echo x >/tmp/s/f",
		": >/tmp/i",
		"truncate -s 8m /tmp/i",
		"printf 'ORLIX_%s\\n' MKE2FS_D_TRUNCATE_OK",
		"mke2fs -q -t ext4 -F -m 0 -O ^metadata_csum -U clear -L ORLIXROOT -E root_owner=0:0,no_copy_xattrs -d /tmp/s /tmp/i",
		"printf 'ORLIX_%s\\n' MKE2FS_D_OK",
		"printf 'stats\\n' >/tmp/c",
		"debugfs -w -f /tmp/c /tmp/i >/tmp/debugfs.out 2>&1",
		"printf 'ORLIX_%s\\n' MKE2FS_D_DEBUGFS_OK",
	].joined(separator: "; ")

	private let script: String
	private let requiredMarkers: [String]
	private let timeout: TimeInterval

	init(
		script: String = OrlixPayloadE2fsprogsRuntimeProof.versionScript,
		requiredMarkers: [String] = OrlixPayloadE2fsprogsRuntimeProof
			.versionMarkers,
		timeout: TimeInterval = OrlixPayloadE2fsprogsRuntimeProof.defaultTimeout
	) {
		self.script = script
		self.requiredMarkers = requiredMarkers
		self.timeout = timeout
	}

    func run() throws -> String {
        guard let profile = OrlixOSDistribution.bundledBootProfile else {
            throw OrlixUpstreamTestRunError.missingBundledBootProfile
        }
        guard let rootImageIdentifier = OrlixOSDistribution
            .productRootImageIdentifier
        else {
            throw OrlixUpstreamTestRunError.missingRootImageDescriptor("product")
        }

		let kernelCommandLine = [
            OrlixEnvironmentRootImage.defaultKernelCommandLine,
            "orlix.exec=\(Self.kernelToken("/bin/sh"))",
            "orlix.argv0=\(Self.kernelToken("/bin/sh"))",
            "orlix.argv1=\(Self.kernelToken("-c"))",
            "orlix.argv2=\(Self.kernelToken(script))",
            "orlix.env0=\(Self.kernelToken("PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"))",
            "orlix.cwd=\(Self.kernelToken("/"))",
            "orlix.uid=0",
            "orlix.gid=0",
        ].joined(separator: " ")
        let session = OrlixLinuxSession(
            bootConfig: OrlixBootConfig(
                profile: profile,
                kernelCommandLine: kernelCommandLine,
                rootImageIdentifier: rootImageIdentifier,
                terminalIdentifier: "orlix.payload.e2fsprogs.terminal"
            )
        )
        let recorder = TerminalOutputRecorder()
		session.terminal.resize(rows: 24, columns: 80)
        let completion = DispatchSemaphore(value: 0)
        let bootStatus = BootStatusRecorder()
        let output = session.terminal.attachOutput { data in
            recorder.append(data)
            let text = recorder.text
			if self.requiredMarkers.allSatisfy({ text.contains($0) }) {
				completion.signal()
			}
		}
        defer { output.cancel() }

		DispatchQueue.global(qos: .userInitiated).async {
			bootStatus.set(session.boot())
			completion.signal()
		}

		guard completion.wait(timeout: .now() + timeout) == .success else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"payload e2fsprogs proof timed out\n\(recorder.text)"
			)
		}
		let text = recorder.text
		for marker in requiredMarkers where !text.contains(marker) {
            if let status = bootStatus.value, status != .ok {
                throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                    "payload e2fsprogs boot failed: \(status.message)\n\(text)"
                )
            }
            throw OrlixOCIDerivedStdioRuntimeProofError.missingMarker(
                marker,
                text
            )
        }
        return text
    }

    private static func kernelToken(_ value: String) -> String {
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
             UInt8(ascii: "="):
            return true
        default:
            return false
        }
    }
}

private final class OrlixOCIDerivedLiveRegistryAlpineRootfsImportProof:
    @unchecked Sendable
{
	private static let timeout: TimeInterval = 600
	private static let fixtureEnvironmentID = "oci-imported-runtime-test-fixture"
    private static let environmentID =
        "oci-live-registry-alpine-rootfs-import-test-fixture"

	private let fileManager = FileManager.default
	private let useLinuxMaterialization: Bool
	private let probeLinuxMaterialization: Bool

	init(
		useLinuxMaterialization: Bool = false,
		probeLinuxMaterialization: Bool = false
	) {
		self.useLinuxMaterialization = useLinuxMaterialization
		self.probeLinuxMaterialization = probeLinuxMaterialization
	}

    private struct MaterializationCommandObservation: Equatable {
        let executable: String
        let arguments: [String]
    }

    private final class MaterializationCommandRecorder: @unchecked Sendable {
        private let lock = NSLock()
        private var storage: [MaterializationCommandObservation] = []

        var commands: [MaterializationCommandObservation] {
            lock.lock()
            defer { lock.unlock() }
            return storage
        }

        func append(executable: URL, arguments: [String]) {
            lock.lock()
            storage.append(
                MaterializationCommandObservation(
                    executable: executable.path,
                    arguments: arguments
                )
            )
            lock.unlock()
        }
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

		let timeout = (useLinuxMaterialization || probeLinuxMaterialization)
			? Self.timeout * 2
			: Self.timeout
		guard completion.wait(timeout: .now() + .seconds(Int(timeout))) == .success else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"live registry Alpine rootfs import proof timed out"
			)
		}
		guard let result = resultBox.value else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"live registry Alpine rootfs import proof completed without result"
			)
		}
		return try result.get()
	}

	private func runAsync() async throws -> String {
		let sourceRoot = OrlixAppLaunchRuntimeRunner.fixtureRoot()
		let ready = sourceRoot.appendingPathComponent(".ready", isDirectory: false)
		guard fileManager.fileExists(atPath: ready.path) else {
			throw OrlixOCIDerivedStdioRuntimeProofError.missingFixture(ready.path)
		}

		let copiedRoot = useLinuxMaterialization
			? try Self.persistentLinuxMaterializationRoot()
			: FileManager.default.temporaryDirectory
				.appendingPathComponent(
					"orlix-oci-alpine-import-\(UUID().uuidString)",
					isDirectory: true
				)
		if fileManager.fileExists(atPath: copiedRoot.path) {
			try fileManager.removeItem(at: copiedRoot)
		}
		try fileManager.createDirectory(
			at: copiedRoot.deletingLastPathComponent(),
			withIntermediateDirectories: true
		)
		try fileManager.copyItem(at: sourceRoot, to: copiedRoot)
		defer {
			if !useLinuxMaterialization {
				try? fileManager.removeItem(at: copiedRoot)
			}
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
		let importedLayout = try OrlixEnvironmentStorageLayout.layout(
			forEnvironmentID: Self.environmentID,
			linuxStateRoot: fixture.linuxStateRoot,
			cacheRoot: fixture.cacheRoot,
			scratchRoot: fixture.scratchRoot
		)

        let installer = OrlixOCIEnvironmentInstaller(registry: registry)
        let materializationCommands = MaterializationCommandRecorder()
        let installResult = try await installer.install(
			image: OrlixOCIRegistryImageReference("alpine:3.20"),
			id: Self.environmentID,
			tools: OrlixOCIEnvironmentMaterializationTools(
				mke2fs: URL(fileURLWithPath: "/usr/bin/orlix-mke2fs"),
				truncate: URL(fileURLWithPath: "/usr/bin/orlix-truncate"),
				debugfs: URL(fileURLWithPath: "/usr/bin/orlix-debugfs"),
                                e2fsck: URL(fileURLWithPath: "/usr/bin/orlix-e2fsck")
			),
            puller: OrlixOCIRegistryPuller(),
            fileManager: fileManager
        ) { executable, arguments in
            materializationCommands.append(
                executable: executable,
                arguments: arguments
            )
			if !self.useLinuxMaterialization {
				try OrlixOCIDerivedRunCommandRuntimeProof
					.runFixtureMaterializationCommand(
						executable: executable,
                        arguments: arguments,
                        sourceBaseImageURL: sourceLayout.baseImageURL,
                        sourceStateImageURL: sourceLayout.stateImageURL
                    )
            }
        }

        try validateImportedRootfs(installResult)
        try validateMaterializationCommands(
            materializationCommands.commands,
            rootfs: installResult.rootfsImport.baseTreeDirectory,
            layout: importedLayout
        )
		var linuxMaterializationOutput = ""
		if probeLinuxMaterialization {
			linuxMaterializationOutput = try runLinuxMaterializationProbe(
				rootfs: installResult.rootfsImport.baseTreeDirectory,
				hostRoot: copiedRoot,
				mountPath: "/mnt/orlix-materialize"
			)
		} else if useLinuxMaterialization {
			try runLinuxMaterializationCommands(
				materializationCommands.commands,
				hostRoot: copiedRoot,
				mountPath: "/mnt/orlix-materialize"
			)
		}
		if !useLinuxMaterialization {
			let deletedEnvironment = try installer.delete(id: Self.environmentID)
			guard deletedEnvironment.lifecycleState == .deleted else {
				throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
					"expected Alpine rootfs import delete cleanup"
				)
			}
			guard !fileManager.fileExists(atPath: importedLayout.rootDirectory.path)
			else {
				throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
					"expected Alpine rootfs import environment directory cleanup"
				)
			}
		}

        var markers = [
            "ORLIX_OCI_ALPINE_ROOTFS_IMPORT_PULL_OK",
            "ORLIX_OCI_ALPINE_ROOTFS_IMPORT_LAYER_OK",
			"ORLIX_OCI_ALPINE_ROOTFS_IMPORT_STAGING_OK",
			"ORLIX_OCI_ALPINE_ROOTFS_IMPORT_APK_OK",
			"ORLIX_OCI_ALPINE_ROOTFS_IMPORT_MATERIALIZATION_PLAN_OK",
		].joined(separator: "\n") + "\n"
		if useLinuxMaterialization {
			markers += "ORLIX_OCI_ALPINE_LINUX_MATERIALIZATION_PREPARED_OK\n"
		} else if probeLinuxMaterialization {
			markers += "ORLIX_OCI_ALPINE_LINUX_MATERIALIZATION_PROBE_OK\n"
		} else {
			markers += "ORLIX_OCI_ALPINE_ROOTFS_IMPORT_DELETE_OK\n"
		}
		return linuxMaterializationOutput + markers
	}

	private func validateImportedRootfs(
		_ result: OrlixOCIRegistryEnvironmentInstallResult
	) throws {
		guard result.id == Self.environmentID else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"unexpected Alpine import id \(result.id)"
			)
		}
		guard !result.pullResult.layerDigests.isEmpty,
		      !result.rootfsImport.layerDigests.isEmpty
		else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected Alpine registry pull to include rootfs layers"
			)
		}
		let root = result.rootfsImport.baseTreeDirectory
		let requiredPaths = [
			"bin/busybox",
			"etc/alpine-release",
			"lib/apk/db/installed",
			"sbin/apk",
		]
		for path in requiredPaths {
			let url = root.appendingPathComponent(path)
			guard fileManager.fileExists(atPath: url.path) else {
				throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
					"missing Alpine staged rootfs path \(path)"
				)
			}
		}
		let installed = try String(
			contentsOf: root.appendingPathComponent("lib/apk/db/installed"),
			encoding: .utf8
		)
        guard installed.contains("P:alpine-baselayout")
            || installed.contains("P:busybox")
            || installed.contains("P:musl")
        else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "Alpine installed package database missing expected packages"
            )
        }
    }

	private func runLinuxMaterializationCommands(
		_ commands: [MaterializationCommandObservation],
		hostRoot: URL,
		mountPath: String
	) throws {
        guard let profile = OrlixOSDistribution.bundledBootProfile else {
            throw OrlixUpstreamTestRunError.missingBundledBootProfile
        }
        guard let rootImageIdentifier = OrlixOSDistribution
            .productRootImageIdentifier
        else {
            throw OrlixUpstreamTestRunError.missingRootImageDescriptor("product")
        }

		let commandScript = commands.map { command in
			([linuxMaterializationExecutable(command.executable)]
				+ command.arguments).map
			{
				shellQuote(
					linuxMaterializationPath(
						$0,
						hostRoot: hostRoot,
						mountPath: mountPath
					)
				)
			}.joined(separator: " ")
		}.joined(separator: "\n")
		let scriptURL = hostRoot.appendingPathComponent(
			".orlix-materialize.sh",
			isDirectory: false
		)
		let script = [
			"set -e",
			"printf '%s\\n' ORLIX_LINUX_MATERIALIZATION_COMMAND_BEGIN",
			commandScript,
			"printf '%s\\n' ORLIX_LINUX_MATERIALIZATION_COMMAND_DONE",
		].joined(separator: "\n") + "\n"
		try script.write(to: scriptURL, atomically: true, encoding: .utf8)
		let linuxScriptPath = mountPath + "/" + scriptURL.lastPathComponent
		let kernelCommandLine = [
			OrlixEnvironmentRootImage.defaultKernelCommandLine,
			"orlix.mount.host0.target=\(mountPath)",
			"orlix.exec=\(kernelToken("/bin/sh"))",
			"orlix.argv0=\(kernelToken("/bin/sh"))",
			"orlix.argv1=\(kernelToken(linuxScriptPath))",
            "orlix.env0=\(kernelToken("PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"))",
            "orlix.cwd=\(kernelToken("/"))",
            "orlix.uid=0",
            "orlix.gid=0",
        ].joined(separator: " ")
        let terminal = OrlixTerminalSession()
        let recorder = TerminalOutputRecorder()
        let completion = DispatchSemaphore(value: 0)
        let bootStatus = BootStatusRecorder()
        let output = terminal.attachOutput { data in
            recorder.append(data)
			if recorder.text.contains(
				"ORLIX_LINUX_MATERIALIZATION_COMMAND_DONE"
			) || recorder.text.contains(
				"orlix-init: shell exit status="
			) {
				completion.signal()
			}
        }
        defer { output.cancel() }

        let session = OrlixLinuxSession(
            bootConfig: OrlixBootConfig(
                profile: profile,
                kernelCommandLine: kernelCommandLine,
                rootImageIdentifier: rootImageIdentifier,
                terminalIdentifier: "orlix.oci.linux.materialization"
            ),
            hostDirectories: [
                OrlixHostDirectoryRegistration(
                    identifier: OrlixEnvironmentRootImage
                        .defaultHostDirectoryIdentifier,
                    hostPath: hostRoot.path,
                    readOnly: false
                )
            ],
            terminal: terminal
        )
		terminal.resize(rows: 24, columns: 80)
        DispatchQueue.global(qos: .userInitiated).async {
            let status = session.boot()
            bootStatus.set(status)
            if status != .ok {
                completion.signal()
            }
        }

        guard completion.wait(timeout: .now() + Self.timeout) == .success else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"Linux materialization command timed out: \(commandScript)\n\(recorder.text)"
            )
        }
        if let status = bootStatus.value, status != .ok {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "Linux materialization command boot failed: \(status)\n\(recorder.text)"
            )
        }
        let text = recorder.text
        guard text.contains("ORLIX_LINUX_MATERIALIZATION_COMMAND_BEGIN"),
            text.contains("ORLIX_LINUX_MATERIALIZATION_COMMAND_DONE")
        else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"Linux materialization command missing markers: \(commandScript)\n\(text)"
            )
        }
    }

	private func validateGeneratedAlpineRuntime(
		installer: OrlixOCIEnvironmentInstaller
	) throws -> String {
        let terminal = OrlixTerminalSession()
        let recorder = OrlixRuntimeProofOutputRecorder()
        let output = terminal.attachOutput { [recorder] data in
            recorder.append(data)
        }
        defer { output.cancel() }
        let run = try installer.run(
            id: Self.environmentID,
            command: [
                "/bin/sh",
                "-c",
                Self.generatedAlpineRuntimeScript,
            ],
            terminal: terminal,
            observationTimeout: Self.timeout
        )
        guard run.startedStateReport.status == .running,
            run.startedStateReport.pid != nil
        else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected generated Alpine runtime started state"
            )
        }
        guard run.completedStateReport.status == .stopped,
            run.completedStateReport.exitStatus == 0
        else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected generated Alpine runtime stopped exit 0\n\(recorder.text)"
            )
        }
        let text = recorder.text
		try Self.validateGeneratedAlpineRuntimeText(text)
		return text
	}

	private static func persistentLinuxMaterializationRoot() throws -> URL {
		let supportRoot = try FileManager.default.url(
			for: .applicationSupportDirectory,
			in: .userDomainMask,
			appropriateFor: nil,
			create: true
		)
		return supportRoot.appendingPathComponent(
			"OrlixRuntimeProofs/alpine-linux-materialization",
			isDirectory: true
		)
	}

	static func validatePreparedLinuxMaterializedAlpine() throws -> String {
		let copiedRoot = try persistentLinuxMaterializationRoot()
		let fixture = OrlixRuntimeEnvironmentFixture(root: copiedRoot)
		let registry = OrlixEnvironmentRegistry(
			linuxStateRoot: fixture.linuxStateRoot,
			cacheRoot: fixture.cacheRoot,
			scratchRoot: fixture.scratchRoot
		)
		let installer = OrlixOCIEnvironmentInstaller(registry: registry)
		let proof = OrlixOCIDerivedLiveRegistryAlpineRootfsImportProof()
		let generatedRootRuntimeMarkers = try proof
			.validateGeneratedAlpineRuntime(installer: installer)
		let deletedEnvironment = try installer.delete(id: environmentID)
		guard deletedEnvironment.lifecycleState == .deleted else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected Linux materialized Alpine delete cleanup"
			)
		}
		let layout = try OrlixEnvironmentStorageLayout.layout(
			forEnvironmentID: environmentID,
			linuxStateRoot: fixture.linuxStateRoot,
			cacheRoot: fixture.cacheRoot,
			scratchRoot: fixture.scratchRoot
		)
		guard !FileManager.default.fileExists(atPath: layout.rootDirectory.path)
		else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected Linux materialized Alpine environment cleanup"
			)
		}
		try? FileManager.default.removeItem(at: copiedRoot)
		return generatedRootRuntimeMarkers
			+ "ORLIX_OCI_ALPINE_LINUX_MATERIALIZATION_GENERATED_ROOT_OK\n"
			+ "ORLIX_OCI_ALPINE_LINUX_MATERIALIZATION_DELETE_OK\n"
	}

	private static var generatedAlpineRuntimeScript: String {
        [
            "printf '%s\\n' ORLIX_ALPINE_GENERATED_ROOT_BEGIN",
            "test -x /sbin/apk",
            "/sbin/apk --help >/tmp/orlix-apk-help.txt 2>&1",
            "printf '%s\\n' ORLIX_ALPINE_GENERATED_ROOT_APK_HELP_OK",
            "test -r /lib/apk/db/installed",
            "grep 'P:busybox' /lib/apk/db/installed >/dev/null",
            "printf '%s\\n' ORLIX_ALPINE_GENERATED_ROOT_PACKAGE_DB_OK",
            "printf '%s\\n' ORLIX_ALPINE_GENERATED_ROOT_DONE",
        ].joined(separator: "\n")
    }

    private static func validateGeneratedAlpineRuntimeText(_ text: String)
        throws
    {
        for marker in [
            "ORLIX_ALPINE_GENERATED_ROOT_BEGIN",
            "ORLIX_ALPINE_GENERATED_ROOT_APK_HELP_OK",
            "ORLIX_ALPINE_GENERATED_ROOT_PACKAGE_DB_OK",
            "ORLIX_ALPINE_GENERATED_ROOT_DONE",
        ] where !text.contains(marker) {
            throw OrlixOCIDerivedStdioRuntimeProofError.missingMarker(
                marker,
                text
            )
        }
    }

    private func linuxMaterializationPath(
        _ value: String,
        hostRoot: URL,
        mountPath: String
    ) -> String {
        let rootPath = hostRoot.path
        if value == rootPath {
            return mountPath
        }
        let prefix = rootPath + "/"
        if value.hasPrefix(prefix) {
            let suffix = value.dropFirst(prefix.count)
            return mountPath + "/" + suffix
        }
		return value
	}

	private func linuxMaterializationExecutable(_ executable: String) -> String {
		switch URL(fileURLWithPath: executable).lastPathComponent {
		case "orlix-truncate":
			return "truncate"
		case "orlix-mke2fs":
			return "mke2fs"
		case "orlix-debugfs":
			return "debugfs"
		default:
			return executable
		}
	}

	private func runLinuxScript(
		scriptURL: URL,
		hostRoot: URL,
		mountPath: String,
		terminalIdentifier: String,
		doneMarker: String
	) throws -> String {
		guard let profile = OrlixOSDistribution.bundledBootProfile else {
			throw OrlixUpstreamTestRunError.missingBundledBootProfile
		}
		guard let rootImageIdentifier = OrlixOSDistribution
			.productRootImageIdentifier
		else {
			throw OrlixUpstreamTestRunError.missingRootImageDescriptor("product")
		}

		let linuxScriptPath = mountPath + "/" + scriptURL.lastPathComponent
		let kernelCommandLine = [
			OrlixEnvironmentRootImage.defaultKernelCommandLine,
			"orlix.mount.host0.target=\(mountPath)",
			"orlix.exec=\(kernelToken("/bin/sh"))",
			"orlix.argv0=\(kernelToken("/bin/sh"))",
			"orlix.argv1=\(kernelToken(linuxScriptPath))",
			"orlix.env0=\(kernelToken("PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"))",
			"orlix.cwd=\(kernelToken("/"))",
			"orlix.uid=0",
			"orlix.gid=0",
		].joined(separator: " ")
		let terminal = OrlixTerminalSession()
		let recorder = TerminalOutputRecorder()
		let completion = DispatchSemaphore(value: 0)
		let bootStatus = BootStatusRecorder()
		let output = terminal.attachOutput { data in
			recorder.append(data)
			if recorder.text.contains(doneMarker)
				|| recorder.text.contains("orlix-init: shell exit status=")
			{
				completion.signal()
			}
		}
		defer { output.cancel() }

		let session = OrlixLinuxSession(
			bootConfig: OrlixBootConfig(
				profile: profile,
				kernelCommandLine: kernelCommandLine,
				rootImageIdentifier: rootImageIdentifier,
				terminalIdentifier: terminalIdentifier
			),
			hostDirectories: [
				OrlixHostDirectoryRegistration(
					identifier: OrlixEnvironmentRootImage
						.defaultHostDirectoryIdentifier,
					hostPath: hostRoot.path,
					readOnly: false
				)
			],
			terminal: terminal
		)
		terminal.resize(rows: 24, columns: 80)
		DispatchQueue.global(qos: .userInitiated).async {
			let status = session.boot()
			bootStatus.set(status)
			if status != .ok {
				completion.signal()
			}
		}
		guard completion.wait(timeout: .now() + Self.timeout) == .success
		else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"Linux script timed out: \(scriptURL.lastPathComponent)\n\(recorder.text)"
			)
		}
		if let status = bootStatus.value, status != .ok {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"Linux script boot failed: \(status)\n\(recorder.text)"
			)
		}
		return recorder.text
	}

	private func runLinuxMaterializationProbe(
		rootfs: URL,
		hostRoot: URL,
		mountPath: String
	) throws -> String {
		let rootfsPath = linuxMaterializationPath(
			rootfs.path,
			hostRoot: hostRoot,
			mountPath: mountPath
		)
		let probeRoot = "\(mountPath)/scratch/alpine-mke2fs-probe"
		let hostProbeRoot = hostRoot.appendingPathComponent(
			"scratch/alpine-mke2fs-probe",
			isDirectory: true
		)
		let hostSyntheticRoot = hostProbeRoot.appendingPathComponent(
			"synthetic",
			isDirectory: true
		)
		let hostImagesRoot = hostProbeRoot.appendingPathComponent(
			"images",
			isDirectory: true
		)
		if fileManager.fileExists(atPath: hostProbeRoot.path) {
			try fileManager.removeItem(at: hostProbeRoot)
		}
		try fileManager.createDirectory(
			at: hostSyntheticRoot,
			withIntermediateDirectories: true
		)
		try fileManager.createDirectory(
			at: hostImagesRoot,
			withIntermediateDirectories: true
		)
		try Data("synthetic\n".utf8).write(
			to: hostSyntheticRoot.appendingPathComponent("file")
		)
		for index in 0..<120 {
			let link = hostSyntheticRoot.appendingPathComponent("link-\(index)")
			try fileManager.createSymbolicLink(
				atPath: link.path,
				withDestinationPath: "file"
			)
		}
		let scriptURL = hostRoot.appendingPathComponent(
			".orlix-materialize-probe.sh",
			isDirectory: false
		)
		let script = [
			"set +e",
			"printf '%s\\n' ORLIX_ALPINE_MKE2FS_PROBE_BEGIN",
			"probe_root=\(shellQuote(probeRoot))",
			"src_root=\(shellQuote(rootfsPath))",
			"run_probe() {",
			"  name=\"$1\"",
			"  src=\"$2\"",
			"  size=\"$3\"",
			"  img=\"$probe_root/images/$name.ext4\"",
			"  printf 'ORLIX_ALPINE_MKE2FS_PROBE_CASE_BEGIN %s\\n' \"$name\"",
			"  truncate -s \"$size\" \"$img\"",
			"  truncate_status=$?",
			"  printf 'ORLIX_ALPINE_MKE2FS_PROBE_TRUNCATE_STATUS %s %d\\n' \"$name\" \"$truncate_status\"",
			"  if [ \"$truncate_status\" -ne 0 ]; then return 0; fi",
			"  mke2fs -q -t ext4 -F -m 0 -O ^metadata_csum -U clear -L PROBE -E root_owner=0:0,no_copy_xattrs -d \"$src\" \"$img\"",
			"  status=$?",
			"  printf 'ORLIX_ALPINE_MKE2FS_PROBE_MKE2FS_STATUS %s %d\\n' \"$name\" \"$status\"",
			"  return 0",
			"}",
			"run_subtree_probe() {",
			" rel=\"$1\"",
			" size=\"$2\"",
			" src=\"$src_root/$rel\"",
			" if [ -d \"$src\" ]; then run_probe \"$(printf '%s' \"$rel\" | tr / _)\" \"$src\" \"$size\"; fi",
			"}",
			"run_probe synthetic \"$probe_root/synthetic\" 8m",
			"run_subtree_probe etc/apk 8m",
			"run_subtree_probe etc/conf.d 8m",
			"run_subtree_probe etc/init.d 8m",
			"run_subtree_probe etc/network 8m",
			"run_subtree_probe etc/periodic 8m",
			"run_subtree_probe etc/profile.d 8m",
			"run_subtree_probe etc/ssl 16m",
			"run_probe etc \"$src_root/etc\" 16m",
			"run_probe bin \"$src_root/bin\" 16m",
			"run_probe sbin \"$src_root/sbin\" 16m",
			"run_subtree_probe usr/bin 16m",
			"run_subtree_probe usr/lib 16m",
			"run_subtree_probe usr/sbin 16m",
			"run_subtree_probe usr/share 16m",
			"run_probe usr \"$src_root/usr\" 32m",
			"run_probe full \"$src_root\" 64m",
			"printf '%s\\n' ORLIX_ALPINE_MKE2FS_PROBE_DONE",
		].joined(separator: "\n") + "\n"
		try script.write(to: scriptURL, atomically: true, encoding: .utf8)

		let text = try runLinuxScript(
			scriptURL: scriptURL,
			hostRoot: hostRoot,
			mountPath: mountPath,
			terminalIdentifier: "orlix.oci.linux.materialization.probe",
			doneMarker: "ORLIX_ALPINE_MKE2FS_PROBE_DONE"
		)
		guard text.contains("ORLIX_ALPINE_MKE2FS_PROBE_BEGIN"),
			text.contains("ORLIX_ALPINE_MKE2FS_PROBE_DONE")
		else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"Linux materialization probe missing markers\n\(text)"
			)
		}
		return text + "\n"
	}

	private func shellQuote(_ value: String) -> String {
        "'\(value.replacingOccurrences(of: "'", with: "'\\''"))'"
    }

    private func kernelToken(_ value: String) -> String {
        var encoded = ""
        for byte in value.utf8 {
            if Self.isKernelCommandLineTokenByte(byte) {
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
            UInt8(ascii: "="):
            return true
        default:
            return false
        }
    }

    private func validateMaterializationCommands(
        _ commands: [MaterializationCommandObservation],
        rootfs: URL,
        layout: OrlixEnvironmentStorageLayout
    ) throws {
        guard commands.count == 6 else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected 6 Alpine materialization commands, got \(commands.count)"
            )
        }
        try validateTruncateCommand(
            commands[0],
            size: "64m",
            imageURL: layout.baseImageURL
        )
        try validateMke2fsCommand(
            commands[1],
            sourceTree: rootfs,
            imageURL: layout.baseImageURL,
            label: "ORLIXROOT"
        )
        try validateDebugfsCommand(commands[2], imageURL: layout.baseImageURL)
        try validateTruncateCommand(
            commands[3],
            size: "32m",
            imageURL: layout.stateImageURL
        )
        try validateMke2fsCommand(
            commands[4],
            sourceTree: layout.importScratchDirectory
                .appendingPathComponent("state-tree", isDirectory: true),
            imageURL: layout.stateImageURL,
            label: "ORLIXSTATE"
        )
        try validateDebugfsCommand(commands[5], imageURL: layout.stateImageURL)
    }

    private func validateTruncateCommand(
        _ command: MaterializationCommandObservation,
        size: String,
        imageURL: URL
    ) throws {
        guard command.executable == "/usr/bin/orlix-truncate",
            command.arguments == ["-s", size, imageURL.path]
        else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "unexpected Alpine truncate command \(command)"
            )
        }
    }

    private func validateMke2fsCommand(
        _ command: MaterializationCommandObservation,
        sourceTree: URL,
        imageURL: URL,
        label: String
    ) throws {
        guard command.executable == "/usr/bin/orlix-mke2fs",
            command.arguments == [
                "-q",
                "-t",
                "ext4",
                "-F",
                "-m",
                "0",
                "-O",
                "^metadata_csum",
                "-U",
                "clear",
                "-L",
                label,
                "-E",
                "root_owner=0:0,no_copy_xattrs",
                "-d",
                sourceTree.path,
                imageURL.path,
            ]
        else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "unexpected Alpine mke2fs command \(command)"
            )
        }
    }

    private func validateDebugfsCommand(
        _ command: MaterializationCommandObservation,
        imageURL: URL
    ) throws {
        guard command.executable == "/usr/bin/orlix-debugfs",
            command.arguments.count == 4,
            command.arguments[0] == "-w",
            command.arguments[1] == "-f",
            command.arguments[3] == imageURL.path
        else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "unexpected Alpine debugfs command \(command)"
            )
        }
    }
}

private final class OrlixOCIDerivedLiveRegistryTerminalProof: @unchecked Sendable {
	private static let timeout: TimeInterval = 600
	private static let fixtureEnvironmentID = "oci-imported-runtime-test-fixture"
	private static let defaultTerminalEnvironmentID =
		"oci-live-registry-terminal-test-fixture"
	private static let defaultLiveImageReference = "registry.k8s.io/pause:3.10"
	private static let defaultRequiredMarkers = [
		"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_BEGIN",
		"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_PTY_OK",
		"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_DONE",
	]
	static let interactiveRequiredMarkers = [
		"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_BEGIN",
		"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_PTY_OK",
		"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_INPUT_READY",
		"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_INPUT_OK",
		"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_DONE",
	]
static let terminalSizeRequiredMarkers = [
"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_BEGIN",
"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_PTY_OK",
"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_SIZE_OK",
"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_DONE",
]
static let dynamicResizeRequiredMarkers = [
"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_BEGIN",
"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_PTY_OK",
"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_RESIZE_READY",
"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_RESIZE_OK",
"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_DONE",
]
static let sigwinchRequiredMarkers = [
"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_BEGIN",
"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_PTY_OK",
"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_SIGWINCH_READY",
"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_SIGWINCH_SEEN",
"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_SIGWINCH_OK",
"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_DONE",
]
static let sigtstpRequiredMarkers = [
"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_BEGIN",
"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_PTY_OK",
"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_SIGTSTP_READY",
"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_SIGTSTP_TRAP",
"ORLIX_ENV_LIVE_REGISTRY_TERMINAL_DONE",
]

	private let fileManager = FileManager.default
	private let recorder = OrlixRuntimeProofOutputRecorder()
	private let liveImageReference: String
	private let terminalEnvironmentID: String
	private let executionScript: String
	private let requiredMarkers: [String]
private let inputAfterMarker: String?
private let input: Data?
private let resizeAfterMarker: String?
private let resizeRows: UInt32?
private let resizeColumns: UInt32?
private let inputAfterResize: Data?
private let terminalRows: UInt32?
private let terminalColumns: UInt32?
private let readOnlyRoot: Bool
private let expectedExitStatus: Int32
private let successMarker: String?

	init(
		liveImageReference: String = defaultLiveImageReference,
		terminalEnvironmentID: String = defaultTerminalEnvironmentID,
		executionScript: String = defaultExecutionScript,
		requiredMarkers: [String] = defaultRequiredMarkers,
inputAfterMarker: String? = nil,
input: Data? = nil,
resizeAfterMarker: String? = nil,
resizeRows: UInt32? = nil,
resizeColumns: UInt32? = nil,
inputAfterResize: Data? = nil,
terminalRows: UInt32? = nil,
terminalColumns: UInt32? = nil,
readOnlyRoot: Bool = false,
expectedExitStatus: Int32 = 0,
successMarker: String? = nil
	) {
		self.liveImageReference = liveImageReference
		self.terminalEnvironmentID = terminalEnvironmentID
		self.executionScript = executionScript
		self.requiredMarkers = requiredMarkers
self.inputAfterMarker = inputAfterMarker
self.input = input
self.resizeAfterMarker = resizeAfterMarker
self.resizeRows = resizeRows
self.resizeColumns = resizeColumns
self.inputAfterResize = inputAfterResize
self.terminalRows = terminalRows
self.terminalColumns = terminalColumns
self.readOnlyRoot = readOnlyRoot
self.expectedExitStatus = expectedExitStatus
self.successMarker = successMarker
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
		guard completion.wait(timeout: .now() + .seconds(Int(Self.timeout))) == .success
		else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"live registry terminal proof timed out\n\(Self.normalized(recorder.text))"
			)
		}
		guard let result = resultBox.value else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"live registry terminal proof completed without result"
			)
		}
		return try result.get()
	}

	private func runAsync() async throws -> String {
		let sourceRoot = OrlixAppLaunchRuntimeRunner.fixtureRoot()
		let ready = sourceRoot.appendingPathComponent(".ready", isDirectory: false)
		guard fileManager.fileExists(atPath: ready.path) else {
			throw OrlixOCIDerivedStdioRuntimeProofError.missingFixture(ready.path)
		}

		let copiedRoot = FileManager.default.temporaryDirectory
			.appendingPathComponent(
				"orlix-oci-live-registry-terminal-\(UUID().uuidString)",
				isDirectory: true
			)
		try fileManager.copyItem(at: sourceRoot, to: copiedRoot)
		defer { try? fileManager.removeItem(at: copiedRoot) }

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
		let terminalLayout = try OrlixEnvironmentStorageLayout.layout(
			forEnvironmentID: terminalEnvironmentID,
			linuxStateRoot: fixture.linuxStateRoot,
			cacheRoot: fixture.cacheRoot,
			scratchRoot: fixture.scratchRoot
		)
		let terminal = OrlixTerminalSession()
		let output = terminal.attachOutput { [recorder] data in
			recorder.append(data)
		}
		defer { output.cancel() }
if let inputAfterMarker, let input {
send(input, to: terminal, after: inputAfterMarker)
}
if let resizeAfterMarker, let resizeRows, let resizeColumns {
resize(
terminal,
rows: resizeRows,
columns: resizeColumns,
after: resizeAfterMarker,
thenSend: inputAfterResize
)
}

let installer = OrlixOCIEnvironmentInstaller(registry: registry)
		let driver = OrlixOCIRuntimeLinuxSessionObservationDriver(timeout: Self.timeout)
		var runArguments = [
			"orlix", "run",
			"--id", terminalEnvironmentID,
			"--tty",
		]
		if let terminalRows, let terminalColumns {
			runArguments.append(contentsOf: [
				"--terminal-size", "\(terminalRows)x\(terminalColumns)",
			])
		}
		if readOnlyRoot {
			runArguments.append("--read-only")
		}
		runArguments.append(contentsOf: [
			"--user", "0:0",
			"--rm",
			liveImageReference,
			"--", "/bin/sh", "-c", executionScript,
		])
		let result = try await installer.run(
			arguments: runArguments,
			tools: OrlixOCIEnvironmentMaterializationTools(
				mke2fs: URL(fileURLWithPath: "/usr/bin/orlix-mke2fs"),
				truncate: URL(fileURLWithPath: "/usr/bin/orlix-truncate"),
				debugfs: URL(fileURLWithPath: "/usr/bin/orlix-debugfs"),
                                e2fsck: URL(fileURLWithPath: "/usr/bin/orlix-e2fsck")
			),
			puller: OrlixOCIRegistryPuller(),
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
		try validateText(text)
		try Self.validateLifecycle(
			result: result,
expectedEnvironmentID: terminalEnvironmentID,
lifecycleRecordURL: try OrlixOCIRuntime(registry: registry)
.lifecycleStore.recordURL(forID: terminalEnvironmentID),
environmentDirectoryURL: terminalLayout.rootDirectory,
expectedExitStatus: expectedExitStatus
		)
		text += "\nORLIX_OCI_LIVE_REGISTRY_TERMINAL_PULL_OK\n"
		text += "ORLIX_OCI_LIVE_REGISTRY_TERMINAL_STARTED_OK\n"
		text += "ORLIX_OCI_LIVE_REGISTRY_TERMINAL_STOPPED_OK\n"
		text += "ORLIX_OCI_LIVE_REGISTRY_TERMINAL_DELETE_OK\n"
		if let successMarker {
			text += "\(successMarker)\n"
		}
		return text
	}

	private static var defaultExecutionScript: String {
		[
			"printf '%s%s\\n' ORLIX_ENV_LIVE_REGISTRY_ TERMINAL_BEGIN",
			"if /bin/test -t 0 && /bin/test -t 1 && /bin/test -t 2; then printf '%s%s\\n' ORLIX_ENV_LIVE_REGISTRY_TERMINAL_ PTY_OK; else printf '%s%s\\n' ORLIX_ENV_LIVE_REGISTRY_TERMINAL_ NOT_PTY; exit 42; fi",
			"printf '%s%s\\n' ORLIX_ENV_LIVE_REGISTRY_TERMINAL_ DONE",
		].joined(separator: "\n")
	}

	static var interactiveExecutionScript: String {
		[
			"printf '%s%s\\n' ORLIX_ENV_LIVE_REGISTRY_ TERMINAL_BEGIN",
			"if /bin/test -t 0 && /bin/test -t 1 && /bin/test -t 2; then printf '%s%s\\n' ORLIX_ENV_LIVE_REGISTRY_TERMINAL_ PTY_OK; else printf '%s%s\\n' ORLIX_ENV_LIVE_REGISTRY_TERMINAL_ NOT_PTY; exit 42; fi",
			"printf '%s%s\\n' ORLIX_ENV_LIVE_REGISTRY_TERMINAL_ INPUT_READY",
			"IFS= read -r orlix_terminal_input",
			"if /bin/test \"$orlix_terminal_input\" = orlix-interactive-alpine; then printf '%s%s\\n' ORLIX_ENV_LIVE_REGISTRY_TERMINAL_ INPUT_OK; else printf 'ORLIX_ENV_LIVE_REGISTRY_TERMINAL_INPUT_BAD=%s\\n' \"$orlix_terminal_input\"; exit 43; fi",
			"printf '%s%s\\n' ORLIX_ENV_LIVE_REGISTRY_TERMINAL_ DONE",
		].joined(separator: "\n")
	}

	static var terminalSizeExecutionScript: String {
		[
			"m=ORLIX_ENV_LIVE_REGISTRY_TERMINAL_",
			"printf '%s\\n' ${m}BEGIN",
			"if /bin/test -t 0 && /bin/test -t 1 && /bin/test -t 2; then printf '%s\\n' ${m}PTY_OK; else printf '%s\\n' ${m}NOT_PTY; exit 42; fi",
			"s=\"$(stty -a 2>&1 || true)\"",
			"case \"$s\" in *'rows 37'*'columns 132'*|*'columns 132'*'rows 37'*) printf '%s\\n' ${m}SIZE_OK;; *) printf '%s\\n' ${m}SIZE_BAD; exit 44;; esac",
			"printf '%s\\n' ${m}DONE",
		].joined(separator: "\n")
	}

static var dynamicResizeExecutionScript: String {
[
"m=ORLIX_ENV_LIVE_REGISTRY_TERMINAL_",
"printf '%s\\n' ${m}BEGIN",
"if /bin/test -t 0 && /bin/test -t 1 && /bin/test -t 2; then printf '%s\\n' ${m}PTY_OK; else printf '%s\\n' ${m}NOT_PTY; exit 42; fi",
"printf '%s\\n' ${m}RESIZE_READY",
"IFS= read -r _orlix_resize_gate",
"s=\"$(stty -a 2>&1 || true)\"",
"case \"$s\" in *'rows 41'*'columns 117'*|*'columns 117'*'rows 41'*) printf '%s\\n' ${m}RESIZE_OK;; *) printf 'ORLIX_ENV_LIVE_REGISTRY_TERMINAL_RESIZE_BAD=%s\\n' \"$s\"; exit 45;; esac",
"printf '%s\\n' ${m}DONE",
].joined(separator: "\n")
}

static var sigwinchExecutionScript: String {
[
"m=ORLIX_ENV_LIVE_REGISTRY_TERMINAL_",
"printf '%s\\n' ${m}BEGIN",
"if /bin/test -t 0 && /bin/test -t 1 && /bin/test -t 2; then printf '%s\\n' ${m}PTY_OK; else printf '%s\\n' ${m}NOT_PTY; exit 42; fi",
"winch_seen=0",
"trap 'winch_seen=1; printf \"%s\\n\" ${m}SIGWINCH_SEEN' WINCH",
"printf '%s\\n' ${m}SIGWINCH_READY",
"IFS= read -r _orlix_sigwinch_gate",
"if /bin/test \"$winch_seen\" = 1; then printf '%s\\n' ${m}SIGWINCH_OK; else printf '%s\\n' ${m}SIGWINCH_MISSING; exit 46; fi",
"printf '%s\\n' ${m}DONE",
].joined(separator: "\n")
}

static var sigtstpExecutionScript: String {
[
"m=ORLIX_ENV_LIVE_REGISTRY_TERMINAL_",
"printf '%s\\n' ${m}BEGIN",
"if /bin/test -t 0 && /bin/test -t 1 && /bin/test -t 2; then printf '%s\\n' ${m}PTY_OK; else printf '%s\\n' ${m}NOT_PTY; exit 42; fi",
"trap 'printf \"%s\\n\" ${m}SIGTSTP_TRAP; printf \"%s\\n\" ${m}DONE; exit 148' TSTP",
"printf '%s\\n' ${m}SIGTSTP_READY",
"while :; do sleep 1; done",
].joined(separator: "\n")
}

private func send(
		_ input: Data,
		to terminal: OrlixTerminalSession,
		after marker: String
	) {
		DispatchQueue.global(qos: .userInitiated).async { [recorder] in
			let deadline = Date().addingTimeInterval(Self.timeout)
			while Date() < deadline {
				if Self.normalized(recorder.text).contains(marker) {
					terminal.send(input)
					return
				}
				Thread.sleep(forTimeInterval: 0.05)
			}
		}
}

private func resize(
_ terminal: OrlixTerminalSession,
rows: UInt32,
columns: UInt32,
after marker: String,
thenSend input: Data? = nil
) {
DispatchQueue.global(qos: .userInitiated).async { [recorder] in
let deadline = Date().addingTimeInterval(Self.timeout)
while Date() < deadline {
if Self.normalized(recorder.text).contains(marker) {
terminal.resize(rows: rows, columns: columns)
if let input {
Thread.sleep(forTimeInterval: 0.1)
terminal.send(input)
}
return
}
Thread.sleep(forTimeInterval: 0.05)
}
}
}

private func validateText(_ text: String) throws {
		for marker in requiredMarkers where !text.contains(marker) {
			throw OrlixOCIDerivedStdioRuntimeProofError.missingMarker(marker, text)
		}
		if text.contains("ORLIX_ENV_LIVE_REGISTRY_TERMINAL_NOT_PTY")
			|| text.contains("ORLIX-APP-RUNTIME-RUNNER-ERROR")
		{
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(text)
		}
	}

private static func validateLifecycle(
result: OrlixOCIRegistryEnvironmentInstallRunResult,
expectedEnvironmentID: String,
lifecycleRecordURL: URL,
environmentDirectoryURL: URL,
expectedExitStatus: Int32
) throws {
guard result.installResult.id == expectedEnvironmentID else {
throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
"expected live registry terminal install id \(expectedEnvironmentID)"
			)
		}
		guard result.runResult.startedStateReport.status == .running,
			result.runResult.startedStateReport.pid != nil
		else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected live registry terminal started lifecycle state running"
			)
		}
guard result.runResult.completedStateReport.status == .stopped,
result.runResult.completedStateReport.exitStatus == expectedExitStatus
else {
throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
"expected live registry terminal completed lifecycle state stopped exit \(expectedExitStatus)"
)
}
		guard result.deleteResult?.lifecycleState == .deleted else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected live registry terminal delete cleanup"
			)
		}
		guard !FileManager.default.fileExists(atPath: lifecycleRecordURL.path) else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected live registry terminal lifecycle record cleanup"
			)
		}
		guard !FileManager.default.fileExists(atPath: environmentDirectoryURL.path) else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected live registry terminal environment directory cleanup"
			)
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
				"missing fixture materialization target \(executable.path)"
			)
		}
		let targetURL = URL(fileURLWithPath: targetPath)
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
				"unexpected fixture materialization command \(executable.path)"
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

private final class OrlixOCIDerivedVirtioNetRuntimeProof: @unchecked Sendable {
	private static let timeout: TimeInterval = 120
	private static let environmentID = "oci-imported-runtime-test-fixture"
	private static let rootImageIdentifier =
		"orlix.test.environment.oci-runtime-test-fixture"
	private static let requiredMarkers = [
		"1..14",
        "ok 1 - virtio-net device is present on the upstream virtio bus",
        "ok 2 - virtio-net device owns a Linux netdev",
		"ok 3 - virtio-net netdev is exposed through sysfs",
		"ok 4 - virtio-net netdev reports Ethernet hardware type",
		"ok 5 - virtio-net netdev reports Ethernet address length",
        "ok 6 - virtio-net netdev reports a positive MTU through sysfs",
        "ok 7 - rtnetlink enumerates the virtio-net Ethernet link with matching MTU and standard operstate",
		"ok 8 - ioctl reports matching virtio-net link flags",
        "ok 9 - AF_PACKET socket binds to the virtio-net link",
		"ok 10 - virtio-net reports carrier after Linux interface up",
		"ok 11 - AF_PACKET send advances virtio-net tx_packets",
		"ok 12 - virtio-net RX queue advances rx_packets",
        "ok 13 - virtio-net link is distinct from loopback",
        "ok 14 - procfs reports the virtio-net interface",
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
			.appendingPathComponent(
				"orlix-oci-virtio-net-\(UUID().uuidString)",
				isDirectory: true
			)
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
			defaultCommand: ["/orlix/virtio_net_device_probe"],
			defaultEnvironment: [
				"HOME": "/root",
				"PATH": "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
				"TERM": "xterm-256color",
			],
			defaultWorkingDirectory: "/",
			defaultUserID: 0,
			defaultGroupID: 0,
			hostname: "oci-virtio-net-host",
			domainname: "oci.example",
			rootMount: .defaultOverlay
		)
		let registry = OrlixEnvironmentRegistry(
			linuxStateRoot: fixture.linuxStateRoot,
			cacheRoot: fixture.cacheRoot,
			scratchRoot: fixture.scratchRoot
		)
		try registry.save(descriptor)
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
			script: "/orlix/virtio_net_device_probe",
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
		text += "\nORLIX_OCI_VIRTIO_NET_RUNTIME_STARTED_OK\n"
		text += "ORLIX_OCI_VIRTIO_NET_RUNTIME_STOPPED_OK\n"
		text += "ORLIX_OCI_VIRTIO_NET_RUNTIME_DELETE_OK\n"
		return text
	}

	private static func validateText(_ text: String) throws {
		for marker in requiredMarkers where !text.contains(marker) {
			throw OrlixOCIDerivedStdioRuntimeProofError.missingMarker(marker, text)
		}
		if text.contains("\nnot ok ") ||
			text.contains("ORLIX-APP-RUNTIME-RUNNER-ERROR")
		{
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
			"OCI virtio-net runtime markers",
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
			run.startedStateReport.pid != nil
		else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI virtio-net proof started lifecycle state running"
			)
		}
		guard run.completedStateReport.status == .stopped,
			run.completedStateReport.exitStatus == 0
		else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI virtio-net proof stopped exit 0"
			)
		}
		guard finalState.status == .stopped, finalState.exitStatus == 0 else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI virtio-net final lifecycle state stopped exit 0"
			)
		}
		guard deletedEnvironment.id == Self.environmentID,
			deletedEnvironment.lifecycleState == .deleted
		else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI virtio-net proof delete cleanup"
			)
		}
		guard !FileManager.default.fileExists(atPath: lifecycleRecordURL.path) else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI virtio-net lifecycle record cleanup"
			)
		}
		guard !FileManager.default.fileExists(atPath: environmentDirectoryURL.path)
		else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI virtio-net environment directory cleanup"
			)
		}
	}

	private static func normalized(_ text: String) -> String {
		text.replacingOccurrences(of: "\r\n", with: "\n")
			.replacingOccurrences(of: "\r", with: "\n")
	}
}

private final class OrlixOCIDerivedDeviceNodesRuntimeProof: @unchecked Sendable {
    private static let timeout: TimeInterval = 120
    private static let environmentID = "oci-imported-runtime-test-fixture"
    private static let rootImageIdentifier =
        "orlix.test.environment.oci-runtime-test-fixture"
    private static let requiredMarkers = [
        "1..4",
        "ok 1 - OCI linux.devices creates configured character device node",
        "ok 2 - OCI character device node uses Linux device behavior",
        "ok 3 - OCI linux.devices creates configured fifo device node",
        "ok 4 - OCI linux.devices creates configured block device node",
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
            .appendingPathComponent("orlix-oci-devices-\(UUID().uuidString)", isDirectory: true)
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
            defaultCommand: ["/orlix/oci_device_nodes_probe"],
            defaultEnvironment: [
                "HOME": "/root",
                "PATH": "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
                "TERM": "xterm-256color",
            ],
            defaultWorkingDirectory: "/",
            defaultUserID: 0,
            defaultGroupID: 0,
            hostname: "oci-devices-host",
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
            environmentDirectoryURL: environmentDirectoryURL,
            output: text
        )
        text += "\nORLIX_OCI_DEVICE_NODES_RUNTIME_STARTED_OK\n"
        text += "ORLIX_OCI_DEVICE_NODES_RUNTIME_STOPPED_OK\n"
        text += "ORLIX_OCI_DEVICE_NODES_RUNTIME_DELETE_OK\n"
        return text
    }

    private static func writeOCIRuntimeConfig(rootPath: String, to bundleRoot: URL) throws {
        let document: [String: Any] = [
            "ociVersion": "1.1.0",
            "process": [
                "terminal": false,
                "args": ["/orlix/oci_device_nodes_probe"],
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
            ],
            "root": [
                "path": rootPath,
            ],
            "hostname": "oci-devices-host",
            "domainname": "oci.example",
            "linux": [
                "devices": [
                    [
                        "path": "/dev/orlix-oci-null",
                        "type": "c",
                        "major": 1,
                        "minor": 3,
                        "fileMode": 0o666,
                        "uid": 0,
                        "gid": 0,
                    ],
                    [
                        "path": "/dev/orlix-oci-pipe",
                        "type": "p",
                        "fileMode": 0o644,
                        "uid": 0,
                        "gid": 0,
                    ],
                    [
                        "path": "/dev/orlix-oci-block",
                        "type": "b",
                        "major": 7,
                        "minor": 0,
                        "fileMode": 0o600,
                        "uid": 0,
                        "gid": 0,
                    ],
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
            "OCI device node runtime markers",
            text
        )
    }

    private static func validateLifecycle(
        run: OrlixOCIEnvironmentRunResult,
        finalState: OrlixOCIRuntimeStateReport,
        deletedEnvironment: OrlixOCIEnvironmentDeleteResult,
        lifecycleRecordURL: URL,
        environmentDirectoryURL: URL,
        output: String
    ) throws {
        guard run.startedStateReport.status == .running,
              run.startedStateReport.pid != nil else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI device node proof started lifecycle state running\n\(output)"
            )
        }
        guard run.completedStateReport.status == .stopped,
              run.completedStateReport.exitStatus == 0 else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI device node proof stopped exit 0\n\(output)"
            )
        }
        guard finalState.status == .stopped,
              finalState.exitStatus == 0 else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI device node final lifecycle state stopped exit 0\n\(output)"
            )
        }
        guard deletedEnvironment.id == Self.environmentID,
              deletedEnvironment.lifecycleState == .deleted else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI device node proof delete cleanup\n\(output)"
            )
        }
        guard !FileManager.default.fileExists(atPath: lifecycleRecordURL.path) else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI device node lifecycle record cleanup\n\(output)"
            )
        }
        guard !FileManager.default.fileExists(atPath: environmentDirectoryURL.path) else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI device node environment directory cleanup\n\(output)"
            )
        }
    }

    private static func normalized(_ text: String) -> String {
        text.replacingOccurrences(of: "\r\n", with: "\n")
    }
}

private final class OrlixOCIDerivedProcessAttributesRuntimeProof: @unchecked Sendable {
	private static let timeout: TimeInterval = 120
	private static let environmentID = "oci-imported-runtime-test-fixture"
	private static let rootImageIdentifier =
		"orlix.test.environment.oci-runtime-test-fixture"
	private static let requiredMarkers = [
		"ORLIX-OCI-PROCESS-ATTRIBUTES-PROBE",
		"1..4",
		"ok 1 - OCI process rlimit is visible through getrlimit",
		"ok 2 - OCI process rlimit is enforced by Linux",
		"ok 3 - OCI noNewPrivileges is visible through prctl",
		"ok 4 - OCI process user umask controls created file mode",
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
			.appendingPathComponent(
				"orlix-oci-process-attributes-\(UUID().uuidString)",
				isDirectory: true
			)
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

		var text = Self.normalized(recorder.text)
		try Self.validateText(text)
		try Self.validateLifecycle(
			run: run,
			finalState: finalState,
			deletedEnvironment: deletedEnvironment,
			lifecycleRecordURL: lifecycleRecordURL,
			environmentDirectoryURL: environmentDirectoryURL,
			output: text
		)
		text += "\nORLIX_OCI_PROCESS_ATTRIBUTES_RUNTIME_STARTED_OK\n"
		text += "ORLIX_OCI_PROCESS_ATTRIBUTES_RUNTIME_STOPPED_OK\n"
		text += "ORLIX_OCI_PROCESS_ATTRIBUTES_RUNTIME_DELETE_OK\n"
		return text
	}

	private static func writeOCIRuntimeConfig(
		rootPath: String,
		to bundleRoot: URL
	) throws {
		let document: [String: Any] = [
			"ociVersion": "1.1.0",
			"process": [
				"terminal": false,
				"args": ["/orlix/oci_process_attributes_probe"],
				"env": [
					"HOME=/root",
					"PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
					"TERM=xterm-256color",
				],
				"cwd": "/tmp",
				"user": [
					"uid": 0,
					"gid": 0,
					"umask": 0o027,
				],
				"rlimits": [
					[
						"type": "RLIMIT_NOFILE",
						"soft": 32,
						"hard": 32,
					],
				],
				"noNewPrivileges": true,
			],
			"root": [
				"path": rootPath,
			],
			"hostname": "oci-process-attributes-host",
			"domainname": "oci.example",
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
		if text.contains("\nnot ok ") ||
			text.contains("ORLIX-APP-RUNTIME-RUNNER-ERROR")
		{
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(text)
		}
	}

	private static func validateLifecycle(
		run: OrlixOCIEnvironmentRunResult,
		finalState: OrlixOCIRuntimeStateReport,
		deletedEnvironment: OrlixOCIEnvironmentDeleteResult,
		lifecycleRecordURL: URL,
		environmentDirectoryURL: URL,
		output: String
	) throws {
		guard run.startedStateReport.status == .running,
			run.startedStateReport.pid != nil
		else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI process attribute proof started lifecycle state running\n\(output)"
			)
		}
		guard run.completedStateReport.status == .stopped,
			run.completedStateReport.exitStatus == 0
		else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI process attribute proof stopped exit 0\n\(output)"
			)
		}
		guard finalState.status == .stopped,
			finalState.exitStatus == 0
		else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI process attribute final lifecycle state stopped exit 0\n\(output)"
			)
		}
		guard deletedEnvironment.id == Self.environmentID,
			deletedEnvironment.lifecycleState == .deleted
		else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI process attribute proof delete cleanup\n\(output)"
			)
		}
		guard !FileManager.default.fileExists(atPath: lifecycleRecordURL.path) else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI process attribute lifecycle record cleanup\n\(output)"
			)
		}
		guard !FileManager.default.fileExists(atPath: environmentDirectoryURL.path) else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI process attribute environment directory cleanup\n\(output)"
			)
		}
	}

	private static func normalized(_ text: String) -> String {
		text.replacingOccurrences(of: "\r\n", with: "\n")
			.replacingOccurrences(of: "\r", with: "\n")
	}
}

private final class OrlixOCIDerivedNamespaceIdentityRuntimeProof: @unchecked Sendable {
    private static let timeout: TimeInterval = 120
    private static let environmentID = "oci-imported-runtime-test-fixture"
    private static let rootImageIdentifier =
        "orlix.test.environment.oci-runtime-test-fixture"
    private static let requiredMarkers = [
        "ORLIX-OCI-NAMESPACE-IDENTITY-PROBE",
        "1..6",
        "ok 1 - OCI hostname visible through gethostname",
        "ok 2 - OCI domainname visible through uname",
        "ok 3 - OCI UTS namespace exposes Linux procfs namespace entry",
        "ok 4 - OCI user namespace exposes Linux procfs namespace entry",
        "ok 5 - OCI uidMappings visible through Linux uid_map",
        "ok 6 - OCI gidMappings visible through Linux gid_map",
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
            .appendingPathComponent(
                "orlix-oci-namespace-identity-\(UUID().uuidString)",
                isDirectory: true
            )
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
        try Self.writeOCIRuntimeConfig(rootPath: "imported-root", to: fixture.root)

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
        try waitForRequiredMarkers()

        let finalState = try installer.state(id: Self.environmentID)
        let lifecycleRecordURL = try runtime.lifecycleStore.recordURL(
            forID: Self.environmentID
        )
        let environmentDirectoryURL = layout.rootDirectory
        let deletedEnvironment = try installer.delete(id: Self.environmentID)

        var text = Self.normalized(recorder.text)
        try Self.validateText(text)
        try Self.validateLifecycle(
            run: run,
            finalState: finalState,
            deletedEnvironment: deletedEnvironment,
            lifecycleRecordURL: lifecycleRecordURL,
            environmentDirectoryURL: environmentDirectoryURL,
            output: text
        )
        text += "\nORLIX_OCI_NAMESPACE_IDENTITY_RUNTIME_STARTED_OK\n"
        text += "ORLIX_OCI_NAMESPACE_IDENTITY_RUNTIME_STOPPED_OK\n"
        text += "ORLIX_OCI_NAMESPACE_IDENTITY_RUNTIME_DELETE_OK\n"
        return text
    }

    private static func writeOCIRuntimeConfig(
        rootPath: String,
        to bundleRoot: URL
    ) throws {
        let idMapping: [String: Any] = [
            "containerID": 0,
            "hostID": 0,
            "size": 1,
        ]
        let document: [String: Any] = [
            "ociVersion": "1.1.0",
            "process": [
                "terminal": false,
                "args": ["/orlix/oci_namespace_identity_probe"],
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
            ],
            "root": [
                "path": rootPath,
            ],
            "hostname": "oci-namespace-host",
            "domainname": "oci.example",
            "linux": [
                "namespaces": [
                    [
                        "type": "uts",
                    ],
                    [
                        "type": "user",
                    ],
                ],
                "uidMappings": [
                    idMapping,
                ],
                "gidMappings": [
                    idMapping,
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
            throw OrlixOCIDerivedStdioRuntimeProofError.missingMarker(
                marker,
                text
            )
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
            "OCI namespace identity runtime markers",
            text
        )
    }

    private static func validateLifecycle(
        run: OrlixOCIEnvironmentRunResult,
        finalState: OrlixOCIRuntimeStateReport,
        deletedEnvironment: OrlixOCIEnvironmentDeleteResult,
        lifecycleRecordURL: URL,
        environmentDirectoryURL: URL,
        output: String
    ) throws {
        guard run.startedStateReport.status == .running,
            run.startedStateReport.pid != nil
        else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI namespace identity proof started lifecycle state running\n\(output)"
            )
        }
        guard run.completedStateReport.status == .stopped,
            run.completedStateReport.exitStatus == 0
        else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI namespace identity proof stopped exit 0\n\(output)"
            )
        }
        guard finalState.status == .stopped, finalState.exitStatus == 0 else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI namespace identity final lifecycle state stopped exit 0\n\(output)"
            )
        }
        guard deletedEnvironment.id == Self.environmentID,
            deletedEnvironment.lifecycleState == .deleted
        else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI namespace identity proof delete cleanup\n\(output)"
            )
        }
        guard !FileManager.default.fileExists(atPath: lifecycleRecordURL.path) else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI namespace identity lifecycle record cleanup\n\(output)"
            )
        }
        guard !FileManager.default.fileExists(atPath: environmentDirectoryURL.path) else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI namespace identity environment directory cleanup\n\(output)"
            )
        }
    }

    private static func normalized(_ text: String) -> String {
        text.replacingOccurrences(of: "\r\n", with: "\n")
            .replacingOccurrences(of: "\r", with: "\n")
    }
}

private final class OrlixOCIDerivedTimeNamespaceRuntimeProof: @unchecked Sendable {
    private static let timeout: TimeInterval = 120
    private static let environmentID = "oci-imported-runtime-test-fixture"
    private static let rootImageIdentifier =
        "orlix.test.environment.oci-runtime-test-fixture"
    private static let requiredMarkers = [
        "ORLIX-OCI-TIME-NAMESPACE-PROBE",
        "1..6",
        "ok 1 - OCI time proof hostname visible through gethostname",
        "ok 2 - OCI time namespace exposes Linux procfs namespace entry",
        "ok 3 - OCI monotonic timeOffset visible through timens_offsets",
        "ok 4 - OCI boottime timeOffset visible through timens_offsets",
        "ok 5 - Linux CLOCK_MONOTONIC reads in OCI time namespace",
        "ok 6 - Linux CLOCK_BOOTTIME reads in OCI time namespace",
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
            .appendingPathComponent(
                "orlix-oci-time-namespace-\(UUID().uuidString)",
                isDirectory: true
            )
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
        try Self.writeOCIRuntimeConfig(rootPath: "imported-root", to: fixture.root)

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
		try waitForRequiredMarkers()

        let finalState = try installer.state(id: Self.environmentID)
        let lifecycleRecordURL = try runtime.lifecycleStore.recordURL(
            forID: Self.environmentID
        )
        let environmentDirectoryURL = layout.rootDirectory
        let deletedEnvironment = try installer.delete(id: Self.environmentID)

        var text = Self.normalized(recorder.text)
        try Self.validateText(text)
        try Self.validateLifecycle(
            run: run,
            finalState: finalState,
            deletedEnvironment: deletedEnvironment,
            lifecycleRecordURL: lifecycleRecordURL,
            environmentDirectoryURL: environmentDirectoryURL,
            output: text
        )
        text += "\nORLIX_OCI_TIME_NAMESPACE_RUNTIME_STARTED_OK\n"
        text += "ORLIX_OCI_TIME_NAMESPACE_RUNTIME_STOPPED_OK\n"
        text += "ORLIX_OCI_TIME_NAMESPACE_RUNTIME_DELETE_OK\n"
        return text
    }

    private static func writeOCIRuntimeConfig(
        rootPath: String,
        to bundleRoot: URL
    ) throws {
        let document: [String: Any] = [
            "ociVersion": "1.1.0",
            "process": [
                "terminal": false,
                "args": ["/orlix/oci_time_namespace_probe"],
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
            ],
            "root": [
                "path": rootPath,
            ],
            "hostname": "oci-time-host",
            "domainname": "oci.example",
            "linux": [
                "namespaces": [
                    ["type": "time"],
                    ["type": "uts"],
                ],
                "timeOffsets": [
                    "monotonic": [
                        "secs": 5,
                        "nanosecs": 123_456_789,
                    ],
                    "boottime": [
                        "secs": 7,
                        "nanosecs": 987_654_321,
                    ],
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
            throw OrlixOCIDerivedStdioRuntimeProofError.missingMarker(
                marker,
                text
            )
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
            "OCI time namespace runtime markers",
            text
        )
    }

    private static func validateLifecycle(
        run: OrlixOCIEnvironmentRunResult,
        finalState: OrlixOCIRuntimeStateReport,
        deletedEnvironment: OrlixOCIEnvironmentDeleteResult,
        lifecycleRecordURL: URL,
        environmentDirectoryURL: URL,
        output: String
    ) throws {
        guard run.startedStateReport.status == .running,
              run.startedStateReport.pid != nil
        else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI time namespace proof started lifecycle state running\n\(output)"
            )
        }
        guard run.completedStateReport.status == .stopped,
              run.completedStateReport.exitStatus == 0
        else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI time namespace proof stopped exit 0\n\(output)"
            )
        }
        guard finalState.status == .stopped, finalState.exitStatus == 0 else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI time namespace final lifecycle state stopped exit 0\n\(output)"
            )
        }
        guard deletedEnvironment.id == Self.environmentID,
              deletedEnvironment.lifecycleState == .deleted
        else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI time namespace proof delete cleanup\n\(output)"
            )
        }
        guard !FileManager.default.fileExists(atPath: lifecycleRecordURL.path) else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI time namespace lifecycle record cleanup\n\(output)"
            )
        }
        guard !FileManager.default.fileExists(atPath: environmentDirectoryURL.path) else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI time namespace environment directory cleanup\n\(output)"
            )
        }
    }

    private static func normalized(_ text: String) -> String {
        text.replacingOccurrences(of: "\r\n", with: "\n")
            .replacingOccurrences(of: "\r", with: "\n")
    }
}

private final class OrlixOCIDerivedRootfsControlsRuntimeProof: @unchecked Sendable {
    private static let timeout: TimeInterval = 120
    private static let environmentID = "oci-imported-runtime-test-fixture"
    private static let rootImageIdentifier =
        "orlix.test.environment.oci-runtime-test-fixture"
    private static let requiredMarkers = [
        "ORLIX-OCI-ROOTFS-CONTROLS-PROBE",
        "1..5",
        "ok 1 - OCI maskedPaths masks configured rootfs file",
        "ok 2 - OCI readonlyPaths remount rejects writes",
        "ok 3 - OCI tmpfs mount creates Linux directory target",
        "ok 4 - OCI tmpfs mount appears in Linux mountinfo",
        "ok 5 - OCI tmpfs mount supports Linux write readback",
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
            .appendingPathComponent(
                "orlix-oci-rootfs-controls-\(UUID().uuidString)",
                isDirectory: true
            )
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
        try Self.writeOCIRuntimeConfig(rootPath: "imported-root", to: fixture.root)

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
        try waitForRequiredMarkers()

        let finalState = try installer.state(id: Self.environmentID)
        let lifecycleRecordURL = try runtime.lifecycleStore.recordURL(
            forID: Self.environmentID
        )
        let environmentDirectoryURL = layout.rootDirectory
        let deletedEnvironment = try installer.delete(id: Self.environmentID)

        var text = Self.normalized(recorder.text)
        try Self.validateText(text)
        try Self.validateLifecycle(
            run: run,
            finalState: finalState,
            deletedEnvironment: deletedEnvironment,
            lifecycleRecordURL: lifecycleRecordURL,
            environmentDirectoryURL: environmentDirectoryURL,
            output: text
        )
        text += "\nORLIX_OCI_ROOTFS_CONTROLS_RUNTIME_STARTED_OK\n"
        text += "ORLIX_OCI_ROOTFS_CONTROLS_RUNTIME_STOPPED_OK\n"
        text += "ORLIX_OCI_ROOTFS_CONTROLS_RUNTIME_DELETE_OK\n"
        return text
    }

    private static func writeOCIRuntimeConfig(
        rootPath: String,
        to bundleRoot: URL
    ) throws {
        let document: [String: Any] = [
            "ociVersion": "1.1.0",
            "process": [
                "terminal": false,
                "args": ["/orlix/oci_rootfs_controls_probe"],
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
            ],
            "root": [
                "path": rootPath,
            ],
            "hostname": "oci-rootfs-controls-host",
            "domainname": "oci.example",
            "mounts": [
                [
                    "destination": "/mnt/oci-tmpfs",
                    "type": "tmpfs",
                    "source": "tmpfs",
                    "options": ["rw", "nosuid", "nodev"],
                ],
            ],
            "linux": [
                "maskedPaths": [
                    "/etc/os-release",
                ],
                "readonlyPaths": [
                    "/root",
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
            throw OrlixOCIDerivedStdioRuntimeProofError.missingMarker(
                marker,
                text
            )
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
            "OCI rootfs controls runtime markers",
            text
        )
    }

    private static func validateLifecycle(
        run: OrlixOCIEnvironmentRunResult,
        finalState: OrlixOCIRuntimeStateReport,
        deletedEnvironment: OrlixOCIEnvironmentDeleteResult,
        lifecycleRecordURL: URL,
        environmentDirectoryURL: URL,
        output: String
    ) throws {
        guard run.startedStateReport.status == .running,
            run.startedStateReport.pid != nil
        else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI rootfs controls proof started lifecycle state running\n\(output)"
            )
        }
        guard run.completedStateReport.status == .stopped,
            run.completedStateReport.exitStatus == 0
        else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI rootfs controls proof stopped exit 0\n\(output)"
            )
        }
        guard finalState.status == .stopped, finalState.exitStatus == 0 else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI rootfs controls final lifecycle state stopped exit 0\n\(output)"
            )
        }
        guard deletedEnvironment.id == Self.environmentID,
            deletedEnvironment.lifecycleState == .deleted
        else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI rootfs controls proof delete cleanup\n\(output)"
            )
        }
        guard !FileManager.default.fileExists(atPath: lifecycleRecordURL.path) else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI rootfs controls lifecycle record cleanup\n\(output)"
            )
        }
        guard !FileManager.default.fileExists(atPath: environmentDirectoryURL.path) else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI rootfs controls environment directory cleanup\n\(output)"
            )
        }
    }

    private static func normalized(_ text: String) -> String {
        text.replacingOccurrences(of: "\r\n", with: "\n")
            .replacingOccurrences(of: "\r", with: "\n")
    }
}

private final class OrlixOCIDerivedCgroupResourcesRuntimeProof: @unchecked Sendable {
    private static let timeout: TimeInterval = 120
    private static let environmentID = "oci-imported-runtime-test-fixture"
    private static let rootImageIdentifier =
        "orlix.test.environment.oci-runtime-test-fixture"
    private static let requiredMarkers = [
        "1..6",
        "ok 1 - OCI cgroupsPath moves process into configured cgroup",
        "ok 2 - OCI pids limit writes pids.max",
        "ok 3 - OCI CPU quota period writes cpu.max",
        "ok 4 - OCI unified cgroup write applies cpu.weight",
        "ok 5 - OCI memory limit writes memory.max",
        "ok 6 - OCI blockIO weight writes io.weight",
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
            .appendingPathComponent("orlix-oci-cgroups-\(UUID().uuidString)", isDirectory: true)
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
            defaultCommand: ["/orlix/oci_cgroup_resources_probe"],
            defaultEnvironment: [
                "HOME": "/root",
                "PATH": "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
                "TERM": "xterm-256color",
            ],
            defaultWorkingDirectory: "/",
            defaultUserID: 0,
            defaultGroupID: 0,
            hostname: "oci-cgroup-host",
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
            environmentDirectoryURL: environmentDirectoryURL,
            output: text
        )
        text += "\nORLIX_OCI_CGROUP_RESOURCES_RUNTIME_STARTED_OK\n"
        text += "ORLIX_OCI_CGROUP_RESOURCES_RUNTIME_STOPPED_OK\n"
        text += "ORLIX_OCI_CGROUP_RESOURCES_RUNTIME_DELETE_OK\n"
        return text
    }

    private static func writeOCIRuntimeConfig(rootPath: String, to bundleRoot: URL) throws {
        let document: [String: Any] = [
            "ociVersion": "1.1.0",
            "process": [
                "terminal": false,
                "args": ["/orlix/oci_cgroup_resources_probe"],
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
            ],
            "root": [
                "path": rootPath,
            ],
            "hostname": "oci-cgroup-host",
            "domainname": "oci.example",
            "linux": [
                "cgroupsPath": "/orlix/oci-cgroup-runtime-proof",
                "resources": [
                    "pids": [
                        "limit": 64,
                    ],
                    "cpu": [
                        "quota": 50_000,
                        "period": 100_000,
                    ],
                    "memory": [
                        "limit": 268_435_456,
                    ],
                    "blockIO": [
                        "weight": 100,
                    ],
                    "unified": [
                        "cpu.weight": "100",
                    ],
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
        if text.contains("\nnot ok ") ||
            text.contains("ORLIX-APP-RUNTIME-RUNNER-ERROR") {
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
            "OCI cgroup resource runtime markers",
            text
        )
    }

    private static func validateLifecycle(
        run: OrlixOCIEnvironmentRunResult,
        finalState: OrlixOCIRuntimeStateReport,
        deletedEnvironment: OrlixOCIEnvironmentDeleteResult,
        lifecycleRecordURL: URL,
        environmentDirectoryURL: URL,
        output: String
    ) throws {
        guard run.startedStateReport.status == .running,
            run.startedStateReport.pid != nil else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI cgroup resource proof started lifecycle state running\n\(output)"
            )
        }
        guard run.completedStateReport.status == .stopped,
            run.completedStateReport.exitStatus == 0 else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI cgroup resource proof stopped exit 0\n\(output)"
            )
        }
        guard finalState.status == .stopped,
            finalState.exitStatus == 0 else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI cgroup resource final lifecycle state stopped exit 0\n\(output)"
            )
        }
        guard deletedEnvironment.id == Self.environmentID,
            deletedEnvironment.lifecycleState == .deleted else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI cgroup resource proof delete cleanup\n\(output)"
            )
        }
        guard !FileManager.default.fileExists(atPath: lifecycleRecordURL.path) else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI cgroup resource lifecycle record cleanup\n\(output)"
            )
        }
        guard !FileManager.default.fileExists(atPath: environmentDirectoryURL.path) else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI cgroup resource environment directory cleanup\n\(output)"
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
environmentDirectoryURL: environmentDirectoryURL,
output: text
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
        environmentDirectoryURL: URL,
        output: String
    ) throws {
        guard run.startedStateReport.status == .running,
            run.startedStateReport.pid != nil else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI virtio-fs proof started lifecycle state running\n\(output)"
            )
        }
        guard run.completedStateReport.status == .stopped,
            run.completedStateReport.exitStatus == 0 else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI virtio-fs proof stopped exit 0\n\(output)"
            )
        }
        guard finalState.status == .stopped,
            finalState.exitStatus == 0 else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI virtio-fs final lifecycle state stopped exit 0\n\(output)"
            )
        }
        guard deletedEnvironment.id == Self.environmentID,
            deletedEnvironment.lifecycleState == .deleted else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI virtio-fs proof delete cleanup\n\(output)"
            )
        }
        guard !FileManager.default.fileExists(atPath: lifecycleRecordURL.path) else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI virtio-fs lifecycle record cleanup\n\(output)"
            )
        }
        guard !FileManager.default.fileExists(atPath: environmentDirectoryURL.path) else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI virtio-fs environment directory cleanup\n\(output)"
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
                "expected OCI host mount target proof stopped exit 0, got status \(run.completedStateReport.status.rawValue) exit \(String(describing: run.completedStateReport.exitStatus))"
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

private final class OrlixOCIDerivedStopSignalRuntimeProof: @unchecked Sendable {
	private static let timeout: TimeInterval = 120
	private static let environmentID = "oci-imported-runtime-test-fixture"
	private static let rootImageIdentifier =
		"orlix.test.environment.oci-runtime-test-fixture"
	private static let requiredMarkers = [
		"ORLIX_ENV_STOP_SIGNAL_BEGIN",
		"ORLIX_ENV_STOP_SIGNAL_READY",
		"ORLIX_ENV_STOP_SIGNAL_SIGQUIT_TRAP",
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
			.appendingPathComponent(
				"orlix-oci-stop-signal-\(UUID().uuidString)",
				isDirectory: true
			)
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
			defaultCommand: ["/bin/sh", "-c", Self.executionScript],
			defaultEnvironment: [
				"HOME": "/root",
				"PATH": "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
				"TERM": "xterm-256color",
			],
			defaultWorkingDirectory: "/",
			defaultUserID: 0,
			defaultGroupID: 0,
			defaultStopSignal: 3,
			defaultTerminal: true,
			hostname: "oci-stop-signal-host",
			domainname: "oci.example",
			rootMount: .defaultOverlay
		)
		let registry = OrlixEnvironmentRegistry(
			linuxStateRoot: fixture.linuxStateRoot,
			cacheRoot: fixture.cacheRoot,
			scratchRoot: fixture.scratchRoot
		)
		try registry.save(descriptor)
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
			script: Self.executionScript,
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
		try registry.save(descriptor)
		try runtime.lifecycleStore.save(lifecycle)
		let driver = OrlixOCIRuntimeLinuxSessionObservationDriver(
			timeout: Self.timeout
		)
		let processSession = try OrlixOCIRuntimeProcessSession(
			lifecycle: lifecycle,
			rootMount: .defaultOverlay,
			registry: registry,
			terminal: terminal,
			lifecycleStore: runtime.lifecycleStore
		)
		_ = try processSession.start(using: driver)
		try waitForMarker("ORLIX_ENV_STOP_SIGNAL_READY")
		let runningState = try runtime.lifecycleStore.stateReport(id: descriptor.id)
		let installer = OrlixOCIEnvironmentInstaller(registry: registry)
		let signaled = try installer.kill(
			arguments: ["kill", descriptor.id],
			terminal: terminal,
			using: driver
		)
		let waited = try installer.wait(
			arguments: ["wait", descriptor.id],
			terminal: terminal,
			using: driver
		)
		let lifecycleRecordURL = try runtime.lifecycleStore.recordURL(
			forID: descriptor.id
		)
		let environmentDirectoryURL = layout.rootDirectory
		let deletedEnvironment = try installer.delete(id: descriptor.id)

		var text = Self.normalized(recorder.text)
		try Self.validateText(text)
		try Self.validateLifecycle(
			runningState: runningState,
			signaled: signaled,
			waited: waited,
			deletedEnvironment: deletedEnvironment,
			lifecycleRecordURL: lifecycleRecordURL,
			environmentDirectoryURL: environmentDirectoryURL
		)
		text += "\nORLIX_OCI_STOP_SIGNAL_RUNTIME_STARTED_OK\n"
		text += "ORLIX_OCI_STOP_SIGNAL_RUNTIME_SIGNALED_OK\n"
		text += "ORLIX_OCI_STOP_SIGNAL_RUNTIME_STOPPED_OK\n"
		text += "ORLIX_OCI_STOP_SIGNAL_RUNTIME_DELETE_OK\n"
		return text
	}

	private static var executionScript: String {
		[
			"trap 'printf \"ORLIX_ENV_STOP_SIGNAL_SIGQUIT_TRAP\\n\"; exit 131' QUIT",
			"printf 'ORLIX_ENV_STOP_SIGNAL_BEGIN\\n'",
			"printf 'ORLIX_ENV_STOP_SIGNAL_READY\\n'",
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
		signaled: OrlixOCIEnvironmentSignalResult,
		waited: OrlixOCIEnvironmentWaitResult,
		deletedEnvironment: OrlixOCIEnvironmentDeleteResult,
		lifecycleRecordURL: URL,
		environmentDirectoryURL: URL
	) throws {
		guard runningState.status == .running, runningState.pid != nil else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI stopSignal proof running state before kill"
			)
		}
		guard signaled.signal == 3 else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI kill without explicit signal to use descriptor stopSignal SIGQUIT"
			)
		}
		guard signaled.stateReport.status == .running,
			signaled.stateReport.pid == runningState.pid
		else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI stopSignal proof to remain running until wait observes completion"
			)
		}
		guard waited.stateReport.status == .stopped,
			waited.stateReport.exitStatus == 131
		else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI stopSignal proof stopped exit 131 after SIGQUIT"
			)
		}
		guard deletedEnvironment.id == Self.environmentID,
			deletedEnvironment.lifecycleState == .deleted
		else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI stopSignal proof delete cleanup"
			)
		}
		guard !FileManager.default.fileExists(atPath: lifecycleRecordURL.path) else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI stopSignal lifecycle record cleanup"
			)
		}
		guard !FileManager.default.fileExists(atPath: environmentDirectoryURL.path)
		else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI stopSignal environment directory cleanup"
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
			"hostname": "oci-stop-signal-host",
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

private final class OrlixOCIDerivedKillSignalRuntimeProof: @unchecked Sendable {
    private static let timeout: TimeInterval = 120
    private static let environmentID = "oci-kill-signal-runtime-test-fixture"
    private static let rootImageIdentifier =
        "orlix.test.environment.oci-runtime-test-fixture"
	private static let requiredMarkers = [
		"ORLIX_ENV_KILL_SIGNAL_BEGIN",
		"ORLIX_ENV_KILL_SIGNAL_READY",
		"ORLIX_ENV_KILL_SIGNAL_SIGINT_TRAP",
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
            .appendingPathComponent(
                "orlix-oci-kill-signal-\(UUID().uuidString)",
                isDirectory: true
            )
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
            defaultCommand: ["/bin/sh", "-c", Self.executionScript],
            defaultEnvironment: [
                "HOME": "/root",
                "PATH": "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
                "TERM": "xterm-256color",
            ],
            defaultWorkingDirectory: "/",
            defaultUserID: 0,
            defaultGroupID: 0,
            defaultTerminal: true,
            hostname: "oci-kill-signal-host",
            domainname: "oci.example",
            rootMount: .defaultOverlay
        )
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: fixture.linuxStateRoot,
            cacheRoot: fixture.cacheRoot,
            scratchRoot: fixture.scratchRoot
        )
        try registry.save(descriptor)
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
		let sourceLayout = try OrlixEnvironmentStorageLayout.layout(
			forEnvironmentID: "oci-imported-runtime-test-fixture",
			linuxStateRoot: fixture.linuxStateRoot,
			cacheRoot: fixture.cacheRoot,
			scratchRoot: fixture.scratchRoot
		)
		try fileManager.createDirectory(
			at: layout.rootDirectory,
			withIntermediateDirectories: true
		)
		try fileManager.copyItem(at: sourceLayout.baseImageURL, to: layout.baseImageURL)
		try fileManager.copyItem(at: sourceLayout.stateImageURL, to: layout.stateImageURL)
		try OrlixOCIDerivedStdioRuntimeProof.writeOCIRuntimeConfig(
			terminal: true,
			script: Self.executionScript,
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
        try registry.save(descriptor)
        try runtime.lifecycleStore.save(lifecycle)
        let driver = OrlixOCIRuntimeLinuxSessionObservationDriver(
            timeout: Self.timeout
        )
        let processSession = try OrlixOCIRuntimeProcessSession(
            lifecycle: lifecycle,
            rootMount: .defaultOverlay,
            registry: registry,
            terminal: terminal,
            lifecycleStore: runtime.lifecycleStore
        )
        _ = try processSession.start(using: driver)
        try waitForMarker("ORLIX_ENV_KILL_SIGNAL_READY")
        let runningState = try runtime.lifecycleStore.stateReport(id: descriptor.id)
        let installer = OrlixOCIEnvironmentInstaller(registry: registry)
		let signaled = try installer.kill(
			arguments: ["kill", "--signal=2", descriptor.id],
			terminal: terminal,
			using: driver
		)
        let waited = try installer.wait(
            arguments: ["wait", descriptor.id],
            terminal: terminal,
            using: driver
        )
        let lifecycleRecordURL = try runtime.lifecycleStore.recordURL(
            forID: descriptor.id
        )
        let environmentDirectoryURL = layout.rootDirectory
        let deletedEnvironment = try installer.delete(id: descriptor.id)

        var text = Self.normalized(recorder.text)
        try Self.validateText(text)
        try Self.validateLifecycle(
            runningState: runningState,
            signaled: signaled,
            waited: waited,
            deletedEnvironment: deletedEnvironment,
            lifecycleRecordURL: lifecycleRecordURL,
            environmentDirectoryURL: environmentDirectoryURL
        )
        text += "\nORLIX_OCI_KILL_SIGNAL_RUNTIME_STARTED_OK\n"
        text += "ORLIX_OCI_KILL_SIGNAL_RUNTIME_SIGNALED_OK\n"
        text += "ORLIX_OCI_KILL_SIGNAL_RUNTIME_STOPPED_OK\n"
        text += "ORLIX_OCI_KILL_SIGNAL_RUNTIME_DELETE_OK\n"
        return text
    }

    private static var executionScript: String {
		[
			"trap 'printf \"ORLIX_ENV_KILL_SIGNAL_SIGINT_TRAP\\n\"; exit 130' INT",
			"printf 'ORLIX_ENV_KILL_SIGNAL_BEGIN\\n'",
			"printf 'ORLIX_ENV_KILL_SIGNAL_READY\\n'",
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
        signaled: OrlixOCIEnvironmentSignalResult,
        waited: OrlixOCIEnvironmentWaitResult,
        deletedEnvironment: OrlixOCIEnvironmentDeleteResult,
        lifecycleRecordURL: URL,
        environmentDirectoryURL: URL
    ) throws {
        guard runningState.status == .running, runningState.pid != nil else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI kill proof running state before SIGKILL"
            )
        }
		guard signaled.signal == 2 else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI kill proof explicit SIGINT"
			)
		}
        guard signaled.stateReport.status == .running,
              signaled.stateReport.pid == runningState.pid
        else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI kill proof remain running until wait observes completion"
            )
        }
		guard waited.stateReport.status == .stopped,
		      waited.stateReport.exitStatus == 130
		else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI kill proof stopped exit 130 after SIGINT"
			)
		}
        guard deletedEnvironment.id == Self.environmentID,
              deletedEnvironment.lifecycleState == .deleted
        else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI kill proof delete cleanup"
            )
        }
        guard !FileManager.default.fileExists(atPath: lifecycleRecordURL.path) else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI kill proof lifecycle record cleanup"
            )
        }
        guard !FileManager.default.fileExists(atPath: environmentDirectoryURL.path)
        else {
            throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
                "expected OCI kill proof environment directory cleanup"
            )
        }
    }

    private static func normalized(_ text: String) -> String {
        text.replacingOccurrences(of: "\r\n", with: "\n")
            .replacingOccurrences(of: "\r", with: "\n")
    }
}

private final class OrlixOCIDerivedLifecycleStateRuntimeProof: @unchecked Sendable {
	private static let timeout: TimeInterval = 120
	private static let environmentID = "oci-imported-runtime-test-fixture"
	private static let rootImageIdentifier =
		"orlix.test.environment.oci-runtime-test-fixture"
	private static let requiredMarkers = [
		"ORLIX_ENV_LIFECYCLE_STATE_BEGIN",
		"ORLIX_ENV_LIFECYCLE_STATE_READY",
		"ORLIX_ENV_LIFECYCLE_STATE_DONE",
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
			.appendingPathComponent(
				"orlix-oci-lifecycle-state-\(UUID().uuidString)",
				isDirectory: true
			)
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
			defaultCommand: ["/bin/sh", "-c", Self.executionScript],
			defaultEnvironment: [
				"HOME": "/root",
				"PATH": "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
				"TERM": "xterm-256color",
			],
			defaultWorkingDirectory: "/",
			defaultUserID: 0,
			defaultGroupID: 0,
			hostname: "oci-lifecycle-state-host",
			domainname: "oci.example",
			rootMount: .defaultOverlay
		)
		let registry = OrlixEnvironmentRegistry(
			linuxStateRoot: fixture.linuxStateRoot,
			cacheRoot: fixture.cacheRoot,
			scratchRoot: fixture.scratchRoot
		)
		try registry.save(descriptor)
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
			script: Self.executionScript,
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

		let createdState = try runtime.lifecycleStore.stateReport(id: descriptor.id)
		let createdList = try runtime.listPreparedEnvironments()
		let driver = OrlixOCIRuntimeLinuxSessionObservationDriver(
			timeout: Self.timeout
		)
		let processSession = try OrlixOCIRuntimeProcessSession(
			lifecycle: lifecycle,
			rootMount: .defaultOverlay,
			registry: registry,
			terminal: terminal,
			lifecycleStore: runtime.lifecycleStore
		)
		let runningSession = try processSession.start(using: driver)
		try waitForMarker("ORLIX_ENV_LIFECYCLE_STATE_READY")
		let runningState = try runtime.lifecycleStore.stateReport(id: descriptor.id)
		let runningList = try runtime.listPreparedEnvironments()
		try waitForMarker("ORLIX_ENV_LIFECYCLE_STATE_DONE")
		_ = try runningSession.wait(using: driver)
		let stoppedState = try runtime.lifecycleStore.stateReport(id: descriptor.id)
		let stoppedList = try runtime.listPreparedEnvironments()
		let lifecycleRecordURL = try runtime.lifecycleStore.recordURL(
			forID: descriptor.id
		)
		let deletedEnvironment = try runtime.delete(id: descriptor.id)

		var text = Self.normalized(recorder.text)
		try Self.validateText(text)
		try Self.validateLifecycle(
			createdState: createdState,
			createdList: createdList,
			runningState: runningState,
			runningList: runningList,
			stoppedState: stoppedState,
			stoppedList: stoppedList,
			deletedEnvironment: deletedEnvironment,
			lifecycleRecordURL: lifecycleRecordURL,
			environmentDirectoryURL: layout.rootDirectory
		)
		text += "\nORLIX_OCI_LIFECYCLE_STATE_CREATED_OK\n"
		text += "ORLIX_OCI_LIFECYCLE_STATE_LIST_CREATED_OK\n"
		text += "ORLIX_OCI_LIFECYCLE_STATE_RUNNING_OK\n"
		text += "ORLIX_OCI_LIFECYCLE_STATE_LIST_RUNNING_OK\n"
		text += "ORLIX_OCI_LIFECYCLE_STATE_STOPPED_OK\n"
		text += "ORLIX_OCI_LIFECYCLE_STATE_LIST_STOPPED_OK\n"
		text += "ORLIX_OCI_LIFECYCLE_STATE_DELETE_OK\n"
		return text
	}

	private static var executionScript: String {
		[
			"printf 'ORLIX_ENV_LIFECYCLE_STATE_BEGIN\\n'",
			"printf 'ORLIX_ENV_LIFECYCLE_STATE_READY\\n'",
			"printf 'ORLIX_ENV_LIFECYCLE_STATE_DONE\\n'",
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
		createdState: OrlixOCIRuntimeStateReport,
		createdList: [OrlixOCIEnvironmentPreparedState],
		runningState: OrlixOCIRuntimeStateReport,
		runningList: [OrlixOCIEnvironmentPreparedState],
		stoppedState: OrlixOCIRuntimeStateReport,
		stoppedList: [OrlixOCIEnvironmentPreparedState],
		deletedEnvironment: OrlixOCIRuntimeDeletedEnvironment,
		lifecycleRecordURL: URL,
		environmentDirectoryURL: URL
	) throws {
		guard createdState.status == .created,
			createdState.pid == nil,
			createdState.exitStatus == nil
		else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI lifecycle state created before start"
			)
		}
		try validatePreparedList(
			createdList,
			expectedLifecycleState: .created,
			expectedStateStatus: .created
		)
		guard runningState.status == .running, runningState.pid != nil else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI lifecycle state running after start"
			)
		}
		try validatePreparedList(
			runningList,
			expectedLifecycleState: .running,
			expectedStateStatus: .running
		)
		guard stoppedState.status == .stopped,
			stoppedState.pid == runningState.pid,
			stoppedState.exitStatus == 0
		else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI lifecycle state stopped exit 0 after wait"
			)
		}
		try validatePreparedList(
			stoppedList,
			expectedLifecycleState: .stopped,
			expectedStateStatus: .stopped
		)
		guard deletedEnvironment.id == Self.environmentID,
			deletedEnvironment.deletedRecord.state == .deleted
		else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI lifecycle delete record state deleted"
			)
		}
		guard !FileManager.default.fileExists(atPath: lifecycleRecordURL.path) else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI lifecycle record cleanup"
			)
		}
		guard !FileManager.default.fileExists(atPath: environmentDirectoryURL.path)
		else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI lifecycle environment directory cleanup"
			)
		}
	}

	private static func validatePreparedList(
		_ prepared: [OrlixOCIEnvironmentPreparedState],
		expectedLifecycleState: OrlixOCIRuntimeLifecycleState,
		expectedStateStatus: OrlixOCIRuntimeStateStatus
	) throws {
		guard let entry = prepared.first(where: { $0.id == Self.environmentID })
		else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI lifecycle prepared list entry"
			)
		}
		guard entry.lifecycleState == expectedLifecycleState,
			entry.stateReport?.status == expectedStateStatus
		else {
			throw OrlixOCIDerivedStdioRuntimeProofError.lifecycle(
				"expected OCI lifecycle prepared list state \(expectedLifecycleState.rawValue)"
			)
		}
	}

	private static func writeOCIRuntimeConfig(
		script: String,
		rootPath: String,
		to bundleRoot: URL
	) throws {
		let process: [String: Any] = [
			"terminal": false,
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
			"hostname": "oci-lifecycle-state-host",
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
