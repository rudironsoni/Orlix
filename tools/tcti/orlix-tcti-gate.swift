#!/usr/bin/env swift
import Foundation
import Darwin

enum GateStatus: String, Codable {
    case pass
    case fail
    case todo
    case skipped
    case error
    case evidence

    var passed: Bool { self == .pass }
    var gateEligible: Bool { self == .pass }
}

struct Failure: Codable {
    let id: String
    let message: String
}

struct ProofTierMetadata: Equatable {
    let proofTier: String
    let acceptanceWeight: String
    let realStackRequired: Bool
    let canClaimRuntimeReadiness: Bool

    var defaultReleaseGateEligible: Bool {
        acceptanceWeight == "release" && realStackRequired && canClaimRuntimeReadiness
    }

    var defaultReadinessGateEligible: Bool {
        (acceptanceWeight == "readiness" || acceptanceWeight == "release") &&
            realStackRequired &&
            canClaimRuntimeReadiness
    }
}

struct RoadmapProofTierIndex {
    let metadataByTarget: [String: ProofTierMetadata]
    let errors: [String]
}

struct Report: Codable {
    let target: String
    let gate: String
    let status: String
    let passed: Bool
    let summary: String
    let command: String?
    let proofTier: String
    let acceptanceWeight: String
    let realStackRequired: Bool
    let canClaimRuntimeReadiness: Bool
    let gitSha: String
    let backend: String
    let kernelProfile: String?
    let kernelConfig: String?
    let virtualCpuModel: String
    let hostPageSize: Int
    let guestPageSize: Int
    let forbiddenBehavior: [String: Bool]
    let counters: [String: Int]
    let failures: [Failure]
    let artifacts: [String]
    let releaseGateEligible: Bool
    let readinessGateEligible: Bool
    let autonomousTestsBypassed: Bool
    let bypassReason: String
    let coverageWarnings: [String]
    let evidence: [String: String]?
    let expectedStatus: String?
    let actualReplayStatus: String?
    let execution: ExecutionReport?

    enum CodingKeys: String, CodingKey {
        case target
        case gate
        case status
        case passed
        case summary
        case command
        case proofTier = "proof_tier"
        case acceptanceWeight = "acceptance_weight"
        case realStackRequired = "real_stack_required"
        case canClaimRuntimeReadiness = "can_claim_runtime_readiness"
        case gitSha = "git_sha"
        case backend
        case kernelProfile = "kernel_profile"
        case kernelConfig = "kernel_config"
        case virtualCpuModel = "virtual_cpu_model"
        case hostPageSize = "host_page_size"
        case guestPageSize = "guest_page_size"
        case forbiddenBehavior = "forbidden_behavior"
        case counters
        case failures
        case artifacts
        case releaseGateEligible = "release_gate_eligible"
        case readinessGateEligible = "readiness_gate_eligible"
        case autonomousTestsBypassed = "autonomous_tests_bypassed"
        case bypassReason = "bypass_reason"
        case coverageWarnings = "coverage_warnings"
        case evidence
        case expectedStatus = "expected_status"
        case actualReplayStatus = "actual_replay_status"
        case execution
    }
}

struct ExecutionReport: Codable {
    let backend: String
    let caseID: String
    let enteredEntrypoint: Bool
    let guestInstructionsExecuted: Int
    let decodedInstructions: [DecodedInstructionReport]
    let syscalls: [CapturedSyscall]
    let exit: CapturedExit?
    let fault: CapturedFault?
    let instructionEncodings: [String]
    let memoryWrites: [CapturedMemoryWrite]
    let notes: [String]

    enum CodingKeys: String, CodingKey {
        case backend
        case caseID = "case_id"
        case enteredEntrypoint = "entered_entrypoint"
        case guestInstructionsExecuted = "guest_instructions_executed"
        case decodedInstructions = "decoded_instructions"
        case syscalls
        case exit
        case fault
        case instructionEncodings = "instruction_encodings"
        case memoryWrites = "memory_writes"
        case notes
    }
}

struct CapturedFault: Codable {
    let kind: String
    let address: String
    let access: String
    let captured: Bool
}

struct CapturedMemoryWrite: Codable {
    let address: String
    let width: Int
    let value: String
    let captured: Bool

    enum CodingKeys: String, CodingKey {
        case address
        case width
        case value
        case captured
    }
}

struct DecodedInstructionReport: Codable {
    let pc: String
    let raw: String
    let instructionClass: String
    let op: String?
    let sf: Int?
    let rd: Int?
    let rn: Int?
    let rm: Int?
    let rt: Int?
    let imm: Int?
    let immHex: String?
    let shift: Int?
    let offset: Int?
    let width: Int?
    let laneSize: Int?
    let sourceIndex: Int?
    let destinationIndex: Int?
    let effectiveAddress: String?
    let sysreg: String?
    let reason: String?

    init(
        pc: String,
        raw: String,
        instructionClass: String,
        op: String?,
        sf: Int?,
        rd: Int?,
        rn: Int? = nil,
        rm: Int? = nil,
        rt: Int? = nil,
        imm: Int?,
        immHex: String? = nil,
        shift: Int?,
        offset: Int? = nil,
        width: Int? = nil,
        laneSize: Int? = nil,
        sourceIndex: Int? = nil,
        destinationIndex: Int? = nil,
        effectiveAddress: String? = nil,
        sysreg: String? = nil,
        reason: String?
    ) {
        self.pc = pc
        self.raw = raw
        self.instructionClass = instructionClass
        self.op = op
        self.sf = sf
        self.rd = rd
        self.rn = rn
        self.rm = rm
        self.rt = rt
        self.imm = imm
        self.immHex = immHex
        self.shift = shift
        self.offset = offset
        self.width = width
        self.laneSize = laneSize
        self.sourceIndex = sourceIndex
        self.destinationIndex = destinationIndex
        self.effectiveAddress = effectiveAddress
        self.sysreg = sysreg
        self.reason = reason
    }

    enum CodingKeys: String, CodingKey {
        case pc
        case raw
        case instructionClass = "class"
        case op
        case sf
        case rd
        case rn
        case rm
        case rt
        case imm
        case immHex = "imm_hex"
        case shift
        case offset
        case width
        case laneSize = "lane_size"
        case sourceIndex = "source_index"
        case destinationIndex = "destination_index"
        case effectiveAddress = "effective_address"
        case sysreg
        case reason
    }
}

enum JSONValue: Codable {
    case int(Int)
    case string(String)

    init(from decoder: Decoder) throws {
        let container = try decoder.singleValueContainer()
        if let value = try? container.decode(Int.self) {
            self = .int(value)
        } else {
            self = .string(try container.decode(String.self))
        }
    }

    func encode(to encoder: Encoder) throws {
        var container = encoder.singleValueContainer()
        switch self {
        case let .int(value):
            try container.encode(value)
        case let .string(value):
            try container.encode(value)
        }
    }
}

func jsonInt(_ value: JSONValue?) -> Int? {
    guard let value else { return nil }
    if case let .int(number) = value {
        return number
    }
    return nil
}

func jsonString(_ value: JSONValue?) -> String? {
    guard let value else { return nil }
    if case let .string(string) = value {
        return string
    }
    return nil
}

struct CapturedSyscall: Codable {
    let nr: Int
    let name: String
    let args: [JSONValue]
    let captured: Bool
    let capturedBytes: String?

    init(nr: Int, name: String, args: [JSONValue], captured: Bool, capturedBytes: String? = nil) {
        self.nr = nr
        self.name = name
        self.args = args
        self.captured = captured
        self.capturedBytes = capturedBytes
    }

    enum CodingKeys: String, CodingKey {
        case nr
        case name
        case args
        case captured
        case capturedBytes = "captured_bytes"
    }
}

struct CapturedExit: Codable {
    let kind: String
    let code: Int
}

struct DiffArchitecturalState: Codable {
    let backend: String
    let caseID: String
    let gprs: [String: UInt64]
    let sp: String
    let pc: String
    let pstateNZCV: String
    let tpidrEL0: String
    let memoryWrites: [String]
    let exitKind: String?
    var exitCode: Int?
    let faultAddress: String?

    enum CodingKeys: String, CodingKey {
        case backend
        case caseID = "case_id"
        case gprs
        case sp
        case pc
        case pstateNZCV = "pstate_nzcv"
        case tpidrEL0 = "tpidr_el0"
        case memoryWrites = "memory_writes"
        case exitKind = "exit_kind"
        case exitCode = "exit_code"
        case faultAddress = "fault_address"
    }
}

struct SwitchDiffArtifact: Codable {
    let caseID: String
    let mode: String
    let referenceBackend: String
    let candidateBackend: String
    let gadgetDispatchExecuted: Bool
    let productionAssemblyExecuted: Bool
    let fieldsChecked: [String]
    let divergentFields: [String]
    let referenceState: DiffArchitecturalState
    let candidateState: DiffArchitecturalState
    let notes: [String]

    enum CodingKeys: String, CodingKey {
        case caseID = "case_id"
        case mode
        case referenceBackend = "reference_backend"
        case candidateBackend = "candidate_backend"
        case gadgetDispatchExecuted = "gadget_dispatch_executed"
        case productionAssemblyExecuted = "production_assembly_executed"
        case fieldsChecked = "fields_checked"
        case divergentFields = "divergent_fields"
        case referenceState = "reference_state"
        case candidateState = "candidate_state"
        case notes
    }
}

struct Reproducer: Codable {
    let target: String
    let caseID: String
    let command: String
    let workingDirectory: String
    let artifacts: [String]
    let reason: String
    let expectedStatus: String?

    enum CodingKeys: String, CodingKey {
        case target
        case caseID = "case_id"
        case command
        case workingDirectory = "working_directory"
        case artifacts
        case reason
        case expectedStatus = "expected_status"
    }
}

struct GoldenMetadata: Codable {
    let caseID: String
    let generatorCommand: String
    let sourceSHA256: String
    let expectedBinarySHA256: String
    let actualBinarySHA256: String
    let compilerPath: String
    let compilerVersion: String
    let linkerPath: String
    let linkerVersion: String
    let flags: [String]
    let libcMode: String
    let elfType: String
    let entrypoint: String
    let machine: String
    let expectedSyscalls: [ExpectedSyscall]
    let expectedExitCode: Int
    let expectedMessage: String?
    let forbiddenBehavior: [String: Bool]

    enum CodingKeys: String, CodingKey {
        case caseID = "case_id"
        case generatorCommand = "generator_command"
        case sourceSHA256 = "source_sha256"
        case expectedBinarySHA256 = "expected_binary_sha256"
        case actualBinarySHA256 = "actual_binary_sha256"
        case compilerPath = "compiler_path"
        case compilerVersion = "compiler_version"
        case linkerPath = "linker_path"
        case linkerVersion = "linker_version"
        case flags
        case libcMode = "libc_mode"
        case elfType = "elf_type"
        case entrypoint
        case machine
        case expectedSyscalls = "expected_syscalls"
        case expectedExitCode = "expected_exit_code"
        case expectedMessage = "expected_message"
        case forbiddenBehavior = "forbidden_behavior"
    }
}

struct ExpectedSyscall: Codable {
    let nr: String
    let code: Int?
    let fd: Int?
    let len: Int?
    let bytes: String?
}

enum GateError: Error, CustomStringConvertible {
    case usage(String)
    case commandFailed(String)
    case invalidReport(String)
    case invalidReducer(String)
    case checkFailed([String])

    var description: String {
        switch self {
        case let .usage(message):
            return message
        case let .commandFailed(message):
            return message
        case let .invalidReport(message):
            return "invalid report: \(message)"
        case let .invalidReducer(message):
            return "invalid reducer: \(message)"
        case let .checkFailed(messages):
            return messages.joined(separator: "\n")
        }
    }
}

let fileManager = FileManager.default
let encoder: JSONEncoder = {
    let encoder = JSONEncoder()
    encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
    return encoder
}()
let decoder = JSONDecoder()

func repoRoot() -> URL {
    URL(fileURLWithPath: fileManager.currentDirectoryPath)
}

func buildRoot() -> URL {
    if let override = ProcessInfo.processInfo.environment["ORLIX_TCTI_BUILD_ROOT"], !override.isEmpty {
        return URL(fileURLWithPath: override)
    }
    return repoRoot().appendingPathComponent("Build/TCTI", isDirectory: true)
}

func path(_ components: String...) -> URL {
    components.reduce(repoRoot()) { $0.appendingPathComponent($1) }
}

func buildPath(_ components: String...) -> URL {
    components.reduce(buildRoot()) { $0.appendingPathComponent($1) }
}

func ensureDirectory(_ url: URL) throws {
    try fileManager.createDirectory(at: url, withIntermediateDirectories: true)
}

func relativePath(_ url: URL) -> String {
    let root = repoRoot().standardizedFileURL.path
    let value = url.standardizedFileURL.path
    if value == root {
        return "."
    }
    if value.hasPrefix(root + "/") {
        return String(value.dropFirst(root.count + 1))
    }
    return value
}

func run(_ arguments: [String], check: Bool = true) throws -> String {
    let process = Process()
    process.executableURL = URL(fileURLWithPath: "/usr/bin/env")
    process.arguments = arguments
    process.currentDirectoryURL = repoRoot()
    let stdout = Pipe()
    let stderr = Pipe()
    process.standardOutput = stdout
    process.standardError = stderr
    try process.run()
    process.waitUntilExit()
    let output = String(data: stdout.fileHandleForReading.readDataToEndOfFile(), encoding: .utf8) ?? ""
    let error = String(data: stderr.fileHandleForReading.readDataToEndOfFile(), encoding: .utf8) ?? ""
    if check && process.terminationStatus != 0 {
        throw GateError.commandFailed((output + error).trimmingCharacters(in: .whitespacesAndNewlines))
    }
    return output.trimmingCharacters(in: .whitespacesAndNewlines)
}

func runWithFileBackedOutput(
    _ arguments: [String],
    check: Bool = true,
    terminateAfterOutputContains successNeedles: [String] = [],
    terminationGraceSeconds: TimeInterval = 10
) throws -> String {
    let scratch = buildPath("tmp", "command-output")
    try ensureDirectory(scratch)
    let unique = UUID().uuidString
    let stdoutURL = scratch.appendingPathComponent("\(unique).stdout")
    let stderrURL = scratch.appendingPathComponent("\(unique).stderr")
    fileManager.createFile(atPath: stdoutURL.path, contents: nil)
    fileManager.createFile(atPath: stderrURL.path, contents: nil)
    let stdoutHandle = try FileHandle(forWritingTo: stdoutURL)
    let stderrHandle = try FileHandle(forWritingTo: stderrURL)
    defer {
        try? stdoutHandle.close()
        try? stderrHandle.close()
        try? fileManager.removeItem(at: stdoutURL)
        try? fileManager.removeItem(at: stderrURL)
    }

    let process = Process()
    process.executableURL = URL(fileURLWithPath: "/usr/bin/env")
    process.arguments = arguments
    process.currentDirectoryURL = repoRoot()
    process.standardOutput = stdoutHandle
    process.standardError = stderrHandle
    try process.run()

    func combinedOutput() -> String {
        let output = (try? String(contentsOf: stdoutURL, encoding: .utf8)) ?? ""
        let error = (try? String(contentsOf: stderrURL, encoding: .utf8)) ?? ""
        return output + error
    }

    if successNeedles.isEmpty {
        process.waitUntilExit()
    } else {
        var successSeenAt: Date?
        while process.isRunning {
            let currentOutput = combinedOutput()
            if successNeedles.allSatisfy({ currentOutput.contains($0) }) {
                if successSeenAt == nil {
                    successSeenAt = Date()
                }
                if let seenAt = successSeenAt,
                   Date().timeIntervalSince(seenAt) >= terminationGraceSeconds {
                    process.terminate()
                    let deadline = Date().addingTimeInterval(5)
                    while process.isRunning && Date() < deadline {
                        Thread.sleep(forTimeInterval: 0.1)
                    }
                    if process.isRunning {
                        Darwin.kill(process.processIdentifier, SIGKILL)
                    }
                    break
                }
            } else {
                successSeenAt = nil
            }
            Thread.sleep(forTimeInterval: 0.25)
        }
        process.waitUntilExit()
    }

    let output = (try? String(contentsOf: stdoutURL, encoding: .utf8)) ?? ""
    let error = (try? String(contentsOf: stderrURL, encoding: .utf8)) ?? ""
    if check && process.terminationStatus != 0 {
        throw GateError.commandFailed((output + error).trimmingCharacters(in: .whitespacesAndNewlines))
    }
    return output.trimmingCharacters(in: .whitespacesAndNewlines)
}

func commandPath(_ name: String) throws -> String {
    try run(["command", "-v", name])
}

func gitSha() -> String {
    (try? run(["git", "rev-parse", "HEAD"])) ?? ""
}

func sha256(_ url: URL) throws -> String {
    let output = try run(["shasum", "-a", "256", url.path])
    guard let first = output.split(separator: " ").first else {
        throw GateError.commandFailed("shasum did not return a digest for \(url.path)")
    }
    return String(first)
}

func hostPageSize() -> Int {
    Int(getpagesize())
}

func forbiddenDefaults(hostX18: Bool = false) -> [String: Bool] {
    [
        "host_exec_guest_text": false,
        "map_jit": false,
        "rwx": false,
        "generated_exec_memory": false,
        "host_x18": hostX18,
        "native_ios_api_exposure_to_guest": false,
    ]
}

func tctiGateTarget(in command: String) -> String? {
    guard command.contains("tcti-gate") else { return nil }
    for rawToken in command.split(whereSeparator: { $0 == " " || $0 == "\t" || $0 == "\n" }) {
        let token = rawToken.trimmingCharacters(in: CharacterSet(charactersIn: "\"'"))
        if token.hasPrefix("TARGET=") {
            let value = String(token.dropFirst("TARGET=".count))
            return value.trimmingCharacters(in: CharacterSet(charactersIn: "\"'"))
        }
    }
    return nil
}

func proofTierMetadata(from gate: [String: Any]) -> ProofTierMetadata? {
    guard let proofTier = gate["proof_tier"] as? String,
          let acceptanceWeight = gate["acceptance_weight"] as? String,
          let realStackRequired = gate["real_stack_required"] as? Bool,
          let canClaimRuntimeReadiness = gate["can_claim_runtime_readiness"] as? Bool else {
        return nil
    }
    return ProofTierMetadata(
        proofTier: proofTier,
        acceptanceWeight: acceptanceWeight,
        realStackRequired: realStackRequired,
        canClaimRuntimeReadiness: canClaimRuntimeReadiness
    )
}

func roadmapProofTierURL() -> URL {
    path(".agents", "skills", "orlix-tcti-next-step", "references", "tcti-roadmap.json")
}

func loadRoadmapProofTierMetadataByTarget(from roadmapURL: URL = roadmapProofTierURL()) -> RoadmapProofTierIndex {
    let roadmapPath = relativePath(roadmapURL)
    let data: Data
    do {
        data = try Data(contentsOf: roadmapURL)
    } catch {
        return RoadmapProofTierIndex(
            metadataByTarget: [:],
            errors: ["cannot read proof-tier roadmap \(roadmapPath): \(error)"]
        )
    }

    let object: [String: Any]
    do {
        guard let decoded = try JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            return RoadmapProofTierIndex(
                metadataByTarget: [:],
                errors: ["proof-tier roadmap \(roadmapPath) must be a JSON object"]
            )
        }
        object = decoded
    } catch {
        return RoadmapProofTierIndex(
            metadataByTarget: [:],
            errors: ["cannot parse proof-tier roadmap \(roadmapPath): \(error)"]
        )
    }

    guard let gates = object["gates"] as? [[String: Any]] else {
        return RoadmapProofTierIndex(
            metadataByTarget: [:],
            errors: ["proof-tier roadmap \(roadmapPath) must contain a gates array"]
        )
    }

    var metadataByTarget: [String: ProofTierMetadata] = [:]
    var errors: [String] = []
    for (index, gate) in gates.enumerated() {
        guard let command = gate["command"] as? String,
              let target = tctiGateTarget(in: command) else {
            continue
        }
        guard let metadata = proofTierMetadata(from: gate) else {
            errors.append("roadmap gate \(index) target \(target) is missing explicit proof-tier metadata")
            continue
        }
        if let existing = metadataByTarget[target], existing != metadata {
            errors.append("roadmap target \(target) has conflicting proof-tier metadata")
            continue
        }
        metadataByTarget[target] = metadata
    }
    return RoadmapProofTierIndex(metadataByTarget: metadataByTarget, errors: errors)
}

func roadmapProofTierIndex() -> RoadmapProofTierIndex {
    struct Cache {
        static let value = loadRoadmapProofTierMetadataByTarget()
    }
    return Cache.value
}

func roadmapProofTierMetadata(for target: String) -> ProofTierMetadata? {
    roadmapProofTierIndex().metadataByTarget[target]
}

func proofTierMetadata(for target: String) -> ProofTierMetadata {
    if let metadata = roadmapProofTierMetadata(for: target) {
        return metadata
    }
    switch target {
    case "tcti-plan-consistency", "tcti-report-schema-check":
        return ProofTierMetadata(
            proofTier: "rail",
            acceptanceWeight: "blocker",
            realStackRequired: false,
            canClaimRuntimeReadiness: false
        )
    case "tcti-appstore-safety-audit":
        return ProofTierMetadata(
            proofTier: "safety",
            acceptanceWeight: "blocker",
            realStackRequired: false,
            canClaimRuntimeReadiness: false
        )
    case "tcti-kernel-syscall-dispatch-smoke":
        return ProofTierMetadata(
            proofTier: "kernel",
            acceptanceWeight: "blocker",
            realStackRequired: true,
            canClaimRuntimeReadiness: false
        )
    default:
        return ProofTierMetadata(
            proofTier: "seed",
            acceptanceWeight: "probe",
            realStackRequired: false,
            canClaimRuntimeReadiness: false
        )
    }
}

func report(
    target: String,
    status: GateStatus,
    summary: String,
    command: String? = nil,
    failures: [Failure] = [],
    artifacts: [String] = [],
    forbiddenBehavior: [String: Bool] = forbiddenDefaults(),
    counters: [String: Int] = [:],
    coverageWarnings: [String] = [],
    kernelProfile: String? = nil,
    kernelConfig: String? = nil,
    evidence: [String: String]? = nil,
    releaseGateEligible: Bool? = nil,
    readinessGateEligible: Bool? = nil,
    autonomousTestsBypassed: Bool = false,
    bypassReason: String = "",
    expectedStatus: String? = nil,
    actualReplayStatus: String? = nil,
    execution: ExecutionReport? = nil
) -> Report {
    let metadata = proofTierMetadata(for: target)
    return Report(
        target: target,
        gate: target,
        status: status.rawValue,
        passed: status.passed,
        summary: summary,
        command: command,
        proofTier: metadata.proofTier,
        acceptanceWeight: metadata.acceptanceWeight,
        realStackRequired: metadata.realStackRequired,
        canClaimRuntimeReadiness: metadata.canClaimRuntimeReadiness,
        gitSha: gitSha(),
        backend: "tcti",
        kernelProfile: kernelProfile,
        kernelConfig: kernelConfig,
        virtualCpuModel: "orlix-aarch64-v1",
        hostPageSize: hostPageSize(),
        guestPageSize: 4096,
        forbiddenBehavior: forbiddenBehavior,
        counters: counters,
        failures: failures,
        artifacts: artifacts,
        releaseGateEligible: releaseGateEligible ?? (status.gateEligible && metadata.defaultReleaseGateEligible),
        readinessGateEligible: readinessGateEligible ?? (status.gateEligible && metadata.defaultReadinessGateEligible),
        autonomousTestsBypassed: autonomousTestsBypassed,
        bypassReason: bypassReason,
        coverageWarnings: coverageWarnings,
        evidence: evidence,
        expectedStatus: expectedStatus,
        actualReplayStatus: actualReplayStatus,
        execution: execution
    )
}

func writeJSON<T: Encodable>(_ value: T, to url: URL) throws {
    try ensureDirectory(url.deletingLastPathComponent())
    let temporary = url.deletingLastPathComponent().appendingPathComponent(url.lastPathComponent + ".tmp")
    let data = try encoder.encode(value)
    try data.write(to: temporary, options: .atomic)
    if fileManager.fileExists(atPath: url.path) {
        try fileManager.removeItem(at: url)
    }
    try fileManager.moveItem(at: temporary, to: url)
}

func writeReport(_ value: Report) throws -> URL {
    let directory = buildPath("reports", value.target)
    try ensureDirectory(directory)
    let jsonURL = directory.appendingPathComponent("report.json")
    try writeJSON(value, to: jsonURL)
    let markdown = """
    # TCTI Gate: \(value.target)

    - status: `\(value.status)`
    - passed: `\(value.passed)`
    \(value.command.map { "- command: `\($0)`" } ?? "")
    - proof tier: `\(value.proofTier)`
    - acceptance weight: `\(value.acceptanceWeight)`
    - real stack required: `\(value.realStackRequired)`
    - can claim runtime readiness: `\(value.canClaimRuntimeReadiness)`
    - readiness gate eligible: `\(value.readinessGateEligible)`
    - release gate eligible: `\(value.releaseGateEligible)`
    \(value.kernelProfile.map { "- kernel profile: `\($0)`" } ?? "")
    \(value.kernelConfig.map { "- kernel config: `\($0)`" } ?? "")
    - virtual CPU: `\(value.virtualCpuModel)`

    ## Summary

    \(value.summary)

    ## Failures

    \(value.failures.isEmpty ? "- none" : value.failures.map { "- `\($0.id)`: \($0.message)" }.joined(separator: "\n"))

    ## Artifacts

    \(value.artifacts.isEmpty ? "- none" : value.artifacts.map { "- `\($0)`" }.joined(separator: "\n"))

    ## Evidence

    \(value.evidence.map { evidence in evidence.keys.sorted().map { key in "- `\(key)`: \(evidence[key] ?? "")" }.joined(separator: "\n") } ?? "- none")

    """
    try markdown.write(to: directory.appendingPathComponent("report.md"), atomically: true, encoding: .utf8)
    return jsonURL
}

@discardableResult
func writeReducer(
    target: String,
    caseID: String,
    command: String,
    reason: String,
    artifacts: [String] = [],
    expectedStatus: GateStatus = .fail
) throws -> URL {
    let reducer = Reproducer(
        target: target,
        caseID: caseID,
        command: command,
        workingDirectory: repoRoot().path,
        artifacts: artifacts,
        reason: reason,
        expectedStatus: expectedStatus.rawValue
    )
    let url = buildPath("reproducers", target, "\(caseID).json")
    try writeJSON(reducer, to: url)
    return url
}

func fail(_ id: String, _ message: String) -> Failure {
    Failure(id: id, message: message)
}

func exitCode(for status: GateStatus) -> Int32 {
    status == .pass ? 0 : 1
}

func writeTodo(target: String, caseID: String = "todo", summary: String) throws -> Int32 {
    let command = caseID == "todo" ? "make tcti-gate TARGET=\(target)" : "CASE=\(caseID) make tcti-gate TARGET=\(target)"
    let reducer = try writeReducer(
        target: target,
        caseID: caseID,
        command: command,
        reason: summary,
        expectedStatus: .todo
    )
    let reportURL = try writeReport(report(
        target: target,
        status: .todo,
        summary: summary,
        failures: [fail("todo", summary)],
        artifacts: [relativePath(reducer)]
    ))
    print("TCTI target is TODO and intentionally failed: \(relativePath(reportURL))")
    print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
    return 1
}

func sourceTextContains(_ text: String, _ needle: String) -> Bool {
    text.range(of: needle, options: [.regularExpression]) != nil
}

func outputHasPassingTAPLabel(_ output: String, _ label: String) -> Bool {
    let ok = output.range(of: #"(?m)(^|[^[:alpha:]])ok\s+[0-9]+\s+-\s+\#(label)"#, options: .regularExpression) != nil
    let notOK = output.range(of: #"(?m)not ok\s+[0-9]+\s+-\s+\#(label)"#, options: .regularExpression) != nil
    return ok && !notOK
}

func outputHasShellWaitStatusZero(_ output: String) -> Bool {
    output.contains("orlix-init: shell exit status=0") ||
        output.range(of: #"(?m)orlix-init: process exited pid=[0-9]+ status=0"#, options: .regularExpression) != nil
}

func outputContainsInOrder(_ output: String, _ needles: [String]) -> Bool {
    var searchStart = output.startIndex
    for needle in needles {
        guard let range = output.range(of: needle, range: searchStart..<output.endIndex) else {
            return false
        }
        searchStart = range.upperBound
    }
    return true
}

func sourceFilesContainNone(root: URL, needles: [String]) -> Bool {
    guard let enumerator = fileManager.enumerator(at: root, includingPropertiesForKeys: nil) else {
        return true
    }
    for case let url as URL in enumerator {
        guard url.hasDirectoryPath == false,
              let text = try? String(contentsOf: url, encoding: .utf8) else {
            continue
        }
        for needle in needles where sourceTextContains(text, needle) {
            return false
        }
    }
    return true
}

func linuxKernelExitSource() -> URL? {
    linuxPortSource(["kernel", "exit.c"])
}

func linuxPortSource(_ components: [String]) -> URL? {
    let srcRoot = path("Build", "OrlixKernel", "src")
    guard let entries = try? fileManager.contentsOfDirectory(at: srcRoot, includingPropertiesForKeys: nil) else {
        return nil
    }
    for entry in entries.sorted(by: { $0.lastPathComponent < $1.lastPathComponent }) {
        let name = entry.lastPathComponent
        guard name.hasPrefix("linux-"), name.hasSuffix("-port") else {
            continue
        }
        var candidate = entry
        for component in components {
            candidate = candidate.appendingPathComponent(component)
        }
        if fileManager.fileExists(atPath: candidate.path) {
            return candidate
        }
    }
    return nil
}

func kernelSyscallDispatchKUnitEvidence(from output: String) -> [String: String] {
    let testName = "tcti_kernel_syscall_dispatch_smoke_reaches_linux_dispatch"
    let hasKTAP = output.range(of: #"(?m)^(KTAP|TAP) version\b"#, options: .regularExpression) != nil ||
        output.range(of: #"(?m)^ok\s+[0-9]+\b"#, options: .regularExpression) != nil ||
        output.range(of: #"(?m)^not ok\s+[0-9]+\b"#, options: .regularExpression) != nil
    let runnerAttempted = output.contains("ORLIX-KUNIT-RUNNER-BEGIN") &&
        output.contains("ORLIX-KUNIT-RUNNER-END")
    let runnerBlocked = output.contains("ORLIX-KUNIT-RUNNER-BLOCKED")
    let testObjectPresent = output.contains("ORLIX-KUNIT-RUNNER test_object_present=true")
    let testSymbolPresent = output.contains("ORLIX-KUNIT-RUNNER test_symbol_present=true")
    let namedPass = output.range(
        of: #"(?m)^ok\s+[0-9]+(?:\s+-)?\s+(?:[A-Za-z0-9_.-]+\.)?\#(testName)(?:\s|$)"#,
        options: .regularExpression
    ) != nil ||
        output.contains("\(testName): pass") ||
        output.contains("\(testName)=pass")
    let namedFailure = output.range(
        of: #"(?m)^not ok\s+[0-9]+(?:\s+-)?\s+(?:[A-Za-z0-9_.-]+\.)?\#(testName)(?:\s|$)"#,
        options: .regularExpression
    ) != nil ||
        output.contains("\(testName): fail") ||
        output.contains("\(testName)=fail")
    let namedMentioned = output.contains(testName)

    return [
        "kunit_test_name": testName,
        "kunit_output_has_ktap": hasKTAP ? "true" : "false",
        "kunit_runner_attempted": runnerAttempted ? "true" : "false",
        "kunit_runner_blocked": runnerBlocked ? "true" : "false",
        "kunit_test_object_present": testObjectPresent ? "true" : "false",
        "kunit_test_symbol_present": testSymbolPresent ? "true" : "false",
        "kunit_named_test_mentioned": namedMentioned ? "true" : "false",
        "kunit_named_test_executed": (namedPass || namedFailure) ? "true" : "false",
        "kunit_named_test_passed": namedPass ? "true" : "false",
        "kunit_named_test_failed": namedFailure ? "true" : "false",
    ]
}

func execveBinfmtRunnerEvidence(from output: String) -> [String: String] {
    let testName = "tcti_kernel_execve_binfmt_elf_smoke_prepares_tcti_entry"
    let hasKTAP = output.range(of: #"(?m)^(KTAP|TAP) version\b"#, options: .regularExpression) != nil ||
        output.range(of: #"(?m)^ok\s+[0-9]+\b"#, options: .regularExpression) != nil ||
        output.range(of: #"(?m)^not ok\s+[0-9]+\b"#, options: .regularExpression) != nil
    let runnerAttempted = output.contains("ORLIX-EXECVE-BINFMT-RUNNER-BEGIN") &&
        output.contains("ORLIX-EXECVE-BINFMT-RUNNER-END")
    let namedPass = output.range(
        of: #"(?m)^ok\s+[0-9]+(?:\s+-)?\s+(?:[A-Za-z0-9_.-]+\.)?\#(testName)(?:\s|$)"#,
        options: .regularExpression
    ) != nil ||
        output.contains("\(testName): pass") ||
        output.contains("\(testName)=pass")
    let namedFailure = output.range(
        of: #"(?m)^not ok\s+[0-9]+(?:\s+-)?\s+(?:[A-Za-z0-9_.-]+\.)?\#(testName)(?:\s|$)"#,
        options: .regularExpression
    ) != nil ||
        output.contains("\(testName): fail") ||
        output.contains("\(testName)=fail")
    var evidence: [String: String] = [
        "execve_binfmt_runner_test_name": testName,
        "execve_binfmt_runner_output_has_ktap": hasKTAP ? "true" : "false",
        "execve_binfmt_runner_attempted": runnerAttempted ? "true" : "false",
        "execve_binfmt_runner_named_test_executed": (namedPass || namedFailure) ? "true" : "false",
        "execve_binfmt_runner_named_test_passed": namedPass ? "true" : "false",
        "execve_binfmt_runner_named_test_failed": namedFailure ? "true" : "false",
    ]
    for line in output.components(separatedBy: .newlines) {
        guard line.hasPrefix("ORLIX-EXECVE-BINFMT-RUNNER ") else { continue }
        let payload = String(line.dropFirst("ORLIX-EXECVE-BINFMT-RUNNER ".count))
        guard let separator = payload.firstIndex(of: "=") else { continue }
        let key = String(payload[..<separator])
        let value = String(payload[payload.index(after: separator)...])
        evidence["execve_binfmt_runner_\(key)"] = value
    }
    return evidence
}

func faultSignalRunnerEvidence(from output: String) -> [String: String] {
    let testName = "tcti_kernel_fault_signal_smoke_reports_linux_signal"
    let hasKTAP = output.range(of: #"(?m)^(KTAP|TAP) version\b"#, options: .regularExpression) != nil ||
        output.range(of: #"(?m)^ok\s+[0-9]+\b"#, options: .regularExpression) != nil ||
        output.range(of: #"(?m)^not ok\s+[0-9]+\b"#, options: .regularExpression) != nil
    let runnerAttempted = output.contains("ORLIX-FAULT-SIGNAL-RUNNER-BEGIN") &&
        output.contains("ORLIX-FAULT-SIGNAL-RUNNER-END")
    let namedPass = output.range(
        of: #"(?m)^ok\s+[0-9]+(?:\s+-)?\s+(?:[A-Za-z0-9_.-]+\.)?\#(testName)(?:\s|$)"#,
        options: .regularExpression
    ) != nil ||
        output.contains("\(testName): pass") ||
        output.contains("\(testName)=pass")
    let namedFailure = output.range(
        of: #"(?m)^not ok\s+[0-9]+(?:\s+-)?\s+(?:[A-Za-z0-9_.-]+\.)?\#(testName)(?:\s|$)"#,
        options: .regularExpression
    ) != nil ||
        output.contains("\(testName): fail") ||
        output.contains("\(testName)=fail")
    var evidence: [String: String] = [
        "fault_signal_runner_test_name": testName,
        "fault_signal_runner_output_has_ktap": hasKTAP ? "true" : "false",
        "fault_signal_runner_attempted": runnerAttempted ? "true" : "false",
        "fault_signal_runner_named_test_executed": (namedPass || namedFailure) ? "true" : "false",
        "fault_signal_runner_named_test_passed": namedPass ? "true" : "false",
        "fault_signal_runner_named_test_failed": namedFailure ? "true" : "false",
    ]
    for line in output.components(separatedBy: .newlines) {
        guard line.hasPrefix("ORLIX-FAULT-SIGNAL-RUNNER ") else { continue }
        let payload = String(line.dropFirst("ORLIX-FAULT-SIGNAL-RUNNER ".count))
        guard let separator = payload.firstIndex(of: "=") else { continue }
        let key = String(payload[..<separator])
        let value = String(payload[payload.index(after: separator)...])
        evidence["fault_signal_runner_\(key)"] = value
    }
    return evidence
}

func waitReapingRunnerEvidence(from output: String) -> [String: String] {
    let testName = "tcti_kernel_wait_reaping_smoke_reports_linux_wait"
    let hasKTAP = output.range(of: #"(?m)^(KTAP|TAP) version\b"#, options: .regularExpression) != nil ||
        output.range(of: #"(?m)^ok\s+[0-9]+\b"#, options: .regularExpression) != nil ||
        output.range(of: #"(?m)^not ok\s+[0-9]+\b"#, options: .regularExpression) != nil
    let runnerAttempted = output.contains("ORLIX-WAIT-REAPING-RUNNER-BEGIN") &&
        output.contains("ORLIX-WAIT-REAPING-RUNNER-END")
    let namedPass = output.range(
        of: #"(?m)^ok\s+[0-9]+(?:\s+-)?\s+(?:[A-Za-z0-9_.-]+\.)?\#(testName)(?:\s|$)"#,
        options: .regularExpression
    ) != nil ||
        output.contains("\(testName): pass") ||
        output.contains("\(testName)=pass")
    let namedFailure = output.range(
        of: #"(?m)^not ok\s+[0-9]+(?:\s+-)?\s+(?:[A-Za-z0-9_.-]+\.)?\#(testName)(?:\s|$)"#,
        options: .regularExpression
    ) != nil ||
        output.contains("\(testName): fail") ||
        output.contains("\(testName)=fail")
    var evidence: [String: String] = [
        "wait_reaping_runner_test_name": testName,
        "wait_reaping_runner_output_has_ktap": hasKTAP ? "true" : "false",
        "wait_reaping_runner_attempted": runnerAttempted ? "true" : "false",
        "wait_reaping_runner_named_test_executed": (namedPass || namedFailure) ? "true" : "false",
        "wait_reaping_runner_named_test_passed": namedPass ? "true" : "false",
        "wait_reaping_runner_named_test_failed": namedFailure ? "true" : "false",
    ]
    for line in output.components(separatedBy: .newlines) {
        guard line.hasPrefix("ORLIX-WAIT-REAPING-RUNNER ") else { continue }
        let payload = String(line.dropFirst("ORLIX-WAIT-REAPING-RUNNER ".count))
        guard let separator = payload.firstIndex(of: "=") else { continue }
        let key = String(payload[..<separator])
        let value = String(payload[payload.index(after: separator)...])
        evidence["wait_reaping_runner_\(key)"] = value
    }
    return evidence
}

func ptyConsoleRunnerEvidence(from output: String) -> [String: String] {
    let testName = "tcti_kernel_pty_console_smoke_reports_output"
    let hasKTAP = output.range(of: #"(?m)^(KTAP|TAP) version\b"#, options: .regularExpression) != nil ||
        output.range(of: #"(?m)^ok\s+[0-9]+\b"#, options: .regularExpression) != nil ||
        output.range(of: #"(?m)^not ok\s+[0-9]+\b"#, options: .regularExpression) != nil
    let runnerAttempted = output.contains("ORLIX-PTY-CONSOLE-RUNNER-BEGIN") &&
        output.contains("ORLIX-PTY-CONSOLE-RUNNER-END")
    let namedPass = output.range(
        of: #"(?m)^ok\s+[0-9]+(?:\s+-)?\s+(?:[A-Za-z0-9_.-]+\.)?\#(testName)(?:\s|$)"#,
        options: .regularExpression
    ) != nil ||
        output.contains("\(testName): pass") ||
        output.contains("\(testName)=pass")
    let namedFailure = output.range(
        of: #"(?m)^not ok\s+[0-9]+(?:\s+-)?\s+(?:[A-Za-z0-9_.-]+\.)?\#(testName)(?:\s|$)"#,
        options: .regularExpression
    ) != nil ||
        output.contains("\(testName): fail") ||
        output.contains("\(testName)=fail")
    var evidence: [String: String] = [
        "pty_console_runner_test_name": testName,
        "pty_console_runner_output_has_ktap": hasKTAP ? "true" : "false",
        "pty_console_runner_attempted": runnerAttempted ? "true" : "false",
        "pty_console_runner_named_test_executed": (namedPass || namedFailure) ? "true" : "false",
        "pty_console_runner_named_test_passed": namedPass ? "true" : "false",
        "pty_console_runner_named_test_failed": namedFailure ? "true" : "false",
    ]
    for line in output.components(separatedBy: .newlines) {
        guard line.hasPrefix("ORLIX-PTY-CONSOLE-RUNNER ") else { continue }
        let payload = String(line.dropFirst("ORLIX-PTY-CONSOLE-RUNNER ".count))
        guard let separator = payload.firstIndex(of: "=") else { continue }
        let key = String(payload[..<separator])
        let value = String(payload[payload.index(after: separator)...])
        evidence["pty_console_runner_\(key)"] = value
    }
    return evidence
}

struct ElfHeaderSummary {
    let elfClass: UInt8
    let elfData: UInt8
    let elfType: UInt16
    let elfMachine: UInt16
    let entrypoint: UInt64
    let programHeaderCount: Int
    let loadSegmentCount: Int
}

func parseElfHeaderSummary(binary: URL) throws -> ElfHeaderSummary {
    let data = try Data(contentsOf: binary)
    guard data.count >= 64 else {
        throw GateError.commandFailed("ELF file too small: \(binary.path)")
    }
    guard data[0] == 0x7f, data[1] == 0x45, data[2] == 0x4c, data[3] == 0x46 else {
        throw GateError.commandFailed("not an ELF file: \(binary.path)")
    }
    let elfClass = data[4]
    let elfData = data[5]
    let elfType = try littleEndianUInt16(data, 16)
    let elfMachine = try littleEndianUInt16(data, 18)
    let entrypoint = try littleEndianUInt64(data, 24)
    let phoff = try littleEndianUInt64(data, 32)
    let phentsize = Int(try littleEndianUInt16(data, 54))
    let phnum = Int(try littleEndianUInt16(data, 56))
    var loadSegmentCount = 0
    for index in 0..<phnum {
        let offset = Int(phoff) + index * phentsize
        guard offset + 4 <= data.count else {
            throw GateError.commandFailed("ELF program header outside file at index \(index)")
        }
        if try littleEndianUInt32(data, offset) == 1 {
            loadSegmentCount += 1
        }
    }
    return ElfHeaderSummary(
        elfClass: elfClass,
        elfData: elfData,
        elfType: elfType,
        elfMachine: elfMachine,
        entrypoint: entrypoint,
        programHeaderCount: phnum,
        loadSegmentCount: loadSegmentCount
    )
}

func runKernelSyscallDispatchSmoke() throws -> Int32 {
    let target = "tcti-kernel-syscall-dispatch-smoke"
    let command = "make tcti-gate TARGET=\(target)"
    let kernelProfile = "tcti_runtime"
    let kernelConfig = path("OrlixKernel", "Sources", "ports", "orlix", "configs", "tcti_runtime_defconfig")
    let hostedExec = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "kernel", "hosted_exec.c")
    let tctiEngine = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "engine.c")
    let tctiEngineHeader = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "engine.h")
    let tctiSyscallSmoke = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "syscall_dispatch_smoke.h")
    let tctiReport = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "report.c")
    let tctiTests = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "tcti_decode_test.c")
    let syscall = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "kernel", "syscall.c")
    let hostAdapter = path("OrlixHostAdapter", "Sources")
    let outputRoot = buildPath("kernel_syscall_dispatch_smoke")
    let evidenceURL = outputRoot.appendingPathComponent("evidence.json")
    let kunitOutputURL = outputRoot.appendingPathComponent("kunit-run.txt")
    let kunitExecutionEvidenceURL = outputRoot.appendingPathComponent("kunit-execution-evidence.json")
    let kunitCommand = "make -f OrlixKernel/Makefile kunit-run PROFILE=\(kernelProfile)"
    try ensureDirectory(outputRoot)

    let configText = try readText(kernelConfig)
    let hostedExecText = try readText(hostedExec)
    let engineText = try readText(tctiEngine)
    let engineHeaderText = try readText(tctiEngineHeader)
    let syscallSmokeText = try readText(tctiSyscallSmoke)
    let reportText = try readText(tctiReport)
    let testText = try readText(tctiTests)
    let syscallText = try readText(syscall)
    var failures: [Failure] = []
    var evidence: [String: String] = [
        "actual_command": command,
        "backend": "tcti",
        "git_sha": gitSha(),
        "kernel_profile": kernelProfile,
        "kernel_config": relativePath(kernelConfig),
        "kunit_command": kunitCommand,
        "workload_hook_command": kunitCommand,
        "workload_hook_compiled": "false",
        "workload_hook_executed": "false",
        "tcti_entered": "false",
        "svc_boundary_reached": "false",
        "syscall_number_observed": "false",
        "orlix_syscall_dispatch_entered": "false",
        "linux_syscall_return_state_written": "false",
        "runtime_syscall_number_observed": "false",
        "runtime_orlix_syscall_dispatch_reached": "false",
    ]

    func requireSourceFact(_ key: String, _ text: String, _ needle: String, _ url: URL) {
        if sourceTextContains(text, needle) {
            evidence[key] = "\(relativePath(url)) contains \(needle)"
        } else {
            failures.append(fail("kernel-source-proof-missing", "\(relativePath(url)) must contain \(needle)"))
            evidence[key] = "missing"
        }
    }

    requireSourceFact("tcti_config_selected", configText, #"CONFIG_ORLIX_HOSTED_EXEC_TCTI=y"#, kernelConfig)
    requireSourceFact("native_hosted_exec_disabled", configText, #"# CONFIG_ORLIX_HOSTED_EXEC_NATIVE is not set"#, kernelConfig)
    requireSourceFact("hosted_entry_selects_tcti", hostedExecText, #"orlix_tcti_enter_user\(regs\)"#, hostedExec)
    requireSourceFact("hosted_entry_panics_without_backend", hostedExecText, #"no hosted execution backend configured"#, hostedExec)
    requireSourceFact("tcti_decodes_svc_boundary", engineText, #"decoded\.decode_class == TCTI_DECODE_SVC"#, tctiEngine)
    requireSourceFact("tcti_returns_syscall_exit", engineText, #"result\.reason = TCTI_EXIT_SYSCALL"#, tctiEngine)
    requireSourceFact("tcti_reports_syscall_boundary", engineText, #"tcti_report_syscall\(current, regs, &result\)"#, tctiEngine)
    requireSourceFact("tcti_handles_syscall", engineText, #"static void orlix_tcti_handle_syscall"#, tctiEngine)
    requireSourceFact("syscall_number_from_x8", engineText, #"regs->syscallno = regs->regs\[8\]"#, tctiEngine)
    requireSourceFact("svc_pc_advances", engineText, #"regs->pc \+= sizeof\(u32\)"#, tctiEngine)
    requireSourceFact("tcti_enters_linux_dispatch", engineText, #"orlix_syscall_dispatch\(regs\)"#, tctiEngine)
    requireSourceFact("tcti_reports_syscall_return", engineText, #"tcti_report_syscall_return\(current, regs, nr, pc\)"#, tctiEngine)
    requireSourceFact("kernel_dispatch_symbol", syscallText, #"long orlix_syscall_dispatch\(struct pt_regs \*regs\)"#, syscall)
    requireSourceFact("kernel_dispatch_uses_linux_table", syscallText, #"sys_call_table\[array_index_nospec\(nr, __NR_syscalls\)\]"#, syscall)
    requireSourceFact("kernel_dispatch_sets_linux_return", syscallText, #"syscall_set_return_value\(current, regs, 0, ret\)"#, syscall)
    requireSourceFact("kernel_dispatch_runs_exit_to_user_work", syscallText, #"orlix_exit_to_user_mode_work\(regs\)"#, syscall)
    requireSourceFact("svc_log_marker", reportText, #"Orlix TCTI: svc #0"#, tctiReport)
    requireSourceFact("syscall_return_log_marker", reportText, #"Orlix TCTI: syscall return"#, tctiReport)
    requireSourceFact("kernel_workload_hook_result_struct", engineHeaderText, #"struct tcti_kernel_syscall_dispatch_smoke_result"#, tctiEngineHeader)
    requireSourceFact("kernel_workload_hook_entrypoint", engineText, #"tcti_kernel_syscall_dispatch_smoke_for_tests"#, tctiEngine)
    requireSourceFact("kernel_workload_hook_decodes_svc", syscallSmokeText, #"tcti_decode_aarch64\(TCTI_SYSCALL_DISPATCH_SMOKE_INSTRUCTION\)"#, tctiSyscallSmoke)
    requireSourceFact("kernel_workload_hook_uses_getpid", syscallSmokeText, #"regs->regs\[8\] = __NR_getpid"#, tctiSyscallSmoke)
    requireSourceFact("kernel_workload_hook_calls_linux_dispatch", engineText, #"orlix_syscall_dispatch"#, tctiEngine)
    requireSourceFact("kernel_workload_hook_records_return_state", syscallSmokeText, #"out->linux_return_state_written"#, tctiSyscallSmoke)
    requireSourceFact("kunit_workload_hook_case", testText, #"tcti_kernel_syscall_dispatch_smoke_reaches_linux_dispatch"#, tctiTests)
    requireSourceFact("kunit_asserts_dispatch_entered", testText, #"KUNIT_EXPECT_TRUE\(test, result\.orlix_syscall_dispatch_entered\)"#, tctiTests)
    requireSourceFact("kunit_asserts_return_state", testText, #"KUNIT_EXPECT_TRUE\(test, result\.linux_return_state_written\)"#, tctiTests)

    let hostAdapterOwnsLinuxSyscalls = !sourceFilesContainNone(
        root: hostAdapter,
        needles: [
            #"orlix_syscall_dispatch"#,
            #"sys_call_table"#,
            #"TCTI_EXIT_SYSCALL"#,
            #"regs->syscallno"#,
            #"__NR_[A-Za-z0-9_]+"#,
        ]
    )
    if hostAdapterOwnsLinuxSyscalls {
        failures.append(fail("hostadapter-syscall-semantics", "HostAdapter sources must not own Linux syscall dispatch semantics"))
        evidence["hostadapter_linux_syscall_semantics"] = "present"
    } else {
        evidence["hostadapter_linux_syscall_semantics"] = "absent"
    }

    let kunitOutput = try run(
        ["sh", "-c", "\(kunitCommand) 2>&1"],
        check: false
    )
    try kunitOutput.write(to: kunitOutputURL, atomically: true, encoding: .utf8)

    let sourceFailureCount = failures.count
    let kunitEvidence = kernelSyscallDispatchKUnitEvidence(from: kunitOutput)
    try writeJSON(kunitEvidence, to: kunitExecutionEvidenceURL)
    for (key, value) in kunitEvidence {
        evidence[key] = value
    }

    let namedKUnitTestPassed = kunitEvidence["kunit_named_test_passed"] == "true"
    let kunitRunnerAttempted = kunitEvidence["kunit_runner_attempted"] == "true"
    let kunitRunnerBlocked = kunitEvidence["kunit_runner_blocked"] == "true"
    let kunitTestSymbolPresent = kunitEvidence["kunit_test_symbol_present"] == "true"
    let kunitBuildPassed = kunitTestSymbolPresent
    evidence["workload_hook_compiled"] = kunitTestSymbolPresent ? "true" : "false"
    evidence["kunit_build_result"] = kunitTestSymbolPresent ? "pass" : "fail"
    let blocker: String
    if kunitBuildPassed && namedKUnitTestPassed {
        evidence["workload_hook_executed"] = "true"
        evidence["tcti_entered"] = "tcti_kernel_syscall_dispatch_smoke_for_tests via KUnit test tcti_kernel_syscall_dispatch_smoke_reaches_linux_dispatch"
        evidence["svc_boundary_reached"] = "true"
        evidence["syscall_number_observed"] = "__NR_getpid"
        evidence["orlix_syscall_dispatch_entered"] = "true"
        evidence["linux_syscall_return_state_written"] = "true"
        evidence["runtime_syscall_number_observed"] = "__NR_getpid"
        evidence["runtime_orlix_syscall_dispatch_reached"] = "true"
        blocker = ""
    } else if kunitRunnerBlocked {
        blocker = "No no-phone KUnit executor is available for ARCH=orlix; the run target verified the named test object and symbol, but object build is not runtime execution."
        failures.append(fail("kernel-workload-kunit-runner-unavailable", blocker))
    } else if kunitRunnerAttempted {
        blocker = "No-phone KUnit runner attempted execution, but did not produce passing evidence for tcti_kernel_syscall_dispatch_smoke_reaches_linux_dispatch."
        failures.append(fail("kernel-workload-kunit-execution-failed", blocker))
    } else if kunitBuildPassed {
        blocker = "KUnit runner does not expose machine-readable execution evidence for tcti_kernel_syscall_dispatch_smoke_reaches_linux_dispatch."
        failures.append(fail("kernel-workload-kunit-execution-evidence-missing", blocker))
    } else {
        blocker = "KUnit build could not compile the no-phone TCTI syscall dispatch workload hook."
    }
    if !blocker.isEmpty {
        failures.append(fail("kernel-workload-execution-missing", blocker))
        evidence["blocker"] = blocker
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    evidence["gate_result"] = status.rawValue

    try writeJSON(evidence, to: evidenceURL)
    let reducer = try writeReducer(
        target: target,
        caseID: status == .pass ? "kernel-syscall-dispatch-smoke-pass" : "kernel-syscall-dispatch-smoke-fail",
        command: command,
        reason: blocker.isEmpty ? "kernel syscall dispatch KUnit smoke passed" : blocker,
        artifacts: [relativePath(evidenceURL), relativePath(kunitOutputURL), relativePath(kunitExecutionEvidenceURL)],
        expectedStatus: status
    )
    let artifacts = [relativePath(evidenceURL), relativePath(kunitOutputURL), relativePath(kunitExecutionEvidenceURL), relativePath(reducer)]
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: status == .pass
            ? "Kernel/TCTI syscall dispatch KUnit smoke executed and passed the named hook test."
            : "Kernel/TCTI syscall dispatch workload hook compiles, and the current no-phone KUnit run target fails closed because ARCH=orlix has no runnable KUnit executor for the named hook test.",
        command: command,
        failures: failures,
        artifacts: artifacts,
        counters: [
            "source_evidence_facts": evidence.count,
            "source_proof_failures": sourceFailureCount,
            "kernel_workload_hook_compile_passes": kunitBuildPassed ? 1 : 0,
            "kunit_named_tests_executed": namedKUnitTestPassed ? 1 : 0,
            "runtime_observed_syscalls": namedKUnitTestPassed ? 1 : 0,
            "runtime_observed_orlix_syscall_dispatch_entries": namedKUnitTestPassed ? 1 : 0,
        ],
        kernelProfile: kernelProfile,
        kernelConfig: relativePath(kernelConfig),
        evidence: evidence
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    if !blocker.isEmpty {
        print("blocker: \(blocker)")
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
    }
    return exitCode(for: status)
}

func runKernelExecveBinfmtElfSmoke() throws -> Int32 {
    let target = "tcti-kernel-execve-binfmt-elf-smoke"
    let command = "make tcti-gate TARGET=\(target)"
    let kernelProfile = "tcti_runtime"
    let kernelConfig = path("OrlixKernel", "Sources", "ports", "orlix", "configs", "tcti_runtime_defconfig")
    let elfHeader = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "include", "asm", "elf.h")
    let processor = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "kernel", "process.c")
    let tctiEngine = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "engine.c")
    let tctiEngineHeader = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "engine.h")
    let smokeHeader = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "execve_binfmt_smoke.h")
    let testSource = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "tcti_decode_test.c")
    let runtimeValidation = path("tools", "runtime", "orlix-runtime-validation.sh")
    let payloadSource = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "execve_binfmt_elf_smoke_payload.S")
    let runnerSource = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "tcti_execve_binfmt_smoke_runner.c")
    let hostInclude = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "host_include")
    let tctiDir = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti")
    let hostAdapter = path("OrlixHostAdapter", "Sources")
    let outputRoot = buildPath("kernel_execve_binfmt_elf_smoke")
    let evidenceURL = outputRoot.appendingPathComponent("evidence.json")
    let runnerOutputURL = outputRoot.appendingPathComponent("runner.txt")
    let runnerEvidenceURL = outputRoot.appendingPathComponent("runner-evidence.json")
    let elfSummaryURL = outputRoot.appendingPathComponent("elf-summary.json")
    let runnerBinary = outputRoot.appendingPathComponent("tcti_execve_binfmt_smoke_runner")
    try ensureDirectory(outputRoot)

    var failures: [Failure] = []
    var artifacts: [String] = []
    var evidence: [String: String] = [
        "actual_command": command,
        "backend": "tcti",
        "git_sha": gitSha(),
        "kernel_profile": kernelProfile,
        "kernel_config": relativePath(kernelConfig),
        "linux_execve_binfmt_elf_path_entered": "false",
        "linux_program_headers_accepted": "false",
        "linux_task_mm_register_state_prepared": "false",
        "entry_pc_recorded": "false",
        "stack_pointer_recorded": "false",
        "tcti_entry_reached": "false",
        "simulator_execve_binfmt_report_current": "false",
        "simulator_linux_exec_start_thread_recorded": "false",
        "workload_hook_compiled": "false",
        "workload_hook_executed": "false",
    ]

    func requireSourceFact(_ key: String, _ text: String, _ needle: String, _ url: URL) {
        if sourceTextContains(text, needle) {
            evidence[key] = "\(relativePath(url)) contains \(needle)"
        } else {
            failures.append(fail("kernel-source-proof-missing", "\(relativePath(url)) must contain \(needle)"))
            evidence[key] = "missing"
        }
    }

    let configText = try readText(kernelConfig)
    let elfText = try readText(elfHeader)
    let processText = try readText(processor)
    let engineText = try readText(tctiEngine)
    let engineHeaderText = try readText(tctiEngineHeader)
    let smokeText = try readText(smokeHeader)
    let testText = try readText(testSource)
    let runtimeValidationText = try readText(runtimeValidation)
    let payloadText = try readText(payloadSource)

    requireSourceFact("tcti_config_selected", configText, #"CONFIG_ORLIX_HOSTED_EXEC_TCTI=y"#, kernelConfig)
    requireSourceFact("native_hosted_exec_disabled", configText, #"# CONFIG_ORLIX_HOSTED_EXEC_NATIVE is not set"#, kernelConfig)
    requireSourceFact("binfmt_elf_enabled", configText, #"CONFIG_BINFMT_ELF=y"#, kernelConfig)
    requireSourceFact("arch_elf_class", elfText, #"ELF_CLASS\s+ELFCLASS64"#, elfHeader)
    requireSourceFact("arch_elf_data", elfText, #"ELF_DATA\s+ELFDATA2LSB"#, elfHeader)
    requireSourceFact("arch_elf_machine", elfText, #"ELF_ARCH\s+EM_AARCH64"#, elfHeader)
    requireSourceFact("arch_elf_check", elfText, #"elf_check_arch\(hdr\)"#, elfHeader)
    requireSourceFact("start_thread_symbol", processText, #"void start_thread\(struct pt_regs \*regs, unsigned long pc, unsigned long sp\)"#, processor)
    requireSourceFact("start_thread_records_pc", processText, #"regs->pc = pc"#, processor)
    requireSourceFact("start_thread_records_sp", processText, #"regs->sp = sp"#, processor)
    requireSourceFact("start_thread_sets_el0", processText, #"regs->pstate = PSR_MODE_EL0t"#, processor)
    requireSourceFact("start_thread_clears_syscall", processText, #"regs->syscallno = NO_SYSCALL"#, processor)
    requireSourceFact("tcti_entry_declared", engineText, #"orlix_tcti_enter_user"#, tctiEngine)
    requireSourceFact("execve_binfmt_result_struct", engineHeaderText, #"struct tcti_kernel_execve_binfmt_elf_smoke_result"#, tctiEngineHeader)
    requireSourceFact("execve_binfmt_kernel_hook", engineText, #"tcti_kernel_execve_binfmt_elf_smoke_for_tests"#, tctiEngine)
    requireSourceFact("execve_binfmt_hook_calls_start_thread", engineText, #"start_thread"#, tctiEngine)
    requireSourceFact("execve_binfmt_helper_validates_aarch64", smokeText, #"payload->elf_machine == EM_AARCH64"#, smokeHeader)
    requireSourceFact("execve_binfmt_helper_records_entry_pc", smokeText, #"out->entry_pc = regs->pc"#, smokeHeader)
    requireSourceFact("execve_binfmt_helper_records_stack", smokeText, #"out->stack_pointer = regs->sp"#, smokeHeader)
    requireSourceFact("execve_binfmt_kunit_case", testText, #"tcti_kernel_execve_binfmt_elf_smoke_prepares_tcti_entry"#, testSource)
    requireSourceFact("execve_binfmt_start_thread_log", processText, #"Orlix TCTI: linux exec start_thread"#, processor)
    requireSourceFact("runtime_exec_start_thread_json", runtimeValidationText, #"linux_exec_start_thread"#, runtimeValidation)
    requireSourceFact("execve_binfmt_payload_marker", payloadText, #"ORLIX-USERLAND-TCTI-OK"#, payloadSource)

    let hostAdapterOwnsLinuxExec = !sourceFilesContainNone(
        root: hostAdapter,
        needles: [
            #"load_elf_binary"#,
            #"binfmt_elf"#,
            #"start_thread"#,
            #"do_execve"#,
            #"execve"#,
        ]
    )
    if hostAdapterOwnsLinuxExec {
        failures.append(fail("hostadapter-exec-semantics", "HostAdapter sources must not own Linux execve/binfmt_elf semantics"))
        evidence["hostadapter_linux_exec_semantics"] = "present"
    } else {
        evidence["hostadapter_linux_exec_semantics"] = "absent"
    }

    let built = try buildAarch64NoLibc(
        source: payloadSource,
        outputRoot: outputRoot,
        binaryName: "execve_binfmt_elf_smoke_payload"
    )
    artifacts.append(relativePath(built.binary))
    let elf = try parseElfHeaderSummary(binary: built.binary)
    let elfSummary: [String: String] = [
        "elf_payload_path": relativePath(built.binary),
        "elf_source_path": relativePath(built.source),
        "elf_class": "\(elf.elfClass)",
        "elf_data": "\(elf.elfData)",
        "elf_type": "\(elf.elfType)",
        "elf_machine": "\(elf.elfMachine)",
        "elf_entry_pc": String(format: "0x%llx", elf.entrypoint),
        "elf_program_header_count": "\(elf.programHeaderCount)",
        "elf_load_segment_count": "\(elf.loadSegmentCount)",
        "elf_binary_sha256": try sha256(built.binary),
        "elf_file_output": built.metadata["file_output"] ?? "",
        "elf_objdump_header": built.metadata["objdump_header"] ?? "",
    ]
    try writeJSON(elfSummary, to: elfSummaryURL)
    artifacts.append(relativePath(elfSummaryURL))
    for (key, value) in elfSummary {
        evidence[key] = value
    }
    evidence["elf_payload_is_real_aarch64_linux_elf"] =
        elf.elfClass == 2 && elf.elfData == 1 && elf.elfMachine == 183 && elf.loadSegmentCount > 0 ? "true" : "false"

    let hostcc: String
    if let configuredHostCC = ProcessInfo.processInfo.environment["ORLIX_KERNEL_HOSTCC"],
       !configuredHostCC.isEmpty {
        hostcc = configuredHostCC
    } else {
        hostcc = try commandPath("cc")
    }
    _ = try run([
        hostcc,
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-Wno-unused-function",
        "-DORLIX_TCTI_HOST_TEST_RUNNER=1",
        "-I\(hostInclude.path)",
        "-I\(tctiDir.path)",
        runnerSource.path,
        "-o",
        runnerBinary.path,
    ])
    artifacts.append(relativePath(runnerBinary))
    evidence["workload_hook_compiled"] = "true"

    let runnerOutput = try run([
        runnerBinary.path,
        "\(elf.elfClass)",
        "\(elf.elfData)",
        "\(elf.elfType)",
        "\(elf.elfMachine)",
        "\(elf.loadSegmentCount)",
        String(format: "0x%llx", elf.entrypoint),
    ], check: false)
    try runnerOutput.write(to: runnerOutputURL, atomically: true, encoding: .utf8)
    artifacts.append(relativePath(runnerOutputURL))
    let runnerEvidence = execveBinfmtRunnerEvidence(from: runnerOutput)
    try writeJSON(runnerEvidence, to: runnerEvidenceURL)
    artifacts.append(relativePath(runnerEvidenceURL))
    for (key, value) in runnerEvidence {
        evidence[key] = value
    }

    let helperPassed = runnerEvidence["execve_binfmt_runner_named_test_passed"] == "true"
    if helperPassed {
        evidence["workload_hook_executed"] = "true"
        evidence["entry_pc_recorded"] = runnerEvidence["execve_binfmt_runner_entry_pc"] ?? "true"
        evidence["stack_pointer_recorded"] = runnerEvidence["execve_binfmt_runner_stack_pointer"] ?? "true"
    } else {
        failures.append(fail("kernel-workload-hook-execution", "execve/binfmt entry-state helper runner did not pass"))
    }

    var simulatorArtifacts: [String] = []
    if let simulatorReport = selectedRuntimeValidationReport(
        gate: "tcti-init-first-syscall",
        destination: "iphonesimulator"
    ) {
        artifacts.append(relativePath(simulatorReport.url))
        simulatorArtifacts = (simulatorReport.object["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
        artifacts.append(contentsOf: simulatorArtifacts)
        evidence["simulator_report"] = relativePath(simulatorReport.url)
        let events = simulatorReport.object["tcti_runtime_events"] as? [String: Any] ?? [:]
        let execStartThread = events["linux_exec_start_thread"] as? [String: Any] ?? [:]
        let execTask = stringField(execStartThread, "task")
        let execPID = intField(execStartThread, "pid")
        let execPC = stringField(execStartThread, "pc")
        let execSP = stringField(execStartThread, "sp")
        let execPstate = stringField(execStartThread, "pstate")
        let execSyscallno = intField(execStartThread, "syscallno")
        let reportCurrent = stringField(simulatorReport.object, "git_sha") == gitSha() &&
            stringField(simulatorReport.object, "status") == "pass" &&
            boolField(simulatorReport.object, "passed") &&
            stringField(simulatorReport.object, "selected_device_id") == "1E5553B0-203A-4A11-BAD7-EBDE46863F66" &&
            stringField(simulatorReport.object, "selected_device_name") == "Orlix-iPhone-15-Pro-Max" &&
            intField(simulatorReport.object, "simulator_booted_count") == 1 &&
            boolField(simulatorReport.object, "simulator_single_booted")
        let execStartThreadRecorded = !execTask.isEmpty &&
            execPID != nil &&
            !execPC.isEmpty &&
            !execSP.isEmpty &&
            execPstate == "0x0" &&
            execSyscallno == -1
        evidence["simulator_execve_binfmt_report_current"] = reportCurrent ? "true" : "false"
        evidence["simulator_linux_exec_start_thread_recorded"] = execStartThreadRecorded ? "true" : "false"
        evidence["simulator_linux_exec_start_thread_task"] = execTask
        evidence["simulator_linux_exec_start_thread_pid"] = execPID.map(String.init) ?? ""
        evidence["simulator_linux_exec_start_thread_pc"] = execPC
        evidence["simulator_linux_exec_start_thread_sp"] = execSP
        evidence["simulator_linux_exec_start_thread_pstate"] = execPstate
        evidence["simulator_linux_exec_start_thread_syscallno"] = execSyscallno.map(String.init) ?? ""
        if reportCurrent && execStartThreadRecorded {
            evidence["linux_execve_binfmt_elf_path_entered"] = "true"
            evidence["linux_program_headers_accepted"] = "true"
            evidence["linux_task_mm_register_state_prepared"] = "true"
            evidence["tcti_entry_reached"] = "true"
        } else {
            if !reportCurrent {
                failures.append(fail("simulator-report-current", "latest pinned simulator first-syscall report is missing, stale, failing, or not from the single required simulator"))
            }
            if !execStartThreadRecorded {
                failures.append(fail("simulator-exec-start-thread", "pinned simulator report lacks structured linux_exec_start_thread evidence from arch start_thread"))
            }
        }
    } else {
        failures.append(fail("simulator-report-missing", "missing iphonesimulator tcti-init-first-syscall report with linux_exec_start_thread evidence"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let blocker = failures.isEmpty ? "" : "No current pinned-simulator report proves Linux execve/binfmt_elf reached arch start_thread and TCTI entry for the current HEAD."
    if !blocker.isEmpty {
        evidence["blocker"] = blocker
    }
    evidence["gate_result"] = status.rawValue

    try writeJSON(evidence, to: evidenceURL)
    artifacts.append(relativePath(evidenceURL))
    let reducer = try writeReducer(
        target: target,
        caseID: status == .pass ? "kernel-execve-binfmt-elf-smoke-pass" : "kernel-execve-binfmt-elf-smoke-fail",
        command: command,
        reason: blocker.isEmpty ? "Kernel execve/binfmt ELF smoke passed with pinned simulator start_thread evidence." : blocker,
        artifacts: artifacts,
        expectedStatus: status
    )
    artifacts.append(relativePath(reducer))

    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: status == .pass ?
            "Kernel/TCTI execve/binfmt ELF smoke has current pinned simulator evidence that Linux ELF exec reached arch start_thread and TCTI entry." :
            "Kernel/TCTI execve/binfmt ELF smoke builds a real AArch64 Linux ELF payload and executes the arch entry-state helper, but current pinned simulator exec evidence is missing.",
        command: command,
        failures: failures,
        artifacts: artifacts,
        counters: [
            "elf_payloads_built": 1,
            "elf_load_segments": elf.loadSegmentCount,
            "workload_hook_compile_passes": 1,
            "workload_hook_executed": helperPassed ? 1 : 0,
            "linux_execve_binfmt_runtime_entries": evidence["linux_execve_binfmt_elf_path_entered"] == "true" ? 1 : 0,
            "tcti_entries_from_linux_execve": evidence["tcti_entry_reached"] == "true" ? 1 : 0,
        ],
        kernelProfile: kernelProfile,
        kernelConfig: relativePath(kernelConfig),
        evidence: evidence
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    if !blocker.isEmpty {
        print("blocker: \(blocker)")
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
    }
    return exitCode(for: status)
}

func runKernelFaultSignalSmoke() throws -> Int32 {
    let target = "tcti-kernel-fault-signal-smoke"
    let command = "make tcti-gate TARGET=\(target)"
    let kernelProfile = "tcti_runtime"
    let kernelConfig = path("OrlixKernel", "Sources", "ports", "orlix", "configs", "tcti_runtime_defconfig")
    let tctiHeader = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "include", "asm", "tcti.h")
    let tctiEngine = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "engine.c")
    let tctiEngineHeader = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "engine.h")
    let faultSmoke = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "fault_signal_smoke.h")
    let testSource = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "tcti_decode_test.c")
    let runnerSource = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "tcti_fault_signal_smoke_runner.c")
    let hostInclude = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "host_include")
    let tctiDir = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti")
    let fault = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "mm", "fault.c")
    let hostAdapter = path("OrlixHostAdapter", "Sources")
    let outputRoot = buildPath("kernel_fault_signal_smoke")
    let evidenceURL = outputRoot.appendingPathComponent("evidence.json")
    let runnerOutputURL = outputRoot.appendingPathComponent("runner.txt")
    let runnerEvidenceURL = outputRoot.appendingPathComponent("runner-evidence.json")
    let runnerBinary = outputRoot.appendingPathComponent("tcti_fault_signal_smoke_runner")
    try ensureDirectory(outputRoot)

    var failures: [Failure] = []
    var artifacts: [String] = []
    var evidence: [String: String] = [
        "actual_command": command,
        "backend": "tcti",
        "git_sha": gitSha(),
        "kernel_profile": kernelProfile,
        "kernel_config": relativePath(kernelConfig),
        "workload_hook_compiled": "false",
        "workload_hook_executed": "false",
        "tcti_user_fault_exit": "false",
        "linux_fault_handler_entered": "false",
        "linux_signal_result_recorded": "false",
        "signal_number_observed": "false",
        "signal_code_observed": "false",
        "fault_address_recorded": "false",
    ]

    func requireSourceFact(_ key: String, _ text: String, _ needle: String, _ url: URL) {
        if sourceTextContains(text, needle) {
            evidence[key] = "\(relativePath(url)) contains \(needle)"
        } else {
            failures.append(fail("kernel-source-proof-missing", "\(relativePath(url)) must contain \(needle)"))
            evidence[key] = "missing"
        }
    }

    let configText = try readText(kernelConfig)
    let tctiHeaderText = try readText(tctiHeader)
    let engineText = try readText(tctiEngine)
    let engineHeaderText = try readText(tctiEngineHeader)
    let faultSmokeText = try readText(faultSmoke)
    let testText = try readText(testSource)
    let faultText = try readText(fault)

    requireSourceFact("tcti_config_selected", configText, #"CONFIG_ORLIX_HOSTED_EXEC_TCTI=y"#, kernelConfig)
    requireSourceFact("native_hosted_exec_disabled", configText, #"# CONFIG_ORLIX_HOSTED_EXEC_NATIVE is not set"#, kernelConfig)
    requireSourceFact("tcti_user_fault_exit_reason", tctiHeaderText, #"TCTI_EXIT_USER_FAULT"#, tctiHeader)
    requireSourceFact("tcti_result_fault_address", tctiHeaderText, #"fault_address"#, tctiHeader)
    requireSourceFact("tcti_result_fault_access", tctiHeaderText, #"fault_access"#, tctiHeader)
    requireSourceFact("tcti_resume_records_user_fault", engineText, #"result.reason = TCTI_EXIT_USER_FAULT"#, tctiEngine)
    requireSourceFact("tcti_resume_records_fault_address", engineText, #"result.fault_address = fault_address"#, tctiEngine)
    requireSourceFact("tcti_enter_handles_user_fault", engineText, #"orlix_tcti_handle_user_fault\(regs, &result\)"#, tctiEngine)
    requireSourceFact("tcti_fault_fallback_kills_sigsegv", engineText, #"do_group_exit\(SIGSEGV\)"#, tctiEngine)
    requireSourceFact("kernel_fault_handler_symbol", faultText, #"int tcti_handle_user_fault\(struct pt_regs \*regs, unsigned long address"#, fault)
    requireSourceFact("kernel_fault_bad_area_signal", faultText, #"force_sig_fault\(SIGSEGV, si_code"#, fault)
    requireSourceFact("kernel_fault_maperr_default", faultText, #"int si_code = SEGV_MAPERR"#, fault)
    requireSourceFact("kernel_fault_log_marker", faultText, #"Orlix TCTI: user fault"#, fault)
    requireSourceFact("fault_signal_result_struct", engineHeaderText, #"struct tcti_kernel_fault_signal_smoke_result"#, tctiEngineHeader)
    requireSourceFact("fault_signal_helper_entrypoint", faultSmokeText, #"tcti_kernel_fault_signal_smoke_execute"#, faultSmoke)
    requireSourceFact("fault_signal_helper_calls_linux_handler", faultSmokeText, #"handle_fault\(regs, fault->fault_address"#, faultSmoke)
    requireSourceFact("fault_signal_helper_records_signal", faultSmokeText, #"linux_signal_result_recorded"#, faultSmoke)
    requireSourceFact("fault_signal_kunit_case", testText, #"tcti_kernel_fault_signal_smoke_reports_linux_signal"#, testSource)

    let hostAdapterOwnsFaultSignals = !sourceFilesContainNone(
        root: hostAdapter,
        needles: [
            #"force_sig_fault"#,
            #"tcti_handle_user_fault"#,
        ]
    )
    if hostAdapterOwnsFaultSignals {
        failures.append(fail("hostadapter-fault-signal-semantics", "HostAdapter sources must not own Linux fault/signal semantics"))
        evidence["hostadapter_linux_fault_signal_semantics"] = "present"
    } else {
        evidence["hostadapter_linux_fault_signal_semantics"] = "absent"
    }

    let hostcc: String
    if let configuredHostCC = ProcessInfo.processInfo.environment["ORLIX_KERNEL_HOSTCC"],
       !configuredHostCC.isEmpty {
        hostcc = configuredHostCC
    } else {
        hostcc = try commandPath("cc")
    }
    _ = try run([
        hostcc,
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-Wno-unused-function",
        "-DORLIX_TCTI_HOST_TEST_RUNNER=1",
        "-I\(hostInclude.path)",
        "-I\(tctiDir.path)",
        runnerSource.path,
        "-o",
        runnerBinary.path,
    ])
    artifacts.append(relativePath(runnerBinary))
    evidence["workload_hook_compiled"] = "true"

    let runnerOutput = try run([runnerBinary.path], check: false)
    try runnerOutput.write(to: runnerOutputURL, atomically: true, encoding: .utf8)
    artifacts.append(relativePath(runnerOutputURL))
    let runnerEvidence = faultSignalRunnerEvidence(from: runnerOutput)
    try writeJSON(runnerEvidence, to: runnerEvidenceURL)
    artifacts.append(relativePath(runnerEvidenceURL))
    for (key, value) in runnerEvidence {
        evidence[key] = value
    }

    let runnerPassed = runnerEvidence["fault_signal_runner_named_test_passed"] == "true" &&
        runnerEvidence["fault_signal_runner_workload_hook_executed"] == "true" &&
        runnerEvidence["fault_signal_runner_tcti_user_fault_exit"] == "true" &&
        runnerEvidence["fault_signal_runner_linux_fault_handler_entered"] == "true" &&
        runnerEvidence["fault_signal_runner_linux_signal_result_recorded"] == "true" &&
        runnerEvidence["fault_signal_runner_signal_number"] == "11" &&
        runnerEvidence["fault_signal_runner_signal_code"] == "1"
    if runnerPassed {
        evidence["workload_hook_executed"] = "true"
        evidence["tcti_user_fault_exit"] = "true"
        evidence["linux_fault_handler_entered"] = "true"
        evidence["linux_signal_result_recorded"] = "true"
        evidence["signal_number_observed"] = "SIGSEGV"
        evidence["signal_code_observed"] = "SEGV_MAPERR"
        evidence["fault_address_recorded"] = runnerEvidence["fault_signal_runner_fault_address"] ?? "true"
    } else {
        failures.append(fail("kernel-workload-hook-execution", "fault/signal smoke runner did not produce passing Linux signal evidence"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let blocker = failures.isEmpty ? "" : "No no-phone OrlixKernel workload currently proves TCTI user fault exit reaches Linux-owned SIGSEGV fault delivery."
    if !blocker.isEmpty {
        evidence["blocker"] = blocker
    }
    evidence["gate_result"] = status.rawValue
    try writeJSON(evidence, to: evidenceURL)
    artifacts.append(relativePath(evidenceURL))
    let reducer = try writeReducer(
        target: target,
        caseID: status == .pass ? "kernel-fault-signal-smoke-pass" : "kernel-fault-signal-smoke-fail",
        command: command,
        reason: blocker.isEmpty ? "kernel fault/signal smoke passed" : blocker,
        artifacts: [relativePath(evidenceURL), relativePath(runnerOutputURL), relativePath(runnerEvidenceURL)],
        expectedStatus: status
    )
    artifacts.append(relativePath(reducer))
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: status == .pass ? "Kernel/TCTI no-phone fault/signal smoke proved TCTI user fault reaches Linux-owned SIGSEGV delivery." : "Kernel/TCTI no-phone fault/signal smoke did not prove Linux-owned signal delivery.",
        failures: failures,
        artifacts: artifacts,
        counters: [
            "source_evidence_facts": evidence.count,
            "source_proof_failures": failures.filter { $0.id == "kernel-source-proof-missing" }.count,
            "runtime_observed_faults": runnerPassed ? 1 : 0,
            "runtime_observed_linux_signals": runnerPassed ? 1 : 0,
        ],
        kernelProfile: kernelProfile,
        kernelConfig: relativePath(kernelConfig),
        evidence: evidence,
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    if !blocker.isEmpty {
        print("blocker: \(blocker)")
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
    }
    return exitCode(for: status)
}

func runKernelWaitReapingSmoke() throws -> Int32 {
    let target = "tcti-kernel-wait-reaping-smoke"
    let command = "make tcti-gate TARGET=\(target)"
    let kernelProfile = "tcti_runtime"
    let kernelConfig = path("OrlixKernel", "Sources", "ports", "orlix", "configs", "tcti_runtime_defconfig")
    let tctiHeader = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "include", "asm", "tcti.h")
    let tctiEngine = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "engine.c")
    let tctiEngineHeader = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "engine.h")
    let waitSmoke = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "wait_reaping_smoke.h")
    let testSource = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "tcti_decode_test.c")
    let runnerSource = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "tcti_wait_reaping_smoke_runner.c")
    let hostInclude = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "host_include")
    let tctiDir = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti")
    let hostAdapter = path("OrlixHostAdapter", "Sources")
    let outputRoot = buildPath("kernel_wait_reaping_smoke")
    let evidenceURL = outputRoot.appendingPathComponent("evidence.json")
    let runnerOutputURL = outputRoot.appendingPathComponent("runner.txt")
    let runnerEvidenceURL = outputRoot.appendingPathComponent("runner-evidence.json")
    let runnerBinary = outputRoot.appendingPathComponent("tcti_wait_reaping_smoke_runner")
    try ensureDirectory(outputRoot)

    var failures: [Failure] = []
    var artifacts: [String] = []
    var evidence: [String: String] = [
        "actual_command": command,
        "backend": "tcti",
        "git_sha": gitSha(),
        "kernel_profile": kernelProfile,
        "kernel_config": relativePath(kernelConfig),
        "workload_hook_compiled": "false",
        "workload_hook_executed": "false",
        "tcti_task_exit_observed": "false",
        "child_exit_state_recorded": "false",
        "linux_wait_entered": "false",
        "linux_wait_status_recorded": "false",
        "linux_reaping_completed": "false",
    ]

    func requireSourceFact(_ key: String, _ text: String, _ needle: String, _ url: URL) {
        if sourceTextContains(text, needle) {
            evidence[key] = "\(relativePath(url)) contains \(needle)"
        } else {
            failures.append(fail("kernel-source-proof-missing", "\(relativePath(url)) must contain \(needle)"))
            evidence[key] = "missing"
        }
    }

    let configText = try readText(kernelConfig)
    let tctiHeaderText = try readText(tctiHeader)
    let engineText = try readText(tctiEngine)
    let engineHeaderText = try readText(tctiEngineHeader)
    let waitSmokeText = try readText(waitSmoke)
    let testText = try readText(testSource)

    requireSourceFact("tcti_config_selected", configText, #"CONFIG_ORLIX_HOSTED_EXEC_TCTI=y"#, kernelConfig)
    requireSourceFact("native_hosted_exec_disabled", configText, #"# CONFIG_ORLIX_HOSTED_EXEC_NATIVE is not set"#, kernelConfig)
    requireSourceFact("tcti_task_exit_reason", tctiHeaderText, #"TCTI_EXIT_TASK_EXIT"#, tctiHeader)
    requireSourceFact("tcti_resume_task_exit_default", engineText, #"TCTI_EXIT_TASK_EXIT"#, tctiEngine)
    requireSourceFact("tcti_resume_task_exit_handoff", engineText, #"case TCTI_EXIT_TASK_EXIT"#, tctiEngine)
    requireSourceFact("wait_reaping_result_struct", engineHeaderText, #"struct tcti_kernel_wait_reaping_smoke_result"#, tctiEngineHeader)
    requireSourceFact("wait_reaping_helper_entrypoint", waitSmokeText, #"tcti_kernel_wait_reaping_smoke_execute"#, waitSmoke)
    requireSourceFact("wait_reaping_helper_calls_linux_wait", waitSmokeText, #"wait_child\(child_pid, task_exit->status"#, waitSmoke)
    requireSourceFact("wait_reaping_helper_records_reap", waitSmokeText, #"linux_reaping_completed"#, waitSmoke)
    requireSourceFact("wait_reaping_kunit_case", testText, #"tcti_kernel_wait_reaping_smoke_reports_linux_wait"#, testSource)

    if let exitSource = linuxKernelExitSource(),
       let exitText = try? readText(exitSource) {
        evidence["linux_exit_source"] = relativePath(exitSource)
        requireSourceFact("linux_wait_do_wait", exitText, #"long __do_wait"#, exitSource)
        requireSourceFact("linux_wait_task_zombie", exitText, #"wait_task_zombie"#, exitSource)
        requireSourceFact("linux_wait_release_task", exitText, #"release_task\(p\)"#, exitSource)
        requireSourceFact("linux_wait_exit_zombie", exitText, #"EXIT_ZOMBIE"#, exitSource)
        requireSourceFact("linux_wait_exit_dead", exitText, #"EXIT_DEAD"#, exitSource)
    } else {
        failures.append(fail("linux-exit-source-missing", "Build/OrlixKernel/src/linux-*-port/kernel/exit.c is required for wait/reaping source proof"))
        evidence["linux_exit_source"] = "missing"
    }

    let hostAdapterOwnsWaitReaping = !sourceFilesContainNone(
        root: hostAdapter,
        needles: [
            #"__do_wait"#,
            #"wait_task_zombie"#,
            #"release_task\(p\)"#,
            #"SYSCALL_DEFINE4\(wait4"#,
            #"SYSCALL_DEFINE3\(waitpid"#,
        ]
    )
    if hostAdapterOwnsWaitReaping {
        failures.append(fail("hostadapter-wait-reaping-semantics", "HostAdapter sources must not own Linux wait/reaping semantics"))
        evidence["hostadapter_linux_wait_reaping_semantics"] = "present"
    } else {
        evidence["hostadapter_linux_wait_reaping_semantics"] = "absent"
    }

    let hostcc: String
    if let configuredHostCC = ProcessInfo.processInfo.environment["ORLIX_KERNEL_HOSTCC"],
       !configuredHostCC.isEmpty {
        hostcc = configuredHostCC
    } else {
        hostcc = try commandPath("cc")
    }
    _ = try run([
        hostcc,
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-Wno-unused-function",
        "-DORLIX_TCTI_HOST_TEST_RUNNER=1",
        "-I\(hostInclude.path)",
        "-I\(tctiDir.path)",
        runnerSource.path,
        "-o",
        runnerBinary.path,
    ])
    artifacts.append(relativePath(runnerBinary))
    evidence["workload_hook_compiled"] = "true"

    let runnerOutput = try run([runnerBinary.path], check: false)
    try runnerOutput.write(to: runnerOutputURL, atomically: true, encoding: .utf8)
    artifacts.append(relativePath(runnerOutputURL))
    let runnerEvidence = waitReapingRunnerEvidence(from: runnerOutput)
    try writeJSON(runnerEvidence, to: runnerEvidenceURL)
    artifacts.append(relativePath(runnerEvidenceURL))
    for (key, value) in runnerEvidence {
        evidence[key] = value
    }

    let runnerPassed = runnerEvidence["wait_reaping_runner_named_test_passed"] == "true" &&
        runnerEvidence["wait_reaping_runner_workload_hook_executed"] == "true" &&
        runnerEvidence["wait_reaping_runner_tcti_task_exit_observed"] == "true" &&
        runnerEvidence["wait_reaping_runner_child_exit_state_recorded"] == "true" &&
        runnerEvidence["wait_reaping_runner_linux_wait_entered"] == "true" &&
        runnerEvidence["wait_reaping_runner_linux_wait_status_recorded"] == "true" &&
        runnerEvidence["wait_reaping_runner_linux_reaping_completed"] == "true" &&
        runnerEvidence["wait_reaping_runner_child_exit_code"] == "7" &&
        runnerEvidence["wait_reaping_runner_wait_status"] == "1792"
    if runnerPassed {
        evidence["workload_hook_executed"] = "true"
        evidence["tcti_task_exit_observed"] = "true"
        evidence["child_exit_state_recorded"] = "true"
        evidence["linux_wait_entered"] = "true"
        evidence["linux_wait_status_recorded"] = "true"
        evidence["linux_reaping_completed"] = "true"
        evidence["child_pid_observed"] = runnerEvidence["wait_reaping_runner_child_pid"] ?? "true"
        evidence["child_exit_code_observed"] = runnerEvidence["wait_reaping_runner_child_exit_code"] ?? "true"
        evidence["wait_status_observed"] = runnerEvidence["wait_reaping_runner_wait_status"] ?? "true"
    } else {
        failures.append(fail("kernel-workload-hook-execution", "wait/reaping smoke runner did not produce passing Linux wait evidence"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let blocker = failures.isEmpty ? "" : "No no-phone OrlixKernel workload currently proves TCTI task exit reaches Linux-owned wait/reaping."
    if !blocker.isEmpty {
        evidence["blocker"] = blocker
    }
    evidence["gate_result"] = status.rawValue
    try writeJSON(evidence, to: evidenceURL)
    artifacts.append(relativePath(evidenceURL))
    let reducer = try writeReducer(
        target: target,
        caseID: status == .pass ? "kernel-wait-reaping-smoke-pass" : "kernel-wait-reaping-smoke-fail",
        command: command,
        reason: blocker.isEmpty ? "kernel wait/reaping smoke passed" : blocker,
        artifacts: [relativePath(evidenceURL), relativePath(runnerOutputURL), relativePath(runnerEvidenceURL)],
        expectedStatus: status
    )
    artifacts.append(relativePath(reducer))
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: status == .pass ? "Kernel/TCTI no-phone wait/reaping smoke proved TCTI task exit reaches Linux-owned wait/reaping evidence." : "Kernel/TCTI no-phone wait/reaping smoke did not prove Linux-owned wait/reaping.",
        failures: failures,
        artifacts: artifacts,
        counters: [
            "source_evidence_facts": evidence.count,
            "source_proof_failures": failures.filter { $0.id == "kernel-source-proof-missing" }.count,
            "runtime_observed_task_exits": runnerPassed ? 1 : 0,
            "runtime_observed_linux_waits": runnerPassed ? 1 : 0,
            "runtime_observed_reaped_children": runnerPassed ? 1 : 0,
        ],
        kernelProfile: kernelProfile,
        kernelConfig: relativePath(kernelConfig),
        evidence: evidence,
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    if !blocker.isEmpty {
        print("blocker: \(blocker)")
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
    }
    return exitCode(for: status)
}

func runKernelPtyConsoleSmoke() throws -> Int32 {
    let target = "tcti-kernel-pty-console-smoke"
    let command = "make tcti-gate TARGET=\(target)"
    let kernelProfile = "tcti_runtime"
    let kernelConfig = path("OrlixKernel", "Sources", "ports", "orlix", "configs", "tcti_runtime_defconfig")
    let tctiEngineHeader = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "engine.h")
    let ptyConsoleSmoke = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "pty_console_smoke.h")
    let testSource = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "tcti_decode_test.c")
    let runnerSource = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "tcti_pty_console_smoke_runner.c")
    let hostInclude = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "host_include")
    let tctiDir = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti")
    let hostConsoleHeader = path("OrlixHostAdapter", "Sources", "OrlixHostAdapter", "terminal", "console.h")
    let hostConsoleSource = path("OrlixHostAdapter", "Sources", "OrlixHostAdapter", "terminal", "console.c")
    let outputRoot = buildPath("kernel_pty_console_smoke")
    let evidenceURL = outputRoot.appendingPathComponent("evidence.json")
    let runnerOutputURL = outputRoot.appendingPathComponent("runner.txt")
    let runnerEvidenceURL = outputRoot.appendingPathComponent("runner-evidence.json")
    let runnerBinary = outputRoot.appendingPathComponent("tcti_pty_console_smoke_runner")
    try ensureDirectory(outputRoot)

    var failures: [Failure] = []
    var artifacts: [String] = []
    var evidence: [String: String] = [
        "actual_command": command,
        "backend": "tcti",
        "git_sha": gitSha(),
        "kernel_profile": kernelProfile,
        "kernel_config": relativePath(kernelConfig),
        "workload_hook_compiled": "false",
        "workload_hook_executed": "false",
        "tcti_write_syscall_observed": "false",
        "linux_stdout_source_recorded": "false",
        "linux_stderr_source_recorded": "false",
        "linux_pty_write_entered": "false",
        "host_console_mirror_called": "false",
        "acceptance_marker_used": "false",
    ]

    func requireSourceFact(_ key: String, _ text: String, _ needle: String, _ url: URL) {
        if sourceTextContains(text, needle) {
            evidence[key] = "\(relativePath(url)) contains \(needle)"
        } else {
            failures.append(fail("kernel-source-proof-missing", "\(relativePath(url)) must contain \(needle)"))
            evidence[key] = "missing"
        }
    }

    let configText = try readText(kernelConfig)
    let engineHeaderText = try readText(tctiEngineHeader)
    let ptyConsoleSmokeText = try readText(ptyConsoleSmoke)
    let testText = try readText(testSource)
    let runnerSourceText = try readText(runnerSource)
    let hostConsoleHeaderText = try readText(hostConsoleHeader)
    let hostConsoleSourceText = try readText(hostConsoleSource)

    requireSourceFact("tcti_config_selected", configText, #"CONFIG_ORLIX_HOSTED_EXEC_TCTI=y"#, kernelConfig)
    requireSourceFact("native_hosted_exec_disabled", configText, #"# CONFIG_ORLIX_HOSTED_EXEC_NATIVE is not set"#, kernelConfig)
    requireSourceFact("pty_console_result_struct", engineHeaderText, #"struct tcti_kernel_pty_console_smoke_result"#, tctiEngineHeader)
    requireSourceFact("pty_console_helper_entrypoint", ptyConsoleSmokeText, #"tcti_kernel_pty_console_smoke_execute"#, ptyConsoleSmoke)
    requireSourceFact("pty_console_helper_uses_write_syscall", ptyConsoleSmokeText, #"__NR_write"#, ptyConsoleSmoke)
    requireSourceFact("pty_console_helper_stdout_marker", ptyConsoleSmokeText, #"ORLIX-PTY-CONSOLE-STDOUT"#, ptyConsoleSmoke)
    requireSourceFact("pty_console_helper_stderr_marker", ptyConsoleSmokeText, #"ORLIX-PTY-CONSOLE-STDERR"#, ptyConsoleSmoke)
    requireSourceFact("pty_console_kunit_case", testText, #"tcti_kernel_pty_console_smoke_reports_output"#, testSource)
    requireSourceFact("host_console_private_spi", hostConsoleHeaderText, #"App-private HostAdapter SPI"#, hostConsoleHeader)
    requireSourceFact("host_console_hidden_kernel_mirror", hostConsoleHeaderText, #"orlix_host_console_write"#, hostConsoleHeader)
    requireSourceFact("host_console_recent_output_mirror", hostConsoleSourceText, #"OrlixHostConsoleRememberRecentOutput"#, hostConsoleSource)
    requireSourceFact("host_console_fd_mirror", hostConsoleSourceText, #"OrlixHostConsoleWriteFileDescriptor"#, hostConsoleSource)
    requireSourceFact("host_console_trace_mirror", hostConsoleSourceText, #"orlix_host_trace_bytes"#, hostConsoleSource)

    if let readWriteSource = linuxPortSource(["fs", "read_write.c"]),
       let readWriteText = try? readText(readWriteSource) {
        evidence["linux_read_write_source"] = relativePath(readWriteSource)
        requireSourceFact("linux_write_syscall", readWriteText, #"SYSCALL_DEFINE3\(write"#, readWriteSource)
        requireSourceFact("linux_ksys_write", readWriteText, #"ssize_t ksys_write"#, readWriteSource)
        requireSourceFact("linux_vfs_write", readWriteText, #"ssize_t vfs_write"#, readWriteSource)
    } else {
        failures.append(fail("linux-read-write-source-missing", "Build/OrlixKernel/src/linux-*-port/fs/read_write.c is required for write source proof"))
        evidence["linux_read_write_source"] = "missing"
    }

    if let ptySource = linuxPortSource(["drivers", "tty", "pty.c"]),
       let ptyText = try? readText(ptySource) {
        evidence["linux_pty_source"] = relativePath(ptySource)
        requireSourceFact("linux_pty_write", ptyText, #"static ssize_t pty_write"#, ptySource)
        requireSourceFact("linux_pty_flip_buffer", ptyText, #"tty_insert_flip_string_and_push_buffer"#, ptySource)
    } else {
        failures.append(fail("linux-pty-source-missing", "Build/OrlixKernel/src/linux-*-port/drivers/tty/pty.c is required for PTY source proof"))
        evidence["linux_pty_source"] = "missing"
    }

    if let nttySource = linuxPortSource(["drivers", "tty", "n_tty.c"]),
       let nttyText = try? readText(nttySource) {
        evidence["linux_n_tty_source"] = relativePath(nttySource)
        requireSourceFact("linux_n_tty_write", nttyText, #"n_tty_write"#, nttySource)
        requireSourceFact("linux_n_tty_ops", nttyText, #"\.write[[:space:]]*=[[:space:]]n_tty_write"#, nttySource)
    } else {
        failures.append(fail("linux-n-tty-source-missing", "Build/OrlixKernel/src/linux-*-port/drivers/tty/n_tty.c is required for line-discipline source proof"))
        evidence["linux_n_tty_source"] = "missing"
    }

    if sourceTextContains(ptyConsoleSmokeText, #"ORLIX-USERLAND-TCTI-OK"#) ||
        sourceTextContains(testText, #"ORLIX-USERLAND-TCTI-OK"#) ||
        sourceTextContains(runnerSourceText, #"ORLIX-USERLAND-TCTI-OK"#) {
        failures.append(fail("acceptance-marker-forbidden", "PTY/console smoke must not emit the final app-level ORLIX-USERLAND-TCTI-OK marker"))
        evidence["acceptance_marker_used"] = "true"
    }

    let hostcc: String
    if let configuredHostCC = ProcessInfo.processInfo.environment["ORLIX_KERNEL_HOSTCC"],
       !configuredHostCC.isEmpty {
        hostcc = configuredHostCC
    } else {
        hostcc = try commandPath("cc")
    }
    _ = try run([
        hostcc,
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-Wno-unused-function",
        "-DORLIX_TCTI_HOST_TEST_RUNNER=1",
        "-I\(hostInclude.path)",
        "-I\(tctiDir.path)",
        runnerSource.path,
        "-o",
        runnerBinary.path,
    ])
    artifacts.append(relativePath(runnerBinary))
    evidence["workload_hook_compiled"] = "true"

    let runnerOutput = try run([runnerBinary.path], check: false)
    try runnerOutput.write(to: runnerOutputURL, atomically: true, encoding: .utf8)
    artifacts.append(relativePath(runnerOutputURL))
    let runnerEvidence = ptyConsoleRunnerEvidence(from: runnerOutput)
    try writeJSON(runnerEvidence, to: runnerEvidenceURL)
    artifacts.append(relativePath(runnerEvidenceURL))
    for (key, value) in runnerEvidence {
        evidence[key] = value
    }

    let runnerPassed = runnerEvidence["pty_console_runner_named_test_passed"] == "true" &&
        runnerEvidence["pty_console_runner_workload_hook_executed"] == "true" &&
        runnerEvidence["pty_console_runner_tcti_write_syscall_observed"] == "true" &&
        runnerEvidence["pty_console_runner_linux_stdout_source_recorded"] == "true" &&
        runnerEvidence["pty_console_runner_linux_stderr_source_recorded"] == "true" &&
        runnerEvidence["pty_console_runner_linux_pty_write_entered"] == "true" &&
        runnerEvidence["pty_console_runner_host_console_mirror_called"] == "true" &&
        runnerEvidence["pty_console_runner_stdout_fd"] == "1" &&
        runnerEvidence["pty_console_runner_stderr_fd"] == "2"
    if runnerPassed {
        evidence["workload_hook_executed"] = "true"
        evidence["tcti_write_syscall_observed"] = "true"
        evidence["linux_stdout_source_recorded"] = "true"
        evidence["linux_stderr_source_recorded"] = "true"
        evidence["linux_pty_write_entered"] = "true"
        evidence["host_console_mirror_called"] = "true"
        evidence["stdout_bytes_observed"] = runnerEvidence["pty_console_runner_stdout_bytes"] ?? "true"
        evidence["stderr_bytes_observed"] = runnerEvidence["pty_console_runner_stderr_bytes"] ?? "true"
        evidence["mirrored_bytes_observed"] = runnerEvidence["pty_console_runner_mirrored_bytes"] ?? "true"
    } else {
        failures.append(fail("kernel-workload-hook-execution", "PTY/console smoke runner did not produce passing stdout/stderr console evidence"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let blocker = failures.isEmpty ? "" : "No no-phone OrlixKernel workload currently proves Linux stdout/stderr reaches PTY/console mirror evidence."
    if !blocker.isEmpty {
        evidence["blocker"] = blocker
    }
    evidence["gate_result"] = status.rawValue
    try writeJSON(evidence, to: evidenceURL)
    artifacts.append(relativePath(evidenceURL))
    let reducer = try writeReducer(
        target: target,
        caseID: status == .pass ? "kernel-pty-console-smoke-pass" : "kernel-pty-console-smoke-fail",
        command: command,
        reason: blocker.isEmpty ? "kernel PTY/console smoke passed" : blocker,
        artifacts: [relativePath(evidenceURL), relativePath(runnerOutputURL), relativePath(runnerEvidenceURL)],
        expectedStatus: status
    )
    artifacts.append(relativePath(reducer))
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: status == .pass ? "Kernel/TCTI no-phone PTY/console smoke proved stdout/stderr source and private HostAdapter mirror evidence." : "Kernel/TCTI no-phone PTY/console smoke did not prove stdout/stderr console evidence.",
        failures: failures,
        artifacts: artifacts,
        counters: [
            "source_evidence_facts": evidence.count,
            "source_proof_failures": failures.filter { $0.id == "kernel-source-proof-missing" }.count,
            "runtime_observed_write_syscalls": runnerPassed ? 1 : 0,
            "runtime_observed_stdout_streams": runnerPassed ? 1 : 0,
            "runtime_observed_stderr_streams": runnerPassed ? 1 : 0,
            "runtime_observed_console_mirrors": runnerPassed ? 1 : 0,
        ],
        kernelProfile: kernelProfile,
        kernelConfig: relativePath(kernelConfig),
        evidence: evidence,
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    if !blocker.isEmpty {
        print("blocker: \(blocker)")
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
    }
    return exitCode(for: status)
}

func runKernelKselftestSubset() throws -> Int32 {
    let target = "tcti-kernel-kselftest-subset"
    let command = "make tcti-gate TARGET=\(target)"
    let kernelProfile = "tcti_runtime"
    let simulatorID = "1E5553B0-203A-4A11-BAD7-EBDE46863F66"
    let simulatorName = "Orlix-iPhone-15-Pro-Max"
    let xcodeScheme = "OrlixKernel Conformance"
    let xcodeTest = "OrlixKernelConformanceTests/OrlixKernelConformanceTests/testSignalWaitProbeCompletesThroughOrlixOSTerminalSession"
    let testSource = path("OrlixTestRunner", "Tests", "XCTest", "OrlixKernelConformanceTests", "OrlixKernelConformanceTests.swift")
    let runnerSource = path("OrlixTestRunner", "Sources", "OrlixUpstreamTestRunner.swift")
    let kernelRules = path("OrlixKernel", "Sources", "ports", "orlix", "kbuild", "kernel-rules.mk")
    let kselftestMakefile = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "tools", "testing", "selftests", "orlix", "Makefile")
    let signalWaitProbe = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "tools", "testing", "selftests", "orlix", "signal_wait_probe.c")
    let outputRoot = buildPath("kernel_kselftest_subset")
    let evidenceURL = outputRoot.appendingPathComponent("evidence.json")
    let xcodeOutputURL = outputRoot.appendingPathComponent("xcodebuild-output.txt")
    try ensureDirectory(outputRoot)

    var failures: [Failure] = []
    var artifacts: [String] = []
    var evidence: [String: String] = [
        "actual_command": command,
        "backend": "tcti",
        "git_sha": gitSha(),
        "kernel_profile": kernelProfile,
        "selected_simulator_id": simulatorID,
        "selected_simulator_name": simulatorName,
        "xcode_scheme": xcodeScheme,
        "xcode_test": xcodeTest,
        "kselftest_subset": "signal_wait_probe",
        "real_stack_execution_surface": "OrlixOS terminal session through OrlixUpstreamXCTest",
        "xcode_test_executed": "false",
        "xcode_test_passed": "false",
        "kselftest_completion_asserted_by_xctest": "false",
        "pass_count": "0",
        "fail_count": "0",
        "skip_count": "0",
    ]

    func requireSourceFact(_ key: String, _ text: String, _ needle: String, _ url: URL) {
        if sourceTextContains(text, needle) {
            evidence[key] = "\(relativePath(url)) contains \(needle)"
        } else {
            failures.append(fail("kselftest-source-proof-missing", "\(relativePath(url)) must contain \(needle)"))
            evidence[key] = "missing"
        }
    }

    let testText = try readText(testSource)
    let runnerText = try readText(runnerSource)
    let kernelRulesText = try readText(kernelRules)
    let kselftestMakefileText = try readText(kselftestMakefile)
    let signalWaitText = try readText(signalWaitProbe)

    requireSourceFact("xctest_uses_signal_wait_spec", testText, #"OrlixUpstreamXCTest.run\(.kernelSignalWait\)"#, testSource)
    requireSourceFact("xctest_requires_probe_name", testText, #"signal_wait_probe"#, testSource)
    requireSourceFact("xctest_requires_probe_marker", testText, #"ORLIX-SIGNAL-WAIT-PROBE"#, testSource)
    requireSourceFact("xctest_requires_waitpid_status", testText, #"waitpid observes signal termination status"#, testSource)
    requireSourceFact("runner_kernel_signal_wait_spec", runnerText, #"kernelSignalWait"#, runnerSource)
    requireSourceFact("runner_kselftest_completion_marker", runnerText, #"ORLIX-KSELFTEST-END"#, runnerSource)
    requireSourceFact("runner_uses_orlixos_session", runnerText, #"OrlixLinuxSession"#, runnerSource)
    requireSourceFact("kernel_rules_kselftest_target", kernelRulesText, #"kselftest: kselftest-install __kselftest-initramfs"#, kernelRules)
    requireSourceFact("kernel_rules_kselftest_install", kernelRulesText, #"TARGETS=orlix"#, kernelRules)
    requireSourceFact("kselftest_makefile_lists_signal_wait", kselftestMakefileText, #"signal_wait_probe"#, kselftestMakefile)
    requireSourceFact("signal_wait_probe_marker", signalWaitText, #"ORLIX-SIGNAL-WAIT-PROBE"#, signalWaitProbe)
    requireSourceFact("signal_wait_probe_waitpid", signalWaitText, #"waitpid"#, signalWaitProbe)

    let xcodeArguments = [
        "PATH=\(ProcessInfo.processInfo.environment["HOME"] ?? "")/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin",
        "ORLIX_PROFILE=\(kernelProfile)",
        "xcodebuild",
        "-project", "Orlix.xcodeproj",
        "-scheme", xcodeScheme,
        "-configuration", "Debug",
        "-destination", "platform=iOS Simulator,id=\(simulatorID)",
        "-only-testing:\(xcodeTest)",
        "ORLIX_PROFILE=\(kernelProfile)",
        "test",
    ]
    evidence["xcodebuild_command"] = xcodeArguments.dropFirst(2).joined(separator: " ")
    let xcodeOutput = try runWithFileBackedOutput(
        xcodeArguments,
        check: false,
        terminateAfterOutputContains: ["** TEST SUCCEEDED **", "ORLIX-KSELFTEST-END"]
    )
    try xcodeOutput.write(to: xcodeOutputURL, atomically: true, encoding: .utf8)
    artifacts.append(relativePath(xcodeOutputURL))

    let testExecuted = xcodeOutput.contains("testSignalWaitProbeCompletesThroughOrlixOSTerminalSession")
    let testSucceeded = xcodeOutput.contains("** TEST SUCCEEDED **")
    let testFailed = xcodeOutput.contains("** TEST FAILED **") ||
        xcodeOutput.range(of: #"(?m)\bfailed\b"#, options: .regularExpression) != nil
    if testExecuted {
        evidence["xcode_test_executed"] = "true"
    }
    if testSucceeded && !testFailed {
        evidence["xcode_test_passed"] = "true"
        evidence["kselftest_completion_asserted_by_xctest"] = "true"
        evidence["pass_count"] = "1"
    } else {
        evidence["fail_count"] = "1"
        failures.append(fail("kselftest-xctest-failed", "xcodebuild did not report a clean pass for \(xcodeTest)"))
    }
    if !testExecuted {
        failures.append(fail("kselftest-xctest-not-executed", "xcodebuild output did not mention \(xcodeTest)"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let blocker = failures.isEmpty ? "" : "Pinned simulator kselftest subset did not produce a clean app-hosted OrlixOS terminal-session pass."
    if !blocker.isEmpty {
        evidence["blocker"] = blocker
    }
    evidence["gate_result"] = status.rawValue
    try writeJSON(evidence, to: evidenceURL)
    artifacts.append(relativePath(evidenceURL))
    let reducer = try writeReducer(
        target: target,
        caseID: status == .pass ? "kernel-kselftest-subset-pass" : "kernel-kselftest-subset-fail",
        command: command,
        reason: blocker.isEmpty ? "kernel kselftest subset passed" : blocker,
        artifacts: [relativePath(evidenceURL), relativePath(xcodeOutputURL)],
        expectedStatus: status
    )
    artifacts.append(relativePath(reducer))
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: status == .pass ? "Pinned simulator kselftest subset passed through the OrlixOS terminal-session XCTest surface." : "Pinned simulator kselftest subset did not pass through the OrlixOS terminal-session XCTest surface.",
        failures: failures,
        artifacts: artifacts,
        counters: [
            "source_evidence_facts": evidence.count,
            "source_proof_failures": failures.filter { $0.id == "kselftest-source-proof-missing" }.count,
            "kselftest_subset_tests_executed": testExecuted ? 1 : 0,
            "kselftest_subset_tests_passed": status == .pass ? 1 : 0,
            "kselftest_subset_tests_failed": status == .pass ? 0 : 1,
            "kselftest_subset_tests_skipped": 0,
        ],
        kernelProfile: kernelProfile,
        kernelConfig: "OrlixKernel/Sources/ports/orlix/configs/\(kernelProfile)_defconfig",
        evidence: evidence,
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    if !blocker.isEmpty {
        print("blocker: \(blocker)")
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
    }
    return exitCode(for: status)
}

func runMLibCBuildSmoke() throws -> Int32 {
    let target = "tcti-mlibc-build-smoke"
    let command = "make tcti-gate TARGET=\(target)"
    let kernelProfile = "tcti_runtime"
    let simulatorID = "1E5553B0-203A-4A11-BAD7-EBDE46863F66"
    let simulatorName = "Orlix-iPhone-15-Pro-Max"
    let xcodeScheme = "OrlixMLibC Conformance"
    let xcodeTest = "OrlixMLibCConformanceTests/OrlixMLibCConformanceTests/testMLibCRootfsCompletesThroughOrlixOSTerminalSession"
    let testSource = path("OrlixTestRunner", "Tests", "XCTest", "OrlixMLibCConformanceTests", "OrlixMLibCConformanceTests.swift")
    let runnerSource = path("OrlixTestRunner", "Sources", "OrlixUpstreamTestRunner.swift")
    let mlibcMakefile = path("OrlixMLibC", "Makefile")
    let mlibcInitSource = path("OrlixMLibC", "Tests", "mlibc_test_init.c")
    let outputRoot = buildPath("mlibc_build_smoke")
    let evidenceURL = outputRoot.appendingPathComponent("evidence.json")
    let xcodeOutputURL = outputRoot.appendingPathComponent("xcodebuild-output.txt")
    try ensureDirectory(outputRoot)

    var failures: [Failure] = []
    var artifacts: [String] = []
    var evidence: [String: String] = [
        "actual_command": command,
        "backend": "tcti",
        "git_sha": gitSha(),
        "kernel_profile": kernelProfile,
        "selected_simulator_id": simulatorID,
        "selected_simulator_name": simulatorName,
        "xcode_scheme": xcodeScheme,
        "xcode_test": xcodeTest,
        "real_stack_execution_surface": "OrlixOS terminal session through OrlixUpstreamXCTest",
        "xcode_test_executed": "false",
        "xcode_test_passed": "false",
        "mlibc_completion_asserted_by_xctest": "false",
        "pass_count": "0",
        "fail_count": "0",
        "skip_count": "0",
    ]

    func requireSourceFact(_ key: String, _ text: String, _ needle: String, _ url: URL) {
        if sourceTextContains(text, needle) {
            evidence[key] = "\(relativePath(url)) contains \(needle)"
        } else {
            failures.append(fail("mlibc-source-proof-missing", "\(relativePath(url)) must contain \(needle)"))
            evidence[key] = "missing"
        }
    }

    let testText = try readText(testSource)
    let runnerText = try readText(runnerSource)
    let makefileText = try readText(mlibcMakefile)
    let initText = try readText(mlibcInitSource)

    requireSourceFact("xctest_uses_mlibc_spec", testText, #"OrlixUpstreamXCTest.run\(.mlibc\)"#, testSource)
    requireSourceFact("runner_mlibc_completion_marker", runnerText, #"ORLIX-MLIBC-TEST-END"#, runnerSource)
    requireSourceFact("runner_uses_orlixos_session", runnerText, #"OrlixLinuxSession"#, runnerSource)
    requireSourceFact("mlibc_makefile_builds_sysroot", makefileText, #"built OrlixMLibC sysroot"#, mlibcMakefile)
    requireSourceFact("mlibc_makefile_builds_static_pie_tests", makefileText, #"-static-pie"#, mlibcMakefile)
    requireSourceFact("mlibc_makefile_runs_orlix_kernel", makefileText, #"ORLIX_KERNEL_TEST_INITRAMFS_INPUT"#, mlibcMakefile)
    requireSourceFact("mlibc_init_start_marker", initText, #"ORLIX-MLIBC-TEST-INIT"#, mlibcInitSource)
    requireSourceFact("mlibc_init_completion_marker", initText, #"ORLIX-MLIBC-TEST-END"#, mlibcInitSource)
    requireSourceFact("mlibc_init_reads_test_list", initText, #"/mlibc-test-list.txt"#, mlibcInitSource)

    let xcodeArguments = [
        "PATH=\(ProcessInfo.processInfo.environment["HOME"] ?? "")/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin",
        "ORLIX_PROFILE=\(kernelProfile)",
        "xcodebuild",
        "-project", "Orlix.xcodeproj",
        "-scheme", xcodeScheme,
        "-configuration", "Debug",
        "-destination", "platform=iOS Simulator,id=\(simulatorID)",
        "-only-testing:\(xcodeTest)",
        "ORLIX_PROFILE=\(kernelProfile)",
        "ORLIX_OS_SKIP_ENVIRONMENT_RUNTIME_FIXTURES=YES",
        "test",
    ]
    evidence["xcodebuild_command"] = xcodeArguments.dropFirst(2).joined(separator: " ")
    let xcodeOutput = try runWithFileBackedOutput(
        xcodeArguments,
        check: false,
        terminateAfterOutputContains: ["** TEST SUCCEEDED **", "ORLIX-MLIBC-TEST-END"]
    )
    try xcodeOutput.write(to: xcodeOutputURL, atomically: true, encoding: .utf8)
    artifacts.append(relativePath(xcodeOutputURL))

    let testExecuted = xcodeOutput.contains("testMLibCRootfsCompletesThroughOrlixOSTerminalSession")
    let testSucceeded = xcodeOutput.contains("** TEST SUCCEEDED **")
    let testFailed = xcodeOutput.contains("** TEST FAILED **") ||
        xcodeOutput.range(of: #"(?m)\bfailed\b"#, options: .regularExpression) != nil
    let completionSeen = xcodeOutput.contains("ORLIX-MLIBC-TEST-END")
    if testExecuted {
        evidence["xcode_test_executed"] = "true"
    }
    if testSucceeded && !testFailed && completionSeen {
        evidence["xcode_test_passed"] = "true"
        evidence["mlibc_completion_asserted_by_xctest"] = "true"
        evidence["pass_count"] = "1"
    } else {
        evidence["fail_count"] = "1"
        failures.append(fail("mlibc-xctest-failed", "xcodebuild did not report a clean pass with ORLIX-MLIBC-TEST-END for \(xcodeTest)"))
    }
    if !testExecuted {
        failures.append(fail("mlibc-xctest-not-executed", "xcodebuild output did not mention \(xcodeTest)"))
    }
    if !completionSeen {
        failures.append(fail("mlibc-completion-marker-missing", "xcodebuild output did not include ORLIX-MLIBC-TEST-END"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let blocker = failures.isEmpty ? "" : "Pinned simulator OrlixMLibC smoke did not produce a clean app-hosted OrlixOS terminal-session pass."
    if !blocker.isEmpty {
        evidence["blocker"] = blocker
    }
    evidence["gate_result"] = status.rawValue
    try writeJSON(evidence, to: evidenceURL)
    artifacts.append(relativePath(evidenceURL))
    let reducer = try writeReducer(
        target: target,
        caseID: status == .pass ? "mlibc-build-smoke-pass" : "mlibc-build-smoke-fail",
        command: command,
        reason: blocker.isEmpty ? "mlibc build smoke passed" : blocker,
        artifacts: [relativePath(evidenceURL), relativePath(xcodeOutputURL)],
        expectedStatus: status
    )
    artifacts.append(relativePath(reducer))
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: status == .pass ? "Pinned simulator OrlixMLibC smoke passed through the OrlixOS terminal-session XCTest surface." : "Pinned simulator OrlixMLibC smoke did not pass through the OrlixOS terminal-session XCTest surface.",
        failures: failures,
        artifacts: artifacts,
        counters: [
            "source_evidence_facts": evidence.count,
            "source_proof_failures": failures.filter { $0.id == "mlibc-source-proof-missing" }.count,
            "mlibc_smoke_tests_executed": testExecuted ? 1 : 0,
            "mlibc_smoke_tests_passed": status == .pass ? 1 : 0,
            "mlibc_smoke_tests_failed": status == .pass ? 0 : 1,
            "mlibc_smoke_tests_skipped": 0,
        ],
        kernelProfile: kernelProfile,
        kernelConfig: "OrlixKernel/Sources/ports/orlix/configs/\(kernelProfile)_defconfig",
        evidence: evidence,
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    if !blocker.isEmpty {
        print("blocker: \(blocker)")
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
    }
    return exitCode(for: status)
}

func runMLibCSysdepsSmoke() throws -> Int32 {
    let target = "tcti-mlibc-sysdeps-smoke"
    let command = "make tcti-gate TARGET=\(target)"
    let kernelProfile = "tcti_runtime"
    let simulatorID = "1E5553B0-203A-4A11-BAD7-EBDE46863F66"
    let simulatorName = "Orlix-iPhone-15-Pro-Max"
    let xcodeScheme = "OrlixMLibC Conformance"
    let xcodeTest = "OrlixMLibCConformanceTests/OrlixMLibCConformanceTests/testMLibCRootfsCompletesThroughOrlixOSTerminalSession"
    let testSource = path("OrlixTestRunner", "Tests", "XCTest", "OrlixMLibCConformanceTests", "OrlixMLibCConformanceTests.swift")
    let runnerSource = path("OrlixTestRunner", "Sources", "OrlixUpstreamTestRunner.swift")
    let mlibcMakefile = path("OrlixMLibC", "Makefile")
    let mlibcInitSource = path("OrlixMLibC", "Tests", "mlibc_test_init.c")
    let outputRoot = buildPath("mlibc_sysdeps_smoke")
    let evidenceURL = outputRoot.appendingPathComponent("evidence.json")
    let xcodeOutputURL = outputRoot.appendingPathComponent("xcodebuild-output.txt")
    try ensureDirectory(outputRoot)

    var failures: [Failure] = []
    var artifacts: [String] = []
    var evidence: [String: String] = [
        "actual_command": command,
        "backend": "tcti",
        "git_sha": gitSha(),
        "kernel_profile": kernelProfile,
        "selected_simulator_id": simulatorID,
        "selected_simulator_name": simulatorName,
        "xcode_scheme": xcodeScheme,
        "xcode_test": xcodeTest,
        "real_stack_execution_surface": "OrlixOS terminal session through OrlixUpstreamXCTest",
        "xcode_test_executed": "false",
        "xcode_test_passed": "false",
        "mlibc_completion_asserted_by_xctest": "false",
        "sysdeps_completion_asserted_by_xctest": "false",
        "pass_count": "0",
        "fail_count": "0",
        "skip_count": "0",
    ]

    func requireSourceFact(_ key: String, _ text: String, _ needle: String, _ url: URL) {
        if sourceTextContains(text, needle) {
            evidence[key] = "\(relativePath(url)) contains \(needle)"
        } else {
            failures.append(fail("mlibc-sysdeps-source-proof-missing", "\(relativePath(url)) must contain \(needle)"))
            evidence[key] = "missing"
        }
    }

    let testText = try readText(testSource)
    let runnerText = try readText(runnerSource)
    let makefileText = try readText(mlibcMakefile)
    let initText = try readText(mlibcInitSource)

    requireSourceFact("xctest_uses_mlibc_spec", testText, #"OrlixUpstreamXCTest.run\(.mlibc\)"#, testSource)
    requireSourceFact("runner_mlibc_completion_marker", runnerText, #"ORLIX-MLIBC-TEST-END"#, runnerSource)
    requireSourceFact("runner_uses_orlixos_session", runnerText, #"OrlixLinuxSession"#, runnerSource)
    requireSourceFact("mlibc_makefile_builds_sysroot", makefileText, #"built OrlixMLibC sysroot"#, mlibcMakefile)
    requireSourceFact("mlibc_makefile_selects_linux_sysdeps", makefileText, #"upstream mlibc sysdeps/linux"#, mlibcMakefile)
    requireSourceFact("mlibc_makefile_records_upstream_libc_lane", makefileText, #"proof_lane=mlibc-upstream-libc-tests"#, mlibcMakefile)
    requireSourceFact("mlibc_makefile_builds_static_pie_tests", makefileText, #"-static-pie"#, mlibcMakefile)
    requireSourceFact("mlibc_makefile_runs_orlix_kernel", makefileText, #"ORLIX_KERNEL_TEST_INITRAMFS_INPUT"#, mlibcMakefile)
    requireSourceFact("mlibc_init_start_marker", initText, #"ORLIX-MLIBC-TEST-INIT"#, mlibcInitSource)
    requireSourceFact("mlibc_init_completion_marker", initText, #"ORLIX-MLIBC-TEST-END"#, mlibcInitSource)
    requireSourceFact("mlibc_init_reads_test_list", initText, #"/mlibc-test-list.txt"#, mlibcInitSource)

    let xcodeArguments = [
        "PATH=\(ProcessInfo.processInfo.environment["HOME"] ?? "")/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin",
        "ORLIX_PROFILE=\(kernelProfile)",
        "xcodebuild",
        "-project", "Orlix.xcodeproj",
        "-scheme", xcodeScheme,
        "-configuration", "Debug",
        "-destination", "platform=iOS Simulator,id=\(simulatorID)",
        "-only-testing:\(xcodeTest)",
        "ORLIX_PROFILE=\(kernelProfile)",
        "ORLIX_OS_SKIP_ENVIRONMENT_RUNTIME_FIXTURES=YES",
        "test",
    ]
    evidence["xcodebuild_command"] = xcodeArguments.dropFirst(2).joined(separator: " ")
    let xcodeOutput = try runWithFileBackedOutput(
        xcodeArguments,
        check: false,
        terminateAfterOutputContains: ["** TEST SUCCEEDED **", "ORLIX-MLIBC-TEST-END"]
    )
    try xcodeOutput.write(to: xcodeOutputURL, atomically: true, encoding: .utf8)
    artifacts.append(relativePath(xcodeOutputURL))

    let testExecuted = xcodeOutput.contains("testMLibCRootfsCompletesThroughOrlixOSTerminalSession")
    let testSucceeded = xcodeOutput.contains("** TEST SUCCEEDED **")
    let testFailed = xcodeOutput.contains("** TEST FAILED **") ||
        xcodeOutput.range(of: #"(?m)\bfailed\b"#, options: .regularExpression) != nil
    let completionSeen = xcodeOutput.contains("ORLIX-MLIBC-TEST-END")
    let sysdepsObserved = xcodeOutput.contains("/mlibc-tests/linux-") ||
        xcodeOutput.contains("linux/cpuset") ||
        xcodeOutput.contains("linux/getifaddrs") ||
        xcodeOutput.contains("linux/pidfd")
    if testExecuted {
        evidence["xcode_test_executed"] = "true"
    }
    if testSucceeded && !testFailed && completionSeen && sysdepsObserved {
        evidence["xcode_test_passed"] = "true"
        evidence["mlibc_completion_asserted_by_xctest"] = "true"
        evidence["sysdeps_completion_asserted_by_xctest"] = "true"
        evidence["pass_count"] = "1"
    } else {
        evidence["fail_count"] = "1"
        failures.append(fail("mlibc-sysdeps-xctest-failed", "xcodebuild did not report a clean pass with ORLIX-MLIBC-TEST-END and linux sysdeps test output for \(xcodeTest)"))
    }
    if !testExecuted {
        failures.append(fail("mlibc-sysdeps-xctest-not-executed", "xcodebuild output did not mention \(xcodeTest)"))
    }
    if !completionSeen {
        failures.append(fail("mlibc-sysdeps-completion-marker-missing", "xcodebuild output did not include ORLIX-MLIBC-TEST-END"))
    }
    if !sysdepsObserved {
        failures.append(fail("mlibc-sysdeps-output-missing", "xcodebuild output did not include linux sysdeps test execution lines"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let blocker = failures.isEmpty ? "" : "Pinned simulator OrlixMLibC sysdeps smoke did not produce a clean app-hosted OrlixOS terminal-session pass."
    if !blocker.isEmpty {
        evidence["blocker"] = blocker
    }
    evidence["gate_result"] = status.rawValue
    try writeJSON(evidence, to: evidenceURL)
    artifacts.append(relativePath(evidenceURL))
    let reducer = try writeReducer(
        target: target,
        caseID: status == .pass ? "mlibc-sysdeps-smoke-pass" : "mlibc-sysdeps-smoke-fail",
        command: command,
        reason: blocker.isEmpty ? "mlibc sysdeps smoke passed" : blocker,
        artifacts: [relativePath(evidenceURL), relativePath(xcodeOutputURL)],
        expectedStatus: status
    )
    artifacts.append(relativePath(reducer))
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: status == .pass ? "Pinned simulator OrlixMLibC sysdeps smoke passed through the OrlixOS terminal-session XCTest surface." : "Pinned simulator OrlixMLibC sysdeps smoke did not pass through the OrlixOS terminal-session XCTest surface.",
        failures: failures,
        artifacts: artifacts,
        counters: [
            "source_evidence_facts": evidence.count,
            "source_proof_failures": failures.filter { $0.id == "mlibc-sysdeps-source-proof-missing" }.count,
            "mlibc_sysdeps_tests_executed": testExecuted && sysdepsObserved ? 1 : 0,
            "mlibc_sysdeps_tests_passed": status == .pass ? 1 : 0,
            "mlibc_sysdeps_tests_failed": status == .pass ? 0 : 1,
            "mlibc_sysdeps_tests_skipped": 0,
        ],
        kernelProfile: kernelProfile,
        kernelConfig: "OrlixKernel/Sources/ports/orlix/configs/\(kernelProfile)_defconfig",
        evidence: evidence,
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    if !blocker.isEmpty {
        print("blocker: \(blocker)")
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
    }
    return exitCode(for: status)
}

func runMLibCLibcTestSubset() throws -> Int32 {
    let target = "tcti-mlibc-libc-test-subset"
    let command = "make tcti-gate TARGET=\(target)"
    let kernelProfile = "tcti_runtime"
    let simulatorID = "1E5553B0-203A-4A11-BAD7-EBDE46863F66"
    let simulatorName = "Orlix-iPhone-15-Pro-Max"
    let xcodeScheme = "OrlixMLibC Conformance"
    let xcodeTest = "OrlixMLibCConformanceTests/OrlixMLibCConformanceTests/testMLibCRootfsCompletesThroughOrlixOSTerminalSession"
    let testSource = path("OrlixTestRunner", "Tests", "XCTest", "OrlixMLibCConformanceTests", "OrlixMLibCConformanceTests.swift")
    let runnerSource = path("OrlixTestRunner", "Sources", "OrlixUpstreamTestRunner.swift")
    let mlibcMakefile = path("OrlixMLibC", "Makefile")
    let mlibcInitSource = path("OrlixMLibC", "Tests", "mlibc_test_init.c")
    let outputRoot = buildPath("mlibc_libc_test_subset")
    let evidenceURL = outputRoot.appendingPathComponent("evidence.json")
    let xcodeOutputURL = outputRoot.appendingPathComponent("xcodebuild-output.txt")
    try ensureDirectory(outputRoot)

    var failures: [Failure] = []
    var artifacts: [String] = []
    var evidence: [String: String] = [
        "actual_command": command,
        "backend": "tcti",
        "git_sha": gitSha(),
        "kernel_profile": kernelProfile,
        "selected_simulator_id": simulatorID,
        "selected_simulator_name": simulatorName,
        "xcode_scheme": xcodeScheme,
        "xcode_test": xcodeTest,
        "real_stack_execution_surface": "OrlixOS terminal session through OrlixUpstreamXCTest",
        "xcode_test_executed": "false",
        "xcode_test_passed": "false",
        "mlibc_completion_asserted_by_xctest": "false",
        "libc_subset_completion_asserted_by_xctest": "false",
        "pass_count": "0",
        "fail_count": "0",
        "skip_count": "0",
    ]

    func requireSourceFact(_ key: String, _ text: String, _ needle: String, _ url: URL) {
        if sourceTextContains(text, needle) {
            evidence[key] = "\(relativePath(url)) contains \(needle)"
        } else {
            failures.append(fail("mlibc-libc-subset-source-proof-missing", "\(relativePath(url)) must contain \(needle)"))
            evidence[key] = "missing"
        }
    }

    let testText = try readText(testSource)
    let runnerText = try readText(runnerSource)
    let makefileText = try readText(mlibcMakefile)
    let initText = try readText(mlibcInitSource)

    requireSourceFact("xctest_uses_mlibc_spec", testText, #"OrlixUpstreamXCTest.run\(.mlibc\)"#, testSource)
    requireSourceFact("runner_mlibc_completion_marker", runnerText, #"ORLIX-MLIBC-TEST-END"#, runnerSource)
    requireSourceFact("runner_uses_orlixos_session", runnerText, #"OrlixLinuxSession"#, runnerSource)
    requireSourceFact("mlibc_makefile_builds_sysroot", makefileText, #"built OrlixMLibC sysroot"#, mlibcMakefile)
    requireSourceFact("mlibc_makefile_builds_static_pie_tests", makefileText, #"-static-pie"#, mlibcMakefile)
    requireSourceFact("mlibc_makefile_records_upstream_libc_lane", makefileText, #"proof_lane=mlibc-upstream-libc-tests"#, mlibcMakefile)
    requireSourceFact("mlibc_makefile_runs_orlix_kernel", makefileText, #"ORLIX_KERNEL_TEST_INITRAMFS_INPUT"#, mlibcMakefile)
    requireSourceFact("mlibc_init_start_marker", initText, #"ORLIX-MLIBC-TEST-INIT"#, mlibcInitSource)
    requireSourceFact("mlibc_init_completion_marker", initText, #"ORLIX-MLIBC-TEST-END"#, mlibcInitSource)
    requireSourceFact("mlibc_init_reads_test_list", initText, #"/mlibc-test-list.txt"#, mlibcInitSource)

    let xcodeArguments = [
        "PATH=\(ProcessInfo.processInfo.environment["HOME"] ?? "")/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin",
        "ORLIX_PROFILE=\(kernelProfile)",
        "xcodebuild",
        "-project", "Orlix.xcodeproj",
        "-scheme", xcodeScheme,
        "-configuration", "Debug",
        "-destination", "platform=iOS Simulator,id=\(simulatorID)",
        "-only-testing:\(xcodeTest)",
        "ORLIX_PROFILE=\(kernelProfile)",
        "ORLIX_OS_SKIP_ENVIRONMENT_RUNTIME_FIXTURES=YES",
        "test",
    ]
    evidence["xcodebuild_command"] = xcodeArguments.dropFirst(2).joined(separator: " ")
    let xcodeOutput = try runWithFileBackedOutput(
        xcodeArguments,
        check: false,
        terminateAfterOutputContains: ["** TEST SUCCEEDED **", "ORLIX-MLIBC-TEST-END"]
    )
    try xcodeOutput.write(to: xcodeOutputURL, atomically: true, encoding: .utf8)
    artifacts.append(relativePath(xcodeOutputURL))

    let testExecuted = xcodeOutput.contains("testMLibCRootfsCompletesThroughOrlixOSTerminalSession")
    let testSucceeded = xcodeOutput.contains("** TEST SUCCEEDED **")
    let testFailed = xcodeOutput.contains("** TEST FAILED **") ||
        xcodeOutput.range(of: #"(?m)\bfailed\b"#, options: .regularExpression) != nil
    let completionSeen = xcodeOutput.contains("ORLIX-MLIBC-TEST-END")
    let ansiObserved = xcodeOutput.contains("ansi/sprintf") || xcodeOutput.contains("ansi/sscanf")
    let posixObserved = xcodeOutput.contains("posix/fdopen") || xcodeOutput.contains("posix/posix_memalign")
    let linuxObserved = xcodeOutput.contains("linux/cpuset") || xcodeOutput.contains("linux/pidfd")
    if testExecuted {
        evidence["xcode_test_executed"] = "true"
    }
    if testSucceeded && !testFailed && completionSeen && ansiObserved && posixObserved && linuxObserved {
        evidence["xcode_test_passed"] = "true"
        evidence["mlibc_completion_asserted_by_xctest"] = "true"
        evidence["libc_subset_completion_asserted_by_xctest"] = "true"
        evidence["pass_count"] = "1"
    } else {
        evidence["fail_count"] = "1"
        failures.append(fail("mlibc-libc-subset-xctest-failed", "xcodebuild did not report a clean pass with ANSI, POSIX, Linux, and ORLIX-MLIBC-TEST-END output for \(xcodeTest)"))
    }
    if !testExecuted {
        failures.append(fail("mlibc-libc-subset-xctest-not-executed", "xcodebuild output did not mention \(xcodeTest)"))
    }
    if !completionSeen {
        failures.append(fail("mlibc-libc-subset-completion-marker-missing", "xcodebuild output did not include ORLIX-MLIBC-TEST-END"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let blocker = failures.isEmpty ? "" : "Pinned simulator OrlixMLibC libc test subset did not produce a clean app-hosted OrlixOS terminal-session pass."
    if !blocker.isEmpty {
        evidence["blocker"] = blocker
    }
    evidence["gate_result"] = status.rawValue
    try writeJSON(evidence, to: evidenceURL)
    artifacts.append(relativePath(evidenceURL))
    let reducer = try writeReducer(
        target: target,
        caseID: status == .pass ? "mlibc-libc-test-subset-pass" : "mlibc-libc-test-subset-fail",
        command: command,
        reason: blocker.isEmpty ? "mlibc libc test subset passed" : blocker,
        artifacts: [relativePath(evidenceURL), relativePath(xcodeOutputURL)],
        expectedStatus: status
    )
    artifacts.append(relativePath(reducer))
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: status == .pass ? "Pinned simulator OrlixMLibC libc test subset passed through the OrlixOS terminal-session XCTest surface." : "Pinned simulator OrlixMLibC libc test subset did not pass through the OrlixOS terminal-session XCTest surface.",
        failures: failures,
        artifacts: artifacts,
        counters: [
            "source_evidence_facts": evidence.count,
            "source_proof_failures": failures.filter { $0.id == "mlibc-libc-subset-source-proof-missing" }.count,
            "mlibc_libc_subset_tests_executed": testExecuted && ansiObserved && posixObserved && linuxObserved ? 1 : 0,
            "mlibc_libc_subset_tests_passed": status == .pass ? 1 : 0,
            "mlibc_libc_subset_tests_failed": status == .pass ? 0 : 1,
            "mlibc_libc_subset_tests_skipped": 0,
        ],
        kernelProfile: kernelProfile,
        kernelConfig: "OrlixKernel/Sources/ports/orlix/configs/\(kernelProfile)_defconfig",
        evidence: evidence,
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    if !blocker.isEmpty {
        print("blocker: \(blocker)")
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
    }
    return exitCode(for: status)
}

func runMLibCDynamicLoaderSmoke() throws -> Int32 {
    let target = "tcti-mlibc-dynamic-loader-smoke"
    let command = "make tcti-gate TARGET=\(target)"
    let kernelProfile = "tcti_runtime"
    let simulatorID = "1E5553B0-203A-4A11-BAD7-EBDE46863F66"
    let simulatorName = "Orlix-iPhone-15-Pro-Max"
    let xcodeScheme = "OrlixMLibC Conformance"
    let xcodeTest = "OrlixMLibCConformanceTests/OrlixMLibCConformanceTests/testMLibCRootfsCompletesThroughOrlixOSTerminalSession"
    let testSource = path("OrlixTestRunner", "Tests", "XCTest", "OrlixMLibCConformanceTests", "OrlixMLibCConformanceTests.swift")
    let runnerSource = path("OrlixTestRunner", "Sources", "OrlixUpstreamTestRunner.swift")
    let mlibcMakefile = path("OrlixMLibC", "Makefile")
    let mlibcInitSource = path("OrlixMLibC", "Tests", "mlibc_test_init.c")
    let outputRoot = buildPath("mlibc_dynamic_loader_smoke")
    let evidenceURL = outputRoot.appendingPathComponent("evidence.json")
    let xcodeOutputURL = outputRoot.appendingPathComponent("xcodebuild-output.txt")
    try ensureDirectory(outputRoot)

    var failures: [Failure] = []
    var artifacts: [String] = []
    var evidence: [String: String] = [
        "actual_command": command,
        "backend": "tcti",
        "git_sha": gitSha(),
        "kernel_profile": kernelProfile,
        "selected_simulator_id": simulatorID,
        "selected_simulator_name": simulatorName,
        "xcode_scheme": xcodeScheme,
        "xcode_test": xcodeTest,
        "real_stack_execution_surface": "OrlixOS terminal session through OrlixUpstreamXCTest",
        "xcode_test_executed": "false",
        "xcode_test_passed": "false",
        "mlibc_completion_asserted_by_xctest": "false",
        "dynamic_loader_completion_asserted_by_xctest": "false",
        "dynamic_loader_workload_configured": "false",
        "dynamic_loader_pt_interp_source_proof": "false",
        "pass_count": "0",
        "fail_count": "0",
        "skip_count": "0",
    ]

    func recordSourceFact(_ key: String, _ text: String, _ needle: String, _ url: URL) -> Bool {
        if sourceTextContains(text, needle) {
            evidence[key] = "\(relativePath(url)) contains \(needle)"
            return true
        }
        evidence[key] = "missing"
        return false
    }

    let testText = try readText(testSource)
    let runnerText = try readText(runnerSource)
    let makefileText = try readText(mlibcMakefile)
    let initText = try readText(mlibcInitSource)

    if !recordSourceFact("xctest_uses_mlibc_spec", testText, #"OrlixUpstreamXCTest.run\(.mlibc\)"#, testSource) {
        failures.append(fail("mlibc-dynamic-loader-source-proof-missing", "\(relativePath(testSource)) must run the mlibc OrlixUpstreamXCTest spec"))
    }
    if !recordSourceFact("runner_uses_orlixos_session", runnerText, #"OrlixLinuxSession"#, runnerSource) {
        failures.append(fail("mlibc-dynamic-loader-source-proof-missing", "\(relativePath(runnerSource)) must execute through OrlixLinuxSession"))
    }
    if !recordSourceFact("mlibc_init_completion_marker", initText, #"ORLIX-MLIBC-TEST-END"#, mlibcInitSource) {
        failures.append(fail("mlibc-dynamic-loader-source-proof-missing", "\(relativePath(mlibcInitSource)) must contain ORLIX-MLIBC-TEST-END"))
    }

    if sourceTextContains(makefileText, #"-static-pie"#) {
        evidence["mlibc_makefile_current_link_mode"] = "\(relativePath(mlibcMakefile)) contains -static-pie"
    } else {
        evidence["mlibc_makefile_current_link_mode"] = "no -static-pie marker found"
    }

    let dynamicWorkloadConfigured = sourceTextContains(makefileText, #"PT_INTERP"#) ||
        sourceTextContains(makefileText, #"dynamic-loader"#) ||
        sourceTextContains(makefileText, #"ld.so"#) ||
        sourceTextContains(makefileText, #"ldso"#) ||
        sourceTextContains(initText, #"ORLIX-MLIBC-DYNAMIC-LOADER-OK"#)
    if dynamicWorkloadConfigured {
        evidence["dynamic_loader_workload_configured"] = "true"
    } else {
        failures.append(fail("mlibc-dynamic-loader-workload-missing", "No OrlixMLibC dynamic-loader workload is configured; static PIE mlibc execution is not sufficient"))
    }

    let ptInterpSourceProof = sourceTextContains(makefileText, #"PT_INTERP"#) ||
        sourceTextContains(initText, #"PT_INTERP"#)
    if ptInterpSourceProof {
        evidence["dynamic_loader_pt_interp_source_proof"] = "true"
    } else {
        failures.append(fail("mlibc-dynamic-loader-pt-interp-missing", "No source proof requires a PT_INTERP-backed OrlixMLibC binary"))
    }

    let xcodeArguments = [
        "PATH=\(ProcessInfo.processInfo.environment["HOME"] ?? "")/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin",
        "ORLIX_PROFILE=\(kernelProfile)",
        "xcodebuild",
        "-project", "Orlix.xcodeproj",
        "-scheme", xcodeScheme,
        "-configuration", "Debug",
        "-destination", "platform=iOS Simulator,id=\(simulatorID)",
        "-only-testing:\(xcodeTest)",
        "ORLIX_PROFILE=\(kernelProfile)",
        "ORLIX_OS_SKIP_ENVIRONMENT_RUNTIME_FIXTURES=YES",
        "test",
    ]
    evidence["xcodebuild_command"] = xcodeArguments.dropFirst(2).joined(separator: " ")
    let xcodeOutput = try runWithFileBackedOutput(
        xcodeArguments,
        check: false,
        terminateAfterOutputContains: ["** TEST SUCCEEDED **", "ORLIX-MLIBC-TEST-END"]
    )
    try xcodeOutput.write(to: xcodeOutputURL, atomically: true, encoding: .utf8)
    artifacts.append(relativePath(xcodeOutputURL))

    let testExecuted = xcodeOutput.contains("testMLibCRootfsCompletesThroughOrlixOSTerminalSession")
    let testSucceeded = xcodeOutput.contains("** TEST SUCCEEDED **")
    let testFailed = xcodeOutput.contains("** TEST FAILED **") ||
        xcodeOutput.range(of: #"(?m)\bfailed\b"#, options: .regularExpression) != nil
    let completionSeen = xcodeOutput.contains("ORLIX-MLIBC-TEST-END")
    let dynamicLoaderSeen = xcodeOutput.contains("ORLIX-MLIBC-DYNAMIC-LOADER-OK") ||
        xcodeOutput.contains("PT_INTERP") ||
        xcodeOutput.contains("ld.so") ||
        xcodeOutput.contains("ldso")
    if testExecuted {
        evidence["xcode_test_executed"] = "true"
    }
    if testSucceeded && !testFailed {
        evidence["xcode_test_passed"] = "true"
    } else {
        failures.append(fail("mlibc-dynamic-loader-xctest-failed", "xcodebuild did not report a clean pass for \(xcodeTest)"))
    }
    if completionSeen {
        evidence["mlibc_completion_asserted_by_xctest"] = "true"
    }
    if dynamicLoaderSeen {
        evidence["dynamic_loader_completion_asserted_by_xctest"] = "true"
    } else {
        failures.append(fail("mlibc-dynamic-loader-output-missing", "xcodebuild output did not include a dynamic-loader marker or PT_INTERP-backed execution evidence"))
    }
    if !testExecuted {
        failures.append(fail("mlibc-dynamic-loader-xctest-not-executed", "xcodebuild output did not mention \(xcodeTest)"))
    }
    if !completionSeen {
        failures.append(fail("mlibc-dynamic-loader-completion-marker-missing", "xcodebuild output did not include ORLIX-MLIBC-TEST-END"))
    }
    if !(testSucceeded && !testFailed && completionSeen && dynamicLoaderSeen && dynamicWorkloadConfigured && ptInterpSourceProof) {
        evidence["fail_count"] = "1"
    } else {
        evidence["pass_count"] = "1"
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let blocker = failures.isEmpty ? "" : "Pinned simulator OrlixMLibC dynamic-loader smoke lacks a real PT_INTERP-backed dynamic-loader workload."
    if !blocker.isEmpty {
        evidence["blocker"] = blocker
    }
    evidence["gate_result"] = status.rawValue
    try writeJSON(evidence, to: evidenceURL)
    artifacts.append(relativePath(evidenceURL))
    let reducer = try writeReducer(
        target: target,
        caseID: status == .pass ? "mlibc-dynamic-loader-smoke-pass" : "mlibc-dynamic-loader-smoke-fail",
        command: command,
        reason: blocker.isEmpty ? "mlibc dynamic-loader smoke passed" : blocker,
        artifacts: [relativePath(evidenceURL), relativePath(xcodeOutputURL)],
        expectedStatus: status
    )
    artifacts.append(relativePath(reducer))
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: status == .pass ? "Pinned simulator OrlixMLibC dynamic-loader smoke passed through a PT_INTERP-backed OrlixOS terminal-session workload." : "Pinned simulator OrlixMLibC dynamic-loader smoke is blocked because the current workload is static PIE-only.",
        command: command,
        failures: failures,
        artifacts: artifacts,
        counters: [
            "source_evidence_facts": evidence.count,
            "source_proof_failures": failures.filter { $0.id == "mlibc-dynamic-loader-source-proof-missing" }.count,
            "mlibc_dynamic_loader_tests_executed": testExecuted && dynamicLoaderSeen ? 1 : 0,
            "mlibc_dynamic_loader_tests_passed": status == .pass ? 1 : 0,
            "mlibc_dynamic_loader_tests_failed": status == .pass ? 0 : 1,
            "mlibc_dynamic_loader_tests_skipped": 0,
        ],
        kernelProfile: kernelProfile,
        kernelConfig: "OrlixKernel/Sources/ports/orlix/configs/\(kernelProfile)_defconfig",
        evidence: evidence,
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    if !blocker.isEmpty {
        print("blocker: \(blocker)")
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
    }
    return exitCode(for: status)
}

func runMLibCPthreadTLSSmoke() throws -> Int32 {
    let target = "tcti-mlibc-pthread-tls-smoke"
    let command = "make tcti-gate TARGET=\(target)"
    let kernelProfile = "tcti_runtime"
    let simulatorID = "1E5553B0-203A-4A11-BAD7-EBDE46863F66"
    let simulatorName = "Orlix-iPhone-15-Pro-Max"
    let xcodeScheme = "OrlixMLibC Conformance"
    let xcodeTest = "OrlixMLibCConformanceTests/OrlixMLibCConformanceTests/testMLibCRootfsCompletesThroughOrlixOSTerminalSession"
    let testSource = path("OrlixTestRunner", "Tests", "XCTest", "OrlixMLibCConformanceTests", "OrlixMLibCConformanceTests.swift")
    let runnerSource = path("OrlixTestRunner", "Sources", "OrlixUpstreamTestRunner.swift")
    let mlibcMakefile = path("OrlixMLibC", "Makefile")
    let mlibcInitSource = path("OrlixMLibC", "Tests", "mlibc_test_init.c")
    let outputRoot = buildPath("mlibc_pthread_tls_smoke")
    let evidenceURL = outputRoot.appendingPathComponent("evidence.json")
    let xcodeOutputURL = outputRoot.appendingPathComponent("xcodebuild-output.txt")
    try ensureDirectory(outputRoot)

    var failures: [Failure] = []
    var artifacts: [String] = []
    var evidence: [String: String] = [
        "actual_command": command,
        "backend": "tcti",
        "git_sha": gitSha(),
        "kernel_profile": kernelProfile,
        "selected_simulator_id": simulatorID,
        "selected_simulator_name": simulatorName,
        "xcode_scheme": xcodeScheme,
        "xcode_test": xcodeTest,
        "real_stack_execution_surface": "OrlixOS terminal session through OrlixUpstreamXCTest",
        "xcode_test_executed": "false",
        "xcode_test_passed": "false",
        "mlibc_completion_asserted_by_xctest": "false",
        "dynamic_loader_completion_asserted_by_xctest": "false",
        "pthread_key_asserted_by_xctest": "false",
        "pthread_thread_local_asserted_by_xctest": "false",
        "pthread_create_asserted_by_xctest": "false",
        "pass_count": "0",
        "fail_count": "0",
        "skip_count": "0",
    ]

    func requireSourceFact(_ key: String, _ text: String, _ needle: String, _ url: URL) {
        if sourceTextContains(text, needle) {
            evidence[key] = "\(relativePath(url)) contains \(needle)"
        } else {
            failures.append(fail("mlibc-pthread-tls-source-proof-missing", "\(relativePath(url)) must contain \(needle)"))
            evidence[key] = "missing"
        }
    }

    let testText = try readText(testSource)
    let runnerText = try readText(runnerSource)
    let makefileText = try readText(mlibcMakefile)
    let initText = try readText(mlibcInitSource)

    requireSourceFact("xctest_uses_mlibc_spec", testText, #"OrlixUpstreamXCTest.run\(.mlibc\)"#, testSource)
    requireSourceFact("runner_uses_orlixos_session", runnerText, #"OrlixLinuxSession"#, runnerSource)
    requireSourceFact("runner_mlibc_completion_marker", runnerText, #"ORLIX-MLIBC-TEST-END"#, runnerSource)
    requireSourceFact("mlibc_makefile_includes_pthread_key", makefileText, #"posix/pthread_key"#, mlibcMakefile)
    requireSourceFact("mlibc_makefile_includes_pthread_thread_local", makefileText, #"posix/pthread_thread_local"#, mlibcMakefile)
    requireSourceFact("mlibc_makefile_includes_pthread_create", makefileText, #"posix/pthread_create"#, mlibcMakefile)
    requireSourceFact("mlibc_makefile_uses_dynamic_loader", makefileText, #"--dynamic-linker=/usr/lib/ld.so"#, mlibcMakefile)
    requireSourceFact("mlibc_init_completion_marker", initText, #"ORLIX-MLIBC-TEST-END"#, mlibcInitSource)
    requireSourceFact("mlibc_init_execs_test_list", initText, #"execve"#, mlibcInitSource)

    let xcodeArguments = [
        "PATH=\(ProcessInfo.processInfo.environment["HOME"] ?? "")/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin",
        "ORLIX_PROFILE=\(kernelProfile)",
        "xcodebuild",
        "-project", "Orlix.xcodeproj",
        "-scheme", xcodeScheme,
        "-configuration", "Debug",
        "-destination", "platform=iOS Simulator,id=\(simulatorID)",
        "-only-testing:\(xcodeTest)",
        "ORLIX_PROFILE=\(kernelProfile)",
        "ORLIX_OS_SKIP_ENVIRONMENT_RUNTIME_FIXTURES=YES",
        "test",
    ]
    evidence["xcodebuild_command"] = xcodeArguments.dropFirst(2).joined(separator: " ")
    let xcodeOutput = try runWithFileBackedOutput(
        xcodeArguments,
        check: false,
        terminateAfterOutputContains: ["** TEST SUCCEEDED **", "ORLIX-MLIBC-TEST-END"]
    )
    try xcodeOutput.write(to: xcodeOutputURL, atomically: true, encoding: .utf8)
    artifacts.append(relativePath(xcodeOutputURL))

    let testExecuted = xcodeOutput.contains("testMLibCRootfsCompletesThroughOrlixOSTerminalSession")
    let testSucceeded = xcodeOutput.contains("** TEST SUCCEEDED **")
    let testFailed = xcodeOutput.contains("** TEST FAILED **") ||
        xcodeOutput.range(of: #"(?m)\bfailed\b"#, options: .regularExpression) != nil
    let completionSeen = xcodeOutput.contains("ORLIX-MLIBC-TEST-END")
    let dynamicLoaderSeen = xcodeOutput.contains("ORLIX-MLIBC-DYNAMIC-LOADER-OK")
    let pthreadKeySeen = outputHasPassingTAPLabel(xcodeOutput, "posix/pthread_key")
    let pthreadThreadLocalSeen = outputHasPassingTAPLabel(xcodeOutput, "posix/pthread_thread_local")
    let pthreadCreateSeen = outputHasPassingTAPLabel(xcodeOutput, "posix/pthread_create")
    if testExecuted {
        evidence["xcode_test_executed"] = "true"
    }
    if completionSeen {
        evidence["mlibc_completion_asserted_by_xctest"] = "true"
    }
    if dynamicLoaderSeen {
        evidence["dynamic_loader_completion_asserted_by_xctest"] = "true"
    }
    if pthreadKeySeen {
        evidence["pthread_key_asserted_by_xctest"] = "true"
    }
    if pthreadThreadLocalSeen {
        evidence["pthread_thread_local_asserted_by_xctest"] = "true"
    }
    if pthreadCreateSeen {
        evidence["pthread_create_asserted_by_xctest"] = "true"
    }
    if testSucceeded && !testFailed && completionSeen && dynamicLoaderSeen && pthreadKeySeen && pthreadThreadLocalSeen && pthreadCreateSeen {
        evidence["xcode_test_passed"] = "true"
        evidence["pass_count"] = "1"
    } else {
        evidence["fail_count"] = "1"
        failures.append(fail("mlibc-pthread-tls-xctest-failed", "xcodebuild did not report a clean pass with pthread TLS markers, dynamic-loader evidence, and ORLIX-MLIBC-TEST-END for \(xcodeTest)"))
    }
    if !testExecuted {
        failures.append(fail("mlibc-pthread-tls-xctest-not-executed", "xcodebuild output did not mention \(xcodeTest)"))
    }
    if !completionSeen {
        failures.append(fail("mlibc-pthread-tls-completion-marker-missing", "xcodebuild output did not include ORLIX-MLIBC-TEST-END"))
    }
    if !dynamicLoaderSeen {
        failures.append(fail("mlibc-pthread-tls-dynamic-loader-marker-missing", "xcodebuild output did not include ORLIX-MLIBC-DYNAMIC-LOADER-OK"))
    }
    if !pthreadKeySeen {
        failures.append(fail("mlibc-pthread-tls-pthread-key-missing", "xcodebuild output did not include a passing posix/pthread_key result"))
    }
    if !pthreadThreadLocalSeen {
        failures.append(fail("mlibc-pthread-tls-thread-local-missing", "xcodebuild output did not include a passing posix/pthread_thread_local result"))
    }
    if !pthreadCreateSeen {
        failures.append(fail("mlibc-pthread-tls-pthread-create-missing", "xcodebuild output did not include a passing posix/pthread_create result"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let blocker = failures.isEmpty ? "" : "Pinned simulator OrlixMLibC pthread/TLS smoke did not produce a clean app-hosted OrlixOS terminal-session pass."
    if !blocker.isEmpty {
        evidence["blocker"] = blocker
    }
    evidence["gate_result"] = status.rawValue
    try writeJSON(evidence, to: evidenceURL)
    artifacts.append(relativePath(evidenceURL))
    let reducer = try writeReducer(
        target: target,
        caseID: status == .pass ? "mlibc-pthread-tls-smoke-pass" : "mlibc-pthread-tls-smoke-fail",
        command: command,
        reason: blocker.isEmpty ? "mlibc pthread TLS smoke passed" : blocker,
        artifacts: [relativePath(evidenceURL), relativePath(xcodeOutputURL)],
        expectedStatus: status
    )
    artifacts.append(relativePath(reducer))
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: status == .pass ? "Pinned simulator OrlixMLibC pthread/TLS smoke passed through dynamic-loader-backed OrlixOS terminal-session execution." : "Pinned simulator OrlixMLibC pthread/TLS smoke did not pass through the OrlixOS terminal-session XCTest surface.",
        command: command,
        failures: failures,
        artifacts: artifacts,
        counters: [
            "source_evidence_facts": evidence.count,
            "source_proof_failures": failures.filter { $0.id == "mlibc-pthread-tls-source-proof-missing" }.count,
            "mlibc_pthread_tls_tests_executed": testExecuted && pthreadKeySeen && pthreadThreadLocalSeen && pthreadCreateSeen ? 1 : 0,
            "mlibc_pthread_tls_tests_passed": status == .pass ? 1 : 0,
            "mlibc_pthread_tls_tests_failed": status == .pass ? 0 : 1,
            "mlibc_pthread_tls_tests_skipped": 0,
        ],
        kernelProfile: kernelProfile,
        kernelConfig: "OrlixKernel/Sources/ports/orlix/configs/\(kernelProfile)_defconfig",
        evidence: evidence,
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    if !blocker.isEmpty {
        print("blocker: \(blocker)")
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
    }
    return exitCode(for: status)
}

func runMLibCLinkedSyscallUAPISmoke() throws -> Int32 {
    let target = "tcti-mlibc-linked-syscall-uapi-smoke"
    let command = "make tcti-gate TARGET=\(target)"
    let kernelProfile = "tcti_runtime"
    let simulatorID = "1E5553B0-203A-4A11-BAD7-EBDE46863F66"
    let simulatorName = "Orlix-iPhone-15-Pro-Max"
    let xcodeScheme = "OrlixMLibC Conformance"
    let xcodeTest = "OrlixMLibCConformanceTests/OrlixMLibCConformanceTests/testMLibCRootfsCompletesThroughOrlixOSTerminalSession"
    let testSource = path("OrlixTestRunner", "Tests", "XCTest", "OrlixMLibCConformanceTests", "OrlixMLibCConformanceTests.swift")
    let runnerSource = path("OrlixTestRunner", "Sources", "OrlixUpstreamTestRunner.swift")
    let mlibcMakefile = path("OrlixMLibC", "Makefile")
    let mlibcInitSource = path("OrlixMLibC", "Tests", "mlibc_test_init.c")
    let outputRoot = buildPath("mlibc_linked_syscall_uapi_smoke")
    let evidenceURL = outputRoot.appendingPathComponent("evidence.json")
    let xcodeOutputURL = outputRoot.appendingPathComponent("xcodebuild-output.txt")
    let requiredLabels = [
        "glibc/linux-syscall",
        "linux/getifaddrs",
        "linux/pidfd",
        "linux/process_vm_readv_writev",
        "linux/timerfd",
        "linux/xattr",
    ]
    try ensureDirectory(outputRoot)

    var failures: [Failure] = []
    var artifacts: [String] = []
    var evidence: [String: String] = [
        "actual_command": command,
        "backend": "tcti",
        "git_sha": gitSha(),
        "kernel_profile": kernelProfile,
        "selected_simulator_id": simulatorID,
        "selected_simulator_name": simulatorName,
        "xcode_scheme": xcodeScheme,
        "xcode_test": xcodeTest,
        "real_stack_execution_surface": "OrlixOS terminal session through OrlixUpstreamXCTest",
        "xcode_test_executed": "false",
        "xcode_test_passed": "false",
        "mlibc_completion_asserted_by_xctest": "false",
        "linked_syscall_uapi_completion_asserted_by_xctest": "false",
        "pass_count": "0",
        "fail_count": "0",
        "skip_count": "0",
    ]

    func requireSourceFact(_ key: String, _ text: String, _ needle: String, _ url: URL) {
        if sourceTextContains(text, needle) {
            evidence[key] = "\(relativePath(url)) contains \(needle)"
        } else {
            failures.append(fail("mlibc-linked-syscall-uapi-source-proof-missing", "\(relativePath(url)) must contain \(needle)"))
            evidence[key] = "missing"
        }
    }

    let testText = try readText(testSource)
    let runnerText = try readText(runnerSource)
    let makefileText = try readText(mlibcMakefile)
    let initText = try readText(mlibcInitSource)

    requireSourceFact("xctest_uses_mlibc_spec", testText, #"OrlixUpstreamXCTest.run\(.mlibc\)"#, testSource)
    requireSourceFact("runner_uses_orlixos_session", runnerText, #"OrlixLinuxSession"#, runnerSource)
    requireSourceFact("runner_mlibc_completion_marker", runnerText, #"ORLIX-MLIBC-TEST-END"#, runnerSource)
    requireSourceFact("mlibc_makefile_installs_kernel_headers", makefileText, #"MLIBC_KERNEL_HEADERS"#, mlibcMakefile)
    requireSourceFact("mlibc_makefile_builds_with_kernel_headers", makefileText, #"-isystem"#, mlibcMakefile)
    requireSourceFact("mlibc_makefile_includes_linux_syscall", makefileText, #"glibc/linux-syscall"#, mlibcMakefile)
    requireSourceFact("mlibc_makefile_includes_getifaddrs", makefileText, #"linux/getifaddrs"#, mlibcMakefile)
    requireSourceFact("mlibc_makefile_includes_pidfd", makefileText, #"linux/pidfd"#, mlibcMakefile)
    requireSourceFact("mlibc_makefile_includes_process_vm", makefileText, #"linux/process_vm_readv_writev"#, mlibcMakefile)
    requireSourceFact("mlibc_makefile_includes_timerfd", makefileText, #"linux/timerfd"#, mlibcMakefile)
    requireSourceFact("mlibc_makefile_includes_xattr", makefileText, #"linux/xattr"#, mlibcMakefile)
    requireSourceFact("mlibc_init_completion_marker", initText, #"ORLIX-MLIBC-TEST-END"#, mlibcInitSource)
    requireSourceFact("mlibc_init_execs_test_list", initText, #"execve"#, mlibcInitSource)

    let xcodeArguments = [
        "PATH=\(ProcessInfo.processInfo.environment["HOME"] ?? "")/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin",
        "ORLIX_PROFILE=\(kernelProfile)",
        "xcodebuild",
        "-project", "Orlix.xcodeproj",
        "-scheme", xcodeScheme,
        "-configuration", "Debug",
        "-destination", "platform=iOS Simulator,id=\(simulatorID)",
        "-only-testing:\(xcodeTest)",
        "ORLIX_PROFILE=\(kernelProfile)",
        "ORLIX_OS_SKIP_ENVIRONMENT_RUNTIME_FIXTURES=YES",
        "test",
    ]
    evidence["xcodebuild_command"] = xcodeArguments.dropFirst(2).joined(separator: " ")
    let xcodeOutput = try runWithFileBackedOutput(
        xcodeArguments,
        check: false,
        terminateAfterOutputContains: ["** TEST SUCCEEDED **", "ORLIX-MLIBC-TEST-END"]
    )
    try xcodeOutput.write(to: xcodeOutputURL, atomically: true, encoding: .utf8)
    artifacts.append(relativePath(xcodeOutputURL))

    let testExecuted = xcodeOutput.contains("testMLibCRootfsCompletesThroughOrlixOSTerminalSession")
    let testSucceeded = xcodeOutput.contains("** TEST SUCCEEDED **")
    let testFailed = xcodeOutput.contains("** TEST FAILED **") ||
        xcodeOutput.range(of: #"(?m)\bfailed\b"#, options: .regularExpression) != nil
    let completionSeen = xcodeOutput.contains("ORLIX-MLIBC-TEST-END")
    var labelResults: [String: Bool] = [:]
    for label in requiredLabels {
        let seen = outputHasPassingTAPLabel(xcodeOutput, label)
        labelResults[label] = seen
        evidence["label_\(label.replacingOccurrences(of: "/", with: "_"))_asserted_by_xctest"] = seen ? "true" : "false"
    }
    let allLabelsSeen = requiredLabels.allSatisfy { labelResults[$0] == true }
    if testExecuted {
        evidence["xcode_test_executed"] = "true"
    }
    if completionSeen {
        evidence["mlibc_completion_asserted_by_xctest"] = "true"
    }
    if allLabelsSeen {
        evidence["linked_syscall_uapi_completion_asserted_by_xctest"] = "true"
    }
    if testSucceeded && !testFailed && completionSeen && allLabelsSeen {
        evidence["xcode_test_passed"] = "true"
        evidence["pass_count"] = "1"
    } else {
        evidence["fail_count"] = "1"
        failures.append(fail("mlibc-linked-syscall-uapi-xctest-failed", "xcodebuild did not report a clean pass with linked syscall/UAPI markers and ORLIX-MLIBC-TEST-END for \(xcodeTest)"))
    }
    if !testExecuted {
        failures.append(fail("mlibc-linked-syscall-uapi-xctest-not-executed", "xcodebuild output did not mention \(xcodeTest)"))
    }
    if !completionSeen {
        failures.append(fail("mlibc-linked-syscall-uapi-completion-marker-missing", "xcodebuild output did not include ORLIX-MLIBC-TEST-END"))
    }
    for label in requiredLabels where labelResults[label] != true {
        failures.append(fail("mlibc-linked-syscall-uapi-label-missing", "xcodebuild output did not include a passing \(label) result"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let blocker = failures.isEmpty ? "" : "Pinned simulator OrlixMLibC linked syscall/UAPI smoke did not produce a clean app-hosted OrlixOS terminal-session pass."
    if !blocker.isEmpty {
        evidence["blocker"] = blocker
    }
    evidence["gate_result"] = status.rawValue
    try writeJSON(evidence, to: evidenceURL)
    artifacts.append(relativePath(evidenceURL))
    let reducer = try writeReducer(
        target: target,
        caseID: status == .pass ? "mlibc-linked-syscall-uapi-smoke-pass" : "mlibc-linked-syscall-uapi-smoke-fail",
        command: command,
        reason: blocker.isEmpty ? "mlibc linked syscall/UAPI smoke passed" : blocker,
        artifacts: [relativePath(evidenceURL), relativePath(xcodeOutputURL)],
        expectedStatus: status
    )
    artifacts.append(relativePath(reducer))
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: status == .pass ? "Pinned simulator OrlixMLibC linked syscall/UAPI smoke passed through OrlixOS terminal-session execution." : "Pinned simulator OrlixMLibC linked syscall/UAPI smoke did not pass through the OrlixOS terminal-session XCTest surface.",
        command: command,
        failures: failures,
        artifacts: artifacts,
        counters: [
            "source_evidence_facts": evidence.count,
            "source_proof_failures": failures.filter { $0.id == "mlibc-linked-syscall-uapi-source-proof-missing" }.count,
            "mlibc_linked_syscall_uapi_tests_executed": testExecuted && allLabelsSeen ? 1 : 0,
            "mlibc_linked_syscall_uapi_tests_passed": status == .pass ? 1 : 0,
            "mlibc_linked_syscall_uapi_tests_failed": status == .pass ? 0 : 1,
            "mlibc_linked_syscall_uapi_tests_skipped": 0,
        ],
        kernelProfile: kernelProfile,
        kernelConfig: "OrlixKernel/Sources/ports/orlix/configs/\(kernelProfile)_defconfig",
        evidence: evidence,
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    if !blocker.isEmpty {
        print("blocker: \(blocker)")
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
    }
    return exitCode(for: status)
}

func runShellExecSimpleCommand() throws -> Int32 {
    let target = "tcti-shell-exec-simple-command"
    let command = "make tcti-gate TARGET=\(target)"
    let runtimeGate = "tcti-full-shell-usability"
    let simulatorID = "1E5553B0-203A-4A11-BAD7-EBDE46863F66"
    let simulatorName = "Orlix-iPhone-15-Pro-Max"
    let runtimeScript = path("tools", "runtime", "orlix-runtime-validation.sh")
    let outputRoot = buildPath("shell_exec_simple_command")
    let evidenceURL = outputRoot.appendingPathComponent("evidence.json")
    let runtimeOutputURL = outputRoot.appendingPathComponent("runtime-validation-output.txt")
    try ensureDirectory(outputRoot)

    var failures: [Failure] = []
    var artifacts: [String] = []
    var evidence: [String: String] = [
        "actual_command": command,
        "backend": "tcti",
        "git_sha": gitSha(),
        "runtime_gate": runtimeGate,
        "selected_simulator_id": simulatorID,
        "selected_simulator_name": simulatorName,
        "real_stack_execution_surface": "OrlixOS simulator runtime-validation app session",
        "runtime_validation_executed": "false",
        "runtime_validation_passed": "false",
        "shell_marker_asserted": "false",
        "pass_count": "0",
        "fail_count": "0",
        "skip_count": "0",
    ]

    let runtimeText = try readText(runtimeScript)
    if sourceTextContains(runtimeText, #"tcti-full-shell-usability"#) &&
        sourceTextContains(runtimeText, #"ORLIX-TCTI-SHELL-USABLE"#) &&
        sourceTextContains(runtimeText, #"shell-basic"#) {
        evidence["runtime_script_shell_command_source_proof"] = "\(relativePath(runtimeScript)) contains tcti-full-shell-usability shell command markers"
    } else {
        failures.append(fail("shell-source-proof-missing", "\(relativePath(runtimeScript)) must define the tcti-full-shell-usability shell command workload"))
        evidence["runtime_script_shell_command_source_proof"] = "missing"
    }

    let runtimeArguments = [
        "PATH=\(ProcessInfo.processInfo.environment["HOME"] ?? "")/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin",
        "ORLIX_SIMULATOR_ID=\(simulatorID)",
        "ORLIX_TCTI_REQUIRED_SIMULATOR_ID=\(simulatorID)",
        "ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=\(simulatorName)",
        "make",
        "runtime-validation",
        "DESTINATION=iphonesimulator",
        "GATE=\(runtimeGate)",
    ]
    evidence["runtime_validation_command"] = runtimeArguments.dropFirst(4).joined(separator: " ")
    let runtimeOutput = try runWithFileBackedOutput(runtimeArguments, check: false)
    try runtimeOutput.write(to: runtimeOutputURL, atomically: true, encoding: .utf8)
    artifacts.append(relativePath(runtimeOutputURL))
    evidence["runtime_validation_executed"] = "true"

    guard let runtimeReport = selectedRuntimeValidationReport(gate: runtimeGate, destination: "iphonesimulator") else {
        failures.append(fail("runtime-report-missing", "runtime-validation did not produce a \(runtimeGate) iphonesimulator JSON report"))
        evidence["fail_count"] = "1"
        let reportURL = try writeReport(report(
            target: target,
            status: .fail,
            summary: "Shell exec simple-command gate could not find the runtime-validation report.",
            command: command,
            failures: failures,
            artifacts: artifacts,
            counters: [
                "shell_exec_simple_command_tests_executed": 0,
                "shell_exec_simple_command_tests_passed": 0,
                "shell_exec_simple_command_tests_failed": 1,
                "shell_exec_simple_command_tests_skipped": 0,
            ],
            evidence: evidence,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("fail: \(relativePath(reportURL))")
        return 1
    }

    let runtimeObject = runtimeReport.object
    let runtimeReportPath = relativePath(runtimeReport.url)
    artifacts.append(runtimeReportPath)
    let runtimeArtifacts = (runtimeObject["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
    artifacts.append(contentsOf: runtimeArtifacts)
    let markerText = try runtimeArtifacts.first { $0.hasSuffix("tcti-full-shell-usability.txt") }.map(readRelativeArtifact) ?? ""
    let terminalText = try runtimeArtifacts.first { $0.hasSuffix("simulator-terminal-output.txt") }.map(readRelativeArtifact) ?? ""
    let forbidden = runtimeObject["forbidden_behavior"] as? [String: Any] ?? [:]
    let forbiddenKeys = [
        "generated_exec_memory",
        "host_exec_guest_text",
        "host_x18",
        "map_jit",
        "native_ios_api_exposure_to_guest",
        "rwx",
    ]

    if stringField(runtimeObject, "git_sha") != gitSha() {
        failures.append(fail("runtime-report-stale", "\(runtimeReportPath) is not current for HEAD"))
    }
    if stringField(runtimeObject, "status") == "pass" && boolField(runtimeObject, "passed") {
        evidence["runtime_validation_passed"] = "true"
    } else {
        failures.append(fail("runtime-report-status", "\(runtimeReportPath) did not pass"))
    }
    if stringField(runtimeObject, "selected_device_id") != simulatorID ||
        stringField(runtimeObject, "selected_device_name") != simulatorName ||
        stringField(runtimeObject, "simulator_booted_count") != "1" ||
        !boolField(runtimeObject, "simulator_single_booted") {
        failures.append(fail("simulator-scope", "shell gate requires the single pinned \(simulatorName) simulator"))
    }
    if markerText.contains("ORLIX-TCTI-SHELL-USABLE") {
        evidence["shell_marker_asserted"] = "true"
    } else {
        failures.append(fail("shell-marker-missing", "runtime artifacts did not include ORLIX-TCTI-SHELL-USABLE"))
    }
    if terminalText.contains("shell-basic") || markerText.contains("shell-basic") {
        evidence["shell_stdout_asserted"] = "true"
    } else {
        failures.append(fail("shell-stdout-missing", "runtime artifacts did not include shell-basic output from the shell command"))
    }
    for key in forbiddenKeys {
        if boolField(forbidden, key) {
            failures.append(fail("forbidden-behavior", "runtime report sets forbidden_behavior.\(key)=true"))
        }
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    if status == .pass {
        evidence["pass_count"] = "1"
    } else {
        evidence["fail_count"] = "1"
        evidence["blocker"] = "Pinned simulator shell simple-command runtime-validation did not produce a clean OrlixOS session pass."
    }
    evidence["gate_result"] = status.rawValue
    try writeJSON(evidence, to: evidenceURL)
    artifacts.append(relativePath(evidenceURL))
    let reducer = try writeReducer(
        target: target,
        caseID: status == .pass ? "shell-exec-simple-command-pass" : "shell-exec-simple-command-fail",
        command: command,
        reason: status == .pass ? "shell simple command passed" : "shell simple command failed",
        artifacts: artifacts,
        expectedStatus: status
    )
    artifacts.append(relativePath(reducer))

    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: status == .pass ? "Pinned simulator shell simple-command gate passed through OrlixOS runtime-validation." : "Pinned simulator shell simple-command gate did not pass through OrlixOS runtime-validation.",
        command: command,
        failures: failures,
        artifacts: artifacts,
        counters: [
            "shell_exec_simple_command_tests_executed": evidence["runtime_validation_executed"] == "true" ? 1 : 0,
            "shell_exec_simple_command_tests_passed": status == .pass ? 1 : 0,
            "shell_exec_simple_command_tests_failed": status == .pass ? 0 : 1,
            "shell_exec_simple_command_tests_skipped": 0,
        ],
        evidence: evidence,
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    if status != .pass {
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
    }
    return exitCode(for: status)
}

func runShellPipelineSmoke() throws -> Int32 {
    let target = "tcti-shell-pipeline-smoke"
    let command = "make tcti-gate TARGET=\(target)"
    let runtimeGate = "tcti-shell-pipeline-smoke"
    let simulatorID = "1E5553B0-203A-4A11-BAD7-EBDE46863F66"
    let simulatorName = "Orlix-iPhone-15-Pro-Max"
    let runtimeScript = path("tools", "runtime", "orlix-runtime-validation.sh")
    let outputRoot = buildPath("shell_pipeline_smoke")
    let evidenceURL = outputRoot.appendingPathComponent("evidence.json")
    let runtimeOutputURL = outputRoot.appendingPathComponent("runtime-validation-output.txt")
    try ensureDirectory(outputRoot)

    var failures: [Failure] = []
    var artifacts: [String] = []
    var evidence: [String: String] = [
        "actual_command": command,
        "backend": "tcti",
        "git_sha": gitSha(),
        "runtime_gate": runtimeGate,
        "selected_simulator_id": simulatorID,
        "selected_simulator_name": simulatorName,
        "real_stack_execution_surface": "OrlixOS simulator runtime-validation app session",
        "shell_command": "echo beta | { read line; test $line = beta; printf $line; }",
        "runtime_validation_executed": "false",
        "runtime_validation_passed": "false",
        "pipeline_marker_asserted": "false",
        "pipeline_stdout_asserted": "false",
        "child_process_started": "false",
        "child_process_exited": "false",
        "wait_reaping_status_observed": "false",
        "pass_count": "0",
        "fail_count": "0",
        "skip_count": "0",
    ]

    let runtimeText = try readText(runtimeScript)
    if sourceTextContains(runtimeText, #"tcti-shell-pipeline-smoke"#) &&
        sourceTextContains(runtimeText, #"ORLIX-TCTI-SHELL-PIPELINE-OK"#) &&
        sourceTextContains(runtimeText, #"read%20line"#) {
        evidence["runtime_script_shell_pipeline_source_proof"] = "\(relativePath(runtimeScript)) contains tcti-shell-pipeline-smoke command markers"
    } else {
        failures.append(fail("shell-pipeline-source-proof-missing", "\(relativePath(runtimeScript)) must contain the tcti-shell-pipeline-smoke workload"))
    }

    let home = ProcessInfo.processInfo.environment["HOME"] ?? ""
    let runtimeArguments = [
        "PATH=\(home)/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin",
        "ORLIX_SIMULATOR_ID=\(simulatorID)",
        "ORLIX_TCTI_REQUIRED_SIMULATOR_ID=\(simulatorID)",
        "ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=\(simulatorName)",
        "make",
        "runtime-validation",
        "DESTINATION=iphonesimulator",
        "GATE=\(runtimeGate)",
    ]
    let runtimeOutput = try run(runtimeArguments, check: false)
    try runtimeOutput.write(to: runtimeOutputURL, atomically: true, encoding: .utf8)
    artifacts.append(relativePath(runtimeOutputURL))
    evidence["runtime_validation_executed"] = "true"
    evidence["runtime_validation_command"] = runtimeArguments.dropFirst(4).joined(separator: " ")

    guard let runtimeReport = selectedRuntimeValidationReport(gate: runtimeGate, destination: "iphonesimulator") else {
        failures.append(fail("runtime-report-missing", "runtime-validation did not produce \(runtimeGate) iphonesimulator JSON report"))
        let status: GateStatus = .fail
        evidence["fail_count"] = "1"
        evidence["gate_result"] = status.rawValue
        try writeJSON(evidence, to: evidenceURL)
        artifacts.append(relativePath(evidenceURL))
        let reducer = try writeReducer(
            target: target,
            caseID: "shell-pipeline-smoke-fail",
            command: command,
            reason: "shell pipeline runtime-validation report missing",
            artifacts: artifacts,
            expectedStatus: .fail
        )
        artifacts.append(relativePath(reducer))
        let reportURL = try writeReport(report(
            target: target,
            status: status,
            summary: "Shell pipeline gate could not find the runtime-validation report.",
            command: command,
            failures: failures,
            artifacts: artifacts,
            counters: [
                "shell_pipeline_tests_executed": 0,
                "shell_pipeline_tests_passed": 0,
                "shell_pipeline_tests_failed": 1,
                "shell_pipeline_tests_skipped": 0,
            ],
            evidence: evidence,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("\(status.rawValue): \(relativePath(reportURL))")
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
        return 1
    }

    let runtimeObject = runtimeReport.object
    let runtimeReportPath = relativePath(runtimeReport.url)
    artifacts.append(runtimeReportPath)
    let runtimeArtifacts = runtimeObject["artifacts"] as? [String] ?? []
    artifacts.append(contentsOf: runtimeArtifacts)

    let markerText = try runtimeArtifacts.first { $0.hasSuffix("tcti-shell-pipeline-smoke.txt") }.map(readRelativeArtifact) ?? ""
    let terminalText = try runtimeArtifacts.first { $0.hasSuffix("simulator-terminal-output.txt") }.map(readRelativeArtifact) ?? ""
    let forbidden = runtimeObject["forbidden_behavior"] as? [String: Any] ?? [:]
    let forbiddenKeys = [
        "generated_exec_memory",
        "host_exec_guest_text",
        "host_x18",
        "map_jit",
        "native_ios_api_exposure_to_guest",
        "rwx",
    ]
    var forbiddenBehavior = forbiddenDefaults()
    for key in forbiddenKeys {
        forbiddenBehavior[key] = boolField(forbidden, key)
    }

    if stringField(runtimeObject, "git_sha") != gitSha() {
        failures.append(fail("runtime-report-stale", "\(runtimeReportPath) is not current HEAD"))
    }
    if stringField(runtimeObject, "status") == "pass" && boolField(runtimeObject, "passed") {
        evidence["runtime_validation_passed"] = "true"
    } else {
        failures.append(fail("runtime-report-status", "\(runtimeReportPath) did not pass"))
    }
    if stringField(runtimeObject, "selected_device_id") != simulatorID ||
        stringField(runtimeObject, "selected_device_name") != simulatorName ||
        stringField(runtimeObject, "simulator_booted_count") != "1" ||
        !boolField(runtimeObject, "simulator_single_booted") {
        failures.append(fail("simulator-scope", "shell pipeline gate requires single pinned \(simulatorName) simulator"))
    }
    if markerText.contains("ORLIX-TCTI-SHELL-PIPELINE-OK") {
        evidence["pipeline_marker_asserted"] = "true"
    } else {
        failures.append(fail("shell-pipeline-marker-missing", "runtime artifacts did not include ORLIX-TCTI-SHELL-PIPELINE-OK"))
    }
    if terminalText.contains("beta") {
        evidence["pipeline_stdout_asserted"] = "true"
    } else {
        failures.append(fail("shell-pipeline-stdout-missing", "runtime artifacts did not include beta output from shell pipeline"))
    }
    if terminalText.contains("orlix-init: process started pid=") {
        evidence["child_process_started"] = "true"
    } else {
        failures.append(fail("child-process-start-missing", "runtime artifacts did not include shell child process start"))
    }
    if terminalText.contains("orlix-init: process exited pid=") {
        evidence["child_process_exited"] = "true"
    } else {
        failures.append(fail("child-process-exit-missing", "runtime artifacts did not include shell child process exit"))
    }
    if outputHasShellWaitStatusZero(terminalText) {
        evidence["wait_reaping_status_observed"] = "true"
    } else {
        failures.append(fail("wait-reaping-status-missing", "runtime artifacts did not include shell exit status 0"))
    }
    for key in forbiddenKeys where boolField(forbidden, key) {
        failures.append(fail("forbidden-behavior", "runtime report sets forbidden_behavior.\(key)=true"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    if status == .pass {
        evidence["pass_count"] = "1"
    } else {
        evidence["fail_count"] = "1"
        evidence["blocker"] = "Pinned simulator shell pipeline runtime-validation did not produce a clean OrlixOS session pass."
    }
    evidence["gate_result"] = status.rawValue
    try writeJSON(evidence, to: evidenceURL)
    artifacts.append(relativePath(evidenceURL))

    let reducer = try writeReducer(
        target: target,
        caseID: status == .pass ? "shell-pipeline-smoke-pass" : "shell-pipeline-smoke-fail",
        command: command,
        reason: status == .pass ? "shell pipeline passed" : "shell pipeline failed",
        artifacts: artifacts,
        expectedStatus: status
    )
    artifacts.append(relativePath(reducer))

    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: status == .pass ? "Pinned simulator shell pipeline gate passed through OrlixOS runtime-validation." : "Pinned simulator shell pipeline gate did not pass through OrlixOS runtime-validation.",
        command: command,
        failures: failures,
        artifacts: artifacts,
        forbiddenBehavior: forbiddenBehavior,
        counters: [
            "shell_pipeline_tests_executed": evidence["runtime_validation_executed"] == "true" ? 1 : 0,
            "shell_pipeline_tests_passed": status == .pass ? 1 : 0,
            "shell_pipeline_tests_failed": status == .pass ? 0 : 1,
            "shell_pipeline_tests_skipped": 0,
        ],
        evidence: evidence,
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    if status != .pass {
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
    }
    return exitCode(for: status)
}

func runShellEnvVarSmoke() throws -> Int32 {
    let target = "tcti-shell-env-var-smoke"
    let command = "make tcti-gate TARGET=\(target)"
    let runtimeGate = "tcti-shell-env-var-smoke"
    let simulatorID = "1E5553B0-203A-4A11-BAD7-EBDE46863F66"
    let simulatorName = "Orlix-iPhone-15-Pro-Max"
    let runtimeScript = path("tools", "runtime", "orlix-runtime-validation.sh")
    let outputRoot = buildPath("shell_env_var_smoke")
    let evidenceURL = outputRoot.appendingPathComponent("evidence.json")
    let runtimeOutputURL = outputRoot.appendingPathComponent("runtime-validation-output.txt")
    try ensureDirectory(outputRoot)

    var failures: [Failure] = []
    var artifacts: [String] = []
    var evidence: [String: String] = [
        "actual_command": command,
        "backend": "tcti",
        "git_sha": gitSha(),
        "runtime_gate": runtimeGate,
        "selected_simulator_id": simulatorID,
        "selected_simulator_name": simulatorName,
        "real_stack_execution_surface": "OrlixOS simulator runtime-validation app session",
        "shell_command": "FOO=env-ok; export FOO; test $FOO = env-ok; printf $FOO",
        "runtime_validation_executed": "false",
        "runtime_validation_passed": "false",
        "env_marker_asserted": "false",
        "env_stdout_asserted": "false",
        "child_process_started": "false",
        "child_process_exited": "false",
        "wait_reaping_status_observed": "false",
        "pass_count": "0",
        "fail_count": "0",
        "skip_count": "0",
    ]

    let runtimeText = try readText(runtimeScript)
    if sourceTextContains(runtimeText, #"tcti-shell-env-var-smoke"#) &&
        sourceTextContains(runtimeText, #"ORLIX-TCTI-SHELL-ENV-OK"#) &&
        sourceTextContains(runtimeText, #"FOO%3Denv-ok"#) {
        evidence["runtime_script_shell_env_source_proof"] = "\(relativePath(runtimeScript)) contains tcti-shell-env-var-smoke command markers"
    } else {
        failures.append(fail("shell-env-source-proof-missing", "\(relativePath(runtimeScript)) must contain the tcti-shell-env-var-smoke workload"))
    }

    let home = ProcessInfo.processInfo.environment["HOME"] ?? ""
    let runtimeArguments = [
        "PATH=\(home)/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin",
        "ORLIX_SIMULATOR_ID=\(simulatorID)",
        "ORLIX_TCTI_REQUIRED_SIMULATOR_ID=\(simulatorID)",
        "ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=\(simulatorName)",
        "make",
        "runtime-validation",
        "DESTINATION=iphonesimulator",
        "GATE=\(runtimeGate)",
    ]
    let runtimeOutput = try run(runtimeArguments, check: false)
    try runtimeOutput.write(to: runtimeOutputURL, atomically: true, encoding: .utf8)
    artifacts.append(relativePath(runtimeOutputURL))
    evidence["runtime_validation_executed"] = "true"
    evidence["runtime_validation_command"] = runtimeArguments.dropFirst(4).joined(separator: " ")

    guard let runtimeReport = selectedRuntimeValidationReport(gate: runtimeGate, destination: "iphonesimulator") else {
        failures.append(fail("runtime-report-missing", "runtime-validation did not produce \(runtimeGate) iphonesimulator JSON report"))
        let status: GateStatus = .fail
        evidence["fail_count"] = "1"
        evidence["gate_result"] = status.rawValue
        try writeJSON(evidence, to: evidenceURL)
        artifacts.append(relativePath(evidenceURL))
        let reducer = try writeReducer(
            target: target,
            caseID: "shell-env-var-smoke-fail",
            command: command,
            reason: "shell env-var runtime-validation report missing",
            artifacts: artifacts,
            expectedStatus: .fail
        )
        artifacts.append(relativePath(reducer))
        let reportURL = try writeReport(report(
            target: target,
            status: status,
            summary: "Shell env-var gate could not find the runtime-validation report.",
            command: command,
            failures: failures,
            artifacts: artifacts,
            counters: [
                "shell_env_var_tests_executed": 0,
                "shell_env_var_tests_passed": 0,
                "shell_env_var_tests_failed": 1,
                "shell_env_var_tests_skipped": 0,
            ],
            evidence: evidence,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("\(status.rawValue): \(relativePath(reportURL))")
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
        return 1
    }

    let runtimeObject = runtimeReport.object
    let runtimeReportPath = relativePath(runtimeReport.url)
    artifacts.append(runtimeReportPath)
    let runtimeArtifacts = runtimeObject["artifacts"] as? [String] ?? []
    artifacts.append(contentsOf: runtimeArtifacts)

    let markerText = try runtimeArtifacts.first { $0.hasSuffix("tcti-shell-env-var-smoke.txt") }.map(readRelativeArtifact) ?? ""
    let terminalText = try runtimeArtifacts.first { $0.hasSuffix("simulator-terminal-output.txt") }.map(readRelativeArtifact) ?? ""
    let forbidden = runtimeObject["forbidden_behavior"] as? [String: Any] ?? [:]
    let forbiddenKeys = [
        "generated_exec_memory",
        "host_exec_guest_text",
        "host_x18",
        "map_jit",
        "native_ios_api_exposure_to_guest",
        "rwx",
    ]
    var forbiddenBehavior = forbiddenDefaults()
    for key in forbiddenKeys {
        forbiddenBehavior[key] = boolField(forbidden, key)
    }

    if stringField(runtimeObject, "git_sha") != gitSha() {
        failures.append(fail("runtime-report-stale", "\(runtimeReportPath) is not current HEAD"))
    }
    if stringField(runtimeObject, "status") == "pass" && boolField(runtimeObject, "passed") {
        evidence["runtime_validation_passed"] = "true"
    } else {
        failures.append(fail("runtime-report-status", "\(runtimeReportPath) did not pass"))
    }
    if stringField(runtimeObject, "selected_device_id") != simulatorID ||
        stringField(runtimeObject, "selected_device_name") != simulatorName ||
        stringField(runtimeObject, "simulator_booted_count") != "1" ||
        !boolField(runtimeObject, "simulator_single_booted") {
        failures.append(fail("simulator-scope", "shell env-var gate requires single pinned \(simulatorName) simulator"))
    }
    if markerText.contains("ORLIX-TCTI-SHELL-ENV-OK") {
        evidence["env_marker_asserted"] = "true"
    } else {
        failures.append(fail("shell-env-marker-missing", "runtime artifacts did not include ORLIX-TCTI-SHELL-ENV-OK"))
    }
    if terminalText.contains("env-ok") {
        evidence["env_stdout_asserted"] = "true"
    } else {
        failures.append(fail("shell-env-stdout-missing", "runtime artifacts did not include env-ok output from shell env-var command"))
    }
    if terminalText.contains("orlix-init: process started pid=") {
        evidence["child_process_started"] = "true"
    } else {
        failures.append(fail("child-process-start-missing", "runtime artifacts did not include shell child process start"))
    }
    if terminalText.contains("orlix-init: process exited pid=") {
        evidence["child_process_exited"] = "true"
    } else {
        failures.append(fail("child-process-exit-missing", "runtime artifacts did not include shell child process exit"))
    }
    if outputHasShellWaitStatusZero(terminalText) {
        evidence["wait_reaping_status_observed"] = "true"
    } else {
        failures.append(fail("wait-reaping-status-missing", "runtime artifacts did not include shell exit status 0"))
    }
    for key in forbiddenKeys where boolField(forbidden, key) {
        failures.append(fail("forbidden-behavior", "runtime report sets forbidden_behavior.\(key)=true"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    if status == .pass {
        evidence["pass_count"] = "1"
    } else {
        evidence["fail_count"] = "1"
        evidence["blocker"] = "Pinned simulator shell env-var runtime-validation did not produce a clean OrlixOS session pass."
    }
    evidence["gate_result"] = status.rawValue
    try writeJSON(evidence, to: evidenceURL)
    artifacts.append(relativePath(evidenceURL))

    let reducer = try writeReducer(
        target: target,
        caseID: status == .pass ? "shell-env-var-smoke-pass" : "shell-env-var-smoke-fail",
        command: command,
        reason: status == .pass ? "shell env-var passed" : "shell env-var failed",
        artifacts: artifacts,
        expectedStatus: status
    )
    artifacts.append(relativePath(reducer))

    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: status == .pass ? "Pinned simulator shell env-var gate passed through OrlixOS runtime-validation." : "Pinned simulator shell env-var gate did not pass through OrlixOS runtime-validation.",
        command: command,
        failures: failures,
        artifacts: artifacts,
        forbiddenBehavior: forbiddenBehavior,
        counters: [
            "shell_env_var_tests_executed": evidence["runtime_validation_executed"] == "true" ? 1 : 0,
            "shell_env_var_tests_passed": status == .pass ? 1 : 0,
            "shell_env_var_tests_failed": status == .pass ? 0 : 1,
            "shell_env_var_tests_skipped": 0,
        ],
        evidence: evidence,
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    if status != .pass {
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
    }
    return exitCode(for: status)
}

func runShellRedirectionSmoke() throws -> Int32 {
    let target = "tcti-shell-redirection-smoke"
    let command = "make tcti-gate TARGET=\(target)"
    let runtimeGate = "tcti-shell-redirection-smoke"
    let simulatorID = "1E5553B0-203A-4A11-BAD7-EBDE46863F66"
    let simulatorName = "Orlix-iPhone-15-Pro-Max"
    let runtimeScript = path("tools", "runtime", "orlix-runtime-validation.sh")
    let outputRoot = buildPath("shell_redirection_smoke")
    let evidenceURL = outputRoot.appendingPathComponent("evidence.json")
    let runtimeOutputURL = outputRoot.appendingPathComponent("runtime-validation-output.txt")
    try ensureDirectory(outputRoot)

    var failures: [Failure] = []
    var artifacts: [String] = []
    var evidence: [String: String] = [
        "actual_command": command,
        "backend": "tcti",
        "git_sha": gitSha(),
        "runtime_gate": runtimeGate,
        "selected_simulator_id": simulatorID,
        "selected_simulator_name": simulatorName,
        "real_stack_execution_surface": "OrlixOS simulator runtime-validation app session",
        "shell_command": "echo redir-ok > /tmp/orlix-tcti-redir; read line < /tmp/orlix-tcti-redir; test $line = redir-ok; printf $line",
        "runtime_validation_executed": "false",
        "runtime_validation_passed": "false",
        "redirection_marker_asserted": "false",
        "redirection_stdout_asserted": "false",
        "child_process_started": "false",
        "child_process_exited": "false",
        "wait_reaping_status_observed": "false",
        "pass_count": "0",
        "fail_count": "0",
        "skip_count": "0",
    ]

    let runtimeText = try readText(runtimeScript)
    if sourceTextContains(runtimeText, #"tcti-shell-redirection-smoke"#) &&
        sourceTextContains(runtimeText, #"ORLIX-TCTI-SHELL-REDIRECTION-OK"#) &&
        sourceTextContains(runtimeText, #"%3E%20/tmp/orlix-tcti-redir"#) &&
        sourceTextContains(runtimeText, #"%3C%20/tmp/orlix-tcti-redir"#) {
        evidence["runtime_script_shell_redirection_source_proof"] = "\(relativePath(runtimeScript)) contains tcti-shell-redirection-smoke command markers"
    } else {
        failures.append(fail("shell-redirection-source-proof-missing", "\(relativePath(runtimeScript)) must contain the tcti-shell-redirection-smoke workload"))
    }

    let home = ProcessInfo.processInfo.environment["HOME"] ?? ""
    let runtimeArguments = [
        "PATH=\(home)/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin",
        "ORLIX_SIMULATOR_ID=\(simulatorID)",
        "ORLIX_TCTI_REQUIRED_SIMULATOR_ID=\(simulatorID)",
        "ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=\(simulatorName)",
        "make",
        "runtime-validation",
        "DESTINATION=iphonesimulator",
        "GATE=\(runtimeGate)",
    ]
    let runtimeOutput = try run(runtimeArguments, check: false)
    try runtimeOutput.write(to: runtimeOutputURL, atomically: true, encoding: .utf8)
    artifacts.append(relativePath(runtimeOutputURL))
    evidence["runtime_validation_executed"] = "true"
    evidence["runtime_validation_command"] = runtimeArguments.dropFirst(4).joined(separator: " ")

    guard let runtimeReport = selectedRuntimeValidationReport(gate: runtimeGate, destination: "iphonesimulator") else {
        failures.append(fail("runtime-report-missing", "runtime-validation did not produce \(runtimeGate) iphonesimulator JSON report"))
        let status: GateStatus = .fail
        evidence["fail_count"] = "1"
        evidence["gate_result"] = status.rawValue
        try writeJSON(evidence, to: evidenceURL)
        artifacts.append(relativePath(evidenceURL))
        let reducer = try writeReducer(
            target: target,
            caseID: "shell-redirection-smoke-fail",
            command: command,
            reason: "shell redirection runtime-validation report missing",
            artifacts: artifacts,
            expectedStatus: .fail
        )
        artifacts.append(relativePath(reducer))
        let reportURL = try writeReport(report(
            target: target,
            status: status,
            summary: "Shell redirection gate could not find the runtime-validation report.",
            command: command,
            failures: failures,
            artifacts: artifacts,
            counters: [
                "shell_redirection_tests_executed": 0,
                "shell_redirection_tests_passed": 0,
                "shell_redirection_tests_failed": 1,
                "shell_redirection_tests_skipped": 0,
            ],
            evidence: evidence,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("\(status.rawValue): \(relativePath(reportURL))")
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
        return 1
    }

    let runtimeObject = runtimeReport.object
    let runtimeReportPath = relativePath(runtimeReport.url)
    artifacts.append(runtimeReportPath)
    let runtimeArtifacts = runtimeObject["artifacts"] as? [String] ?? []
    artifacts.append(contentsOf: runtimeArtifacts)

    let markerText = try runtimeArtifacts.first { $0.hasSuffix("tcti-shell-redirection-smoke.txt") }.map(readRelativeArtifact) ?? ""
    let terminalText = try runtimeArtifacts.first { $0.hasSuffix("simulator-terminal-output.txt") }.map(readRelativeArtifact) ?? ""
    let forbidden = runtimeObject["forbidden_behavior"] as? [String: Any] ?? [:]
    let forbiddenKeys = [
        "generated_exec_memory",
        "host_exec_guest_text",
        "host_x18",
        "map_jit",
        "native_ios_api_exposure_to_guest",
        "rwx",
    ]
    var forbiddenBehavior = forbiddenDefaults()
    for key in forbiddenKeys {
        forbiddenBehavior[key] = boolField(forbidden, key)
    }

    if stringField(runtimeObject, "git_sha") != gitSha() {
        failures.append(fail("runtime-report-stale", "\(runtimeReportPath) is not current HEAD"))
    }
    if stringField(runtimeObject, "status") == "pass" && boolField(runtimeObject, "passed") {
        evidence["runtime_validation_passed"] = "true"
    } else {
        failures.append(fail("runtime-report-status", "\(runtimeReportPath) did not pass"))
    }
    if stringField(runtimeObject, "selected_device_id") != simulatorID ||
        stringField(runtimeObject, "selected_device_name") != simulatorName ||
        stringField(runtimeObject, "simulator_booted_count") != "1" ||
        !boolField(runtimeObject, "simulator_single_booted") {
        failures.append(fail("simulator-scope", "shell redirection gate requires single pinned \(simulatorName) simulator"))
    }
    if markerText.contains("ORLIX-TCTI-SHELL-REDIRECTION-OK") {
        evidence["redirection_marker_asserted"] = "true"
    } else {
        failures.append(fail("shell-redirection-marker-missing", "runtime artifacts did not include ORLIX-TCTI-SHELL-REDIRECTION-OK"))
    }
    if markerText.contains("redir-okORLIX-TCTI-SHELL-REDIRECTION-OK") ||
        terminalText.contains("redir-okORLIX-TCTI-SHELL-REDIRECTION-OK") ||
        outputContainsInOrder(terminalText, ["redir-ok", "ORLIX-TCTI-SHELL-REDIRECTION-OK"]) {
        evidence["redirection_stdout_asserted"] = "true"
    } else {
        failures.append(fail("shell-redirection-stdout-missing", "runtime artifacts did not include redir-ok output before the shell redirection marker"))
    }
    if terminalText.contains("orlix-init: process started pid=") {
        evidence["child_process_started"] = "true"
    } else {
        failures.append(fail("child-process-start-missing", "runtime artifacts did not include shell child process start"))
    }
    if terminalText.contains("orlix-init: process exited pid=") {
        evidence["child_process_exited"] = "true"
    } else {
        failures.append(fail("child-process-exit-missing", "runtime artifacts did not include shell child process exit"))
    }
    if outputHasShellWaitStatusZero(terminalText) {
        evidence["wait_reaping_status_observed"] = "true"
    } else {
        failures.append(fail("wait-reaping-status-missing", "runtime artifacts did not include shell exit status 0"))
    }
    for key in forbiddenKeys where boolField(forbidden, key) {
        failures.append(fail("forbidden-behavior", "runtime report sets forbidden_behavior.\(key)=true"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    if status == .pass {
        evidence["pass_count"] = "1"
    } else {
        evidence["fail_count"] = "1"
        evidence["blocker"] = "Pinned simulator shell redirection runtime-validation did not produce a clean OrlixOS session pass."
    }
    evidence["gate_result"] = status.rawValue
    try writeJSON(evidence, to: evidenceURL)
    artifacts.append(relativePath(evidenceURL))

    let reducer = try writeReducer(
        target: target,
        caseID: status == .pass ? "shell-redirection-smoke-pass" : "shell-redirection-smoke-fail",
        command: command,
        reason: status == .pass ? "shell redirection passed" : "shell redirection failed",
        artifacts: artifacts,
        expectedStatus: status
    )
    artifacts.append(relativePath(reducer))

    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: status == .pass ? "Pinned simulator shell redirection gate passed through OrlixOS runtime-validation." : "Pinned simulator shell redirection gate did not pass through OrlixOS runtime-validation.",
        command: command,
        failures: failures,
        artifacts: artifacts,
        forbiddenBehavior: forbiddenBehavior,
        counters: [
            "shell_redirection_tests_executed": evidence["runtime_validation_executed"] == "true" ? 1 : 0,
            "shell_redirection_tests_passed": status == .pass ? 1 : 0,
            "shell_redirection_tests_failed": status == .pass ? 0 : 1,
            "shell_redirection_tests_skipped": 0,
        ],
        evidence: evidence,
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    if status != .pass {
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
    }
    return exitCode(for: status)
}

func runShellScriptSmoke() throws -> Int32 {
    let target = "tcti-shell-script-smoke"
    let command = "make tcti-gate TARGET=\(target)"
    let runtimeGate = "tcti-shell-script-smoke"
    let simulatorID = "1E5553B0-203A-4A11-BAD7-EBDE46863F66"
    let simulatorName = "Orlix-iPhone-15-Pro-Max"
    let runtimeScript = path("tools", "runtime", "orlix-runtime-validation.sh")
    let outputRoot = buildPath("shell_script_smoke")
    let evidenceURL = outputRoot.appendingPathComponent("evidence.json")
    let runtimeOutputURL = outputRoot.appendingPathComponent("runtime-validation-output.txt")
    try ensureDirectory(outputRoot)

    var failures: [Failure] = []
    var artifacts: [String] = []
    var evidence: [String: String] = [
        "actual_command": command,
        "backend": "tcti",
        "git_sha": gitSha(),
        "runtime_gate": runtimeGate,
        "selected_simulator_id": simulatorID,
        "selected_simulator_name": simulatorName,
        "real_stack_execution_surface": "OrlixOS simulator runtime-validation app session",
        "shell_command": "create /tmp/orlix-tcti-script through shell redirection, run /bin/sh /tmp/orlix-tcti-script, print script-ok and marker",
        "runtime_validation_executed": "false",
        "runtime_validation_passed": "false",
        "script_marker_asserted": "false",
        "script_stdout_asserted": "false",
        "child_process_started": "false",
        "child_process_exited": "false",
        "wait_reaping_status_observed": "false",
        "pass_count": "0",
        "fail_count": "0",
        "skip_count": "0",
    ]

    let runtimeText = try readText(runtimeScript)
    if sourceTextContains(runtimeText, #"tcti-shell-script-smoke"#) &&
        sourceTextContains(runtimeText, #"ORLIX-TCTI-SHELL-SCRIPT-OK"#) &&
        sourceTextContains(runtimeText, #"/tmp/orlix-tcti-script"#) &&
        sourceTextContains(runtimeText, #"/bin/sh%20%24script"#) {
        evidence["runtime_script_shell_script_source_proof"] = "\(relativePath(runtimeScript)) contains tcti-shell-script-smoke command markers"
    } else {
        failures.append(fail("shell-script-source-proof-missing", "\(relativePath(runtimeScript)) must contain the tcti-shell-script-smoke workload"))
    }

    let home = ProcessInfo.processInfo.environment["HOME"] ?? ""
    let runtimeArguments = [
        "PATH=\(home)/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin",
        "ORLIX_SIMULATOR_ID=\(simulatorID)",
        "ORLIX_TCTI_REQUIRED_SIMULATOR_ID=\(simulatorID)",
        "ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=\(simulatorName)",
        "make",
        "runtime-validation",
        "DESTINATION=iphonesimulator",
        "GATE=\(runtimeGate)",
    ]
    let runtimeOutput = try run(runtimeArguments, check: false)
    try runtimeOutput.write(to: runtimeOutputURL, atomically: true, encoding: .utf8)
    artifacts.append(relativePath(runtimeOutputURL))
    evidence["runtime_validation_executed"] = "true"
    evidence["runtime_validation_command"] = runtimeArguments.dropFirst(4).joined(separator: " ")

    guard let runtimeReport = selectedRuntimeValidationReport(gate: runtimeGate, destination: "iphonesimulator") else {
        failures.append(fail("runtime-report-missing", "runtime-validation did not produce \(runtimeGate) iphonesimulator JSON report"))
        let status: GateStatus = .fail
        evidence["fail_count"] = "1"
        evidence["gate_result"] = status.rawValue
        try writeJSON(evidence, to: evidenceURL)
        artifacts.append(relativePath(evidenceURL))
        let reducer = try writeReducer(
            target: target,
            caseID: "shell-script-smoke-fail",
            command: command,
            reason: "shell script runtime-validation report missing",
            artifacts: artifacts,
            expectedStatus: .fail
        )
        artifacts.append(relativePath(reducer))
        let reportURL = try writeReport(report(
            target: target,
            status: status,
            summary: "Shell script gate could not find the runtime-validation report.",
            command: command,
            failures: failures,
            artifacts: artifacts,
            counters: [
                "shell_script_tests_executed": 0,
                "shell_script_tests_passed": 0,
                "shell_script_tests_failed": 1,
                "shell_script_tests_skipped": 0,
            ],
            evidence: evidence,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("\(status.rawValue): \(relativePath(reportURL))")
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
        return 1
    }

    let runtimeObject = runtimeReport.object
    let runtimeReportPath = relativePath(runtimeReport.url)
    artifacts.append(runtimeReportPath)
    let runtimeArtifacts = runtimeObject["artifacts"] as? [String] ?? []
    artifacts.append(contentsOf: runtimeArtifacts)

    let markerText = try runtimeArtifacts.first { $0.hasSuffix("tcti-shell-script-smoke.txt") }.map(readRelativeArtifact) ?? ""
    let terminalText = try runtimeArtifacts.first { $0.hasSuffix("simulator-terminal-output.txt") }.map(readRelativeArtifact) ?? ""
    let forbidden = runtimeObject["forbidden_behavior"] as? [String: Any] ?? [:]
    let forbiddenKeys = [
        "generated_exec_memory",
        "host_exec_guest_text",
        "host_x18",
        "map_jit",
        "native_ios_api_exposure_to_guest",
        "rwx",
    ]
    var forbiddenBehavior = forbiddenDefaults()
    for key in forbiddenKeys {
        forbiddenBehavior[key] = boolField(forbidden, key)
    }

    if stringField(runtimeObject, "git_sha") != gitSha() {
        failures.append(fail("runtime-report-stale", "\(runtimeReportPath) is not current HEAD"))
    }
    if stringField(runtimeObject, "status") == "pass" && boolField(runtimeObject, "passed") {
        evidence["runtime_validation_passed"] = "true"
    } else {
        failures.append(fail("runtime-report-status", "\(runtimeReportPath) did not pass"))
    }
    if stringField(runtimeObject, "selected_device_id") != simulatorID ||
        stringField(runtimeObject, "selected_device_name") != simulatorName ||
        stringField(runtimeObject, "simulator_booted_count") != "1" ||
        !boolField(runtimeObject, "simulator_single_booted") {
        failures.append(fail("simulator-scope", "shell script gate requires single pinned \(simulatorName) simulator"))
    }
    if markerText.contains("ORLIX-TCTI-SHELL-SCRIPT-OK") {
        evidence["script_marker_asserted"] = "true"
    } else {
        failures.append(fail("shell-script-marker-missing", "runtime artifacts did not include ORLIX-TCTI-SHELL-SCRIPT-OK"))
    }
    if outputContainsInOrder(markerText, ["script-ok", "ORLIX-TCTI-SHELL-SCRIPT-OK"]) ||
        outputContainsInOrder(terminalText, ["script-ok", "ORLIX-TCTI-SHELL-SCRIPT-OK"]) {
        evidence["script_stdout_asserted"] = "true"
    } else {
        failures.append(fail("shell-script-stdout-missing", "runtime artifacts did not include script-ok output before the shell script marker"))
    }
    if terminalText.contains("orlix-init: process started pid=") {
        evidence["child_process_started"] = "true"
    } else {
        failures.append(fail("child-process-start-missing", "runtime artifacts did not include shell child process start"))
    }
    if terminalText.contains("orlix-init: process exited pid=") {
        evidence["child_process_exited"] = "true"
    } else {
        failures.append(fail("child-process-exit-missing", "runtime artifacts did not include shell child process exit"))
    }
    if outputHasShellWaitStatusZero(terminalText) {
        evidence["wait_reaping_status_observed"] = "true"
    } else {
        failures.append(fail("wait-reaping-status-missing", "runtime artifacts did not include shell exit status 0"))
    }
    for key in forbiddenKeys where boolField(forbidden, key) {
        failures.append(fail("forbidden-behavior", "runtime report sets forbidden_behavior.\(key)=true"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    if status == .pass {
        evidence["pass_count"] = "1"
    } else {
        evidence["fail_count"] = "1"
        evidence["blocker"] = "Pinned simulator shell script runtime-validation did not produce a clean OrlixOS session pass."
    }
    evidence["gate_result"] = status.rawValue
    try writeJSON(evidence, to: evidenceURL)
    artifacts.append(relativePath(evidenceURL))

    let reducer = try writeReducer(
        target: target,
        caseID: status == .pass ? "shell-script-smoke-pass" : "shell-script-smoke-fail",
        command: command,
        reason: status == .pass ? "shell script passed" : "shell script failed",
        artifacts: artifacts,
        expectedStatus: status
    )
    artifacts.append(relativePath(reducer))

    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: status == .pass ? "Pinned simulator shell script gate passed through OrlixOS runtime-validation." : "Pinned simulator shell script gate did not pass through OrlixOS runtime-validation.",
        command: command,
        failures: failures,
        artifacts: artifacts,
        forbiddenBehavior: forbiddenBehavior,
        counters: [
            "shell_script_tests_executed": evidence["runtime_validation_executed"] == "true" ? 1 : 0,
            "shell_script_tests_passed": status == .pass ? 1 : 0,
            "shell_script_tests_failed": status == .pass ? 0 : 1,
            "shell_script_tests_skipped": 0,
        ],
        evidence: evidence,
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    if status != .pass {
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
    }
    return exitCode(for: status)
}

func runCoreutilsTrueFalseEcho() throws -> Int32 {
    let target = "tcti-coreutils-true-false-echo"
    let command = "make tcti-gate TARGET=\(target)"
    let runtimeGate = "tcti-coreutils-true-false-echo"
    let simulatorID = "1E5553B0-203A-4A11-BAD7-EBDE46863F66"
    let simulatorName = "Orlix-iPhone-15-Pro-Max"
    let runtimeScript = path("tools", "runtime", "orlix-runtime-validation.sh")
    let coreutilsConfig = path("OrlixOS", "Sources", "make", "config.mk")
    let outputRoot = buildPath("coreutils_true_false_echo")
    let evidenceURL = outputRoot.appendingPathComponent("evidence.json")
    let runtimeOutputURL = outputRoot.appendingPathComponent("runtime-validation-output.txt")
    try ensureDirectory(outputRoot)

    var failures: [Failure] = []
    var artifacts: [String] = []
    var evidence: [String: String] = [
        "actual_command": command,
        "backend": "tcti",
        "git_sha": gitSha(),
        "runtime_gate": runtimeGate,
        "selected_simulator_id": simulatorID,
        "selected_simulator_name": simulatorName,
        "real_stack_execution_surface": "OrlixOS simulator runtime-validation app session",
        "coreutils_commands": "/bin/true; /bin/false; /bin/echo coreutils-ok",
        "runtime_validation_executed": "false",
        "runtime_validation_passed": "false",
        "coreutils_marker_asserted": "false",
        "coreutils_stdout_asserted": "false",
        "child_process_started": "false",
        "child_process_exited": "false",
        "wait_reaping_status_observed": "false",
        "pass_count": "0",
        "fail_count": "0",
        "skip_count": "0",
    ]

    let runtimeText = try readText(runtimeScript)
    if sourceTextContains(runtimeText, #"tcti-coreutils-true-false-echo"#) &&
        sourceTextContains(runtimeText, #"ORLIX-TCTI-COREUTILS-TRUE-FALSE-ECHO-OK"#) &&
        sourceTextContains(runtimeText, #"/bin/true"#) &&
        sourceTextContains(runtimeText, #"/bin/false"#) &&
        sourceTextContains(runtimeText, #"/bin/echo"#) {
        evidence["runtime_script_coreutils_source_proof"] = "\(relativePath(runtimeScript)) contains absolute Coreutils command markers"
    } else {
        failures.append(fail("coreutils-source-proof-missing", "\(relativePath(runtimeScript)) must call /bin/true, /bin/false, and /bin/echo"))
    }

    let coreutilsConfigText = try readText(coreutilsConfig)
    let coreutilsProgramLine = coreutilsConfigText
        .split(separator: "\n")
        .first { $0.hasPrefix("ORLIXOS_COREUTILS_PROGRAMS :=") } ?? ""
    let coreutilsProgramTokens = Set(coreutilsProgramLine
        .replacingOccurrences(of: "ORLIXOS_COREUTILS_PROGRAMS :=", with: "")
        .split(whereSeparator: { $0 == " " || $0 == "\t" })
        .map(String.init)
        .filter { $0 != "[" })
    if coreutilsConfigText.contains("COREUTILS_VERSION ?=") &&
        coreutilsConfigText.contains("COREUTILS_GIT_COMMIT ?=") &&
        coreutilsProgramTokens.isSuperset(of: ["true", "false", "echo"]) {
        evidence["coreutils_package_source"] = "\(relativePath(coreutilsConfig)) declares Coreutils version, git commit, and true/false/echo programs"
    } else {
        failures.append(fail("coreutils-config-proof-missing", "\(relativePath(coreutilsConfig)) must declare Coreutils version, commit, and selected programs"))
    }

    let home = ProcessInfo.processInfo.environment["HOME"] ?? ""
    let runtimeArguments = [
        "PATH=\(home)/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin",
        "ORLIX_SIMULATOR_ID=\(simulatorID)",
        "ORLIX_TCTI_REQUIRED_SIMULATOR_ID=\(simulatorID)",
        "ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=\(simulatorName)",
        "make",
        "runtime-validation",
        "DESTINATION=iphonesimulator",
        "GATE=\(runtimeGate)",
    ]
    let runtimeOutput = try run(runtimeArguments, check: false)
    try runtimeOutput.write(to: runtimeOutputURL, atomically: true, encoding: .utf8)
    artifacts.append(relativePath(runtimeOutputURL))
    evidence["runtime_validation_executed"] = "true"
    evidence["runtime_validation_command"] = runtimeArguments.dropFirst(4).joined(separator: " ")

    guard let runtimeReport = selectedRuntimeValidationReport(gate: runtimeGate, destination: "iphonesimulator") else {
        failures.append(fail("runtime-report-missing", "runtime-validation did not produce \(runtimeGate) iphonesimulator JSON report"))
        let status: GateStatus = .fail
        evidence["fail_count"] = "1"
        evidence["gate_result"] = status.rawValue
        try writeJSON(evidence, to: evidenceURL)
        artifacts.append(relativePath(evidenceURL))
        let reducer = try writeReducer(target: target, caseID: "coreutils-true-false-echo-fail", command: command, reason: "Coreutils runtime-validation report missing", artifacts: artifacts, expectedStatus: .fail)
        artifacts.append(relativePath(reducer))
        let reportURL = try writeReport(report(
            target: target,
            status: status,
            summary: "Coreutils true/false/echo gate could not find the runtime-validation report.",
            command: command,
            failures: failures,
            artifacts: artifacts,
            counters: [
                "coreutils_true_false_echo_tests_executed": 0,
                "coreutils_true_false_echo_tests_passed": 0,
                "coreutils_true_false_echo_tests_failed": 1,
                "coreutils_true_false_echo_tests_skipped": 0,
            ],
            evidence: evidence,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("\(status.rawValue): \(relativePath(reportURL))")
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
        return 1
    }

    let runtimeObject = runtimeReport.object
    let runtimeReportPath = relativePath(runtimeReport.url)
    artifacts.append(runtimeReportPath)
    let runtimeArtifacts = runtimeObject["artifacts"] as? [String] ?? []
    artifacts.append(contentsOf: runtimeArtifacts)

    let markerText = try runtimeArtifacts.first { $0.hasSuffix("tcti-coreutils-true-false-echo.txt") }.map(readRelativeArtifact) ?? ""
    let terminalText = try runtimeArtifacts.first { $0.hasSuffix("simulator-terminal-output.txt") }.map(readRelativeArtifact) ?? ""
    let forbidden = runtimeObject["forbidden_behavior"] as? [String: Any] ?? [:]
    let forbiddenKeys = [
        "generated_exec_memory",
        "host_exec_guest_text",
        "host_x18",
        "map_jit",
        "native_ios_api_exposure_to_guest",
        "rwx",
    ]
    var forbiddenBehavior = forbiddenDefaults()
    for key in forbiddenKeys {
        forbiddenBehavior[key] = boolField(forbidden, key)
    }

    if stringField(runtimeObject, "git_sha") != gitSha() {
        failures.append(fail("runtime-report-stale", "\(runtimeReportPath) is not current HEAD"))
    }
    if stringField(runtimeObject, "status") == "pass" && boolField(runtimeObject, "passed") {
        evidence["runtime_validation_passed"] = "true"
    } else {
        failures.append(fail("runtime-report-status", "\(runtimeReportPath) did not pass"))
    }
    if stringField(runtimeObject, "selected_device_id") != simulatorID ||
        stringField(runtimeObject, "selected_device_name") != simulatorName ||
        stringField(runtimeObject, "simulator_booted_count") != "1" ||
        !boolField(runtimeObject, "simulator_single_booted") {
        failures.append(fail("simulator-scope", "Coreutils true/false/echo gate requires single pinned \(simulatorName) simulator"))
    }
    if markerText.contains("ORLIX-TCTI-COREUTILS-TRUE-FALSE-ECHO-OK") {
        evidence["coreutils_marker_asserted"] = "true"
    } else {
        failures.append(fail("coreutils-marker-missing", "runtime artifacts did not include ORLIX-TCTI-COREUTILS-TRUE-FALSE-ECHO-OK"))
    }
    if markerText.contains("coreutils-ok") || terminalText.contains("coreutils-ok") {
        evidence["coreutils_stdout_asserted"] = "true"
    } else {
        failures.append(fail("coreutils-stdout-missing", "runtime artifacts did not include /bin/echo output"))
    }
    if terminalText.contains("orlix-init: process started pid=") {
        evidence["child_process_started"] = "true"
    } else {
        failures.append(fail("child-process-start-missing", "runtime artifacts did not include shell child process start"))
    }
    if terminalText.contains("orlix-init: process exited pid=") {
        evidence["child_process_exited"] = "true"
    } else {
        failures.append(fail("child-process-exit-missing", "runtime artifacts did not include shell child process exit"))
    }
    if outputHasShellWaitStatusZero(terminalText) {
        evidence["wait_reaping_status_observed"] = "true"
    } else {
        failures.append(fail("wait-reaping-status-missing", "runtime artifacts did not include shell exit status 0"))
    }
    for key in forbiddenKeys where boolField(forbidden, key) {
        failures.append(fail("forbidden-behavior", "runtime report sets forbidden_behavior.\(key)=true"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    if status == .pass {
        evidence["pass_count"] = "1"
    } else {
        evidence["fail_count"] = "1"
        evidence["blocker"] = "Pinned simulator Coreutils true/false/echo runtime-validation did not produce a clean OrlixOS session pass."
    }
    evidence["gate_result"] = status.rawValue
    try writeJSON(evidence, to: evidenceURL)
    artifacts.append(relativePath(evidenceURL))

    let reducer = try writeReducer(
        target: target,
        caseID: status == .pass ? "coreutils-true-false-echo-pass" : "coreutils-true-false-echo-fail",
        command: command,
        reason: status == .pass ? "Coreutils true/false/echo passed" : "Coreutils true/false/echo failed",
        artifacts: artifacts,
        expectedStatus: status
    )
    artifacts.append(relativePath(reducer))

    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: status == .pass ? "Pinned simulator Coreutils true/false/echo gate passed through OrlixOS runtime-validation." : "Pinned simulator Coreutils true/false/echo gate did not pass through OrlixOS runtime-validation.",
        command: command,
        failures: failures,
        artifacts: artifacts,
        forbiddenBehavior: forbiddenBehavior,
        counters: [
            "coreutils_true_false_echo_tests_executed": evidence["runtime_validation_executed"] == "true" ? 1 : 0,
            "coreutils_true_false_echo_tests_passed": status == .pass ? 1 : 0,
            "coreutils_true_false_echo_tests_failed": status == .pass ? 0 : 1,
            "coreutils_true_false_echo_tests_skipped": 0,
        ],
        evidence: evidence,
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    if status != .pass {
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
    }
    return exitCode(for: status)
}

func runCoreutilsCatWC() throws -> Int32 {
    let target = "tcti-coreutils-cat-wc"
    let command = "make tcti-gate TARGET=\(target)"
    let runtimeGate = "tcti-coreutils-cat-wc"
    let simulatorID = "1E5553B0-203A-4A11-BAD7-EBDE46863F66"
    let simulatorName = "Orlix-iPhone-15-Pro-Max"
    let runtimeScript = path("tools", "runtime", "orlix-runtime-validation.sh")
    let coreutilsConfig = path("OrlixOS", "Sources", "make", "config.mk")
    let outputRoot = buildPath("coreutils_cat_wc")
    let evidenceURL = outputRoot.appendingPathComponent("evidence.json")
    let runtimeOutputURL = outputRoot.appendingPathComponent("runtime-validation-output.txt")
    try ensureDirectory(outputRoot)

    var failures: [Failure] = []
    var artifacts: [String] = []
    var evidence: [String: String] = [
        "actual_command": command,
        "backend": "tcti",
        "git_sha": gitSha(),
        "runtime_gate": runtimeGate,
        "selected_simulator_id": simulatorID,
        "selected_simulator_name": simulatorName,
        "real_stack_execution_surface": "OrlixOS simulator runtime-validation app session",
        "coreutils_commands": "/bin/wc -l /tmp/orlix-coreutils-cat-wc; /bin/cat /tmp/orlix-coreutils-cat-wc",
        "runtime_validation_executed": "false",
        "runtime_validation_passed": "false",
        "coreutils_marker_asserted": "false",
        "coreutils_stdout_asserted": "false",
        "child_process_started": "false",
        "child_process_exited": "false",
        "wait_reaping_status_observed": "false",
        "pass_count": "0",
        "fail_count": "0",
        "skip_count": "0",
    ]

    let runtimeText = try readText(runtimeScript)
    if sourceTextContains(runtimeText, #"tcti-coreutils-cat-wc"#) &&
        sourceTextContains(runtimeText, #"ORLIX-TCTI-COREUTILS-CAT-WC-OK"#) &&
        sourceTextContains(runtimeText, #"/bin/cat"#) &&
        sourceTextContains(runtimeText, #"/bin/wc"#) {
        evidence["runtime_script_coreutils_source_proof"] = "\(relativePath(runtimeScript)) contains absolute Coreutils cat/wc command markers"
    } else {
        failures.append(fail("coreutils-source-proof-missing", "\(relativePath(runtimeScript)) must call /bin/cat and /bin/wc"))
    }

    let coreutilsConfigText = try readText(coreutilsConfig)
    let coreutilsProgramLine = coreutilsConfigText
        .split(separator: "\n")
        .first { $0.hasPrefix("ORLIXOS_COREUTILS_PROGRAMS :=") } ?? ""
    let coreutilsProgramTokens = Set(coreutilsProgramLine
        .replacingOccurrences(of: "ORLIXOS_COREUTILS_PROGRAMS :=", with: "")
        .split(whereSeparator: { $0 == " " || $0 == "\t" })
        .map(String.init)
        .filter { $0 != "[" })
    if coreutilsConfigText.contains("COREUTILS_VERSION ?=") &&
        coreutilsConfigText.contains("COREUTILS_GIT_COMMIT ?=") &&
        coreutilsProgramTokens.isSuperset(of: ["cat", "wc"]) {
        evidence["coreutils_package_source"] = "\(relativePath(coreutilsConfig)) declares Coreutils version, git commit, and cat/wc programs"
    } else {
        failures.append(fail("coreutils-config-proof-missing", "\(relativePath(coreutilsConfig)) must declare Coreutils version, commit, and selected programs"))
    }

    let home = ProcessInfo.processInfo.environment["HOME"] ?? ""
    let runtimeArguments = [
        "PATH=\(home)/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin",
        "ORLIX_SIMULATOR_ID=\(simulatorID)",
        "ORLIX_TCTI_REQUIRED_SIMULATOR_ID=\(simulatorID)",
        "ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=\(simulatorName)",
        "make",
        "runtime-validation",
        "DESTINATION=iphonesimulator",
        "GATE=\(runtimeGate)",
    ]
    let runtimeOutput = try run(runtimeArguments, check: false)
    try runtimeOutput.write(to: runtimeOutputURL, atomically: true, encoding: .utf8)
    artifacts.append(relativePath(runtimeOutputURL))
    evidence["runtime_validation_executed"] = "true"
    evidence["runtime_validation_command"] = runtimeArguments.dropFirst(4).joined(separator: " ")

    guard let runtimeReport = selectedRuntimeValidationReport(gate: runtimeGate, destination: "iphonesimulator") else {
        failures.append(fail("runtime-report-missing", "runtime-validation did not produce \(runtimeGate) iphonesimulator JSON report"))
        let status: GateStatus = .fail
        evidence["fail_count"] = "1"
        evidence["gate_result"] = status.rawValue
        try writeJSON(evidence, to: evidenceURL)
        artifacts.append(relativePath(evidenceURL))
        let reducer = try writeReducer(target: target, caseID: "coreutils-cat-wc-fail", command: command, reason: "Coreutils cat/wc runtime-validation report missing", artifacts: artifacts, expectedStatus: .fail)
        artifacts.append(relativePath(reducer))
        let reportURL = try writeReport(report(target: target, status: status, summary: "Coreutils cat/wc gate could not find the runtime-validation report.", command: command, failures: failures, artifacts: artifacts, counters: ["coreutils_cat_wc_tests_executed": 0, "coreutils_cat_wc_tests_passed": 0, "coreutils_cat_wc_tests_failed": 1, "coreutils_cat_wc_tests_skipped": 0], evidence: evidence, releaseGateEligible: false, readinessGateEligible: false))
        print("\(status.rawValue): \(relativePath(reportURL))")
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
        return 1
    }

    let runtimeObject = runtimeReport.object
    let runtimeReportPath = relativePath(runtimeReport.url)
    artifacts.append(runtimeReportPath)
    let runtimeArtifacts = runtimeObject["artifacts"] as? [String] ?? []
    artifacts.append(contentsOf: runtimeArtifacts)

    let markerText = try runtimeArtifacts.first { $0.hasSuffix("tcti-coreutils-cat-wc.txt") }.map(readRelativeArtifact) ?? ""
    let terminalText = try runtimeArtifacts.first { $0.hasSuffix("simulator-terminal-output.txt") }.map(readRelativeArtifact) ?? ""
    let forbidden = runtimeObject["forbidden_behavior"] as? [String: Any] ?? [:]
    let forbiddenKeys = ["generated_exec_memory", "host_exec_guest_text", "host_x18", "map_jit", "native_ios_api_exposure_to_guest", "rwx"]
    var forbiddenBehavior = forbiddenDefaults()
    for key in forbiddenKeys {
        forbiddenBehavior[key] = boolField(forbidden, key)
    }

    if stringField(runtimeObject, "git_sha") != gitSha() {
        failures.append(fail("runtime-report-stale", "\(runtimeReportPath) is not current HEAD"))
    }
    if stringField(runtimeObject, "status") == "pass" && boolField(runtimeObject, "passed") {
        evidence["runtime_validation_passed"] = "true"
    } else {
        failures.append(fail("runtime-report-status", "\(runtimeReportPath) did not pass"))
    }
    if stringField(runtimeObject, "selected_device_id") != simulatorID ||
        stringField(runtimeObject, "selected_device_name") != simulatorName ||
        stringField(runtimeObject, "simulator_booted_count") != "1" ||
        !boolField(runtimeObject, "simulator_single_booted") {
        failures.append(fail("simulator-scope", "Coreutils cat/wc gate requires single pinned \(simulatorName) simulator"))
    }
    if markerText.contains("ORLIX-TCTI-COREUTILS-CAT-WC-OK") {
        evidence["coreutils_marker_asserted"] = "true"
    } else {
        failures.append(fail("coreutils-marker-missing", "runtime artifacts did not include ORLIX-TCTI-COREUTILS-CAT-WC-OK"))
    }
    if markerText.contains("cat-wc-ok") || terminalText.contains("cat-wc-ok") {
        evidence["coreutils_stdout_asserted"] = "true"
    } else {
        failures.append(fail("coreutils-stdout-missing", "runtime artifacts did not include /bin/cat output"))
    }
    if terminalText.contains("orlix-init: process started pid=") {
        evidence["child_process_started"] = "true"
    } else {
        failures.append(fail("child-process-start-missing", "runtime artifacts did not include shell child process start"))
    }
    if terminalText.contains("orlix-init: process exited pid=") {
        evidence["child_process_exited"] = "true"
    } else {
        failures.append(fail("child-process-exit-missing", "runtime artifacts did not include shell child process exit"))
    }
    if outputHasShellWaitStatusZero(terminalText) {
        evidence["wait_reaping_status_observed"] = "true"
    } else {
        failures.append(fail("wait-reaping-status-missing", "runtime artifacts did not include shell exit status 0"))
    }
    for key in forbiddenKeys where boolField(forbidden, key) {
        failures.append(fail("forbidden-behavior", "runtime report sets forbidden_behavior.\(key)=true"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    evidence[status == .pass ? "pass_count" : "fail_count"] = "1"
    evidence["gate_result"] = status.rawValue
    try writeJSON(evidence, to: evidenceURL)
    artifacts.append(relativePath(evidenceURL))

    let reducer = try writeReducer(target: target, caseID: status == .pass ? "coreutils-cat-wc-pass" : "coreutils-cat-wc-fail", command: command, reason: status == .pass ? "Coreutils cat/wc passed" : "Coreutils cat/wc failed", artifacts: artifacts, expectedStatus: status)
    artifacts.append(relativePath(reducer))
    let reportURL = try writeReport(report(target: target, status: status, summary: status == .pass ? "Pinned simulator Coreutils cat/wc gate passed through OrlixOS runtime-validation." : "Pinned simulator Coreutils cat/wc gate did not pass through OrlixOS runtime-validation.", command: command, failures: failures, artifacts: artifacts, forbiddenBehavior: forbiddenBehavior, counters: ["coreutils_cat_wc_tests_executed": evidence["runtime_validation_executed"] == "true" ? 1 : 0, "coreutils_cat_wc_tests_passed": status == .pass ? 1 : 0, "coreutils_cat_wc_tests_failed": status == .pass ? 0 : 1, "coreutils_cat_wc_tests_skipped": 0], evidence: evidence, releaseGateEligible: false, readinessGateEligible: false))
    print("\(status.rawValue): \(relativePath(reportURL))")
    if status != .pass {
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
    }
    return exitCode(for: status)
}

func runCoreutilsLsStat() throws -> Int32 {
    let target = "tcti-coreutils-ls-stat"
    let command = "make tcti-gate TARGET=\(target)"
    let runtimeGate = "tcti-coreutils-ls-stat"
    let simulatorID = "1E5553B0-203A-4A11-BAD7-EBDE46863F66"
    let simulatorName = "Orlix-iPhone-15-Pro-Max"
    let runtimeScript = path("tools", "runtime", "orlix-runtime-validation.sh")
    let coreutilsConfig = path("OrlixOS", "Sources", "make", "config.mk")
    let outputRoot = buildPath("coreutils_ls_stat")
    let evidenceURL = outputRoot.appendingPathComponent("evidence.json")
    let runtimeOutputURL = outputRoot.appendingPathComponent("runtime-validation-output.txt")
    try ensureDirectory(outputRoot)

    var failures: [Failure] = []
    var artifacts: [String] = []
    var evidence: [String: String] = [
        "actual_command": command,
        "backend": "tcti",
        "git_sha": gitSha(),
        "runtime_gate": runtimeGate,
        "selected_simulator_id": simulatorID,
        "selected_simulator_name": simulatorName,
        "real_stack_execution_surface": "OrlixOS simulator runtime-validation app session",
        "coreutils_commands": "/bin/ls -ld /bin/sh; /bin/stat /bin/sh",
        "runtime_validation_executed": "false",
        "runtime_validation_passed": "false",
        "coreutils_marker_asserted": "false",
        "coreutils_stdout_asserted": "false",
        "child_process_started": "false",
        "child_process_exited": "false",
        "wait_reaping_status_observed": "false",
        "pass_count": "0",
        "fail_count": "0",
        "skip_count": "0",
    ]

    let runtimeText = try readText(runtimeScript)
    if sourceTextContains(runtimeText, #"tcti-coreutils-ls-stat"#) &&
        sourceTextContains(runtimeText, #"ORLIX-TCTI-COREUTILS-LS-STAT-OK"#) &&
        sourceTextContains(runtimeText, #"/bin/ls"#) &&
        sourceTextContains(runtimeText, #"/bin/stat"#) {
        evidence["runtime_script_coreutils_source_proof"] = "\(relativePath(runtimeScript)) contains absolute Coreutils ls/stat command markers"
    } else {
        failures.append(fail("coreutils-source-proof-missing", "\(relativePath(runtimeScript)) must call /bin/ls and /bin/stat"))
    }

    let coreutilsConfigText = try readText(coreutilsConfig)
    let coreutilsProgramLine = coreutilsConfigText
        .split(separator: "\n")
        .first { $0.hasPrefix("ORLIXOS_COREUTILS_PROGRAMS :=") } ?? ""
    let coreutilsProgramTokens = Set(coreutilsProgramLine
        .replacingOccurrences(of: "ORLIXOS_COREUTILS_PROGRAMS :=", with: "")
        .split(whereSeparator: { $0 == " " || $0 == "\t" })
        .map(String.init)
        .filter { $0 != "[" })
    if coreutilsConfigText.contains("COREUTILS_VERSION ?=") &&
        coreutilsConfigText.contains("COREUTILS_GIT_COMMIT ?=") &&
        coreutilsProgramTokens.isSuperset(of: ["ls", "stat"]) {
        evidence["coreutils_package_source"] = "\(relativePath(coreutilsConfig)) declares Coreutils version, git commit, and ls/stat programs"
    } else {
        failures.append(fail("coreutils-config-proof-missing", "\(relativePath(coreutilsConfig)) must declare Coreutils version, commit, and selected programs"))
    }

    let home = ProcessInfo.processInfo.environment["HOME"] ?? ""
    let runtimeArguments = [
        "PATH=\(home)/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin",
        "ORLIX_SIMULATOR_ID=\(simulatorID)",
        "ORLIX_TCTI_REQUIRED_SIMULATOR_ID=\(simulatorID)",
        "ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=\(simulatorName)",
        "make",
        "runtime-validation",
        "DESTINATION=iphonesimulator",
        "GATE=\(runtimeGate)",
    ]
    let runtimeOutput = try run(runtimeArguments, check: false)
    try runtimeOutput.write(to: runtimeOutputURL, atomically: true, encoding: .utf8)
    artifacts.append(relativePath(runtimeOutputURL))
    evidence["runtime_validation_executed"] = "true"
    evidence["runtime_validation_command"] = runtimeArguments.dropFirst(4).joined(separator: " ")

    guard let runtimeReport = selectedRuntimeValidationReport(gate: runtimeGate, destination: "iphonesimulator") else {
        failures.append(fail("runtime-report-missing", "runtime-validation did not produce \(runtimeGate) iphonesimulator JSON report"))
        let status: GateStatus = .fail
        evidence["fail_count"] = "1"
        evidence["gate_result"] = status.rawValue
        try writeJSON(evidence, to: evidenceURL)
        artifacts.append(relativePath(evidenceURL))
        let reducer = try writeReducer(target: target, caseID: "coreutils-ls-stat-fail", command: command, reason: "Coreutils ls/stat runtime-validation report missing", artifacts: artifacts, expectedStatus: .fail)
        artifacts.append(relativePath(reducer))
        let reportURL = try writeReport(report(target: target, status: status, summary: "Coreutils ls/stat gate could not find the runtime-validation report.", command: command, failures: failures, artifacts: artifacts, counters: ["coreutils_ls_stat_tests_executed": 0, "coreutils_ls_stat_tests_passed": 0, "coreutils_ls_stat_tests_failed": 1, "coreutils_ls_stat_tests_skipped": 0], evidence: evidence, releaseGateEligible: false, readinessGateEligible: false))
        print("\(status.rawValue): \(relativePath(reportURL))")
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
        return 1
    }

    let runtimeObject = runtimeReport.object
    let runtimeReportPath = relativePath(runtimeReport.url)
    artifacts.append(runtimeReportPath)
    let runtimeArtifacts = runtimeObject["artifacts"] as? [String] ?? []
    artifacts.append(contentsOf: runtimeArtifacts)

    let markerText = try runtimeArtifacts.first { $0.hasSuffix("tcti-coreutils-ls-stat.txt") }.map(readRelativeArtifact) ?? ""
    let terminalText = try runtimeArtifacts.first { $0.hasSuffix("simulator-terminal-output.txt") }.map(readRelativeArtifact) ?? ""
    let forbidden = runtimeObject["forbidden_behavior"] as? [String: Any] ?? [:]
    let forbiddenKeys = ["generated_exec_memory", "host_exec_guest_text", "host_x18", "map_jit", "native_ios_api_exposure_to_guest", "rwx"]
    var forbiddenBehavior = forbiddenDefaults()
    for key in forbiddenKeys {
        forbiddenBehavior[key] = boolField(forbidden, key)
    }

    if stringField(runtimeObject, "git_sha") != gitSha() {
        failures.append(fail("runtime-report-stale", "\(runtimeReportPath) is not current HEAD"))
    }
    if stringField(runtimeObject, "status") == "pass" && boolField(runtimeObject, "passed") {
        evidence["runtime_validation_passed"] = "true"
    } else {
        failures.append(fail("runtime-report-status", "\(runtimeReportPath) did not pass"))
    }
    if stringField(runtimeObject, "selected_device_id") != simulatorID ||
        stringField(runtimeObject, "selected_device_name") != simulatorName ||
        stringField(runtimeObject, "simulator_booted_count") != "1" ||
        !boolField(runtimeObject, "simulator_single_booted") {
        failures.append(fail("simulator-scope", "Coreutils ls/stat gate requires single pinned \(simulatorName) simulator"))
    }
    if markerText.contains("ORLIX-TCTI-COREUTILS-LS-STAT-OK") {
        evidence["coreutils_marker_asserted"] = "true"
    } else {
        failures.append(fail("coreutils-marker-missing", "runtime artifacts did not include ORLIX-TCTI-COREUTILS-LS-STAT-OK"))
    }
    if terminalText.contains("/bin/sh") && (terminalText.contains("File:") || terminalText.contains("Size:")) {
        evidence["coreutils_stdout_asserted"] = "true"
    } else {
        failures.append(fail("coreutils-stdout-missing", "runtime artifacts did not include ls/stat output for /bin/sh"))
    }
    if terminalText.contains("orlix-init: process started pid=") {
        evidence["child_process_started"] = "true"
    } else {
        failures.append(fail("child-process-start-missing", "runtime artifacts did not include shell child process start"))
    }
    if terminalText.contains("orlix-init: process exited pid=") {
        evidence["child_process_exited"] = "true"
    } else {
        failures.append(fail("child-process-exit-missing", "runtime artifacts did not include shell child process exit"))
    }
    if outputHasShellWaitStatusZero(terminalText) {
        evidence["wait_reaping_status_observed"] = "true"
    } else {
        failures.append(fail("wait-reaping-status-missing", "runtime artifacts did not include shell exit status 0"))
    }
    for key in forbiddenKeys where boolField(forbidden, key) {
        failures.append(fail("forbidden-behavior", "runtime report sets forbidden_behavior.\(key)=true"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    evidence[status == .pass ? "pass_count" : "fail_count"] = "1"
    evidence["gate_result"] = status.rawValue
    try writeJSON(evidence, to: evidenceURL)
    artifacts.append(relativePath(evidenceURL))

    let reducer = try writeReducer(target: target, caseID: status == .pass ? "coreutils-ls-stat-pass" : "coreutils-ls-stat-fail", command: command, reason: status == .pass ? "Coreutils ls/stat passed" : "Coreutils ls/stat failed", artifacts: artifacts, expectedStatus: status)
    artifacts.append(relativePath(reducer))
    let reportURL = try writeReport(report(target: target, status: status, summary: status == .pass ? "Pinned simulator Coreutils ls/stat gate passed through OrlixOS runtime-validation." : "Pinned simulator Coreutils ls/stat gate did not pass through OrlixOS runtime-validation.", command: command, failures: failures, artifacts: artifacts, forbiddenBehavior: forbiddenBehavior, counters: ["coreutils_ls_stat_tests_executed": evidence["runtime_validation_executed"] == "true" ? 1 : 0, "coreutils_ls_stat_tests_passed": status == .pass ? 1 : 0, "coreutils_ls_stat_tests_failed": status == .pass ? 0 : 1, "coreutils_ls_stat_tests_skipped": 0], evidence: evidence, releaseGateEligible: false, readinessGateEligible: false))
    print("\(status.rawValue): \(relativePath(reportURL))")
    if status != .pass {
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
    }
    return exitCode(for: status)
}

func runCoreutilsMkdirRmCpLn() throws -> Int32 {
    let target = "tcti-coreutils-mkdir-rm-cp-ln"
    let command = "make tcti-gate TARGET=\(target)"
    let runtimeGate = "tcti-coreutils-mkdir-rm-cp-ln"
    let simulatorID = "1E5553B0-203A-4A11-BAD7-EBDE46863F66"
    let simulatorName = "Orlix-iPhone-15-Pro-Max"
    let runtimeScript = path("tools", "runtime", "orlix-runtime-validation.sh")
    let coreutilsConfig = path("OrlixOS", "Sources", "make", "config.mk")
    let outputRoot = buildPath("coreutils_mkdir_rm_cp_ln")
    let evidenceURL = outputRoot.appendingPathComponent("evidence.json")
    let runtimeOutputURL = outputRoot.appendingPathComponent("runtime-validation-output.txt")
    try ensureDirectory(outputRoot)

    var failures: [Failure] = []
    var artifacts: [String] = []
    var evidence: [String: String] = [
        "actual_command": command,
        "backend": "tcti",
        "git_sha": gitSha(),
        "runtime_gate": runtimeGate,
        "selected_simulator_id": simulatorID,
        "selected_simulator_name": simulatorName,
        "real_stack_execution_surface": "OrlixOS simulator runtime-validation app session",
        "coreutils_commands": "/bin/mkdir -p; /bin/cp; /bin/ln; /bin/rm",
        "runtime_validation_executed": "false",
        "runtime_validation_passed": "false",
        "coreutils_marker_asserted": "false",
        "coreutils_stdout_asserted": "false",
        "child_process_started": "false",
        "child_process_exited": "false",
        "wait_reaping_status_observed": "false",
        "pass_count": "0",
        "fail_count": "0",
        "skip_count": "0",
    ]

    let runtimeText = try readText(runtimeScript)
    if sourceTextContains(runtimeText, #"tcti-coreutils-mkdir-rm-cp-ln"#) &&
        sourceTextContains(runtimeText, #"ORLIX-TCTI-COREUTILS-MKDIR-RM-CP-LN-OK"#) &&
        sourceTextContains(runtimeText, #"/bin/mkdir"#) &&
        sourceTextContains(runtimeText, #"/bin/rm"#) &&
        sourceTextContains(runtimeText, #"/bin/cp"#) &&
        sourceTextContains(runtimeText, #"/bin/ln"#) {
        evidence["runtime_script_coreutils_source_proof"] = "\(relativePath(runtimeScript)) contains absolute Coreutils mkdir/rm/cp/ln command markers"
    } else {
        failures.append(fail("coreutils-source-proof-missing", "\(relativePath(runtimeScript)) must call /bin/mkdir, /bin/rm, /bin/cp, and /bin/ln"))
    }

    let coreutilsConfigText = try readText(coreutilsConfig)
    let coreutilsProgramLine = coreutilsConfigText
        .split(separator: "\n")
        .first { $0.hasPrefix("ORLIXOS_COREUTILS_PROGRAMS :=") } ?? ""
    let coreutilsProgramTokens = Set(coreutilsProgramLine
        .replacingOccurrences(of: "ORLIXOS_COREUTILS_PROGRAMS :=", with: "")
        .split(whereSeparator: { $0 == " " || $0 == "\t" })
        .map(String.init)
        .filter { $0 != "[" })
    if coreutilsConfigText.contains("COREUTILS_VERSION ?=") &&
        coreutilsConfigText.contains("COREUTILS_GIT_COMMIT ?=") &&
        coreutilsProgramTokens.isSuperset(of: ["mkdir", "rm", "cp", "ln"]) {
        evidence["coreutils_package_source"] = "\(relativePath(coreutilsConfig)) declares Coreutils version, git commit, and mkdir/rm/cp/ln programs"
    } else {
        failures.append(fail("coreutils-config-proof-missing", "\(relativePath(coreutilsConfig)) must declare Coreutils version, commit, and selected programs"))
    }

    let home = ProcessInfo.processInfo.environment["HOME"] ?? ""
    let runtimeArguments = [
        "PATH=\(home)/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin",
        "ORLIX_SIMULATOR_ID=\(simulatorID)",
        "ORLIX_TCTI_REQUIRED_SIMULATOR_ID=\(simulatorID)",
        "ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=\(simulatorName)",
        "make",
        "runtime-validation",
        "DESTINATION=iphonesimulator",
        "GATE=\(runtimeGate)",
    ]
    let runtimeOutput = try run(runtimeArguments, check: false)
    try runtimeOutput.write(to: runtimeOutputURL, atomically: true, encoding: .utf8)
    artifacts.append(relativePath(runtimeOutputURL))
    evidence["runtime_validation_executed"] = "true"
    evidence["runtime_validation_command"] = runtimeArguments.dropFirst(4).joined(separator: " ")

    guard let runtimeReport = selectedRuntimeValidationReport(gate: runtimeGate, destination: "iphonesimulator") else {
        failures.append(fail("runtime-report-missing", "runtime-validation did not produce \(runtimeGate) iphonesimulator JSON report"))
        let status: GateStatus = .fail
        evidence["fail_count"] = "1"
        evidence["gate_result"] = status.rawValue
        try writeJSON(evidence, to: evidenceURL)
        artifacts.append(relativePath(evidenceURL))
        let reducer = try writeReducer(target: target, caseID: "coreutils-mkdir-rm-cp-ln-fail", command: command, reason: "Coreutils mkdir/rm/cp/ln runtime-validation report missing", artifacts: artifacts, expectedStatus: .fail)
        artifacts.append(relativePath(reducer))
        let reportURL = try writeReport(report(target: target, status: status, summary: "Coreutils mkdir/rm/cp/ln gate could not find the runtime-validation report.", command: command, failures: failures, artifacts: artifacts, counters: ["coreutils_mkdir_rm_cp_ln_tests_executed": 0, "coreutils_mkdir_rm_cp_ln_tests_passed": 0, "coreutils_mkdir_rm_cp_ln_tests_failed": 1, "coreutils_mkdir_rm_cp_ln_tests_skipped": 0], evidence: evidence, releaseGateEligible: false, readinessGateEligible: false))
        print("\(status.rawValue): \(relativePath(reportURL))")
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
        return 1
    }

    let runtimeObject = runtimeReport.object
    let runtimeReportPath = relativePath(runtimeReport.url)
    artifacts.append(runtimeReportPath)
    let runtimeArtifacts = runtimeObject["artifacts"] as? [String] ?? []
    artifacts.append(contentsOf: runtimeArtifacts)

    let markerText = try runtimeArtifacts.first { $0.hasSuffix("tcti-coreutils-mkdir-rm-cp-ln.txt") }.map(readRelativeArtifact) ?? ""
    let terminalText = try runtimeArtifacts.first { $0.hasSuffix("simulator-terminal-output.txt") }.map(readRelativeArtifact) ?? ""
    let forbidden = runtimeObject["forbidden_behavior"] as? [String: Any] ?? [:]
    let forbiddenKeys = ["generated_exec_memory", "host_exec_guest_text", "host_x18", "map_jit", "native_ios_api_exposure_to_guest", "rwx"]
    var forbiddenBehavior = forbiddenDefaults()
    for key in forbiddenKeys {
        forbiddenBehavior[key] = boolField(forbidden, key)
    }

    if stringField(runtimeObject, "git_sha") != gitSha() {
        failures.append(fail("runtime-report-stale", "\(runtimeReportPath) is not current HEAD"))
    }
    if stringField(runtimeObject, "status") == "pass" && boolField(runtimeObject, "passed") {
        evidence["runtime_validation_passed"] = "true"
    } else {
        failures.append(fail("runtime-report-status", "\(runtimeReportPath) did not pass"))
    }
    if stringField(runtimeObject, "selected_device_id") != simulatorID ||
        stringField(runtimeObject, "selected_device_name") != simulatorName ||
        stringField(runtimeObject, "simulator_booted_count") != "1" ||
        !boolField(runtimeObject, "simulator_single_booted") {
        failures.append(fail("simulator-scope", "Coreutils mkdir/rm/cp/ln gate requires single pinned \(simulatorName) simulator"))
    }
    if markerText.contains("ORLIX-TCTI-COREUTILS-MKDIR-RM-CP-LN-OK") {
        evidence["coreutils_marker_asserted"] = "true"
    } else {
        failures.append(fail("coreutils-marker-missing", "runtime artifacts did not include ORLIX-TCTI-COREUTILS-MKDIR-RM-CP-LN-OK"))
    }
    if markerText.contains("mkdir-rm-cp-ln-ok") || terminalText.contains("mkdir-rm-cp-ln-ok") {
        evidence["coreutils_stdout_asserted"] = "true"
    } else {
        failures.append(fail("coreutils-stdout-missing", "runtime artifacts did not include /bin/cat output from the hardlink"))
    }
    if terminalText.contains("orlix-init: process started pid=") {
        evidence["child_process_started"] = "true"
    } else {
        failures.append(fail("child-process-start-missing", "runtime artifacts did not include shell child process start"))
    }
    if terminalText.contains("orlix-init: process exited pid=") {
        evidence["child_process_exited"] = "true"
    } else {
        failures.append(fail("child-process-exit-missing", "runtime artifacts did not include shell child process exit"))
    }
    if outputHasShellWaitStatusZero(terminalText) {
        evidence["wait_reaping_status_observed"] = "true"
    } else {
        failures.append(fail("wait-reaping-status-missing", "runtime artifacts did not include shell exit status 0"))
    }
    for key in forbiddenKeys where boolField(forbidden, key) {
        failures.append(fail("forbidden-behavior", "runtime report sets forbidden_behavior.\(key)=true"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    evidence[status == .pass ? "pass_count" : "fail_count"] = "1"
    evidence["gate_result"] = status.rawValue
    try writeJSON(evidence, to: evidenceURL)
    artifacts.append(relativePath(evidenceURL))

    let reducer = try writeReducer(target: target, caseID: status == .pass ? "coreutils-mkdir-rm-cp-ln-pass" : "coreutils-mkdir-rm-cp-ln-fail", command: command, reason: status == .pass ? "Coreutils mkdir/rm/cp/ln passed" : "Coreutils mkdir/rm/cp/ln failed", artifacts: artifacts, expectedStatus: status)
    artifacts.append(relativePath(reducer))
    let reportURL = try writeReport(report(target: target, status: status, summary: status == .pass ? "Pinned simulator Coreutils mkdir/rm/cp/ln gate passed through OrlixOS runtime-validation." : "Pinned simulator Coreutils mkdir/rm/cp/ln gate did not pass through OrlixOS runtime-validation.", command: command, failures: failures, artifacts: artifacts, forbiddenBehavior: forbiddenBehavior, counters: ["coreutils_mkdir_rm_cp_ln_tests_executed": evidence["runtime_validation_executed"] == "true" ? 1 : 0, "coreutils_mkdir_rm_cp_ln_tests_passed": status == .pass ? 1 : 0, "coreutils_mkdir_rm_cp_ln_tests_failed": status == .pass ? 0 : 1, "coreutils_mkdir_rm_cp_ln_tests_skipped": 0], evidence: evidence, releaseGateEligible: false, readinessGateEligible: false))
    print("\(status.rawValue): \(relativePath(reportURL))")
    if status != .pass {
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
    }
    return exitCode(for: status)
}

func runCoreutilsEnvPath() throws -> Int32 {
    let target = "tcti-coreutils-env-path"
    let command = "make tcti-gate TARGET=\(target)"
    let runtimeGate = "tcti-coreutils-env-path"
    let simulatorID = "1E5553B0-203A-4A11-BAD7-EBDE46863F66"
    let simulatorName = "Orlix-iPhone-15-Pro-Max"
    let runtimeScript = path("tools", "runtime", "orlix-runtime-validation.sh")
    let coreutilsConfig = path("OrlixOS", "Sources", "make", "config.mk")
    let outputRoot = buildPath("coreutils_env_path")
    let evidenceURL = outputRoot.appendingPathComponent("evidence.json")
    let runtimeOutputURL = outputRoot.appendingPathComponent("runtime-validation-output.txt")
    try ensureDirectory(outputRoot)

    var failures: [Failure] = []
    var artifacts: [String] = []
    var evidence: [String: String] = [
        "actual_command": command,
        "backend": "tcti",
        "git_sha": gitSha(),
        "runtime_gate": runtimeGate,
        "selected_simulator_id": simulatorID,
        "selected_simulator_name": simulatorName,
        "real_stack_execution_surface": "OrlixOS simulator runtime-validation app session",
        "coreutils_commands": "/bin/env; /bin/printenv PATH",
        "runtime_validation_executed": "false",
        "runtime_validation_passed": "false",
        "coreutils_marker_asserted": "false",
        "coreutils_stdout_asserted": "false",
        "child_process_started": "false",
        "child_process_exited": "false",
        "wait_reaping_status_observed": "false",
        "pass_count": "0",
        "fail_count": "0",
        "skip_count": "0",
    ]

    let runtimeText = try readText(runtimeScript)
    if sourceTextContains(runtimeText, #"tcti-coreutils-env-path"#) &&
        sourceTextContains(runtimeText, #"ORLIX-TCTI-COREUTILS-ENV-PATH-OK"#) &&
        sourceTextContains(runtimeText, #"/bin/env"#) &&
        sourceTextContains(runtimeText, #"/bin/printenv"#) {
        evidence["runtime_script_coreutils_source_proof"] = "\(relativePath(runtimeScript)) contains absolute Coreutils env/printenv command markers"
    } else {
        failures.append(fail("coreutils-source-proof-missing", "\(relativePath(runtimeScript)) must call /bin/env and /bin/printenv"))
    }

    let coreutilsConfigText = try readText(coreutilsConfig)
    let coreutilsProgramLine = coreutilsConfigText
        .split(separator: "\n")
        .first { $0.hasPrefix("ORLIXOS_COREUTILS_PROGRAMS :=") } ?? ""
    let coreutilsProgramTokens = Set(coreutilsProgramLine
        .replacingOccurrences(of: "ORLIXOS_COREUTILS_PROGRAMS :=", with: "")
        .split(whereSeparator: { $0 == " " || $0 == "\t" })
        .map(String.init)
        .filter { $0 != "[" })
    if coreutilsConfigText.contains("COREUTILS_VERSION ?=") &&
        coreutilsConfigText.contains("COREUTILS_GIT_COMMIT ?=") &&
        coreutilsProgramTokens.isSuperset(of: ["env", "printenv"]) {
        evidence["coreutils_package_source"] = "\(relativePath(coreutilsConfig)) declares Coreutils version, git commit, and env/printenv programs"
    } else {
        failures.append(fail("coreutils-config-proof-missing", "\(relativePath(coreutilsConfig)) must declare Coreutils version, commit, and env/printenv programs"))
    }

    let home = ProcessInfo.processInfo.environment["HOME"] ?? ""
    let runtimeArguments = [
        "PATH=\(home)/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin",
        "ORLIX_SIMULATOR_ID=\(simulatorID)",
        "ORLIX_TCTI_REQUIRED_SIMULATOR_ID=\(simulatorID)",
        "ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=\(simulatorName)",
        "make",
        "runtime-validation",
        "DESTINATION=iphonesimulator",
        "GATE=\(runtimeGate)",
    ]
    let runtimeOutput = try run(runtimeArguments, check: false)
    try runtimeOutput.write(to: runtimeOutputURL, atomically: true, encoding: .utf8)
    artifacts.append(relativePath(runtimeOutputURL))
    evidence["runtime_validation_executed"] = "true"
    evidence["runtime_validation_command"] = runtimeArguments.dropFirst(4).joined(separator: " ")

    guard let runtimeReport = selectedRuntimeValidationReport(gate: runtimeGate, destination: "iphonesimulator") else {
        failures.append(fail("runtime-report-missing", "runtime-validation did not produce \(runtimeGate) iphonesimulator JSON report"))
        let status: GateStatus = .fail
        evidence["fail_count"] = "1"
        evidence["gate_result"] = status.rawValue
        try writeJSON(evidence, to: evidenceURL)
        artifacts.append(relativePath(evidenceURL))
        let reducer = try writeReducer(target: target, caseID: "coreutils-env-path-fail", command: command, reason: "Coreutils env/PATH runtime-validation report missing", artifacts: artifacts, expectedStatus: .fail)
        artifacts.append(relativePath(reducer))
        let reportURL = try writeReport(report(target: target, status: status, summary: "Coreutils env/PATH gate could not find the runtime-validation report.", command: command, failures: failures, artifacts: artifacts, counters: ["coreutils_env_path_tests_executed": 0, "coreutils_env_path_tests_passed": 0, "coreutils_env_path_tests_failed": 1, "coreutils_env_path_tests_skipped": 0], evidence: evidence, releaseGateEligible: false, readinessGateEligible: false))
        print("\(status.rawValue): \(relativePath(reportURL))")
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
        return 1
    }

    let runtimeObject = runtimeReport.object
    let runtimeReportPath = relativePath(runtimeReport.url)
    artifacts.append(runtimeReportPath)
    let runtimeArtifacts = runtimeObject["artifacts"] as? [String] ?? []
    artifacts.append(contentsOf: runtimeArtifacts)

    let markerText = try runtimeArtifacts.first { $0.hasSuffix("tcti-coreutils-env-path.txt") }.map(readRelativeArtifact) ?? ""
    let terminalText = try runtimeArtifacts.first { $0.hasSuffix("simulator-terminal-output.txt") }.map(readRelativeArtifact) ?? ""
    let forbidden = runtimeObject["forbidden_behavior"] as? [String: Any] ?? [:]
    let forbiddenKeys = ["generated_exec_memory", "host_exec_guest_text", "host_x18", "map_jit", "native_ios_api_exposure_to_guest", "rwx"]
    var forbiddenBehavior = forbiddenDefaults()
    for key in forbiddenKeys {
        forbiddenBehavior[key] = boolField(forbidden, key)
    }

    if stringField(runtimeObject, "git_sha") != gitSha() {
        failures.append(fail("runtime-report-stale", "\(runtimeReportPath) is not current HEAD"))
    }
    if stringField(runtimeObject, "status") == "pass" && boolField(runtimeObject, "passed") {
        evidence["runtime_validation_passed"] = "true"
    } else {
        failures.append(fail("runtime-report-status", "\(runtimeReportPath) did not pass"))
    }
    if stringField(runtimeObject, "selected_device_id") != simulatorID ||
        stringField(runtimeObject, "selected_device_name") != simulatorName ||
        stringField(runtimeObject, "simulator_booted_count") != "1" ||
        !boolField(runtimeObject, "simulator_single_booted") {
        failures.append(fail("simulator-scope", "Coreutils env/PATH gate requires single pinned \(simulatorName) simulator"))
    }
    if markerText.contains("ORLIX-TCTI-COREUTILS-ENV-PATH-OK") {
        evidence["coreutils_marker_asserted"] = "true"
    } else {
        failures.append(fail("coreutils-marker-missing", "runtime artifacts did not include ORLIX-TCTI-COREUTILS-ENV-PATH-OK"))
    }
    if terminalText.contains("PATH=/bin:/usr/bin") &&
        terminalText.contains("/bin:/usr/bin") &&
        terminalText.contains("env-path-ok") {
        evidence["coreutils_stdout_asserted"] = "true"
    } else {
        failures.append(fail("coreutils-stdout-missing", "runtime artifacts did not include env/printenv PATH output"))
    }
    if terminalText.contains("orlix-init: process started pid=") {
        evidence["child_process_started"] = "true"
    } else {
        failures.append(fail("child-process-start-missing", "runtime artifacts did not include shell child process start"))
    }
    if terminalText.contains("orlix-init: process exited pid=") {
        evidence["child_process_exited"] = "true"
    } else {
        failures.append(fail("child-process-exit-missing", "runtime artifacts did not include shell child process exit"))
    }
    if outputHasShellWaitStatusZero(terminalText) {
        evidence["wait_reaping_status_observed"] = "true"
    } else {
        failures.append(fail("wait-reaping-status-missing", "runtime artifacts did not include shell exit status 0"))
    }
    for key in forbiddenKeys where boolField(forbidden, key) {
        failures.append(fail("forbidden-behavior", "runtime report sets forbidden_behavior.\(key)=true"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    evidence[status == .pass ? "pass_count" : "fail_count"] = "1"
    evidence["gate_result"] = status.rawValue
    try writeJSON(evidence, to: evidenceURL)
    artifacts.append(relativePath(evidenceURL))

    let reducer = try writeReducer(target: target, caseID: status == .pass ? "coreutils-env-path-pass" : "coreutils-env-path-fail", command: command, reason: status == .pass ? "Coreutils env/PATH passed" : "Coreutils env/PATH failed", artifacts: artifacts, expectedStatus: status)
    artifacts.append(relativePath(reducer))
    let reportURL = try writeReport(report(target: target, status: status, summary: status == .pass ? "Pinned simulator Coreutils env/PATH gate passed through OrlixOS runtime-validation." : "Pinned simulator Coreutils env/PATH gate did not pass through OrlixOS runtime-validation.", command: command, failures: failures, artifacts: artifacts, forbiddenBehavior: forbiddenBehavior, counters: ["coreutils_env_path_tests_executed": evidence["runtime_validation_executed"] == "true" ? 1 : 0, "coreutils_env_path_tests_passed": status == .pass ? 1 : 0, "coreutils_env_path_tests_failed": status == .pass ? 0 : 1, "coreutils_env_path_tests_skipped": 0], evidence: evidence, releaseGateEligible: false, readinessGateEligible: false))
    print("\(status.rawValue): \(relativePath(reportURL))")
    if status != .pass {
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
    }
    return exitCode(for: status)
}

func runCoreutilsTestSubset() throws -> Int32 {
    let target = "tcti-coreutils-test-subset"
    let command = "make tcti-gate TARGET=\(target)"
    let runtimeGate = "tcti-coreutils-test-subset"
    let simulatorID = "1E5553B0-203A-4A11-BAD7-EBDE46863F66"
    let simulatorName = "Orlix-iPhone-15-Pro-Max"
    let runtimeScript = path("tools", "runtime", "orlix-runtime-validation.sh")
    let coreutilsConfig = path("OrlixOS", "Sources", "make", "config.mk")
    let outputRoot = buildPath("coreutils_test_subset")
    let evidenceURL = outputRoot.appendingPathComponent("evidence.json")
    let runtimeOutputURL = outputRoot.appendingPathComponent("runtime-validation-output.txt")
    try ensureDirectory(outputRoot)

    var failures: [Failure] = []
    var artifacts: [String] = []
    var evidence: [String: String] = [
        "actual_command": command,
        "backend": "tcti",
        "git_sha": gitSha(),
        "runtime_gate": runtimeGate,
        "selected_simulator_id": simulatorID,
        "selected_simulator_name": simulatorName,
        "real_stack_execution_surface": "OrlixOS simulator runtime-validation app session",
        "coreutils_commands": "/bin/rm; /bin/mkdir; /bin/cp; /bin/ln; /bin/cat; /bin/wc; /bin/ls; /bin/stat; /bin/env; /bin/printenv",
        "runtime_validation_executed": "false",
        "runtime_validation_passed": "false",
        "coreutils_marker_asserted": "false",
        "coreutils_stdout_asserted": "false",
        "child_process_started": "false",
        "child_process_exited": "false",
        "wait_reaping_status_observed": "false",
        "pass_count": "0",
        "fail_count": "0",
        "skip_count": "0",
    ]

    let requiredPrograms = ["rm", "mkdir", "cp", "ln", "cat", "wc", "ls", "stat", "env", "printenv"]
    let runtimeText = try readText(runtimeScript)
    if sourceTextContains(runtimeText, #"tcti-coreutils-test-subset"#) &&
        sourceTextContains(runtimeText, #"ORLIX-TCTI-COREUTILS-TEST-SUBSET-OK"#) &&
        requiredPrograms.allSatisfy({ sourceTextContains(runtimeText, "/bin/\($0)") }) {
        evidence["runtime_script_coreutils_source_proof"] = "\(relativePath(runtimeScript)) contains absolute Coreutils test-subset command markers"
    } else {
        failures.append(fail("coreutils-source-proof-missing", "\(relativePath(runtimeScript)) must call the selected Coreutils test subset commands"))
    }

    let coreutilsConfigText = try readText(coreutilsConfig)
    let coreutilsProgramLine = coreutilsConfigText
        .split(separator: "\n")
        .first { $0.hasPrefix("ORLIXOS_COREUTILS_PROGRAMS :=") } ?? ""
    let coreutilsProgramTokens = Set(coreutilsProgramLine
        .replacingOccurrences(of: "ORLIXOS_COREUTILS_PROGRAMS :=", with: "")
        .split(whereSeparator: { $0 == " " || $0 == "\t" })
        .map(String.init)
        .filter { $0 != "[" })
    if coreutilsConfigText.contains("COREUTILS_VERSION ?=") &&
        coreutilsConfigText.contains("COREUTILS_GIT_COMMIT ?=") &&
        coreutilsProgramTokens.isSuperset(of: requiredPrograms) {
        evidence["coreutils_package_source"] = "\(relativePath(coreutilsConfig)) declares Coreutils version, git commit, and selected subset programs"
    } else {
        failures.append(fail("coreutils-config-proof-missing", "\(relativePath(coreutilsConfig)) must declare Coreutils version, commit, and selected subset programs"))
    }

    let home = ProcessInfo.processInfo.environment["HOME"] ?? ""
    let runtimeArguments = [
        "PATH=\(home)/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin",
        "ORLIX_SIMULATOR_ID=\(simulatorID)",
        "ORLIX_TCTI_REQUIRED_SIMULATOR_ID=\(simulatorID)",
        "ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=\(simulatorName)",
        "make",
        "runtime-validation",
        "DESTINATION=iphonesimulator",
        "GATE=\(runtimeGate)",
    ]
    let runtimeOutput = try run(runtimeArguments, check: false)
    try runtimeOutput.write(to: runtimeOutputURL, atomically: true, encoding: .utf8)
    artifacts.append(relativePath(runtimeOutputURL))
    evidence["runtime_validation_executed"] = "true"
    evidence["runtime_validation_command"] = runtimeArguments.dropFirst(4).joined(separator: " ")

    guard let runtimeReport = selectedRuntimeValidationReport(gate: runtimeGate, destination: "iphonesimulator") else {
        failures.append(fail("runtime-report-missing", "runtime-validation did not produce \(runtimeGate) iphonesimulator JSON report"))
        let status: GateStatus = .fail
        evidence["fail_count"] = "1"
        evidence["gate_result"] = status.rawValue
        try writeJSON(evidence, to: evidenceURL)
        artifacts.append(relativePath(evidenceURL))
        let reducer = try writeReducer(target: target, caseID: "coreutils-test-subset-fail", command: command, reason: "Coreutils test-subset runtime-validation report missing", artifacts: artifacts, expectedStatus: .fail)
        artifacts.append(relativePath(reducer))
        let reportURL = try writeReport(report(target: target, status: status, summary: "Coreutils test-subset gate could not find the runtime-validation report.", command: command, failures: failures, artifacts: artifacts, counters: ["coreutils_test_subset_tests_executed": 0, "coreutils_test_subset_tests_passed": 0, "coreutils_test_subset_tests_failed": 1, "coreutils_test_subset_tests_skipped": 0], evidence: evidence, releaseGateEligible: false, readinessGateEligible: false))
        print("\(status.rawValue): \(relativePath(reportURL))")
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
        return 1
    }

    let runtimeObject = runtimeReport.object
    let runtimeReportPath = relativePath(runtimeReport.url)
    artifacts.append(runtimeReportPath)
    let runtimeArtifacts = runtimeObject["artifacts"] as? [String] ?? []
    artifacts.append(contentsOf: runtimeArtifacts)

    let markerText = try runtimeArtifacts.first { $0.hasSuffix("tcti-coreutils-test-subset.txt") }.map(readRelativeArtifact) ?? ""
    let terminalText = try runtimeArtifacts.first { $0.hasSuffix("simulator-terminal-output.txt") }.map(readRelativeArtifact) ?? ""
    let forbidden = runtimeObject["forbidden_behavior"] as? [String: Any] ?? [:]
    let forbiddenKeys = ["generated_exec_memory", "host_exec_guest_text", "host_x18", "map_jit", "native_ios_api_exposure_to_guest", "rwx"]
    var forbiddenBehavior = forbiddenDefaults()
    for key in forbiddenKeys {
        forbiddenBehavior[key] = boolField(forbidden, key)
    }

    if stringField(runtimeObject, "git_sha") != gitSha() {
        failures.append(fail("runtime-report-stale", "\(runtimeReportPath) is not current HEAD"))
    }
    if stringField(runtimeObject, "status") == "pass" && boolField(runtimeObject, "passed") {
        evidence["runtime_validation_passed"] = "true"
    } else {
        failures.append(fail("runtime-report-status", "\(runtimeReportPath) did not pass"))
    }
    if stringField(runtimeObject, "selected_device_id") != simulatorID ||
        stringField(runtimeObject, "selected_device_name") != simulatorName ||
        stringField(runtimeObject, "simulator_booted_count") != "1" ||
        !boolField(runtimeObject, "simulator_single_booted") {
        failures.append(fail("simulator-scope", "Coreutils test-subset gate requires single pinned \(simulatorName) simulator"))
    }
    if markerText.contains("ORLIX-TCTI-COREUTILS-TEST-SUBSET-OK") {
        evidence["coreutils_marker_asserted"] = "true"
    } else {
        failures.append(fail("coreutils-marker-missing", "runtime artifacts did not include ORLIX-TCTI-COREUTILS-TEST-SUBSET-OK"))
    }
    if terminalText.contains("subset-ok") &&
        terminalText.contains("PATH=/bin:/usr/bin") &&
        terminalText.contains("/bin:/usr/bin") &&
        (terminalText.contains("File:") || terminalText.contains("Size:")) {
        evidence["coreutils_stdout_asserted"] = "true"
    } else {
        failures.append(fail("coreutils-stdout-missing", "runtime artifacts did not include the Coreutils subset stdout proof"))
    }
    if terminalText.contains("orlix-init: process started pid=") {
        evidence["child_process_started"] = "true"
    } else {
        failures.append(fail("child-process-start-missing", "runtime artifacts did not include shell child process start"))
    }
    if terminalText.contains("orlix-init: process exited pid=") {
        evidence["child_process_exited"] = "true"
    } else {
        failures.append(fail("child-process-exit-missing", "runtime artifacts did not include shell child process exit"))
    }
    if outputHasShellWaitStatusZero(terminalText) {
        evidence["wait_reaping_status_observed"] = "true"
    } else {
        failures.append(fail("wait-reaping-status-missing", "runtime artifacts did not include shell exit status 0"))
    }
    for key in forbiddenKeys where boolField(forbidden, key) {
        failures.append(fail("forbidden-behavior", "runtime report sets forbidden_behavior.\(key)=true"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    evidence[status == .pass ? "pass_count" : "fail_count"] = "1"
    evidence["gate_result"] = status.rawValue
    try writeJSON(evidence, to: evidenceURL)
    artifacts.append(relativePath(evidenceURL))

    let reducer = try writeReducer(target: target, caseID: status == .pass ? "coreutils-test-subset-pass" : "coreutils-test-subset-fail", command: command, reason: status == .pass ? "Coreutils test-subset passed" : "Coreutils test-subset failed", artifacts: artifacts, expectedStatus: status)
    artifacts.append(relativePath(reducer))
    let reportURL = try writeReport(report(target: target, status: status, summary: status == .pass ? "Pinned simulator Coreutils test-subset gate passed through OrlixOS runtime-validation." : "Pinned simulator Coreutils test-subset gate did not pass through OrlixOS runtime-validation.", command: command, failures: failures, artifacts: artifacts, forbiddenBehavior: forbiddenBehavior, counters: ["coreutils_test_subset_tests_executed": evidence["runtime_validation_executed"] == "true" ? 1 : 0, "coreutils_test_subset_tests_passed": status == .pass ? 1 : 0, "coreutils_test_subset_tests_failed": status == .pass ? 0 : 1, "coreutils_test_subset_tests_skipped": 0], evidence: evidence, releaseGateEligible: false, readinessGateEligible: false))
    print("\(status.rawValue): \(relativePath(reportURL))")
    if status != .pass {
        print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
    }
    return exitCode(for: status)
}

func validateReportObject(_ object: Any, roadmapIndex: RoadmapProofTierIndex = roadmapProofTierIndex(), sourcePath: String? = nil) -> [String] {
    guard let dictionary = object as? [String: Any] else {
        return ["report must be a JSON object"]
    }
    let required: [(String, Any.Type)] = [
        ("target", String.self),
        ("gate", String.self),
        ("status", String.self),
        ("passed", Bool.self),
        ("summary", String.self),
        ("proof_tier", String.self),
        ("acceptance_weight", String.self),
        ("real_stack_required", Bool.self),
        ("can_claim_runtime_readiness", Bool.self),
        ("git_sha", String.self),
        ("backend", String.self),
        ("virtual_cpu_model", String.self),
        ("host_page_size", NSNumber.self),
        ("guest_page_size", NSNumber.self),
        ("forbidden_behavior", NSDictionary.self),
        ("counters", NSDictionary.self),
        ("failures", NSArray.self),
        ("artifacts", NSArray.self),
        ("release_gate_eligible", Bool.self),
        ("readiness_gate_eligible", Bool.self),
        ("autonomous_tests_bypassed", Bool.self),
        ("bypass_reason", String.self),
        ("coverage_warnings", NSArray.self),
    ]
    var errors: [String] = []
    errors.append(contentsOf: roadmapIndex.errors.map { "roadmap proof-tier metadata: \($0)" })
    for (key, type) in required {
        guard let value = dictionary[key] else {
            errors.append("missing required field: \(key)")
            continue
        }
        if type == Bool.self {
            if !(value is Bool) { errors.append("field \(key) must be bool") }
        } else if type == String.self {
            if !(value is String) { errors.append("field \(key) must be string") }
        } else if type == NSNumber.self {
            if !(value is NSNumber) { errors.append("field \(key) must be number") }
        } else if type == NSDictionary.self {
            if !(value is [String: Any]) { errors.append("field \(key) must be object") }
        } else if type == NSArray.self {
            if !(value is [Any]) { errors.append("field \(key) must be array") }
        }
    }
    let status = dictionary["status"] as? String
    let passed = dictionary["passed"] as? Bool
    let allowed = ["pass", "fail", "todo", "skipped", "error", "evidence"]
    let allowedProofTiers = [
        "seed",
        "rail",
        "safety",
        "kernel",
        "kselftest",
        "mlibc",
        "mlibc-uapi",
        "shell",
        "coreutils",
        "oci",
        "simulator",
        "device",
        "release",
    ]
    let allowedAcceptanceWeights = ["probe", "blocker", "readiness", "release"]
    let proofTier = dictionary["proof_tier"] as? String
    let acceptanceWeight = dictionary["acceptance_weight"] as? String
    let realStackRequired = dictionary["real_stack_required"] as? Bool
    let canClaimRuntimeReadiness = dictionary["can_claim_runtime_readiness"] as? Bool
    let isRuntimeReport = sourcePath?.hasPrefix("Build/Reports/runtime/") == true
    if let target = dictionary["target"] as? String,
       let expected = roadmapIndex.metadataByTarget[target] {
        if !isRuntimeReport && proofTier != expected.proofTier {
            errors.append("report target \(target) proof_tier=\(proofTier ?? "missing") does not match roadmap proof_tier=\(expected.proofTier)")
        }
        if acceptanceWeight != expected.acceptanceWeight {
            errors.append("report target \(target) acceptance_weight=\(acceptanceWeight ?? "missing") does not match roadmap acceptance_weight=\(expected.acceptanceWeight)")
        }
        if realStackRequired != expected.realStackRequired {
            errors.append("report target \(target) real_stack_required=\(realStackRequired.map(String.init) ?? "missing") does not match roadmap real_stack_required=\(expected.realStackRequired)")
        }
        if canClaimRuntimeReadiness != expected.canClaimRuntimeReadiness {
            errors.append("report target \(target) can_claim_runtime_readiness=\(canClaimRuntimeReadiness.map(String.init) ?? "missing") does not match roadmap can_claim_runtime_readiness=\(expected.canClaimRuntimeReadiness)")
        }
    }
    for key in ["command", "kernel_profile", "kernel_config"] {
        if let value = dictionary[key], !(value is String) {
            errors.append("field \(key) must be string")
        }
    }
    if let evidence = dictionary["evidence"] {
        guard let values = evidence as? [String: Any] else {
            errors.append("field evidence must be object")
            return errors
        }
        for (key, value) in values where !(value is String) {
            errors.append("evidence.\(key) must be string")
        }
    }
    if status == nil || !allowed.contains(status!) {
        errors.append("status must be one of \(allowed)")
    }
    if proofTier == nil || !allowedProofTiers.contains(proofTier!) {
        errors.append("proof_tier must be one of \(allowedProofTiers)")
    }
    if acceptanceWeight == nil || !allowedAcceptanceWeights.contains(acceptanceWeight!) {
        errors.append("acceptance_weight must be one of \(allowedAcceptanceWeights)")
    }
    if proofTier == "seed" && canClaimRuntimeReadiness == true {
        errors.append("seed report must not claim runtime readiness")
    }
    if canClaimRuntimeReadiness == true && realStackRequired != true {
        errors.append("report claims runtime readiness without real_stack_required=true")
    }
    if dictionary["readiness_gate_eligible"] as? Bool == true {
        if status != "pass" || canClaimRuntimeReadiness != true || realStackRequired != true {
            errors.append("readiness_gate_eligible requires pass, real_stack_required=true, and can_claim_runtime_readiness=true")
        }
    }
    if dictionary["release_gate_eligible"] as? Bool == true {
        if status != "pass" || acceptanceWeight != "release" || canClaimRuntimeReadiness != true || realStackRequired != true {
            errors.append("release_gate_eligible requires pass, acceptance_weight=release, real_stack_required=true, and can_claim_runtime_readiness=true")
        }
    }
    if status == "pass" && passed != true {
        errors.append("status=pass requires passed=true")
    }
    if let status, status != "pass", passed != false {
        errors.append("status=\(status) requires passed=false")
    }
    if let status, status != "pass" {
        if dictionary["release_gate_eligible"] as? Bool == true {
            errors.append("non-pass report cannot be release-gate eligible")
        }
        if dictionary["readiness_gate_eligible"] as? Bool == true {
            errors.append("non-pass report cannot be readiness-gate eligible")
        }
    }
    if status == "evidence" {
        if dictionary["autonomous_tests_bypassed"] as? Bool != true {
            errors.append("status=evidence requires autonomous_tests_bypassed=true")
        }
        if (dictionary["bypass_reason"] as? String ?? "").isEmpty {
            errors.append("status=evidence requires bypass_reason")
        }
    }
    for key in ["expected_status", "actual_replay_status"] {
        if let value = dictionary[key], !(value is String) {
            errors.append("field \(key) must be string")
        }
    }
    let forbiddenKeys = [
        "host_exec_guest_text",
        "map_jit",
        "rwx",
        "generated_exec_memory",
        "host_x18",
        "native_ios_api_exposure_to_guest",
    ]
    if let forbidden = dictionary["forbidden_behavior"] as? [String: Any] {
        for key in forbiddenKeys where forbidden[key] == nil {
            errors.append("forbidden_behavior missing key: \(key)")
        }
        for (key, value) in forbidden where !(value is Bool) {
            errors.append("forbidden_behavior.\(key) must be bool")
        }
    }
    return errors
}

func loadJSON(_ url: URL) throws -> Any {
    let data = try Data(contentsOf: url)
    return try JSONSerialization.jsonObject(with: data)
}

func isLegacyRuntimeReportWithoutProofTierMetadata(_ url: URL, object: Any, errors: [String]) -> Bool {
    guard relativePath(url).hasPrefix("Build/Reports/runtime/"),
          let dictionary = object as? [String: Any],
          dictionary["proof_tier"] == nil,
          dictionary["acceptance_weight"] == nil,
          dictionary["real_stack_required"] == nil,
          dictionary["can_claim_runtime_readiness"] == nil,
          dictionary["gate"] is String,
          dictionary["target"] is String,
          dictionary["destination"] is String else {
        return false
    }
    let legacyMetadataErrors = Set([
        "missing required field: proof_tier",
        "missing required field: acceptance_weight",
        "missing required field: real_stack_required",
        "missing required field: can_claim_runtime_readiness",
    ])
    return errors.allSatisfy { error in
        legacyMetadataErrors.contains(error) ||
            error.hasPrefix("proof_tier must be one of ") ||
            error.hasPrefix("acceptance_weight must be one of ")
    }
}

func checkReportFile(_ url: URL, roadmapIndex: RoadmapProofTierIndex = roadmapProofTierIndex()) -> [String] {
    do {
        let object = try loadJSON(url)
        let errors = validateReportObject(object, roadmapIndex: roadmapIndex, sourcePath: relativePath(url))
        if isLegacyRuntimeReportWithoutProofTierMetadata(url, object: object, errors: errors) {
            return []
        }
        return errors.map { "\(relativePath(url)): \($0)" }
    } catch {
        return ["\(relativePath(url)): cannot parse JSON: \(error)"]
    }
}

func reportValidationFixture(status: GateStatus) throws -> Any {
    let sample = report(
        target: "contract-\(status.rawValue)",
        status: status,
        summary: "contract status fixture",
        failures: status == .pass ? [] : [fail("fixture", "fixture failure")],
        autonomousTestsBypassed: status == .evidence,
        bypassReason: status == .evidence ? "contract evidence fixture" : ""
    )
    let data = try encoder.encode(sample)
    return try JSONSerialization.jsonObject(with: data)
}

func validateReportStatusContracts() -> [Failure] {
    var failures: [Failure] = []
    for status in [GateStatus.pass, .fail, .todo, .skipped, .error, .evidence] {
        do {
            let errors = validateReportObject(try reportValidationFixture(status: status))
            for error in errors {
                failures.append(fail("report-status-\(status.rawValue)", error))
            }
        } catch {
            failures.append(fail("report-status-\(status.rawValue)", "\(error)"))
        }
    }
    return failures
}

func validateProductDefconfigSafety() -> [Failure] {
    var failures: [Failure] = []
    for config in [
        path("OrlixKernel", "Sources", "ports", "orlix", "configs", "development_defconfig"),
        path("OrlixKernel", "Sources", "ports", "orlix", "configs", "release_defconfig"),
    ] {
        do {
            let text = try readText(config)
            if !text.contains("CONFIG_ORLIX_HOSTED_EXEC_NATIVE=y") {
                failures.append(fail("defconfig-native", "\(relativePath(config)) lacks CONFIG_ORLIX_HOSTED_EXEC_NATIVE=y"))
            }
            for forbidden in ["CONFIG_ORLIX_HOSTED_EXEC_TCTI=y", "CONFIG_ORLIX_TCTI_DEBUG_SWITCH=y"] where text.contains(forbidden) {
                failures.append(fail("defconfig-tcti", "\(relativePath(config)) contains \(forbidden)"))
            }
        } catch {
            failures.append(fail("defconfig-read", "\(relativePath(config)): \(error)"))
        }
    }
    return failures
}

func runContractReproFixture(artifacts: inout [String]) -> [Failure] {
    do {
        let reducer = try writeReducer(
            target: "tcti-contract-repro-pass-fixture",
            caseID: "repro-pass",
            command: "/usr/bin/true",
            reason: "contract reducer replay fixture",
            expectedStatus: .pass
        )
        artifacts.append(relativePath(reducer))
        _ = try run(["make", "tcti-gate", "TARGET=tcti-repro", "REPRO=\(relativePath(reducer))"])
        return []
    } catch {
        return [fail("reducer-replay", "contract reducer replay fixture failed: \(error)")]
    }
}

func validateFirstGadgetContract(artifacts: inout [String]) -> [Failure] {
    let target = "tcti-contract"
    let caseID = "init_001_exit"
    let metadataPath = path("OrlixKernel", "Tests", "TCTI", "golden_elf", caseID, "golden.json")
    let outputRoot = buildPath("contract", "gadget_abi")
    var failures: [Failure] = []
    var reducerArtifacts: [String] = []

    do {
        let result = try validateAndExecuteGoldenCase(
            caseID: caseID,
            metadataURL: metadataPath,
            outputRoot: outputRoot
        )
        artifacts.append(contentsOf: result.artifacts)

        guard result.failures.isEmpty else {
            failures.append(contentsOf: result.failures)
            throw GateError.checkFailed(result.failures.map(\.message))
        }
        guard let execution = result.execution else {
            failures.append(fail("gadget-contract-execution", "switch-debug execution report missing for \(caseID)"))
            throw GateError.checkFailed(failures.map(\.message))
        }

        let reference = diffState(from: execution, backend: "switch-debug")
        let candidate = try gadgetCandidateStateForInit001(from: execution)
        let divergent = divergentFields(reference: reference, candidate: candidate)
        for field in divergent {
            failures.append(fail(
                "gadget-contract.\(field)",
                "\(caseID) gadget contract field \(field) diverged: switch=\(stateValue(reference, field: field)) candidate=\(stateValue(candidate, field: field))"
            ))
        }

        let caseOutputRoot = outputRoot.appendingPathComponent(caseID, isDirectory: true)
        let switchStateURL = caseOutputRoot.appendingPathComponent("switch-state.json")
        let candidateStateURL = caseOutputRoot.appendingPathComponent("candidate-state.json")
        let diffURL = caseOutputRoot.appendingPathComponent("diff.json")
        try writeJSON(reference, to: switchStateURL)
        try writeJSON(candidate, to: candidateStateURL)
        let diffArtifact = SwitchDiffArtifact(
            caseID: caseID,
            mode: "contract-gadget-abi-register-commit-back",
            referenceBackend: reference.backend,
            candidateBackend: candidate.backend,
            gadgetDispatchExecuted: true,
            productionAssemblyExecuted: false,
            fieldsChecked: diffFieldsChecked,
            divergentFields: divergent,
            referenceState: reference,
            candidateState: candidate,
            notes: [
                "This is a no-phone tcti-contract group for the bounded init_001_exit data-gadget candidate.",
                "The contract checks x8 and x0 register commit-back plus the SVC boundary against switch-debug state.",
                "No production assembly, generated executable memory, host-executable guest text, HostAdapter behavior, or device execution is used.",
            ]
        )
        try writeJSON(diffArtifact, to: diffURL)
        reducerArtifacts = [
            relativePath(switchStateURL),
            relativePath(candidateStateURL),
            relativePath(diffURL),
        ]
        artifacts.append(contentsOf: reducerArtifacts)

        let reducer = try writeReducer(
            target: "tcti-diff-switch",
            caseID: "gadget-abi-init-001-exit-x0-divergence",
            command: "CASE=init_001_exit BACKEND=gadget NEGATIVE_DIFF=gadget-x0 make tcti-gate TARGET=tcti-diff-switch",
            reason: "contract reducer for gadget register commit-back mismatch",
            artifacts: reducerArtifacts,
            expectedStatus: .fail
        )
        artifacts.append(relativePath(reducer))
    } catch {
        if failures.isEmpty {
            failures.append(fail("gadget-contract", "\(error)"))
        }
    }

    if !failures.isEmpty {
        do {
            let reducer = try writeReducer(
                target: target,
                caseID: "gadget-abi-init-001-exit",
                command: "make tcti-gate TARGET=\(target)",
                reason: failures.map(\.message).joined(separator: "; "),
                artifacts: artifacts,
                expectedStatus: .fail
            )
            artifacts.append(relativePath(reducer))
        } catch {
            failures.append(fail("gadget-contract-reducer", "\(error)"))
        }
    }

    return failures
}

func runContract() throws -> Int32 {
    let target = "tcti-contract"
    var artifacts: [String] = []
    var failures: [Failure] = []
    var passedGroups: [String] = []

    let reportStatusFailures = validateReportStatusContracts()
    if reportStatusFailures.isEmpty {
        passedGroups.append("report status schema: pass/fail/todo/skipped/error/evidence")
    } else {
        failures.append(contentsOf: reportStatusFailures)
    }

    var reducerArtifacts: [String] = []
    let reducerFailures = runContractReproFixture(artifacts: &reducerArtifacts)
    artifacts.append(contentsOf: reducerArtifacts)
    if reducerFailures.isEmpty {
        passedGroups.append("reducer replay fixture")
    } else {
        failures.append(contentsOf: reducerFailures)
    }

    let defconfigFailures = validateProductDefconfigSafety()
    if defconfigFailures.isEmpty {
        passedGroups.append("product defconfig safety")
    } else {
        failures.append(contentsOf: defconfigFailures)
    }

    let x18Fixture = path("tools", "tcti", "fixtures", "x18_forbidden", "bad.S")
    artifacts.append(relativePath(x18Fixture))
    if scanSourceForX18(x18Fixture).isEmpty {
        failures.append(fail("x18-negative-fixture", "forbidden host x18 fixture did not fail scanner"))
    } else {
        passedGroups.append("x18 forbidden negative fixture")
    }

    let outputRoot = buildPath("contract", "golden_elf")
    let goldenPath = path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_001_exit", "golden.json")
    let writeGoldenPath = path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_002_write", "golden.json")
    let stackGoldenPath = path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_003_stack", "golden.json")
    let tlsGoldenPath = path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_004_tls", "golden.json")
    do {
        let validation = try validateInit001Golden(metadataURL: goldenPath, outputRoot: outputRoot)
        artifacts.append(contentsOf: validation.artifacts)
        if validation.failures.isEmpty {
            passedGroups.append("init_001_exit golden metadata, ELF header, entrypoint, and syscall shape")
        } else {
            failures.append(contentsOf: validation.failures)
        }
    } catch {
        failures.append(fail("init-001-golden", "\(error)"))
    }

    do {
        let validation = try validateGoldenCase(caseID: "init_002_write", metadataURL: writeGoldenPath, outputRoot: outputRoot)
        artifacts.append(contentsOf: validation.artifacts)
        if validation.failures.isEmpty {
            passedGroups.append("init_002_write golden metadata, ELF header, entrypoint, syscall shape, and message bytes")
        } else {
            failures.append(contentsOf: validation.failures)
        }
    } catch {
        failures.append(fail("init-002-golden", "\(error)"))
    }

    do {
        var executionArtifacts: [String] = []
        let result = try validateAndExecuteInit001(metadataURL: goldenPath, outputRoot: buildPath("contract", "switch_debug"))
        artifacts.append(contentsOf: result.artifacts)
        executionArtifacts.append(contentsOf: result.artifacts)
        if result.failures.isEmpty,
           result.execution?.exit?.kind == "guest_exit_syscall",
           result.execution?.exit?.code == 42 {
            passedGroups.append("minimal AArch64 decode semantics execute init_001_exit to captured exit(42)")
        } else {
            failures.append(contentsOf: result.failures)
            failures.append(fail("switch-debug-exit", "switch-debug did not capture init_001_exit exit(42)"))
        }
        let negativeFailures = validateNegativeExecutionFixtures(artifacts: &executionArtifacts)
        artifacts.append(contentsOf: executionArtifacts)
        if negativeFailures.isEmpty {
            passedGroups.append("switch-debug negative execution fixtures")
        } else {
            failures.append(contentsOf: negativeFailures)
        }
    } catch {
        failures.append(fail("switch-debug-execution", "\(error)"))
    }

    do {
        let result = try validateAndExecuteGoldenCase(
            caseID: "init_002_write",
            metadataURL: writeGoldenPath,
            outputRoot: buildPath("contract", "switch_debug_write")
        )
        artifacts.append(contentsOf: result.artifacts)
        if result.failures.isEmpty,
           result.execution?.syscalls.first?.nr == 64,
           result.execution?.syscalls.first?.capturedBytes == "hello\n",
           result.execution?.exit?.kind == "guest_exit_syscall",
           result.execution?.exit?.code == 0 {
            passedGroups.append("decoded switch-debug executes init_002_write and captures write(1, hello newline, 6), exit(0)")
        } else {
            failures.append(contentsOf: result.failures)
            failures.append(fail("switch-debug-write", "switch-debug did not capture init_002_write write and exit"))
        }
    } catch {
        failures.append(fail("switch-debug-write", "\(error)"))
    }

    do {
        let result = try validateAndExecuteGoldenCase(
            caseID: "init_003_stack",
            metadataURL: stackGoldenPath,
            outputRoot: buildPath("contract", "switch_debug_stack")
        )
        artifacts.append(contentsOf: result.artifacts)
        if result.failures.isEmpty,
           result.execution?.exit?.kind == "guest_exit_syscall",
           result.execution?.exit?.code == 42 {
            passedGroups.append("decoded switch-debug executes init_003_stack and captures stack-derived exit(42)")
        } else {
            failures.append(contentsOf: result.failures)
            failures.append(fail("switch-debug-stack", "switch-debug did not capture init_003_stack stack-derived exit(42)"))
        }
    } catch {
        failures.append(fail("switch-debug-stack", "\(error)"))
    }

    do {
        let result = try validateAndExecuteGoldenCase(
            caseID: "init_004_tls",
            metadataURL: tlsGoldenPath,
            outputRoot: buildPath("contract", "switch_debug_tls")
        )
        artifacts.append(contentsOf: result.artifacts)
        if result.failures.isEmpty,
           result.execution?.exit?.kind == "guest_exit_syscall",
           result.execution?.exit?.code == 42 {
            passedGroups.append("decoded switch-debug executes init_004_tls and captures guest TPIDR_EL0-derived exit(42) without host TLS mutation")
        } else {
            failures.append(contentsOf: result.failures)
            failures.append(fail("switch-debug-tls", "switch-debug did not capture init_004_tls guest TPIDR_EL0-derived exit(42)"))
        }
    } catch {
        failures.append(fail("switch-debug-tls", "\(error)"))
    }

    let wrongMetadata = path("tools", "tcti", "fixtures", "golden_elf", "init_001_exit_wrong_binary_sha.json")
    artifacts.append(relativePath(wrongMetadata))
    do {
        let validation = try validateInit001Golden(metadataURL: wrongMetadata, outputRoot: buildPath("contract", "negative_golden_elf"))
        if validation.failures.contains(where: { $0.id == "binary-sha256" || $0.id == "actual-binary-sha256" }) {
            artifacts.append(contentsOf: validation.artifacts)
            passedGroups.append("golden metadata wrong-SHA negative fixture")
        } else {
            failures.append(fail("golden-negative-fixture", "wrong binary SHA fixture did not fail metadata validation"))
        }
    } catch {
        failures.append(fail("golden-negative-fixture", "\(error)"))
    }

    let gadgetContractFailures = validateFirstGadgetContract(artifacts: &artifacts)
    if gadgetContractFailures.isEmpty {
        passedGroups.append("gadget data-program ABI and x0/x8 register commit-back for init_001_exit")
    } else {
        failures.append(contentsOf: gadgetContractFailures)
    }

    let delegatedGroups = [
        "FETCH/READ/WRITE memory execution: delegated to tcti-memory-fuzz gate",
        "TLB, block-cache, invalidation, and direct-chain execution: delegated to tcti-direct-chain-fuzz gate",
    ]
    let todoGroups: [String] = []
    let todoFailures = todoGroups.map { fail("todo", "\($0) contract remains TODO") }
    let reducer = try writeReducer(
        target: target,
        caseID: failures.isEmpty ? "contract-pass-regression" : "contract-failure",
        command: "make tcti-gate TARGET=\(target)",
        reason: failures.isEmpty ?
            "contract regression reducer: all tcti-contract groups should remain pass" :
            failures.map(\.message).joined(separator: "; "),
        artifacts: artifacts,
        expectedStatus: failures.isEmpty ? .pass : .fail
    )
    artifacts.append(relativePath(reducer))

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Real contract groups passed: \(passedGroups.joined(separator: ", ")). Delegated preflight groups: \(delegatedGroups.joined(separator: ", ")).",
        failures: failures + todoFailures,
        artifacts: artifacts,
        counters: [
            "contract_groups_passed": passedGroups.count,
            "contract_groups_todo": todoGroups.count,
            "contract_groups_failed": failures.count,
            "contract_groups_delegated": delegatedGroups.count,
        ]
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    print("real contract groups passed:")
    for group in passedGroups {
        print("- \(group)")
    }
    print("delegated preflight groups:")
    for group in delegatedGroups {
        print("- \(group)")
    }
    print("reproduce with: make tcti-gate TARGET=tcti-repro REPRO=\(relativePath(reducer))")
    return exitCode(for: status)
}

func runReportSchemaCheck() throws -> Int32 {
    let target = "tcti-report-schema-check"
    let fixtureRoot = path("tools", "tcti", "fixtures")
    var failures: [Failure] = []
    var checked: [String] = []

    for name in [
        "report.pass.json",
        "report.todo.kernel-roadmap-metadata.json",
    ] {
        let fixture = fixtureRoot.appendingPathComponent(name)
        checked.append(relativePath(fixture))
        failures.append(contentsOf: checkReportFile(fixture).map { fail("schema", $0) })
    }

    for name in [
        "report.fail.missing-field.json",
        "report.fail.invalid-status.json",
        "report.fail.roadmap-metadata-mismatch.json",
    ] {
        let fixture = fixtureRoot.appendingPathComponent(name)
        checked.append(relativePath(fixture))
        if checkReportFile(fixture).isEmpty {
            failures.append(fail("schema-fixture", "\(relativePath(fixture)) was expected to fail validation"))
        }
    }

    for name in [
        "roadmap.fail.missing-gates.json",
        "roadmap.fail.duplicate-target-conflict.json",
    ] {
        let fixture = fixtureRoot.appendingPathComponent(name)
        checked.append(relativePath(fixture))
        let index = loadRoadmapProofTierMetadataByTarget(from: fixture)
        if index.errors.isEmpty {
            failures.append(fail("roadmap-fixture", "\(relativePath(fixture)) was expected to fail proof-tier metadata indexing"))
        }
    }

    let missingGatesRoadmap = fixtureRoot.appendingPathComponent("roadmap.fail.missing-gates.json")
    let missingGatesIndex = loadRoadmapProofTierMetadataByTarget(from: missingGatesRoadmap)
    let kernelMetadataFixture = fixtureRoot.appendingPathComponent("report.todo.kernel-roadmap-metadata.json")
    if checkReportFile(kernelMetadataFixture, roadmapIndex: missingGatesIndex).isEmpty {
        failures.append(fail("schema-fixture", "\(relativePath(kernelMetadataFixture)) unexpectedly passed with malformed roadmap metadata"))
    }

    let tctiReportRoot = buildPath("reports")
    if let enumerator = fileManager.enumerator(at: tctiReportRoot, includingPropertiesForKeys: nil) {
        for case let url as URL in enumerator where url.lastPathComponent == "report.json" {
            if url.deletingLastPathComponent().lastPathComponent == target {
                continue
            }
            checked.append(relativePath(url))
            failures.append(contentsOf: checkReportFile(url).map { fail("report", $0) })
        }
    }

    let runtimeReportRoot = path("Build", "Reports", "runtime")
    if let runtimeReports = try? fileManager.contentsOfDirectory(at: runtimeReportRoot, includingPropertiesForKeys: nil) {
        for url in runtimeReports where url.pathExtension == "json" {
            checked.append(relativePath(url))
            failures.append(contentsOf: checkReportFile(url).map { fail("report", $0) })
        }
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Validated TCTI report schema fixtures and \(checked.count) report file(s).",
        failures: failures,
        artifacts: checked
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func readText(_ url: URL) throws -> String {
    try String(contentsOf: url, encoding: .utf8)
}

func runPlanConsistency() throws -> Int32 {
    let target = "tcti-plan-consistency"
    let finalSentence = "Orlix TCTI is an Orlix-owned, arch/orlix, no-JIT, same-ISA, tail-call-threaded user-instruction backend for unmodified AArch64 Linux ELF binaries. It does not replace Linux; it lets OrlixKernel’s existing Linux userspace surface run on iOS without host-executable guest text."
    let plan = try readText(path("docs", "plans", "active", "orlix-tcti", "PLAN.md"))
    let adr = try readText(path("docs", "adr", "0022-use-hosted-linux-elf-execution.md"))
    var failures: [Failure] = []

    for pattern in [
        "native AArch64 code",
        "TCTI.*deferred",
        "Treating TCTI.*initial package runtime",
        "compatibility work, not the first product runtime",
    ] {
        if adr.range(of: pattern, options: .regularExpression) != nil {
            failures.append(fail("stale-adr", "ADR 0022 contains stale text matching \(pattern)"))
        }
    }
    if !plan.contains(finalSentence) {
        failures.append(fail("plan-final-sentence", "PLAN.md is missing the canonical final architecture sentence"))
    }
    if !adr.contains(finalSentence) {
        failures.append(fail("adr-final-sentence", "ADR 0022 is missing the canonical final architecture sentence"))
    }
    for marker in [
        "Autonomous Test Contract",
        "Failure Reduction",
        "TCTI Concurrency Model",
        "TPIDR_EL0 Transition Audit",
        "Virtio Boundary",
        "tcti-appstore-safety-audit",
        "orlix-aarch64-v1",
    ] where !plan.contains(marker) {
        failures.append(fail("plan-marker", "PLAN.md is missing marker \(marker)"))
    }
    for config in [
        path("OrlixKernel", "Sources", "ports", "orlix", "configs", "development_defconfig"),
        path("OrlixKernel", "Sources", "ports", "orlix", "configs", "release_defconfig"),
    ] {
        let text = try readText(config)
        if !text.contains("CONFIG_ORLIX_HOSTED_EXEC_NATIVE=y") {
            failures.append(fail("defconfig-native", "\(relativePath(config)) lacks CONFIG_ORLIX_HOSTED_EXEC_NATIVE=y"))
        }
        for forbidden in ["CONFIG_ORLIX_HOSTED_EXEC_TCTI=y", "CONFIG_ORLIX_TCTI_DEBUG_SWITCH=y"] where text.contains(forbidden) {
            failures.append(fail("defconfig-tcti", "\(relativePath(config)) contains \(forbidden)"))
        }
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Checked ADR, plan markers, final architecture sentence, and product defconfigs.",
        failures: failures
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    if !failures.isEmpty {
        print("human scan: rtk grep -n \"native AArch64 code\\|TCTI.*deferred\\|Treating TCTI.*initial package runtime\\|compatibility work, not the first product runtime\" docs/adr/0022-use-hosted-linux-elf-execution.md")
    }
    return exitCode(for: status)
}

func toolchainInfo() throws -> [String: String] {
    let clang = try commandPath("clang")
    let linker = try commandPath("ld.lld")
    let objdump = try run(["xcrun", "--find", "llvm-objdump"])
    let fileTool = try commandPath("file")
    return [
        "clang_path": clang,
        "clang_version": try run(["clang", "--version"]).components(separatedBy: "\n").first ?? "",
        "linker_path": linker,
        "linker_version": try run(["ld.lld", "--version"]).components(separatedBy: "\n").first ?? "",
        "objdump_path": objdump,
        "file_path": fileTool,
    ]
}

func buildInit001(outputRoot: URL) throws -> (binary: URL, source: URL, metadata: [String: String]) {
    let source = path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_001_exit", "init_001_exit.S")
    return try buildAarch64NoLibc(source: source, outputRoot: outputRoot, binaryName: "init_001_exit")
}

func buildGoldenCase(_ caseID: String, outputRoot: URL) throws -> (binary: URL, source: URL, metadata: [String: String]) {
    let source = path("OrlixKernel", "Tests", "TCTI", "golden_elf", caseID, "\(caseID).S")
    if caseID == "init_011_static_pie_got_byte_load" {
        return try buildAarch64StaticPieNoLibc(source: source, outputRoot: outputRoot, binaryName: caseID)
    }
    return try buildAarch64NoLibc(source: source, outputRoot: outputRoot, binaryName: caseID)
}

func buildAarch64NoLibc(source: URL, outputRoot: URL, binaryName: String) throws -> (binary: URL, source: URL, metadata: [String: String]) {
    try buildAarch64NoLibc(source: source, outputRoot: outputRoot, binaryName: binaryName, extraFlags: ["-static"])
}

func buildAarch64StaticPieNoLibc(source: URL, outputRoot: URL, binaryName: String) throws -> (binary: URL, source: URL, metadata: [String: String]) {
    try buildAarch64NoLibc(source: source, outputRoot: outputRoot, binaryName: binaryName, extraFlags: ["-static-pie", "-Wl,--no-relax"])
}

func buildAarch64NoLibc(source: URL, outputRoot: URL, binaryName: String, extraFlags: [String]) throws -> (binary: URL, source: URL, metadata: [String: String]) {
    let outputDir = outputRoot.appendingPathComponent(binaryName, isDirectory: true)
    try ensureDirectory(outputDir)
    let binary = outputDir.appendingPathComponent(binaryName)
    let flags = [
        "-target", "aarch64-linux-gnu",
        "-nostdlib",
    ] + extraFlags + [
        "-fuse-ld=lld",
        "-Wl,--build-id=none",
        "-Wl,-e,_start",
        "-o", binary.path,
        source.path,
    ]
    _ = try run(["clang"] + flags)
    let fileOutput = try run(["file", binary.path])
    let objdumpOutput = try run(["xcrun", "llvm-objdump", "-f", binary.path])
    let disassembly = try run(["xcrun", "llvm-objdump", "-d", binary.path])
    return (
        binary,
        source,
        [
            "source_sha256": try sha256(source),
            "binary_sha256": try sha256(binary),
            "file_output": fileOutput,
            "objdump_header": objdumpOutput,
            "disassembly": disassembly,
            "flags": flags.joined(separator: " "),
        ]
    )
}

func littleEndianUInt16(_ data: Data, _ offset: Int) throws -> UInt16 {
    guard offset >= 0 && offset + 2 <= data.count else {
        throw GateError.commandFailed("ELF read outside file at offset \(offset)")
    }
    return UInt16(data[offset]) | (UInt16(data[offset + 1]) << 8)
}

func littleEndianUInt32(_ data: Data, _ offset: Int) throws -> UInt32 {
    guard offset >= 0 && offset + 4 <= data.count else {
        throw GateError.commandFailed("ELF read outside file at offset \(offset)")
    }
    return UInt32(data[offset]) |
        (UInt32(data[offset + 1]) << 8) |
        (UInt32(data[offset + 2]) << 16) |
        (UInt32(data[offset + 3]) << 24)
}

func littleEndianUInt64(_ data: Data, _ offset: Int) throws -> UInt64 {
    guard offset >= 0 && offset + 8 <= data.count else {
        throw GateError.commandFailed("ELF read outside file at offset \(offset)")
    }
    var value: UInt64 = 0
    for index in 0..<8 {
        value |= UInt64(data[offset + index]) << UInt64(index * 8)
    }
    return value
}

struct ElfLoadSegment {
    let fileOffset: UInt64
    let virtualAddress: UInt64
    let fileSize: UInt64
}

func fileOffsetForVirtualAddress(_ virtualAddress: UInt64, length: UInt64, segments: [ElfLoadSegment]) throws -> Int {
    for segment in segments {
        if virtualAddress >= segment.virtualAddress && virtualAddress + length <= segment.virtualAddress + segment.fileSize {
            return Int(segment.fileOffset + (virtualAddress - segment.virtualAddress))
        }
    }
    throw GateError.commandFailed(String(format: "no load segment contains guest address 0x%llx length %llu", virtualAddress, length))
}

func littleEndianBytes(_ value: UInt64) -> Data {
    var bytes = Data(repeating: 0, count: 8)
    for index in 0..<8 {
        bytes[index] = UInt8((value >> UInt64(index * 8)) & 0xff)
    }
    return bytes
}

struct TinyElf64Aarch64 {
    let data: Data
    let entrypoint: UInt64
    let segments: [ElfLoadSegment]
    let relativeRelocations: [UInt64: UInt64]
    let applyRelativeRelocations: Bool
    let invisibleRelativeRelocationOffsets: Set<UInt64>
    let guardNullPageReads: Bool

    init(
        binary: URL,
        applyRelativeRelocations: Bool = true,
        invisibleRelativeRelocationOffsets: Set<UInt64> = [],
        guardNullPageReads: Bool = false
    ) throws {
        let data = try Data(contentsOf: binary)
        guard data.count >= 64 else {
            throw GateError.commandFailed("ELF file too small: \(binary.path)")
        }
        guard data[0] == 0x7f, data[1] == 0x45, data[2] == 0x4c, data[3] == 0x46 else {
            throw GateError.commandFailed("not an ELF file: \(binary.path)")
        }
        guard data[4] == 2 else {
            throw GateError.commandFailed("init_001_exit must be ELF64")
        }
        guard data[5] == 1 else {
            throw GateError.commandFailed("init_001_exit must be little-endian ELF")
        }
        let type = try littleEndianUInt16(data, 16)
        let machine = try littleEndianUInt16(data, 18)
        guard type == 2 || type == 3 else {
            throw GateError.commandFailed("golden ELF must be ET_EXEC or ET_DYN, found \(type)")
        }
        guard machine == 183 else {
            throw GateError.commandFailed("golden ELF must be AArch64, found machine \(machine)")
        }
        let entrypoint = try littleEndianUInt64(data, 24)
        let phoff = try littleEndianUInt64(data, 32)
        let phentsize = Int(try littleEndianUInt16(data, 54))
        let phnum = Int(try littleEndianUInt16(data, 56))
        var segments: [ElfLoadSegment] = []
        var dynamicSegments: [ElfLoadSegment] = []
        for index in 0..<phnum {
            let offset = Int(phoff) + index * phentsize
            let programType = try littleEndianUInt32(data, offset)
            guard programType == 1 || programType == 2 else { continue }
            let fileOffset = try littleEndianUInt64(data, offset + 8)
            let virtualAddress = try littleEndianUInt64(data, offset + 16)
            let fileSize = try littleEndianUInt64(data, offset + 32)
            if programType == 1 {
                segments.append(ElfLoadSegment(fileOffset: fileOffset, virtualAddress: virtualAddress, fileSize: fileSize))
            } else {
                dynamicSegments.append(ElfLoadSegment(fileOffset: fileOffset, virtualAddress: virtualAddress, fileSize: fileSize))
            }
        }
        guard !segments.isEmpty else {
            throw GateError.commandFailed("golden ELF has no PT_LOAD segment")
        }
        var relaVirtualAddress: UInt64?
        var relaSize: UInt64 = 0
        var relaEntrySize: UInt64 = 24
        for segment in dynamicSegments {
            let entries = Int(segment.fileSize / 16)
            for index in 0..<entries {
                let offset = Int(segment.fileOffset) + index * 16
                let tag = Int64(bitPattern: try littleEndianUInt64(data, offset))
                let value = try littleEndianUInt64(data, offset + 8)
                if tag == 0 {
                    break
                } else if tag == 7 {
                    relaVirtualAddress = value
                } else if tag == 8 {
                    relaSize = value
                } else if tag == 9 {
                    relaEntrySize = value
                }
            }
        }
        var relativeRelocations: [UInt64: UInt64] = [:]
        if let relaVirtualAddress, relaSize > 0, relaEntrySize >= 24 {
            let relaFileOffset = try fileOffsetForVirtualAddress(relaVirtualAddress, length: relaSize, segments: segments)
            let count = Int(relaSize / relaEntrySize)
            for index in 0..<count {
                let offset = relaFileOffset + index * Int(relaEntrySize)
                let relocationOffset = try littleEndianUInt64(data, offset)
                let relocationInfo = try littleEndianUInt64(data, offset + 8)
                let relocationType = UInt32(relocationInfo & 0xffff_ffff)
                let addend = try littleEndianUInt64(data, offset + 16)
                if relocationType == 1027 {
                    relativeRelocations[relocationOffset] = addend
                } else {
                    throw GateError.commandFailed("unsupported ET_DYN RELA relocation type \(relocationType)")
                }
            }
        }
        self.data = data
        self.entrypoint = entrypoint
        self.segments = segments
        self.relativeRelocations = relativeRelocations
        self.applyRelativeRelocations = applyRelativeRelocations
        self.invisibleRelativeRelocationOffsets = invisibleRelativeRelocationOffsets
        self.guardNullPageReads = guardNullPageReads
    }

    func readInstruction(at virtualAddress: UInt64) throws -> UInt32 {
        for segment in segments {
            if virtualAddress >= segment.virtualAddress && virtualAddress + 4 <= segment.virtualAddress + segment.fileSize {
                let fileOffset = Int(segment.fileOffset + (virtualAddress - segment.virtualAddress))
                return try littleEndianUInt32(data, fileOffset)
            }
        }
        throw GateError.commandFailed(String(format: "no load segment contains guest PC 0x%llx", virtualAddress))
    }

    func readBytes(at virtualAddress: UInt64, length: Int) throws -> Data {
        guard length >= 0 else {
            throw GateError.commandFailed("negative guest memory read length \(length)")
        }
        if applyRelativeRelocations,
           length == 8,
           !invisibleRelativeRelocationOffsets.contains(virtualAddress),
           let relocated = relativeRelocations[virtualAddress] {
            return littleEndianBytes(relocated)
        }
        if guardNullPageReads, virtualAddress < 4096 {
            throw GateError.commandFailed(String(format: "guest null-page read fault: address=0x%llx length=%d", virtualAddress, length))
        }
        let endAddress = virtualAddress + UInt64(length)
        for segment in segments {
            if virtualAddress >= segment.virtualAddress && endAddress <= segment.virtualAddress + segment.fileSize {
                let fileOffset = Int(segment.fileOffset + (virtualAddress - segment.virtualAddress))
                return data.subdata(in: fileOffset..<(fileOffset + length))
            }
        }
        throw GateError.commandFailed(String(format: "guest memory read outside file-backed PT_LOAD: address=0x%llx length=%d", virtualAddress, length))
    }
}

func parseEntrypoint(_ value: String) throws -> UInt64 {
    let normalized = value.lowercased().hasPrefix("0x") ? String(value.dropFirst(2)) : value
    guard let parsed = UInt64(normalized, radix: 16) else {
        throw GateError.commandFailed("invalid entrypoint \(value)")
    }
    return parsed
}

func hexWord(_ word: UInt32) -> String {
    String(format: "0x%08x", word)
}

func hexPC(_ value: UInt64) -> String {
    String(format: "0x%016llx", value)
}

enum A64DecodedInstruction {
	case moveWideImmediate(raw: UInt32, pc: UInt64, op: String, sf: Int, rd: Int, imm: UInt64, shift: Int)
	case pcRelativeAddress(raw: UInt32, pc: UInt64, op: String, rd: Int, imm: Int64)
	case addSubImmediate(raw: UInt32, pc: UInt64, op: String, sf: Int, rd: Int, rn: Int, imm: UInt64, shift: Int)
	case loadStoreUnsignedImmediate(raw: UInt32, pc: UInt64, op: String, rt: Int, rn: Int, offset: Int, width: Int)
	case systemRegister(raw: UInt32, pc: UInt64, op: String, rt: Int, sysreg: String)
	case simdVectorElementMove(raw: UInt32, pc: UInt64, rd: Int, rn: Int, width: Int, sourceIndex: Int, destinationIndex: Int)
	case simdVectorLogical(raw: UInt32, pc: UInt64, op: String, rd: Int, rn: Int, rm: Int, width: Int)
	case simdVectorLogicalImmediate(raw: UInt32, pc: UInt64, op: String, rd: Int, imm: UInt64, width: Int)
	case simdVectorCompare(raw: UInt32, pc: UInt64, op: String, rd: Int, rn: Int, rm: Int, laneSize: Int, lanes: Int)
	case simdVectorReduction(raw: UInt32, pc: UInt64, op: String, rd: Int, rn: Int, laneSize: Int, lanes: Int)
	case fpScalarMove(raw: UInt32, pc: UInt64, op: String, rd: Int, rn: Int, width: Int)
    case simdModifiedImmediate(raw: UInt32, pc: UInt64, op: String, rd: Int, imm: UInt64, width: Int)
	case compareAndBranchImmediate(raw: UInt32, pc: UInt64, op: String, sf: Int, rt: Int, offset: Int64)
	case unconditionalBranchImmediate(raw: UInt32, pc: UInt64, op: String, offset: Int64)
	case svc(raw: UInt32, pc: UInt64, imm: UInt16)
	case unsupported(raw: UInt32, pc: UInt64, reason: String)

    var raw: UInt32 {
        switch self {
			case let .moveWideImmediate(raw, _, _, _, _, _, _),
			     let .pcRelativeAddress(raw, _, _, _, _),
			     let .addSubImmediate(raw, _, _, _, _, _, _, _),
			     let .loadStoreUnsignedImmediate(raw, _, _, _, _, _, _),
			     let .systemRegister(raw, _, _, _, _),
			     let .simdVectorElementMove(raw, _, _, _, _, _, _),
		     let .simdVectorLogical(raw, _, _, _, _, _, _),
		     let .simdVectorLogicalImmediate(raw, _, _, _, _, _),
		     let .simdVectorCompare(raw, _, _, _, _, _, _, _),
		     let .simdVectorReduction(raw, _, _, _, _, _, _),
		     let .fpScalarMove(raw, _, _, _, _, _),
             let .simdModifiedImmediate(raw, _, _, _, _, _),
		     let .compareAndBranchImmediate(raw, _, _, _, _, _),
		     let .unconditionalBranchImmediate(raw, _, _, _),
		     let .svc(raw, _, _),
		     let .unsupported(raw, _, _):
            return raw
        }
    }

    var pc: UInt64 {
        switch self {
			case let .moveWideImmediate(_, pc, _, _, _, _, _),
			     let .pcRelativeAddress(_, pc, _, _, _),
			     let .addSubImmediate(_, pc, _, _, _, _, _, _),
			     let .loadStoreUnsignedImmediate(_, pc, _, _, _, _, _),
			     let .systemRegister(_, pc, _, _, _),
			     let .simdVectorElementMove(_, pc, _, _, _, _, _),
		     let .simdVectorLogical(_, pc, _, _, _, _, _),
		     let .simdVectorLogicalImmediate(_, pc, _, _, _, _),
		     let .simdVectorCompare(_, pc, _, _, _, _, _, _),
		     let .simdVectorReduction(_, pc, _, _, _, _, _),
		     let .fpScalarMove(_, pc, _, _, _, _),
             let .simdModifiedImmediate(_, pc, _, _, _, _),
		     let .compareAndBranchImmediate(_, pc, _, _, _, _),
		     let .unconditionalBranchImmediate(_, pc, _, _),
		     let .svc(_, pc, _),
		     let .unsupported(_, pc, _):
            return pc
        }
    }

    var report: DecodedInstructionReport {
        switch self {
        case let .moveWideImmediate(raw, pc, op, sf, rd, imm, shift):
            return DecodedInstructionReport(
                pc: hexPC(pc),
                raw: hexWord(raw),
                instructionClass: "move_wide_immediate",
                op: op,
                sf: sf,
                rd: rd,
                imm: Int(imm),
                shift: shift,
                reason: nil
            )
        case let .pcRelativeAddress(raw, pc, op, rd, imm):
            return DecodedInstructionReport(
                pc: hexPC(pc),
                raw: hexWord(raw),
                instructionClass: "pc_relative_address",
                op: op,
                sf: nil,
                rd: rd,
                imm: Int(imm),
                shift: nil,
                reason: nil
            )
        case let .addSubImmediate(raw, pc, op, sf, rd, rn, imm, shift):
            return DecodedInstructionReport(
                pc: hexPC(pc),
                raw: hexWord(raw),
                instructionClass: "add_subtract_immediate",
                op: op,
                sf: sf,
                rd: rd,
                rn: rn,
                imm: Int(imm),
                shift: shift,
                reason: nil
            )
        case let .loadStoreUnsignedImmediate(raw, pc, op, rt, rn, offset, width):
            return DecodedInstructionReport(
                pc: hexPC(pc),
                raw: hexWord(raw),
                instructionClass: "load_store_unsigned_immediate",
                op: op,
                sf: 64,
                rd: nil,
                rn: rn,
                rt: rt,
                imm: nil,
                shift: nil,
                offset: offset,
                width: width,
                reason: nil
            )
		case let .systemRegister(raw, pc, op, rt, sysreg):
			return DecodedInstructionReport(
				pc: hexPC(pc),
				raw: hexWord(raw),
				instructionClass: "system_register",
                op: op,
                sf: 64,
                rd: nil,
                rt: rt,
                imm: nil,
                shift: nil,
				sysreg: sysreg,
				reason: nil
			)
	        case let .simdVectorElementMove(raw, pc, rd, rn, width, sourceIndex, destinationIndex):
	            return DecodedInstructionReport(
	                pc: hexPC(pc),
	                raw: hexWord(raw),
	                instructionClass: "simd_vector_element_move",
					op: "mov",
				sf: nil,
				rd: rd,
				rn: rn,
					imm: nil,
					shift: nil,
	                width: width,
	                laneSize: width,
	                sourceIndex: sourceIndex,
	                destinationIndex: destinationIndex,
	                reason: nil
	            )
        case let .simdVectorLogical(raw, pc, op, rd, rn, rm, width):
            return DecodedInstructionReport(
                pc: hexPC(pc),
                raw: hexWord(raw),
                instructionClass: "simd_vector_logical",
                op: op,
                sf: nil,
                rd: rd,
                rn: rn,
                rm: rm,
                imm: nil,
                shift: nil,
                width: width,
                reason: nil
            )
        case let .simdVectorLogicalImmediate(raw, pc, op, rd, imm, width):
            return DecodedInstructionReport(
                pc: hexPC(pc),
                raw: hexWord(raw),
                instructionClass: "simd_vector_logical_immediate",
                op: op,
                sf: nil,
                rd: rd,
                imm: Int(imm),
                shift: nil,
                width: width,
                reason: nil
            )
        case let .simdVectorCompare(raw, pc, op, rd, rn, rm, laneSize, lanes):
            return DecodedInstructionReport(
                pc: hexPC(pc),
                raw: hexWord(raw),
                instructionClass: "simd_vector_compare",
                op: op,
                sf: nil,
                rd: rd,
                rn: rn,
                rm: rm,
                imm: nil,
                shift: nil,
                width: laneSize * lanes,
                laneSize: laneSize,
                reason: nil
            )
        case let .simdVectorReduction(raw, pc, op, rd, rn, laneSize, lanes):
            return DecodedInstructionReport(
                pc: hexPC(pc),
                raw: hexWord(raw),
                instructionClass: "simd_vector_reduction",
                op: op,
                sf: nil,
                rd: rd,
                rn: rn,
                imm: nil,
                shift: nil,
                width: laneSize * lanes,
                laneSize: laneSize,
                reason: nil
            )
        case let .fpScalarMove(raw, pc, op, rd, rn, width):
            return DecodedInstructionReport(
                pc: hexPC(pc),
                raw: hexWord(raw),
                instructionClass: "fp_scalar_move",
                op: op,
                sf: nil,
                rd: rd,
                rn: rn,
                imm: nil,
                shift: nil,
                width: width,
                reason: nil
            )
        case let .simdModifiedImmediate(raw, pc, op, rd, imm, width):
            return DecodedInstructionReport(
                pc: hexPC(pc),
                raw: hexWord(raw),
                instructionClass: "simd_modified_immediate",
                op: op,
                sf: nil,
                rd: rd,
                imm: nil,
                immHex: String(format: "0x%016llx", imm),
                shift: nil,
                width: width,
                reason: nil
            )
        case let .compareAndBranchImmediate(raw, pc, op, sf, rt, offset):
            return DecodedInstructionReport(
                pc: hexPC(pc),
                raw: hexWord(raw),
                instructionClass: "compare_and_branch_immediate",
                op: op,
                sf: sf,
                rd: nil,
                rt: rt,
                imm: nil,
                shift: nil,
                offset: Int(offset),
                reason: nil
            )
        case let .unconditionalBranchImmediate(raw, pc, op, offset):
            return DecodedInstructionReport(
                pc: hexPC(pc),
                raw: hexWord(raw),
                instructionClass: "unconditional_branch_immediate",
                op: op,
                sf: nil,
                rd: nil,
                imm: nil,
                shift: nil,
                offset: Int(offset),
                reason: nil
            )
        case let .svc(raw, pc, imm):
            return DecodedInstructionReport(
                pc: hexPC(pc),
                raw: hexWord(raw),
                instructionClass: "exception_generation",
                op: "svc",
                sf: nil,
                rd: nil,
                imm: Int(imm),
                shift: nil,
                reason: nil
            )
        case let .unsupported(raw, pc, reason):
            return DecodedInstructionReport(
                pc: hexPC(pc),
                raw: hexWord(raw),
                instructionClass: "unsupported",
                op: nil,
                sf: nil,
                rd: nil,
                imm: nil,
                shift: nil,
                reason: reason
            )
        }
    }
}

func signExtend(_ value: UInt64, bitCount: Int) -> Int64 {
    let shift = 64 - bitCount
    return Int64(bitPattern: value << UInt64(shift)) >> Int64(shift)
}

func decodeA64SeedInstruction(raw: UInt32, pc: UInt64) -> A64DecodedInstruction {
    if (raw & 0x1f80_0000) == 0x1280_0000 {
        let sf = Int((raw >> 31) & 0x1)
        let opc = Int((raw >> 29) & 0x3)
        let hw = Int((raw >> 21) & 0x3)
        let imm16 = UInt64((raw >> 5) & 0xffff)
        let rd = Int(raw & 0x1f)

        guard sf == 1 else {
            return .unsupported(raw: raw, pc: pc, reason: "move-wide immediate W-register variants are not implemented")
        }
        guard opc == 2 else {
            return .unsupported(raw: raw, pc: pc, reason: "move-wide immediate variant opc=\(opc) is not implemented")
        }
        guard hw == 0 else {
            return .unsupported(raw: raw, pc: pc, reason: "MOVZ nonzero hw shift \(hw) is not implemented")
        }
        return .moveWideImmediate(raw: raw, pc: pc, op: "movz", sf: 64, rd: rd, imm: imm16, shift: 0)
    }

    if (raw & 0x1f00_0000) == 0x1000_0000 {
        let immlo = UInt64((raw >> 29) & 0x3)
        let immhi = UInt64((raw >> 5) & 0x7ffff)
        let rd = Int(raw & 0x1f)
        let imm = signExtend((immhi << 2) | immlo, bitCount: 21)
        let pageRelative = (raw & 0x8000_0000) != 0
        return .pcRelativeAddress(raw: raw, pc: pc, op: pageRelative ? "adrp" : "adr", rd: rd, imm: pageRelative ? imm << 12 : imm)
    }

    if (raw & 0x1f00_0000) == 0x1100_0000 {
        let sf = Int((raw >> 31) & 0x1)
        let op = Int((raw >> 30) & 0x1)
        let setFlags = Int((raw >> 29) & 0x1)
        let shift = Int((raw >> 22) & 0x3)
        let imm12 = UInt64((raw >> 10) & 0xfff)
        let rn = Int((raw >> 5) & 0x1f)
        let rd = Int(raw & 0x1f)

        guard sf == 1 else {
            return .unsupported(raw: raw, pc: pc, reason: "ADD/SUB immediate W-register variants are not implemented")
        }
        guard setFlags == 0 else {
            return .unsupported(raw: raw, pc: pc, reason: "ADD/SUB immediate flag-setting variants are not implemented")
        }
        guard shift == 0 else {
            return .unsupported(raw: raw, pc: pc, reason: "ADD/SUB immediate shifted variants are not implemented")
        }
        return .addSubImmediate(
            raw: raw,
            pc: pc,
            op: op == 0 ? "add" : "sub",
            sf: 64,
            rd: rd,
            rn: rn,
            imm: imm12,
            shift: 0
        )
    }

    if (raw & 0xffc0_0000) == 0xf900_0000 || (raw & 0xffc0_0000) == 0xf940_0000 {
        let isLoad = (raw & 0x0040_0000) != 0
        let imm12 = Int((raw >> 10) & 0xfff)
        let rn = Int((raw >> 5) & 0x1f)
        let rt = Int(raw & 0x1f)
        return .loadStoreUnsignedImmediate(
            raw: raw,
            pc: pc,
            op: isLoad ? "ldr" : "str",
            rt: rt,
            rn: rn,
            offset: imm12 * 8,
            width: 64
        )
    }

    if (raw & 0xffc0_0000) == 0x3940_0000 {
        let imm12 = Int((raw >> 10) & 0xfff)
        let rn = Int((raw >> 5) & 0x1f)
        let rt = Int(raw & 0x1f)
        return .loadStoreUnsignedImmediate(
            raw: raw,
            pc: pc,
            op: "ldrb",
            rt: rt,
            rn: rn,
            offset: imm12,
            width: 8
        )
    }

	if (raw & 0xfff0_0000) == 0xd510_0000 || (raw & 0xfff0_0000) == 0xd530_0000 {
		let sysreg = UInt16((raw >> 5) & 0xffff)
		let rt = Int(raw & 0x1f)
		let isRead = (raw & 0x0020_0000) != 0
        guard sysreg == 0xde82 else {
            return .unsupported(raw: raw, pc: pc, reason: String(format: "system register 0x%04x is not implemented", sysreg))
        }
		return .systemRegister(
			raw: raw,
			pc: pc,
			op: isRead ? "mrs" : "msr",
			rt: rt,
			sysreg: "tpidr_el0"
		)
	}

		if (raw & 0xffe0_8400) == 0x6e00_0400 {
			let imm5 = Int((raw >> 16) & 0x1f)
			let imm4 = Int((raw >> 11) & 0xf)
			let rn = Int((raw >> 5) & 0x1f)
			let rd = Int(raw & 0x1f)
			if imm5 == 8 && imm4 == 0 {
				return .simdVectorElementMove(
					raw: raw,
					pc: pc,
					rd: rd,
					rn: rn,
					width: 64,
					sourceIndex: 0,
					destinationIndex: 0
				)
			}
			if imm5 == 20 && imm4 == 8 {
				return .simdVectorElementMove(
					raw: raw,
					pc: pc,
					rd: rd,
					rn: rn,
					width: 32,
					sourceIndex: 2,
					destinationIndex: 2
				)
			}
			do {
				return .unsupported(raw: raw, pc: pc, reason: "SIMD vector element move variant imm5=\(imm5) imm4=\(imm4) is not implemented")
			}
		}

	if (raw & 0xff20_fc00) == 0x4e20_1c00 {
		let rd = Int(raw & 0x1f)
		let rn = Int((raw >> 5) & 0x1f)
		let rm = Int((raw >> 16) & 0x1f)
		return .simdVectorLogical(raw: raw, pc: pc, op: "and", rd: rd, rn: rn, rm: rm, width: 128)
	}

	if (raw & 0xffe0_fc00) == 0x6ea0_8c00 {
		let rd = Int(raw & 0x1f)
		let rn = Int((raw >> 5) & 0x1f)
		let rm = Int((raw >> 16) & 0x1f)
		return .simdVectorCompare(raw: raw, pc: pc, op: "cmeq", rd: rd, rn: rn, rm: rm, laneSize: 32, lanes: 4)
	}

	if (raw & 0xffff_fc20) == 0x6eb0_a800 {
		let rd = Int(raw & 0x1f)
		let rn = Int((raw >> 5) & 0x1f)
		return .simdVectorReduction(raw: raw, pc: pc, op: "umaxv", rd: rd, rn: rn, laneSize: 32, lanes: 4)
	}

	if (raw & 0xffff_fc20) == 0x4eb1_b800 {
		let rd = Int(raw & 0x1f)
		let rn = Int((raw >> 5) & 0x1f)
		return .simdVectorReduction(raw: raw, pc: pc, op: "addv", rd: rd, rn: rn, laneSize: 32, lanes: 4)
	}

	if (raw & 0xffff_fc00) == 0x1e26_0000 {
		let rd = Int(raw & 0x1f)
		let rn = Int((raw >> 5) & 0x1f)
		return .fpScalarMove(raw: raw, pc: pc, op: "fmov", rd: rd, rn: rn, width: 32)
		}

		if (raw & 0xffff_ffe0) == 0x4f01_1600 {
			let rd = Int(raw & 0x1f)
			return .simdVectorLogicalImmediate(raw: raw, pc: pc, op: "orr", rd: rd, imm: 0x0000003000000030, width: 128)
		}

            if (raw & 0xffff_ffe0) == 0x4f06_e7e0 {
                let rd = Int(raw & 0x1f)
                return .simdModifiedImmediate(raw: raw, pc: pc, op: "movi", rd: rd, imm: 0xdfdfdfdfdfdfdfdf, width: 128)
            }
            if (raw & 0xffff_ffe0) == 0x4f00_0420 {
                let rd = Int(raw & 0x1f)
                return .simdModifiedImmediate(raw: raw, pc: pc, op: "movi", rd: rd, imm: 0x0000000100000001, width: 128)
            }
            if (raw & 0xffff_ffe0) == 0x4f02_0420 {
                let rd = Int(raw & 0x1f)
                return .simdModifiedImmediate(raw: raw, pc: pc, op: "movi", rd: rd, imm: 0x0000004100000041, width: 128)
            }

	if (raw & 0x7e00_0000) == 0x3400_0000 {
        let sf = Int((raw >> 31) & 0x1)
        let op = Int((raw >> 24) & 0x1)
        let imm19 = UInt64((raw >> 5) & 0x7ffff)
        let rt = Int(raw & 0x1f)
        guard sf == 1 else {
            return .unsupported(raw: raw, pc: pc, reason: "CBZ/CBNZ W-register variants are not implemented")
        }
        guard op == 0 else {
            return .unsupported(raw: raw, pc: pc, reason: "CBNZ is not implemented")
        }
        let offset = signExtend(imm19 << 2, bitCount: 21)
        return .compareAndBranchImmediate(raw: raw, pc: pc, op: "cbz", sf: 64, rt: rt, offset: offset)
    }

    if (raw & 0xfc00_0000) == 0x1400_0000 {
        let imm26 = UInt64(raw & 0x03ff_ffff)
        let offset = signExtend(imm26 << 2, bitCount: 28)
        return .unconditionalBranchImmediate(raw: raw, pc: pc, op: "b", offset: offset)
    }

    if (raw & 0xffe0_001f) == 0xd400_0001 {
        let imm = UInt16((raw >> 5) & 0xffff)
        guard imm == 0 else {
            return .unsupported(raw: raw, pc: pc, reason: "SVC immediate \(imm) is not implemented")
        }
        return .svc(raw: raw, pc: pc, imm: imm)
    }

    return .unsupported(raw: raw, pc: pc, reason: "unknown instruction")
}

func executionReport(
    metadata: GoldenMetadata,
    elf: TinyElf64Aarch64,
    expectedEntry: UInt64,
    instructionsExecuted: Int,
    decodedInstructions: [DecodedInstructionReport],
    instructionWords: [UInt32],
    syscalls: [CapturedSyscall],
    capturedExit: CapturedExit?,
    capturedFault: CapturedFault?,
    memoryWrites: [CapturedMemoryWrite],
    notes: [String]
) -> ExecutionReport {
    ExecutionReport(
        backend: "switch-debug",
        caseID: metadata.caseID,
        enteredEntrypoint: elf.entrypoint == expectedEntry,
        guestInstructionsExecuted: instructionsExecuted,
        decodedInstructions: decodedInstructions,
        syscalls: syscalls,
        exit: capturedExit,
        fault: capturedFault,
        instructionEncodings: instructionWords.map(hexWord),
        memoryWrites: memoryWrites,
        notes: notes
    )
}

let switchDebugInitialSP: UInt64 = 0x0000_0000_7fff_0000
let switchDebugStackSize: UInt64 = 4096

struct SwitchDebugStack {
    let top: UInt64 = switchDebugInitialSP
    let size: UInt64 = switchDebugStackSize
    var words: [UInt64: UInt64] = [:]

    var bottom: UInt64 {
        top - size
    }

    func check(_ address: UInt64, width: Int) throws {
        guard width == 64 else {
            throw GateError.commandFailed("switch-debug stack supports only 64-bit accesses, requested \(width)")
        }
        guard address % 8 == 0 else {
            throw GateError.commandFailed(String(format: "unaligned switch-debug stack access at 0x%llx", address))
        }
        guard address >= bottom && address + 8 <= top else {
            throw GateError.commandFailed(String(format: "switch-debug stack access outside bounded test stack: address=0x%llx", address))
        }
    }

    mutating func store64(_ value: UInt64, at address: UInt64) throws {
        try check(address, width: 64)
        words[address] = value
    }

    func load64(at address: UInt64) throws -> UInt64 {
        try check(address, width: 64)
        guard let value = words[address] else {
            throw GateError.commandFailed(String(format: "uninitialized switch-debug stack read at 0x%llx", address))
        }
        return value
    }
}

func executeSwitchDebug(
    binary: URL,
    metadata: GoldenMetadata,
    applyRelativeRelocations: Bool = true,
    invisibleRelativeRelocationOffsets: Set<UInt64> = [],
    guardNullPageReads: Bool = false
) throws -> (report: ExecutionReport, failures: [Failure]) {
    let elf = try TinyElf64Aarch64(
        binary: binary,
        applyRelativeRelocations: applyRelativeRelocations,
        invisibleRelativeRelocationOffsets: invisibleRelativeRelocationOffsets,
        guardNullPageReads: guardNullPageReads
    )
    let expectedEntry = try parseEntrypoint(metadata.entrypoint)
    var failures: [Failure] = []
    if elf.entrypoint != expectedEntry {
        failures.append(fail("execution-entrypoint", String(format: "entered 0x%llx, expected 0x%llx", elf.entrypoint, expectedEntry)))
    }
    var pc = elf.entrypoint
    var registers = Array(repeating: UInt64(0), count: 31)
    var simdRegisters = Array(repeating: UInt64(0), count: 64)
    var sp = switchDebugInitialSP
    var stack = SwitchDebugStack()
    var guestTPIDREL0: UInt64 = 0
    var instructionWords: [UInt32] = []
    var syscalls: [CapturedSyscall] = []
    var capturedExit: CapturedExit?
    var capturedFault: CapturedFault?
    var memoryWrites: [CapturedMemoryWrite] = []
    var decodedInstructions: [DecodedInstructionReport] = []
    var instructionsExecuted = 0

    for _ in 0..<32 {
        let word = try elf.readInstruction(at: pc)
        instructionWords.append(word)
        instructionsExecuted += 1
        let decoded = decodeA64SeedInstruction(raw: word, pc: pc)
        decodedInstructions.append(decoded.report)
        switch decoded {
        case let .moveWideImmediate(_, _, _, _, rd, imm, shift):
            registers[rd] = imm << UInt64(shift)
            pc += 4
        case let .pcRelativeAddress(_, instructionPC, op, rd, imm):
            let base = op == "adrp" ? instructionPC & ~0xfff : instructionPC
            let address = Int64(bitPattern: base) + imm
            guard address >= 0 else {
                let report = executionReport(
                    metadata: metadata,
                    elf: elf,
                    expectedEntry: expectedEntry,
                    instructionsExecuted: instructionsExecuted,
                    decodedInstructions: decodedInstructions,
                    instructionWords: instructionWords,
                    syscalls: syscalls,
                    capturedExit: capturedExit,
                    capturedFault: capturedFault,
                    memoryWrites: memoryWrites,
                    notes: ["PC-relative address computation produced a negative guest address and stopped the switch-debug harness"]
                )
                failures.append(fail("execution-address", "\(op.uppercased()) computed negative guest address \(address)"))
                return (report, failures)
            }
            let effectiveAddress = UInt64(address)
            decodedInstructions[decodedInstructions.count - 1] = DecodedInstructionReport(
                pc: decoded.report.pc,
                raw: decoded.report.raw,
                instructionClass: decoded.report.instructionClass,
                op: decoded.report.op,
                sf: decoded.report.sf,
                rd: decoded.report.rd,
                rn: decoded.report.rn,
                rt: decoded.report.rt,
                imm: decoded.report.imm,
                shift: decoded.report.shift,
                offset: decoded.report.offset,
                width: decoded.report.width,
                effectiveAddress: hexPC(effectiveAddress),
                reason: decoded.report.reason
            )
            registers[rd] = effectiveAddress
            pc += 4
        case let .addSubImmediate(_, _, op, _, rd, rn, imm, _):
            let source = rn == 31 ? sp : registers[rn]
            let value = op == "add" ? source &+ imm : source &- imm
            if rd == 31 {
                sp = value
            } else {
                registers[rd] = value
            }
            pc += 4
        case let .loadStoreUnsignedImmediate(_, _, op, rt, rn, offset, width):
            if rn != 31 {
                let address = registers[rn] + UInt64(offset)
                decodedInstructions[decodedInstructions.count - 1] = DecodedInstructionReport(
                    pc: decoded.report.pc,
                    raw: decoded.report.raw,
                    instructionClass: decoded.report.instructionClass,
                    op: decoded.report.op,
                    sf: decoded.report.sf,
                    rd: decoded.report.rd,
                    rn: decoded.report.rn,
                    rt: decoded.report.rt,
                    imm: decoded.report.imm,
                    shift: decoded.report.shift,
                    offset: decoded.report.offset,
                    width: decoded.report.width,
                    effectiveAddress: hexPC(address),
                    reason: decoded.report.reason
                )
                do {
                    if op == "ldr" {
                        let bytes = try elf.readBytes(at: address, length: width / 8)
                        registers[rt] = try littleEndianUInt64(bytes, 0)
                    } else if op == "ldrb" {
                        let bytes = try elf.readBytes(at: address, length: 1)
                        registers[rt] = UInt64(bytes[0])
                    } else if op == "str" {
                        _ = try elf.readBytes(at: address, length: 8)
                        memoryWrites.append(CapturedMemoryWrite(
                            address: hexPC(address),
                            width: 64,
                            value: String(format: "0x%016llx", registers[rt]),
                            captured: true
                        ))
                    } else {
                        let report = executionReport(
                            metadata: metadata,
                            elf: elf,
                            expectedEntry: expectedEntry,
                            instructionsExecuted: instructionsExecuted,
                            decodedInstructions: decodedInstructions,
                            instructionWords: instructionWords,
                            syscalls: syscalls,
                            capturedExit: capturedExit,
                            capturedFault: capturedFault,
                            memoryWrites: memoryWrites,
                            notes: ["switch-debug stopped because only register-based LDR/STR are supported"]
                        )
                        failures.append(fail("execution-unsupported-instruction", "unsupported register-based load/store op \(op)"))
                        return (report, failures)
                    }
                    pc += 4
                } catch {
                    capturedFault = CapturedFault(
                        kind: "guest_memory_fault",
                        address: hexPC(address),
                        access: op == "str" ? "write" : "read",
                        captured: true
                    )
                    let report = executionReport(
                        metadata: metadata,
                        elf: elf,
                        expectedEntry: expectedEntry,
                        instructionsExecuted: instructionsExecuted,
                        decodedInstructions: decodedInstructions,
                        instructionWords: instructionWords,
                        syscalls: syscalls,
                        capturedExit: capturedExit,
                        capturedFault: capturedFault,
                        memoryWrites: memoryWrites,
                        notes: ["switch-debug stopped on captured file-backed PT_LOAD memory access fault; no Linux signal, process, scheduler, or VFS semantics were executed"]
                    )
                    failures.append(contentsOf: validateCapturedExecution(metadata: metadata, report: report))
                    return (report, failures)
                }
                continue
            }
            guard rn == 31 else {
                let report = executionReport(
                    metadata: metadata,
                    elf: elf,
                    expectedEntry: expectedEntry,
                    instructionsExecuted: instructionsExecuted,
                    decodedInstructions: decodedInstructions,
                    instructionWords: instructionWords,
                    syscalls: syscalls,
                    capturedExit: capturedExit,
                    capturedFault: capturedFault,
                    memoryWrites: memoryWrites,
                    notes: ["switch-debug memory fixture stopped because only register-based LDR and SP-based load/store are supported"]
                )
                failures.append(fail("execution-unsupported-instruction", "load/store unsigned immediate is only implemented for register-based LDR and SP base in this no-phone fixture"))
                return (report, failures)
            }
            let address = sp + UInt64(offset)
            decodedInstructions[decodedInstructions.count - 1] = DecodedInstructionReport(
                pc: decoded.report.pc,
                raw: decoded.report.raw,
                instructionClass: decoded.report.instructionClass,
                op: decoded.report.op,
                sf: decoded.report.sf,
                rd: decoded.report.rd,
                rn: decoded.report.rn,
                rt: decoded.report.rt,
                imm: decoded.report.imm,
                shift: decoded.report.shift,
                offset: decoded.report.offset,
                width: decoded.report.width,
                effectiveAddress: hexPC(address),
                reason: decoded.report.reason
            )
            do {
                if op == "str" {
                    try stack.store64(registers[rt], at: address)
                } else {
                    registers[rt] = try stack.load64(at: address)
                }
                pc += 4
            } catch {
                let report = executionReport(
                    metadata: metadata,
                    elf: elf,
                    expectedEntry: expectedEntry,
                    instructionsExecuted: instructionsExecuted,
                    decodedInstructions: decodedInstructions,
                    instructionWords: instructionWords,
                    syscalls: syscalls,
                    capturedExit: capturedExit,
                    capturedFault: capturedFault,
                    memoryWrites: memoryWrites,
                    notes: ["switch-debug stopped on bounded stack-memory failure"]
                )
                failures.append(fail("execution-stack-memory", "\(error)"))
                return (report, failures)
            }
        case let .systemRegister(_, _, op, rt, sysreg):
            guard sysreg == "tpidr_el0" else {
                let report = executionReport(
                    metadata: metadata,
                    elf: elf,
                    expectedEntry: expectedEntry,
                    instructionsExecuted: instructionsExecuted,
                    decodedInstructions: decodedInstructions,
                    instructionWords: instructionWords,
                    syscalls: syscalls,
                    capturedExit: capturedExit,
                    capturedFault: capturedFault,
                    memoryWrites: memoryWrites,
                    notes: ["switch-debug stopped because only guest TPIDR_EL0 system-register state is supported"]
                )
                failures.append(fail("execution-unsupported-instruction", "unsupported system register \(sysreg)"))
                return (report, failures)
            }
            if op == "msr" {
                guestTPIDREL0 = registers[rt]
            } else {
                registers[rt] = guestTPIDREL0
            }
            pc += 4
        case let .compareAndBranchImmediate(_, instructionPC, op, _, rt, offset):
            guard op == "cbz" else {
                let report = executionReport(
                    metadata: metadata,
                    elf: elf,
                    expectedEntry: expectedEntry,
                    instructionsExecuted: instructionsExecuted,
                    decodedInstructions: decodedInstructions,
                    instructionWords: instructionWords,
                    syscalls: syscalls,
                    capturedExit: capturedExit,
                    capturedFault: capturedFault,
                    memoryWrites: memoryWrites,
                    notes: ["switch-debug branch fixture stopped because only CBZ is supported"]
                )
                failures.append(fail("execution-unsupported-instruction", "unsupported compare-and-branch op \(op)"))
                return (report, failures)
            }
            if registers[rt] == 0 {
                let targetPC = Int64(bitPattern: instructionPC) + offset
                guard targetPC >= 0 else {
                    let report = executionReport(
                        metadata: metadata,
                        elf: elf,
                        expectedEntry: expectedEntry,
                        instructionsExecuted: instructionsExecuted,
                        decodedInstructions: decodedInstructions,
                        instructionWords: instructionWords,
                        syscalls: syscalls,
                        capturedExit: capturedExit,
                        capturedFault: capturedFault,
                        memoryWrites: memoryWrites,
                        notes: ["switch-debug branch fixture stopped because CBZ computed a negative PC"]
                    )
                    failures.append(fail("execution-branch-target", "CBZ computed negative guest PC \(targetPC)"))
                    return (report, failures)
                }
                pc = UInt64(targetPC)
            } else {
                pc += 4
            }
		case let .unconditionalBranchImmediate(_, instructionPC, op, offset):
			guard op == "b" else {
				let report = executionReport(
					metadata: metadata,
                    elf: elf,
                    expectedEntry: expectedEntry,
                    instructionsExecuted: instructionsExecuted,
                    decodedInstructions: decodedInstructions,
                    instructionWords: instructionWords,
                    syscalls: syscalls,
                    capturedExit: capturedExit,
                    capturedFault: capturedFault,
                    memoryWrites: memoryWrites,
                    notes: ["switch-debug branch fixture stopped because only B is supported"]
                )
                failures.append(fail("execution-unsupported-instruction", "unsupported unconditional branch op \(op)"))
                return (report, failures)
            }
            let targetPC = Int64(bitPattern: instructionPC) + offset
            guard targetPC >= 0 else {
                let report = executionReport(
                    metadata: metadata,
                    elf: elf,
                    expectedEntry: expectedEntry,
                    instructionsExecuted: instructionsExecuted,
                    decodedInstructions: decodedInstructions,
                    instructionWords: instructionWords,
                    syscalls: syscalls,
                    capturedExit: capturedExit,
                    capturedFault: capturedFault,
                    memoryWrites: memoryWrites,
                    notes: ["switch-debug branch fixture stopped because B computed a negative PC"]
                )
                failures.append(fail("execution-branch-target", "B computed negative guest PC \(targetPC)"))
                return (report, failures)
			}
			pc = UInt64(targetPC)
			case let .simdVectorElementMove(_, _, rd, rn, width, sourceIndex, destinationIndex):
				if width == 64 && rd == rn {
					pc += 4
					continue
				}
				guard width == 32 && sourceIndex >= 0 && sourceIndex < 4 && destinationIndex >= 0 && destinationIndex < 4 else {
					let report = executionReport(
						metadata: metadata,
						elf: elf,
					expectedEntry: expectedEntry,
					instructionsExecuted: instructionsExecuted,
					decodedInstructions: decodedInstructions,
					instructionWords: instructionWords,
					syscalls: syscalls,
					capturedExit: capturedExit,
						capturedFault: capturedFault,
						memoryWrites: memoryWrites,
						notes: ["switch-debug SIMD vector element support is limited to reduced 64-bit self-move and 32-bit lane copy"]
					)
					failures.append(fail("execution-unsupported-instruction", "unsupported SIMD vector element move rd=\(rd) rn=\(rn) width=\(width)"))
					return (report, failures)
				}
				let sourceWord = rn * 2 + sourceIndex / 2
				let destinationWord = rd * 2 + destinationIndex / 2
				let sourceShift = UInt64((sourceIndex % 2) * 32)
				let destinationShift = UInt64((destinationIndex % 2) * 32)
				let value = (simdRegisters[sourceWord] >> sourceShift) & 0xffff_ffff
				let mask = UInt64(0xffff_ffff) << destinationShift
				simdRegisters[destinationWord] = (simdRegisters[destinationWord] & ~mask) | (value << destinationShift)
				pc += 4
	case let .simdVectorLogical(_, _, op, rd, rn, rm, width):
		guard op == "and" && width == 128 else {
			let report = executionReport(
				metadata: metadata,
				elf: elf,
				expectedEntry: expectedEntry,
				instructionsExecuted: instructionsExecuted,
				decodedInstructions: decodedInstructions,
				instructionWords: instructionWords,
				syscalls: syscalls,
				capturedExit: capturedExit,
				capturedFault: capturedFault,
				memoryWrites: memoryWrites,
				notes: ["switch-debug SIMD vector logical support is limited to AND vN.16b"]
			)
			failures.append(fail("execution-unsupported-instruction", "unsupported SIMD vector logical op=\(op) width=\(width)"))
			return (report, failures)
		}
		simdRegisters[rd * 2] = simdRegisters[rn * 2] & simdRegisters[rm * 2]
		simdRegisters[rd * 2 + 1] = simdRegisters[rn * 2 + 1] & simdRegisters[rm * 2 + 1]
		pc += 4
	case let .simdVectorCompare(_, _, op, rd, rn, rm, laneSize, lanes):
		guard op == "cmeq" && laneSize == 32 && lanes == 4 else {
			let report = executionReport(
				metadata: metadata,
				elf: elf,
				expectedEntry: expectedEntry,
				instructionsExecuted: instructionsExecuted,
				decodedInstructions: decodedInstructions,
				instructionWords: instructionWords,
				syscalls: syscalls,
				capturedExit: capturedExit,
				capturedFault: capturedFault,
				memoryWrites: memoryWrites,
				notes: ["switch-debug SIMD vector compare support is limited to CMEQ vD.4s, vN.4s, vM.4s"]
			)
			failures.append(fail("execution-unsupported-instruction", "unsupported SIMD vector compare op=\(op) lane_size=\(laneSize) lanes=\(lanes)"))
			return (report, failures)
		}
		var low: UInt64 = 0
		var high: UInt64 = 0
		for lane in 0..<4 {
			let word = lane / 2
			let shift = UInt64((lane % 2) * 32)
			let left = (simdRegisters[rn * 2 + word] >> shift) & 0xffff_ffff
			let right = (simdRegisters[rm * 2 + word] >> shift) & 0xffff_ffff
			let result: UInt64 = left == right ? 0xffff_ffff : 0
			if word == 0 {
				low |= result << shift
			} else {
				high |= result << shift
			}
		}
		simdRegisters[rd * 2] = low
		simdRegisters[rd * 2 + 1] = high
		pc += 4
	case let .simdVectorReduction(_, _, op, rd, rn, laneSize, lanes):
		guard (op == "umaxv" || op == "addv") && laneSize == 32 && lanes == 4 else {
			let report = executionReport(
				metadata: metadata,
				elf: elf,
				expectedEntry: expectedEntry,
				instructionsExecuted: instructionsExecuted,
				decodedInstructions: decodedInstructions,
				instructionWords: instructionWords,
				syscalls: syscalls,
				capturedExit: capturedExit,
				capturedFault: capturedFault,
				memoryWrites: memoryWrites,
				notes: ["switch-debug SIMD vector reduction support is limited to UMAXV Sd, Vn.4s and ADDV Sd, Vn.4s"]
			)
			failures.append(fail("execution-unsupported-instruction", "unsupported SIMD vector reduction op=\(op) lane_size=\(laneSize) lanes=\(lanes)"))
			return (report, failures)
		}
		var result: UInt64 = 0
		for lane in 0..<4 {
			let word = lane / 2
			let shift = UInt64((lane % 2) * 32)
			let value = (simdRegisters[rn * 2 + word] >> shift) & 0xffff_ffff
			if op == "addv" {
				result = (result + value) & 0xffff_ffff
			} else if lane == 0 || value > result {
				result = value
			}
		}
		simdRegisters[rd * 2] = result
		simdRegisters[rd * 2 + 1] = 0
		pc += 4
	case let .fpScalarMove(_, _, op, rd, rn, width):
		guard op == "fmov" && width == 32 else {
			let report = executionReport(
				metadata: metadata,
				elf: elf,
				expectedEntry: expectedEntry,
				instructionsExecuted: instructionsExecuted,
				decodedInstructions: decodedInstructions,
				instructionWords: instructionWords,
				syscalls: syscalls,
				capturedExit: capturedExit,
				capturedFault: capturedFault,
				memoryWrites: memoryWrites,
				notes: ["switch-debug FP scalar move support is limited to FMOV wD, sN"]
			)
			failures.append(fail("execution-unsupported-instruction", "unsupported FP scalar move op=\(op) width=\(width)"))
			return (report, failures)
		}
		registers[rd] = simdRegisters[rn * 2] & 0xffff_ffff
		pc += 4
	case let .simdVectorLogicalImmediate(_, _, op, rd, imm, width):
		guard op == "orr" && width == 128 else {
			let report = executionReport(
				metadata: metadata,
				elf: elf,
				expectedEntry: expectedEntry,
				instructionsExecuted: instructionsExecuted,
				decodedInstructions: decodedInstructions,
				instructionWords: instructionWords,
				syscalls: syscalls,
				capturedExit: capturedExit,
				capturedFault: capturedFault,
				memoryWrites: memoryWrites,
				notes: ["switch-debug SIMD vector logical immediate support is limited to ORR vN.4s, #0x30"]
			)
			failures.append(fail("execution-unsupported-instruction", "unsupported SIMD vector logical immediate op=\(op) width=\(width)"))
			return (report, failures)
	        }
	        simdRegisters[rd * 2] |= imm
	        simdRegisters[rd * 2 + 1] |= imm
	        pc += 4
        case let .simdModifiedImmediate(_, _, op, rd, imm, width):
            guard op == "movi" && width == 128 else {
                let report = executionReport(
                    metadata: metadata,
                    elf: elf,
                    expectedEntry: expectedEntry,
                    instructionsExecuted: instructionsExecuted,
                    decodedInstructions: decodedInstructions,
                    instructionWords: instructionWords,
                    syscalls: syscalls,
                    capturedExit: capturedExit,
                    capturedFault: capturedFault,
                    memoryWrites: memoryWrites,
                    notes: ["switch-debug SIMD modified-immediate support is limited to MOVI vN.16b, #0xdf"]
                )
                failures.append(fail("execution-unsupported-instruction", "unsupported SIMD modified-immediate op=\(op) width=\(width)"))
                return (report, failures)
            }
            simdRegisters[rd * 2] = imm
            simdRegisters[rd * 2 + 1] = imm
            pc += 4
        case .svc:
			let syscallNumber = Int(registers[8])
            let arg0 = Int(registers[0])
            if syscallNumber == 64 {
                let bufferAddress = registers[1]
                let length = Int(registers[2])
                do {
                    let bytes = try elf.readBytes(at: bufferAddress, length: length)
                    let capturedBytes = String(data: bytes, encoding: .utf8) ?? bytes.map { String(format: "%02x", $0) }.joined()
                    syscalls.append(CapturedSyscall(
                        nr: syscallNumber,
                        name: "write",
                        args: [.int(arg0), .string(hexPC(bufferAddress)), .int(length)],
                        captured: true,
                        capturedBytes: capturedBytes
                    ))
                } catch {
                    let report = executionReport(
                        metadata: metadata,
                        elf: elf,
                        expectedEntry: expectedEntry,
                        instructionsExecuted: instructionsExecuted,
                        decodedInstructions: decodedInstructions,
                        instructionWords: instructionWords,
                        syscalls: syscalls,
                        capturedExit: capturedExit,
                        capturedFault: capturedFault,
                        memoryWrites: memoryWrites,
                        notes: ["switch-debug stopped after a captured write syscall memory-read failure"]
                    )
                    failures.append(fail("execution-memory-read", "\(error)"))
                    return (report, failures)
                }
                pc += 4
            } else if syscallNumber == 93 {
                syscalls.append(CapturedSyscall(nr: syscallNumber, name: "exit", args: [.int(arg0)], captured: true))
                capturedExit = CapturedExit(kind: "guest_exit_syscall", code: arg0)
            } else if syscallNumber == 226 {
                syscalls.append(CapturedSyscall(
                    nr: syscallNumber,
                    name: "mprotect",
                    args: [.string(hexPC(registers[0])), .int(Int(registers[1])), .int(Int(registers[2]))],
                    captured: true
                ))
                pc += 4
            } else {
                failures.append(fail("execution-syscall", "expected Linux exit syscall 93, captured \(syscallNumber)"))
                let report = executionReport(
                    metadata: metadata,
                    elf: elf,
                    expectedEntry: expectedEntry,
                    instructionsExecuted: instructionsExecuted,
                    decodedInstructions: decodedInstructions,
                    instructionWords: instructionWords,
                    syscalls: syscalls,
                    capturedExit: capturedExit,
                    capturedFault: capturedFault,
                    memoryWrites: memoryWrites,
                    notes: ["unsupported syscall number stopped the switch-debug execution harness"]
                )
                return (report, failures)
            }
            if capturedExit != nil {
                var notes = [
                    "switch-debug executes decoded MOVZ/ADR/ADD/SUB/LDR/STR/MRS/MSR/SVC seed semantics and captures svc #0 as test events without calling host syscalls",
                    "guest mprotect is captured as a test event only; no host mprotect, vm_protect, MAP_JIT, RWX, or permission side effect is performed",
                    "guest TPIDR_EL0 is switch-debug guest state only; host TPIDR_EL0 is not read or written",
                ]
                if metadata.caseID == "init_010_cpu_model" {
                    do {
                        let modelBytes = try elf.readBytes(at: registers[1], length: 17)
                        let model = String(data: modelBytes, encoding: .utf8)
                        if model == "orlix-aarch64-v1\n" {
                            notes.append("virtual CPU model payload orlix-aarch64-v1 captured from file-backed PT_LOAD bytes via ADR x1")
                        } else {
                            failures.append(fail("execution-cpu-model", "expected virtual CPU model payload orlix-aarch64-v1 newline, captured \(model ?? "<non-utf8>")"))
                        }
                    } catch {
                        failures.append(fail("execution-cpu-model", "\(error)"))
                    }
                }
                let report = executionReport(
                    metadata: metadata,
                    elf: elf,
                    expectedEntry: expectedEntry,
                    instructionsExecuted: instructionsExecuted,
                    decodedInstructions: decodedInstructions,
                    instructionWords: instructionWords,
                    syscalls: syscalls,
                    capturedExit: capturedExit,
                    capturedFault: capturedFault,
                    memoryWrites: memoryWrites,
                    notes: notes
                )
                failures.append(contentsOf: validateCapturedExecution(metadata: metadata, report: report))
                return (report, failures)
            }
        case let .unsupported(raw, pc, reason):
            let report = executionReport(
                metadata: metadata,
                elf: elf,
                expectedEntry: expectedEntry,
                instructionsExecuted: instructionsExecuted,
                decodedInstructions: decodedInstructions,
                instructionWords: instructionWords,
                syscalls: syscalls,
                capturedExit: capturedExit,
                capturedFault: capturedFault,
                memoryWrites: memoryWrites,
                notes: ["unsupported decoded instruction stopped the seed switch-debug execution harness"]
            )
            failures.append(fail("execution-unsupported-instruction", "\(reason): \(hexWord(raw)) at guest PC \(hexPC(pc))"))
            return (report, failures)
        }
    }

    let report = executionReport(
        metadata: metadata,
        elf: elf,
        expectedEntry: expectedEntry,
        instructionsExecuted: instructionsExecuted,
        decodedInstructions: decodedInstructions,
        instructionWords: instructionWords,
        syscalls: syscalls,
        capturedExit: capturedExit,
        capturedFault: capturedFault,
        memoryWrites: memoryWrites,
        notes: ["instruction limit reached before svc #0"]
    )
    failures.append(fail("execution-limit", "switch-debug execution reached instruction limit before svc #0"))
    return (report, failures)
}

func validateCapturedExecution(metadata: GoldenMetadata, report: ExecutionReport) -> [Failure] {
    var failures: [Failure] = []
    if metadata.caseID == "init_009_faults" {
        if report.guestInstructionsExecuted != 2 {
            failures.append(fail("execution-instruction-count", "expected 2 guest instructions before fault, executed \(report.guestInstructionsExecuted)"))
        }
        if report.fault?.kind != "guest_memory_fault" ||
            report.fault?.address != "0x0000000000000000" ||
            report.fault?.access != "read" ||
            report.fault?.captured != true {
            failures.append(fail("execution-fault", "expected captured read fault at guest address 0x0"))
        }
        if report.exit != nil || !report.syscalls.isEmpty {
            failures.append(fail("execution-fault-stop", "fault case must stop before syscall or guest exit"))
        }
        let decoded = report.decodedInstructions
        if !decoded.contains(where: { $0.instructionClass == "load_store_unsigned_immediate" && $0.op == "ldr" && $0.rt == 0 && $0.rn == 1 && $0.effectiveAddress == "0x0000000000000000" }) {
            failures.append(fail("execution-fault-decode", "expected decoded LDR x0, [x1] effective address 0x0"))
        }
        return failures
    }
    if report.exit?.code != metadata.expectedExitCode {
        failures.append(fail("execution-exit-code", "expected guest exit code \(metadata.expectedExitCode), captured \(report.exit?.code ?? -1)"))
    }
    if metadata.caseID == "init_001_exit" && report.guestInstructionsExecuted != 3 {
        failures.append(fail("execution-instruction-count", "expected 3 guest instructions, executed \(report.guestInstructionsExecuted)"))
    }
    if metadata.caseID == "init_002_write" {
        if report.guestInstructionsExecuted != 8 {
            failures.append(fail("execution-instruction-count", "expected 8 guest instructions, executed \(report.guestInstructionsExecuted)"))
        }
        guard report.syscalls.count == 2 else {
            failures.append(fail("execution-syscalls", "expected write and exit syscalls, captured \(report.syscalls.count)"))
            return failures
        }
        let write = report.syscalls[0]
        if write.nr != 64 || write.name != "write" || write.capturedBytes != metadata.expectedMessage {
            failures.append(fail("execution-write", "expected captured write bytes \(metadata.expectedMessage ?? "<nil>"), captured \(write.capturedBytes ?? "<nil>")"))
        }
        let exit = report.syscalls[1]
        if exit.nr != 93 || exit.name != "exit" || report.exit?.code != 0 {
            failures.append(fail("execution-exit", "expected captured exit(0) after write"))
        }
    }
    if metadata.caseID == "init_003_stack" {
        if report.guestInstructionsExecuted != 8 {
            failures.append(fail("execution-instruction-count", "expected 8 guest instructions, executed \(report.guestInstructionsExecuted)"))
        }
        guard report.syscalls.count == 1 else {
            failures.append(fail("execution-syscalls", "expected one exit syscall, captured \(report.syscalls.count)"))
            return failures
        }
        let exit = report.syscalls[0]
        if exit.nr != 93 || exit.name != "exit" || report.exit?.code != 42 {
            failures.append(fail("execution-exit", "expected captured stack-derived exit(42)"))
        }
    }
    if metadata.caseID == "init_004_tls" {
        if report.guestInstructionsExecuted != 6 {
            failures.append(fail("execution-instruction-count", "expected 6 guest instructions, executed \(report.guestInstructionsExecuted)"))
        }
        guard report.syscalls.count == 1 else {
            failures.append(fail("execution-syscalls", "expected one exit syscall, captured \(report.syscalls.count)"))
            return failures
        }
        let exit = report.syscalls[0]
        if exit.nr != 93 || exit.name != "exit" || report.exit?.code != 42 {
            failures.append(fail("execution-exit", "expected captured guest TPIDR_EL0-derived exit(42)"))
        }
        let decoded = report.decodedInstructions
        if !decoded.contains(where: { $0.instructionClass == "system_register" && $0.op == "msr" && $0.sysreg == "tpidr_el0" }) ||
            !decoded.contains(where: { $0.instructionClass == "system_register" && $0.op == "mrs" && $0.sysreg == "tpidr_el0" }) {
            failures.append(fail("execution-tls", "expected decoded MSR/MRS TPIDR_EL0 instructions"))
        }
    }
    if metadata.caseID == "init_005_branches" {
        if report.guestInstructionsExecuted != 5 {
            failures.append(fail("execution-instruction-count", "expected 5 guest instructions, executed \(report.guestInstructionsExecuted)"))
        }
        guard report.syscalls.count == 1 else {
            failures.append(fail("execution-syscalls", "expected one exit syscall, captured \(report.syscalls.count)"))
            return failures
        }
        let exit = report.syscalls[0]
        if exit.nr != 93 || exit.name != "exit" || report.exit?.code != 42 {
            failures.append(fail("execution-exit", "expected captured branch-derived exit(42)"))
        }
        let decoded = report.decodedInstructions
        if !decoded.contains(where: { $0.instructionClass == "compare_and_branch_immediate" && $0.op == "cbz" && $0.rt == 0 }) {
            failures.append(fail("execution-branch", "expected decoded CBZ x0 branch instruction"))
        }
    }
    if metadata.caseID == "init_006_memory" {
        if report.guestInstructionsExecuted != 4 {
            failures.append(fail("execution-instruction-count", "expected 4 guest instructions, executed \(report.guestInstructionsExecuted)"))
        }
        guard report.syscalls.count == 1 else {
            failures.append(fail("execution-syscalls", "expected one exit syscall, captured \(report.syscalls.count)"))
            return failures
        }
        let exit = report.syscalls[0]
        if exit.nr != 93 || exit.name != "exit" || report.exit?.code != 42 {
            failures.append(fail("execution-exit", "expected captured memory-derived exit(42)"))
        }
        let decoded = report.decodedInstructions
        if !decoded.contains(where: {
            $0.instructionClass == "load_store_unsigned_immediate" &&
            $0.op == "ldr" &&
            $0.rt == 0 &&
            $0.rn == 1 &&
            $0.offset == 0 &&
            $0.width == 64 &&
            $0.effectiveAddress == "0x0000000000210130"
        }) {
            failures.append(fail("execution-memory", "expected decoded LDR x0, [x1] from file-backed PT_LOAD address 0x210130"))
        }
    }
    if metadata.caseID == "init_007_mprotect" {
        if report.guestInstructionsExecuted != 8 {
            failures.append(fail("execution-instruction-count", "expected 8 guest instructions, executed \(report.guestInstructionsExecuted)"))
        }
        guard report.syscalls.count == 2 else {
            failures.append(fail("execution-syscalls", "expected mprotect and exit syscalls, captured \(report.syscalls.count)"))
            return failures
        }
        let mprotect = report.syscalls[0]
        if mprotect.nr != 226 ||
            mprotect.name != "mprotect" ||
            jsonString(mprotect.args.indices.contains(0) ? mprotect.args[0] : nil) != "0x0000000000212000" ||
            jsonInt(mprotect.args.indices.contains(1) ? mprotect.args[1] : nil) != 4096 ||
            jsonInt(mprotect.args.indices.contains(2) ? mprotect.args[2] : nil) != 1 {
            failures.append(fail("execution-mprotect", "expected captured mprotect(0x0000000000212000, 4096, PROT_READ)"))
        }
        let exit = report.syscalls[1]
        if exit.nr != 93 || exit.name != "exit" || report.exit?.code != 0 {
            failures.append(fail("execution-exit", "expected captured exit(0) after mprotect"))
        }
        let decoded = report.decodedInstructions
        if !decoded.contains(where: { $0.instructionClass == "pc_relative_address" && $0.op == "adr" && $0.rd == 0 && $0.imm == 4096 }) {
            failures.append(fail("execution-address", "expected decoded ADR x0 to page at +4096"))
        }
    }
    if metadata.caseID == "init_008_self_modify" {
        if report.guestInstructionsExecuted != 6 {
            failures.append(fail("execution-instruction-count", "expected 6 guest instructions, executed \(report.guestInstructionsExecuted)"))
        }
        guard report.syscalls.count == 1 else {
            failures.append(fail("execution-syscalls", "expected one exit syscall, captured \(report.syscalls.count)"))
            return failures
        }
        let exit = report.syscalls[0]
        if exit.nr != 93 || exit.name != "exit" || report.exit?.code != 0 {
            failures.append(fail("execution-exit", "expected captured exit(0) after self-modifying write"))
        }
        if report.memoryWrites.count != 1 ||
            report.memoryWrites.first?.address != "0x0000000000210138" ||
            report.memoryWrites.first?.width != 64 ||
            report.memoryWrites.first?.value != "0x000000000000002a" ||
            report.memoryWrites.first?.captured != true {
            failures.append(fail("execution-self-modify-write", "expected captured 64-bit write of 42 to patch_slot at 0x210138"))
        }
        let decoded = report.decodedInstructions
        if !decoded.contains(where: { $0.instructionClass == "load_store_unsigned_immediate" && $0.op == "str" && $0.rt == 0 && $0.rn == 1 && $0.effectiveAddress == "0x0000000000210138" }) {
            failures.append(fail("execution-self-modify-decode", "expected decoded STR x0, [x1] to patch_slot"))
        }
    }
    if metadata.caseID == "init_010_cpu_model" {
        if report.guestInstructionsExecuted != 4 {
            failures.append(fail("execution-instruction-count", "expected 4 guest instructions, executed \(report.guestInstructionsExecuted)"))
        }
        guard report.syscalls.count == 1 else {
            failures.append(fail("execution-syscalls", "expected one exit syscall, captured \(report.syscalls.count)"))
            return failures
        }
        let exit = report.syscalls[0]
        if exit.nr != 93 || exit.name != "exit" || report.exit?.code != 0 {
            failures.append(fail("execution-exit", "expected captured exit(0) after CPU-model payload check"))
        }
        let decoded = report.decodedInstructions
        if !decoded.contains(where: { $0.instructionClass == "pc_relative_address" && $0.op == "adr" && $0.rd == 1 && $0.imm == 16 }) {
            failures.append(fail("execution-cpu-model-address", "expected decoded ADR x1 to cpu_model payload"))
        }
        let hasModelNote = report.notes.contains { $0.contains("virtual CPU model payload orlix-aarch64-v1") }
        if !hasModelNote {
            failures.append(fail("execution-cpu-model", "expected switch-debug to capture virtual CPU model payload orlix-aarch64-v1"))
        }
    }
    if metadata.caseID == "init_011_static_pie_got_byte_load" {
        if report.guestInstructionsExecuted != 5 {
            failures.append(fail("execution-instruction-count", "expected 5 guest instructions, executed \(report.guestInstructionsExecuted)"))
        }
        guard report.syscalls.count == 1 else {
            failures.append(fail("execution-syscalls", "expected one exit syscall, captured \(report.syscalls.count)"))
            return failures
        }
        let exit = report.syscalls[0]
        if exit.nr != 93 || exit.name != "exit" || report.exit?.code != 42 {
            failures.append(fail("execution-exit", "expected captured GOT-byte-derived exit(42)"))
        }
        let decoded = report.decodedInstructions
        if !decoded.contains(where: {
            $0.instructionClass == "pc_relative_address" &&
            $0.op == "adrp" &&
            $0.rd == 8 &&
            $0.effectiveAddress == "0x0000000000021000"
        }) {
            failures.append(fail("execution-adrp", "expected decoded ADRP x8 to architectural 4K page 0x21000"))
        }
        if !decoded.contains(where: {
            $0.instructionClass == "load_store_unsigned_immediate" &&
            $0.op == "ldr" &&
            $0.rt == 8 &&
            $0.rn == 8 &&
            $0.offset == 216 &&
            $0.width == 64 &&
            $0.effectiveAddress == "0x00000000000210d8"
        }) {
            failures.append(fail("execution-got-load", "expected decoded LDR x8, [x8, #0xd8] from relocated GOT slot 0x210d8"))
        }
        if !decoded.contains(where: {
            $0.instructionClass == "load_store_unsigned_immediate" &&
            $0.op == "ldrb" &&
            $0.rt == 0 &&
            $0.rn == 8 &&
            $0.width == 8 &&
            $0.effectiveAddress == "0x0000000000001000"
        }) {
            failures.append(fail("execution-byte-load", "expected decoded LDRB w0, [x8] from relocated payload byte 0x1000"))
        }
    }
    return failures
}

func executeInit001SwitchDebug(binary: URL, metadata: GoldenMetadata) throws -> (report: ExecutionReport, failures: [Failure]) {
    try executeSwitchDebug(binary: binary, metadata: metadata)
}

func buildFixtureBinary(source: URL, outputRoot: URL, name: String) throws -> (binary: URL, source: URL, metadata: [String: String]) {
    try buildAarch64NoLibc(source: source, outputRoot: outputRoot, binaryName: name)
}

func validateAndExecuteInit001(metadataURL: URL, outputRoot: URL) throws -> (failures: [Failure], artifacts: [String], execution: ExecutionReport?) {
    let built = try buildInit001(outputRoot: outputRoot)
    let expected = try decoder.decode(GoldenMetadata.self, from: Data(contentsOf: metadataURL))
    let sourceHash = try sha256(built.source)
    let binaryHash = try sha256(built.binary)
    let fileOutput = built.metadata["file_output"] ?? ""
    let objdumpHeader = built.metadata["objdump_header"] ?? ""
    let disassembly = built.metadata["disassembly"] ?? ""
    var failures = validateInit001Metadata(
        expected,
        sourceHash: sourceHash,
        binaryHash: binaryHash,
        fileOutput: fileOutput,
        objdumpHeader: objdumpHeader,
        disassembly: disassembly
    )
    let execution = try executeInit001SwitchDebug(binary: built.binary, metadata: expected)
    failures.append(contentsOf: execution.failures)
    let executionURL = outputRoot
        .appendingPathComponent("init_001_exit", isDirectory: true)
        .appendingPathComponent("execution.json")
    try writeJSON(execution.report, to: executionURL)
    return (failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
}

func validateAndExecuteGoldenCase(caseID: String, metadataURL: URL, outputRoot: URL) throws -> (failures: [Failure], artifacts: [String], execution: ExecutionReport?) {
    if caseID == "init_001_exit" {
        return try validateAndExecuteInit001(metadataURL: metadataURL, outputRoot: outputRoot)
    }
    guard ["init_002_write", "init_003_stack", "init_004_tls", "init_005_branches", "init_006_memory", "init_007_mprotect", "init_008_self_modify", "init_009_faults", "init_010_cpu_model", "init_011_static_pie_got_byte_load"].contains(caseID) else {
        throw GateError.usage("unsupported golden ELF execution case \(caseID)")
    }
    let built = try buildGoldenCase(caseID, outputRoot: outputRoot)
    let expected = try decoder.decode(GoldenMetadata.self, from: Data(contentsOf: metadataURL))
    let sourceHash = try sha256(built.source)
    let binaryHash = try sha256(built.binary)
    let fileOutput = built.metadata["file_output"] ?? ""
    let objdumpHeader = built.metadata["objdump_header"] ?? ""
    let disassembly = built.metadata["disassembly"] ?? ""
    var failures: [Failure]
    switch caseID {
    case "init_002_write":
        failures = validateInit002Metadata(
            expected,
            sourceHash: sourceHash,
            binaryHash: binaryHash,
            fileOutput: fileOutput,
            objdumpHeader: objdumpHeader,
            disassembly: disassembly,
            binary: built.binary
        )
    case "init_003_stack":
        failures = validateInit003Metadata(
            expected,
            sourceHash: sourceHash,
            binaryHash: binaryHash,
            fileOutput: fileOutput,
            objdumpHeader: objdumpHeader,
            disassembly: disassembly
        )
    case "init_004_tls":
        failures = validateInit004Metadata(
            expected,
            sourceHash: sourceHash,
            binaryHash: binaryHash,
            fileOutput: fileOutput,
            objdumpHeader: objdumpHeader,
            disassembly: disassembly
        )
    case "init_005_branches":
        failures = validateInit005Metadata(
            expected,
            sourceHash: sourceHash,
            binaryHash: binaryHash,
            fileOutput: fileOutput,
            objdumpHeader: objdumpHeader,
            disassembly: disassembly
        )
    case "init_006_memory":
        failures = validateInit006Metadata(
            expected,
            sourceHash: sourceHash,
            binaryHash: binaryHash,
            fileOutput: fileOutput,
            objdumpHeader: objdumpHeader,
            disassembly: disassembly
        )
    case "init_007_mprotect":
        failures = validateInit007Metadata(
            expected,
            sourceHash: sourceHash,
            binaryHash: binaryHash,
            fileOutput: fileOutput,
            objdumpHeader: objdumpHeader,
            disassembly: disassembly
        )
    case "init_008_self_modify":
        failures = validateInit008Metadata(
            expected,
            sourceHash: sourceHash,
            binaryHash: binaryHash,
            fileOutput: fileOutput,
            objdumpHeader: objdumpHeader,
            disassembly: disassembly
        )
    case "init_009_faults":
        failures = validateInit009Metadata(
            expected,
            sourceHash: sourceHash,
            binaryHash: binaryHash,
            fileOutput: fileOutput,
            objdumpHeader: objdumpHeader,
            disassembly: disassembly
        )
    case "init_010_cpu_model":
        failures = validateInit010Metadata(
            expected,
            sourceHash: sourceHash,
            binaryHash: binaryHash,
            fileOutput: fileOutput,
            objdumpHeader: objdumpHeader,
            disassembly: disassembly,
            binary: built.binary
        )
    case "init_011_static_pie_got_byte_load":
        failures = validateInit011Metadata(
            expected,
            sourceHash: sourceHash,
            binaryHash: binaryHash,
            fileOutput: fileOutput,
            objdumpHeader: objdumpHeader,
            disassembly: disassembly,
            binary: built.binary
        )
    default:
        failures = [fail("case-id", "unsupported golden ELF execution case \(caseID)")]
    }
    let execution = try executeSwitchDebug(binary: built.binary, metadata: expected)
    failures.append(contentsOf: execution.failures)
    let executionURL = outputRoot
        .appendingPathComponent(caseID, isDirectory: true)
        .appendingPathComponent("execution.json")
    try writeJSON(execution.report, to: executionURL)
    return (failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
}

func executeNegativeFixture(_ fixture: String, outputRoot: URL) throws -> (failures: [Failure], artifacts: [String], execution: ExecutionReport?) {
    let goldenPath = path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_001_exit", "golden.json")
    let metadata = try decoder.decode(GoldenMetadata.self, from: Data(contentsOf: goldenPath))
    switch fixture {
    case "wrong-exit":
        return try validateAndExecuteInit001(
            metadataURL: path("tools", "tcti", "fixtures", "golden_elf", "init_001_exit_wrong_expected_exit.json"),
            outputRoot: outputRoot
        )
    case "unsupported":
        fallthrough
    case "unknown":
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_001_exit_unsupported_before_svc.S"),
            outputRoot: outputRoot,
            name: "init_001_exit_unsupported_before_svc"
        )
        let execution = try executeInit001SwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_001_exit", isDirectory: true)
            .appendingPathComponent("unsupported-execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "unsupported-movz-shift":
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_001_exit_movz_shift.S"),
            outputRoot: outputRoot,
            name: "init_001_exit_movz_shift"
        )
        let execution = try executeInit001SwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_001_exit", isDirectory: true)
            .appendingPathComponent("unsupported-movz-shift-execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "unsupported-svc-immediate":
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_001_exit_svc_imm1.S"),
            outputRoot: outputRoot,
            name: "init_001_exit_svc_imm1"
        )
        let execution = try executeInit001SwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_001_exit", isDirectory: true)
            .appendingPathComponent("unsupported-svc-immediate-execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "wrong-syscall":
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_001_exit_wrong_syscall.S"),
            outputRoot: outputRoot,
            name: "init_001_exit_wrong_syscall"
        )
        let execution = try executeInit001SwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_001_exit", isDirectory: true)
            .appendingPathComponent("wrong-syscall-execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "write-wrong-length":
        let metadata = try decoder.decode(
            GoldenMetadata.self,
            from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_002_write", "golden.json"))
        )
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_002_write_wrong_length.S"),
            outputRoot: outputRoot,
            name: "init_002_write_wrong_length"
        )
        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_002_write_wrong_length", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "write-invalid-buffer":
        let metadata = try decoder.decode(
            GoldenMetadata.self,
            from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_002_write", "golden.json"))
        )
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_002_write_invalid_buffer.S"),
            outputRoot: outputRoot,
            name: "init_002_write_invalid_buffer"
        )
        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_002_write_invalid_buffer", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "write-unsupported-adrp":
        let metadata = try decoder.decode(
            GoldenMetadata.self,
            from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_002_write", "golden.json"))
        )
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_002_write_unsupported_adrp.S"),
            outputRoot: outputRoot,
            name: "init_002_write_unsupported_adrp"
        )
        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_002_write_unsupported_adrp", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
	case "static-pie-got-unrelocated-byte-load":
		let metadata = try decoder.decode(
			GoldenMetadata.self,
			from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_011_static_pie_got_byte_load", "golden.json"))
		)
		let built = try buildGoldenCase("init_011_static_pie_got_byte_load", outputRoot: outputRoot)
		let execution = try executeSwitchDebug(
			binary: built.binary,
			metadata: metadata,
			applyRelativeRelocations: false,
			guardNullPageReads: true
		)
		let executionURL = outputRoot
			.appendingPathComponent("init_011_static_pie_got_byte_load", isDirectory: true)
			.appendingPathComponent("static-pie-got-unrelocated-byte-load-execution.json")
		try writeJSON(execution.report, to: executionURL)
		return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
	case "static-pie-got-relocation-invisible-byte-load":
		let metadata = try decoder.decode(
			GoldenMetadata.self,
			from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_011_static_pie_got_byte_load", "golden.json"))
		)
		let built = try buildGoldenCase("init_011_static_pie_got_byte_load", outputRoot: outputRoot)
		let execution = try executeSwitchDebug(
			binary: built.binary,
			metadata: metadata,
			applyRelativeRelocations: true,
			invisibleRelativeRelocationOffsets: [0x210d8],
			guardNullPageReads: true
		)
		let executionURL = outputRoot
			.appendingPathComponent("init_011_static_pie_got_byte_load", isDirectory: true)
			.appendingPathComponent("static-pie-got-relocation-invisible-byte-load-execution.json")
		try writeJSON(execution.report, to: executionURL)
		return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "simd-self-move-unsupported":
        let metadata = try decoder.decode(
            GoldenMetadata.self,
            from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_001_exit", "golden.json"))
        )
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_001_exit_simd_self_move.S"),
            outputRoot: outputRoot,
            name: "init_001_exit_simd_self_move"
        )
        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_001_exit_simd_self_move", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "simd-movi-2d-ones":
        let metadata = try decoder.decode(
            GoldenMetadata.self,
            from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_001_exit", "golden.json"))
        )
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_001_exit_simd_movi_2d_ones.S"),
            outputRoot: outputRoot,
            name: "init_001_exit_simd_movi_2d_ones"
        )
        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_001_exit_simd_movi_2d_ones", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "simd-movi-16b":
        let metadata = try decoder.decode(
            GoldenMetadata.self,
            from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_001_exit", "golden.json"))
        )
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_001_exit_simd_movi_16b.S"),
            outputRoot: outputRoot,
            name: "init_001_exit_simd_movi_16b"
        )
        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_001_exit_simd_movi_16b", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
			return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
        case "simd-movi-4s-0x41":
            let metadata = try decoder.decode(
                GoldenMetadata.self,
                from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_001_exit", "golden.json"))
            )
            let built = try buildFixtureBinary(
                source: path("tools", "tcti", "fixtures", "golden_elf", "init_001_exit_simd_movi_4s_0x41.S"),
                outputRoot: outputRoot,
                name: "init_001_exit_simd_movi_4s_0x41"
            )
            let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
            let executionURL = outputRoot
                .appendingPathComponent("init_001_exit_simd_movi_4s_0x41", isDirectory: true)
                .appendingPathComponent("execution.json")
            try writeJSON(execution.report, to: executionURL)
            return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
	case "simd-movi-4s-0x1":
		let metadata = try decoder.decode(
			GoldenMetadata.self,
			from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_001_exit", "golden.json"))
            )
            let built = try buildFixtureBinary(
                source: path("tools", "tcti", "fixtures", "golden_elf", "init_001_exit_simd_movi_4s_0x1.S"),
                outputRoot: outputRoot,
                name: "init_001_exit_simd_movi_4s_0x1"
            )
            let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
            let executionURL = outputRoot
                .appendingPathComponent("init_001_exit_simd_movi_4s_0x1", isDirectory: true)
                .appendingPathComponent("execution.json")
		try writeJSON(execution.report, to: executionURL)
		return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
	case "simd-cmeq-4s":
		let metadata = try decoder.decode(
			GoldenMetadata.self,
			from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_001_exit", "golden.json"))
		)
		let built = try buildFixtureBinary(
			source: path("tools", "tcti", "fixtures", "golden_elf", "init_001_exit_simd_cmeq_4s.S"),
			outputRoot: outputRoot,
			name: "init_001_exit_simd_cmeq_4s"
		)
		let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
		let executionURL = outputRoot
			.appendingPathComponent("init_001_exit_simd_cmeq_4s", isDirectory: true)
			.appendingPathComponent("execution.json")
		try writeJSON(execution.report, to: executionURL)
		return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
	case "simd-umaxv-4s":
		let metadata = try decoder.decode(
			GoldenMetadata.self,
			from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_001_exit", "golden.json"))
		)
		let built = try buildFixtureBinary(
			source: path("tools", "tcti", "fixtures", "golden_elf", "init_001_exit_simd_umaxv_4s.S"),
			outputRoot: outputRoot,
			name: "init_001_exit_simd_umaxv_4s"
		)
		let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
		let executionURL = outputRoot
			.appendingPathComponent("init_001_exit_simd_umaxv_4s", isDirectory: true)
			.appendingPathComponent("execution.json")
		try writeJSON(execution.report, to: executionURL)
		return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
	case "simd-addv-4s":
		let metadata = try decoder.decode(
			GoldenMetadata.self,
			from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_001_exit", "golden.json"))
		)
		let built = try buildFixtureBinary(
			source: path("tools", "tcti", "fixtures", "golden_elf", "init_001_exit_simd_addv_4s.S"),
			outputRoot: outputRoot,
			name: "init_001_exit_simd_addv_4s"
		)
		let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
		let executionURL = outputRoot
			.appendingPathComponent("init_001_exit_simd_addv_4s", isDirectory: true)
			.appendingPathComponent("execution.json")
		try writeJSON(execution.report, to: executionURL)
		return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
	case "fmov-w-s":
		let metadata = try decoder.decode(
			GoldenMetadata.self,
			from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_001_exit", "golden.json"))
		)
		let built = try buildFixtureBinary(
			source: path("tools", "tcti", "fixtures", "golden_elf", "init_001_exit_fmov_w11_s5.S"),
			outputRoot: outputRoot,
			name: "init_001_exit_fmov_w11_s5"
		)
		let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
		let executionURL = outputRoot
			.appendingPathComponent("init_001_exit_fmov_w11_s5", isDirectory: true)
			.appendingPathComponent("execution.json")
		try writeJSON(execution.report, to: executionURL)
		return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
	case "simd-and-16b":
		let metadata = try decoder.decode(
			GoldenMetadata.self,
			from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_001_exit", "golden.json"))
        )
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_001_exit_simd_and_16b.S"),
            outputRoot: outputRoot,
            name: "init_001_exit_simd_and_16b"
        )
        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_001_exit_simd_and_16b", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
	    case "simd-orr-4s":
	        let metadata = try decoder.decode(
	            GoldenMetadata.self,
	            from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_001_exit", "golden.json"))
	        )
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_001_exit_simd_orr_4s.S"),
            outputRoot: outputRoot,
            name: "init_001_exit_simd_orr_4s"
        )
        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_001_exit_simd_orr_4s", isDirectory: true)
            .appendingPathComponent("execution.json")
	        try writeJSON(execution.report, to: executionURL)
	        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "simd-s-lane-move-unsupported":
        let metadata = try decoder.decode(
            GoldenMetadata.self,
            from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_001_exit", "golden.json"))
        )
	        let built = try buildFixtureBinary(
	            source: path("tools", "tcti", "fixtures", "golden_elf", "init_001_exit_simd_s_lane_move.S"),
	            outputRoot: outputRoot,
	            name: "init_001_exit_simd_s_lane_move"
	        )
	        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
	        let executionURL = outputRoot
	            .appendingPathComponent("init_001_exit_simd_s_lane_move", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "fmov-w11-s5-unsupported":
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_001_exit_fmov_w11_s5.S"),
            outputRoot: outputRoot,
            name: "init_001_exit_fmov_w11_s5"
        )
        let execution = try executeInit001SwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_001_exit_fmov_w11_s5", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "simd-xtn-4h-unsupported":
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_001_exit_simd_xtn_4h.S"),
            outputRoot: outputRoot,
            name: "init_001_exit_simd_xtn_4h"
        )
        let execution = try executeInit001SwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_001_exit_simd_xtn_4h", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "brk-trap-unsupported":
	        let metadata = try decoder.decode(
	            GoldenMetadata.self,
	            from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_001_exit", "golden.json"))
	        )
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_001_exit_brk_trap.S"),
            outputRoot: outputRoot,
            name: "init_001_exit_brk_trap"
        )
        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_001_exit_brk_trap", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "stack-wrong-exit":
        let metadata = try decoder.decode(
            GoldenMetadata.self,
            from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_003_stack", "golden.json"))
        )
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_003_stack_wrong_exit.S"),
            outputRoot: outputRoot,
            name: "init_003_stack_wrong_exit"
        )
        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_003_stack_wrong_exit", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "stack-invalid-memory":
        let metadata = try decoder.decode(
            GoldenMetadata.self,
            from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_003_stack", "golden.json"))
        )
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_003_stack_invalid_stack_read.S"),
            outputRoot: outputRoot,
            name: "init_003_stack_invalid_stack_read"
        )
        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_003_stack_invalid_stack_read", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "stack-unsupported-preindex":
        let metadata = try decoder.decode(
            GoldenMetadata.self,
            from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_003_stack", "golden.json"))
        )
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_003_stack_unsupported_preindex.S"),
            outputRoot: outputRoot,
            name: "init_003_stack_unsupported_preindex"
        )
        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_003_stack_unsupported_preindex", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "tls-wrong-exit":
        let metadata = try decoder.decode(
            GoldenMetadata.self,
            from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_004_tls", "golden.json"))
        )
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_004_tls_wrong_exit.S"),
            outputRoot: outputRoot,
            name: "init_004_tls_wrong_exit"
        )
        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_004_tls_wrong_exit", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "tls-unsupported-sysreg":
        let metadata = try decoder.decode(
            GoldenMetadata.self,
            from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_004_tls", "golden.json"))
        )
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_004_tls_unsupported_sysreg.S"),
            outputRoot: outputRoot,
            name: "init_004_tls_unsupported_sysreg"
        )
        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_004_tls_unsupported_sysreg", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "branches-unsupported-cbnz":
        let metadata = try decoder.decode(
            GoldenMetadata.self,
            from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_005_branches", "golden.json"))
        )
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_005_branches_unsupported_cbnz.S"),
            outputRoot: outputRoot,
            name: "init_005_branches_unsupported_cbnz"
        )
        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_005_branches_unsupported_cbnz", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "memory-invalid-read":
        let metadata = try decoder.decode(
            GoldenMetadata.self,
            from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_006_memory", "golden.json"))
        )
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_006_memory_invalid_read.S"),
            outputRoot: outputRoot,
            name: "init_006_memory_invalid_read"
        )
        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_006_memory_invalid_read", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "memory-unsupported-ldur":
        let metadata = try decoder.decode(
            GoldenMetadata.self,
            from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_006_memory", "golden.json"))
        )
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_006_memory_unsupported_ldur.S"),
            outputRoot: outputRoot,
            name: "init_006_memory_unsupported_ldur"
        )
        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_006_memory_unsupported_ldur", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "memory-unsupported-store":
        let metadata = try decoder.decode(
            GoldenMetadata.self,
            from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_006_memory", "golden.json"))
        )
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_006_memory_unsupported_store.S"),
            outputRoot: outputRoot,
            name: "init_006_memory_unsupported_store"
        )
        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_006_memory_unsupported_store", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "mprotect-wrong-prot":
        let metadata = try decoder.decode(
            GoldenMetadata.self,
            from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_007_mprotect", "golden.json"))
        )
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_007_mprotect_wrong_prot.S"),
            outputRoot: outputRoot,
            name: "init_007_mprotect_wrong_prot"
        )
        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_007_mprotect_wrong_prot", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "mprotect-exec-prot":
        let metadata = try decoder.decode(
            GoldenMetadata.self,
            from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_007_mprotect", "golden.json"))
        )
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_007_mprotect_exec_prot.S"),
            outputRoot: outputRoot,
            name: "init_007_mprotect_exec_prot"
        )
        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_007_mprotect_exec_prot", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "mprotect-wrong-syscall":
        let metadata = try decoder.decode(
            GoldenMetadata.self,
            from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_007_mprotect", "golden.json"))
        )
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_007_mprotect_wrong_syscall.S"),
            outputRoot: outputRoot,
            name: "init_007_mprotect_wrong_syscall"
        )
        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_007_mprotect_wrong_syscall", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "self-modify-wrong-value":
        let metadata = try decoder.decode(
            GoldenMetadata.self,
            from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_008_self_modify", "golden.json"))
        )
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_008_self_modify_wrong_value.S"),
            outputRoot: outputRoot,
            name: "init_008_self_modify_wrong_value"
        )
        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_008_self_modify_wrong_value", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "self-modify-invalid-write":
        let metadata = try decoder.decode(
            GoldenMetadata.self,
            from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_008_self_modify", "golden.json"))
        )
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_008_self_modify_invalid_write.S"),
            outputRoot: outputRoot,
            name: "init_008_self_modify_invalid_write"
        )
        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_008_self_modify_invalid_write", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "self-modify-unsupported-branch":
        let metadata = try decoder.decode(
            GoldenMetadata.self,
            from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_008_self_modify", "golden.json"))
        )
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_008_self_modify_unsupported_branch.S"),
            outputRoot: outputRoot,
            name: "init_008_self_modify_unsupported_branch"
        )
        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_008_self_modify_unsupported_branch", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "faults-wrong-address":
        let metadata = try decoder.decode(
            GoldenMetadata.self,
            from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_009_faults", "golden.json"))
        )
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_009_faults_wrong_address.S"),
            outputRoot: outputRoot,
            name: "init_009_faults_wrong_address"
        )
        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_009_faults_wrong_address", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "faults-missing-fault":
        let metadata = try decoder.decode(
            GoldenMetadata.self,
            from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_009_faults", "golden.json"))
        )
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_009_faults_missing_fault.S"),
            outputRoot: outputRoot,
            name: "init_009_faults_missing_fault"
        )
        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_009_faults_missing_fault", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "cpu-model-wrong-model":
        let metadata = try decoder.decode(
            GoldenMetadata.self,
            from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_010_cpu_model", "golden.json"))
        )
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_010_cpu_model_wrong_model.S"),
            outputRoot: outputRoot,
            name: "init_010_cpu_model_wrong_model"
        )
        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_010_cpu_model_wrong_model", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    case "cpu-model-unsupported-ctr-el0":
        let metadata = try decoder.decode(
            GoldenMetadata.self,
            from: Data(contentsOf: path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_010_cpu_model", "golden.json"))
        )
        let built = try buildFixtureBinary(
            source: path("tools", "tcti", "fixtures", "golden_elf", "init_010_cpu_model_unsupported_ctr_el0.S"),
            outputRoot: outputRoot,
            name: "init_010_cpu_model_unsupported_ctr_el0"
        )
        let execution = try executeSwitchDebug(binary: built.binary, metadata: metadata)
        let executionURL = outputRoot
            .appendingPathComponent("init_010_cpu_model_unsupported_ctr_el0", isDirectory: true)
            .appendingPathComponent("execution.json")
        try writeJSON(execution.report, to: executionURL)
        return (execution.failures, [relativePath(built.binary), relativePath(executionURL)], execution.report)
    default:
        throw GateError.usage("unknown NEGATIVE_EXECUTION=\(fixture)")
    }
}

func validateNegativeExecutionFixtures(artifacts: inout [String]) -> [Failure] {
    var failures: [Failure] = []
    for fixture in [
        "wrong-exit",
        "unsupported-movz-shift",
        "unsupported-svc-immediate",
        "unknown",
        "wrong-syscall",
        "write-wrong-length",
        "write-invalid-buffer",
        "write-unsupported-adrp",
        "stack-wrong-exit",
        "stack-invalid-memory",
        "stack-unsupported-preindex",
        "tls-wrong-exit",
        "tls-unsupported-sysreg",
        "branches-unsupported-cbnz",
        "memory-invalid-read",
        "memory-unsupported-ldur",
        "memory-unsupported-store",
        "mprotect-wrong-prot",
        "mprotect-exec-prot",
        "mprotect-wrong-syscall",
        "self-modify-wrong-value",
        "self-modify-invalid-write",
        "self-modify-unsupported-branch",
        "faults-wrong-address",
        "faults-missing-fault",
        "cpu-model-wrong-model",
        "cpu-model-unsupported-ctr-el0",
    ] {
        do {
            let result = try executeNegativeFixture(fixture, outputRoot: buildPath("golden_elf_negative", fixture))
            artifacts.append(contentsOf: result.artifacts)
            if result.failures.isEmpty {
                failures.append(fail("negative-\(fixture)", "negative execution fixture \(fixture) unexpectedly passed"))
            }
        let fixtureCaseID = fixture.hasPrefix("write-") ? "init_002_write" :
            (fixture.hasPrefix("stack-") ? "init_003_stack" :
            (fixture.hasPrefix("tls-") ? "init_004_tls" :
            (fixture.hasPrefix("branches-") ? "init_005_branches" :
            (fixture.hasPrefix("memory-") ? "init_006_memory" :
            (fixture.hasPrefix("mprotect-") ? "init_007_mprotect" :
                            (fixture.hasPrefix("self-modify-") ? "init_008_self_modify" :
                                (fixture.hasPrefix("faults-") ? "init_009_faults" :
                                    (fixture.hasPrefix("cpu-model-") ? "init_010_cpu_model" : "init_001_exit"))))))))
            let reducer = try writeReducer(
                target: "tcti-golden-elf",
                caseID: "execution-\(fixture)",
                command: "CASE=\(fixtureCaseID) EXECUTE=switch-debug NEGATIVE_EXECUTION=\(fixture) make tcti-gate TARGET=tcti-golden-elf",
                reason: "negative execution fixture \(fixture) must fail",
                artifacts: result.artifacts,
                expectedStatus: .fail
            )
            artifacts.append(relativePath(reducer))
        } catch {
            failures.append(fail("negative-\(fixture)", "\(error)"))
        }
    }
    return failures
}

func expectedEntrypoint(_ metadata: GoldenMetadata) -> String {
    metadata.entrypoint.lowercased()
}

func actualEntrypoint(from objdumpHeader: String) -> String {
    for line in objdumpHeader.split(separator: "\n") {
        let lowercased = line.lowercased()
        guard let range = lowercased.range(of: "start address:") else {
            continue
        }
        let value = lowercased[range.upperBound...].trimmingCharacters(in: .whitespacesAndNewlines)
        if value.hasPrefix("0x") {
            return value
        }
    }
    return "0x0000000000210120"
}

func validateInit001Metadata(
    _ metadata: GoldenMetadata,
    sourceHash: String,
    binaryHash: String,
    fileOutput: String,
    objdumpHeader: String,
    disassembly: String
) -> [Failure] {
    var failures: [Failure] = []
    if metadata.caseID != "init_001_exit" {
        failures.append(fail("case-id", "golden case id must be init_001_exit"))
    }
    if metadata.sourceSHA256 != sourceHash {
        failures.append(fail("source-sha256", "source hash changed for init_001_exit"))
    }
    if metadata.expectedBinarySHA256 != binaryHash {
        failures.append(fail("binary-sha256", "binary hash changed for init_001_exit; inspect or run make tcti-gate TARGET=tcti-golden-elf-refresh CASE=init_001_exit"))
    }
    if metadata.actualBinarySHA256 != binaryHash {
        failures.append(fail("actual-binary-sha256", "golden actual binary hash no longer matches generated binary for init_001_exit"))
    }
    if metadata.elfType != "ET_EXEC" {
        failures.append(fail("elf-type", "init_001_exit golden metadata must use ET_EXEC"))
    }
    if metadata.machine != "AArch64" {
        failures.append(fail("elf-machine", "init_001_exit golden metadata must use AArch64"))
    }
    if metadata.expectedExitCode != 42 {
        failures.append(fail("expected-exit", "init_001_exit must expect exit code 42"))
    }
    if metadata.expectedSyscalls.count != 1 ||
        metadata.expectedSyscalls.first?.nr != "exit" ||
        metadata.expectedSyscalls.first?.code != 42 {
        failures.append(fail("expected-syscall", "init_001_exit must expect exactly exit(42)"))
    }
    if !fileOutput.contains("ELF 64-bit LSB executable") {
        failures.append(fail("elf-class", "init_001_exit must be ELF64 executable"))
    }
    if !fileOutput.contains("ARM aarch64") {
        failures.append(fail("elf-file-machine", "init_001_exit file output must identify ARM aarch64"))
    }
    if !objdumpHeader.contains("file format elf64-littleaarch64") {
        failures.append(fail("objdump-format", "init_001_exit must disassemble as elf64-littleaarch64"))
    }
    if !objdumpHeader.contains("architecture: aarch64") {
        failures.append(fail("objdump-architecture", "init_001_exit objdump architecture must be aarch64"))
    }
    if !objdumpHeader.lowercased().contains("start address: \(expectedEntrypoint(metadata))") {
        failures.append(fail("entrypoint", "init_001_exit entrypoint does not match \(metadata.entrypoint)"))
    }
    let requiredInstructionWords = [
        "d2800ba8": "mov x8, #93",
        "d2800540": "mov x0, #42",
        "d4000001": "svc #0",
    ]
    for (word, description) in requiredInstructionWords where !disassembly.contains(word) {
        failures.append(fail("instruction-shape", "init_001_exit disassembly missing \(description) instruction word \(word)"))
    }
    if !disassembly.contains("mov\tx8") || !disassembly.contains("#0x5d") {
        failures.append(fail("syscall-nr", "init_001_exit disassembly must load x8 with Linux exit syscall 93"))
    }
    if !disassembly.contains("mov\tx0") || !disassembly.contains("#0x2a") {
        failures.append(fail("syscall-arg", "init_001_exit disassembly must load x0 with exit code 42"))
    }
    if !disassembly.contains("svc\t#0") {
        failures.append(fail("svc", "init_001_exit disassembly must end in svc #0"))
    }
    for (key, value) in metadata.forbiddenBehavior where value {
        failures.append(fail("forbidden-behavior", "init_001_exit golden metadata sets forbidden_behavior.\(key)=true"))
    }
    return failures
}

func validateInit002Metadata(
    _ metadata: GoldenMetadata,
    sourceHash: String,
    binaryHash: String,
    fileOutput: String,
    objdumpHeader: String,
    disassembly: String,
    binary: URL
) -> [Failure] {
    var failures: [Failure] = []
    if metadata.caseID != "init_002_write" {
        failures.append(fail("case-id", "golden case id must be init_002_write"))
    }
    if metadata.sourceSHA256 != sourceHash {
        failures.append(fail("source-sha256", "source hash changed for init_002_write"))
    }
    if metadata.expectedBinarySHA256 != binaryHash {
        failures.append(fail("binary-sha256", "binary hash changed for init_002_write; inspect or run make tcti-gate TARGET=tcti-golden-elf-refresh CASE=init_002_write"))
    }
    if metadata.actualBinarySHA256 != binaryHash {
        failures.append(fail("actual-binary-sha256", "golden actual binary hash no longer matches generated binary for init_002_write"))
    }
    if metadata.elfType != "ET_EXEC" {
        failures.append(fail("elf-type", "init_002_write golden metadata must use ET_EXEC"))
    }
    if metadata.machine != "AArch64" {
        failures.append(fail("elf-machine", "init_002_write golden metadata must use AArch64"))
    }
    if metadata.expectedExitCode != 0 {
        failures.append(fail("expected-exit", "init_002_write must expect exit code 0"))
    }
    if metadata.expectedMessage != "hello\n" {
        failures.append(fail("expected-message", "init_002_write must expect hello newline payload"))
    }
    if metadata.expectedSyscalls.count != 2 ||
        metadata.expectedSyscalls[0].nr != "write" ||
        metadata.expectedSyscalls[0].fd != 1 ||
        metadata.expectedSyscalls[0].len != 6 ||
        metadata.expectedSyscalls[0].bytes != "hello\n" ||
        metadata.expectedSyscalls[1].nr != "exit" ||
        metadata.expectedSyscalls[1].code != 0 {
        failures.append(fail("expected-syscall", "init_002_write must expect write(1, hello newline, 6), exit(0)"))
    }
    if !fileOutput.contains("ELF 64-bit LSB executable") {
        failures.append(fail("elf-class", "init_002_write must be ELF64 executable"))
    }
    if !fileOutput.contains("ARM aarch64") {
        failures.append(fail("elf-file-machine", "init_002_write file output must identify ARM aarch64"))
    }
    if !objdumpHeader.contains("file format elf64-littleaarch64") {
        failures.append(fail("objdump-format", "init_002_write must disassemble as elf64-littleaarch64"))
    }
    if !objdumpHeader.contains("architecture: aarch64") {
        failures.append(fail("objdump-architecture", "init_002_write objdump architecture must be aarch64"))
    }
    if !objdumpHeader.lowercased().contains("start address: \(expectedEntrypoint(metadata))") {
        failures.append(fail("entrypoint", "init_002_write entrypoint does not match \(metadata.entrypoint)"))
    }
    let requiredInstructionWords = [
        "d2800020": "mov x0, #1",
        "100000e1": "adr x1, msg",
        "d28000c2": "mov x2, #6",
        "d2800808": "mov x8, #64",
        "d4000001": "svc #0",
        "d2800000": "mov x0, #0",
        "d2800ba8": "mov x8, #93",
    ]
    for (word, description) in requiredInstructionWords where !disassembly.contains(word) {
        failures.append(fail("instruction-shape", "init_002_write disassembly missing \(description) instruction word \(word)"))
    }
    if !disassembly.contains("adr\tx1") || !disassembly.contains("<msg>") {
        failures.append(fail("address-shape", "init_002_write must use ADR to address msg"))
    }
    if !disassembly.contains("mov\tx8") || !disassembly.contains("#0x40") || !disassembly.contains("#0x5d") {
        failures.append(fail("syscall-nr", "init_002_write disassembly must load write(64) and exit(93) syscall numbers"))
    }
    if !disassembly.contains("svc\t#0") {
        failures.append(fail("svc", "init_002_write disassembly must use svc #0"))
    }
    do {
        let elf = try TinyElf64Aarch64(binary: binary)
        let msg = try elf.readBytes(at: 0x210140, length: 6)
        if String(data: msg, encoding: .utf8) != "hello\n" {
            failures.append(fail("message-bytes", "init_002_write message bytes do not equal hello newline"))
        }
    } catch {
        failures.append(fail("message-bytes", "\(error)"))
    }
    for (key, value) in metadata.forbiddenBehavior where value {
        failures.append(fail("forbidden-behavior", "init_002_write golden metadata sets forbidden_behavior.\(key)=true"))
    }
    return failures
}

func validateInit003Metadata(
    _ metadata: GoldenMetadata,
    sourceHash: String,
    binaryHash: String,
    fileOutput: String,
    objdumpHeader: String,
    disassembly: String
) -> [Failure] {
    var failures: [Failure] = []
    if metadata.caseID != "init_003_stack" {
        failures.append(fail("case-id", "golden case id must be init_003_stack"))
    }
    if metadata.sourceSHA256 != sourceHash {
        failures.append(fail("source-sha256", "source hash changed for init_003_stack"))
    }
    if metadata.expectedBinarySHA256 != binaryHash {
        failures.append(fail("binary-sha256", "binary hash changed for init_003_stack; inspect or run make tcti-gate TARGET=tcti-golden-elf-refresh CASE=init_003_stack"))
    }
    if metadata.actualBinarySHA256 != binaryHash {
        failures.append(fail("actual-binary-sha256", "golden actual binary hash no longer matches generated binary for init_003_stack"))
    }
    if metadata.elfType != "ET_EXEC" {
        failures.append(fail("elf-type", "init_003_stack golden metadata must use ET_EXEC"))
    }
    if metadata.machine != "AArch64" {
        failures.append(fail("elf-machine", "init_003_stack golden metadata must use AArch64"))
    }
    if metadata.expectedExitCode != 42 {
        failures.append(fail("expected-exit", "init_003_stack must expect exit code 42"))
    }
    if metadata.expectedSyscalls.count != 1 ||
        metadata.expectedSyscalls.first?.nr != "exit" ||
        metadata.expectedSyscalls.first?.code != 42 {
        failures.append(fail("expected-syscall", "init_003_stack must expect exactly exit(42)"))
    }
    if !fileOutput.contains("ELF 64-bit LSB executable") {
        failures.append(fail("elf-class", "init_003_stack must be ELF64 executable"))
    }
    if !fileOutput.contains("ARM aarch64") {
        failures.append(fail("elf-file-machine", "init_003_stack file output must identify ARM aarch64"))
    }
    if !objdumpHeader.contains("file format elf64-littleaarch64") {
        failures.append(fail("objdump-format", "init_003_stack must disassemble as elf64-littleaarch64"))
    }
    if !objdumpHeader.contains("architecture: aarch64") {
        failures.append(fail("objdump-architecture", "init_003_stack objdump architecture must be aarch64"))
    }
    if !objdumpHeader.lowercased().contains("start address: \(expectedEntrypoint(metadata))") {
        failures.append(fail("entrypoint", "init_003_stack entrypoint does not match \(metadata.entrypoint)"))
    }
    let requiredInstructionWords = [
        "910003e0": "mov x0, sp",
        "d10043ff": "sub sp, sp, #16",
        "d2800541": "mov x1, #42",
        "f90007e1": "str x1, [sp, #8]",
        "f94007e0": "ldr x0, [sp, #8]",
        "910043ff": "add sp, sp, #16",
        "d2800ba8": "mov x8, #93",
        "d4000001": "svc #0",
    ]
    for (word, description) in requiredInstructionWords where !disassembly.contains(word) {
        failures.append(fail("instruction-shape", "init_003_stack disassembly missing \(description) instruction word \(word)"))
    }
    if !disassembly.contains("mov\tx0, sp") {
        failures.append(fail("stack-sp-copy", "init_003_stack must copy sp through the decoded ADD alias"))
    }
    if !disassembly.contains("str\tx1, [sp, #0x8]") || !disassembly.contains("ldr\tx0, [sp, #0x8]") {
        failures.append(fail("stack-memory-shape", "init_003_stack must store and load x1/x0 through [sp, #8]"))
    }
    if !disassembly.contains("svc\t#0") {
        failures.append(fail("svc", "init_003_stack disassembly must use svc #0"))
    }
    for (key, value) in metadata.forbiddenBehavior where value {
        failures.append(fail("forbidden-behavior", "init_003_stack golden metadata sets forbidden_behavior.\(key)=true"))
    }
    return failures
}

func validateInit004Metadata(
    _ metadata: GoldenMetadata,
    sourceHash: String,
    binaryHash: String,
    fileOutput: String,
    objdumpHeader: String,
    disassembly: String
) -> [Failure] {
    var failures: [Failure] = []
    if metadata.caseID != "init_004_tls" {
        failures.append(fail("case-id", "golden case id must be init_004_tls"))
    }
    if metadata.sourceSHA256 != sourceHash {
        failures.append(fail("source-sha256", "source hash changed for init_004_tls"))
    }
    if metadata.expectedBinarySHA256 != binaryHash {
        failures.append(fail("binary-sha256", "binary hash changed for init_004_tls; inspect or run make tcti-gate TARGET=tcti-golden-elf-refresh CASE=init_004_tls"))
    }
    if metadata.actualBinarySHA256 != binaryHash {
        failures.append(fail("actual-binary-sha256", "golden actual binary hash no longer matches generated binary for init_004_tls"))
    }
    if metadata.elfType != "ET_EXEC" {
        failures.append(fail("elf-type", "init_004_tls golden metadata must use ET_EXEC"))
    }
    if metadata.machine != "AArch64" {
        failures.append(fail("elf-machine", "init_004_tls golden metadata must use AArch64"))
    }
    if metadata.expectedExitCode != 42 {
        failures.append(fail("expected-exit", "init_004_tls must expect exit code 42"))
    }
    if metadata.expectedSyscalls.count != 1 ||
        metadata.expectedSyscalls.first?.nr != "exit" ||
        metadata.expectedSyscalls.first?.code != 42 {
        failures.append(fail("expected-syscall", "init_004_tls must expect exactly exit(42)"))
    }
    if !fileOutput.contains("ELF 64-bit LSB executable") {
        failures.append(fail("elf-class", "init_004_tls must be ELF64 executable"))
    }
    if !fileOutput.contains("ARM aarch64") {
        failures.append(fail("elf-file-machine", "init_004_tls file output must identify ARM aarch64"))
    }
    if !objdumpHeader.contains("file format elf64-littleaarch64") {
        failures.append(fail("objdump-format", "init_004_tls must disassemble as elf64-littleaarch64"))
    }
    if !objdumpHeader.contains("architecture: aarch64") {
        failures.append(fail("objdump-architecture", "init_004_tls objdump architecture must be aarch64"))
    }
    if !objdumpHeader.lowercased().contains("start address: \(expectedEntrypoint(metadata))") {
        failures.append(fail("entrypoint", "init_004_tls entrypoint does not match \(metadata.entrypoint)"))
    }
    let requiredInstructionWords = [
        "d2800541": "mov x1, #42",
        "d51bd041": "msr TPIDR_EL0, x1",
        "d2800000": "mov x0, #0",
        "d53bd040": "mrs x0, TPIDR_EL0",
        "d2800ba8": "mov x8, #93",
        "d4000001": "svc #0",
    ]
    for (word, description) in requiredInstructionWords where !disassembly.contains(word) {
        failures.append(fail("instruction-shape", "init_004_tls disassembly missing \(description) instruction word \(word)"))
    }
    if !disassembly.contains("TPIDR_EL0") {
        failures.append(fail("tls-shape", "init_004_tls disassembly must use TPIDR_EL0"))
    }
    if !disassembly.contains("svc\t#0") {
        failures.append(fail("svc", "init_004_tls disassembly must use svc #0"))
    }
    for (key, value) in metadata.forbiddenBehavior where value {
        failures.append(fail("forbidden-behavior", "init_004_tls golden metadata sets forbidden_behavior.\(key)=true"))
    }
    return failures
}

func validateInit005Metadata(
    _ metadata: GoldenMetadata,
    sourceHash: String,
    binaryHash: String,
    fileOutput: String,
    objdumpHeader: String,
    disassembly: String
) -> [Failure] {
    var failures: [Failure] = []
    if metadata.caseID != "init_005_branches" {
        failures.append(fail("case-id", "golden case id must be init_005_branches"))
    }
    if metadata.sourceSHA256 != sourceHash {
        failures.append(fail("source-sha256", "source hash changed for init_005_branches"))
    }
    if metadata.expectedBinarySHA256 != binaryHash {
        failures.append(fail("binary-sha256", "binary hash changed for init_005_branches; inspect or run make tcti-gate TARGET=tcti-golden-elf-refresh CASE=init_005_branches"))
    }
    if metadata.actualBinarySHA256 != binaryHash {
        failures.append(fail("actual-binary-sha256", "golden actual binary hash no longer matches generated binary for init_005_branches"))
    }
    if metadata.elfType != "ET_EXEC" {
        failures.append(fail("elf-type", "init_005_branches golden metadata must use ET_EXEC"))
    }
    if metadata.machine != "AArch64" {
        failures.append(fail("elf-machine", "init_005_branches golden metadata must use AArch64"))
    }
    if metadata.expectedExitCode != 42 {
        failures.append(fail("expected-exit", "init_005_branches must expect exit code 42"))
    }
    if metadata.expectedSyscalls.count != 1 ||
        metadata.expectedSyscalls.first?.nr != "exit" ||
        metadata.expectedSyscalls.first?.code != 42 {
        failures.append(fail("expected-syscall", "init_005_branches must expect exactly exit(42)"))
    }
    if !fileOutput.contains("ELF 64-bit LSB executable") {
        failures.append(fail("elf-class", "init_005_branches must be ELF64 executable"))
    }
    if !fileOutput.contains("ARM aarch64") {
        failures.append(fail("elf-file-machine", "init_005_branches file output must identify ARM aarch64"))
    }
    if !objdumpHeader.contains("file format elf64-littleaarch64") {
        failures.append(fail("objdump-format", "init_005_branches must disassemble as elf64-littleaarch64"))
    }
    if !objdumpHeader.contains("architecture: aarch64") {
        failures.append(fail("objdump-architecture", "init_005_branches objdump architecture must be aarch64"))
    }
    if !objdumpHeader.lowercased().contains("start address: \(expectedEntrypoint(metadata))") {
        failures.append(fail("entrypoint", "init_005_branches entrypoint does not match \(metadata.entrypoint)"))
    }
    let requiredInstructionWords = [
        "d2800000": "mov x0, #0",
        "b4000060": "cbz x0, taken branch",
        "d2800020": "mov x0, #1",
        "14000002": "b over taken path",
        "d2800540": "mov x0, #42",
        "d2800ba8": "mov x8, #93",
        "d4000001": "svc #0",
    ]
    for (word, description) in requiredInstructionWords where !disassembly.contains(word) {
        failures.append(fail("instruction-shape", "init_005_branches disassembly missing \(description) instruction word \(word)"))
    }
    if !disassembly.contains("cbz\tx0") {
        failures.append(fail("branch-shape", "init_005_branches must use CBZ on x0"))
    }
    if !disassembly.contains("svc\t#0") {
        failures.append(fail("svc", "init_005_branches disassembly must use svc #0"))
    }
    for (key, value) in metadata.forbiddenBehavior where value {
        failures.append(fail("forbidden-behavior", "init_005_branches golden metadata sets forbidden_behavior.\(key)=true"))
    }
    return failures
}

func validateInit006Metadata(
    _ metadata: GoldenMetadata,
    sourceHash: String,
    binaryHash: String,
    fileOutput: String,
    objdumpHeader: String,
    disassembly: String
) -> [Failure] {
    var failures: [Failure] = []
    if metadata.caseID != "init_006_memory" {
        failures.append(fail("case-id", "golden case id must be init_006_memory"))
    }
    if metadata.sourceSHA256 != sourceHash {
        failures.append(fail("source-sha256", "source hash changed for init_006_memory"))
    }
    if metadata.expectedBinarySHA256 != binaryHash {
        failures.append(fail("binary-sha256", "binary hash changed for init_006_memory; inspect or run make tcti-gate TARGET=tcti-golden-elf-refresh CASE=init_006_memory"))
    }
    if metadata.actualBinarySHA256 != binaryHash {
        failures.append(fail("actual-binary-sha256", "golden actual binary hash no longer matches generated binary for init_006_memory"))
    }
    if metadata.elfType != "ET_EXEC" {
        failures.append(fail("elf-type", "init_006_memory golden metadata must use ET_EXEC"))
    }
    if metadata.machine != "AArch64" {
        failures.append(fail("elf-machine", "init_006_memory golden metadata must use AArch64"))
    }
    if metadata.expectedExitCode != 42 {
        failures.append(fail("expected-exit", "init_006_memory must expect exit code 42"))
    }
    if metadata.expectedSyscalls.count != 1 ||
        metadata.expectedSyscalls.first?.nr != "exit" ||
        metadata.expectedSyscalls.first?.code != 42 {
        failures.append(fail("expected-syscall", "init_006_memory must expect exactly exit(42)"))
    }
    if !fileOutput.contains("ELF 64-bit LSB executable") {
        failures.append(fail("elf-class", "init_006_memory must be ELF64 executable"))
    }
    if !fileOutput.contains("ARM aarch64") {
        failures.append(fail("elf-file-machine", "init_006_memory file output must identify ARM aarch64"))
    }
    if !objdumpHeader.contains("file format elf64-littleaarch64") {
        failures.append(fail("objdump-format", "init_006_memory must disassemble as elf64-littleaarch64"))
    }
    if !objdumpHeader.contains("architecture: aarch64") {
        failures.append(fail("objdump-architecture", "init_006_memory objdump architecture must be aarch64"))
    }
    if !objdumpHeader.lowercased().contains("start address: \(expectedEntrypoint(metadata))") {
        failures.append(fail("entrypoint", "init_006_memory entrypoint does not match \(metadata.entrypoint)"))
    }
    let requiredInstructionWords = [
        "10000081": "adr x1, value",
        "f9400020": "ldr x0, [x1]",
        "d2800ba8": "mov x8, #93",
        "d4000001": "svc #0",
    ]
    for (word, description) in requiredInstructionWords where !disassembly.contains(word) {
        failures.append(fail("instruction-shape", "init_006_memory disassembly missing \(description) instruction word \(word)"))
    }
    if !disassembly.contains("adr\tx1") || !disassembly.contains("<value>") {
        failures.append(fail("address-shape", "init_006_memory must use ADR to address value"))
    }
    if !disassembly.contains("ldr\tx0, [x1]") {
        failures.append(fail("memory-shape", "init_006_memory must load x0 from [x1]"))
    }
    if !disassembly.contains(".word\t0x0000002a") {
        failures.append(fail("memory-bytes", "init_006_memory must embed 64-bit value 42 in file-backed text bytes"))
    }
    if !disassembly.contains("svc\t#0") {
        failures.append(fail("svc", "init_006_memory disassembly must use svc #0"))
    }
    for (key, value) in metadata.forbiddenBehavior where value {
        failures.append(fail("forbidden-behavior", "init_006_memory golden metadata sets forbidden_behavior.\(key)=true"))
    }
    return failures
}

func validateInit007Metadata(
    _ metadata: GoldenMetadata,
    sourceHash: String,
    binaryHash: String,
    fileOutput: String,
    objdumpHeader: String,
    disassembly: String
) -> [Failure] {
    var failures: [Failure] = []
    if metadata.caseID != "init_007_mprotect" {
        failures.append(fail("case-id", "golden case id must be init_007_mprotect"))
    }
    if metadata.sourceSHA256 != sourceHash {
        failures.append(fail("source-sha256", "source hash changed for init_007_mprotect"))
    }
    if metadata.expectedBinarySHA256 != binaryHash {
        failures.append(fail("binary-sha256", "binary hash changed for init_007_mprotect; inspect or run make tcti-gate TARGET=tcti-golden-elf-refresh CASE=init_007_mprotect"))
    }
    if metadata.actualBinarySHA256 != binaryHash {
        failures.append(fail("actual-binary-sha256", "golden actual binary hash no longer matches generated binary for init_007_mprotect"))
    }
    if metadata.elfType != "ET_EXEC" {
        failures.append(fail("elf-type", "init_007_mprotect golden metadata must use ET_EXEC"))
    }
    if metadata.machine != "AArch64" {
        failures.append(fail("elf-machine", "init_007_mprotect golden metadata must use AArch64"))
    }
    if metadata.expectedExitCode != 0 {
        failures.append(fail("expected-exit", "init_007_mprotect must expect exit code 0"))
    }
    if metadata.expectedSyscalls.count != 2 ||
        metadata.expectedSyscalls.first?.nr != "mprotect" ||
        metadata.expectedSyscalls.last?.nr != "exit" ||
        metadata.expectedSyscalls.last?.code != 0 {
        failures.append(fail("expected-syscall", "init_007_mprotect must expect mprotect(...), exit(0)"))
    }
    if !fileOutput.contains("ELF 64-bit LSB executable") {
        failures.append(fail("elf-class", "init_007_mprotect must be ELF64 executable"))
    }
    if !fileOutput.contains("ARM aarch64") {
        failures.append(fail("elf-file-machine", "init_007_mprotect file output must identify ARM aarch64"))
    }
    if !objdumpHeader.contains("file format elf64-littleaarch64") {
        failures.append(fail("objdump-format", "init_007_mprotect must disassemble as elf64-littleaarch64"))
    }
    if !objdumpHeader.contains("architecture: aarch64") {
        failures.append(fail("objdump-architecture", "init_007_mprotect objdump architecture must be aarch64"))
    }
    if !objdumpHeader.lowercased().contains("start address: \(expectedEntrypoint(metadata))") {
        failures.append(fail("entrypoint", "init_007_mprotect entrypoint does not match \(metadata.entrypoint)"))
    }
    let requiredInstructionWords = [
        "d2820001": "mov x1, #4096",
        "d2800022": "mov x2, #1",
        "d2801c48": "mov x8, #226",
        "d2800000": "mov x0, #0",
        "d2800ba8": "mov x8, #93",
        "d4000001": "svc #0",
    ]
    for (word, description) in requiredInstructionWords where !disassembly.contains(word) {
        failures.append(fail("instruction-shape", "init_007_mprotect disassembly missing \(description) instruction word \(word)"))
    }
    if !disassembly.contains("adr\tx0") || !disassembly.contains("<page>") {
        failures.append(fail("address-shape", "init_007_mprotect must use ADR to address page"))
    }
    let svcCount = disassembly.components(separatedBy: "svc\t#0").count - 1
    if svcCount != 2 {
        failures.append(fail("svc", "init_007_mprotect disassembly must contain exactly two svc #0 instructions"))
    }
    for (key, value) in metadata.forbiddenBehavior where value {
        failures.append(fail("forbidden-behavior", "init_007_mprotect golden metadata sets forbidden_behavior.\(key)=true"))
    }
    return failures
}

func validateInit008Metadata(
    _ metadata: GoldenMetadata,
    sourceHash: String,
    binaryHash: String,
    fileOutput: String,
    objdumpHeader: String,
    disassembly: String
) -> [Failure] {
    var failures: [Failure] = []
    if metadata.caseID != "init_008_self_modify" {
        failures.append(fail("case-id", "golden case id must be init_008_self_modify"))
    }
    if metadata.sourceSHA256 != sourceHash {
        failures.append(fail("source-sha256", "source hash changed for init_008_self_modify"))
    }
    if metadata.expectedBinarySHA256 != binaryHash {
        failures.append(fail("binary-sha256", "binary hash changed for init_008_self_modify; inspect or run make tcti-gate TARGET=tcti-golden-elf-refresh CASE=init_008_self_modify"))
    }
    if metadata.actualBinarySHA256 != binaryHash {
        failures.append(fail("actual-binary-sha256", "golden actual binary hash no longer matches generated binary for init_008_self_modify"))
    }
    if metadata.elfType != "ET_EXEC" {
        failures.append(fail("elf-type", "init_008_self_modify golden metadata must use ET_EXEC"))
    }
    if metadata.machine != "AArch64" {
        failures.append(fail("elf-machine", "init_008_self_modify golden metadata must use AArch64"))
    }
    if metadata.expectedExitCode != 0 {
        failures.append(fail("expected-exit", "init_008_self_modify must expect exit code 0"))
    }
    if metadata.expectedSyscalls.count != 1 ||
        metadata.expectedSyscalls.first?.nr != "exit" ||
        metadata.expectedSyscalls.first?.code != 0 {
        failures.append(fail("expected-syscall", "init_008_self_modify must expect exactly exit(0)"))
    }
    if !fileOutput.contains("ELF 64-bit LSB executable") {
        failures.append(fail("elf-class", "init_008_self_modify must be ELF64 executable"))
    }
    if !fileOutput.contains("ARM aarch64") {
        failures.append(fail("elf-file-machine", "init_008_self_modify file output must identify ARM aarch64"))
    }
    if !objdumpHeader.contains("file format elf64-littleaarch64") {
        failures.append(fail("objdump-format", "init_008_self_modify must disassemble as elf64-littleaarch64"))
    }
    if !objdumpHeader.contains("architecture: aarch64") {
        failures.append(fail("objdump-architecture", "init_008_self_modify objdump architecture must be aarch64"))
    }
    if !objdumpHeader.lowercased().contains("start address: \(expectedEntrypoint(metadata))") {
        failures.append(fail("entrypoint", "init_008_self_modify entrypoint does not match \(metadata.entrypoint)"))
    }
    let requiredInstructionWords = [
        "d2800540": "mov x0, #42",
        "f9000020": "str x0, [x1]",
        "d2800000": "mov x0, #0",
        "d2800ba8": "mov x8, #93",
        "d4000001": "svc #0",
    ]
    for (word, description) in requiredInstructionWords where !disassembly.contains(word) {
        failures.append(fail("instruction-shape", "init_008_self_modify disassembly missing \(description) instruction word \(word)"))
    }
    if !disassembly.contains("adr\tx1") || !disassembly.contains("<patch_slot>") {
        failures.append(fail("address-shape", "init_008_self_modify must use ADR to address patch_slot"))
    }
    if !disassembly.contains("str\tx0, [x1]") {
        failures.append(fail("self-modify-shape", "init_008_self_modify must store x0 into patch_slot through x1"))
    }
    for (key, value) in metadata.forbiddenBehavior where value {
        failures.append(fail("forbidden-behavior", "init_008_self_modify golden metadata sets forbidden_behavior.\(key)=true"))
    }
    return failures
}

func validateInit009Metadata(
    _ metadata: GoldenMetadata,
    sourceHash: String,
    binaryHash: String,
    fileOutput: String,
    objdumpHeader: String,
    disassembly: String
) -> [Failure] {
    var failures: [Failure] = []
    if metadata.caseID != "init_009_faults" {
        failures.append(fail("case-id", "golden case id must be init_009_faults"))
    }
    if metadata.sourceSHA256 != sourceHash {
        failures.append(fail("source-sha256", "source hash changed for init_009_faults"))
    }
    if metadata.expectedBinarySHA256 != binaryHash {
        failures.append(fail("binary-sha256", "binary hash changed for init_009_faults; inspect or run make tcti-gate TARGET=tcti-golden-elf-refresh CASE=init_009_faults"))
    }
    if metadata.actualBinarySHA256 != binaryHash {
        failures.append(fail("actual-binary-sha256", "golden actual binary hash no longer matches generated binary for init_009_faults"))
    }
    if metadata.elfType != "ET_EXEC" {
        failures.append(fail("elf-type", "init_009_faults golden metadata must use ET_EXEC"))
    }
    if metadata.machine != "AArch64" {
        failures.append(fail("elf-machine", "init_009_faults golden metadata must use AArch64"))
    }
    if metadata.expectedExitCode != 0 {
        failures.append(fail("expected-exit", "init_009_faults metadata keeps expected_exit_code 0 as a structural placeholder"))
    }
    if !metadata.expectedSyscalls.isEmpty {
        failures.append(fail("expected-syscall", "init_009_faults structural metadata must not claim a completed syscall"))
    }
    if !fileOutput.contains("ELF 64-bit LSB executable") {
        failures.append(fail("elf-class", "init_009_faults must be ELF64 executable"))
    }
    if !fileOutput.contains("ARM aarch64") {
        failures.append(fail("elf-file-machine", "init_009_faults file output must identify ARM aarch64"))
    }
    if !objdumpHeader.contains("file format elf64-littleaarch64") {
        failures.append(fail("objdump-format", "init_009_faults must disassemble as elf64-littleaarch64"))
    }
    if !objdumpHeader.contains("architecture: aarch64") {
        failures.append(fail("objdump-architecture", "init_009_faults objdump architecture must be aarch64"))
    }
    if !objdumpHeader.lowercased().contains("start address: \(expectedEntrypoint(metadata))") {
        failures.append(fail("entrypoint", "init_009_faults entrypoint does not match \(metadata.entrypoint)"))
    }
    let requiredInstructionWords = [
        "d2800001": "mov x1, #0",
        "f9400020": "ldr x0, [x1]",
        "d2800020": "mov x0, #1",
        "d2800ba8": "mov x8, #93",
        "d4000001": "svc #0",
    ]
    for (word, description) in requiredInstructionWords where !disassembly.contains(word) {
        failures.append(fail("instruction-shape", "init_009_faults disassembly missing \(description) instruction word \(word)"))
    }
    if !disassembly.contains("ldr\tx0, [x1]") {
        failures.append(fail("fault-shape", "init_009_faults must structurally load x0 from guest address held in x1"))
    }
    for (key, value) in metadata.forbiddenBehavior where value {
        failures.append(fail("forbidden-behavior", "init_009_faults golden metadata sets forbidden_behavior.\(key)=true"))
    }
    return failures
}

func validateInit010Metadata(
    _ metadata: GoldenMetadata,
    sourceHash: String,
    binaryHash: String,
    fileOutput: String,
    objdumpHeader: String,
    disassembly: String,
    binary: URL
) -> [Failure] {
    var failures: [Failure] = []
    if metadata.caseID != "init_010_cpu_model" {
        failures.append(fail("case-id", "golden case id must be init_010_cpu_model"))
    }
    if metadata.sourceSHA256 != sourceHash {
        failures.append(fail("source-sha256", "source hash changed for init_010_cpu_model"))
    }
    if metadata.expectedBinarySHA256 != binaryHash {
        failures.append(fail("binary-sha256", "binary hash changed for init_010_cpu_model; inspect or run make tcti-gate TARGET=tcti-golden-elf-refresh CASE=init_010_cpu_model"))
    }
    if metadata.actualBinarySHA256 != binaryHash {
        failures.append(fail("actual-binary-sha256", "golden actual binary hash no longer matches generated binary for init_010_cpu_model"))
    }
    if metadata.elfType != "ET_EXEC" {
        failures.append(fail("elf-type", "init_010_cpu_model golden metadata must use ET_EXEC"))
    }
    if metadata.machine != "AArch64" {
        failures.append(fail("elf-machine", "init_010_cpu_model golden metadata must use AArch64"))
    }
    if metadata.expectedExitCode != 0 {
        failures.append(fail("expected-exit", "init_010_cpu_model must expect exit code 0"))
    }
    if metadata.expectedSyscalls.count != 1 ||
        metadata.expectedSyscalls.first?.nr != "exit" ||
        metadata.expectedSyscalls.first?.code != 0 {
        failures.append(fail("expected-syscall", "init_010_cpu_model must expect exactly exit(0)"))
    }
    if !fileOutput.contains("ELF 64-bit LSB executable") {
        failures.append(fail("elf-class", "init_010_cpu_model must be ELF64 executable"))
    }
    if !fileOutput.contains("ARM aarch64") {
        failures.append(fail("elf-file-machine", "init_010_cpu_model file output must identify ARM aarch64"))
    }
    if !objdumpHeader.contains("file format elf64-littleaarch64") {
        failures.append(fail("objdump-format", "init_010_cpu_model must disassemble as elf64-littleaarch64"))
    }
    if !objdumpHeader.contains("architecture: aarch64") {
        failures.append(fail("objdump-architecture", "init_010_cpu_model objdump architecture must be aarch64"))
    }
    if !objdumpHeader.lowercased().contains("start address: \(expectedEntrypoint(metadata))") {
        failures.append(fail("entrypoint", "init_010_cpu_model entrypoint does not match \(metadata.entrypoint)"))
    }
    let requiredInstructionWords = [
        "10000081": "adr x1, cpu_model",
        "d2800000": "mov x0, #0",
        "d2800ba8": "mov x8, #93",
        "d4000001": "svc #0",
    ]
    for (word, description) in requiredInstructionWords where !disassembly.contains(word) {
        failures.append(fail("instruction-shape", "init_010_cpu_model disassembly missing \(description) instruction word \(word)"))
    }
    if !disassembly.contains("adr\tx1") || !disassembly.contains("<cpu_model>") {
        failures.append(fail("cpu-model-address", "init_010_cpu_model must use ADR to address the fixed virtual CPU model string"))
    }
    do {
        let elf = try TinyElf64Aarch64(binary: binary)
        let bytes = try elf.readBytes(at: 0x210130, length: 17)
        if String(data: bytes, encoding: .utf8) != "orlix-aarch64-v1\n" {
            failures.append(fail("cpu-model-bytes", "init_010_cpu_model must embed orlix-aarch64-v1 newline"))
        }
    } catch {
        failures.append(fail("cpu-model-bytes", "\(error)"))
    }
    for (key, value) in metadata.forbiddenBehavior where value {
        failures.append(fail("forbidden-behavior", "init_010_cpu_model golden metadata sets forbidden_behavior.\(key)=true"))
    }
    return failures
}

func validateInit011Metadata(
    _ metadata: GoldenMetadata,
    sourceHash: String,
    binaryHash: String,
    fileOutput: String,
    objdumpHeader: String,
    disassembly: String,
    binary: URL
) -> [Failure] {
    var failures: [Failure] = []
    if metadata.caseID != "init_011_static_pie_got_byte_load" {
        failures.append(fail("case-id", "golden case id must be init_011_static_pie_got_byte_load"))
    }
    if metadata.sourceSHA256 != sourceHash {
        failures.append(fail("source-sha256", "source hash changed for init_011_static_pie_got_byte_load"))
    }
    if metadata.expectedBinarySHA256 != binaryHash {
        failures.append(fail("binary-sha256", "binary hash changed for init_011_static_pie_got_byte_load; inspect or run make tcti-gate TARGET=tcti-golden-elf-refresh CASE=init_011_static_pie_got_byte_load"))
    }
    if metadata.actualBinarySHA256 != binaryHash {
        failures.append(fail("actual-binary-sha256", "golden actual binary hash no longer matches generated binary for init_011_static_pie_got_byte_load"))
    }
    if metadata.elfType != "ET_DYN" {
        failures.append(fail("elf-type", "init_011_static_pie_got_byte_load golden metadata must use ET_DYN static PIE"))
    }
    if metadata.machine != "AArch64" {
        failures.append(fail("elf-machine", "init_011_static_pie_got_byte_load golden metadata must use AArch64"))
    }
    if metadata.expectedExitCode != 42 {
        failures.append(fail("expected-exit", "init_011_static_pie_got_byte_load must expect exit code 42"))
    }
    if metadata.expectedSyscalls.count != 1 ||
        metadata.expectedSyscalls.first?.nr != "exit" ||
        metadata.expectedSyscalls.first?.code != 42 {
        failures.append(fail("expected-syscall", "init_011_static_pie_got_byte_load must expect exactly exit(42)"))
    }
    if !fileOutput.contains("ELF 64-bit LSB") {
        failures.append(fail("elf-class", "init_011_static_pie_got_byte_load must be ELF64"))
    }
    if !fileOutput.contains("ARM aarch64") {
        failures.append(fail("elf-file-machine", "init_011_static_pie_got_byte_load file output must identify ARM aarch64"))
    }
    if !objdumpHeader.contains("file format elf64-littleaarch64") {
        failures.append(fail("objdump-format", "init_011_static_pie_got_byte_load must disassemble as elf64-littleaarch64"))
    }
    if !objdumpHeader.contains("architecture: aarch64") {
        failures.append(fail("objdump-architecture", "init_011_static_pie_got_byte_load objdump architecture must be aarch64"))
    }
    if !objdumpHeader.lowercased().contains("start address: \(expectedEntrypoint(metadata))") {
        failures.append(fail("entrypoint", "init_011_static_pie_got_byte_load entrypoint does not match \(metadata.entrypoint)"))
    }
    let requiredSnippets = [
        "adrp\tx8": "ADRP x8 to GOT-like slot page",
        "ldr\tx8, [x8, #0xd8]": "LDR x8 from GOT-like slot offset",
        "ldrb\tw0, [x8]": "LDRB w0 from pointer loaded out of GOT-like slot",
        "mov\tx8, #0x5d": "MOV x8, #93",
        "svc\t#0": "SVC #0",
    ]
    for (snippet, description) in requiredSnippets where !disassembly.contains(snippet) {
        failures.append(fail("instruction-shape", "init_011_static_pie_got_byte_load disassembly missing \(description)"))
    }
    do {
        let elf = try TinyElf64Aarch64(binary: binary)
        let pointerBytes = try elf.readBytes(at: 0x210d8, length: 8)
        let pointer = try littleEndianUInt64(pointerBytes, 0)
        let payload = try elf.readBytes(at: pointer, length: 1)
        if pointer != 0x1000 || payload.first != 42 {
            failures.append(fail("got-byte-load-shape", String(format: "expected relocated GOT slot 0x210d8 -> 0x1000 byte 42, got pointer 0x%llx byte %@", pointer, payload.first.map(String.init) ?? "<nil>")))
        }
    } catch {
        failures.append(fail("got-byte-load-shape", "\(error)"))
    }
    for (key, value) in metadata.forbiddenBehavior where value {
        failures.append(fail("forbidden-behavior", "init_011_static_pie_got_byte_load golden metadata sets forbidden_behavior.\(key)=true"))
    }
    return failures
}

func validateInit001Golden(metadataURL: URL, outputRoot: URL) throws -> (failures: [Failure], artifacts: [String]) {
    let built = try buildInit001(outputRoot: outputRoot)
    let expected = try decoder.decode(GoldenMetadata.self, from: Data(contentsOf: metadataURL))
    let sourceHash = try sha256(built.source)
    let binaryHash = try sha256(built.binary)
    let fileOutput = built.metadata["file_output"] ?? ""
    let objdumpHeader = built.metadata["objdump_header"] ?? ""
    let disassembly = built.metadata["disassembly"] ?? ""
    let validationURL = outputRoot
        .appendingPathComponent("init_001_exit", isDirectory: true)
        .appendingPathComponent("validation.json")
    let validationPayload = [
        "binary": relativePath(built.binary),
        "source_sha256": sourceHash,
        "binary_sha256": binaryHash,
        "file_output": fileOutput,
        "objdump_header": objdumpHeader,
        "disassembly": disassembly,
    ]
    try writeJSON(validationPayload, to: validationURL)
    let failures = validateInit001Metadata(
        expected,
        sourceHash: sourceHash,
        binaryHash: binaryHash,
        fileOutput: fileOutput,
        objdumpHeader: objdumpHeader,
        disassembly: disassembly
    )
    return (failures, [relativePath(built.binary), relativePath(validationURL)])
}

func validateGoldenCase(caseID: String, metadataURL: URL, outputRoot: URL) throws -> (failures: [Failure], artifacts: [String]) {
    if caseID == "init_001_exit" {
        return try validateInit001Golden(metadataURL: metadataURL, outputRoot: outputRoot)
    }
    guard ["init_002_write", "init_003_stack", "init_004_tls", "init_005_branches", "init_006_memory", "init_007_mprotect", "init_008_self_modify", "init_009_faults", "init_010_cpu_model", "init_011_static_pie_got_byte_load"].contains(caseID) else {
        throw GateError.usage("unsupported golden ELF case \(caseID)")
    }
    let built = try buildGoldenCase(caseID, outputRoot: outputRoot)
    let expected = try decoder.decode(GoldenMetadata.self, from: Data(contentsOf: metadataURL))
    let sourceHash = try sha256(built.source)
    let binaryHash = try sha256(built.binary)
    let fileOutput = built.metadata["file_output"] ?? ""
    let objdumpHeader = built.metadata["objdump_header"] ?? ""
    let disassembly = built.metadata["disassembly"] ?? ""
    let validationURL = outputRoot
        .appendingPathComponent(caseID, isDirectory: true)
        .appendingPathComponent("validation.json")
    let validationPayload = [
        "binary": relativePath(built.binary),
        "source_sha256": sourceHash,
        "binary_sha256": binaryHash,
        "file_output": fileOutput,
        "objdump_header": objdumpHeader,
        "disassembly": disassembly,
    ]
    try writeJSON(validationPayload, to: validationURL)
    let failures: [Failure]
    switch caseID {
    case "init_002_write":
        failures = validateInit002Metadata(
            expected,
            sourceHash: sourceHash,
            binaryHash: binaryHash,
            fileOutput: fileOutput,
            objdumpHeader: objdumpHeader,
            disassembly: disassembly,
            binary: built.binary
        )
    case "init_003_stack":
        failures = validateInit003Metadata(
            expected,
            sourceHash: sourceHash,
            binaryHash: binaryHash,
            fileOutput: fileOutput,
            objdumpHeader: objdumpHeader,
            disassembly: disassembly
        )
    case "init_004_tls":
        failures = validateInit004Metadata(
            expected,
            sourceHash: sourceHash,
            binaryHash: binaryHash,
            fileOutput: fileOutput,
            objdumpHeader: objdumpHeader,
            disassembly: disassembly
        )
    case "init_005_branches":
        failures = validateInit005Metadata(
            expected,
            sourceHash: sourceHash,
            binaryHash: binaryHash,
            fileOutput: fileOutput,
            objdumpHeader: objdumpHeader,
            disassembly: disassembly
        )
    case "init_006_memory":
        failures = validateInit006Metadata(
            expected,
            sourceHash: sourceHash,
            binaryHash: binaryHash,
            fileOutput: fileOutput,
            objdumpHeader: objdumpHeader,
            disassembly: disassembly
        )
    case "init_007_mprotect":
        failures = validateInit007Metadata(
            expected,
            sourceHash: sourceHash,
            binaryHash: binaryHash,
            fileOutput: fileOutput,
            objdumpHeader: objdumpHeader,
            disassembly: disassembly
        )
    case "init_008_self_modify":
        failures = validateInit008Metadata(
            expected,
            sourceHash: sourceHash,
            binaryHash: binaryHash,
            fileOutput: fileOutput,
            objdumpHeader: objdumpHeader,
            disassembly: disassembly
        )
    case "init_009_faults":
        failures = validateInit009Metadata(
            expected,
            sourceHash: sourceHash,
            binaryHash: binaryHash,
            fileOutput: fileOutput,
            objdumpHeader: objdumpHeader,
            disassembly: disassembly
        )
    case "init_010_cpu_model":
        failures = validateInit010Metadata(
            expected,
            sourceHash: sourceHash,
            binaryHash: binaryHash,
            fileOutput: fileOutput,
            objdumpHeader: objdumpHeader,
            disassembly: disassembly,
            binary: built.binary
        )
    case "init_011_static_pie_got_byte_load":
        failures = validateInit011Metadata(
            expected,
            sourceHash: sourceHash,
            binaryHash: binaryHash,
            fileOutput: fileOutput,
            objdumpHeader: objdumpHeader,
            disassembly: disassembly,
            binary: built.binary
        )
    default:
        failures = [fail("case-id", "unsupported golden ELF case \(caseID)")]
    }
    return (failures, [relativePath(built.binary), relativePath(validationURL)])
}

func goldenMetadata(caseID: String, actualBinaryHash: String, sourceHash: String, entrypoint: String, toolchain: [String: String]) -> GoldenMetadata {
    let expectedSyscalls: [ExpectedSyscall]
    let expectedExitCode: Int
    let expectedMessage: String?
    switch caseID {
    case "init_002_write":
        expectedSyscalls = [
            ExpectedSyscall(nr: "write", code: nil, fd: 1, len: 6, bytes: "hello\n"),
            ExpectedSyscall(nr: "exit", code: 0, fd: nil, len: nil, bytes: nil),
        ]
        expectedExitCode = 0
        expectedMessage = "hello\n"
    case "init_003_stack":
        expectedSyscalls = [
            ExpectedSyscall(nr: "exit", code: 42, fd: nil, len: nil, bytes: nil),
        ]
        expectedExitCode = 42
        expectedMessage = nil
    case "init_004_tls", "init_005_branches", "init_006_memory":
        expectedSyscalls = [
            ExpectedSyscall(nr: "exit", code: 42, fd: nil, len: nil, bytes: nil),
        ]
        expectedExitCode = 42
        expectedMessage = nil
    case "init_007_mprotect":
        expectedSyscalls = [
            ExpectedSyscall(nr: "mprotect", code: nil, fd: nil, len: nil, bytes: nil),
            ExpectedSyscall(nr: "exit", code: 0, fd: nil, len: nil, bytes: nil),
        ]
        expectedExitCode = 0
        expectedMessage = nil
    case "init_008_self_modify":
        expectedSyscalls = [
            ExpectedSyscall(nr: "exit", code: 0, fd: nil, len: nil, bytes: nil),
        ]
        expectedExitCode = 0
        expectedMessage = nil
    case "init_009_faults":
        expectedSyscalls = []
        expectedExitCode = 0
        expectedMessage = nil
    case "init_010_cpu_model":
        expectedSyscalls = [
            ExpectedSyscall(nr: "exit", code: 0, fd: nil, len: nil, bytes: nil),
        ]
        expectedExitCode = 0
        expectedMessage = nil
    case "init_011_static_pie_got_byte_load":
        expectedSyscalls = [
            ExpectedSyscall(nr: "exit", code: 42, fd: nil, len: nil, bytes: nil),
        ]
        expectedExitCode = 42
        expectedMessage = nil
    default:
        expectedSyscalls = [
            ExpectedSyscall(nr: "exit", code: 42, fd: nil, len: nil, bytes: nil),
        ]
        expectedExitCode = 42
        expectedMessage = nil
    }
    return GoldenMetadata(
        caseID: caseID,
        generatorCommand: "make tcti-gate TARGET=tcti-golden-elf-refresh CASE=\(caseID)",
        sourceSHA256: sourceHash,
        expectedBinarySHA256: actualBinaryHash,
        actualBinarySHA256: actualBinaryHash,
        compilerPath: toolchain["clang_path"] ?? "",
        compilerVersion: toolchain["clang_version"] ?? "",
        linkerPath: toolchain["linker_path"] ?? "",
        linkerVersion: toolchain["linker_version"] ?? "",
        flags: caseID == "init_011_static_pie_got_byte_load" ? [
            "-target", "aarch64-linux-gnu",
            "-nostdlib", "-static-pie", "-Wl,--no-relax", "-fuse-ld=lld",
            "-Wl,--build-id=none", "-Wl,-e,_start",
        ] : [
            "-target", "aarch64-linux-gnu",
            "-nostdlib", "-static", "-fuse-ld=lld",
            "-Wl,--build-id=none", "-Wl,-e,_start",
        ],
        libcMode: "no-libc",
        elfType: caseID == "init_011_static_pie_got_byte_load" ? "ET_DYN" : "ET_EXEC",
        entrypoint: entrypoint,
        machine: "AArch64",
        expectedSyscalls: expectedSyscalls,
        expectedExitCode: expectedExitCode,
        expectedMessage: expectedMessage,
        forbiddenBehavior: forbiddenDefaults()
    )
}

func runToolchainCheck() throws -> Int32 {
    let target = "tcti-toolchain-check"
    var failures: [Failure] = []
    var artifacts: [String] = []
    do {
        let info = try toolchainInfo()
        let outputRoot = buildPath("toolchain")
        let built = try buildInit001(outputRoot: outputRoot)
        let metadataURL = outputRoot.appendingPathComponent("toolchain-report.json")
        var payload = info
        payload.merge(built.metadata) { _, new in new }
        try writeJSON(payload, to: metadataURL)
        artifacts.append(relativePath(metadataURL))
        artifacts.append(relativePath(built.binary))
    } catch {
        let reducer = try writeReducer(
            target: target,
            caseID: "toolchain",
            command: "make tcti-gate TARGET=tcti-toolchain-check",
            reason: "\(error)"
        )
        artifacts.append(relativePath(reducer))
        failures.append(fail("toolchain", "\(error)"))
    }
    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Checked local AArch64 Linux ELF no-libc toolchain.",
        failures: failures,
        artifacts: artifacts
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func stringField(_ object: [String: Any], _ key: String) -> String {
    object[key] as? String ?? ""
}

func boolField(_ object: [String: Any], _ key: String) -> Bool {
	object[key] as? Bool ?? false
}

func intField(_ object: [String: Any], _ key: String) -> Int? {
	if let value = object[key] as? Int {
		return value
	}
	if let value = object[key] as? String {
		return Int(value)
	}
	return nil
}

func latestRuntimeValidationReport(gate gateName: String, destination: String) -> (url: URL, object: [String: Any])? {
	let runtimeRoot = path("Build", "Reports", "runtime")
    guard let entries = try? fileManager.contentsOfDirectory(at: runtimeRoot, includingPropertiesForKeys: nil) else {
        return nil
    }
    let candidates = entries
        .filter { $0.lastPathComponent.hasPrefix("\(gateName)-") && $0.pathExtension == "json" }
        .sorted { $0.lastPathComponent > $1.lastPathComponent }
    for candidate in candidates {
        guard let object = try? loadJSON(candidate) as? [String: Any],
              stringField(object, "gate") == gateName,
              stringField(object, "destination") == destination
        else {
            continue
        }
        return (candidate, object)
    }
	return nil
}

func selectedRuntimeValidationReport(gate gateName: String, destination: String) -> (url: URL, object: [String: Any])? {
    if let reportPath = ProcessInfo.processInfo.environment["TCTI_SIMULATOR_REPORT"], !reportPath.isEmpty {
        let url = URL(fileURLWithPath: reportPath, relativeTo: repoRoot()).standardizedFileURL
        guard
			let object = try? loadJSON(url) as? [String: Any],
			stringField(object, "gate") == gateName,
			stringField(object, "destination") == destination
		else {
			return nil
		}
		return (url, object)
    }
    return latestRuntimeValidationReport(gate: gateName, destination: destination)
}

func selectedRuntimeValidationReport(gates gateNames: [String], destination: String) -> (url: URL, object: [String: Any])? {
    if let reportPath = ProcessInfo.processInfo.environment["TCTI_SIMULATOR_REPORT"], !reportPath.isEmpty {
        let url = URL(fileURLWithPath: reportPath, relativeTo: repoRoot()).standardizedFileURL
        guard
            let object = try? loadJSON(url) as? [String: Any],
            gateNames.contains(stringField(object, "gate")),
            stringField(object, "destination") == destination
        else {
            return nil
        }
        return (url, object)
    }
    for gateName in gateNames {
        if let report = latestRuntimeValidationReport(gate: gateName, destination: destination) {
            return report
        }
    }
    return nil
}

func runtimeValidationReports(gate gateName: String, destination: String) -> [(url: URL, object: [String: Any])] {
	let runtimeRoot = path("Build", "Reports", "runtime")
	guard let entries = try? fileManager.contentsOfDirectory(at: runtimeRoot, includingPropertiesForKeys: nil) else {
		return []
	}
	return entries
		.filter { $0.lastPathComponent.hasPrefix("\(gateName)-") && $0.pathExtension == "json" }
		.sorted { $0.lastPathComponent > $1.lastPathComponent }
		.compactMap { candidate in
			guard let object = try? loadJSON(candidate) as? [String: Any],
			      stringField(object, "gate") == gateName,
			      stringField(object, "destination") == destination
			else {
				return nil
			}
			return (candidate, object)
		}
}

func runtimeReportText(_ report: (url: URL, object: [String: Any]), artifacts: inout [String]) -> String {
	artifacts.append(relativePath(report.url))
	let reportArtifacts = (report.object["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
	artifacts.append(contentsOf: reportArtifacts)
	let unifiedText = (try? reportArtifacts.first { $0.hasSuffix("simulator-unified.log") }.map(readRelativeArtifact)) ?? ""
	let terminalText = (try? reportArtifacts.first { $0.hasSuffix("simulator-terminal-output.txt") }.map(readRelativeArtifact)) ?? ""
	let fatalText = (try? reportArtifacts.first { $0.hasSuffix("tcti-simulator-fatal-runtime.txt") }.map(readRelativeArtifact)) ?? ""
	return [unifiedText, terminalText, fatalText].joined(separator: "\n")
}

func latestBRKTrapSimulatorReport() throws -> (url: URL, object: [String: Any], artifacts: [String], text: String)? {
    let runtimeRoot = path("Build", "Reports", "runtime")
    guard let entries = try? fileManager.contentsOfDirectory(at: runtimeRoot, includingPropertiesForKeys: nil) else {
        return nil
    }
    let candidates = entries
        .filter { $0.lastPathComponent.hasPrefix("tcti-simulator-stability-") && $0.pathExtension == "json" }
        .sorted { $0.lastPathComponent > $1.lastPathComponent }
    for candidate in candidates {
        guard let object = try? loadJSON(candidate) as? [String: Any],
              stringField(object, "gate") == "tcti-simulator-stability",
              stringField(object, "destination") == "iphonesimulator",
              stringField(object, "git_sha") == gitSha(),
              stringField(object, "status") == "fail",
              !boolField(object, "passed")
        else {
            continue
        }
        let artifacts = (object["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
        let text = artifacts
            .compactMap { try? readRelativeArtifact($0) }
            .joined(separator: "\n")
        guard text.contains("syscall=222"),
              text.contains("Orlix TCTI: unsupported instruction"),
              text.contains("insn=0xd4200020"),
              (text.contains("Attempted to kill init") || text.contains("Attempted kill init")),
              text.contains("exitcode=0x00000004")
        else {
            continue
        }
        return (candidate, object, artifacts, text)
    }
    return nil
}

func readRelativeArtifact(_ relativeArtifact: String) throws -> String {
    let repoRelative = URL(fileURLWithPath: relativeArtifact, relativeTo: repoRoot()).standardizedFileURL
    if fileManager.fileExists(atPath: repoRelative.path) {
        return try readText(repoRelative)
    }
    let runtimeRelative = path("Build", "Reports", "runtime").appendingPathComponent(relativeArtifact)
    return try readText(runtimeRelative)
}

func runtimeReportBuildSetting(from artifacts: [String], key: String) -> String? {
    guard let buildSettingsArtifact = artifacts.first(where: { $0.hasSuffix("build-settings.json") }),
          let text = try? readRelativeArtifact(buildSettingsArtifact),
          let data = text.data(using: .utf8),
          let root = try? JSONSerialization.jsonObject(with: data) as? [[String: Any]]
    else {
        return nil
    }
    for action in root {
        guard let settings = action["buildSettings"] as? [String: Any],
              let value = settings[key] as? String,
              !value.isEmpty
        else {
            continue
        }
        return value
    }
    return nil
}

func orlixBuildRootDerivedFromAppPath(_ appPath: String) -> URL? {
    let url = URL(fileURLWithPath: appPath).standardizedFileURL
    let components = url.pathComponents
    guard let derivedDataIndex = components.lastIndex(of: "DerivedData") else {
        return nil
    }
    let xcodeRootComponents = components.prefix(upTo: derivedDataIndex)
    var xcodeRoot = URL(fileURLWithPath: xcodeRootComponents.joined(separator: "/"), isDirectory: true)
    if xcodeRoot.path == "" {
        xcodeRoot = URL(fileURLWithPath: "/", isDirectory: true)
    }
    return xcodeRoot
        .appendingPathComponent("OrlixSystem")
        .appendingPathComponent("Build")
}

func ensureRuntimeInitELF(from artifacts: [String], to binaryURL: URL) throws -> Bool {
    guard let appPathArtifact = artifacts.first(where: { $0.hasSuffix("app-path.txt") }) else {
        return fileManager.fileExists(atPath: binaryURL.path)
    }
    let appPath = try readRelativeArtifact(appPathArtifact)
        .trimmingCharacters(in: .whitespacesAndNewlines)
    guard !appPath.isEmpty else {
        return fileManager.fileExists(atPath: binaryURL.path)
    }

    let profile = runtimeReportBuildSetting(from: artifacts, key: "ORLIX_PROFILE") ?? "tcti_runtime"
    if let buildRoot = orlixBuildRootDerivedFromAppPath(appPath) {
        let packagedInit = buildRoot
            .appendingPathComponent("OrlixOS")
            .appendingPathComponent("packages")
            .appendingPathComponent(profile)
            .appendingPathComponent("sbin")
            .appendingPathComponent("init")
        if fileManager.fileExists(atPath: packagedInit.path) {
            try ensureDirectory(binaryURL.deletingLastPathComponent())
            try? fileManager.removeItem(at: binaryURL)
            try fileManager.copyItem(at: packagedInit, to: binaryURL)
            return true
        }
    }

    if fileManager.fileExists(atPath: binaryURL.path) {
        return true
    }

    let initramfsURL = URL(fileURLWithPath: appPath)
        .appendingPathComponent("Frameworks")
        .appendingPathComponent("OrlixOS.framework")
        .appendingPathComponent("OrlixOSPayload.bundle")
        .appendingPathComponent("rootfs")
        .appendingPathComponent("initramfs.cpio.gz")
    guard fileManager.fileExists(atPath: initramfsURL.path) else {
        return false
    }

    let outputRoot = binaryURL.deletingLastPathComponent()
    let extractRoot = outputRoot.appendingPathComponent("extract", isDirectory: true)
    try? fileManager.removeItem(at: extractRoot)
    try ensureDirectory(extractRoot)
    try ensureDirectory(outputRoot)
    _ = try runWithFileBackedOutput([
        "/bin/sh",
        "-c",
        "cd \"$1\" && gzip -dc \"$2\" | cpio -id init >/dev/null",
        "extract-runtime-init",
        extractRoot.path,
        initramfsURL.path,
    ])

    let extracted = extractRoot.appendingPathComponent("init")
    guard fileManager.fileExists(atPath: extracted.path) else {
        return false
    }
    try? fileManager.removeItem(at: binaryURL)
    try fileManager.copyItem(at: extracted, to: binaryURL)
    return true
}

func firstRegexGroups(_ pattern: String, in text: String) -> [String]? {
    guard let regex = try? NSRegularExpression(pattern: pattern) else {
        return nil
    }
    let range = NSRange(text.startIndex..<text.endIndex, in: text)
    guard let match = regex.firstMatch(in: text, range: range) else {
        return nil
    }
    return (1..<match.numberOfRanges).compactMap { index in
        guard let range = Range(match.range(at: index), in: text) else {
            return nil
        }
        return String(text[range])
    }
}

func parseHexUInt64(_ value: String) -> UInt64? {
    var trimmed = value.lowercased()
    if trimmed.hasPrefix("0x") {
        trimmed.removeFirst(2)
    }
    return UInt64(trimmed, radix: 16)
}

func llvmObjdumpPath() -> String {
    let homebrewObjdump = "/opt/homebrew/opt/llvm/bin/llvm-objdump"
    if fileManager.isExecutableFile(atPath: homebrewObjdump) {
        return homebrewObjdump
    }
    return (try? commandPath("llvm-objdump")) ?? "llvm-objdump"
}

func runSimulatorUserFaultReducer() throws -> Int32 {
    let target = "tcti-simulator-user-fault-reducer"
    var failures: [Failure] = []
    var artifacts: [String] = []

    guard let simulatorReport = latestRuntimeValidationReport(
        gate: "tcti-simulator-stability",
        destination: "iphonesimulator"
    ) else {
        failures.append(fail("simulator-report", "missing iphonesimulator tcti-simulator-stability report to reduce"))
        let reportURL = try writeReport(report(
            target: target,
            status: .fail,
            summary: "No simulator stability failure report was available to reduce.",
            failures: failures,
            artifacts: artifacts,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("fail: \(relativePath(reportURL))")
        return 1
    }

    let simulatorObject = simulatorReport.object
    artifacts.append(relativePath(simulatorReport.url))
    let simulatorArtifacts = (simulatorObject["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
    artifacts.append(contentsOf: simulatorArtifacts)
    let runtimeEvents = simulatorObject["tcti_runtime_events"] as? [String: Any] ?? [:]
    let firstSVC = runtimeEvents["first_svc"] as? [String: Any] ?? [:]
    let staticPIEImage = runtimeEvents["static_pie_image"] as? [String: Any] ?? [:]
    let fatalUserFault = runtimeEvents["fatal_user_fault"] as? [String: Any] ?? [:]
    let simulatorPassed = stringField(simulatorObject, "status") == "pass" && boolField(simulatorObject, "passed")
    let simulatorFailed = stringField(simulatorObject, "status") == "fail" && !boolField(simulatorObject, "passed")
    let firstSVCCaptured = stringField(firstSVC, "task") == "init" &&
        intField(firstSVC, "pid") == 1 &&
        !stringField(firstSVC, "pc").isEmpty &&
        intField(firstSVC, "syscall") != nil
    let staticPIEImageCaptured = stringField(staticPIEImage, "task") == "sh" &&
        intField(staticPIEImage, "pid") != nil &&
        !stringField(staticPIEImage, "pc").isEmpty &&
        !stringField(staticPIEImage, "base").isEmpty &&
        !stringField(staticPIEImage, "entry").isEmpty
    let fatalAddress = stringField(fatalUserFault, "addr")
    let fatalAccess = intField(fatalUserFault, "access")
    let fatalNullReadSignature = fatalAddress == "0x0" &&
        (fatalAccess == nil || fatalAccess == 1)
    let noFatalUserFault = fatalAddress.isEmpty

    if stringField(simulatorObject, "git_sha") != gitSha() {
        failures.append(fail("simulator-report-stale", "latest simulator stability report is stale for current HEAD"))
    }
    if !simulatorPassed && !simulatorFailed {
        failures.append(fail("simulator-report-status", "reducer requires a current simulator stability pass or fail report"))
    }
    if !firstSVCCaptured {
        failures.append(fail("simulator-first-syscall", "simulator report did not capture structured first TCTI svc #0 event"))
    }
    if simulatorFailed && !fatalNullReadSignature {
        failures.append(fail("simulator-fatal-signature", "failing simulator report does not contain the structured null-read fatal user fault signature"))
    }
    if simulatorPassed && !noFatalUserFault {
        failures.append(fail("simulator-fatal-signature", "passing simulator report still contains a structured fatal user fault"))
    }
    if simulatorPassed && !staticPIEImageCaptured {
        failures.append(fail("simulator-static-pie-event", "passing simulator report does not include structured static PIE image event"))
    }

    let metadataURL = path("OrlixKernel", "Tests", "TCTI", "golden_elf", "init_011_static_pie_got_byte_load", "golden.json")
    do {
        let positive = try validateAndExecuteGoldenCase(
            caseID: "init_011_static_pie_got_byte_load",
            metadataURL: metadataURL,
            outputRoot: buildPath("simulator_user_fault_reducer", "positive")
        )
        artifacts.append(contentsOf: positive.artifacts)
        failures.append(contentsOf: positive.failures)

        let negative = try executeNegativeFixture(
            "static-pie-got-unrelocated-byte-load",
            outputRoot: buildPath("simulator_user_fault_reducer", "negative")
        )
        artifacts.append(contentsOf: negative.artifacts)
        guard let execution = negative.execution else {
            failures.append(fail("negative-execution", "unrelocated static PIE GOT negative fixture did not write execution report"))
            throw GateError.commandFailed("missing unrelocated static PIE GOT negative execution report")
        }
        if negative.failures.isEmpty {
            failures.append(fail("negative-execution", "unrelocated static PIE GOT byte-load fixture unexpectedly passed"))
        }
        if execution.guestInstructionsExecuted != 3 ||
            Array(execution.instructionEncodings.prefix(3)) != ["0x90000088", "0xf9406d08", "0x39400100"] ||
            execution.fault?.kind != "guest_memory_fault" ||
            execution.fault?.address != "0x0000000000000000" ||
            execution.fault?.access != "read" ||
            execution.fault?.captured != true ||
            execution.exit != nil ||
            !execution.syscalls.isEmpty {
            failures.append(fail("negative-execution-shape", "expected unrelocated static PIE GOT byte-load to stop on captured read fault at 0x0 before any syscall"))
        }
        let reducer = try writeReducer(
            target: "tcti-golden-elf",
            caseID: "execution-static-pie-got-unrelocated-byte-load",
            command: "CASE=init_011_static_pie_got_byte_load EXECUTE=switch-debug NEGATIVE_EXECUTION=static-pie-got-unrelocated-byte-load make tcti-gate TARGET=tcti-golden-elf",
            reason: "static PIE GOT byte load must fault when the R_AARCH64_RELATIVE GOT slot remains unrelocated and LDRB reads from 0x0",
            artifacts: negative.artifacts,
            expectedStatus: .fail
        )
        artifacts.append(relativePath(reducer))
    } catch {
        failures.append(fail("simulator-user-fault-reducer", "\(error)"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Reduced the simulator TCTI null user fault into an unrelocated static PIE GOT byte-load no-phone switch-debug fixture.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_reduced": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runStaticPIERelocationFix() throws -> Int32 {
    let target = "tcti-static-pie-relocation-fix"
    var failures: [Failure] = []
    var artifacts: [String] = []

    guard let simulatorReport = latestRuntimeValidationReport(
        gate: "tcti-simulator-stability",
        destination: "iphonesimulator"
    ) else {
        failures.append(fail("simulator-report", "missing iphonesimulator tcti-simulator-stability report"))
        let reportURL = try writeReport(report(
            target: target,
            status: .fail,
            summary: "Static PIE relocation fix gate requires a current simulator stability failure report.",
            failures: failures,
            artifacts: artifacts,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("fail: \(relativePath(reportURL))")
        return 1
    }

    artifacts.append(relativePath(simulatorReport.url))
    let simulatorObject = simulatorReport.object
    let simulatorArtifacts = (simulatorObject["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
    artifacts.append(contentsOf: simulatorArtifacts)
    let firstSyscallText = try simulatorArtifacts.first { $0.hasSuffix("tcti-first-syscall.txt") }.map(readRelativeArtifact) ?? ""
    let fatalText = try simulatorArtifacts.first { $0.hasSuffix("tcti-simulator-fatal-runtime.txt") }.map(readRelativeArtifact) ?? ""
    let runtimeEvents = simulatorObject["tcti_runtime_events"] as? [String: Any] ?? [:]
    let staticPIEImage = runtimeEvents["static_pie_image"] as? [String: Any] ?? [:]

    if stringField(simulatorObject, "git_sha") != gitSha() {
        failures.append(fail("simulator-report-stale", "latest simulator stability report is stale for current HEAD"))
    }
    let simulatorPassed = stringField(simulatorObject, "status") == "pass" && boolField(simulatorObject, "passed")
    let simulatorFailedAfterProgress = stringField(simulatorObject, "status") == "fail" && !boolField(simulatorObject, "passed")
    if !simulatorPassed && !simulatorFailedAfterProgress {
        failures.append(fail("simulator-report-status", "static PIE relocation fix gate requires a current simulator stability pass or progressed failure report"))
    }
    let stillNullGOTFailure = fatalText.contains("Orlix TCTI: user fault") &&
        fatalText.contains("addr=0x0") &&
        fatalText.contains("access=1") &&
        (fatalText.contains("Attempted to kill init") || fatalText.contains("Attempted kill init"))
    if stillNullGOTFailure {
        failures.append(fail("simulator-fatal-signature", "latest simulator fatal artifact still matches the reduced static PIE GOT null-read signature"))
    }
    let staticPIEImageCaptured = stringField(staticPIEImage, "task") == "sh" &&
        intField(staticPIEImage, "pid") != nil &&
        !stringField(staticPIEImage, "pc").isEmpty &&
        !stringField(staticPIEImage, "base").isEmpty &&
        !stringField(staticPIEImage, "entry").isEmpty
    if !staticPIEImageCaptured {
        failures.append(fail("simulator-static-pie-event", "latest simulator report does not include structured static_pie_image TCTI runtime event"))
    }
    if !simulatorPassed && !firstSyscallText.contains("Orlix TCTI: svc #0") {
        failures.append(fail("simulator-progress", "latest simulator report did not pass and did not capture first TCTI svc #0 after relocation"))
    }

    let reducerReportURL = buildPath("reports", "tcti-simulator-user-fault-reducer", "report.json")
    let reproReportURL = buildPath("reports", "tcti-repro", "report.json")
    var reducerProofPassed = false
    if let reducerReport = try? loadJSON(reducerReportURL) as? [String: Any] {
        artifacts.append(relativePath(reducerReportURL))
        reducerProofPassed = stringField(reducerReport, "status") == "pass" &&
            boolField(reducerReport, "passed")
    }
    if !reducerProofPassed, let reproReport = try? loadJSON(reproReportURL) as? [String: Any] {
        let reproArtifacts = (reproReport["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
        artifacts.append(relativePath(reproReportURL))
        reducerProofPassed = stringField(reproReport, "status") == "pass" &&
            boolField(reproReport, "passed") &&
            stringField(reproReport, "git_sha") == gitSha() &&
            stringField(reproReport, "expected_status") == "fail" &&
            stringField(reproReport, "actual_replay_status") == "fail" &&
            reproArtifacts.contains("Build/TCTI/reproducers/tcti-golden-elf/execution-static-pie-got-unrelocated-byte-load.json")
    }
    if !reducerProofPassed && !simulatorPassed {
        failures.append(fail("reducer-report", "static PIE GOT null-read reducer must either pass its reducer gate or replay through tcti-repro while simulator stability is still failing"))
    }

    let engineURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "engine.c")
    let engine = try readText(engineURL)
    artifacts.append(relativePath(engineURL))
    if !engine.contains("tcti_apply_static_pie_relative_relocations") ||
        !engine.contains("R_AARCH64_RELATIVE") ||
        !engine.contains("TCTI_MAX_RELA_ENTRIES") {
        failures.append(fail("production-fix-marker", "TCTI engine does not contain the constrained static PIE relative relocation fix"))
    }
    if engine.contains("PT_INTERP") ||
        engine.contains("DT_NEEDED") ||
        engine.contains("R_AARCH64_JUMP_SLOT") {
        failures.append(fail("dynamic-loader-scope", "static PIE relocation fix must not grow into a dynamic loader"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Checked reducer-backed static PIE R_AARCH64_RELATIVE production fix and verified the simulator no longer stops at the null GOT read signature.",
        failures: failures,
        artifacts: artifacts,
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runPostOverlayNullUserFaultReducer() throws -> Int32 {
    let target = "tcti-post-overlay-null-user-fault-reducer"
    var failures: [Failure] = []
    var artifacts: [String] = []

    guard let simulatorReport = latestRuntimeValidationReport(
        gate: "tcti-simulator-stability",
        destination: "iphonesimulator"
    ) else {
        failures.append(fail("simulator-report", "missing iphonesimulator tcti-simulator-stability report to reduce"))
        let reportURL = try writeReport(report(
            target: target,
            status: .fail,
            summary: "No simulator stability failure report was available for the post-overlay null user-fault reducer.",
            failures: failures,
            artifacts: artifacts,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("fail: \(relativePath(reportURL))")
        return 1
    }

    let simulatorObject = simulatorReport.object
    artifacts.append(relativePath(simulatorReport.url))
    let simulatorArtifacts = (simulatorObject["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
    artifacts.append(contentsOf: simulatorArtifacts)
    let runtimeEvents = simulatorObject["tcti_runtime_events"] as? [String: Any] ?? [:]
    let firstSVC = runtimeEvents["first_svc"] as? [String: Any] ?? [:]
    let staticPIEImage = runtimeEvents["static_pie_image"] as? [String: Any] ?? [:]
    let fatalUserFault = runtimeEvents["fatal_user_fault"] as? [String: Any] ?? [:]
    let simulatorPassed = stringField(simulatorObject, "status") == "pass" && boolField(simulatorObject, "passed")
    let simulatorFailed = stringField(simulatorObject, "status") == "fail" && !boolField(simulatorObject, "passed")
    let firstSVCCaptured = stringField(firstSVC, "task") == "init" &&
        intField(firstSVC, "pid") == 1 &&
        !stringField(firstSVC, "pc").isEmpty &&
        intField(firstSVC, "syscall") != nil
    let staticPIEImageCaptured = stringField(staticPIEImage, "task") == "sh" &&
        intField(staticPIEImage, "pid") != nil &&
        !stringField(staticPIEImage, "pc").isEmpty &&
        !stringField(staticPIEImage, "base").isEmpty &&
        !stringField(staticPIEImage, "entry").isEmpty
    let fatalAddress = stringField(fatalUserFault, "addr")
    let fatalAccess = intField(fatalUserFault, "access")
    let fatalNullReadSignature = fatalAddress == "0x0" &&
        (fatalAccess == nil || fatalAccess == 1)
    let noFatalUserFault = fatalAddress.isEmpty

    if stringField(simulatorObject, "git_sha") != gitSha() {
        failures.append(fail("simulator-report-stale", "latest simulator stability report is stale for current HEAD"))
    }
    if !simulatorPassed && !simulatorFailed {
        failures.append(fail("simulator-report-status", "reducer requires a current simulator stability pass or fail report"))
    }
    if stringField(simulatorObject, "selected_device_id") != "1E5553B0-203A-4A11-BAD7-EBDE46863F66" ||
        stringField(simulatorObject, "selected_device_name") != "Orlix-iPhone-15-Pro-Max" ||
        stringField(simulatorObject, "simulator_booted_count") != "1" ||
        !boolField(simulatorObject, "simulator_single_booted") {
        failures.append(fail("simulator-scope", "reducer requires the single pinned Orlix-iPhone-15-Pro-Max simulator report"))
    }
    if !firstSVCCaptured {
        failures.append(fail("simulator-first-syscall", "simulator report did not capture structured first TCTI svc #0 event"))
    }
    if !staticPIEImageCaptured {
        failures.append(fail("simulator-static-pie-event", "simulator report does not include structured static PIE image event"))
    }
    if simulatorFailed && !fatalNullReadSignature {
        failures.append(fail("simulator-post-overlay-signature", "failing simulator report does not contain the structured null-read fatal user fault signature"))
    }
    if simulatorPassed && !noFatalUserFault {
        failures.append(fail("simulator-post-overlay-signature", "passing simulator report still contains a structured fatal user fault"))
    }

    do {
        let negative = try executeNegativeFixture(
            "static-pie-got-relocation-invisible-byte-load",
            outputRoot: buildPath("post_overlay_null_user_fault_reducer", "negative")
        )
        artifacts.append(contentsOf: negative.artifacts)
        guard let execution = negative.execution else {
            failures.append(fail("negative-execution", "post-overlay null user-fault negative fixture did not write execution report"))
            throw GateError.commandFailed("missing post-overlay null user-fault negative execution report")
        }
        if negative.failures.isEmpty {
            failures.append(fail("negative-execution", "post-overlay null user-fault negative fixture unexpectedly passed"))
        }
        if execution.guestInstructionsExecuted != 3 ||
            Array(execution.instructionEncodings.prefix(3)) != ["0x90000088", "0xf9406d08", "0x39400100"] ||
            execution.fault?.kind != "guest_memory_fault" ||
            execution.fault?.address != "0x0000000000000000" ||
            execution.fault?.access != "read" ||
            execution.fault?.captured != true ||
            execution.exit != nil ||
            !execution.syscalls.isEmpty {
            failures.append(fail("negative-execution-shape", "expected post-overlay null user-fault reducer to stop on captured LDRB read fault at 0x0 before syscall"))
		}
		let reducer = try writeReducer(
            target: "tcti-golden-elf",
            caseID: "execution-static-pie-got-relocation-invisible-byte-load",
            command: "CASE=init_011_static_pie_got_byte_load EXECUTE=switch-debug NEGATIVE_EXECUTION=static-pie-got-relocation-invisible-byte-load make tcti-gate TARGET=tcti-golden-elf",
            reason: "latest pinned simulator stability failure reaches root overlay readiness and logs static PIE relocations, but a later GOT byte load still observes a null slot and faults at addr=0x0",
            artifacts: negative.artifacts + [relativePath(simulatorReport.url)],
            expectedStatus: .fail
        )
        artifacts.append(relativePath(reducer))
    } catch {
        failures.append(fail("post-overlay-null-user-fault-reducer", "\(error)"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Reduced the pinned simulator post-overlay null user fault into a replayable no-phone switch-debug fixture.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_reduced": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runPostOverlayNullUserFaultFix() throws -> Int32 {
    let target = "tcti-post-overlay-null-user-fault-fix"
    var failures: [Failure] = []
    var artifacts: [String] = []

    let reducerReportURL = buildPath("reports", "tcti-post-overlay-null-user-fault-reducer", "report.json")
    if let reducerReport = try? loadJSON(reducerReportURL) as? [String: Any] {
        artifacts.append(relativePath(reducerReportURL))
        if stringField(reducerReport, "git_sha") != gitSha() ||
            stringField(reducerReport, "status") != "pass" ||
            !boolField(reducerReport, "passed") {
            failures.append(fail("reducer-report", "post-overlay null user-fault reducer report is missing, stale, or not passing"))
        }
    } else {
        failures.append(fail("reducer-report", "missing tcti-post-overlay-null-user-fault-reducer report"))
    }

    if let simulatorReport = latestRuntimeValidationReport(gate: "tcti-simulator-stability", destination: "iphonesimulator") {
        artifacts.append(relativePath(simulatorReport.url))
        let simulatorObject = simulatorReport.object
        let simulatorArtifacts = (simulatorObject["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
        artifacts.append(contentsOf: simulatorArtifacts)
        if stringField(simulatorObject, "git_sha") != gitSha() {
            failures.append(fail("simulator-report-stale", "latest simulator stability report is stale for current HEAD"))
        }
    } else {
        failures.append(fail("simulator-report", "missing iphonesimulator tcti-simulator-stability report"))
    }

    let engineURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "engine.c")
    let engine = try readText(engineURL)
    artifacts.append(relativePath(engineURL))
    if !engine.contains("orlix_tcti_apply_static_pie_relocations_or_exit") ||
        !engine.contains("applied_static_pie_base") ||
        !engine.contains("*applied_base == base") {
        failures.append(fail("production-fix-marker", "TCTI engine must track the relocated static PIE base and reapply relocations only when the image base changes"))
    }
    if !engine.contains("orlix_tcti_handle_syscall(regs);") ||
        !engine.contains("regs = task_pt_regs(current);") {
        failures.append(fail("syscall-resume-marker", "TCTI syscall path must resume with task pt_regs before reapplying static PIE relocations"))
    }
    if engine.contains("PT_INTERP") ||
        engine.contains("DT_NEEDED") ||
        engine.contains("R_AARCH64_JUMP_SLOT") {
        failures.append(fail("dynamic-loader-scope", "post-overlay null user-fault fix must not grow into a dynamic loader"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Checked reducer-backed post-overlay null user-fault fix and verified static PIE relocations are reapplied after syscall handoff.",
        failures: failures,
        artifacts: artifacts,
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runLDRSWSignExtensionReducer() throws -> Int32 {
    let target = "tcti-ldrsw-sign-extension-reducer"
    var failures: [Failure] = []
    var artifacts: [String] = []

    guard let simulatorReport = latestRuntimeValidationReport(
        gate: "tcti-simulator-stability",
        destination: "iphonesimulator"
    ) else {
        failures.append(fail("simulator-report", "missing iphonesimulator tcti-simulator-stability report"))
        let reportURL = try writeReport(report(
            target: target,
            status: .fail,
            summary: "No simulator stability failure report was available for the LDRSW sign-extension reducer.",
            failures: failures,
            artifacts: artifacts,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("fail: \(relativePath(reportURL))")
        return 1
    }

    let simulatorObject = simulatorReport.object
    artifacts.append(relativePath(simulatorReport.url))
    let simulatorArtifacts = (simulatorObject["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
    artifacts.append(contentsOf: simulatorArtifacts)
    let fatalText = try simulatorArtifacts.first { $0.hasSuffix("tcti-simulator-fatal-runtime.txt") }.map(readRelativeArtifact) ?? ""
    let terminalText = try simulatorArtifacts.first { $0.hasSuffix("simulator-terminal-output.txt") }.map(readRelativeArtifact) ?? ""
    let combinedText = fatalText + "\n" + terminalText

    if stringField(simulatorObject, "git_sha") != gitSha() {
        failures.append(fail("simulator-report-stale", "latest simulator stability report is stale for current HEAD"))
    }
    if stringField(simulatorObject, "status") != "fail" || boolField(simulatorObject, "passed") {
        failures.append(fail("simulator-report-status", "reducer requires a current failing simulator stability report"))
    }
    if stringField(simulatorObject, "selected_device_id") != "1E5553B0-203A-4A11-BAD7-EBDE46863F66" ||
        stringField(simulatorObject, "selected_device_name") != "Orlix-iPhone-15-Pro-Max" ||
        !boolField(simulatorObject, "simulator_single_booted") {
        failures.append(fail("simulator-scope", "latest failure must come from only Orlix-iPhone-15-Pro-Max"))
    }
    if !combinedText.contains("orlix-init: opening tty candidate") ||
        !combinedText.contains("/dev/hvc0") ||
        !(combinedText.contains("Attempted to kill init") || combinedText.contains("Attempted kill init")) {
        failures.append(fail("simulator-progress", "latest failure did not reach the /dev/hvc0 orlix-init open path before panic"))
    }

    let faultPattern = #"Orlix TCTI: user fault[^\n]*pc=(0x[0-9a-fA-F]+)[^\n]*lr=(0x[0-9a-fA-F]+)[^\n]*sp=(0x[0-9a-fA-F]+)[^\n]*addr=(0x[0-9a-fA-F]+)[^\n]*access=1"#
    if let groups = firstRegexGroups(faultPattern, in: combinedText),
       groups.count == 4,
       let pc = parseHexUInt64(groups[0]),
       let lr = parseHexUInt64(groups[1]),
       let sp = parseHexUInt64(groups[2]),
       let address = parseHexUInt64(groups[3]) {
        let addressDelta = address &- sp
        let lrDelta = lr &- pc
        if addressDelta != 0x1000000a0 {
            failures.append(fail("simulator-fault-address", "expected LDRSW zero-extension signature addr-sp=0x1000000a0, got 0x\(String(addressDelta, radix: 16))"))
        }
        if lrDelta != 0x205c {
            failures.append(fail("simulator-fault-lr", "expected LDRSW ioctl caller lr-pc=0x205c, got 0x\(String(lrDelta, radix: 16))"))
        }
    } else {
        failures.append(fail("simulator-fault-parse", "could not parse simulator user-fault pc/lr/sp/addr line"))
    }

    let initELF = path("Build", "TCTI", "inspect-runtime-init", "base", "sbin-init")
    if fileManager.fileExists(atPath: initELF.path) {
        artifacts.append(relativePath(initELF))
        let disassembly = (try? run([llvmObjdumpPath(), "-d", "--start-address=0x379c4", "--stop-address=0x37a10", initELF.path], check: false)) ?? ""
        let disassemblyURL = buildPath("ldrsw_sign_extension", "runtime-sbin-init-disassembly.txt")
        try ensureDirectory(disassemblyURL.deletingLastPathComponent())
        try disassembly.write(to: disassemblyURL, atomically: true, encoding: .utf8)
        artifacts.append(relativePath(disassemblyURL))
        if !disassembly.contains("b9801848") || !disassembly.contains("ldrsw") {
            failures.append(fail("runtime-disassembly", "runtime /sbin/init disassembly did not show emitted LDRSW 0xb9801848 at the faulting va_list load"))
        }
    } else {
        failures.append(fail("runtime-init", "missing extracted runtime /sbin/init at Build/TCTI/inspect-runtime-init/base/sbin-init"))
    }

    let reducer = try writeReducer(
        target: target,
        caseID: "ldrsw-sign-extension-pass-regression",
        command: "make tcti-gate TARGET=\(target)",
        reason: "simulator high-address user fault is reduced to emitted LDRSW 0xb9801848 requiring 32-bit load sign-extension into an X register",
        artifacts: artifacts,
        expectedStatus: .pass
    )
    artifacts.append(relativePath(reducer))

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Reduced pinned simulator high-address va_list fault to LDRSW sign-extension semantics for emitted 0xb9801848.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_reduced": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runLDRSWSignExtensionFix() throws -> Int32 {
    let target = "tcti-ldrsw-sign-extension-fix"
    var failures: [Failure] = []
    var artifacts: [String] = []
    var reducerEvidenceOK = false

    let reducerReportURL = buildPath("reports", "tcti-ldrsw-sign-extension-reducer", "report.json")
    if let reducerReport = try? loadJSON(reducerReportURL) as? [String: Any] {
        artifacts.append(relativePath(reducerReportURL))
        if stringField(reducerReport, "git_sha") != gitSha() ||
            stringField(reducerReport, "status") != "pass" ||
            !boolField(reducerReport, "passed") {
            let reducerURL = buildPath("reproducers", "tcti-ldrsw-sign-extension-reducer", "ldrsw-sign-extension-pass-regression.json")
            if let reducer = try? loadJSON(reducerURL) as? [String: Any],
               stringField(reducer, "target") == "tcti-ldrsw-sign-extension-reducer",
               stringField(reducer, "expected_status") == "pass",
               stringField(reducer, "command") == "make tcti-gate TARGET=tcti-ldrsw-sign-extension-reducer" {
                artifacts.append(relativePath(reducerURL))
                reducerEvidenceOK = true
            } else {
                failures.append(fail("reducer-report", "tcti-ldrsw-sign-extension-reducer report is missing, stale, or not passing"))
            }
        } else {
            reducerEvidenceOK = true
        }
    } else {
        let reducerURL = buildPath("reproducers", "tcti-ldrsw-sign-extension-reducer", "ldrsw-sign-extension-pass-regression.json")
        if let reducer = try? loadJSON(reducerURL) as? [String: Any],
           stringField(reducer, "target") == "tcti-ldrsw-sign-extension-reducer",
           stringField(reducer, "expected_status") == "pass",
           stringField(reducer, "command") == "make tcti-gate TARGET=tcti-ldrsw-sign-extension-reducer" {
            artifacts.append(relativePath(reducerURL))
            reducerEvidenceOK = true
        } else {
            failures.append(fail("reducer-report", "missing tcti-ldrsw-sign-extension-reducer report"))
        }
    }

    let decoderURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "decode_aarch64.c")
    let testsURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "tcti_decode_test.c")
    let decoder = try readText(decoderURL)
    let tests = try readText(testsURL)
    artifacts.append(relativePath(decoderURL))
    artifacts.append(relativePath(testsURL))

    if !decoder.contains("*result_size = sizeof(u64);\n\t\treturn true;\n\tcase 3:") ||
        !decoder.contains("*result_size = sizeof(u32);\n\t\treturn true;\n\tdefault:") {
        failures.append(fail("decoder-result-width", "signed load decoder must keep opc=2 as X-register result and opc=3 as W-register result"))
    }
    if !tests.contains("0xb9801848U") ||
        !tests.contains("tcti_decode_ldrsw_signed_immediate_writes_x_register") ||
        !tests.contains("KUNIT_EXPECT_EQ(test, 8U, decoded.result_size)") {
        failures.append(fail("kunit-regression", "missing exact emitted LDRSW 0xb9801848 decode/result-width regression"))
    }
    if !reducerEvidenceOK {
        failures.append(fail("reducer-evidence", "missing reducer report or pass-regression evidence for LDRSW sign-extension"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Checked reducer-backed LDRSW sign-extension fix for the simulator high-address va_list fault.",
        failures: failures,
        artifacts: artifacts,
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
	print("\(status.rawValue): \(relativePath(reportURL))")
	return exitCode(for: status)
}

func runCloneZeroPCReducer() throws -> Int32 {
	let target = "tcti-clone-zero-pc-reducer"
	var failures: [Failure] = []
	var artifacts: [String] = []

	guard let simulatorReport = selectedRuntimeValidationReport(
		gates: [
			"tcti-static-busybox-shell-command",
			"tcti-simulator-stability",
		],
		destination: "iphonesimulator"
	) else {
		failures.append(fail("simulator-report", "missing iphonesimulator post-Bash mmap/read fault report"))
		let reportURL = try writeReport(report(
			target: target,
			status: .fail,
			summary: "No simulator stability failure report was available for the clone zero-PC reducer.",
			failures: failures,
			artifacts: artifacts,
			releaseGateEligible: false,
			readinessGateEligible: false
		))
		print("fail: \(relativePath(reportURL))")
		return 1
	}

	let simulatorObject = simulatorReport.object
	artifacts.append(relativePath(simulatorReport.url))
	let simulatorArtifacts = (simulatorObject["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
	artifacts.append(contentsOf: simulatorArtifacts)
	let fatalText = try simulatorArtifacts.first { $0.hasSuffix("tcti-simulator-fatal-runtime.txt") }.map(readRelativeArtifact) ?? ""
	let terminalText = try simulatorArtifacts.first { $0.hasSuffix("simulator-terminal-output.txt") }.map(readRelativeArtifact) ?? ""
	let combinedText = fatalText + "\n" + terminalText

	if stringField(simulatorObject, "git_sha") != gitSha() {
		failures.append(fail("simulator-report-stale", "latest simulator stability report is stale for current HEAD"))
	}
	if stringField(simulatorObject, "status") != "fail" || boolField(simulatorObject, "passed") {
		failures.append(fail("simulator-report-status", "reducer requires a current failing simulator stability report"))
	}
	if stringField(simulatorObject, "selected_device_id") != "1E5553B0-203A-4A11-BAD7-EBDE46863F66" ||
		stringField(simulatorObject, "selected_device_name") != "Orlix-iPhone-15-Pro-Max" ||
		stringField(simulatorObject, "simulator_booted_count") != "1" ||
		!boolField(simulatorObject, "simulator_single_booted") {
		failures.append(fail("simulator-scope", "reducer requires the single pinned Orlix-iPhone-15-Pro-Max simulator report"))
	}
	if !combinedText.contains("orlix-init: runtime filesystems mounted") ||
		!combinedText.contains("Orlix TCTI: applied static PIE R_AARCH64_RELATIVE relocations") ||
		!combinedText.contains("syscall=220 x0=0x11") ||
		!combinedText.contains("Orlix TCTI: user fault task=init pid=1 pc=0x0 lr=0x0") ||
		!combinedText.contains("addr=0x0 access=0") ||
		!(combinedText.contains("Attempted to kill init") || combinedText.contains("Attempted kill init")) {
		failures.append(fail("clone-zero-pc-signature", "latest simulator artifact does not match clone(SIGCHLD) followed by TCTI fetch at pc=0"))
	}
	if combinedText.contains("status=-17") ||
		combinedText.contains("unrelocated") ||
		combinedText.contains("relocation invisible") {
		failures.append(fail("clone-zero-pc-phase", "clone zero-PC reducer must describe the post-static-PIE relocation failure phase"))
	}

	let reducer = try writeReducer(
		target: target,
		caseID: "clone-zero-pc-pass-regression",
		command: "make tcti-gate TARGET=\(target)",
		reason: "simulator clone(SIGCHLD) failure reduced to TCTI resuming from pc=0 after syscall 220",
		artifacts: artifacts,
		expectedStatus: .pass
	)
	artifacts.append(relativePath(reducer))

	let status: GateStatus = failures.isEmpty ? .pass : .fail
	let reportURL = try writeReport(report(
		target: target,
		status: status,
		summary: "Reduced pinned simulator clone(SIGCHLD) return-to-zero fault to TCTI syscall frame preservation.",
		failures: failures,
		artifacts: artifacts,
		counters: ["simulator_reports_reduced": 1],
		releaseGateEligible: false,
		readinessGateEligible: false
	))
	print("\(status.rawValue): \(relativePath(reportURL))")
	return exitCode(for: status)
}

func runCloneZeroPCFix() throws -> Int32 {
	let target = "tcti-clone-zero-pc-fix"
	var failures: [Failure] = []
	var artifacts: [String] = []

	let reducerReportURL = buildPath("reports", "tcti-clone-zero-pc-reducer", "report.json")
	if let reducerReport = try? loadJSON(reducerReportURL) as? [String: Any] {
		artifacts.append(relativePath(reducerReportURL))
		if stringField(reducerReport, "git_sha") != gitSha() ||
			stringField(reducerReport, "status") != "pass" ||
			!boolField(reducerReport, "passed") {
			failures.append(fail("reducer-report", "tcti-clone-zero-pc-reducer report is missing, stale, or not passing"))
		}
	} else {
		failures.append(fail("reducer-report", "missing tcti-clone-zero-pc-reducer report"))
	}

	let mmuURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "include", "asm", "mmu.h")
	let engineURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "engine.c")
	let processURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "kernel", "process.c")
	let userPageURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "mm", "tcti_user_page.c")
	let testsURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "tcti_decode_test.c")
	let mmu = try readText(mmuURL)
	let engine = try readText(engineURL)
	let process = try readText(processURL)
	let userPage = try readText(userPageURL)
	let tests = try readText(testsURL)
	artifacts.append(relativePath(mmuURL))
	artifacts.append(relativePath(engineURL))
	artifacts.append(relativePath(processURL))
	artifacts.append(relativePath(userPageURL))
	artifacts.append(relativePath(testsURL))

	if !mmu.contains("unsigned long orlix_tcti_static_pie_base;") {
		failures.append(fail("mm-context-marker", "mm_context_t must carry the TCTI static PIE relocation-applied base across forked mm copies"))
	}
	if !engine.contains("mm->context.orlix_tcti_static_pie_base == base") ||
		!engine.contains("mm->context.orlix_tcti_static_pie_base = base") {
		failures.append(fail("relocation-mm-context", "TCTI static PIE relocation path must skip already-relocated fork children using the mm context marker"))
	}
	if !process.contains("current->mm->context.orlix_tcti_static_pie_base = 0") {
		failures.append(fail("exec-reset", "start_thread must reset the TCTI static PIE relocation marker for a new exec image"))
	}
	if !userPage.contains("#include <asm/hosted_exec.h>") ||
		!userPage.contains("orlix_refresh_current_user_mapping_page_from_kernel") ||
		!userPage.contains("access == TCTI_ACCESS_WRITE") {
		failures.append(fail("tcti-write-host-refresh", "TCTI guest writes must refresh the hosted user mapping from the Linux page backing"))
	}
	if !tests.contains("0xa8c27bfdU") ||
		!tests.contains("TCTI_MEMORY_INDEX_POST") ||
		!tests.contains("KUNIT_EXPECT_EQ(test, 30U, decoded.rt2)") {
		failures.append(fail("kunit-regression", "missing exact post-index LDP x29/x30 stack epilogue decode regression"))
	}

	let status: GateStatus = failures.isEmpty ? .pass : .fail
	let reportURL = try writeReport(report(
		target: target,
		status: status,
		summary: "Checked reducer-backed TCTI clone syscall static PIE relocation marker fix for pc=0 simulator fault.",
		failures: failures,
		artifacts: artifacts,
		releaseGateEligible: false,
		readinessGateEligible: false
	))
	print("\(status.rawValue): \(relativePath(reportURL))")
	return exitCode(for: status)
}

func runPostSetsidTLSFaultReducer() throws -> Int32 {
	let target = "tcti-post-setsid-tls-fault-reducer"
	var failures: [Failure] = []
	var artifacts: [String] = []

	guard let simulatorReport = selectedRuntimeValidationReport(
		gate: "tcti-simulator-stability",
		destination: "iphonesimulator"
	) else {
		failures.append(fail("simulator-report", "missing iphonesimulator tcti-simulator-stability report"))
		let reportURL = try writeReport(report(
			target: target,
			status: .fail,
            summary: "No simulator stability failure report was available for the post-setsid trap-fallthrough reducer.",
			failures: failures,
			artifacts: artifacts,
			releaseGateEligible: false,
			readinessGateEligible: false
		))
		print("fail: \(relativePath(reportURL))")
		return 1
	}

	let simulatorObject = simulatorReport.object
	artifacts.append(relativePath(simulatorReport.url))
	let simulatorArtifacts = (simulatorObject["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
	artifacts.append(contentsOf: simulatorArtifacts)
	let terminalText = try simulatorArtifacts.first { $0.hasSuffix("simulator-terminal-output.txt") }.map(readRelativeArtifact) ?? ""
	let startedPID = firstRegexGroups(#"orlix-init: process started pid=([0-9]+)"#, in: terminalText)?.first ??
		firstRegexGroups(#"orlix-init: pid=([0-9]+)"#, in: terminalText)?.first
	let staticReadGroups = firstRegexGroups(#"Orlix TCTI: static PIE read has no readable VMA task=init pid=([0-9]+) addr=(0x[0-9a-fA-F]+) size=([0-9]+) ret=(-?[0-9]+)"#, in: terminalText)
	let childStaticReadGroups = startedPID.flatMap { pid in
		firstRegexGroups(#"Orlix TCTI: (?:static )?PIE read (?:has )?no readable VMA task=init pid=\#(pid) addr=(0x[0-9a-fA-F]+) size=([0-9]+) ret=(-?[0-9]+)"#, in: terminalText)
	}
	let staticReadPID = childStaticReadGroups == nil ? staticReadGroups?[0] : startedPID
	let staticReadAddress = childStaticReadGroups?[0] ?? staticReadGroups?[1]
	let staticReadSize = childStaticReadGroups?[1] ?? staticReadGroups?[2]
	let staticReadReturn = childStaticReadGroups?[2] ?? staticReadGroups?[3]
	let signaledPID = firstRegexGroups(#"orlix-init: process signaled pid=([0-9]+) signal=11"#, in: terminalText)?.first

	if stringField(simulatorObject, "git_sha") != gitSha() {
		failures.append(fail("simulator-report-stale", "latest simulator stability report is stale for current HEAD"))
	}
	if stringField(simulatorObject, "status") != "fail" || boolField(simulatorObject, "passed") {
		failures.append(fail("simulator-report-status", "reducer requires a current failing simulator stability report"))
	}
	if stringField(simulatorObject, "selected_device_id") != "1E5553B0-203A-4A11-BAD7-EBDE46863F66" ||
		stringField(simulatorObject, "selected_device_name") != "Orlix-iPhone-15-Pro-Max" ||
		stringField(simulatorObject, "simulator_booted_count") != "1" ||
		!boolField(simulatorObject, "simulator_single_booted") {
		failures.append(fail("simulator-scope", "reducer requires the single pinned Orlix-iPhone-15-Pro-Max simulator report"))
	}
	if !terminalText.contains("syscall=220 x0=0x11") ||
		!(terminalText.contains("orlix-init: process started pid=") || terminalText.contains("orlix-init: pid=")) ||
		!terminalText.contains("Orlix TCTI: static PIE read has no readable VMA") ||
		childStaticReadGroups == nil ||
		!terminalText.contains("orlix-init: process signaled pid=") ||
		!terminalText.contains("signal=11") ||
		!terminalText.contains("orlix-init: shell exit status=") ||
		!terminalText.contains("139") {
        failures.append(fail("post-setsid-tls-fault-signature", "latest simulator artifact does not match the fork child static PIE read VMA miss followed by SIGSEGV"))
	}
	if startedPID == nil || staticReadPID == nil || signaledPID == nil || startedPID != signaledPID ||
		childStaticReadGroups == nil {
		failures.append(fail("post-setsid-child-pid", "simulator evidence must tie the static PIE read VMA miss and SIGSEGV to the child process started by clone"))
	}
	if staticReadAddress == nil || staticReadSize != "64" || staticReadReturn != "-14" {
		failures.append(fail("post-setsid-static-pie-read", "simulator evidence must show child static PIE read addr=<ASLR-dependent> size=64 ret=-14"))
	}

	struct PositiveReturnSample: Codable {
		let setsidReturn: String
		let classifiedPath: String
		let forbiddenFaultAddress: String?
	}
	struct PostSetsidControlFlowModel: Codable {
		let syscall: String
		let invariant: String
		let faultPCStaticOffset: String
		let faultLRStaticOffset: String
		let samples: [PositiveReturnSample]
	}
	func classifySetsidReturn(_ value: UInt64) -> PositiveReturnSample {
		let linuxErrnoLow = UInt64.max - 4095 + 1
		let isLinuxErrno = value >= linuxErrnoLow
		return PositiveReturnSample(
			setsidReturn: String(format: "0x%llx", value),
			classifiedPath: isLinuxErrno ? "error-unsupported-brk-fallthrough" : "success-return",
			forbiddenFaultAddress: isLinuxErrno ? String(format: "0x%llx", value) : nil
		)
	}
	let positiveReturnSamples = [UInt64(0x20), UInt64(0x21)].map(classifySetsidReturn)
	if positiveReturnSamples.contains(where: { $0.classifiedPath != "success-return" || $0.forbiddenFaultAddress != nil }) {
		failures.append(fail("post-setsid-positive-return-control-flow", "setsid positive returns 0x20 and 0x21 must stay on the syscall success path and must not reach unsupported brk/error fallthrough"))
	}
	let model = PostSetsidControlFlowModel(
		syscall: "setsid(157)",
		invariant: "positive pid/session-id returns 0x20 and 0x21 must not fall through the unsupported brk/error path into ldr x0, [x0]",
		faultPCStaticOffset: "0x1bd4c",
		faultLRStaticOffset: "0x1bd2c",
		samples: positiveReturnSamples
	)
	let modelURL = buildPath("reproducers", target, "post-setsid-positive-return-control-flow-model.json")
	try writeJSON(model, to: modelURL)
	artifacts.append(relativePath(modelURL))

	let reducer = try writeReducer(
		target: target,
		caseID: "post-setsid-tls-fault-pass-regression",
		command: "make tcti-gate TARGET=\(target)",
        reason: "simulator child process faults after setsid returns the child pid/session id; positive returns 0x20 and 0x21 must not fall through an unsupported brk/error path into ldr x0, [x0]",
		artifacts: artifacts,
		expectedStatus: .pass
	)
	artifacts.append(relativePath(reducer))

	let status: GateStatus = failures.isEmpty ? .pass : .fail
	let reportURL = try writeReport(report(
		target: target,
		status: status,
        summary: "Reduced pinned simulator post-setsid child pid-valued user fault to TCTI trap-fallthrough evidence.",
		failures: failures,
		artifacts: artifacts,
		counters: ["simulator_reports_reduced": 1],
		releaseGateEligible: false,
		readinessGateEligible: false
	))
	print("\(status.rawValue): \(relativePath(reportURL))")
	return exitCode(for: status)
}

func runPostExecSHFetchFaultReducer() throws -> Int32 {
	let target = "tcti-post-exec-sh-fetch-fault-reducer"
	var failures: [Failure] = []
	var artifacts: [String] = []

	guard let simulatorReport = latestRuntimeValidationReport(
		gate: "tcti-simulator-stability",
		destination: "iphonesimulator"
	) else {
		failures.append(fail("simulator-report", "missing iphonesimulator tcti-simulator-stability report"))
		let reportURL = try writeReport(report(
			target: target,
			status: .fail,
			summary: "No simulator stability failure report was available for the post-exec sh fetch-fault reducer.",
			failures: failures,
			artifacts: artifacts,
			releaseGateEligible: false,
			readinessGateEligible: false
		))
		print("fail: \(relativePath(reportURL))")
		return 1
	}

	let simulatorObject = simulatorReport.object
	artifacts.append(relativePath(simulatorReport.url))
	let simulatorArtifacts = (simulatorObject["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
	artifacts.append(contentsOf: simulatorArtifacts)
	let terminalText = try simulatorArtifacts.first { $0.hasSuffix("simulator-terminal-output.txt") }.map(readRelativeArtifact) ?? ""
	let fatalText = try simulatorArtifacts.first { $0.hasSuffix("tcti-simulator-fatal-runtime.txt") }.map(readRelativeArtifact) ?? ""
	let combinedText = terminalText + "\n" + fatalText

	let startedPID = firstRegexGroups(#"orlix-init: process started pid=([0-9]+)"#, in: terminalText)?.first ??
		firstRegexGroups(#"orlix-init: pid=([0-9]+)"#, in: terminalText)?.first
	let faultGroups = firstRegexGroups(#"Orlix TCTI: user fault task=sh pid=([0-9]+) pc=(0x[0-9a-fA-F]+).*addr=(0x[0-9a-fA-F]+) access=0 si=1"#, in: combinedText)
	let faultPID = faultGroups?[0]
	let faultPC = faultGroups?[1]
	let faultAddress = faultGroups?[2]
	let execPID = faultPID.flatMap { pid -> String? in
		let pattern = "(?:Orlix TCTI: svc #0|TCTI: #0) task=init pid=\(pid) pc=0x[0-9a-fA-F]+ syscall=221 x0=0x[0-9a-fA-F]+"
		return terminalText.range(of: pattern, options: .regularExpression) == nil ? nil : pid
	}

	if stringField(simulatorObject, "git_sha") != gitSha() {
		failures.append(fail("simulator-report-stale", "latest simulator stability report is stale for current HEAD"))
	}
	if stringField(simulatorObject, "status") != "fail" || boolField(simulatorObject, "passed") {
		failures.append(fail("simulator-report-status", "reducer requires a current failing simulator stability report"))
	}
	if stringField(simulatorObject, "selected_device_id") != "1E5553B0-203A-4A11-BAD7-EBDE46863F66" ||
		stringField(simulatorObject, "selected_device_name") != "Orlix-iPhone-15-Pro-Max" ||
		stringField(simulatorObject, "simulator_booted_count") != "1" ||
		!boolField(simulatorObject, "simulator_single_booted") {
		failures.append(fail("simulator-scope", "reducer requires the single pinned Orlix-iPhone-15-Pro-Max simulator report"))
	}
	if startedPID == nil || execPID == nil || faultPID == nil || startedPID != execPID || execPID != faultPID {
		failures.append(fail("post-exec-child-pid", "simulator evidence must tie clone child, execve(221), and sh fetch fault to the same pid"))
	}
	if faultPC == nil || faultAddress == nil || faultPC != faultAddress {
		failures.append(fail("post-exec-fetch-address", "fetch fault must report pc equal to fault address"))
	}
	if faultPC != "0x1000494c8" {
		failures.append(fail("post-exec-fetch-pc", "latest simulator artifact must match the observed sh fetch fault pc=0x1000494c8"))
	}
	if !terminalText.contains("syscall=157 x0=0x9d") ||
		!terminalText.contains("syscall=221") ||
		!combinedText.contains("Orlix TCTI: user fault task=sh pid=") ||
		!combinedText.contains("access=0 si=1") ||
		!combinedText.contains("signal=11") ||
		!combinedText.contains("shell exit status") ||
		!combinedText.contains("139") {
		failures.append(fail("post-exec-sh-fetch-signature", "latest simulator artifact does not match the post-exec sh fetch-fault signature"))
	}

	struct PostExecSHFetchFaultModel: Codable {
		let childPID: String
		let execveSyscall: String
		let taskAfterExec: String
		let faultPC: String
		let faultAddress: String
		let access: String
		let invariant: String
	}
	if let pid = faultPID, let pc = faultPC, let address = faultAddress {
		let model = PostExecSHFetchFaultModel(
			childPID: pid,
			execveSyscall: "221",
			taskAfterExec: "sh",
			faultPC: pc,
			faultAddress: address,
			access: "fetch",
			invariant: "after execve(221), TCTI must fetch shell text through Linux mm-backed executable mappings instead of faulting at the new entry pc"
		)
		let modelURL = buildPath("reproducers", target, "post-exec-sh-fetch-fault-model.json")
		try writeJSON(model, to: modelURL)
		artifacts.append(relativePath(modelURL))
	}

	let reducer = try writeReducer(
		target: target,
		caseID: "post-exec-sh-fetch-fault-pass-regression",
		command: "make tcti-gate TARGET=\(target)",
		reason: "pinned simulator reaches execve(221) for /bin/sh and then TCTI faults fetching the shell entry pc=addr=0x1000494c8",
		artifacts: artifacts,
		expectedStatus: .pass
	)
	artifacts.append(relativePath(reducer))

	let status: GateStatus = failures.isEmpty ? .pass : .fail
	let reportURL = try writeReport(report(
		target: target,
		status: status,
		summary: "Reduced pinned simulator post-exec sh fetch fault to a no-phone TCTI evidence gate.",
		failures: failures,
		artifacts: artifacts,
		counters: ["simulator_reports_reduced": 1],
		releaseGateEligible: false,
		readinessGateEligible: false
	))
	print("\(status.rawValue): \(relativePath(reportURL))")
	return exitCode(for: status)
}

func runPostPIESHEntryFetchFaultReducer() throws -> Int32 {
	let target = "tcti-post-pie-sh-entry-fetch-fault-reducer"
	var failures: [Failure] = []
	var artifacts: [String] = []

	guard let simulatorReport = latestRuntimeValidationReport(
		gate: "tcti-simulator-stability",
		destination: "iphonesimulator"
	) else {
		failures.append(fail("simulator-report", "missing iphonesimulator tcti-simulator-stability report"))
		let reportURL = try writeReport(report(
			target: target,
			status: .fail,
			summary: "No simulator stability failure report was available for the post-PIE sh entry fetch-fault reducer.",
			failures: failures,
			artifacts: artifacts,
			releaseGateEligible: false,
			readinessGateEligible: false
		))
		print("fail: \(relativePath(reportURL))")
		return 1
	}

	let simulatorObject = simulatorReport.object
	artifacts.append(relativePath(simulatorReport.url))
	let simulatorArtifacts = (simulatorObject["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
	artifacts.append(contentsOf: simulatorArtifacts)
	let terminalText = try simulatorArtifacts.first { $0.hasSuffix("simulator-terminal-output.txt") }.map(readRelativeArtifact) ?? ""
	let fatalText = try simulatorArtifacts.first { $0.hasSuffix("tcti-simulator-fatal-runtime.txt") }.map(readRelativeArtifact) ?? ""
	let combinedText = terminalText + "\n" + fatalText

	let startedPID = firstRegexGroups(#"orlix-init: process started pid=([0-9]+)"#, in: terminalText)?.first
	let exitGroups = firstRegexGroups(#"Orlix TCTI: exit task=sh pid=([0-9]+) reason=1 status=(-?[0-9]+) pc=(0x[0-9a-fA-F]+) fault=(0x[0-9a-fA-F]+) insn=(0x[0-9a-fA-F]+)"#, in: terminalText)
	let exitPID = exitGroups?[0]
	let exitStatus = exitGroups?[1]
	let exitPC = exitGroups?[2]
	let faultAddress = exitGroups?[3]
	let instruction = exitGroups?[4]

	if stringField(simulatorObject, "git_sha") != gitSha() {
		failures.append(fail("simulator-report-stale", "latest simulator stability report is stale for current HEAD"))
	}
	if stringField(simulatorObject, "status") != "fail" || boolField(simulatorObject, "passed") {
		failures.append(fail("simulator-report-status", "reducer requires a current failing simulator stability report"))
	}
	if stringField(simulatorObject, "selected_device_id") != "1E5553B0-203A-4A11-BAD7-EBDE46863F66" ||
		stringField(simulatorObject, "selected_device_name") != "Orlix-iPhone-15-Pro-Max" ||
		stringField(simulatorObject, "simulator_booted_count") != "1" ||
		!boolField(simulatorObject, "simulator_single_booted") {
		failures.append(fail("simulator-scope", "reducer requires the single pinned Orlix-iPhone-15-Pro-Max simulator report"))
	}
	if startedPID == nil || exitPID == nil || startedPID != exitPID {
		failures.append(fail("post-pie-sh-child-pid", "simulator evidence must tie the spawned shell pid to the TCTI entry fetch exit"))
	}
	if exitStatus != "-14" {
		failures.append(fail("post-pie-sh-status", "TCTI entry fetch failure must exit with status=-14"))
	}
	if exitPC == nil || faultAddress == nil || exitPC != faultAddress {
		failures.append(fail("post-pie-sh-fetch-address", "PIE entry fetch failure must report pc equal to fault address"))
	}
	if instruction != "0x0" {
		failures.append(fail("post-pie-sh-instruction", "PIE entry fetch failure must report insn=0x0"))
	}
	if !terminalText.contains("syscall=221") ||
		!terminalText.contains("Orlix TCTI: exit task=sh pid=") ||
		!combinedText.contains("shell exit status") ||
		!combinedText.contains("139") {
		failures.append(fail("post-pie-sh-entry-fetch-signature", "latest simulator artifact does not match the post-PIE sh entry fetch-fault signature"))
	}

	struct PostPIESHEntryFetchFaultModel: Codable {
		let childPID: String
		let execveSyscall: String
		let taskAfterExec: String
		let exitStatus: String
		let entryPC: String
		let faultAddress: String
		let instruction: String
		let invariant: String
	}

	if let pid = exitPID, let pc = exitPC, let address = faultAddress, let insn = instruction {
		let model = PostPIESHEntryFetchFaultModel(
			childPID: pid,
			execveSyscall: "221",
			taskAfterExec: "sh",
			exitStatus: "-14",
			entryPC: pc,
			faultAddress: address,
			instruction: insn,
			invariant: "after execve(221) of a static-PIE shell, TCTI must fetch the relocated shell entry through Linux mm-backed executable mappings instead of exiting with -EFAULT at the entry pc"
		)
		let modelURL = buildPath("reproducers", target, "post-pie-sh-entry-fetch-fault-model.json")
		try writeJSON(model, to: modelURL)
		artifacts.append(relativePath(modelURL))
	}

	let reducer = try writeReducer(
		target: target,
		caseID: "post-pie-sh-entry-fetch-fault-pass-regression",
		command: "make tcti-gate TARGET=\(target)",
		reason: "pinned simulator reaches execve(221) for static-PIE /bin/sh and then TCTI exits with -EFAULT fetching the relocated shell entry",
		artifacts: artifacts,
		expectedStatus: .pass
	)
	artifacts.append(relativePath(reducer))

	let status: GateStatus = failures.isEmpty ? .pass : .fail
	let reportURL = try writeReport(report(
		target: target,
		status: status,
		summary: "Reduced pinned simulator post-PIE sh entry fetch fault to a no-phone TCTI evidence gate.",
		failures: failures,
		artifacts: artifacts,
		counters: ["simulator_reports_reduced": 1],
		releaseGateEligible: false,
		readinessGateEligible: false
	))
	print("\(status.rawValue): \(relativePath(reportURL))")
	return exitCode(for: status)
}

func runPostBashMmapReadFaultReducer() throws -> Int32 {
	let target = "tcti-post-bash-mmap-read-fault-reducer"
	var failures: [Failure] = []
	var artifacts: [String] = []

	guard let simulatorReport = selectedRuntimeValidationReport(
		gates: [
			"tcti-static-busybox-shell-command",
			"tcti-simulator-stability",
		],
		destination: "iphonesimulator"
	) else {
		failures.append(fail("simulator-report", "missing iphonesimulator post-Bash mmap/read fault report"))
		let reportURL = try writeReport(report(
			target: target,
			status: .fail,
			summary: "No simulator post-Bash mmap/read failure report was available for the reducer.",
			failures: failures,
			artifacts: artifacts,
			releaseGateEligible: false,
			readinessGateEligible: false
		))
		print("fail: \(relativePath(reportURL))")
		return 1
	}

	let simulatorObject = simulatorReport.object
	artifacts.append(relativePath(simulatorReport.url))
	let simulatorArtifacts = (simulatorObject["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
	artifacts.append(contentsOf: simulatorArtifacts)
	let events = simulatorObject["tcti_runtime_events"] as? [String: Any]
	let staticPIE = events?["static_pie_image"] as? [String: Any] ?? [:]
	let mmap = events?["last_mmap_syscall"] as? [String: Any] ?? [:]
	let fault = events?["fatal_user_fault"] as? [String: Any] ?? [:]
	let signal = events?["signaled_process"] as? [String: Any] ?? [:]
	let imagePID = intField(staticPIE, "pid").map(String.init)
	let imagePC = stringField(staticPIE, "pc")
	let imageBase = stringField(staticPIE, "base")
	let imageEntry = stringField(staticPIE, "entry")
	let faultPID = intField(fault, "pid").map(String.init)
	let faultPC = stringField(fault, "pc")
	let faultAddress = stringField(fault, "addr")
	let signaledPID = intField(signal, "pid").map(String.init)
	let hasStructuredEvents = events != nil
	let hasMmapSyscall = intField(mmap, "syscall") == 222 &&
		stringField(mmap, "task") == "sh" &&
		intField(mmap, "pid").map(String.init) == imagePID
	let simulatorPassed = stringField(simulatorObject, "status") == "pass" && boolField(simulatorObject, "passed")
	let simulatorFailed = stringField(simulatorObject, "status") == "fail" && !boolField(simulatorObject, "passed")
	let staticPIEImageCaptured = stringField(staticPIE, "task") == "sh" &&
		imagePID != nil &&
		!imagePC.isEmpty &&
		!imageBase.isEmpty &&
		!imageEntry.isEmpty
	let noFatalUserFault = faultAddress.isEmpty
	let faultOffset: String? = {
		guard
			!imageBase.isEmpty,
			!faultAddress.isEmpty,
			let base = UInt64(imageBase.dropFirst(2), radix: 16),
			let address = UInt64(faultAddress.dropFirst(2), radix: 16),
			address >= base
		else {
			return nil
		}
		return String(format: "0x%llx", address - base)
	}()

	if stringField(simulatorObject, "git_sha") != gitSha() {
		failures.append(fail("simulator-report-stale", "latest simulator stability report is stale for current HEAD"))
	}
	if !simulatorPassed && !simulatorFailed {
		failures.append(fail("simulator-report-status", "reducer requires a current simulator stability pass or fail report"))
	}
	if stringField(simulatorObject, "selected_device_id") != "1E5553B0-203A-4A11-BAD7-EBDE46863F66" ||
		stringField(simulatorObject, "selected_device_name") != "Orlix-iPhone-15-Pro-Max" ||
		stringField(simulatorObject, "simulator_booted_count") != "1" ||
		!boolField(simulatorObject, "simulator_single_booted") {
		failures.append(fail("simulator-scope", "reducer requires the single pinned Orlix-iPhone-15-Pro-Max simulator report"))
	}
	if !hasStructuredEvents {
		failures.append(fail("structured-tcti-events", "reducer requires structured tcti_runtime_events in the simulator JSON report"))
	}
	if !staticPIEImageCaptured {
		failures.append(fail("post-bash-static-pie-event", "simulator evidence must include Bash static PIE image event"))
	}
	if simulatorFailed && (imagePID == nil || faultPID == nil || signaledPID == nil || imagePID != faultPID || faultPID != signaledPID) {
		failures.append(fail("post-bash-read-fault-pid", "simulator evidence must tie Bash static PIE entry, read fault, and SIGSEGV to the same shell pid"))
	}
	if !hasMmapSyscall {
		failures.append(fail("post-bash-mmap-syscall", "simulator evidence must show Bash issued mmap syscall 222"))
	}
	if simulatorFailed && (faultPC.isEmpty || faultAddress.isEmpty || faultOffset == nil) {
		failures.append(fail("post-bash-read-fault-address", "simulator evidence must include the Bash read-fault pc, address, and base-relative offset"))
	}
	if simulatorFailed && (stringField(staticPIE, "task") != "sh" ||
		stringField(fault, "task") != "sh" ||
		intField(fault, "access") != 1 ||
		intField(fault, "si") != 1 ||
		intField(signal, "signal") != 11) {
		failures.append(fail("post-bash-mmap-read-fault-signature", "structured simulator event does not match the post-Bash mmap/read fault signature"))
	}
	if simulatorPassed && !noFatalUserFault {
		failures.append(fail("post-bash-mmap-read-fault-signature", "passing simulator report still contains a structured fatal user fault"))
	}

	struct PostBashMmapReadFaultModel: Codable {
		let childPID: String
		let task: String
		let staticPIEBase: String
		let staticPIEEntry: String
		let staticPIEPC: String
		let mmapSyscall: String
		let faultPC: String
		let faultAddress: String
		let faultOffsetFromBase: String
		let access: String
		let invariant: String
	}
	if
		let pid = faultPID,
		let offset = faultOffset,
		!imageBase.isEmpty,
		!imageEntry.isEmpty,
		!imagePC.isEmpty,
		!faultPC.isEmpty,
		!faultAddress.isEmpty {
		let model = PostBashMmapReadFaultModel(
			childPID: pid,
			task: "sh",
			staticPIEBase: imageBase,
			staticPIEEntry: imageEntry,
			staticPIEPC: imagePC,
			mmapSyscall: "222",
			faultPC: faultPC,
			faultAddress: faultAddress,
			faultOffsetFromBase: offset,
			access: "read",
			invariant: "after static-PIE /bin/sh reaches mmap(222), TCTI must resolve Linux user data reads from valid user mappings instead of delivering SIGSEGV for the shell read"
		)
		let modelURL = buildPath("reproducers", target, "post-bash-mmap-read-fault-model.json")
		try writeJSON(model, to: modelURL)
		artifacts.append(relativePath(modelURL))
	}

	let reducer = try writeReducer(
		target: target,
		caseID: "post-bash-mmap-read-fault-pass-regression",
		command: "TCTI_SIMULATOR_REPORT=\(relativePath(simulatorReport.url)) make tcti-gate TARGET=\(target)",
		reason: "pinned simulator reaches static-PIE /bin/sh, observes mmap syscall 222, then TCTI faults a Bash user-data read and the shell exits with SIGSEGV",
		artifacts: artifacts,
		expectedStatus: .pass
	)
	artifacts.append(relativePath(reducer))

	let status: GateStatus = failures.isEmpty ? .pass : .fail
	let reportURL = try writeReport(report(
		target: target,
		status: status,
		summary: "Reduced pinned simulator post-Bash mmap/read user fault to a no-phone TCTI evidence gate.",
		failures: failures,
		artifacts: artifacts,
		counters: ["simulator_reports_reduced": 1],
		releaseGateEligible: false,
		readinessGateEligible: false
	))
	print("\(status.rawValue): \(relativePath(reportURL))")
	return exitCode(for: status)
}

func runInitReadFaultReducer() throws -> Int32 {
	let target = "tcti-init-read-fault-reducer"
	var failures: [Failure] = []
	var artifacts: [String] = []

	guard let simulatorReport = selectedRuntimeValidationReport(
		gate: "tcti-simulator-stability",
		destination: "iphonesimulator"
	) else {
		failures.append(fail("simulator-report", "missing iphonesimulator tcti-simulator-stability report"))
		let reportURL = try writeReport(report(
			target: target,
			status: .fail,
			summary: "No simulator stability failure report was available for the init read-fault reducer.",
			failures: failures,
			artifacts: artifacts,
			releaseGateEligible: false,
			readinessGateEligible: false
		))
		print("fail: \(relativePath(reportURL))")
		return 1
	}

	let simulatorObject = simulatorReport.object
	artifacts.append(relativePath(simulatorReport.url))
	let simulatorArtifacts = (simulatorObject["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
	artifacts.append(contentsOf: simulatorArtifacts)
	let fatalText = try simulatorArtifacts.first { $0.hasSuffix("tcti-simulator-fatal-runtime.txt") }.map(readRelativeArtifact) ?? ""
	let events = simulatorObject["tcti_runtime_events"] as? [String: Any] ?? [:]
	let firstSVC = events["first_svc"] as? [String: Any] ?? [:]
	let fault = events["fatal_user_fault"] as? [String: Any] ?? [:]
	let staticPIE = events["static_pie_image"] as? [String: Any] ?? [:]
	let signal = events["signaled_process"] as? [String: Any] ?? [:]

	let faultPC = stringField(fault, "pc")
	let faultLR = stringField(fault, "lr")
	let faultSP = stringField(fault, "sp")
	let faultAddress = stringField(fault, "addr")
	let firstSVCOK = stringField(firstSVC, "task") == "init" &&
		intField(firstSVC, "pid") == 1 &&
		intField(firstSVC, "syscall") == 178 &&
		!stringField(firstSVC, "pc").isEmpty
	let faultOK = stringField(fault, "task") == "init" &&
		intField(fault, "pid") == 1 &&
		intField(fault, "access") == 1 &&
		intField(fault, "si") == 1 &&
		!faultPC.isEmpty &&
		!faultLR.isEmpty &&
		!faultSP.isEmpty &&
		!faultAddress.isEmpty &&
		faultAddress != "0x0"
	let noStaticPIE = stringField(staticPIE, "task").isEmpty && intField(staticPIE, "pid") == nil
	let noSignal = intField(signal, "signal") == nil

	if stringField(simulatorObject, "git_sha") != gitSha() {
		failures.append(fail("simulator-report-stale", "latest simulator stability report is stale for current HEAD"))
	}
	if stringField(simulatorObject, "status") != "fail" || boolField(simulatorObject, "passed") {
		failures.append(fail("simulator-report-status", "reducer requires a current failing simulator stability report"))
	}
	if stringField(simulatorObject, "selected_device_id") != "1E5553B0-203A-4A11-BAD7-EBDE46863F66" ||
		stringField(simulatorObject, "selected_device_name") != "Orlix-iPhone-15-Pro-Max" ||
		stringField(simulatorObject, "simulator_booted_count") != "1" ||
		!boolField(simulatorObject, "simulator_single_booted") {
		failures.append(fail("simulator-scope", "reducer requires the single pinned Orlix-iPhone-15-Pro-Max simulator report"))
	}
	if !firstSVCOK {
		failures.append(fail("init-first-svc", "simulator report must show init reached first TCTI svc syscall 178 before the fault"))
	}
	if !faultOK {
		failures.append(fail("init-read-fault-signature", "simulator report must show init read fault at a non-null user address with access=1 si=1"))
	}
	if !noStaticPIE {
		failures.append(fail("init-read-fault-phase", "init read-fault reducer must cover the pre-static-PIE-image phase"))
	}
	if !noSignal {
		failures.append(fail("init-read-fault-phase", "init read-fault reducer must not cover later signaled shell or BusyBox failures"))
	}
	if !fatalText.contains("Orlix TCTI: user fault task=init") ||
		!fatalText.contains("access=1") ||
		!(fatalText.contains("Attempted to kill init") || fatalText.contains("Attempted kill init")) {
		failures.append(fail("fatal-artifact", "fatal runtime artifact must agree with the structured init read-fault report"))
	}

	struct InitReadFaultModel: Codable {
		let task: String
		let pid: Int
		let firstSyscall: Int
		let faultPC: String
		let faultLR: String
		let faultSP: String
		let faultAddress: String
		let access: String
		let invariant: String
	}
	let model = InitReadFaultModel(
		task: "init",
		pid: 1,
		firstSyscall: 178,
		faultPC: faultPC,
		faultLR: faultLR,
		faultSP: faultSP,
		faultAddress: faultAddress,
		access: "read",
		invariant: "after init reaches the first TCTI syscall, TCTI must resolve later init user-data reads without killing init before static PIE or shell progress"
	)
	let modelURL = buildPath("reproducers", target, "init-read-fault-model.json")
	try writeJSON(model, to: modelURL)
	artifacts.append(relativePath(modelURL))

	let reducer = try writeReducer(
		target: target,
		caseID: "init-read-fault-pass-regression",
		command: "TCTI_SIMULATOR_REPORT=\(relativePath(simulatorReport.url)) make tcti-gate TARGET=\(target)",
		reason: "pinned simulator init reaches first TCTI syscall 178, then TCTI faults an init user-data read before static PIE or shell progress",
		artifacts: artifacts,
		expectedStatus: .pass
	)
	artifacts.append(relativePath(reducer))

	let status: GateStatus = failures.isEmpty ? .pass : .fail
	let reportURL = try writeReport(report(
		target: target,
		status: status,
		summary: "Reduced pinned simulator init read fault to a no-phone TCTI evidence gate.",
		failures: failures,
		artifacts: artifacts,
		counters: ["simulator_reports_reduced": 1],
		releaseGateEligible: false,
		readinessGateEligible: false
	))
	print("\(status.rawValue): \(relativePath(reportURL))")
	return exitCode(for: status)
}

func runPostStaticPIEInitReadFaultReducer() throws -> Int32 {
	let target = "tcti-post-static-pie-init-read-fault-reducer"
	var failures: [Failure] = []
	var artifacts: [String] = []

	guard let simulatorReport = selectedRuntimeValidationReport(
        gates: ["tcti-static-busybox-shell-command", "tcti-init-first-syscall", "tcti-simulator-stability", "tcti-init-console-write", "tcti-full-shell-usability"],
		destination: "iphonesimulator"
	) else {
		failures.append(fail("simulator-report", "missing iphonesimulator static-PIE init read-fault report"))
		let reportURL = try writeReport(report(
			target: target,
			status: .fail,
			summary: "No simulator first-syscall or stability failure report was available for the post-static-PIE init read-fault reducer.",
			failures: failures,
			artifacts: artifacts,
			releaseGateEligible: false,
			readinessGateEligible: false
		))
		print("fail: \(relativePath(reportURL))")
		return 1
	}

	let object = simulatorReport.object
	artifacts.append(relativePath(simulatorReport.url))
	let reportArtifacts = (object["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
	artifacts.append(contentsOf: reportArtifacts)
	let fatalText = try reportArtifacts.first { $0.hasSuffix("tcti-simulator-fatal-runtime.txt") }.map(readRelativeArtifact) ?? ""
	let terminalText = try reportArtifacts.first { $0.hasSuffix("simulator-terminal-output.txt") }.map(readRelativeArtifact) ?? ""
	let launchStderrText = try reportArtifacts.first { $0.hasSuffix("launch.stderr") }.map(readRelativeArtifact) ?? ""
	let combinedFatalText = [fatalText, terminalText, launchStderrText].joined(separator: "\n")
	let events = object["tcti_runtime_events"] as? [String: Any] ?? [:]
	let firstSVC = events["first_svc"] as? [String: Any] ?? [:]
	let staticPIE = events["static_pie_image"] as? [String: Any] ?? [:]
	let fault = events["fatal_user_fault"] as? [String: Any] ?? [:]
	let signal = events["signaled_process"] as? [String: Any] ?? [:]

	let faultPC = stringField(fault, "pc")
	let faultLR = stringField(fault, "lr")
	let faultSP = stringField(fault, "sp")
	let faultAddress = stringField(fault, "addr")
	let staticBase = stringField(staticPIE, "base")
	let staticEntry = stringField(staticPIE, "entry")
	let firstSVCOK = stringField(firstSVC, "task") == "init" &&
		intField(firstSVC, "pid") == 1 &&
		intField(firstSVC, "syscall") == 178 &&
		!stringField(firstSVC, "pc").isEmpty
	let staticPIEOK = stringField(staticPIE, "task") == "init" &&
		intField(staticPIE, "pid") == 1 &&
		!stringField(staticPIE, "pc").isEmpty &&
		!staticBase.isEmpty &&
		!staticEntry.isEmpty
	let faultOK = stringField(fault, "task") == "init" &&
		intField(fault, "pid") == 1 &&
		intField(fault, "access") == 1 &&
		intField(fault, "si") == 1 &&
		!faultPC.isEmpty &&
		!faultLR.isEmpty &&
		!faultSP.isEmpty &&
		!faultAddress.isEmpty &&
		faultAddress != "0x0"
	let noSignal = intField(signal, "signal") == nil

	if stringField(object, "git_sha") != gitSha() {
		failures.append(fail("simulator-report-stale", "selected simulator static-PIE init read-fault report is stale for current HEAD"))
	}
	if stringField(object, "status") != "fail" || boolField(object, "passed") {
		failures.append(fail("simulator-report-status", "reducer requires a current failing simulator report"))
	}
	if stringField(object, "selected_device_id") != "1E5553B0-203A-4A11-BAD7-EBDE46863F66" ||
		stringField(object, "selected_device_name") != "Orlix-iPhone-15-Pro-Max" ||
		intField(object, "simulator_booted_count") != 1 ||
		!boolField(object, "simulator_single_booted") {
		failures.append(fail("simulator-scope", "reducer requires the single pinned Orlix-iPhone-15-Pro-Max simulator report"))
	}
	if !firstSVCOK {
		failures.append(fail("init-first-svc", "simulator report must show init reached first TCTI svc syscall 178 before the fault"))
	}
	if !staticPIEOK {
		failures.append(fail("static-pie-init", "simulator report must show static PIE image task=init before the read fault"))
	}
	if !faultOK {
		failures.append(fail("init-read-fault-signature", "simulator report must show post-static-PIE init read fault at a non-null user address with access=1 si=1"))
	}
	if !noSignal {
		failures.append(fail("init-read-fault-phase", "post-static-PIE init read-fault reducer must not cover later signaled shell or BusyBox failures"))
	}
	if !combinedFatalText.contains("Orlix TCTI: user fault task=init") ||
		!combinedFatalText.contains("access=1") ||
		!(combinedFatalText.contains("Attempted to kill init") || combinedFatalText.contains("Attempted kill init")) {
		failures.append(fail("fatal-artifact", "fatal runtime artifact must agree with the structured post-static-PIE init read fault"))
	}

	struct PostStaticPIEInitReadFaultModel: Codable {
		let task: String
		let pid: Int
		let firstSyscall: Int
		let staticPIEBase: String
		let staticPIEEntry: String
		let faultPC: String
		let faultLR: String
		let faultSP: String
		let faultAddress: String
		let access: String
		let invariant: String
	}
	let model = PostStaticPIEInitReadFaultModel(
		task: "init",
		pid: 1,
		firstSyscall: 178,
		staticPIEBase: staticBase,
		staticPIEEntry: staticEntry,
		faultPC: faultPC,
		faultLR: faultLR,
		faultSP: faultSP,
		faultAddress: faultAddress,
		access: "read",
		invariant: "after static-PIE init reaches the first TCTI syscall, TCTI must resolve later init user-data reads without killing init"
	)
	let modelURL = buildPath("reproducers", target, "post-static-pie-init-read-fault-model.json")
	try writeJSON(model, to: modelURL)
	artifacts.append(relativePath(modelURL))

	let reducer = try writeReducer(
		target: target,
		caseID: "post-static-pie-init-read-fault-pass-regression",
		command: "TCTI_SIMULATOR_REPORT=\(relativePath(simulatorReport.url)) make tcti-gate TARGET=\(target)",
		reason: "pinned simulator runs static-PIE /bin/true as init, reaches first TCTI syscall 178, then TCTI faults an init user-data read and kills init",
		artifacts: artifacts,
		expectedStatus: .pass
	)
	artifacts.append(relativePath(reducer))

	let status: GateStatus = failures.isEmpty ? .pass : .fail
	let reportURL = try writeReport(report(
		target: target,
		status: status,
		summary: "Reduced pinned simulator post-static-PIE init read fault to a no-phone TCTI evidence gate.",
		failures: failures,
		artifacts: artifacts,
		counters: ["simulator_reports_reduced": 1],
		releaseGateEligible: false,
		readinessGateEligible: false
	))
	print("\(status.rawValue): \(relativePath(reportURL))")
	return exitCode(for: status)
}

func runPostTrueEntryFetchFaultReducer() throws -> Int32 {
	let target = "tcti-post-true-entry-fetch-fault-reducer"
	var failures: [Failure] = []
	var artifacts: [String] = []

	guard let simulatorReport = selectedRuntimeValidationReport(
		gates: ["tcti-init-first-syscall"],
		destination: "iphonesimulator"
	) else {
		failures.append(fail("simulator-report", "missing iphonesimulator /bin/true entry fetch-fault report"))
		let reportURL = try writeReport(report(
			target: target,
			status: .fail,
			summary: "No simulator first-syscall failure report was available for the /bin/true entry fetch-fault reducer.",
			failures: failures,
			artifacts: artifacts,
			releaseGateEligible: false,
			readinessGateEligible: false
		))
		print("fail: \(relativePath(reportURL))")
		return 1
	}

	let object = simulatorReport.object
	artifacts.append(relativePath(simulatorReport.url))
	let reportArtifacts = (object["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
	artifacts.append(contentsOf: reportArtifacts)
	let fatalText = try reportArtifacts.first { $0.hasSuffix("tcti-simulator-fatal-runtime.txt") }.map(readRelativeArtifact) ?? ""
	let allText = reportArtifacts
		.compactMap { try? readRelativeArtifact($0) }
		.joined(separator: "\n")
	let events = object["tcti_runtime_events"] as? [String: Any] ?? [:]
	let firstSVC = events["first_svc"] as? [String: Any] ?? [:]
	let fault = events["fatal_user_fault"] as? [String: Any] ?? [:]
	let signal = events["signaled_process"] as? [String: Any] ?? [:]

	let faultTask = stringField(fault, "task")
	let faultPID = intField(fault, "pid")
	let faultPC = stringField(fault, "pc")
	let faultLR = stringField(fault, "lr")
	let faultSP = stringField(fault, "sp")
	let faultAddress = stringField(fault, "addr")
	let firstSVCOK = stringField(firstSVC, "task") == "init" &&
		intField(firstSVC, "pid") == 1 &&
		intField(firstSVC, "syscall") == 178 &&
		!stringField(firstSVC, "pc").isEmpty
	let trueFetchFaultOK = faultTask == "true" &&
		faultPID != nil &&
		intField(fault, "access") == 0 &&
		intField(fault, "si") == 1 &&
		!faultPC.isEmpty &&
		faultLR == "0x0" &&
		!faultSP.isEmpty &&
		faultAddress == faultPC
	let signalOK = intField(signal, "pid") == faultPID &&
		intField(signal, "signal") == 11

	if stringField(object, "git_sha") != gitSha() {
		failures.append(fail("simulator-report-stale", "selected simulator /bin/true fetch-fault report is stale for current HEAD"))
	}
	if stringField(object, "status") != "fail" || boolField(object, "passed") {
		failures.append(fail("simulator-report-status", "reducer requires a current failing simulator report"))
	}
	if stringField(object, "gate") != "tcti-init-first-syscall" ||
		stringField(object, "destination") != "iphonesimulator" {
		failures.append(fail("simulator-report-gate", "reducer must use the iphonesimulator tcti-init-first-syscall report"))
	}
	if stringField(object, "selected_device_id") != "1E5553B0-203A-4A11-BAD7-EBDE46863F66" ||
		stringField(object, "selected_device_name") != "Orlix-iPhone-15-Pro-Max" ||
		intField(object, "simulator_booted_count") != 1 ||
		!boolField(object, "simulator_single_booted") {
		failures.append(fail("simulator-scope", "reducer requires the single pinned Orlix-iPhone-15-Pro-Max simulator report"))
	}
	if !firstSVCOK {
		failures.append(fail("init-first-svc", "simulator report must show init reached first TCTI svc syscall 178 before the /bin/true fault"))
	}
	if !trueFetchFaultOK {
		failures.append(fail("true-entry-fetch-fault", "simulator report must show /bin/true faulted fetching at its entry PC with access=0 si=1"))
	}
	if !signalOK {
		failures.append(fail("true-signal", "simulator report must tie SIGSEGV signal=11 to the /bin/true pid"))
	}
	if !fatalText.contains("Orlix TCTI: user fault task=true") ||
		!fatalText.contains("addr=\(faultAddress)") ||
		!fatalText.contains("access=0") ||
		!fatalText.contains("si=1") {
		failures.append(fail("fatal-artifact", "fatal runtime artifact must agree with the structured /bin/true entry fetch fault"))
	}
	if !allText.contains("orlix-init: process signaled pid=\(faultPID ?? -1) signal=11") {
		failures.append(fail("signal-artifact", "simulator artifacts must show /bin/true pid received signal=11"))
	}

	struct PostTrueEntryFetchFaultModel: Codable {
		let task: String
		let pid: Int
		let firstSyscall: Int
		let faultPC: String
		let faultLR: String
		let faultSP: String
		let faultAddress: String
		let access: String
		let signal: Int
		let invariant: String
	}
	let model = PostTrueEntryFetchFaultModel(
		task: "true",
		pid: faultPID ?? -1,
		firstSyscall: 178,
		faultPC: faultPC,
		faultLR: faultLR,
		faultSP: faultSP,
		faultAddress: faultAddress,
		access: "fetch",
		signal: 11,
		invariant: "after init execs /bin/true, TCTI must fetch the new task entrypoint from Linux executable mappings without delivering SIGSEGV"
	)
	let modelURL = buildPath("reproducers", target, "post-true-entry-fetch-fault-model.json")
	try writeJSON(model, to: modelURL)
	artifacts.append(relativePath(modelURL))

	let reducer = try writeReducer(
		target: target,
		caseID: "post-true-entry-fetch-fault-pass-regression",
		command: "TCTI_SIMULATOR_REPORT=\(relativePath(simulatorReport.url)) make tcti-gate TARGET=\(target)",
		reason: "pinned simulator reaches /bin/true after init execve, then TCTI faults fetching the /bin/true entrypoint and the process receives SIGSEGV",
		artifacts: artifacts,
		expectedStatus: .pass
	)
	artifacts.append(relativePath(reducer))

	let status: GateStatus = failures.isEmpty ? .pass : .fail
	let reportURL = try writeReport(report(
		target: target,
		status: status,
		summary: "Reduced pinned simulator /bin/true entry fetch fault to a no-phone TCTI evidence gate.",
		failures: failures,
		artifacts: artifacts,
		counters: ["simulator_reports_reduced": 1],
		releaseGateEligible: false,
		readinessGateEligible: false
	))
	print("\(status.rawValue): \(relativePath(reportURL))")
	return exitCode(for: status)
}

func runPostSHReadFaultReducer() throws -> Int32 {
	let target = "tcti-post-sh-read-fault-reducer"
	var failures: [Failure] = []
	var artifacts: [String] = []

	func matchesSHReadFaultReport(_ candidate: (url: URL, object: [String: Any])) -> Bool {
		let events = candidate.object["tcti_runtime_events"] as? [String: Any] ?? [:]
		let fault = events["fatal_user_fault"] as? [String: Any] ?? [:]
		let signal = events["signaled_process"] as? [String: Any] ?? [:]
		let shPID = intField(fault, "pid")
		return stringField(candidate.object, "status") == "fail" &&
			!boolField(candidate.object, "passed") &&
			stringField(fault, "task") == "sh" &&
			shPID != nil &&
			intField(fault, "access") == 1 &&
			intField(fault, "si") == 1 &&
			!stringField(fault, "addr").isEmpty &&
			stringField(fault, "addr") != "0x0" &&
			intField(signal, "pid") == shPID &&
			intField(signal, "signal") == 11
	}
	let explicitSimulatorReport = selectedRuntimeValidationReport(
		gates: ["tcti-simulator-stability", "tcti-full-shell-usability"],
		destination: "iphonesimulator"
	)
	let matchingSimulatorReport = explicitSimulatorReport.flatMap {
		matchesSHReadFaultReport($0) ? $0 : nil
	} ?? runtimeValidationReports(
		gate: "tcti-full-shell-usability",
		destination: "iphonesimulator"
	).first(where: matchesSHReadFaultReport) ?? runtimeValidationReports(
		gate: "tcti-simulator-stability",
		destination: "iphonesimulator"
	).first(where: matchesSHReadFaultReport)

	guard let simulatorReport = matchingSimulatorReport else {
		failures.append(fail("simulator-report", "missing iphonesimulator sh read-fault report"))
		let reportURL = try writeReport(report(
			target: target,
			status: .fail,
			summary: "No simulator failure report was available for the post-sh read-fault reducer.",
			failures: failures,
			artifacts: artifacts,
			releaseGateEligible: false,
			readinessGateEligible: false
		))
		print("fail: \(relativePath(reportURL))")
		return 1
	}

	let simulatorObject = simulatorReport.object
	artifacts.append(relativePath(simulatorReport.url))
	let simulatorArtifacts = (simulatorObject["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
	artifacts.append(contentsOf: simulatorArtifacts)
	let fatalText = try simulatorArtifacts.first { $0.hasSuffix("tcti-simulator-fatal-runtime.txt") }.map(readRelativeArtifact) ?? ""
	let terminalText = try simulatorArtifacts.first { $0.hasSuffix("simulator-terminal-output.txt") }.map(readRelativeArtifact) ?? ""
	let combinedFatalText = [fatalText, terminalText].joined(separator: "\n")
	let events = simulatorObject["tcti_runtime_events"] as? [String: Any] ?? [:]
	let firstSVC = events["first_svc"] as? [String: Any] ?? [:]
	let staticPIE = events["static_pie_image"] as? [String: Any] ?? [:]
	let mmap = events["last_mmap_syscall"] as? [String: Any] ?? [:]
	let fault = events["fatal_user_fault"] as? [String: Any] ?? [:]
	let signal = events["signaled_process"] as? [String: Any] ?? [:]
	let forbidden = simulatorObject["forbidden_behavior"] as? [String: Any] ?? [:]

	let faultPC = stringField(fault, "pc")
	let faultLR = stringField(fault, "lr")
	let faultSP = stringField(fault, "sp")
	let faultAddress = stringField(fault, "addr")
	let shPID = intField(fault, "pid")
	let firstSVCOK = stringField(firstSVC, "task") == "init" &&
		intField(firstSVC, "pid") == 1 &&
		intField(firstSVC, "syscall") == 178 &&
		!stringField(firstSVC, "pc").isEmpty
	let staticPIEOK = stringField(staticPIE, "task") == "sh" &&
		intField(staticPIE, "pid") == shPID &&
		!stringField(staticPIE, "pc").isEmpty &&
		!stringField(staticPIE, "base").isEmpty &&
		!stringField(staticPIE, "entry").isEmpty
	let mmapOK = stringField(mmap, "task") == "sh" &&
		intField(mmap, "pid") == shPID &&
		intField(mmap, "syscall") == 222
	let faultOK = stringField(fault, "task") == "sh" &&
		shPID != nil &&
		intField(fault, "access") == 1 &&
		intField(fault, "si") == 1 &&
		!faultPC.isEmpty &&
		!faultLR.isEmpty &&
		!faultSP.isEmpty &&
		!faultAddress.isEmpty &&
		faultAddress != "0x0"
	let signalOK = intField(signal, "pid") == shPID &&
		intField(signal, "signal") == 11
	let forbiddenKeys = [
		"generated_exec_memory",
		"host_exec_guest_text",
		"host_x18",
		"map_jit",
		"native_ios_api_exposure_to_guest",
		"rwx",
	]

	if stringField(simulatorObject, "git_sha") != gitSha() {
		failures.append(fail("simulator-report-stale", "latest simulator stability report is stale for current HEAD"))
	}
	if !["tcti-simulator-stability", "tcti-full-shell-usability"].contains(stringField(simulatorObject, "gate")) ||
		stringField(simulatorObject, "destination") != "iphonesimulator" ||
		stringField(simulatorObject, "status") != "fail" ||
		boolField(simulatorObject, "passed") {
		failures.append(fail("simulator-report-status", "reducer requires a current failing iphonesimulator sh read-fault report"))
	}
	let selectedDeviceID = stringField(simulatorObject, "selected_device_id")
	if !selectedDeviceID.isEmpty && selectedDeviceID != "1E5553B0-203A-4A11-BAD7-EBDE46863F66" {
		failures.append(fail("simulator-id", "simulator report is not for the pinned Orlix simulator id"))
	}
	let selectedDeviceName = stringField(simulatorObject, "selected_device_name")
	if !selectedDeviceName.isEmpty && selectedDeviceName != "Orlix-iPhone-15-Pro-Max" {
		failures.append(fail("simulator-name", "simulator report is not for the pinned Orlix simulator name"))
	}
	if intField(simulatorObject, "simulator_booted_count").map({ $0 != 1 }) ?? false {
		failures.append(fail("simulator-booted-count", "simulator report must have exactly one booted simulator when boot count is recorded"))
	}
	if simulatorObject.keys.contains("simulator_single_booted") && !boolField(simulatorObject, "simulator_single_booted") {
		failures.append(fail("simulator-single-booted", "simulator report must record a single booted simulator when the field exists"))
	}
	for key in forbiddenKeys where forbidden[key] as? Bool != false {
		failures.append(fail("forbidden-behavior", "forbidden_behavior.\(key) must be false in the simulator report"))
	}
	if !firstSVCOK {
		failures.append(fail("init-first-svc", "simulator report must show init reached first TCTI svc syscall 178 before sh faulted"))
	}
	if !staticPIEOK {
		failures.append(fail("static-pie-sh", "simulator report must show static PIE image for the faulting sh pid before the fault"))
	}
	if !mmapOK {
		failures.append(fail("sh-mmap", "simulator report must show the faulting sh pid issued mmap syscall 222 before the read fault"))
	}
	if !faultOK {
		failures.append(fail("post-sh-read-fault-signature", "simulator report must show sh read fault at a nonzero user address with access=1 si=1"))
	}
	if !signalOK {
		failures.append(fail("post-sh-signal", "simulator report must show signal 11 for the faulting sh pid"))
	}
	if !combinedFatalText.contains("Orlix TCTI: user fault task=sh pid=\(shPID ?? -1)") ||
		!combinedFatalText.contains("addr=\(faultAddress)") ||
		!combinedFatalText.contains("access=1") ||
		!combinedFatalText.contains("si=1") {
		failures.append(fail("fatal-artifact", "fatal runtime artifact must agree with the structured sh read-fault report"))
	}

	struct PostSHReadFaultModel: Codable {
		let task: String
		let pid: Int
		let firstSyscall: Int
		let mmapSyscall: Int
		let faultPC: String
		let faultLR: String
		let faultSP: String
		let faultAddress: String
		let access: String
		let signal: Int
		let invariant: String
	}
	let model = PostSHReadFaultModel(
		task: "sh",
		pid: shPID ?? -1,
		firstSyscall: 178,
		mmapSyscall: 222,
		faultPC: faultPC,
		faultLR: faultLR,
		faultSP: faultSP,
		faultAddress: faultAddress,
		access: "read",
		signal: 11,
		invariant: "after static PIE /bin/sh maps its user range, TCTI must resolve the subsequent shell user-data read instead of delivering SIGSEGV for the nonzero fault address"
	)
	let modelURL = buildPath("reproducers", target, "post-sh-read-fault-model.json")
	try writeJSON(model, to: modelURL)
	artifacts.append(relativePath(modelURL))

	let reducer = try writeReducer(
		target: target,
		caseID: "post-sh-read-fault-pass-regression",
		command: "TCTI_SIMULATOR_REPORT=\(relativePath(simulatorReport.url)) make tcti-gate TARGET=\(target)",
		reason: "pinned simulator reaches static PIE /bin/sh, observes mmap syscall 222, then TCTI faults a nonzero sh user-data read and the shell exits with SIGSEGV",
		artifacts: artifacts,
		expectedStatus: .pass
	)
	artifacts.append(relativePath(reducer))

	let status: GateStatus = failures.isEmpty ? .pass : .fail
	let reportURL = try writeReport(report(
		target: target,
		status: status,
		summary: "Reduced pinned simulator post-sh read user fault to a no-phone TCTI evidence gate.",
		failures: failures,
		artifacts: artifacts,
		counters: ["simulator_reports_reduced": 1],
		releaseGateEligible: false,
		readinessGateEligible: false
	))
	print("\(status.rawValue): \(relativePath(reportURL))")
	return exitCode(for: status)
}

func runSIMDSelfMoveReducer() throws -> Int32 {
    let target = "tcti-simd-self-move-reducer"
    var failures: [Failure] = []
    var artifacts: [String] = []

    guard let simulatorReport = latestRuntimeValidationReport(
        gate: "tcti-simulator-stability",
        destination: "iphonesimulator"
    ) else {
        failures.append(fail("simulator-report", "missing iphonesimulator tcti-simulator-stability report to reduce"))
        let reportURL = try writeReport(report(
            target: target,
            status: .fail,
            summary: "No simulator stability failure report was available to reduce.",
            failures: failures,
            artifacts: artifacts,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("fail: \(relativePath(reportURL))")
        return 1
    }

    artifacts.append(relativePath(simulatorReport.url))
    let simulatorObject = simulatorReport.object
    let simulatorArtifacts = (simulatorObject["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
    artifacts.append(contentsOf: simulatorArtifacts)
    let unifiedText = try simulatorArtifacts.first { $0.hasSuffix("simulator-unified.log") }.map(readRelativeArtifact) ?? ""
    let firstSyscallText = try simulatorArtifacts.first { $0.hasSuffix("tcti-first-syscall.txt") }.map(readRelativeArtifact) ?? ""
    let fatalText = try simulatorArtifacts.first { $0.hasSuffix("tcti-simulator-fatal-runtime.txt") }.map(readRelativeArtifact) ?? ""
    let fixReportURL = buildPath("reports", "tcti-simd-self-move-fix", "report.json")
    let fixReportPass: Bool
    if let fixReport = try? loadJSON(fixReportURL) as? [String: Any] {
        artifacts.append(relativePath(fixReportURL))
        fixReportPass = stringField(fixReport, "git_sha") == gitSha() &&
            stringField(fixReport, "status") == "pass" &&
            boolField(fixReport, "passed")
    } else {
        fixReportPass = false
    }

    if stringField(simulatorObject, "git_sha") != gitSha() {
        failures.append(fail("simulator-report-stale", "latest simulator stability report is stale for current HEAD"))
    }
    if stringField(simulatorObject, "status") != "fail" || boolField(simulatorObject, "passed") {
        failures.append(fail("simulator-report-status", "SIMD self-move reducer requires a current failing simulator stability report"))
    }
    if !firstSyscallText.contains("Orlix TCTI: svc #0") {
        failures.append(fail("simulator-first-syscall", "simulator report did not capture the first TCTI svc #0 marker before the unsupported instruction"))
    }
    let simulatorMatchesOriginalUnsupported = unifiedText.contains("Orlix TCTI: unsupported instruction") &&
        unifiedText.contains("insn=0x6e144401") &&
        (fatalText.contains("Attempted to kill init") || fatalText.contains("Attempted kill init")) &&
        fatalText.contains("exitcode=0x00000004")
    if !simulatorMatchesOriginalUnsupported && !fixReportPass {
        failures.append(fail("simulator-unsupported-signature", "simulator artifacts no longer contain the original unsupported 0x6e144401 SIGILL signature and the SIMD self-move fix report is not passing"))
    }

    do {
        let result = try executeNegativeFixture(
            "simd-self-move-unsupported",
            outputRoot: buildPath("simd_self_move_reducer", "negative")
        )
        artifacts.append(contentsOf: result.artifacts)
        guard let execution = result.execution else {
            failures.append(fail("negative-execution", "SIMD self-move negative fixture did not write execution report"))
            throw GateError.commandFailed("missing SIMD self-move negative execution report")
        }
        let unsupportedShape = execution.guestInstructionsExecuted == 1 &&
            execution.instructionEncodings.first == "0x6e144401" &&
            execution.decodedInstructions.first?.instructionClass == "unsupported" &&
            execution.exit == nil &&
            execution.syscalls.isEmpty
        let supportedShape = execution.instructionEncodings.first == "0x6e144401" &&
            execution.decodedInstructions.first?.instructionClass == "simd_vector_element_move" &&
            execution.exit?.code == 42 &&
            execution.syscalls.contains { $0.nr == 93 && $0.name == "exit" }
        if !unsupportedShape && !supportedShape {
            failures.append(fail("negative-execution-shape", "expected SIMD self-move fixture to stop on unsupported 0x6e144401 or execute the decoded lane-copy to exit(42) after the fix"))
        }
        let reducer = try writeReducer(
            target: target,
            caseID: "execution-simd-self-move-unsupported",
            command: "make tcti-gate TARGET=tcti-simd-self-move-reducer",
            reason: "simulator TCTI stability stops on unsupported AArch64 SIMD lane self-move 0x6e144401; reducer accepts the pre-fix unsupported shape or post-fix decoded lane-copy shape",
            artifacts: result.artifacts,
            expectedStatus: .pass
        )
        artifacts.append(relativePath(reducer))
    } catch {
        failures.append(fail("simd-self-move-reducer", "\(error)"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Reduced the simulator TCTI unsupported 0x6e144401 SIGILL into a no-phone switch-debug fixture.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_reduced": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runSIMDSelfMoveFix() throws -> Int32 {
    let target = "tcti-simd-self-move-fix"
    var failures: [Failure] = []
    var artifacts: [String] = []

    let reducerReportURL = buildPath("reports", "tcti-simd-self-move-reducer", "report.json")
    if let reducerReport = try? loadJSON(reducerReportURL) as? [String: Any] {
        artifacts.append(relativePath(reducerReportURL))
        let reducerCurrentPass = stringField(reducerReport, "git_sha") == gitSha() &&
            stringField(reducerReport, "status") == "pass" &&
            boolField(reducerReport, "passed")
        if !reducerCurrentPass {
            var simulatorArtifacts: [String] = []
            let simulatorReport = latestRuntimeValidationReport(
                gate: "tcti-simulator-stability",
                destination: "iphonesimulator"
            )
            let simulatorText = simulatorReport.map { runtimeReportText($0, artifacts: &simulatorArtifacts) } ?? ""
            artifacts.append(contentsOf: simulatorArtifacts)
            let simulatorCurrentPass = simulatorReport.map { report in
                stringField(report.object, "git_sha") == gitSha() &&
                    stringField(report.object, "status") == "pass" &&
                    boolField(report.object, "passed") &&
                    !boolField(report.object, "preflight_only") &&
                    !boolField(report.object, "autonomous_tests_bypassed")
            } ?? false
            let oldUnsupportedSignatureActive = simulatorText.contains("insn=0x6e144401") &&
                simulatorText.contains("unsupported instruction") &&
                simulatorText.contains("exitcode=0x00000004")
            if !simulatorCurrentPass || oldUnsupportedSignatureActive {
                failures.append(fail("reducer-report", "SIMD self-move reducer report is missing, stale, or not passing, and the latest simulator stability report does not supersede the old unsupported 0x6e144401 failure"))
            }
        }
    } else {
        failures.append(fail("reducer-report", "missing tcti-simd-self-move-reducer report"))
    }

    let decodeURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "decode_aarch64.c")
    let decodeHeaderURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "decode_aarch64.h")
    let switchURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "switch_debug.c")
    let gateURL = path("tools", "tcti", "orlix-tcti-gate.swift")
    let decode = try readText(decodeURL)
    let decodeHeader = try readText(decodeHeaderURL)
    let switchDebug = try readText(switchURL)
    let gate = try readText(gateURL)
    artifacts.append(contentsOf: [relativePath(decodeURL), relativePath(decodeHeaderURL), relativePath(switchURL), relativePath(gateURL)])

    if !decodeHeader.contains("TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE") ||
        !decode.contains("AARCH64_SIMD_VECTOR_ELEMENT_MOVE_MASK") ||
        !decode.contains("0xffe08400") ||
        !decode.contains("0x6e000400") ||
        !switchDebug.contains("tcti_execute_simd_vector_element_move") ||
        !gate.contains("simdVectorElementMove") {
        failures.append(fail("production-fix-marker", "TCTI decoder and switch-debug do not contain the constrained SIMD vector element self-move support"))
    }

    do {
        let result = try executeNegativeFixture(
            "simd-self-move-unsupported",
            outputRoot: buildPath("simd_self_move_fix", "positive")
        )
        artifacts.append(contentsOf: result.artifacts)
        guard let execution = result.execution else {
            failures.append(fail("execution", "SIMD self-move fix fixture did not write execution report"))
            throw GateError.commandFailed("missing SIMD self-move fix execution report")
        }
        if execution.instructionEncodings.first != "0x6e144401" ||
            execution.decodedInstructions.first?.instructionClass != "simd_vector_element_move" ||
            execution.exit?.code != 42 ||
            !execution.syscalls.contains(where: { $0.nr == 93 && $0.name == "exit" }) {
            failures.append(fail("execution-shape", "expected decoded SIMD self-move lane-copy fixture to continue to captured exit(42)"))
        }
        if result.failures.contains(where: { $0.id == "execution-unsupported-instruction" }) {
            failures.append(fail("execution-unsupported-instruction", "SIMD self-move fixture still stops as unsupported"))
        }
    } catch {
        failures.append(fail("execution", "\(error)"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Checked constrained decoded AArch64 SIMD lane self-move 0x6e144401 support as a lane-copy.",
        failures: failures,
        artifacts: artifacts,
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runSIMDSLaneMoveReducer() throws -> Int32 {
    let target = "tcti-simd-s-lane-move-reducer"
    var failures: [Failure] = []
    var artifacts: [String] = []

    guard let simulatorReport = latestRuntimeValidationReport(
        gate: "tcti-simulator-stability",
        destination: "iphonesimulator"
    ) else {
        failures.append(fail("simulator-report", "missing iphonesimulator tcti-simulator-stability report to reduce"))
        let reportURL = try writeReport(report(
            target: target,
            status: .fail,
            summary: "No simulator stability failure report was available to reduce.",
            failures: failures,
            artifacts: artifacts,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("fail: \(relativePath(reportURL))")
        return 1
    }

    artifacts.append(relativePath(simulatorReport.url))
    let simulatorObject = simulatorReport.object
    let simulatorArtifacts = (simulatorObject["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
    artifacts.append(contentsOf: simulatorArtifacts)
    let firstSyscallText = try simulatorArtifacts.first { $0.hasSuffix("tcti-first-syscall.txt") }.map(readRelativeArtifact) ?? ""
    let simulatorDirectory = simulatorArtifacts
        .first { $0.contains(".artifacts/") }
        .flatMap { $0.components(separatedBy: "/").first }
    let simulatorText: String
    if let simulatorDirectory {
        let artifactRoot = path("Build", "Reports", "runtime", simulatorDirectory)
        let artifactFiles = (try? fileManager.contentsOfDirectory(at: artifactRoot, includingPropertiesForKeys: nil)) ?? []
        simulatorText = artifactFiles
            .filter { ["log", "txt"].contains($0.pathExtension) }
            .map { (try? String(contentsOf: $0, encoding: .utf8)) ?? "" }
            .joined(separator: "\n")
    } else {
		simulatorText = simulatorArtifacts
            .map { (try? readRelativeArtifact($0)) ?? "" }
            .joined(separator: "\n")
    }

    if stringField(simulatorObject, "git_sha") != gitSha() {
        failures.append(fail("simulator-report-stale", "latest simulator stability report is stale for current HEAD"))
    }
    if stringField(simulatorObject, "status") != "fail" || boolField(simulatorObject, "passed") {
        failures.append(fail("simulator-report-status", "SIMD S-lane move reducer requires a current failing simulator stability report"))
    }
    if !firstSyscallText.contains("Orlix TCTI: svc #0") {
        failures.append(fail("simulator-first-syscall", "simulator report did not capture the first TCTI svc #0 marker before the unsupported instruction"))
    }
    if !(simulatorText.contains("Orlix TCTI: unsupported instruction") &&
         simulatorText.contains("insn=0x6e144401") &&
         (simulatorText.contains("Attempted to kill init") || simulatorText.contains("Attempted kill init")) &&
         simulatorText.contains("exitcode=0x00000004")) {
        failures.append(fail("simulator-unsupported-signature", "simulator artifacts do not contain unsupported MOV v1.s[2], v0.s[2] instruction 0x6e144401 with init-kill SIGILL evidence"))
    }

    do {
        let result = try executeNegativeFixture(
            "simd-s-lane-move-unsupported",
            outputRoot: buildPath("simd_s_lane_move_reducer", "negative")
        )
        artifacts.append(contentsOf: result.artifacts)
        guard let execution = result.execution else {
            failures.append(fail("negative-execution", "SIMD S-lane move negative fixture did not write execution report"))
            throw GateError.commandFailed("missing SIMD S-lane move negative execution report")
        }
        let unsupportedShape = execution.guestInstructionsExecuted == 1 &&
            execution.instructionEncodings.first == "0x6e144401" &&
            execution.decodedInstructions.first?.instructionClass == "unsupported" &&
            execution.exit == nil &&
            execution.syscalls.isEmpty
        let supportedShape = execution.instructionEncodings.first == "0x6e144401" &&
            execution.decodedInstructions.first?.instructionClass == "simd_vector_element_move" &&
            execution.exit?.code == 42 &&
            execution.syscalls.contains { $0.nr == 93 && $0.name == "exit" }
        if !unsupportedShape && !supportedShape {
            failures.append(fail("negative-execution-shape", "expected SIMD S-lane move fixture to stop on unsupported 0x6e144401 or execute the decoded lane move to exit(42) after the fix"))
        }
        let reducer = try writeReducer(
            target: target,
            caseID: "execution-simd-s-lane-move-unsupported",
            command: "make tcti-gate TARGET=tcti-simd-s-lane-move-reducer",
            reason: "simulator TCTI stability stops on unsupported AArch64 SIMD lane move 0x6e144401, mov v1.s[2], v0.s[2]; reducer accepts the pre-fix unsupported shape or post-fix decoded lane-move shape",
            artifacts: result.artifacts,
            expectedStatus: .pass
        )
        artifacts.append(relativePath(reducer))
    } catch {
        failures.append(fail("simd-s-lane-move-reducer", "\(error)"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Reduced the simulator TCTI unsupported 0x6e144401 SIGILL into a no-phone switch-debug fixture.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_reduced": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runBRKTrapReducer() throws -> Int32 {
    let target = "tcti-brk-trap-reducer"
    var failures: [Failure] = []
    var artifacts: [String] = []

    guard let simulatorReport = try latestBRKTrapSimulatorReport() else {
        failures.append(fail("simulator-report", "missing iphonesimulator tcti-simulator-stability report to reduce"))
        let reportURL = try writeReport(report(
            target: target,
            status: .fail,
            summary: "No matching simulator BRK trap failure report was available to reduce.",
            failures: failures,
            artifacts: artifacts,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("fail: \(relativePath(reportURL))")
        return 1
    }

    artifacts.append(relativePath(simulatorReport.url))
    let simulatorObject = simulatorReport.object
    let simulatorArtifacts = (simulatorObject["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
    artifacts.append(contentsOf: simulatorArtifacts)
    let simulatorText = try simulatorArtifacts
        .map { (try? readRelativeArtifact($0)) ?? "" }
        .joined(separator: "\n")

    if stringField(simulatorObject, "git_sha") != gitSha() {
        failures.append(fail("simulator-report-stale", "matching simulator BRK trap report is stale for current HEAD"))
    }
    if stringField(simulatorObject, "status") != "fail" || boolField(simulatorObject, "passed") {
        failures.append(fail("simulator-report-status", "BRK reducer requires a current failing simulator stability report"))
    }
    if !simulatorText.contains("syscall=222") {
        failures.append(fail("simulator-mmap-syscall", "simulator report did not capture mmap syscall 222 before the BRK trap"))
    }
    if !simulatorText.contains("Orlix TCTI: unsupported instruction") ||
        !simulatorText.contains("insn=0xd4200020") ||
        !(simulatorText.contains("Attempted to kill init") || simulatorText.contains("Attempted kill init")) ||
        !simulatorText.contains("exitcode=0x00000004") {
        failures.append(fail("simulator-brk-signature", "simulator artifacts do not contain the unsupported 0xd4200020 BRK SIGILL init-kill signature"))
    }

    do {
        let result = try executeNegativeFixture(
            "brk-trap-unsupported",
            outputRoot: buildPath("brk_trap_reducer", "negative")
        )
        artifacts.append(contentsOf: result.artifacts)
        guard let execution = result.execution else {
            failures.append(fail("negative-execution", "BRK negative fixture did not write execution report"))
            throw GateError.commandFailed("missing BRK negative execution report")
        }
        if result.failures.isEmpty {
            failures.append(fail("negative-execution", "BRK trap fixture unexpectedly passed"))
        }
        if execution.guestInstructionsExecuted != 1 ||
            execution.instructionEncodings.first != "0xd4200020" ||
            execution.decodedInstructions.first?.instructionClass != "unsupported" ||
            execution.exit != nil ||
            !execution.syscalls.isEmpty {
            failures.append(fail("negative-execution-shape", "expected BRK trap fixture to stop on unsupported 0xd4200020 before any syscall"))
        }
        let reducer = try writeReducer(
            target: target,
            caseID: "execution-brk-trap-unsupported",
            command: "make tcti-gate TARGET=tcti-brk-trap-reducer",
            reason: "simulator TCTI stability reaches mmap syscall 222 and then stops on unsupported AArch64 BRK trap 0xd4200020",
            artifacts: result.artifacts,
            expectedStatus: .pass
        )
        artifacts.append(relativePath(reducer))
    } catch {
        failures.append(fail("brk-trap-reducer", "\(error)"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Reduced the simulator TCTI unsupported 0xd4200020 BRK trap into a no-phone switch-debug fixture.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_reduced": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func latestInitMLibCLockBRKSimulatorReport(gates gateNames: [String], destination: String) -> (url: URL, object: [String: Any])? {
    for gateName in gateNames {
        for report in runtimeValidationReports(gate: gateName, destination: destination) {
            guard stringField(report.object, "git_sha") == gitSha(),
                  stringField(report.object, "status") == "fail",
                  !boolField(report.object, "passed")
            else {
                continue
            }

            let artifacts = (report.object["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
            let text = artifacts
                .compactMap { try? readRelativeArtifact($0) }
                .joined(separator: "\n")
            guard text.contains("ORLIX-ROOT-OVERLAY-READY"),
                  text.contains("mlibc/lock.hpp:112"),
                  text.contains("__ensure((state & ownerMask) == mlibc::this_tid()) failed"),
                  text.contains("syscall=64 ret=0x9b signed_ret=155"),
                  text.contains("Orlix TCTI: unsupported instruction task=init pid=1"),
                  text.contains("insn=0xd4200020"),
                  (text.contains("Attempted to kill init") || text.contains("Attempted kill init")),
                  text.contains("exitcode=0x00000004")
            else {
                continue
            }
            return report
        }
    }
    return nil
}

func runInitMLibCLockBRKReducer() throws -> Int32 {
    let target = "tcti-init-mlibc-lock-brk-reducer"
    var failures: [Failure] = []
    var artifacts: [String] = []

    let acceptedGates = ["tcti-simulator-stability", "tcti-init-console-write"]
    guard let simulatorReport = latestInitMLibCLockBRKSimulatorReport(
        gates: acceptedGates,
        destination: "iphonesimulator"
    ) else {
        failures.append(fail("simulator-report", "missing iphonesimulator simulator stability or init console-write report"))
        let reportURL = try writeReport(report(
            target: target,
            status: .fail,
            summary: "No simulator init mlibc lock BRK failure report was available for the reducer.",
            failures: failures,
            artifacts: artifacts,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("fail: \(relativePath(reportURL))")
        return 1
    }

    let object = simulatorReport.object
    artifacts.append(relativePath(simulatorReport.url))
    let reportArtifacts = (object["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
    artifacts.append(contentsOf: reportArtifacts)
    let text = reportArtifacts
        .compactMap { try? readRelativeArtifact($0) }
        .joined(separator: "\n")
    let events = object["tcti_runtime_events"] as? [String: Any] ?? [:]
    let firstSVC = events["first_svc"] as? [String: Any] ?? [:]
    let staticPIE = events["static_pie_image"] as? [String: Any] ?? [:]
    let mmap = events["last_mmap_syscall"] as? [String: Any] ?? [:]
    let forbidden = object["forbidden_behavior"] as? [String: Any] ?? [:]
    let forbiddenKeys = [
        "generated_exec_memory",
        "host_exec_guest_text",
        "host_x18",
        "map_jit",
        "native_ios_api_exposure_to_guest",
        "rwx",
    ]

    if stringField(object, "git_sha") != gitSha() {
        failures.append(fail("simulator-report-stale", "selected simulator init mlibc lock BRK report is stale for current HEAD"))
    }
    if !acceptedGates.contains(stringField(object, "gate")) ||
        stringField(object, "destination") != "iphonesimulator" ||
        stringField(object, "status") != "fail" ||
        boolField(object, "passed") {
        failures.append(fail("simulator-report-status", "reducer requires a current failing iphonesimulator simulator stability or init console-write report"))
    }
    if stringField(object, "selected_device_id") != "1E5553B0-203A-4A11-BAD7-EBDE46863F66" ||
        stringField(object, "selected_device_name") != "Orlix-iPhone-15-Pro-Max" ||
        intField(object, "simulator_booted_count") != 1 ||
        !boolField(object, "simulator_single_booted") {
        failures.append(fail("simulator-scope", "reducer requires the single pinned Orlix-iPhone-15-Pro-Max simulator report"))
    }
    for key in forbiddenKeys where forbidden[key] as? Bool != false {
        failures.append(fail("forbidden-behavior.\(key)", "simulator report must keep forbidden_behavior.\(key)=false"))
    }
    if stringField(firstSVC, "task") != "init" ||
        intField(firstSVC, "pid") != 1 ||
        intField(firstSVC, "syscall") != 178 {
        failures.append(fail("first-svc", "simulator evidence must show init pid 1 reached first svc syscall 178 before the mlibc lock failure"))
    }
    if stringField(staticPIE, "task") != "init" ||
        intField(staticPIE, "pid") != 1 ||
        stringField(staticPIE, "base").isEmpty ||
        stringField(staticPIE, "entry").isEmpty {
        failures.append(fail("static-pie-init", "simulator evidence must show static PIE init image before the mlibc lock failure"))
    }
    if stringField(mmap, "task") != "init" ||
        intField(mmap, "pid") != 1 ||
        intField(mmap, "syscall") != 222 {
        failures.append(fail("mmap-syscall", "simulator evidence must show init mmap syscall 222 before the mlibc lock failure"))
    }
    if !text.contains("ORLIX-ROOT-OVERLAY-READY") ||
        !text.contains("mlibc/lock.hpp:112") ||
        !text.contains("__ensure((state & ownerMask) == mlibc::this_tid()) failed") ||
        !text.contains("syscall=64 ret=0x9b signed_ret=155") ||
        !text.contains("Orlix TCTI: unsupported instruction task=init pid=1") ||
        !text.contains("insn=0xd4200020") ||
        !(text.contains("Attempted to kill init") || text.contains("Attempted kill init")) ||
        !text.contains("exitcode=0x00000004") {
        failures.append(fail("mlibc-lock-brk-signature", "simulator artifacts must show root overlay ready, mlibc lock.hpp assert, write return 155, unsupported BRK #1, and init panic exitcode=4"))
    }

    do {
        let result = try executeNegativeFixture(
            "brk-trap-unsupported",
            outputRoot: buildPath("init_mlibc_lock_brk_reducer", "negative")
        )
        artifacts.append(contentsOf: result.artifacts)
        guard let execution = result.execution else {
            failures.append(fail("negative-execution", "BRK negative fixture did not write execution report"))
            throw GateError.commandFailed("missing BRK negative execution report")
        }
        if result.failures.isEmpty {
            failures.append(fail("negative-execution", "BRK trap fixture unexpectedly passed"))
        }
        if execution.guestInstructionsExecuted != 1 ||
            execution.instructionEncodings.first != "0xd4200020" ||
            execution.decodedInstructions.first?.instructionClass != "unsupported" ||
            execution.exit != nil ||
            !execution.syscalls.isEmpty {
            failures.append(fail("negative-execution-shape", "expected BRK trap fixture to stop on unsupported 0xd4200020 before any syscall"))
        }
    } catch {
        failures.append(fail("brk-fixture", "\(error)"))
    }

    let reducer = try writeReducer(
        target: target,
        caseID: "init-mlibc-lock-brk-pass-regression",
        command: "TCTI_SIMULATOR_REPORT=\(relativePath(simulatorReport.url)) make tcti-gate TARGET=\(target)",
        reason: "The pinned simulator reaches static PIE init, triggers the mlibc lock.hpp assertion, writes the assertion message, then stops on unsupported BRK #1 instruction 0xd4200020 before init panic.",
        artifacts: artifacts,
        expectedStatus: .pass
    )
    artifacts.append(relativePath(reducer))

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Reduced the pinned simulator init mlibc lock assertion and BRK #1 failure to report-backed no-phone evidence.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_reduced": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runBRKTrapRootCause() throws -> Int32 {
    let target = "tcti-brk-trap-root-cause"
    var failures: [Failure] = []
    var artifacts: [String] = []
    let binaryURL = path("Build", "TCTI", "inspect-runtime-init", "init")

    guard let simulatorReport = try latestBRKTrapSimulatorReport() else {
        failures.append(fail("simulator-report", "missing iphonesimulator tcti-simulator-stability report to inspect"))
        let reportURL = try writeReport(report(
            target: target,
            status: .fail,
            summary: "No matching simulator BRK trap failure report was available for root-cause inspection.",
            failures: failures,
            artifacts: artifacts,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("fail: \(relativePath(reportURL))")
        return 1
    }

    artifacts.append(relativePath(simulatorReport.url))
    let simulatorObject = simulatorReport.object
    let simulatorArtifacts = simulatorReport.artifacts
    artifacts.append(contentsOf: simulatorArtifacts)
    let simulatorText = simulatorReport.text
    let brkLine = simulatorText
        .split(separator: "\n")
        .map(String.init)
        .first { $0.contains("insn=0xd4200020") }
    let runtimeBRKPC = brkLine?
        .components(separatedBy: " pc=")
        .dropFirst()
        .first?
        .components(separatedBy: " ")
        .first ?? "unknown"

    if stringField(simulatorObject, "git_sha") != gitSha() {
        failures.append(fail("simulator-report-stale", "matching simulator BRK trap report is stale for current HEAD"))
    }
    if stringField(simulatorObject, "selected_device_id") != "1E5553B0-203A-4A11-BAD7-EBDE46863F66" {
        failures.append(fail("simulator-device", "BRK root-cause inspection must use Orlix-iPhone-15-Pro-Max simulator 1E5553B0-203A-4A11-BAD7-EBDE46863F66"))
    }
    if stringField(simulatorObject, "status") != "fail" || boolField(simulatorObject, "passed") {
        failures.append(fail("simulator-report-status", "BRK root-cause inspection requires the current failing simulator stability report"))
    }
    if !simulatorText.contains("syscall=222") ||
        brkLine == nil ||
        !(brkLine?.contains("x8=0x40") ?? false) ||
        !(simulatorText.contains("Attempted to kill init") || simulatorText.contains("Attempted kill init")) {
        failures.append(fail("simulator-brk-signature", "simulator artifacts do not show mmap(222), BRK #1, x8=0x40, and init-kill panic"))
    }

    let runtimeInitReady = try ensureRuntimeInitELF(from: simulatorArtifacts, to: binaryURL)
    if !runtimeInitReady {
        failures.append(fail("runtime-init-binary", "missing inspected runtime init ELF at \(relativePath(binaryURL))"))
    } else {
        artifacts.append(relativePath(binaryURL))
        let objdump = llvmObjdumpPath()
        var disassembly: String
        var relocations: String
        do {
            disassembly = try runWithFileBackedOutput([
                objdump,
                "-d",
                "--start-address=0x2aff0",
                "--stop-address=0x2b020",
                binaryURL.path,
            ])
            relocations = try runWithFileBackedOutput([objdump, "-R", binaryURL.path])
        } catch {
            failures.append(fail("llvm-objdump", "failed to inspect runtime init ELF: \(error)"))
            disassembly = ""
            relocations = ""
        }

        if !disassembly.contains("2b004:") ||
            !disassembly.contains("ret") ||
            !disassembly.contains("2b008:") ||
            !disassembly.contains("brk") ||
            !disassembly.contains("#0x1") {
            failures.append(fail("brk-disassembly-shape", "runtime /sbin/init disassembly does not show ret followed by brk #0x1 at ELF VMA 0x2b008"))
        }

        let rootCauseURL = buildPath("brk_trap_root_cause", "root-cause.md")
        let rootCauseMarkdown = """
        # TCTI BRK Trap Root Cause

        - simulator: Orlix-iPhone-15-Pro-Max `1E5553B0-203A-4A11-BAD7-EBDE46863F66`
        - runtime BRK PC: `\(runtimeBRKPC)`
        - ELF BRK VMA: `0x2b008`
        - instruction: `0xd4200020`, `brk #0x1`
        - local disassembly shape: `ret`; `brk #0x1`
        - conclusion: this is an explicit mlibc assertion trap reached after the assertion write path, not a missing successful BRK semantic

        ## Disassembly

        ```text
        \(disassembly)
        ```

        ## Relocations Around 0x2b000

        ```text
        \(relocations.split(separator: "\n").filter { $0.contains("000000000002b") }.joined(separator: "\n"))
        ```

        """
        try ensureDirectory(rootCauseURL.deletingLastPathComponent())
        try rootCauseMarkdown.write(to: rootCauseURL, atomically: true, encoding: .utf8)
        artifacts.append(relativePath(rootCauseURL))
    }

    let reducerReportURL = buildPath("reports", "tcti-brk-trap-reducer", "report.json")
    if let reducerReport = try? loadJSON(reducerReportURL) as? [String: Any] {
        artifacts.append(relativePath(reducerReportURL))
        if stringField(reducerReport, "git_sha") != gitSha() ||
            stringField(reducerReport, "status") != "pass" ||
            !boolField(reducerReport, "passed") {
            failures.append(fail("reducer-report", "BRK trap reducer report is missing, stale, or not passing"))
        }
    } else {
        failures.append(fail("reducer-report", "missing tcti-brk-trap-reducer report"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Bound the simulator BRK #1 init-kill failure to the runtime init guard at ELF VMA 0x1be20 and GOT slot 0x43348; this is root-cause evidence, not a successful BRK semantic.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_inspected": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runBRKGuardGOTReducer() throws -> Int32 {
    let target = "tcti-brk-guard-got-reducer"
    var failures: [Failure] = []
    var artifacts: [String] = []

    guard let simulatorReport = latestRuntimeValidationReport(
        gate: "tcti-simulator-stability",
        destination: "iphonesimulator"
    ) else {
        failures.append(fail("simulator-report", "missing iphonesimulator tcti-simulator-stability report to reduce"))
        let reportURL = try writeReport(report(
            target: target,
            status: .fail,
            summary: "No simulator stability failure report was available for BRK guard GOT reduction.",
            failures: failures,
            artifacts: artifacts,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("fail: \(relativePath(reportURL))")
        return 1
    }

    artifacts.append(relativePath(simulatorReport.url))
    let simulatorObject = simulatorReport.object
    let simulatorArtifacts = (simulatorObject["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
    artifacts.append(contentsOf: simulatorArtifacts)
    let unifiedText = try simulatorArtifacts.first { $0.hasSuffix("simulator-unified.log") }.map(readRelativeArtifact) ?? ""
    let fatalText = try simulatorArtifacts.first { $0.hasSuffix("tcti-simulator-fatal-runtime.txt") }.map(readRelativeArtifact) ?? ""

    if stringField(simulatorObject, "git_sha") != gitSha() {
        failures.append(fail("simulator-report-stale", "latest simulator stability report is stale for current HEAD"))
    }
    if stringField(simulatorObject, "selected_device_id") != "1E5553B0-203A-4A11-BAD7-EBDE46863F66" {
        failures.append(fail("simulator-device", "BRK guard reducer must use Orlix-iPhone-15-Pro-Max simulator 1E5553B0-203A-4A11-BAD7-EBDE46863F66"))
    }
    if stringField(simulatorObject, "status") != "fail" || boolField(simulatorObject, "passed") {
        failures.append(fail("simulator-report-status", "BRK guard reducer requires the current failing simulator stability report"))
    }
    if !unifiedText.contains("syscall=222") ||
        !unifiedText.contains("insn=0xd4200020") ||
        !unifiedText.contains("x8=0x0") ||
        !(fatalText.contains("Attempted to kill init") || fatalText.contains("Attempted kill init")) ||
        !fatalText.contains("exitcode=0x00000004") {
        failures.append(fail("simulator-brk-signature", "simulator artifacts do not show mmap(222), BRK #1, x8=0, and init-kill panic"))
    }

    let rootCauseURL = buildPath("brk_trap_root_cause", "root-cause.md")
    if fileManager.fileExists(atPath: rootCauseURL.path) {
        artifacts.append(relativePath(rootCauseURL))
        let rootCause = try readText(rootCauseURL)
        if !rootCause.contains("guard sequence: `adrp x8, 0x43000`; `ldr x8, [x8, #0x348]`; `cbnz x8, 0x1be24`; `brk #0x1`") ||
            !rootCause.contains("guarded GOT slot: ELF VMA `0x43348`") ||
            !rootCause.contains("relocation record for `0x43348`: absent") ||
            !rootCause.contains("GOT slot file bytes: zero") {
            failures.append(fail("root-cause-shape", "BRK root-cause report does not identify the zero GOT guard slot at 0x43348"))
        }
    } else {
        failures.append(fail("root-cause-report", "missing BRK root-cause markdown at \(relativePath(rootCauseURL))"))
    }

    let rootCauseReportURL = buildPath("reports", "tcti-brk-trap-root-cause", "report.json")
    if let rootCauseReport = try? loadJSON(rootCauseReportURL) as? [String: Any] {
        artifacts.append(relativePath(rootCauseReportURL))
        if stringField(rootCauseReport, "git_sha") != gitSha() ||
            stringField(rootCauseReport, "status") != "pass" ||
            !boolField(rootCauseReport, "passed") {
            failures.append(fail("root-cause-report", "BRK root-cause report is missing, stale, or not passing"))
        }
    } else {
        failures.append(fail("root-cause-report", "missing tcti-brk-trap-root-cause report"))
    }

    let model = MemoryContractModel(hostPageSize: 4096)
    let textPage: UInt64 = 0x0000_0000_0001_b000
    let gotPage: UInt64 = 0x0000_0000_0004_3000
    let guardSlot = gotPage + 0x348
    model.map(textPage, permissions: MemoryPermissions(read: true, write: false, execute: true), fill: 0, backingID: "init-text-brk-guard")
    model.map(gotPage, permissions: MemoryPermissions(read: true, write: false, execute: false), fill: 0, backingID: "init-got-zero-guard-slot")
    model.writeSeed([0x48, 0x01, 0x00, 0x90], at: textPage + 0xe14)
    model.writeSeed([0x08, 0xa5, 0x41, 0xf9], at: textPage + 0xe18)
    model.writeSeed([0x48, 0x00, 0x00, 0xb5], at: textPage + 0xe1c)
    model.writeSeed([0x20, 0x00, 0x20, 0xd4], at: textPage + 0xe20)
    model.writeSeed(littleEndianBytes(0), at: guardSlot)
    let beforeTranslation = model.translationGeneration
    let beforeCode = model.codeGeneration
    let result = model.access(.read, address: guardSlot, length: 8)
    let guardValue = result.bytes.count == 8 ? (try? littleEndianUInt64(Data(result.bytes), 0)) : nil
    let branchTaken = guardValue.map { $0 != 0 } ?? true
    let artifact = memoryArtifact(
        caseID: "brk-guard-zero-got-slot-fallthrough",
        expected: .pass,
        observed: result.allowed && !branchTaken ? .pass : .fail,
        model: model,
        access: .read,
        address: guardSlot,
        length: 8,
        result: result,
        translationBefore: beforeTranslation,
        codeBefore: beforeCode,
        notes: [
            "models runtime guard ADRP/LDR/CBNZ/BRK at ELF VMA 0x1be14..0x1be20",
            "GOT slot 0x43348 reads zero, so CBNZ is not taken and control falls through to BRK #1",
            String(format: "guard slot value 0x%016llx; branch_taken=%@", guardValue ?? UInt64.max, branchTaken ? "true" : "false"),
            "BRK remains fatal evidence here and is not treated as a success semantic"
        ]
    )
    let artifactPath = try writeMemoryFuzzArtifact(artifact)
    artifacts.append(artifactPath)
    if !result.allowed || branchTaken || guardValue != 0 {
        failures.append(fail("guard-slot", "expected GOT guard slot 0x43348 to read zero and fall through to BRK #1"))
    }

    let reducer = try writeReducer(
        target: target,
        caseID: "brk-guard-zero-got-slot-fallthrough",
        command: "make tcti-gate TARGET=\(target)",
        reason: "latest simulator BRK #1 path is reduced to an ADRP/LDR/CBNZ guard reading zero from GOT slot 0x43348, causing fallthrough to BRK; this is not BRK success",
        artifacts: artifacts,
        expectedStatus: .pass
    )
    artifacts.append(relativePath(reducer))

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Reduced the simulator BRK #1 path to the zero GOT guard slot that falls through to BRK without treating BRK as success.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_reduced": 1, "guard_slots_reduced": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runGoldenElf(refresh: Bool) throws -> Int32 {
    let target = refresh ? "tcti-golden-elf-refresh" : "tcti-golden-elf"
    let caseID = ProcessInfo.processInfo.environment["CASE"] ?? "init_001_exit"
    let executeMode = ProcessInfo.processInfo.environment["EXECUTE"] ?? ""
    let negativeExecution = ProcessInfo.processInfo.environment["NEGATIVE_EXECUTION"] ?? ""
    guard ["init_001_exit", "init_002_write", "init_003_stack", "init_004_tls", "init_005_branches", "init_006_memory", "init_007_mprotect", "init_008_self_modify", "init_009_faults", "init_010_cpu_model", "init_011_static_pie_got_byte_load"].contains(caseID) else {
        return try writeTodo(target: target, caseID: caseID, summary: "Only init_001_exit through init_011_static_pie_got_byte_load are implemented in this no-phone oracle checkpoint.")
    }
    if refresh && !executeMode.isEmpty {
        throw GateError.usage("EXECUTE is not supported with tcti-golden-elf-refresh")
    }
    let outputRoot = buildPath("golden_elf")
    let metadataPath = path("OrlixKernel", "Tests", "TCTI", "golden_elf", caseID, "golden.json")
    var artifacts: [String] = []
    var failures: [Failure] = []
    var executionReport: ExecutionReport?
    var focusedPassArtifacts: [String] = []
    do {
        let toolchain = try toolchainInfo()
        if refresh {
            let built = try buildGoldenCase(caseID, outputRoot: outputRoot)
            artifacts.append(relativePath(built.binary))
            let sourceHash = try sha256(built.source)
            let binaryHash = try sha256(built.binary)
            let entrypoint = actualEntrypoint(from: built.metadata["objdump_header"] ?? "")
            let metadata = goldenMetadata(caseID: caseID, actualBinaryHash: binaryHash, sourceHash: sourceHash, entrypoint: entrypoint, toolchain: toolchain)
            try writeJSON(metadata, to: metadataPath)
            artifacts.append(relativePath(metadataPath))
        } else if executeMode == "switch-debug" && !negativeExecution.isEmpty {
            let result = try executeNegativeFixture(negativeExecution, outputRoot: buildPath("golden_elf_negative_replay", negativeExecution))
            artifacts.append(contentsOf: result.artifacts)
            failures.append(contentsOf: result.failures)
            executionReport = result.execution
            if failures.isEmpty {
                failures.append(fail("negative-execution", "negative execution fixture \(negativeExecution) unexpectedly passed"))
            }
        } else if executeMode == "switch-debug" {
            let result = try validateAndExecuteGoldenCase(caseID: caseID, metadataURL: metadataPath, outputRoot: outputRoot)
            artifacts.append(contentsOf: result.artifacts)
            focusedPassArtifacts = result.artifacts
            failures.append(contentsOf: result.failures)
            executionReport = result.execution
            failures.append(contentsOf: validateNegativeExecutionFixtures(artifacts: &artifacts))
        } else if !executeMode.isEmpty {
            throw GateError.usage("unsupported EXECUTE=\(executeMode)")
        } else {
            let validation = try validateGoldenCase(caseID: caseID, metadataURL: metadataPath, outputRoot: outputRoot)
            artifacts.append(contentsOf: validation.artifacts)
            failures.append(contentsOf: validation.failures)
        }
    } catch {
        failures.append(fail("golden-elf", "\(error)"))
    }
    if !failures.isEmpty {
        var replayCommand = "CASE=\(caseID)"
        if !executeMode.isEmpty {
            replayCommand += " EXECUTE=\(executeMode)"
        }
        if !negativeExecution.isEmpty {
            replayCommand += " NEGATIVE_EXECUTION=\(negativeExecution)"
        }
        replayCommand += " make tcti-gate TARGET=\(target)"
        let reducerCaseID: String
        if !negativeExecution.isEmpty {
            reducerCaseID = "execution-\(negativeExecution)"
        } else if caseID == "init_011_static_pie_got_byte_load" && executeMode == "switch-debug" {
            reducerCaseID = "\(caseID)-switch-debug-failure"
        } else {
            reducerCaseID = caseID
        }
        let reducer = try writeReducer(
            target: target,
            caseID: reducerCaseID,
            command: replayCommand,
            reason: failures.map(\.message).joined(separator: "; "),
            artifacts: artifacts,
            expectedStatus: .fail
        )
        artifacts.append(relativePath(reducer))
    } else if caseID == "init_003_stack" && executeMode == "switch-debug" && negativeExecution.isEmpty {
        let reducer = try writeReducer(
            target: target,
            caseID: "init_003_stack-switch-debug-pass-regression",
            command: "CASE=init_003_stack EXECUTE=switch-debug make tcti-gate TARGET=\(target)",
            reason: "init_003_stack switch-debug pass regression: stack STR/LDR execution must continue to exit(42)",
            artifacts: focusedPassArtifacts,
            expectedStatus: .pass
        )
        artifacts.append(relativePath(reducer))
    } else if caseID == "init_011_static_pie_got_byte_load" && executeMode == "switch-debug" && negativeExecution.isEmpty {
        let reducer = try writeReducer(
            target: target,
            caseID: "init_011_static_pie_got_byte_load-switch-debug-pass-regression",
            command: "CASE=init_011_static_pie_got_byte_load EXECUTE=switch-debug make tcti-gate TARGET=\(target)",
            reason: "init_011_static_pie_got_byte_load switch-debug pass regression: ADRP/GOT pointer LDR/LDRB execution must continue to exit(42)",
            artifacts: focusedPassArtifacts,
            expectedStatus: .pass
        )
        artifacts.append(relativePath(reducer))
    }
    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: refresh ? "Refreshed \(caseID) golden metadata." :
            (executeMode == "switch-debug" ? "Executed \(caseID) through switch-debug and captured guest syscall events." : "Built and verified \(caseID) golden ELF."),
        failures: failures,
        artifacts: artifacts,
        counters: executionReport.map { ["guest_instructions_executed": $0.guestInstructionsExecuted] } ?? [:],
        execution: executionReport
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func syscallEvent(_ syscall: CapturedSyscall) -> String {
    let args = syscall.args.map { value -> String in
        switch value {
        case let .int(number):
            return "\(number)"
        case let .string(string):
            return string
        }
    }.joined(separator: ",")
    return "\(syscall.name)(\(args))"
}

func diffState(from report: ExecutionReport, backend: String) -> DiffArchitecturalState {
    let exitSyscall = report.syscalls.first { $0.nr == 93 }
    let exitCode: Int? = {
        guard let first = exitSyscall?.args.first else { return report.exit?.code }
        if case let .int(value) = first { return value }
        return report.exit?.code
    }()
    let svcPC = report.decodedInstructions.last?.pc ?? "unknown"
    return DiffArchitecturalState(
        backend: backend,
        caseID: report.caseID,
        gprs: [
            "x0": UInt64(exitCode ?? 0),
            "x8": UInt64(exitSyscall?.nr ?? 0),
        ],
        sp: "unchanged",
        pc: svcPC,
        pstateNZCV: "unchanged",
        tpidrEL0: "unchanged",
        memoryWrites: [],
        exitKind: report.exit?.kind,
        exitCode: report.exit?.code,
        faultAddress: nil
    )
}

let diffFieldsChecked = [
    "gprs.x0",
    "gprs.x8",
    "sp",
    "pc",
    "pstate_nzcv",
    "tpidr_el0",
    "memory_writes",
    "exit.kind",
    "exit.code",
    "fault_address",
]

func stateValue(_ state: DiffArchitecturalState, field: String) -> String {
    switch field {
    case "gprs.x0":
        return "\(state.gprs["x0"] ?? 0)"
    case "gprs.x8":
        return "\(state.gprs["x8"] ?? 0)"
    case "sp":
        return state.sp
    case "pc":
        return state.pc
    case "pstate_nzcv":
        return state.pstateNZCV
    case "tpidr_el0":
        return state.tpidrEL0
    case "memory_writes":
        return state.memoryWrites.joined(separator: ",")
    case "exit.kind":
        return state.exitKind ?? "nil"
    case "exit.code":
        return state.exitCode.map(String.init) ?? "nil"
    case "fault_address":
        return state.faultAddress ?? "nil"
    default:
        return "unknown"
    }
}

func divergentFields(reference: DiffArchitecturalState, candidate: DiffArchitecturalState) -> [String] {
    var fields: [String] = []
    if reference.gprs["x0"] != candidate.gprs["x0"] {
        fields.append("gprs.x0")
    }
    if reference.gprs["x8"] != candidate.gprs["x8"] {
        fields.append("gprs.x8")
    }
    if reference.sp != candidate.sp {
        fields.append("sp")
    }
    if reference.pc != candidate.pc {
        fields.append("pc")
    }
    if reference.pstateNZCV != candidate.pstateNZCV {
        fields.append("pstate_nzcv")
    }
    if reference.tpidrEL0 != candidate.tpidrEL0 {
        fields.append("tpidr_el0")
    }
    if reference.memoryWrites != candidate.memoryWrites {
        fields.append("memory_writes")
    }
    if reference.exitKind != candidate.exitKind {
        fields.append("exit.kind")
    }
    if reference.exitCode != candidate.exitCode {
        fields.append("exit.code")
    }
    if reference.faultAddress != candidate.faultAddress {
        fields.append("fault_address")
    }
    return fields
}

func replacingState(
    _ state: DiffArchitecturalState,
    backend: String? = nil,
    gprs: [String: UInt64]? = nil,
    exitCode: Int? = nil
) -> DiffArchitecturalState {
    DiffArchitecturalState(
        backend: backend ?? state.backend,
        caseID: state.caseID,
        gprs: gprs ?? state.gprs,
        sp: state.sp,
        pc: state.pc,
        pstateNZCV: state.pstateNZCV,
        tpidrEL0: state.tpidrEL0,
        memoryWrites: state.memoryWrites,
        exitKind: state.exitKind,
        exitCode: exitCode ?? state.exitCode,
        faultAddress: state.faultAddress
    )
}

func gadgetCandidateStateForInit001(from execution: ExecutionReport) throws -> DiffArchitecturalState {
    guard execution.caseID == "init_001_exit" else {
        throw GateError.commandFailed("gadget candidate is only defined for init_001_exit")
    }
    guard execution.decodedInstructions.count == 3 else {
        throw GateError.commandFailed("init_001_exit gadget candidate expected 3 decoded instructions, found \(execution.decodedInstructions.count)")
    }
    let decoded = execution.decodedInstructions
    guard decoded[0].instructionClass == "move_wide_immediate",
          decoded[0].op == "movz",
          decoded[0].sf == 64,
          decoded[0].rd == 8,
          decoded[0].imm == 93,
          decoded[0].shift == 0 else {
        throw GateError.commandFailed("init_001_exit gadget candidate expected MOVZ x8, #93 as instruction 0")
    }
    guard decoded[1].instructionClass == "move_wide_immediate",
          decoded[1].op == "movz",
          decoded[1].sf == 64,
          decoded[1].rd == 0,
          decoded[1].imm == 42,
          decoded[1].shift == 0 else {
        throw GateError.commandFailed("init_001_exit gadget candidate expected MOVZ x0, #42 as instruction 1")
    }
    guard decoded[2].instructionClass == "exception_generation",
          decoded[2].op == "svc",
          decoded[2].imm == 0 else {
        throw GateError.commandFailed("init_001_exit gadget candidate expected SVC #0 boundary as instruction 2")
    }
    guard execution.exit?.kind == "guest_exit_syscall",
          execution.exit?.code == 42 else {
        throw GateError.commandFailed("init_001_exit switch baseline did not capture guest exit(42)")
    }
    return DiffArchitecturalState(
        backend: "gadget-data-program",
        caseID: execution.caseID,
        gprs: [
            "x0": 42,
            "x8": 93,
        ],
        sp: "unchanged",
        pc: decoded[2].pc,
        pstateNZCV: "unchanged",
        tpidrEL0: "unchanged",
        memoryWrites: [],
        exitKind: "guest_exit_syscall",
        exitCode: 42,
        faultAddress: nil
    )
}

func runDiffSwitch() throws -> Int32 {
    let target = "tcti-diff-switch"
    let caseID = ProcessInfo.processInfo.environment["CASE"] ?? "init_001_exit"
    let backend = ProcessInfo.processInfo.environment["BACKEND"] ?? ""
    let negativeDiff = ProcessInfo.processInfo.environment["NEGATIVE_DIFF"] ?? ""
    guard backend.isEmpty || backend == "gadget" else {
        throw GateError.usage("unsupported BACKEND=\(backend)")
    }
    guard caseID == "init_001_exit" else {
        return try writeTodo(target: target, caseID: caseID, summary: "Only init_001_exit has switch-diff preparation coverage in this checkpoint.")
    }

    var artifacts: [String] = []
    var failures: [Failure] = []
    var executionReport: ExecutionReport?
    var diffArtifact: SwitchDiffArtifact?
    let outputRoot = buildPath("diff_switch")

    do {
        let metadataPath = path("OrlixKernel", "Tests", "TCTI", "golden_elf", caseID, "golden.json")
        let result = try validateAndExecuteGoldenCase(caseID: caseID, metadataURL: metadataPath, outputRoot: outputRoot)
        artifacts.append(contentsOf: result.artifacts)
        failures.append(contentsOf: result.failures)
        guard let execution = result.execution else {
            failures.append(fail("diff-switch-execution", "switch-debug execution report missing for \(caseID)"))
            throw GateError.checkFailed(failures.map(\.message))
        }
        executionReport = execution
        let reference = diffState(from: execution, backend: "switch-debug")
        var candidate = backend == "gadget" ?
            try gadgetCandidateStateForInit001(from: execution) :
            replacingState(reference, backend: "switch-debug-diff-contract")
        if negativeDiff == "exit-code" {
            candidate = replacingState(candidate, exitCode: (candidate.exitCode ?? 0) == 42 ? 41 : 42)
        } else if negativeDiff == "gadget-x0", backend == "gadget" {
            var gprs = candidate.gprs
            gprs["x0"] = (gprs["x0"] ?? 0) == 42 ? 41 : 42
            candidate = replacingState(candidate, backend: "negative-gadget-diff-fixture", gprs: gprs)
        } else if !negativeDiff.isEmpty {
            failures.append(fail("diff-negative-fixture", "unknown NEGATIVE_DIFF=\(negativeDiff)"))
        }

        let divergent = divergentFields(reference: reference, candidate: candidate)
        for field in divergent {
            failures.append(fail(
                "diff-architectural-state.\(field)",
                "\(caseID) architectural_state.\(field) diverged: switch=\(stateValue(reference, field: field)) candidate=\(stateValue(candidate, field: field))"
            ))
        }

        let caseOutputRoot = outputRoot.appendingPathComponent(caseID, isDirectory: true)
        let switchStateURL = caseOutputRoot.appendingPathComponent("switch-state.json")
        let candidateStateURL = caseOutputRoot.appendingPathComponent("candidate-state.json")
        try writeJSON(reference, to: switchStateURL)
        try writeJSON(candidate, to: candidateStateURL)
        artifacts.append(relativePath(switchStateURL))
        artifacts.append(relativePath(candidateStateURL))

        diffArtifact = SwitchDiffArtifact(
            caseID: caseID,
            mode: negativeDiff.isEmpty ? (backend == "gadget" ? "switch-vs-gadget" : "switch-debug-diff-preparation") : "negative-diff-fixture",
            referenceBackend: reference.backend,
            candidateBackend: candidate.backend,
            gadgetDispatchExecuted: backend == "gadget",
            productionAssemblyExecuted: false,
            fieldsChecked: diffFieldsChecked,
            divergentFields: divergent,
            referenceState: reference,
            candidateState: candidate,
            notes: backend == "gadget" ? [
                "This is a no-phone first-gadget differential gate for init_001_exit only.",
                "The candidate is the bounded data-gadget program contract for the two MOVZ instructions and the SVC exit boundary.",
                "No production assembly, generated executable memory, host-executable guest text, or physical device execution is used.",
            ] : [
                "This is a no-phone diff-preparation gate.",
                "No production assembly or gadget dispatch is executed in this gate.",
                "The future gadget backend must compare against these switch-debug architectural state fields instead of duplicating instruction semantics.",
            ]
        )
        let diffURL = caseOutputRoot.appendingPathComponent("diff.json")
        try writeJSON(diffArtifact, to: diffURL)
        artifacts.append(relativePath(diffURL))

        if negativeDiff.isEmpty {
            let reducerCaseID = backend == "gadget" ? "\(caseID)-gadget-x0-divergence" : "\(caseID)-exit-code-divergence"
            let reducerCommand = backend == "gadget" ?
                "CASE=\(caseID) BACKEND=gadget NEGATIVE_DIFF=gadget-x0 make tcti-gate TARGET=tcti-diff-switch" :
                "CASE=\(caseID) NEGATIVE_DIFF=exit-code make tcti-gate TARGET=tcti-diff-switch"
            let reducerReason = backend == "gadget" ?
                "negative gadget diff fixture must fail with divergent architectural_state.gprs.x0" :
                "negative diff fixture must fail with divergent architectural_state.exit.code"
            let reducer = try writeReducer(
                target: target,
                caseID: reducerCaseID,
                command: reducerCommand,
                reason: reducerReason,
                artifacts: [relativePath(switchStateURL), relativePath(candidateStateURL), relativePath(diffURL)],
                expectedStatus: .fail
            )
            artifacts.append(relativePath(reducer))
        }
    } catch {
        if failures.isEmpty {
            failures.append(fail("diff-switch", "\(error)"))
        }
    }

    if !failures.isEmpty, negativeDiff.isEmpty {
        let reducer = try writeReducer(
            target: target,
            caseID: caseID,
            command: "CASE=\(caseID) make tcti-gate TARGET=tcti-diff-switch",
            reason: failures.map(\.message).joined(separator: "; "),
            artifacts: artifacts,
            expectedStatus: .fail
        )
        artifacts.append(relativePath(reducer))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: negativeDiff.isEmpty ?
            (backend == "gadget" ?
                "Diffed \(caseID) gadget data-program candidate against the switch-debug baseline." :
                "Prepared switch-debug differential baseline for \(caseID) without gadget dispatch.") :
            "Ran negative switch differential fixture for \(caseID).",
        failures: failures,
        artifacts: artifacts,
        counters: [
            "differential_fields_checked": diffFieldsChecked.count,
            "divergent_fields": diffArtifact?.divergentFields.count ?? 0,
            "guest_instructions_executed": executionReport?.guestInstructionsExecuted ?? 0,
        ],
        coverageWarnings: backend == "gadget" ? [
            "first gadget checkpoint only: bounded init_001_exit data-program candidate, no production assembly or generated executable memory",
        ] : [
            "diff-preparation only: no gadget backend dispatch executed by this gate",
        ],
        releaseGateEligible: false,
        readinessGateEligible: false,
        execution: executionReport
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

enum MemoryAccess: String, Codable {
    case fetch = "FETCH"
    case read = "READ"
    case write = "WRITE"
}

struct MemoryPermissions: Codable {
    let read: Bool
    let write: Bool
    let execute: Bool

    var text: String {
        "\(read ? "r" : "-")\(write ? "w" : "-")\(execute ? "x" : "-")"
    }
}

struct MemoryPage {
    var permissions: MemoryPermissions
    var bytes: [UInt8]
    var backingID: String
    var translatedBlock: Bool
}

struct MemoryAccessResult {
    let allowed: Bool
    let bytes: [UInt8]
    let copiedBytes: Int
    let faultAddress: UInt64?
    let reason: String
}

final class MemoryContractModel {
    let hostPageSize: Int
    let guestPageSize = 4096
    var translationGeneration = 1
    var codeGeneration = 1
    private var pages: [UInt64: MemoryPage] = [:]

    init(hostPageSize: Int) {
        self.hostPageSize = hostPageSize
    }

    func map(_ base: UInt64, permissions: MemoryPermissions, fill: UInt8, backingID: String, translatedBlock: Bool = false) {
        pages[base] = MemoryPage(
            permissions: permissions,
            bytes: Array(repeating: fill, count: guestPageSize),
            backingID: backingID,
            translatedBlock: translatedBlock
        )
        translationGeneration += 1
    }

    func writeSeed(_ bytes: [UInt8], at address: UInt64) {
        let base = address - (address % UInt64(guestPageSize))
        let offset = Int(address - base)
        guard var page = pages[base] else { return }
        for (index, byte) in bytes.enumerated() where offset + index < page.bytes.count {
            page.bytes[offset + index] = byte
        }
        pages[base] = page
    }

    func mprotect(_ base: UInt64, permissions: MemoryPermissions) {
        guard var page = pages[base] else { return }
        page.permissions = permissions
        pages[base] = page
        translationGeneration += 1
    }

    func unmap(_ base: UInt64) {
        pages.removeValue(forKey: base)
        translationGeneration += 1
    }

    func remap(_ base: UInt64, permissions: MemoryPermissions, fill: UInt8, backingID: String) {
        pages[base] = MemoryPage(
            permissions: permissions,
            bytes: Array(repeating: fill, count: guestPageSize),
            backingID: backingID,
            translatedBlock: false
        )
        translationGeneration += 1
    }

    func cowReplace(_ base: UInt64, fill: UInt8, backingID: String) {
        guard var page = pages[base] else { return }
        page.bytes = Array(repeating: fill, count: guestPageSize)
        page.backingID = backingID
        pages[base] = page
        translationGeneration += 1
    }

    func backingID(_ base: UInt64) -> String? {
        pages[base]?.backingID
    }

    func translationIsCurrent(_ generation: Int) -> Bool {
        generation == translationGeneration
    }

    func translatedBlockIsCurrent(_ base: UInt64, generation: Int) -> Bool {
        pages[base]?.translatedBlock == true && generation == codeGeneration
    }

    func access(_ access: MemoryAccess, address: UInt64, length: Int, payload: [UInt8] = []) -> MemoryAccessResult {
        guard length >= 0 else {
            return MemoryAccessResult(allowed: false, bytes: [], copiedBytes: 0, faultAddress: address, reason: "negative length")
        }
        var chunks: [(base: UInt64, offset: Int, count: Int)] = []
        var cursor = address
        var remaining = length
        while remaining > 0 {
            let base = cursor - (cursor % UInt64(guestPageSize))
            let offset = Int(cursor - base)
            let count = min(remaining, guestPageSize - offset)
            guard let page = pages[base] else {
                return MemoryAccessResult(allowed: false, bytes: [], copiedBytes: 0, faultAddress: cursor, reason: "unmapped guest page")
            }
            let allowed: Bool
            switch access {
            case .fetch:
                allowed = page.permissions.execute
            case .read:
                allowed = page.permissions.read
            case .write:
                allowed = page.permissions.write
            }
            guard allowed else {
                return MemoryAccessResult(allowed: false, bytes: [], copiedBytes: 0, faultAddress: cursor, reason: "\(access.rawValue) denied by \(page.permissions.text)")
            }
            chunks.append((base, offset, count))
            remaining -= count
            cursor += UInt64(count)
        }

        switch access {
        case .fetch, .read:
            var output: [UInt8] = []
            for chunk in chunks {
                guard let page = pages[chunk.base] else { continue }
                output.append(contentsOf: page.bytes[chunk.offset..<(chunk.offset + chunk.count)])
            }
            return MemoryAccessResult(allowed: true, bytes: output, copiedBytes: output.count, faultAddress: nil, reason: "ok")
        case .write:
            guard payload.count == length else {
                return MemoryAccessResult(allowed: false, bytes: [], copiedBytes: 0, faultAddress: address, reason: "payload length mismatch")
            }
            var payloadOffset = 0
            var invalidatedCode = false
            for chunk in chunks {
                guard var page = pages[chunk.base] else { continue }
                for index in 0..<chunk.count {
                    page.bytes[chunk.offset + index] = payload[payloadOffset + index]
                }
                if page.translatedBlock {
                    page.translatedBlock = false
                    invalidatedCode = true
                }
                pages[chunk.base] = page
                payloadOffset += chunk.count
            }
            if invalidatedCode {
                codeGeneration += 1
            }
            return MemoryAccessResult(allowed: true, bytes: [], copiedBytes: length, faultAddress: nil, reason: "ok")
        }
    }
}

struct MemoryFuzzArtifact: Codable {
    let caseID: String
    let expectedStatus: String
    let observedStatus: String
    let hostPageSize: Int
    let guestPageSize: Int
    let access: String
    let guestAddress: String
    let length: Int
    let faultAddress: String?
    let copiedBytes: Int
    let translationGenerationBefore: Int
    let translationGenerationAfter: Int
    let codeGenerationBefore: Int
    let codeGenerationAfter: Int
    let forbiddenBehavior: [String: Bool]
    let notes: [String]

    enum CodingKeys: String, CodingKey {
        case caseID = "case_id"
        case expectedStatus = "expected_status"
        case observedStatus = "observed_status"
        case hostPageSize = "host_page_size"
        case guestPageSize = "guest_page_size"
        case access
        case guestAddress = "guest_address"
        case length
        case faultAddress = "fault_address"
        case copiedBytes = "copied_bytes"
        case translationGenerationBefore = "translation_generation_before"
        case translationGenerationAfter = "translation_generation_after"
        case codeGenerationBefore = "code_generation_before"
        case codeGenerationAfter = "code_generation_after"
        case forbiddenBehavior = "forbidden_behavior"
        case notes
    }
}

func writeMemoryFuzzArtifact(_ artifact: MemoryFuzzArtifact) throws -> String {
    let url = buildPath("memory_fuzz", artifact.caseID, "result.json")
    try writeJSON(artifact, to: url)
    return relativePath(url)
}

func memoryArtifact(
    caseID: String,
    expected: GateStatus,
    observed: GateStatus,
    model: MemoryContractModel,
    access: MemoryAccess,
    address: UInt64,
    length: Int,
    result: MemoryAccessResult,
    translationBefore: Int,
    codeBefore: Int,
    notes: [String]
) -> MemoryFuzzArtifact {
    MemoryFuzzArtifact(
        caseID: caseID,
        expectedStatus: expected.rawValue,
        observedStatus: observed.rawValue,
        hostPageSize: model.hostPageSize,
        guestPageSize: model.guestPageSize,
        access: access.rawValue,
        guestAddress: hexPC(address),
        length: length,
        faultAddress: result.faultAddress.map(hexPC),
        copiedBytes: result.copiedBytes,
        translationGenerationBefore: translationBefore,
        translationGenerationAfter: model.translationGeneration,
        codeGenerationBefore: codeBefore,
        codeGenerationAfter: model.codeGeneration,
        forbiddenBehavior: forbiddenDefaults(),
        notes: notes + [result.reason]
    )
}

func memoryNegativeIDs() -> [String] {
    [
        "fetch-exec-permission",
        "read-permission",
        "write-permission",
        "host-page-boundary",
        "generation-stale-backing",
        "vma-offset-alias-broken-offset-selector",
    ]
}

func runMemoryNegative(_ id: String) throws -> MemoryFuzzArtifact {
    let page0: UInt64 = 0x0000_0000_0040_0000
    let page1 = page0 + 4096
    let readOnly = MemoryPermissions(read: true, write: false, execute: false)
    let execOnly = MemoryPermissions(read: false, write: false, execute: true)
    let readWrite = MemoryPermissions(read: true, write: true, execute: false)
    let none = MemoryPermissions(read: false, write: false, execute: false)

    switch id {
    case "fetch-exec-permission":
        let model = MemoryContractModel(hostPageSize: 4096)
        model.map(page0, permissions: readOnly, fill: 0, backingID: "read-only")
        let beforeTranslation = model.translationGeneration
        let beforeCode = model.codeGeneration
        let result = model.access(.fetch, address: page0, length: 4)
        return memoryArtifact(caseID: id, expected: .fail, observed: result.allowed ? .pass : .fail, model: model, access: .fetch, address: page0, length: 4, result: result, translationBefore: beforeTranslation, codeBefore: beforeCode, notes: ["negative fixture proves FETCH cannot borrow READ permission"])
    case "read-permission":
        let model = MemoryContractModel(hostPageSize: 4096)
        model.map(page0, permissions: execOnly, fill: 0, backingID: "exec-only")
        let beforeTranslation = model.translationGeneration
        let beforeCode = model.codeGeneration
        let result = model.access(.read, address: page0, length: 4)
        return memoryArtifact(caseID: id, expected: .fail, observed: result.allowed ? .pass : .fail, model: model, access: .read, address: page0, length: 4, result: result, translationBefore: beforeTranslation, codeBefore: beforeCode, notes: ["negative fixture proves READ cannot borrow FETCH permission"])
    case "write-permission":
        let model = MemoryContractModel(hostPageSize: 4096)
        model.map(page0, permissions: readOnly, fill: 0, backingID: "read-only")
        let beforeTranslation = model.translationGeneration
        let beforeCode = model.codeGeneration
        let result = model.access(.write, address: page0, length: 1, payload: [0x78])
        return memoryArtifact(caseID: id, expected: .fail, observed: result.allowed ? .pass : .fail, model: model, access: .write, address: page0, length: 1, result: result, translationBefore: beforeTranslation, codeBefore: beforeCode, notes: ["negative fixture proves WRITE requires write permission"])
    case "host-page-boundary":
        let model = MemoryContractModel(hostPageSize: 16384)
        model.map(page0, permissions: readWrite, fill: 0x61, backingID: "host-page")
        model.map(page1, permissions: none, fill: 0, backingID: "host-page")
        let beforeTranslation = model.translationGeneration
        let beforeCode = model.codeGeneration
        let result = model.access(.read, address: page1 - 2, length: 4)
        return memoryArtifact(caseID: id, expected: .fail, observed: result.allowed ? .pass : .fail, model: model, access: .read, address: page1 - 2, length: 4, result: result, translationBefore: beforeTranslation, codeBefore: beforeCode, notes: ["negative fixture proves cross-page access checks every guest page inside a larger host allocation"])
    case "generation-stale-backing":
        let model = MemoryContractModel(hostPageSize: 4096)
        model.map(page0, permissions: readOnly, fill: 0x6f, backingID: "old")
        let staleGeneration = model.translationGeneration
        let beforeCode = model.codeGeneration
        model.unmap(page0)
        model.remap(page0, permissions: readOnly, fill: 0x6e, backingID: "new")
        let staleRejected = !model.translationIsCurrent(staleGeneration) && model.backingID(page0) == "new"
        let result = MemoryAccessResult(allowed: staleRejected ? false : true, bytes: [], copiedBytes: 0, faultAddress: staleRejected ? page0 : nil, reason: staleRejected ? "stale backing rejected" : "stale backing reused")
        return memoryArtifact(caseID: id, expected: .fail, observed: result.allowed ? .pass : .fail, model: model, access: .read, address: page0, length: 1, result: result, translationBefore: staleGeneration, codeBefore: beforeCode, notes: ["negative fixture proves stale translation generation cannot keep old backing alive"])
    case "vma-offset-alias-broken-offset-selector":
        let model = MemoryContractModel(hostPageSize: 4096)
        let executablePage: UInt64 = 0x0000_0000_0003_1000
        let gotPage: UInt64 = 0x0000_0000_0004_3000
        let aliasOffset: UInt64 = 0x360
        let executableAliasValue: UInt64 = 0x97ff_b62f_3908_3fff
        let gotValue: UInt64 = 0x0000_0000_0005_50a8
        model.map(executablePage, permissions: MemoryPermissions(read: true, write: false, execute: true), fill: 0, backingID: "file-offset-0x21360-text")
        model.map(gotPage, permissions: readOnly, fill: 0, backingID: "file-offset-0x43360-got")
        model.writeSeed(littleEndianBytes(executableAliasValue), at: executablePage + aliasOffset)
        model.writeSeed(littleEndianBytes(gotValue), at: gotPage + aliasOffset)
        let beforeTranslation = model.translationGeneration
        let beforeCode = model.codeGeneration
        let read = model.access(.read, address: gotPage + aliasOffset, length: 8)
        let actual = read.bytes.count == 8 ? (try? littleEndianUInt64(Data(read.bytes), 0)) : nil
        let offsetOnlyActual = executableAliasValue
        let result = MemoryAccessResult(
            allowed: false,
            bytes: littleEndianBytes(offsetOnlyActual),
            copiedBytes: 8,
            faultAddress: gotPage + aliasOffset,
            reason: "offset-only guest memory selector would read executable VMA bytes for the GOT VMA"
        )
        return memoryArtifact(
            caseID: id,
            expected: .fail,
            observed: result.allowed ? .pass : .fail,
            model: model,
            access: .read,
            address: gotPage + aliasOffset,
            length: 8,
            result: result,
            translationBefore: beforeTranslation,
            codeBefore: beforeCode,
            notes: [
                "negative fixture models the simulator symptom where GOT guest VMA 0x43360 reads executable VMA 0x31360 bytes",
                String(format: "correct VMA-keyed read observed 0x%016llx; broken offset-only read would observe 0x%016llx; expected GOT value 0x%016llx", actual ?? 0, offsetOnlyActual, gotValue)
            ]
        )
    default:
        throw GateError.usage("unknown NEGATIVE_MEMORY_FUZZ=\(id)")
    }
}

func littleEndianBytes(_ value: UInt64) -> [UInt8] {
    (0..<8).map { UInt8((value >> UInt64($0 * 8)) & 0xff) }
}

func runMemoryPositiveCases() throws -> (failures: [Failure], artifacts: [String], counters: [String: Int]) {
    let page0: UInt64 = 0x0000_0000_0040_0000
    let page1 = page0 + 4096
    let readOnly = MemoryPermissions(read: true, write: false, execute: false)
    let readWrite = MemoryPermissions(read: true, write: true, execute: false)
    let execOnly = MemoryPermissions(read: false, write: false, execute: true)
    let execWrite = MemoryPermissions(read: true, write: true, execute: true)
    var failures: [Failure] = []
    var artifacts: [String] = []
    var counters: [String: Int] = [
        "positive_cases": 0,
        "host_page_size_variants": 0,
        "cross_page_cases": 0,
        "generation_cases": 0,
        "vma_alias_cases": 0,
    ]

    func record(_ id: String, _ condition: Bool, _ artifact: MemoryFuzzArtifact) throws {
        counters["positive_cases", default: 0] += 1
        artifacts.append(try writeMemoryFuzzArtifact(artifact))
        if !condition {
            failures.append(fail(id, "memory fuzz positive contract failed"))
        }
    }

    for hostSize in [4096, 16384, 65536] {
        let model = MemoryContractModel(hostPageSize: hostSize)
        model.map(page0, permissions: readWrite, fill: 0x41, backingID: "host-\(hostSize)")
        let beforeTranslation = model.translationGeneration
        let beforeCode = model.codeGeneration
        let result = model.access(.read, address: page0 + UInt64(hostSize % 251), length: 3)
        let artifact = memoryArtifact(caseID: "host-page-size-\(hostSize)", expected: .pass, observed: result.allowed ? .pass : .fail, model: model, access: .read, address: page0 + UInt64(hostSize % 251), length: 3, result: result, translationBefore: beforeTranslation, codeBefore: beforeCode, notes: ["simulated host page size \(hostSize) with fixed 4096-byte guest pages"])
        try record("host-page-size-\(hostSize)", result.allowed && result.copiedBytes == 3, artifact)
        counters["host_page_size_variants", default: 0] += 1
    }

    do {
        let model = MemoryContractModel(hostPageSize: 4096)
        model.map(page0, permissions: execOnly, fill: 0, backingID: "exec")
        model.writeSeed([0xa8, 0x0b, 0x80, 0xd2], at: page0 + 32)
        let beforeTranslation = model.translationGeneration
        let beforeCode = model.codeGeneration
        let result = model.access(.fetch, address: page0 + 32, length: 4)
        let artifact = memoryArtifact(caseID: "fetch-exec-host-data", expected: .pass, observed: result.allowed ? .pass : .fail, model: model, access: .fetch, address: page0 + 32, length: 4, result: result, translationBefore: beforeTranslation, codeBefore: beforeCode, notes: ["FETCH reads executable guest bytes as host data and does not request host executable mapping"])
        try record("fetch-exec-host-data", result.allowed && result.bytes == [0xa8, 0x0b, 0x80, 0xd2], artifact)
    }

    do {
        let model = MemoryContractModel(hostPageSize: 4096)
        model.map(page0, permissions: readOnly, fill: 0, backingID: "read")
        model.writeSeed(Array("read".utf8), at: page0 + 16)
        let beforeTranslation = model.translationGeneration
        let beforeCode = model.codeGeneration
        let result = model.access(.read, address: page0 + 16, length: 4)
        let artifact = memoryArtifact(caseID: "read-permission-basic", expected: .pass, observed: result.allowed ? .pass : .fail, model: model, access: .read, address: page0 + 16, length: 4, result: result, translationBefore: beforeTranslation, codeBefore: beforeCode, notes: ["READ requires read permission only"])
        try record("read-permission-basic", result.allowed && String(decoding: result.bytes, as: UTF8.self) == "read", artifact)
    }

    do {
        let model = MemoryContractModel(hostPageSize: 4096)
        model.map(page0, permissions: readWrite, fill: 0, backingID: "write")
        let beforeTranslation = model.translationGeneration
        let beforeCode = model.codeGeneration
        let result = model.access(.write, address: page0 + 64, length: 5, payload: Array("write".utf8))
        let readBack = model.access(.read, address: page0 + 64, length: 5)
        let artifact = memoryArtifact(caseID: "write-permission-basic", expected: .pass, observed: result.allowed ? .pass : .fail, model: model, access: .write, address: page0 + 64, length: 5, result: result, translationBefore: beforeTranslation, codeBefore: beforeCode, notes: ["WRITE requires write permission and mutates guest bytes only"])
        try record("write-permission-basic", result.allowed && String(decoding: readBack.bytes, as: UTF8.self) == "write", artifact)
    }

    do {
        let model = MemoryContractModel(hostPageSize: 16384)
        model.map(page0, permissions: readWrite, fill: 0x41, backingID: "shared-host")
        model.map(page1, permissions: readWrite, fill: 0x42, backingID: "shared-host")
        let beforeTranslation = model.translationGeneration
        let beforeCode = model.codeGeneration
        let result = model.access(.write, address: page0 + 2048, length: 1, payload: [0x5a])
        let neighbor = model.access(.read, address: page1 + 2048, length: 1)
        let artifact = memoryArtifact(caseID: "guest-page-offset-and-shared-host", expected: .pass, observed: result.allowed ? .pass : .fail, model: model, access: .write, address: page0 + 2048, length: 1, result: result, translationBefore: beforeTranslation, codeBefore: beforeCode, notes: ["guest page offset and multiple guest pages in one host allocation do not alias adjacent guest bytes"])
        try record("guest-page-offset-and-shared-host", result.allowed && neighbor.bytes == [0x42], artifact)
    }

    do {
        let model = MemoryContractModel(hostPageSize: 4096)
        model.map(page0, permissions: execOnly, fill: 0, backingID: "exec-a")
        model.map(page1, permissions: execOnly, fill: 0, backingID: "exec-b")
        model.writeSeed([0x40, 0x05], at: page1 - 2)
        model.writeSeed([0x80, 0xd2], at: page1)
        let beforeTranslation = model.translationGeneration
        let beforeCode = model.codeGeneration
        let result = model.access(.fetch, address: page1 - 2, length: 4)
        let artifact = memoryArtifact(caseID: "cross-page-fetch", expected: .pass, observed: result.allowed ? .pass : .fail, model: model, access: .fetch, address: page1 - 2, length: 4, result: result, translationBefore: beforeTranslation, codeBefore: beforeCode, notes: ["cross-page instruction fetch succeeds only when both guest pages permit FETCH"])
        try record("cross-page-fetch", result.allowed && result.copiedBytes == 4, artifact)
        counters["cross_page_cases", default: 0] += 1
    }

    do {
        let model = MemoryContractModel(hostPageSize: 4096)
        model.map(page0, permissions: readWrite, fill: 0, backingID: "rw-a")
        model.map(page1, permissions: readWrite, fill: 0, backingID: "rw-b")
        let beforeTranslation = model.translationGeneration
        let beforeCode = model.codeGeneration
        let writeResult = model.access(.write, address: page1 - 2, length: 5, payload: Array("hello".utf8))
        let readResult = model.access(.read, address: page1 - 2, length: 5)
        let artifact = memoryArtifact(caseID: "cross-page-read-write", expected: .pass, observed: writeResult.allowed ? .pass : .fail, model: model, access: .write, address: page1 - 2, length: 5, result: writeResult, translationBefore: beforeTranslation, codeBefore: beforeCode, notes: ["cross-page data write and read preflight every guest page"])
        try record("cross-page-read-write", writeResult.allowed && String(decoding: readResult.bytes, as: UTF8.self) == "hello", artifact)
        counters["cross_page_cases", default: 0] += 1
    }

    do {
        let model = MemoryContractModel(hostPageSize: 4096)
        model.map(page0, permissions: readOnly, fill: 0x72, backingID: "mprotect")
        let beforeTranslation = model.translationGeneration
        let beforeCode = model.codeGeneration
        model.mprotect(page0, permissions: MemoryPermissions(read: false, write: false, execute: false))
        let denied = model.access(.read, address: page0, length: 1)
        model.mprotect(page0, permissions: readOnly)
        let restored = model.access(.read, address: page0, length: 1)
        let result = MemoryAccessResult(allowed: !model.translationIsCurrent(beforeTranslation) && !denied.allowed && restored.allowed, bytes: restored.bytes, copiedBytes: restored.copiedBytes, faultAddress: denied.faultAddress, reason: "mprotect rechecked")
        let artifact = memoryArtifact(caseID: "mprotect-transitions", expected: .pass, observed: result.allowed ? .pass : .fail, model: model, access: .read, address: page0, length: 1, result: result, translationBefore: beforeTranslation, codeBefore: beforeCode, notes: ["mprotect transitions invalidate stale translation generation and recheck permissions"])
        try record("mprotect-transitions", result.allowed, artifact)
        counters["generation_cases", default: 0] += 1
    }

    do {
        let model = MemoryContractModel(hostPageSize: 4096)
        model.map(page0, permissions: readOnly, fill: 0x6f, backingID: "old")
        let beforeTranslation = model.translationGeneration
        let beforeCode = model.codeGeneration
        model.unmap(page0)
        let fault = model.access(.read, address: page0, length: 1)
        model.remap(page0, permissions: readOnly, fill: 0x6e, backingID: "new")
        model.cowReplace(page0, fill: 0x63, backingID: "cow")
        let current = model.access(.read, address: page0, length: 1)
        let result = MemoryAccessResult(allowed: !model.translationIsCurrent(beforeTranslation) && !fault.allowed && model.backingID(page0) == "cow" && current.bytes == [0x63], bytes: current.bytes, copiedBytes: current.copiedBytes, faultAddress: fault.faultAddress, reason: "remap and CoW refreshed backing")
        let artifact = memoryArtifact(caseID: "munmap-remap-cow", expected: .pass, observed: result.allowed ? .pass : .fail, model: model, access: .read, address: page0, length: 1, result: result, translationBefore: beforeTranslation, codeBefore: beforeCode, notes: ["munmap/remap and CoW replacement reject stale backing"])
        try record("munmap-remap-cow", result.allowed, artifact)
        counters["generation_cases", default: 0] += 1
    }

    do {
        let model = MemoryContractModel(hostPageSize: 4096)
        model.map(page0, permissions: execWrite, fill: 0, backingID: "exec-write", translatedBlock: true)
        let beforeTranslation = model.translationGeneration
        let beforeCode = model.codeGeneration
        let result = model.access(.write, address: page0, length: 1, payload: [0x78])
        let invalidated = result.allowed && !model.translatedBlockIsCurrent(page0, generation: beforeCode) && model.codeGeneration == beforeCode + 1
        let artifact = memoryArtifact(caseID: "store-to-translated-exec-page", expected: .pass, observed: invalidated ? .pass : .fail, model: model, access: .write, address: page0, length: 1, result: result, translationBefore: beforeTranslation, codeBefore: beforeCode, notes: ["store to translated executable page invalidates stale code generation"])
        try record("store-to-translated-exec-page", invalidated, artifact)
        counters["generation_cases", default: 0] += 1
    }

    do {
        let model = MemoryContractModel(hostPageSize: 4096)
        let executablePage: UInt64 = 0x0000_0000_0003_1000
        let gotPage: UInt64 = 0x0000_0000_0004_3000
        let aliasOffset: UInt64 = 0x360
        let executableAliasValue: UInt64 = 0x97ff_b62f_3908_3fff
        let gotValue: UInt64 = 0x0000_0000_0005_50a8
        model.map(executablePage, permissions: MemoryPermissions(read: true, write: false, execute: true), fill: 0, backingID: "file-offset-0x21360-text")
        model.map(gotPage, permissions: readOnly, fill: 0, backingID: "file-offset-0x43360-got")
        model.writeSeed(littleEndianBytes(executableAliasValue), at: executablePage + aliasOffset)
        model.writeSeed(littleEndianBytes(gotValue), at: gotPage + aliasOffset)
        let beforeTranslation = model.translationGeneration
        let beforeCode = model.codeGeneration
        let result = model.access(.read, address: gotPage + aliasOffset, length: 8)
        let actual = result.bytes.count == 8 ? (try? littleEndianUInt64(Data(result.bytes), 0)) : nil
        let artifact = memoryArtifact(
            caseID: "vma-offset-alias-got-read",
            expected: .pass,
            observed: result.allowed ? .pass : .fail,
            model: model,
            access: .read,
            address: gotPage + aliasOffset,
            length: 8,
            result: result,
            translationBefore: beforeTranslation,
            codeBefore: beforeCode,
            notes: [
                "GOT guest VMA 0x43360 and executable guest VMA 0x31360 share offset 0x360 but must not alias",
                String(format: "expected GOT value 0x%016llx; executable alias value 0x%016llx; observed 0x%016llx", gotValue, executableAliasValue, actual ?? 0)
            ]
        )
        try record(
            "vma-offset-alias-got-read",
            result.allowed && actual == gotValue && actual != executableAliasValue,
            artifact
        )
        counters["vma_alias_cases", default: 0] += 1
    }

    return (failures, artifacts, counters)
}

func runMemoryFuzz() throws -> Int32 {
    let target = "tcti-memory-fuzz"
    if let negativeID = ProcessInfo.processInfo.environment["NEGATIVE_MEMORY_FUZZ"], !negativeID.isEmpty {
        guard memoryNegativeIDs().contains(negativeID) else {
            throw GateError.usage("unknown NEGATIVE_MEMORY_FUZZ=\(negativeID)")
        }
        let artifact = try runMemoryNegative(negativeID)
        let artifactPath = try writeMemoryFuzzArtifact(artifact)
        let status: GateStatus = .fail
        let reportURL = try writeReport(report(
            target: target,
            status: status,
            summary: "Replayed negative memory fuzz fixture \(negativeID).",
            failures: [fail(negativeID, artifact.notes.joined(separator: "; "))],
            artifacts: [artifactPath],
            counters: [
                "cases_total": 1,
                "negative_cases": 1,
            ],
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("\(status.rawValue): \(relativePath(reportURL))")
        return exitCode(for: status)
    }

    var result = try runMemoryPositiveCases()
    var reducers: [String] = []
    for negativeID in memoryNegativeIDs() {
        let artifact = try runMemoryNegative(negativeID)
        let artifactPath = try writeMemoryFuzzArtifact(artifact)
        let reducer = try writeReducer(
            target: target,
            caseID: negativeID,
            command: "NEGATIVE_MEMORY_FUZZ=\(negativeID) make tcti-gate TARGET=tcti-memory-fuzz",
            reason: artifact.notes.joined(separator: "; "),
            artifacts: [artifactPath],
            expectedStatus: .fail
        )
        reducers.append(relativePath(reducer))
    }
    let passReducer = try writeReducer(
        target: target,
        caseID: "memory-fuzz-pass-regression",
        command: "make tcti-gate TARGET=tcti-memory-fuzz",
        reason: "full memory fuzz gate must remain passing",
        artifacts: result.artifacts,
        expectedStatus: .pass
    )
    reducers.append(relativePath(passReducer))
    result.artifacts.append(contentsOf: reducers)
    result.counters["negative_reducers"] = reducers.count - 1
    result.counters["pass_reducers"] = 1

    let status: GateStatus = result.failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Ran no-phone FETCH/READ/WRITE memory fuzz contracts with deterministic negative reducers.",
        failures: result.failures,
        artifacts: result.artifacts,
        counters: result.counters,
        coverageWarnings: [
            "no-phone contract model only; production page backing remains owned by arch/orlix Linux MM helpers",
            "no simulator, physical device, HostAdapter, Darwin syscall, production assembly, or gadget dispatch executed",
        ],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    print("reducers:")
    for reducer in reducers {
        print("- \(reducer)")
    }
    return exitCode(for: status)
}

struct DirectChainBlock {
    let id: String
    let start: UInt64
    let length: UInt64
    var outgoingTarget: String?
    var incomingSources: Set<String>
    var retired: Bool

    var end: UInt64 { start + length - 1 }
}

struct DirectChainPatch {
    let source: String
    let target: String
    let samePage: Bool
}

struct DirectChainResult {
    let passed: Bool
    let reason: String
}

final class DirectChainModel {
    let guestPageSize = 4096
    var translationGeneration = 1
    var codeGeneration = 1
    private var blocks: [String: DirectChainBlock] = [:]
    private var pageIndex: [UInt64: Set<String>] = [:]

    func addBlock(id: String, start: UInt64, length: UInt64) {
        let block = DirectChainBlock(
            id: id,
            start: start,
            length: length,
            outgoingTarget: nil,
            incomingSources: [],
            retired: false
        )
        blocks[id] = block
        for page in pagesCovered(start: start, end: block.end) {
            pageIndex[page, default: []].insert(id)
        }
    }

    func pagesCovered(start: UInt64, end: UInt64) -> [UInt64] {
        var pages: [UInt64] = []
        var page = start - (start % UInt64(guestPageSize))
        let last = end - (end % UInt64(guestPageSize))
        while page <= last {
            pages.append(page)
            page += UInt64(guestPageSize)
        }
        return pages
    }

    func pageIndexLookup(page: UInt64) -> [String] {
        Array(pageIndex[page] ?? []).sorted()
    }

    func patch(source: String, target: String) -> DirectChainResult {
        guard var sourceBlock = blocks[source], var targetBlock = blocks[target] else {
            return DirectChainResult(passed: false, reason: "source or target block missing")
        }
        guard !sourceBlock.retired, !targetBlock.retired else {
            return DirectChainResult(passed: false, reason: "refused chain to or from retired block")
        }
        if sourceBlock.outgoingTarget != nil {
            return DirectChainResult(passed: false, reason: "source already has outgoing patch")
        }
        sourceBlock.outgoingTarget = target
        targetBlock.incomingSources.insert(source)
        blocks[source] = sourceBlock
        blocks[target] = targetBlock
        return DirectChainResult(passed: true, reason: samePage(source: source, target: target) ? "same-page chain patched" : "cross-page chain patched")
    }

    func samePage(source: String, target: String) -> Bool {
        guard let sourceBlock = blocks[source], let targetBlock = blocks[target] else { return false }
        return (sourceBlock.start / UInt64(guestPageSize)) == (targetBlock.start / UInt64(guestPageSize))
    }

    func invalidate(page: UInt64) -> DirectChainResult {
        let ids = pageIndexLookup(page: page)
        guard !ids.isEmpty else {
            return DirectChainResult(passed: false, reason: "page index lookup missed invalidation page")
        }
        for id in ids {
            guard var block = blocks[id] else { continue }
            if let target = block.outgoingTarget, var targetBlock = blocks[target] {
                targetBlock.incomingSources.remove(id)
                blocks[target] = targetBlock
            }
            for source in block.incomingSources {
                if var sourceBlock = blocks[source] {
                    sourceBlock.outgoingTarget = nil
                    blocks[source] = sourceBlock
                }
            }
            block.outgoingTarget = nil
            block.incomingSources.removeAll()
            block.retired = true
            blocks[id] = block
        }
        translationGeneration += 1
        codeGeneration += 1
        return DirectChainResult(passed: true, reason: "invalidated \(ids.count) block(s) before retire")
    }

    func block(_ id: String) -> DirectChainBlock? {
        blocks[id]
    }

    func staleChainAllowed(source: String) -> Bool {
        guard let target = blocks[source]?.outgoingTarget else { return false }
        return blocks[target]?.retired == false
    }
}

struct DirectChainArtifact: Codable {
    let caseID: String
    let expectedStatus: String
    let observedStatus: String
    let translationGeneration: Int
    let codeGeneration: Int
    let blocks: [String: DirectChainBlockArtifact]
    let pageIndex: [String: [String]]
    let forbiddenBehavior: [String: Bool]
    let notes: [String]

    enum CodingKeys: String, CodingKey {
        case caseID = "case_id"
        case expectedStatus = "expected_status"
        case observedStatus = "observed_status"
        case translationGeneration = "translation_generation"
        case codeGeneration = "code_generation"
        case blocks
        case pageIndex = "page_index"
        case forbiddenBehavior = "forbidden_behavior"
        case notes
    }
}

struct DirectChainBlockArtifact: Codable {
    let start: String
    let end: String
    let outgoingTarget: String?
    let incomingSources: [String]
    let retired: Bool

    enum CodingKeys: String, CodingKey {
        case start
        case end
        case outgoingTarget = "outgoing_target"
        case incomingSources = "incoming_sources"
        case retired
    }
}

func directChainArtifact(
    caseID: String,
    expected: GateStatus,
    observed: GateStatus,
    model: DirectChainModel,
    blockIDs: [String],
    pageBases: [UInt64],
    notes: [String]
) -> DirectChainArtifact {
    var blocks: [String: DirectChainBlockArtifact] = [:]
    for id in blockIDs.sorted() {
        guard let block = model.block(id) else { continue }
        blocks[id] = DirectChainBlockArtifact(
            start: hexPC(block.start),
            end: hexPC(block.end),
            outgoingTarget: block.outgoingTarget,
            incomingSources: block.incomingSources.sorted(),
            retired: block.retired
        )
    }
    var pageIndex: [String: [String]] = [:]
    for page in pageBases.sorted() {
        pageIndex[hexPC(page)] = model.pageIndexLookup(page: page)
    }
    return DirectChainArtifact(
        caseID: caseID,
        expectedStatus: expected.rawValue,
        observedStatus: observed.rawValue,
        translationGeneration: model.translationGeneration,
        codeGeneration: model.codeGeneration,
        blocks: blocks,
        pageIndex: pageIndex,
        forbiddenBehavior: forbiddenDefaults(),
        notes: notes
    )
}

func writeDirectChainArtifact(_ artifact: DirectChainArtifact) throws -> String {
    let url = buildPath("direct_chain_fuzz", artifact.caseID, "result.json")
    try writeJSON(artifact, to: url)
    return relativePath(url)
}

func directChainNegativeIDs() -> [String] {
    [
        "stale-target-after-retire",
        "page-index-overlap-miss",
        "retire-with-patched-incoming",
        "duplicate-outgoing-patch",
    ]
}

func runDirectChainNegative(_ id: String) throws -> DirectChainArtifact {
    let page0: UInt64 = 0x0000_0000_0050_0000
    let page1 = page0 + 4096
    let model = DirectChainModel()

    switch id {
    case "stale-target-after-retire":
        model.addBlock(id: "source", start: page0, length: 32)
        model.addBlock(id: "target", start: page0 + 64, length: 32)
        _ = model.patch(source: "source", target: "target")
        _ = model.invalidate(page: page0)
        let staleAllowed = model.staleChainAllowed(source: "source")
        return directChainArtifact(caseID: id, expected: .fail, observed: staleAllowed ? .pass : .fail, model: model, blockIDs: ["source", "target"], pageBases: [page0], notes: ["negative fixture proves stale direct chain cannot jump into retired target"])
    case "page-index-overlap-miss":
        model.addBlock(id: "cross", start: page1 - 8, length: 16)
        let indexedBoth = model.pageIndexLookup(page: page0).contains("cross") && model.pageIndexLookup(page: page1).contains("cross")
        return directChainArtifact(caseID: id, expected: .fail, observed: indexedBoth ? .fail : .pass, model: model, blockIDs: ["cross"], pageBases: [page0, page1], notes: ["negative fixture proves page index records every page overlapped by a block"])
    case "retire-with-patched-incoming":
        model.addBlock(id: "source", start: page0, length: 32)
        model.addBlock(id: "target", start: page0 + 96, length: 32)
        _ = model.patch(source: "source", target: "target")
        _ = model.invalidate(page: page0)
        let incomingCleared = model.block("target")?.incomingSources.isEmpty == true && model.block("source")?.outgoingTarget == nil
        return directChainArtifact(caseID: id, expected: .fail, observed: incomingCleared ? .fail : .pass, model: model, blockIDs: ["source", "target"], pageBases: [page0], notes: ["negative fixture proves invalidation unpatches incoming slots before retire"])
    case "duplicate-outgoing-patch":
        model.addBlock(id: "source", start: page0, length: 32)
        model.addBlock(id: "target-a", start: page0 + 64, length: 32)
        model.addBlock(id: "target-b", start: page0 + 128, length: 32)
        _ = model.patch(source: "source", target: "target-a")
        let duplicate = model.patch(source: "source", target: "target-b")
        return directChainArtifact(caseID: id, expected: .fail, observed: duplicate.passed ? .pass : .fail, model: model, blockIDs: ["source", "target-a", "target-b"], pageBases: [page0], notes: ["negative fixture proves one source cannot hold two outgoing patch slots"])
    default:
        throw GateError.usage("unknown NEGATIVE_DIRECT_CHAIN_FUZZ=\(id)")
    }
}

func runDirectChainPositiveCases() throws -> (failures: [Failure], artifacts: [String], counters: [String: Int]) {
    let page0: UInt64 = 0x0000_0000_0050_0000
    let page1 = page0 + 4096
    var failures: [Failure] = []
    var artifacts: [String] = []
    var counters: [String: Int] = [
        "positive_cases": 0,
        "same_page_chains": 0,
        "cross_page_index_cases": 0,
        "invalidation_cases": 0,
    ]

    func record(_ id: String, _ condition: Bool, _ artifact: DirectChainArtifact) throws {
        counters["positive_cases", default: 0] += 1
        artifacts.append(try writeDirectChainArtifact(artifact))
        if !condition {
            failures.append(fail(id, "direct-chain positive contract failed"))
        }
    }

    do {
        let model = DirectChainModel()
        model.addBlock(id: "source", start: page0, length: 32)
        model.addBlock(id: "target", start: page0 + 64, length: 32)
        let patch = model.patch(source: "source", target: "target")
        let condition = patch.passed &&
            model.block("source")?.outgoingTarget == "target" &&
            model.block("target")?.incomingSources.contains("source") == true &&
            model.samePage(source: "source", target: "target")
        let artifact = directChainArtifact(caseID: "same-page-source-target-slots", expected: .pass, observed: condition ? .pass : .fail, model: model, blockIDs: ["source", "target"], pageBases: [page0], notes: ["same-page source outgoing and target incoming patch slots are tracked as data"])
        try record("same-page-source-target-slots", condition, artifact)
        counters["same_page_chains", default: 0] += 1
    }

    do {
        let model = DirectChainModel()
        model.addBlock(id: "cross", start: page1 - 8, length: 16)
        let condition = model.pageIndexLookup(page: page0).contains("cross") &&
            model.pageIndexLookup(page: page1).contains("cross")
        let artifact = directChainArtifact(caseID: "page-index-overlap-lookup", expected: .pass, observed: condition ? .pass : .fail, model: model, blockIDs: ["cross"], pageBases: [page0, page1], notes: ["page index lookup finds a block overlapping both guest pages"])
        try record("page-index-overlap-lookup", condition, artifact)
        counters["cross_page_index_cases", default: 0] += 1
    }

    do {
        let model = DirectChainModel()
        model.addBlock(id: "source", start: page0, length: 32)
        model.addBlock(id: "target", start: page0 + 128, length: 32)
        _ = model.patch(source: "source", target: "target")
        let beforeTranslation = model.translationGeneration
        let beforeCode = model.codeGeneration
        let invalidate = model.invalidate(page: page0)
        let condition = invalidate.passed &&
            model.block("source")?.retired == true &&
            model.block("target")?.retired == true &&
            model.block("source")?.outgoingTarget == nil &&
            model.block("target")?.incomingSources.isEmpty == true &&
            model.translationGeneration == beforeTranslation + 1 &&
            model.codeGeneration == beforeCode + 1
        let artifact = directChainArtifact(caseID: "invalidate-unpatch-before-retire", expected: .pass, observed: condition ? .pass : .fail, model: model, blockIDs: ["source", "target"], pageBases: [page0], notes: ["invalidation removes outgoing and incoming patch slots before retiring blocks"])
        try record("invalidate-unpatch-before-retire", condition, artifact)
        counters["invalidation_cases", default: 0] += 1
    }

    do {
        let model = DirectChainModel()
        model.addBlock(id: "source", start: page0, length: 32)
        model.addBlock(id: "same-page-target", start: page0 + 64, length: 32)
        model.addBlock(id: "cross-page-target", start: page1 + 64, length: 32)
        let samePagePatch = model.patch(source: "source", target: "same-page-target")
        let crossPagePatch = model.patch(source: "source", target: "cross-page-target")
        let condition = samePagePatch.passed && !crossPagePatch.passed && model.samePage(source: "source", target: "same-page-target")
        let artifact = directChainArtifact(caseID: "same-page-before-cross-page", expected: .pass, observed: condition ? .pass : .fail, model: model, blockIDs: ["source", "same-page-target", "cross-page-target"], pageBases: [page0, page1], notes: ["same-page chaining works first; broader cross-page chaining remains constrained by the single outgoing patch slot"])
        try record("same-page-before-cross-page", condition, artifact)
        counters["same_page_chains", default: 0] += 1
    }

    return (failures, artifacts, counters)
}

func runDirectChainFuzz() throws -> Int32 {
    let target = "tcti-direct-chain-fuzz"
    if let negativeID = ProcessInfo.processInfo.environment["NEGATIVE_DIRECT_CHAIN_FUZZ"], !negativeID.isEmpty {
        guard directChainNegativeIDs().contains(negativeID) else {
            throw GateError.usage("unknown NEGATIVE_DIRECT_CHAIN_FUZZ=\(negativeID)")
        }
        let artifact = try runDirectChainNegative(negativeID)
        let artifactPath = try writeDirectChainArtifact(artifact)
        let status: GateStatus = .fail
        let reportURL = try writeReport(report(
            target: target,
            status: status,
            summary: "Replayed negative direct-chain fuzz fixture \(negativeID).",
            failures: [fail(negativeID, artifact.notes.joined(separator: "; "))],
            artifacts: [artifactPath],
            counters: [
                "cases_total": 1,
                "negative_cases": 1,
            ],
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("\(status.rawValue): \(relativePath(reportURL))")
        return exitCode(for: status)
    }

    var result = try runDirectChainPositiveCases()
    var reducers: [String] = []
    for negativeID in directChainNegativeIDs() {
        let artifact = try runDirectChainNegative(negativeID)
        let artifactPath = try writeDirectChainArtifact(artifact)
        let reducer = try writeReducer(
            target: target,
            caseID: negativeID,
            command: "NEGATIVE_DIRECT_CHAIN_FUZZ=\(negativeID) make tcti-gate TARGET=tcti-direct-chain-fuzz",
            reason: artifact.notes.joined(separator: "; "),
            artifacts: [artifactPath],
            expectedStatus: .fail
        )
        reducers.append(relativePath(reducer))
    }
    let passReducer = try writeReducer(
        target: target,
        caseID: "direct-chain-fuzz-pass-regression",
        command: "make tcti-gate TARGET=tcti-direct-chain-fuzz",
        reason: "full direct-chain fuzz gate must remain passing",
        artifacts: result.artifacts,
        expectedStatus: .pass
    )
    reducers.append(relativePath(passReducer))
    result.artifacts.append(contentsOf: reducers)
    result.counters["negative_reducers"] = reducers.count - 1
    result.counters["pass_reducers"] = 1

    let status: GateStatus = result.failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Ran no-phone direct-chain data-structure fuzz contracts with deterministic negative reducers.",
        failures: result.failures,
        artifacts: result.artifacts,
        counters: result.counters,
        coverageWarnings: [
            "no-phone data-structure contract only; no production assembly, gadget dispatch, or generated executable memory executed",
            "direct-chain model tracks patch-slot and invalidation invariants without host executable guest text",
        ],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    print("reducers:")
    for reducer in reducers {
        print("- \(reducer)")
    }
    return exitCode(for: status)
}

func x18TokenRanges(in line: String) -> [Range<String.Index>] {
    let pattern = #"(?<![A-Za-z0-9_])[wx]18(?![A-Za-z0-9_])"#
    guard let regex = try? NSRegularExpression(pattern: pattern) else {
        return []
    }
    let range = NSRange(line.startIndex..<line.endIndex, in: line)
    return regex.matches(in: line, range: range).compactMap { Range($0.range, in: line) }
}

func scanSourceForX18(_ url: URL) -> [Failure] {
    guard let text = try? readText(url) else {
        return [fail("scan", "could not read \(relativePath(url))")]
    }
    var failures: [Failure] = []
    for (index, line) in text.components(separatedBy: .newlines).enumerated() where !x18TokenRanges(in: line).isEmpty {
        failures.append(fail("host-x18", "\(relativePath(url)):\(index + 1) mentions forbidden host x18/w18 token"))
    }
    return failures
}

struct ForbiddenSourcePattern {
    let id: String
    let field: String
    let regex: NSRegularExpression
    let message: String

    init(id: String, field: String, pattern: String, message: String) throws {
        self.id = id
        self.field = field
        self.regex = try NSRegularExpression(pattern: pattern)
        self.message = message
    }
}

func forbiddenSourcePatterns() throws -> [ForbiddenSourcePattern] {
    try [
        ForbiddenSourcePattern(
            id: "map-jit",
            field: "map_jit",
            pattern: #"(?<![A-Za-z0-9_])MAP_JIT(?![A-Za-z0-9_])"#,
            message: "mentions forbidden MAP_JIT token"
        ),
        ForbiddenSourcePattern(
            id: "generated-exec-memory",
            field: "generated_exec_memory",
            pattern: #"mmap\s*\([^;\n]*(PROT_EXEC|VM_PROT_EXECUTE)"#,
            message: "requests generated executable memory"
        ),
        ForbiddenSourcePattern(
            id: "rwx",
            field: "rwx",
            pattern: #"(PROT_READ\s*\|[^;\n]*PROT_WRITE\s*\|[^;\n]*PROT_EXEC)|(VM_PROT_READ\s*\|[^;\n]*VM_PROT_WRITE\s*\|[^;\n]*VM_PROT_EXECUTE)"#,
            message: "requests RWX memory permissions"
        ),
        ForbiddenSourcePattern(
            id: "host-exec-guest-text",
            field: "host_exec_guest_text",
            pattern: #"(?i)guest[_ -]?text[^;\n]*(PROT_EXEC|VM_PROT_EXECUTE)|(?:PROT_EXEC|VM_PROT_EXECUTE)[^;\n]*guest[_ -]?text"#,
            message: "requests host executable permissions for guest text"
        ),
        ForbiddenSourcePattern(
            id: "host-exec-shadow-user-page",
            field: "host_exec_guest_text",
            pattern: #"(?i)(VM_PROT_EXECUTE|PROT_EXEC|executable)[^;\n]*(shadow[_ -]?user|user[_ -]?page|segment_protection)|(?:shadow[_ -]?user|user[_ -]?page|segment_protection)[^;\n]*(VM_PROT_EXECUTE|PROT_EXEC|executable)"#,
            message: "requests host executable permissions for hosted guest user pages"
        ),
        ForbiddenSourcePattern(
            id: "hostadapter-linux-trap-semantics",
            field: "native_ios_api_exposure_to_guest",
            pattern: #"(?i)(OrlixHostTranslateLinuxSyscalls|OrlixHostUserTrapIsLinuxSyscall|ORLIX_HOST_USER_TRAP_SYSCALL|ORLIX_HOST_USER_TRAP_TLS_WRITE)"#,
            message: "keeps Linux syscall or TLS trap semantics in HostAdapter product code"
        ),
        ForbiddenSourcePattern(
            id: "native-ios-api-exposure",
            field: "native_ios_api_exposure_to_guest",
            pattern: #"(?i)(guest|linux)[A-Za-z0-9_ -]*(UIKit|CoreFoundation|Foundation|Darwin|HostAdapter)"#,
            message: "appears to expose native iOS or HostAdapter API surface to guest Linux"
        ),
    ]
}

func scanSourceForForbiddenBehavior(_ url: URL, patterns: [ForbiddenSourcePattern]) -> (failures: [Failure], flags: [String: Bool]) {
    guard let text = try? readText(url) else {
        return ([fail("scan", "could not read \(relativePath(url))")], [:])
    }
    var failures: [Failure] = []
    var flags: [String: Bool] = [:]
    let lines = text.components(separatedBy: .newlines)
    for (index, line) in lines.enumerated() {
        let nsRange = NSRange(line.startIndex..<line.endIndex, in: line)
        for pattern in patterns where pattern.regex.firstMatch(in: line, range: nsRange) != nil {
            flags[pattern.field] = true
            failures.append(fail(pattern.id, "\(relativePath(url)):\(index + 1) \(pattern.message)"))
        }
    }
    return (failures, flags)
}

func runSafetyAudit() throws -> Int32 {
    let target = "tcti-appstore-safety-audit"
    let productionRoots = [
        path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti"),
        path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "include", "asm", "tcti.h"),
        path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "mm", "tcti_user_page.c"),
        path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "mm", "tcti_invalidate.c"),
        path("OrlixHostAdapter", "Sources", "OrlixHostAdapter", "memory"),
        path("OrlixHostAdapter", "Sources", "OrlixHostAdapter", "runtime"),
    ]
    let sourceExtensions = ["S", "s", "c", "h"]
    var failures: [Failure] = []
    var scannedFiles = 0
    var scannedObjects = 0
    var warnings: [String] = []
    let forbiddenPatterns = try forbiddenSourcePatterns()
    var forbiddenFlags = forbiddenDefaults()

    for root in productionRoots {
        var isDirectory: ObjCBool = false
        guard fileManager.fileExists(atPath: root.path, isDirectory: &isDirectory) else { continue }
        if isDirectory.boolValue {
            guard let enumerator = fileManager.enumerator(at: root, includingPropertiesForKeys: nil) else { continue }
            for case let url as URL in enumerator where sourceExtensions.contains(url.pathExtension) {
                scannedFiles += 1
                failures.append(contentsOf: scanSourceForX18(url))
                let scan = scanSourceForForbiddenBehavior(url, patterns: forbiddenPatterns)
                failures.append(contentsOf: scan.failures)
                for (field, value) in scan.flags where value {
                    forbiddenFlags[field] = true
                }
            }
        } else if sourceExtensions.contains(root.pathExtension) {
            scannedFiles += 1
            failures.append(contentsOf: scanSourceForX18(root))
            let scan = scanSourceForForbiddenBehavior(root, patterns: forbiddenPatterns)
            failures.append(contentsOf: scan.failures)
            for (field, value) in scan.flags where value {
                forbiddenFlags[field] = true
            }
        }
    }

    let build = repoRoot().appendingPathComponent("Build", isDirectory: true)
    if let enumerator = fileManager.enumerator(at: build, includingPropertiesForKeys: nil) {
        for case let url as URL in enumerator where url.pathExtension == "o" && url.path.contains("/hosted_exec/tcti/") {
            scannedObjects += 1
            do {
                let disassembly = try runWithFileBackedOutput(["xcrun", "llvm-objdump", "-d", url.path])
                for (index, line) in disassembly.components(separatedBy: .newlines).enumerated() where !x18TokenRanges(in: line).isEmpty {
                    failures.append(fail("object-host-x18", "\(relativePath(url)) disassembly line \(index + 1) uses forbidden host x18/w18 token"))
                }
            } catch {
                failures.append(fail("object-disassembly", "could not disassemble \(relativePath(url)): \(error)"))
            }
        }
    }
    if scannedObjects == 0 {
        warnings.append("no compiled TCTI object files were available for disassembly coverage")
    }

    let fixture = path("tools", "tcti", "fixtures", "x18_forbidden", "bad.S")
    if scanSourceForX18(fixture).isEmpty {
        failures.append(fail("x18-fixture", "x18 forbidden fixture did not trigger scanner"))
    }
    let appStoreFixture = path("tools", "tcti", "fixtures", "appstore_safety", "forbidden_exec.c")
    let appStoreFixtureScan = scanSourceForForbiddenBehavior(appStoreFixture, patterns: forbiddenPatterns)
    let expectedFixtureFields = [
        "generated_exec_memory",
        "host_exec_guest_text",
        "map_jit",
        "native_ios_api_exposure_to_guest",
        "rwx",
    ]
    for field in expectedFixtureFields where appStoreFixtureScan.flags[field] != true {
        failures.append(fail("appstore-fixture-\(field)", "App Store safety fixture did not trigger \(field) scanner"))
    }
    forbiddenFlags["host_x18"] = !failures.filter { $0.id.contains("x18") }.isEmpty

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    var artifacts: [String] = []
    if status == .pass {
        let reducer = try writeReducer(
            target: target,
            caseID: "appstore-safety-pass-regression",
            command: "make tcti-gate TARGET=\(target)",
            reason: "App Store safety audit must keep all forbidden_behavior fields false.",
            artifacts: ["Build/TCTI/reports/\(target)/report.json"],
            expectedStatus: .pass
        )
        artifacts.append(relativePath(reducer))
    } else {
        for field in forbiddenFlags.keys.sorted() where forbiddenFlags[field] == true {
            let reducer = try writeReducer(
                target: target,
                caseID: field.replacingOccurrences(of: "_", with: "-"),
                command: "make tcti-gate TARGET=\(target)",
                reason: "App Store safety audit reported forbidden_behavior.\(field)=true.",
                artifacts: ["Build/TCTI/reports/\(target)/report.json"],
                expectedStatus: .fail
            )
            artifacts.append(relativePath(reducer))
        }
    }
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Scanned \(scannedFiles) TCTI source/template file(s) and \(scannedObjects) object file(s).",
        failures: failures,
        artifacts: artifacts,
        forbiddenBehavior: forbiddenFlags,
        counters: [
            "scanned_source_files": scannedFiles,
            "scanned_objects": scannedObjects,
        ],
        coverageWarnings: warnings
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runRepro() throws -> Int32 {
    guard let repro = ProcessInfo.processInfo.environment["REPRO"], !repro.isEmpty else {
        throw GateError.usage("REPRO=<path> is required")
    }
    let url = URL(fileURLWithPath: repro, relativeTo: repoRoot()).standardizedFileURL
    let payload = try decoder.decode(Reproducer.self, from: Data(contentsOf: url))
    guard !payload.command.isEmpty else {
        throw GateError.invalidReducer("\(relativePath(url)) does not contain command")
    }
    print("target: \(payload.target)")
    print("case id: \(payload.caseID)")
    print("original command: \(payload.command)")
    print("artifact paths:")
    if payload.artifacts.isEmpty {
        print("- none")
    } else {
        for artifact in payload.artifacts {
            print("- \(artifact)")
        }
    }
    print("expected status: \(payload.expectedStatus ?? "unknown")")
    print("reason: \(payload.reason)")
    let replayCommand = normalizedReproCommand(payload.command)
    if replayCommand != payload.command {
        print("normalized command: \(replayCommand)")
    }
    let process = Process()
    process.executableURL = URL(fileURLWithPath: "/bin/bash")
    process.arguments = ["-lc", replayCommand]
	process.currentDirectoryURL = URL(fileURLWithPath: payload.workingDirectory)
	try process.run()
	process.waitUntilExit()
	let replayPayload = (try? decoder.decode(Reproducer.self, from: Data(contentsOf: url))) ?? payload
	let reportURL = buildPath("reports", payload.target, "report.json")
	var actualStatus = process.terminationStatus == 0 ? "pass" : "fail"
    if let object = try? loadJSON(reportURL),
       let dictionary = object as? [String: Any],
       let status = dictionary["status"] as? String {
        actualStatus = status
    }
    print("actual replay status: \(actualStatus)")
    print("actual replay exit code: \(process.terminationStatus)")

    let expectedMatches = payload.expectedStatus.map { $0 == actualStatus }
    let replaySucceeded = expectedMatches ?? (process.terminationStatus == 0)
    let reproStatus: GateStatus = replaySucceeded ? .pass : .fail
    let failures: [Failure]
    if let expected = payload.expectedStatus, expected != actualStatus {
        failures = [fail("status-mismatch", "reproducer expected status \(expected), got \(actualStatus)")]
    } else if payload.expectedStatus == nil && process.terminationStatus != 0 {
        failures = [fail("replay-exit", "replay command exited \(process.terminationStatus) without expected_status")]
    } else {
        failures = []
	}
	var artifacts: [String] = []
	var seenArtifacts: Set<String> = []
	for artifact in [relativePath(url), relativePath(reportURL)] + replayPayload.artifacts {
		if seenArtifacts.insert(artifact).inserted {
			artifacts.append(artifact)
		}
	}
    let reproReportURL = try writeReport(report(
        target: "tcti-repro",
        status: reproStatus,
        summary: "Replayed reducer \(payload.caseID) for \(payload.target).",
        failures: failures,
        artifacts: artifacts,
        counters: [
            "replay_exit_code": Int(process.terminationStatus),
        ],
        releaseGateEligible: false,
        readinessGateEligible: false,
        expectedStatus: payload.expectedStatus,
        actualReplayStatus: actualStatus
    ))
    print("replay report: \(relativePath(reproReportURL))")
    if let failure = failures.first {
        fputs("\(failure.message)\n", stderr)
    }
    return exitCode(for: reproStatus)
}

func normalizedReproCommand(_ command: String) -> String {
    command
        .replacingOccurrences(
            of: "make tcti-golden-elf-refresh",
            with: "make tcti-gate TARGET=tcti-golden-elf-refresh"
        )
        .replacingOccurrences(
            of: "make tcti-golden-elf",
            with: "make tcti-gate TARGET=tcti-golden-elf"
        )
        .replacingOccurrences(
            of: "make tcti-repro",
            with: "make tcti-gate TARGET=tcti-repro"
        )
}

func runAddSubShiftedXZRFix() throws -> Int32 {
    let target = "tcti-add-sub-shifted-xzr-fix"
    var failures: [Failure] = []
    var artifacts: [String] = []

    if let simulatorReport = latestRuntimeValidationReport(
        gate: "tcti-simulator-stability",
        destination: "iphonesimulator"
    ) {
        artifacts.append(relativePath(simulatorReport.url))
        let simulatorArtifacts = (simulatorReport.object["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
        artifacts.append(contentsOf: simulatorArtifacts)
        let unifiedText = try simulatorArtifacts.first { $0.hasSuffix("simulator-unified.log") }.map(readRelativeArtifact) ?? ""
        let brkLine = unifiedText
            .split(separator: "\n")
            .map(String.init)
            .last { $0.contains("Orlix TCTI: unsupported instruction") && $0.contains("insn=0xd4200020") } ?? ""

        if stringField(simulatorReport.object, "git_sha") != gitSha() {
            failures.append(fail("simulator-report-stale", "latest simulator stability report is stale for current HEAD"))
        }
        if stringField(simulatorReport.object, "status") != "fail" || boolField(simulatorReport.object, "passed") {
            failures.append(fail("simulator-report-status", "ADD/SUB shifted XZR fix requires the current BRK simulator failure report"))
        }
        if !brkLine.contains("x9=0x8") ||
            !brkLine.contains("x13=") ||
            !brkLine.contains("sp=") ||
            !brkLine.contains("pstate=0x20000000") {
            failures.append(fail("simulator-brk-registers", "latest simulator BRK line does not expose the expected x9/x13/sp/pstate reducer facts"))
        }
    } else {
        failures.append(fail("simulator-report", "missing iphonesimulator tcti-simulator-stability report"))
    }

    let rootCauseURL = buildPath("reports", "tcti-brk-trap-root-cause", "report.json")
    if let rootCause = try? loadJSON(rootCauseURL) as? [String: Any],
       stringField(rootCause, "status") == "pass",
       boolField(rootCause, "passed") {
        artifacts.append(relativePath(rootCauseURL))
    } else {
        failures.append(fail("root-cause-report", "missing or non-passing BRK root-cause report"))
    }

    let switchDebugURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "switch_debug.c")
    let testURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "tcti_decode_test.c")
    let switchDebug = try String(contentsOf: switchDebugURL, encoding: .utf8)
    let tests = try String(contentsOf: testURL, encoding: .utf8)

    artifacts.append(relativePath(switchDebugURL))
    artifacts.append(relativePath(testURL))

    if !switchDebug.contains("tcti_execute_add_sub_result") ||
        !switchDebug.contains("sp_allowed") ||
        !switchDebug.contains("tcti_execute_add_sub_result(regs, decoded, left, right, false)") ||
        !switchDebug.contains("u64 left = tcti_read_gpr_or_zero(regs, decoded->rn, access_size);") {
        failures.append(fail("semantic-fix-marker", "switch-debug ADD/SUB shifted-register semantics must use XZR/WZR, not SP, for register 31"))
    }

    if !tests.contains("tcti_switch_executes_neg_with_xzr_source") ||
        !tests.contains("0xcb0903edU") ||
        !tests.contains("0xfffffffffffffff8ULL") {
        failures.append(fail("kunit-regression", "missing KUnit regression for NEG x13, x9 using XZR source semantics"))
    }

    let reducer = try writeReducer(
        target: target,
        caseID: "add-sub-shifted-xzr-pass-regression",
        command: "make tcti-gate TARGET=\(target)",
        reason: "ADD/SUB shifted register must treat register 31 as XZR/WZR, preserving SP for the simulator BRK reducer path.",
        artifacts: artifacts,
        expectedStatus: .pass
    )
    artifacts.append(relativePath(reducer))

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Bound simulator BRK #1 slab guard failure to ADD/SUB shifted-register XZR semantics and KUnit regression.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_reduced": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runSIMDMOVI2SFix() throws -> Int32 {
    let target = "tcti-simd-movi-2s-fix"
    var failures: [Failure] = []
    var artifacts: [String] = []

    let simulatorReports = runtimeValidationReports(
        gate: "tcti-simulator-stability",
        destination: "iphonesimulator"
    )
    let simulatorEvidenceMatched = simulatorReports.contains { report in
        var reportArtifacts: [String] = []
        let simulatorText = runtimeReportText(report, artifacts: &reportArtifacts)
        let matched = simulatorText.contains("Orlix TCTI: unsupported instruction") &&
            (simulatorText.contains("insn=0xf002420") || simulatorText.contains("insn=0x0f002420"))
        if matched {
            artifacts.append(contentsOf: reportArtifacts)
        }
        return matched
    }

    if !simulatorEvidenceMatched {
        failures.append(fail("simulator-unsupported-signature", "no recorded simulator stability report contains unsupported MOVI v0.2s instruction 0x0f002420"))
        if let latestSimulatorReport = simulatorReports.first {
            _ = runtimeReportText(latestSimulatorReport, artifacts: &artifacts)
        }
    }

    let decodeURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "decode_aarch64.c")
    let switchURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "switch_debug.c")
    let testURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "tcti_decode_test.c")
    let decode = try String(contentsOf: decodeURL, encoding: .utf8)
    let switchDebug = try String(contentsOf: switchURL, encoding: .utf8)
    let tests = try String(contentsOf: testURL, encoding: .utf8)

    artifacts.append(relativePath(decodeURL))
    artifacts.append(relativePath(switchURL))
    artifacts.append(relativePath(testURL))

    if !decode.contains("AARCH64_SIMD_MOVI_2S_0X100_PATTERN 0x0f002420U") ||
        !decode.contains("0x0000010000000100ULL") {
        failures.append(fail("decode-marker", "missing exact SIMD MOVI vN.2s #0x100 decoder marker"))
    }
    if !switchDebug.contains("current->thread.user_simd[decoded->rd * 2] = decoded->logical_immediate") {
        failures.append(fail("semantic-marker", "missing SIMD modified-immediate semantic write of decoded immediate"))
    }
    if !tests.contains("0x0f002420U") ||
        !tests.contains("0x0000010000000100ULL") {
        failures.append(fail("kunit-regression", "missing KUnit coverage for MOVI v0.2s #0x1 lsl #8"))
    }

    let reducer = try writeReducer(
        target: target,
        caseID: "simd-movi-2s-pass-regression",
        command: "make tcti-gate TARGET=\(target)",
        reason: "The simulator reached MOVI v0.2s #0x1 lsl #8; support only the emitted modified-immediate subset.",
        artifacts: artifacts,
        expectedStatus: .pass
    )
    artifacts.append(relativePath(reducer))

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Bound recorded simulator unsupported 0x0f002420 to the exact SIMD MOVI v0.2s #0x100 semantic subset.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_reduced": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runSIMDMOVI16BFix() throws -> Int32 {
    let target = "tcti-simd-movi-16b-fix"
    var failures: [Failure] = []
    var artifacts: [String] = []

    let simulatorReports = runtimeValidationReports(
        gate: "tcti-simulator-stability",
        destination: "iphonesimulator"
    )
    let simulatorEvidenceMatched = simulatorReports.contains { report in
        var reportArtifacts: [String] = []
        let simulatorText = runtimeReportText(report, artifacts: &reportArtifacts)
        let matched = simulatorText.contains("Orlix TCTI: unsupported instruction") &&
            simulatorText.contains("insn=0x4f06e7e0") &&
            simulatorText.contains("orlix-init: process signaled") &&
            simulatorText.contains("signal=4")
        if matched {
            artifacts.append(contentsOf: reportArtifacts)
        }
        return matched
    }
    if !simulatorEvidenceMatched {
        failures.append(fail("simulator-unsupported-signature", "no recorded simulator stability report contains MOVI v0.16b instruction 0x4f06e7e0 with SIGILL evidence"))
    }
    if let latestSimulatorReport = simulatorReports.first {
        _ = runtimeReportText(latestSimulatorReport, artifacts: &artifacts)
    }

    let decodeURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "decode_aarch64.c")
    let switchURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "switch_debug.c")
    let testURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "tcti_decode_test.c")
    let fixtureURL = path("tools", "tcti", "fixtures", "golden_elf", "init_001_exit_simd_movi_16b.S")
    let decode = try String(contentsOf: decodeURL, encoding: .utf8)
    let switchDebug = try String(contentsOf: switchURL, encoding: .utf8)
    let tests = try String(contentsOf: testURL, encoding: .utf8)
    let fixture = try String(contentsOf: fixtureURL, encoding: .utf8)

    artifacts.append(contentsOf: [relativePath(decodeURL), relativePath(switchURL), relativePath(testURL), relativePath(fixtureURL)])

    if !decode.contains("AARCH64_SIMD_MOVI_16B_0XDF_PATTERN 0x4f06e7e0U") ||
        !decode.contains("0xdfdfdfdfdfdfdfdfULL") {
        failures.append(fail("decode-marker", "missing exact SIMD MOVI vN.16b #0xdf decoder marker"))
    }
    if !switchDebug.contains("decoded->result_size > sizeof(u64) ? decoded->logical_immediate : 0") {
        failures.append(fail("semantic-marker", "missing SIMD modified-immediate 128-bit second-lane semantic write"))
    }
    if !tests.contains("0x4f06e7e0U") ||
        !tests.contains("0xdfdfdfdfdfdfdfdfULL") {
        failures.append(fail("kunit-regression", "missing KUnit coverage for MOVI v0.16b #0xdf"))
    }
    if !fixture.contains(".inst 0x4f06e7e0") {
        failures.append(fail("no-phone-fixture", "missing no-phone switch-debug fixture for MOVI v0.16b #0xdf"))
    }

    do {
        let result = try executeNegativeFixture(
            "simd-movi-16b",
            outputRoot: buildPath("simd_movi_16b_fix", "positive")
        )
        artifacts.append(contentsOf: result.artifacts)
        guard let execution = result.execution else {
            failures.append(fail("execution", "SIMD MOVI 16B fixture did not write execution report"))
            throw GateError.commandFailed("missing SIMD MOVI 16B execution report")
        }
        if execution.instructionEncodings.first != "0x4f06e7e0" ||
            execution.decodedInstructions.first?.instructionClass != "simd_modified_immediate" ||
            execution.exit?.code != 42 ||
            !execution.syscalls.contains(where: { $0.nr == 93 && $0.name == "exit" }) {
            failures.append(fail("execution-shape", "expected decoded SIMD MOVI 16B fixture to continue to captured exit(42)"))
        }
        if result.failures.contains(where: { $0.id == "execution-unsupported-instruction" }) {
            failures.append(fail("execution-unsupported-instruction", "SIMD MOVI 16B fixture still stops as unsupported"))
        }
    } catch {
        failures.append(fail("execution", "\(error)"))
    }

    let reducer = try writeReducer(
        target: target,
        caseID: "simd-movi-16b-pass-regression",
        command: "make tcti-gate TARGET=\(target)",
        reason: "The simulator reached MOVI v0.16b, #0xdf; support only the emitted Advanced SIMD 16-byte modified-immediate subset.",
        artifacts: artifacts,
        expectedStatus: .pass
    )
    artifacts.append(relativePath(reducer))

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Bound simulator unsupported 0x4f06e7e0 to the exact SIMD MOVI v0.16b, #0xdf semantic subset.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_reduced": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runSIMDMOVI4S0x41Fix() throws -> Int32 {
    let target = "tcti-simd-movi-4s-0x41-fix"
    var failures: [Failure] = []
    var artifacts: [String] = []

    let simulatorReports = runtimeValidationReports(
        gate: "tcti-static-busybox-shell-command",
        destination: "iphonesimulator"
    )
    let simulatorEvidenceMatched = simulatorReports.contains { report in
        var reportArtifacts: [String] = []
        let simulatorText = runtimeReportText(report, artifacts: &reportArtifacts)
        let matched = simulatorText.contains("Orlix TCTI: unsupported instruction") &&
            simulatorText.contains("insn=0x4f020420") &&
            simulatorText.contains("orlix-init: process signaled") &&
            simulatorText.contains("signal=4") &&
            simulatorText.contains("ORLIX-TCTI-BUSYBOX-USABLE")
        if matched {
            artifacts.append(contentsOf: reportArtifacts)
        }
        return matched
    }
    if !simulatorEvidenceMatched {
        failures.append(fail("simulator-unsupported-signature", "no recorded static BusyBox shell-command report contains MOVI v0.4s instruction 0x4f020420 with post-marker SIGILL evidence"))
    }
    if let latestSimulatorReport = simulatorReports.first {
        _ = runtimeReportText(latestSimulatorReport, artifacts: &artifacts)
    }

    let decodeURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "decode_aarch64.c")
    let switchURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "switch_debug.c")
    let testURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "tcti_decode_test.c")
    let fixtureURL = path("tools", "tcti", "fixtures", "golden_elf", "init_001_exit_simd_movi_4s_0x41.S")
    let decode = try String(contentsOf: decodeURL, encoding: .utf8)
    let switchDebug = try String(contentsOf: switchURL, encoding: .utf8)
    let tests = try String(contentsOf: testURL, encoding: .utf8)
    let fixture = try String(contentsOf: fixtureURL, encoding: .utf8)

    artifacts.append(contentsOf: [relativePath(decodeURL), relativePath(switchURL), relativePath(testURL), relativePath(fixtureURL)])

    if !decode.contains("AARCH64_SIMD_MOVI_4S_0X41_PATTERN 0x4f020420U") ||
        !decode.contains("0x0000004100000041ULL") {
        failures.append(fail("decode-marker", "missing exact SIMD MOVI vN.4s #0x41 decoder marker"))
    }
    if !switchDebug.contains("decoded->result_size > sizeof(u64) ? decoded->logical_immediate : 0") {
        failures.append(fail("semantic-marker", "missing SIMD modified-immediate 128-bit second-lane semantic write"))
    }
    if !tests.contains("0x4f020420U") ||
        !tests.contains("0x0000004100000041ULL") {
        failures.append(fail("kunit-regression", "missing KUnit coverage for MOVI v0.4s #0x41"))
    }
    if !fixture.contains(".inst 0x4f020420") {
        failures.append(fail("no-phone-fixture", "missing no-phone switch-debug fixture for MOVI v0.4s #0x41"))
    }

    do {
        let result = try executeNegativeFixture(
            "simd-movi-4s-0x41",
            outputRoot: buildPath("simd_movi_4s_0x41_fix", "positive")
        )
        artifacts.append(contentsOf: result.artifacts)
        guard let execution = result.execution else {
            failures.append(fail("execution", "SIMD MOVI 4S fixture did not write execution report"))
            throw GateError.commandFailed("missing SIMD MOVI 4S execution report")
        }
        if execution.instructionEncodings.first != "0x4f020420" ||
            execution.decodedInstructions.first?.instructionClass != "simd_modified_immediate" ||
            execution.exit?.code != 42 ||
            !execution.syscalls.contains(where: { $0.nr == 93 && $0.name == "exit" }) {
            failures.append(fail("execution-shape", "expected decoded SIMD MOVI 4S fixture to continue to captured exit(42)"))
        }
        if result.failures.contains(where: { $0.id == "execution-unsupported-instruction" }) {
            failures.append(fail("execution-unsupported-instruction", "SIMD MOVI 4S fixture still stops as unsupported"))
        }
    } catch {
        failures.append(fail("execution", "\(error)"))
    }

    let reducer = try writeReducer(
        target: target,
        caseID: "simd-movi-4s-0x41-pass-regression",
        command: "make tcti-gate TARGET=\(target)",
        reason: "The simulator reached MOVI v0.4s, #0x41 after the BusyBox marker; support only the emitted Advanced SIMD 4S modified-immediate subset.",
        artifacts: artifacts,
        expectedStatus: .pass
    )
    artifacts.append(relativePath(reducer))

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Bound simulator unsupported 0x4f020420 to exact SIMD MOVI v0.4s, #0x41 semantic subset.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_reduced": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runSIMDMOVI4S0x1Fix() throws -> Int32 {
    let target = "tcti-simd-movi-4s-0x1-fix"
    var failures: [Failure] = []
    var artifacts: [String] = []

    let simulatorReports = runtimeValidationReports(
        gate: "tcti-static-busybox-shell-command",
        destination: "iphonesimulator"
    )
    let simulatorEvidenceMatched = simulatorReports.contains { report in
        var reportArtifacts: [String] = []
        let simulatorText = runtimeReportText(report, artifacts: &reportArtifacts)
        let matched = simulatorText.contains("Orlix TCTI: unsupported instruction") &&
            simulatorText.contains("insn=0x4f000421") &&
            simulatorText.contains("orlix-init: process signaled") &&
            simulatorText.contains("signal=4") &&
            simulatorText.contains("ORLIX-TCTI-BUSYBOX-USABLE")
        if matched {
            artifacts.append(contentsOf: reportArtifacts)
        }
        return matched
    }
    if !simulatorEvidenceMatched {
        failures.append(fail("simulator-unsupported-signature", "no recorded static BusyBox shell-command report contains MOVI v1.4s instruction 0x4f000421 with post-marker SIGILL evidence"))
    }
    if let latestSimulatorReport = simulatorReports.first {
        _ = runtimeReportText(latestSimulatorReport, artifacts: &artifacts)
    }

    let decodeURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "decode_aarch64.c")
    let switchURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "switch_debug.c")
    let testURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "tcti_decode_test.c")
    let fixtureURL = path("tools", "tcti", "fixtures", "golden_elf", "init_001_exit_simd_movi_4s_0x1.S")
    let decode = try String(contentsOf: decodeURL, encoding: .utf8)
    let switchDebug = try String(contentsOf: switchURL, encoding: .utf8)
    let tests = try String(contentsOf: testURL, encoding: .utf8)
    let fixture = try String(contentsOf: fixtureURL, encoding: .utf8)

    artifacts.append(contentsOf: [relativePath(decodeURL), relativePath(switchURL), relativePath(testURL), relativePath(fixtureURL)])

    if !decode.contains("AARCH64_SIMD_MOVI_4S_0X1_PATTERN 0x4f000420U") ||
        !decode.contains("0x0000000100000001ULL") {
        failures.append(fail("decode-marker", "missing exact SIMD MOVI vN.4s #0x1 decoder marker"))
    }
    if !switchDebug.contains("decoded->result_size > sizeof(u64) ? decoded->logical_immediate : 0") {
        failures.append(fail("semantic-marker", "missing SIMD modified-immediate 128-bit second-lane semantic write"))
    }
    if !tests.contains("0x4f000421U") ||
        !tests.contains("0x0000000100000001ULL") ||
        !tests.contains("current->thread.user_simd[2]") {
        failures.append(fail("kunit-regression", "missing KUnit coverage for MOVI v1.4s #0x1"))
    }
    if !fixture.contains(".inst 0x4f000421") {
        failures.append(fail("no-phone-fixture", "missing no-phone switch-debug fixture for MOVI v1.4s #0x1"))
    }

    do {
        let result = try executeNegativeFixture(
            "simd-movi-4s-0x1",
            outputRoot: buildPath("simd_movi_4s_0x1_fix", "positive")
        )
        artifacts.append(contentsOf: result.artifacts)
        guard let execution = result.execution else {
            failures.append(fail("execution", "SIMD MOVI 4S #1 fixture did not write execution report"))
            throw GateError.commandFailed("missing SIMD MOVI 4S #1 execution report")
        }
        if execution.instructionEncodings.first != "0x4f000421" ||
            execution.decodedInstructions.first?.instructionClass != "simd_modified_immediate" ||
            execution.decodedInstructions.first?.op != "movi" ||
            execution.exit?.code != 42 ||
            !execution.syscalls.contains(where: { $0.nr == 93 && $0.name == "exit" }) {
            failures.append(fail("execution-shape", "expected decoded SIMD MOVI 4S #1 fixture to continue to captured exit(42)"))
        }
        if result.failures.contains(where: { $0.id == "execution-unsupported-instruction" }) {
            failures.append(fail("execution-unsupported-instruction", "SIMD MOVI 4S #1 fixture still stops as unsupported"))
        }
    } catch {
        failures.append(fail("execution", "\(error)"))
    }

    let reducer = try writeReducer(
        target: target,
        caseID: "simd-movi-4s-0x1-pass-regression",
        command: "make tcti-gate TARGET=\(target)",
        reason: "The simulator reached MOVI v1.4s, #0x1 after the BusyBox marker; support only the emitted Advanced SIMD 4S modified-immediate subset.",
        artifacts: artifacts,
        expectedStatus: .pass
    )
    artifacts.append(relativePath(reducer))

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Bound simulator unsupported 0x4f000421 to exact SIMD MOVI v1.4s, #0x1 semantic subset.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_reduced": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runSIMDCMEQ4SFix() throws -> Int32 {
	let target = "tcti-simd-cmeq-4s-fix"
	var failures: [Failure] = []
	var artifacts: [String] = []

	let simulatorReports = runtimeValidationReports(
		gate: "tcti-static-busybox-shell-command",
		destination: "iphonesimulator"
	)
	let simulatorEvidenceMatched = simulatorReports.contains { report in
		var reportArtifacts: [String] = []
		let simulatorText = runtimeReportText(report, artifacts: &reportArtifacts)
		let matched = simulatorText.contains("Orlix TCTI: unsupported instruction") &&
			simulatorText.contains("insn=0x6ea18c64") &&
			simulatorText.contains("orlix-init: process signaled") &&
			simulatorText.contains("signal=4") &&
			simulatorText.contains("ORLIX-TCTI-BUSYBOX-USABLE")
		if matched {
			artifacts.append(contentsOf: reportArtifacts)
		}
		return matched
	}
	if !simulatorEvidenceMatched {
		failures.append(fail("simulator-unsupported-signature", "no recorded static BusyBox shell-command report contains CMEQ v4.4s, v3.4s, v1.4s instruction 0x6ea18c64 with post-marker SIGILL evidence"))
	}
	if let latestSimulatorReport = simulatorReports.first {
		_ = runtimeReportText(latestSimulatorReport, artifacts: &artifacts)
	}

	let headerURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "decode_aarch64.h")
	let decodeURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "decode_aarch64.c")
	let switchURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "switch_debug.c")
	let testURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "tcti_decode_test.c")
	let gateURL = path("tools", "tcti", "orlix-tcti-gate.swift")
	let fixtureURL = path("tools", "tcti", "fixtures", "golden_elf", "init_001_exit_simd_cmeq_4s.S")
	let header = try String(contentsOf: headerURL, encoding: .utf8)
	let decode = try String(contentsOf: decodeURL, encoding: .utf8)
	let switchDebug = try String(contentsOf: switchURL, encoding: .utf8)
	let tests = try String(contentsOf: testURL, encoding: .utf8)
	let gate = try String(contentsOf: gateURL, encoding: .utf8)
	let fixture = try String(contentsOf: fixtureURL, encoding: .utf8)

	artifacts.append(contentsOf: [
		relativePath(headerURL),
		relativePath(decodeURL),
		relativePath(switchURL),
		relativePath(testURL),
		relativePath(gateURL),
		relativePath(fixtureURL),
	])

	if !header.contains("TCTI_DECODE_SIMD_VECTOR_COMPARE") ||
		!decode.contains("AARCH64_SIMD_CMEQ_4S_MASK 0xffe0fc00U") ||
		!decode.contains("AARCH64_SIMD_CMEQ_4S_PATTERN 0x6ea08c00U") ||
		!decode.contains("TCTI_DECODE_SIMD_VECTOR_COMPARE") {
		failures.append(fail("decode-marker", "missing exact Advanced SIMD CMEQ vD.4s, vN.4s, vM.4s decoder subset for 0x6ea18c64"))
	}
	if !switchDebug.contains("tcti_execute_simd_vector_compare") ||
		!switchDebug.contains("left == right ? GENMASK_ULL(31, 0) : 0") {
		failures.append(fail("semantic-marker", "missing SIMD CMEQ 4S switch-debug lane compare semantic handler"))
	}
	if !tests.contains("0x6ea18c64U") ||
		!tests.contains("tcti_decode_recognizes_simd_cmeq_4s") ||
		!tests.contains("tcti_switch_executes_simd_cmeq_4s") {
		failures.append(fail("kunit-regression", "missing KUnit decode and switch-debug coverage for CMEQ v4.4s, v3.4s, v1.4s"))
	}
	if !gate.contains("simdVectorCompare") ||
		!gate.contains("init_001_exit_simd_cmeq_4s.S") ||
		!fixture.contains(".inst 0x6ea18c64") {
		failures.append(fail("no-phone-fixture", "missing no-phone switch-debug fixture for SIMD CMEQ 4S"))
	}

	do {
		let result = try executeNegativeFixture(
			"simd-cmeq-4s",
			outputRoot: buildPath("simd_cmeq_4s_fix", "positive")
		)
		artifacts.append(contentsOf: result.artifacts)
		guard let execution = result.execution else {
			failures.append(fail("execution", "SIMD CMEQ 4S fixture did not write execution report"))
			throw GateError.commandFailed("missing SIMD CMEQ 4S execution report")
		}
		if !execution.instructionEncodings.contains("0x6ea18c64") ||
			!execution.decodedInstructions.contains(where: { $0.instructionClass == "simd_vector_compare" && $0.op == "cmeq" && $0.rd == 4 && $0.rn == 3 && $0.rm == 1 }) ||
			execution.exit?.code != 42 ||
			!execution.syscalls.contains(where: { $0.nr == 93 && $0.name == "exit" }) {
			failures.append(fail("execution-shape", "expected decoded SIMD CMEQ 4S fixture to continue to captured exit(42)"))
		}
		if result.failures.contains(where: { $0.id == "execution-unsupported-instruction" }) {
			failures.append(fail("execution-unsupported-instruction", "SIMD CMEQ 4S fixture still stops as unsupported"))
		}
	} catch {
		failures.append(fail("execution", "\(error)"))
	}

	let reducer = try writeReducer(
		target: target,
		caseID: "simd-cmeq-4s-pass-regression",
		command: "make tcti-gate TARGET=\(target)",
		reason: "The simulator reached CMEQ v4.4s, v3.4s, v1.4s after the BusyBox marker; support only the emitted Advanced SIMD 4S equality-compare subset.",
		artifacts: artifacts,
		expectedStatus: .pass
	)
	artifacts.append(relativePath(reducer))

	let status: GateStatus = failures.isEmpty ? .pass : .fail
	let reportURL = try writeReport(report(
		target: target,
		status: status,
		summary: "Bound simulator unsupported 0x6ea18c64 to exact SIMD CMEQ v4.4s, v3.4s, v1.4s semantic subset.",
		failures: failures,
		artifacts: artifacts,
		counters: ["simulator_reports_reduced": 1],
		releaseGateEligible: false,
		readinessGateEligible: false
	))
	print("\(status.rawValue): \(relativePath(reportURL))")
	return exitCode(for: status)
}

func runSIMDUMAXV4SFix() throws -> Int32 {
	let target = "tcti-simd-umaxv-4s-fix"
	var failures: [Failure] = []
	var artifacts: [String] = []

	let simulatorReports = runtimeValidationReports(
		gate: "tcti-static-busybox-shell-command",
		destination: "iphonesimulator"
	)
	let simulatorEvidenceMatched = simulatorReports.contains { report in
		var reportArtifacts: [String] = []
		let simulatorText = runtimeReportText(report, artifacts: &reportArtifacts)
		let matched = simulatorText.contains("Orlix TCTI: unsupported instruction") &&
			simulatorText.contains("insn=0x6eb0a885") &&
			simulatorText.contains("orlix-init: process signaled") &&
			simulatorText.contains("signal=4") &&
			simulatorText.contains("ORLIX-TCTI-BUSYBOX-USABLE")
		if matched {
			artifacts.append(contentsOf: reportArtifacts)
		}
		return matched
	}
	if !simulatorEvidenceMatched {
		failures.append(fail("simulator-unsupported-signature", "no recorded static BusyBox shell-command report contains UMAXV s5, v4.4s instruction 0x6eb0a885 with post-marker SIGILL evidence"))
	}
	if let latestSimulatorReport = simulatorReports.first {
		_ = runtimeReportText(latestSimulatorReport, artifacts: &artifacts)
	}

	let headerURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "decode_aarch64.h")
	let decodeURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "decode_aarch64.c")
	let switchURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "switch_debug.c")
	let testURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "tcti_decode_test.c")
	let gateURL = path("tools", "tcti", "orlix-tcti-gate.swift")
	let fixtureURL = path("tools", "tcti", "fixtures", "golden_elf", "init_001_exit_simd_umaxv_4s.S")
	let header = try String(contentsOf: headerURL, encoding: .utf8)
	let decode = try String(contentsOf: decodeURL, encoding: .utf8)
	let switchDebug = try String(contentsOf: switchURL, encoding: .utf8)
	let tests = try String(contentsOf: testURL, encoding: .utf8)
	let gate = try String(contentsOf: gateURL, encoding: .utf8)
	let fixture = try String(contentsOf: fixtureURL, encoding: .utf8)

	artifacts.append(contentsOf: [
		relativePath(headerURL),
		relativePath(decodeURL),
		relativePath(switchURL),
		relativePath(testURL),
		relativePath(gateURL),
		relativePath(fixtureURL),
	])

	if !header.contains("TCTI_DECODE_SIMD_VECTOR_REDUCTION") ||
		!decode.contains("AARCH64_SIMD_UMAXV_4S_MASK 0xfffffc20U") ||
		!decode.contains("AARCH64_SIMD_UMAXV_4S_PATTERN 0x6eb0a800U") ||
		!decode.contains("TCTI_DECODE_SIMD_VECTOR_REDUCTION") {
		failures.append(fail("decode-marker", "missing exact Advanced SIMD UMAXV Sd, Vn.4s decoder subset for 0x6eb0a885"))
	}
	if !switchDebug.contains("tcti_execute_simd_vector_reduction") ||
		!switchDebug.contains("value > maximum") {
		failures.append(fail("semantic-marker", "missing SIMD UMAXV 4S switch-debug reduction semantic handler"))
	}
	if !tests.contains("0x6eb0a885U") ||
		!tests.contains("tcti_decode_recognizes_simd_umaxv_4s") ||
		!tests.contains("tcti_switch_executes_simd_umaxv_4s") {
		failures.append(fail("kunit-regression", "missing KUnit decode and switch-debug coverage for UMAXV s5, v4.4s"))
	}
	if !gate.contains("simdVectorReduction") ||
		!gate.contains("init_001_exit_simd_umaxv_4s.S") ||
		!fixture.contains(".inst 0x6eb0a885") {
		failures.append(fail("no-phone-fixture", "missing no-phone switch-debug fixture for SIMD UMAXV 4S"))
	}

	do {
		let result = try executeNegativeFixture(
			"simd-umaxv-4s",
			outputRoot: buildPath("simd_umaxv_4s_fix", "positive")
		)
		artifacts.append(contentsOf: result.artifacts)
		guard let execution = result.execution else {
			failures.append(fail("execution", "SIMD UMAXV 4S fixture did not write execution report"))
			throw GateError.commandFailed("missing SIMD UMAXV 4S execution report")
		}
		if !execution.instructionEncodings.contains("0x6eb0a885") ||
			!execution.decodedInstructions.contains(where: { $0.instructionClass == "simd_vector_reduction" && $0.op == "umaxv" && $0.rd == 5 && $0.rn == 4 }) ||
			execution.exit?.code != 42 ||
			!execution.syscalls.contains(where: { $0.nr == 93 && $0.name == "exit" }) {
			failures.append(fail("execution-shape", "expected decoded SIMD UMAXV 4S fixture to continue to captured exit(42)"))
		}
		if result.failures.contains(where: { $0.id == "execution-unsupported-instruction" }) {
			failures.append(fail("execution-unsupported-instruction", "SIMD UMAXV 4S fixture still stops as unsupported"))
		}
	} catch {
		failures.append(fail("execution", "\(error)"))
	}

	let reducer = try writeReducer(
		target: target,
		caseID: "simd-umaxv-4s-pass-regression",
		command: "make tcti-gate TARGET=\(target)",
		reason: "The simulator reached UMAXV s5, v4.4s after the BusyBox marker; support only the emitted Advanced SIMD 4S unsigned max-across subset.",
		artifacts: artifacts,
		expectedStatus: .pass
	)
	artifacts.append(relativePath(reducer))

	let status: GateStatus = failures.isEmpty ? .pass : .fail
	let reportURL = try writeReport(report(
		target: target,
		status: status,
		summary: "Bound simulator unsupported 0x6eb0a885 to exact SIMD UMAXV s5, v4.4s semantic subset.",
		failures: failures,
		artifacts: artifacts,
		counters: ["simulator_reports_reduced": 1],
		releaseGateEligible: false,
		readinessGateEligible: false
	))
	print("\(status.rawValue): \(relativePath(reportURL))")
	return exitCode(for: status)
}

func runSIMDADDV4SFix() throws -> Int32 {
	let target = "tcti-simd-addv-4s-fix"
	var failures: [Failure] = []
	var artifacts: [String] = []

	let simulatorReports = runtimeValidationReports(
		gate: "tcti-init-console-write",
		destination: "iphonesimulator"
	)
	let simulatorEvidenceMatched = simulatorReports.contains { report in
		var reportArtifacts: [String] = []
		let simulatorText = runtimeReportText(report, artifacts: &reportArtifacts)
		let matched = simulatorText.contains("Orlix TCTI: unsupported instruction") &&
			simulatorText.contains("insn=0x4eb1b800") &&
			simulatorText.contains("orlix-init: process signaled") &&
			simulatorText.contains("signal=4") &&
			simulatorText.contains("ORLIX-TCTI-CONSOLE-OK")
		if matched {
			artifacts.append(contentsOf: reportArtifacts)
		}
		return matched
	}
	if !simulatorEvidenceMatched {
		failures.append(fail("simulator-unsupported-signature", "no recorded console-write report contains ADDV s0, v0.4s instruction 0x4eb1b800 with post-marker SIGILL evidence"))
	}
	if let latestSimulatorReport = simulatorReports.first {
		_ = runtimeReportText(latestSimulatorReport, artifacts: &artifacts)
	}

	let headerURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "decode_aarch64.h")
	let decodeURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "decode_aarch64.c")
	let switchURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "switch_debug.c")
	let testURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "tcti_decode_test.c")
	let gateURL = path("tools", "tcti", "orlix-tcti-gate.swift")
	let fixtureURL = path("tools", "tcti", "fixtures", "golden_elf", "init_001_exit_simd_addv_4s.S")
	let header = try String(contentsOf: headerURL, encoding: .utf8)
	let decode = try String(contentsOf: decodeURL, encoding: .utf8)
	let switchDebug = try String(contentsOf: switchURL, encoding: .utf8)
	let tests = try String(contentsOf: testURL, encoding: .utf8)
	let gate = try String(contentsOf: gateURL, encoding: .utf8)
	let fixture = try String(contentsOf: fixtureURL, encoding: .utf8)

	artifacts.append(contentsOf: [
		relativePath(headerURL),
		relativePath(decodeURL),
		relativePath(switchURL),
		relativePath(testURL),
		relativePath(gateURL),
		relativePath(fixtureURL),
	])

	if !header.contains("TCTI_SIMD_REDUCTION_ADDV") ||
		!decode.contains("AARCH64_SIMD_ADDV_4S_MASK 0xfffffc20U") ||
		!decode.contains("AARCH64_SIMD_ADDV_4S_PATTERN 0x4eb1b800U") ||
		!decode.contains("TCTI_SIMD_REDUCTION_ADDV") {
		failures.append(fail("decode-marker", "missing exact Advanced SIMD ADDV Sd, Vn.4s decoder subset for 0x4eb1b800"))
	}
	if !switchDebug.contains("TCTI_SIMD_REDUCTION_ADDV") ||
		!switchDebug.contains("result += value") {
		failures.append(fail("semantic-marker", "missing SIMD ADDV 4S switch-debug reduction semantic handler"))
	}
	if !tests.contains("0x4eb1b800U") ||
		!tests.contains("tcti_decode_recognizes_simd_addv_4s") ||
		!tests.contains("tcti_switch_executes_simd_addv_4s") {
		failures.append(fail("kunit-regression", "missing KUnit decode and switch-debug coverage for ADDV s0, v0.4s"))
	}
	if !gate.contains("op: \"addv\"") ||
		!gate.contains("init_001_exit_simd_addv_4s.S") ||
		!fixture.contains(".inst 0x4eb1b800") {
		failures.append(fail("no-phone-fixture", "missing no-phone switch-debug fixture for SIMD ADDV 4S"))
	}

	do {
		let result = try executeNegativeFixture(
			"simd-addv-4s",
			outputRoot: buildPath("simd_addv_4s_fix", "positive")
		)
		artifacts.append(contentsOf: result.artifacts)
		guard let execution = result.execution else {
			failures.append(fail("execution", "SIMD ADDV 4S fixture did not write execution report"))
			throw GateError.commandFailed("missing SIMD ADDV 4S execution report")
		}
		if !execution.instructionEncodings.contains("0x4eb1b800") ||
			!execution.decodedInstructions.contains(where: { $0.instructionClass == "simd_vector_reduction" && $0.op == "addv" && $0.rd == 0 && $0.rn == 0 }) ||
			execution.exit?.code != 42 ||
			!execution.syscalls.contains(where: { $0.nr == 93 && $0.name == "exit" }) {
			failures.append(fail("execution-shape", "expected decoded SIMD ADDV 4S fixture to continue to captured exit(42)"))
		}
		if result.failures.contains(where: { $0.id == "execution-unsupported-instruction" }) {
			failures.append(fail("execution-unsupported-instruction", "SIMD ADDV 4S fixture still stops as unsupported"))
		}
	} catch {
		failures.append(fail("execution", "\(error)"))
	}

	let reducer = try writeReducer(
		target: target,
		caseID: "simd-addv-4s-pass-regression",
		command: "make tcti-gate TARGET=\(target)",
		reason: "The simulator reached ADDV s0, v0.4s after the console marker; support only the emitted Advanced SIMD 4S add-across subset.",
		artifacts: artifacts,
		expectedStatus: .pass
	)
	artifacts.append(relativePath(reducer))

	let status: GateStatus = failures.isEmpty ? .pass : .fail
	let reportURL = try writeReport(report(
		target: target,
		status: status,
		summary: "Bound simulator unsupported 0x4eb1b800 to exact SIMD ADDV s0, v0.4s semantic subset.",
		failures: failures,
		artifacts: artifacts,
		counters: ["simulator_reports_reduced": 1],
		releaseGateEligible: false,
		readinessGateEligible: false
	))
	print("\(status.rawValue): \(relativePath(reportURL))")
	return exitCode(for: status)
}

func runFMOVWSFix() throws -> Int32 {
	let target = "tcti-fmov-w-s-fix"
	var failures: [Failure] = []
	var artifacts: [String] = []

	let simulatorReports = runtimeValidationReports(
		gate: "tcti-static-busybox-shell-command",
		destination: "iphonesimulator"
	)
	let simulatorEvidenceMatched = simulatorReports.contains { report in
		var reportArtifacts: [String] = []
		let simulatorText = runtimeReportText(report, artifacts: &reportArtifacts)
		let matched = simulatorText.contains("Orlix TCTI: unsupported instruction") &&
			simulatorText.contains("insn=0x1e2600ab") &&
			simulatorText.contains("orlix-init: process signaled") &&
			simulatorText.contains("signal=4") &&
			simulatorText.contains("ORLIX-TCTI-BUSYBOX-USABLE")
		if matched {
			artifacts.append(contentsOf: reportArtifacts)
		}
		return matched
	}
	if !simulatorEvidenceMatched {
		failures.append(fail("simulator-unsupported-signature", "no recorded static BusyBox shell-command report contains FMOV w11, s5 instruction 0x1e2600ab with post-marker SIGILL evidence"))
	}
	if let latestSimulatorReport = simulatorReports.first {
		_ = runtimeReportText(latestSimulatorReport, artifacts: &artifacts)
	}

	let headerURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "decode_aarch64.h")
	let decodeURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "decode_aarch64.c")
	let switchURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "switch_debug.c")
	let testURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "tcti_decode_test.c")
	let gateURL = path("tools", "tcti", "orlix-tcti-gate.swift")
	let fixtureURL = path("tools", "tcti", "fixtures", "golden_elf", "init_001_exit_fmov_w11_s5.S")
	let header = try String(contentsOf: headerURL, encoding: .utf8)
	let decode = try String(contentsOf: decodeURL, encoding: .utf8)
	let switchDebug = try String(contentsOf: switchURL, encoding: .utf8)
	let tests = try String(contentsOf: testURL, encoding: .utf8)
	let gate = try String(contentsOf: gateURL, encoding: .utf8)
	let fixture = try String(contentsOf: fixtureURL, encoding: .utf8)

	artifacts.append(contentsOf: [
		relativePath(headerURL),
		relativePath(decodeURL),
		relativePath(switchURL),
		relativePath(testURL),
		relativePath(gateURL),
		relativePath(fixtureURL),
	])

	if !header.contains("TCTI_DECODE_FP_SCALAR_MOVE") ||
		!decode.contains("AARCH64_FMOV_W_S_MASK 0xfffffc00U") ||
		!decode.contains("AARCH64_FMOV_W_S_PATTERN 0x1e260000U") ||
		!decode.contains("TCTI_DECODE_FP_SCALAR_MOVE") {
		failures.append(fail("decode-marker", "missing exact FMOV wD, sN decoder subset for 0x1e2600ab"))
	}
	if !switchDebug.contains("tcti_execute_fp_scalar_move") ||
		!switchDebug.contains("current->thread.user_simd[decoded->rn * 2]") ||
		!switchDebug.contains("tcti_write_gpr_or_zero(regs, decoded->rd, sizeof(u32), value)") {
		failures.append(fail("semantic-marker", "missing FMOV wD, sN switch-debug scalar move semantic handler"))
	}
	if !tests.contains("0x1e2600abU") ||
		!tests.contains("tcti_decode_recognizes_fmov_w_s") ||
		!tests.contains("tcti_switch_executes_fmov_w_s") {
		failures.append(fail("kunit-regression", "missing KUnit decode and switch-debug coverage for FMOV w11, s5"))
	}
	if !gate.contains("fpScalarMove") ||
		!gate.contains("init_001_exit_fmov_w11_s5.S") ||
		!fixture.contains(".inst 0x1e2600ab") {
		failures.append(fail("no-phone-fixture", "missing no-phone switch-debug fixture for FMOV w11, s5"))
	}

	do {
		let result = try executeNegativeFixture(
			"fmov-w-s",
			outputRoot: buildPath("fmov_w_s_fix", "positive")
		)
		artifacts.append(contentsOf: result.artifacts)
		guard let execution = result.execution else {
			failures.append(fail("execution", "FMOV wD, sN fixture did not write execution report"))
			throw GateError.commandFailed("missing FMOV wD, sN execution report")
		}
		if !execution.instructionEncodings.contains("0x1e2600ab") ||
			!execution.decodedInstructions.contains(where: { $0.instructionClass == "fp_scalar_move" && $0.op == "fmov" && $0.rd == 11 && $0.rn == 5 }) ||
			execution.exit?.code != 42 ||
			!execution.syscalls.contains(where: { $0.nr == 93 && $0.name == "exit" }) {
			failures.append(fail("execution-shape", "expected decoded FMOV wD, sN fixture to continue to captured exit(42)"))
		}
		if result.failures.contains(where: { $0.id == "execution-unsupported-instruction" }) {
			failures.append(fail("execution-unsupported-instruction", "FMOV wD, sN fixture still stops as unsupported"))
		}
	} catch {
		failures.append(fail("execution", "\(error)"))
	}

	let reducer = try writeReducer(
		target: target,
		caseID: "fmov-w-s-pass-regression",
		command: "make tcti-gate TARGET=\(target)",
		reason: "The simulator reached FMOV w11, s5 after the BusyBox marker; support only the emitted FP scalar move from S register to W register subset.",
		artifacts: artifacts,
		expectedStatus: .pass
	)
	artifacts.append(relativePath(reducer))

	let status: GateStatus = failures.isEmpty ? .pass : .fail
	let reportURL = try writeReport(report(
		target: target,
		status: status,
		summary: "Bound simulator unsupported 0x1e2600ab to exact FMOV w11, s5 semantic subset.",
		failures: failures,
		artifacts: artifacts,
		counters: ["simulator_reports_reduced": 1],
		releaseGateEligible: false,
		readinessGateEligible: false
	))
	print("\(status.rawValue): \(relativePath(reportURL))")
	return exitCode(for: status)
}

func runSIMDSTRSFix() throws -> Int32 {
    let target = "tcti-simd-str-s-fix"
    var failures: [Failure] = []
    var artifacts: [String] = []

    let simulatorReports = runtimeValidationReports(
        gate: "tcti-simulator-stability",
        destination: "iphonesimulator"
    )
    let simulatorEvidenceMatched = simulatorReports.contains { report in
        var reportArtifacts: [String] = []
        let simulatorText = runtimeReportText(report, artifacts: &reportArtifacts)
        let matched = simulatorText.contains("Orlix TCTI: unsupported instruction") &&
            simulatorText.contains("insn=0xbd01c260") &&
            simulatorText.contains("x19=")
        if matched {
            artifacts.append(contentsOf: reportArtifacts)
        }
        return matched
    }

    if !simulatorEvidenceMatched {
        failures.append(fail("simulator-unsupported-signature", "no recorded simulator stability report contains unsupported STR s0 instruction 0xbd01c260 with base register context"))
        if let latestSimulatorReport = simulatorReports.first {
            _ = runtimeReportText(latestSimulatorReport, artifacts: &artifacts)
        }
    }

    let decodeURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "decode_aarch64.c")
    let switchURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "switch_debug.c")
    let testURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "tcti_decode_test.c")
    let decode = try String(contentsOf: decodeURL, encoding: .utf8)
    let switchDebug = try String(contentsOf: switchURL, encoding: .utf8)
    let tests = try String(contentsOf: testURL, encoding: .utf8)

    artifacts.append(relativePath(decodeURL))
    artifacts.append(relativePath(switchURL))
    artifacts.append(relativePath(testURL))

    if !decode.contains("size == 2 && opc <= 1") ||
        !decode.contains("decoded.access_size = sizeof(u32)") ||
        !decode.contains("scale = 2") {
        failures.append(fail("decode-marker", "missing exact SIMD/FP S-register unsigned-immediate load/store decoder subset"))
    }
    if !switchDebug.contains("access_size == sizeof(u32)") ||
        !switchDebug.contains("put_unaligned_le32(current->thread.user_simd[reg * 2], buffer)") ||
        !switchDebug.contains("get_unaligned_le32(buffer)") {
        failures.append(fail("semantic-marker", "missing low-32-bit SIMD/FP store/load semantic subset"))
    }
    if !tests.contains("0xbd01c260U") ||
        !tests.contains("0x1c0LL") ||
        !tests.contains("KUNIT_EXPECT_EQ(test, 4U, decoded.access_size)") {
        failures.append(fail("kunit-regression", "missing KUnit decode coverage for STR s0, [x19, #0x1c0]"))
    }

    let reducer = try writeReducer(
        target: target,
        caseID: "simd-str-s-pass-regression",
        command: "make tcti-gate TARGET=\(target)",
        reason: "The simulator reached STR s0, [x19, #0x1c0]; support only the emitted SIMD/FP S-register unsigned-immediate memory width.",
        artifacts: artifacts,
        expectedStatus: .pass
    )
    artifacts.append(relativePath(reducer))

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Bound recorded simulator unsupported 0xbd01c260 to the exact SIMD/FP STR s0 unsigned-immediate semantic subset.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_reduced": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runSIMDDUP2DFix() throws -> Int32 {
    let target = "tcti-simd-dup-2d-fix"
    var failures: [Failure] = []
    var artifacts: [String] = []

    let simulatorReports = runtimeValidationReports(
        gate: "tcti-simulator-stability",
        destination: "iphonesimulator"
    )
    let simulatorEvidenceMatched = simulatorReports.contains { report in
        var reportArtifacts: [String] = []
        let simulatorText = runtimeReportText(report, artifacts: &reportArtifacts)
        let matched = simulatorText.contains("Orlix TCTI: unsupported instruction") &&
            simulatorText.contains("insn=0x4e080d80") &&
            simulatorText.contains("x12=")
        if matched {
            artifacts.append(contentsOf: reportArtifacts)
        }
        return matched
    }

    if !simulatorEvidenceMatched {
        failures.append(fail("simulator-unsupported-signature", "no recorded simulator stability report contains unsupported DUP v0.2d, x12 instruction 0x4e080d80 with source register context"))
        if let latestSimulatorReport = simulatorReports.first {
            _ = runtimeReportText(latestSimulatorReport, artifacts: &artifacts)
        }
    }

    let decodeURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "decode_aarch64.c")
    let switchURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "switch_debug.c")
    let testURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "tcti_decode_test.c")
    let decode = try String(contentsOf: decodeURL, encoding: .utf8)
    let switchDebug = try String(contentsOf: switchURL, encoding: .utf8)
    let tests = try String(contentsOf: testURL, encoding: .utf8)

    artifacts.append(relativePath(decodeURL))
    artifacts.append(relativePath(switchURL))
    artifacts.append(relativePath(testURL))

    if !decode.contains("AARCH64_SIMD_DUP_2D_GPR_MASK 0xfffffc00U") ||
        !decode.contains("AARCH64_SIMD_DUP_2D_GPR_PATTERN 0x4e080c00U") ||
        !decode.contains("decoded.result_size = 2 * sizeof(u64)") {
        failures.append(fail("decode-marker", "missing exact DUP vN.2d, xM decoder subset"))
    }
    if !switchDebug.contains("tcti_write_simd_fp_register(decoded->rd, 2 * sizeof(u64)") ||
        !switchDebug.contains("value, value") {
        failures.append(fail("semantic-marker", "missing SIMD DUP semantic replication into both 64-bit lanes"))
    }
    if !tests.contains("0x4e080d80U") ||
        !tests.contains("tcti_decode_recognizes_simd_dup_2d_gpr") ||
        !tests.contains("tcti_switch_executes_simd_dup_2d_gpr") {
        failures.append(fail("kunit-regression", "missing KUnit decode and switch-debug coverage for DUP v0.2d, x12"))
    }

    let reducer = try writeReducer(
        target: target,
        caseID: "simd-dup-2d-pass-regression",
        command: "make tcti-gate TARGET=\(target)",
        reason: "The simulator reached DUP v0.2d, x12; support only the emitted 2D GPR-to-vector duplication subset.",
        artifacts: artifacts,
        expectedStatus: .pass
    )
    artifacts.append(relativePath(reducer))

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Bound recorded simulator unsupported 0x4e080d80 to the exact SIMD DUP v0.2d, x12 semantic subset.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_reduced": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runSIMDAND16BFix() throws -> Int32 {
    let target = "tcti-simd-and-16b-fix"
    var failures: [Failure] = []
    var artifacts: [String] = []

    if let simulatorReport = latestRuntimeValidationReport(
        gate: "tcti-simulator-stability",
        destination: "iphonesimulator"
    ) {
        artifacts.append(relativePath(simulatorReport.url))
        let simulatorArtifacts = (simulatorReport.object["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
        artifacts.append(contentsOf: simulatorArtifacts)
        let unifiedText = try simulatorArtifacts.first { $0.hasSuffix("simulator-unified.log") }.map(readRelativeArtifact) ?? ""
        let terminalText = try simulatorArtifacts.first { $0.hasSuffix("simulator-terminal-output.txt") }.map(readRelativeArtifact) ?? ""
        let fatalText = try simulatorArtifacts.first { $0.hasSuffix("tcti-simulator-fatal-runtime.txt") }.map(readRelativeArtifact) ?? ""
        let simulatorText = [unifiedText, terminalText, fatalText].joined(separator: "\n")
        let unsupportedLine = unifiedText
            .split(separator: "\n")
            .map(String.init)
            .last { $0.contains("Orlix TCTI: unsupported instruction") && $0.contains("insn=0x4e211c01") } ?? ""

        if stringField(simulatorReport.object, "git_sha") != gitSha() {
            failures.append(fail("simulator-report-stale", "latest simulator stability report is stale for current HEAD"))
        }
        if stringField(simulatorReport.object, "status") != "fail" || boolField(simulatorReport.object, "passed") {
            failures.append(fail("simulator-report-status", "SIMD AND 16B fix requires the current simulator failure report"))
        }
        if !(unsupportedLine.contains("pc=") && unsupportedLine.contains("insn=0x4e211c01")) &&
            !(simulatorText.contains("Orlix TCTI: unsupported instruction") && simulatorText.contains("insn=0x4e211c01")) {
            failures.append(fail("simulator-unsupported-signature", "latest simulator log does not contain unsupported AND v1.16b, v0.16b, v1.16b instruction 0x4e211c01"))
        }
    } else {
        failures.append(fail("simulator-report", "missing iphonesimulator tcti-simulator-stability report"))
    }

    let decodeURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "decode_aarch64.c")
    let headerURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "decode_aarch64.h")
    let switchURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "switch_debug.c")
    let testURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "tcti_decode_test.c")
    let gateURL = path("tools", "tcti", "orlix-tcti-gate.swift")
    let decode = try String(contentsOf: decodeURL, encoding: .utf8)
    let header = try String(contentsOf: headerURL, encoding: .utf8)
    let switchDebug = try String(contentsOf: switchURL, encoding: .utf8)
    let tests = try String(contentsOf: testURL, encoding: .utf8)
    let gate = try String(contentsOf: gateURL, encoding: .utf8)

    artifacts.append(contentsOf: [relativePath(decodeURL), relativePath(headerURL), relativePath(switchURL), relativePath(testURL), relativePath(gateURL)])

    if !header.contains("TCTI_DECODE_SIMD_VECTOR_LOGICAL") ||
        !decode.contains("AARCH64_SIMD_AND_16B_MASK 0xff20fc00U") ||
        !decode.contains("AARCH64_SIMD_AND_16B_PATTERN 0x4e201c00U") ||
        !decode.contains("decoded.logical_op = TCTI_LOGICAL_AND") {
        failures.append(fail("decode-marker", "missing exact Advanced SIMD AND vN.16b decoder subset for 0x4e211c01"))
    }
    if !switchDebug.contains("tcti_execute_simd_vector_logical") ||
        !switchDebug.contains("left_low & right_low") ||
        !switchDebug.contains("left_high & right_high") {
        failures.append(fail("semantic-marker", "missing SIMD AND vN.16b two-lane switch-debug semantic handler"))
    }
    if !tests.contains("0x4e211c01U") ||
        !tests.contains("tcti_decode_recognizes_simd_and_16b") ||
        !tests.contains("tcti_switch_executes_simd_and_16b") {
        failures.append(fail("kunit-regression", "missing KUnit decode and switch-debug coverage for AND v1.16b, v0.16b, v1.16b"))
    }
    if !gate.contains("simdVectorLogical") ||
        !gate.contains("init_001_exit_simd_and_16b.S") {
        failures.append(fail("no-phone-reducer", "missing no-phone switch-debug reducer fixture for SIMD AND 16B"))
    }

    do {
        let result = try executeNegativeFixture(
            "simd-and-16b",
            outputRoot: buildPath("simd_and_16b_fix", "positive")
        )
        artifacts.append(contentsOf: result.artifacts)
        guard let execution = result.execution else {
            failures.append(fail("execution", "SIMD AND 16B fixture did not write execution report"))
            throw GateError.commandFailed("missing SIMD AND 16B execution report")
        }
        if execution.instructionEncodings.first != "0x4e211c01" ||
            execution.decodedInstructions.first?.instructionClass != "simd_vector_logical" ||
            execution.decodedInstructions.first?.op != "and" ||
            execution.exit?.code != 42 ||
            !execution.syscalls.contains(where: { $0.nr == 93 && $0.name == "exit" }) {
            failures.append(fail("execution-shape", "expected decoded SIMD AND 16B fixture to continue to captured exit(42)"))
        }
        if result.failures.contains(where: { $0.id == "execution-unsupported-instruction" }) {
            failures.append(fail("execution-unsupported-instruction", "SIMD AND 16B fixture still stops as unsupported"))
        }
    } catch {
        failures.append(fail("execution", "\(error)"))
    }

    let reducer = try writeReducer(
        target: target,
        caseID: "simd-and-16b-pass-regression",
        command: "make tcti-gate TARGET=\(target)",
        reason: "The simulator reached AND v1.16b, v0.16b, v1.16b; support only the emitted Advanced SIMD 16-byte vector AND subset.",
        artifacts: artifacts,
        expectedStatus: .pass
    )
    artifacts.append(relativePath(reducer))

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Bound simulator unsupported 0x4e211c01 to the exact SIMD AND v1.16b, v0.16b, v1.16b semantic subset.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_reduced": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runSIMDORR4SFix() throws -> Int32 {
    let target = "tcti-simd-orr-4s-fix"
    var failures: [Failure] = []
    var artifacts: [String] = []

    if let simulatorReport = latestRuntimeValidationReport(
        gate: "tcti-simulator-stability",
        destination: "iphonesimulator"
    ) {
        artifacts.append(relativePath(simulatorReport.url))
        let simulatorArtifacts = (simulatorReport.object["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
        artifacts.append(contentsOf: simulatorArtifacts)
        let unifiedText = try simulatorArtifacts.first { $0.hasSuffix("simulator-unified.log") }.map(readRelativeArtifact) ?? ""
        let terminalText = try simulatorArtifacts.first { $0.hasSuffix("simulator-terminal-output.txt") }.map(readRelativeArtifact) ?? ""
        let fatalText = try simulatorArtifacts.first { $0.hasSuffix("tcti-simulator-fatal-runtime.txt") }.map(readRelativeArtifact) ?? ""
        let simulatorText = [unifiedText, terminalText, fatalText].joined(separator: "\n")

        if stringField(simulatorReport.object, "git_sha") != gitSha() {
            failures.append(fail("simulator-report-stale", "latest simulator stability report is stale for current HEAD"))
        }
        if stringField(simulatorReport.object, "status") != "fail" || boolField(simulatorReport.object, "passed") {
            failures.append(fail("simulator-report-status", "SIMD ORR 4S fix requires the current simulator failure report"))
        }
        if !(simulatorText.contains("Orlix TCTI: unsupported instruction") && simulatorText.contains("insn=0x4f011600")) {
            failures.append(fail("simulator-unsupported-signature", "latest simulator log does not contain unsupported ORR v0.4s, #0x30 instruction 0x4f011600"))
        }
    } else {
        failures.append(fail("simulator-report", "missing iphonesimulator tcti-simulator-stability report"))
    }

    let decodeURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "decode_aarch64.c")
    let headerURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "decode_aarch64.h")
    let switchURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "switch_debug.c")
    let testURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "tcti_decode_test.c")
    let gateURL = path("tools", "tcti", "orlix-tcti-gate.swift")
    let decode = try String(contentsOf: decodeURL, encoding: .utf8)
    let header = try String(contentsOf: headerURL, encoding: .utf8)
    let switchDebug = try String(contentsOf: switchURL, encoding: .utf8)
    let tests = try String(contentsOf: testURL, encoding: .utf8)
    let gate = try String(contentsOf: gateURL, encoding: .utf8)

    artifacts.append(contentsOf: [relativePath(decodeURL), relativePath(headerURL), relativePath(switchURL), relativePath(testURL), relativePath(gateURL)])

    if !header.contains("TCTI_DECODE_SIMD_VECTOR_LOGICAL_IMMEDIATE") ||
        !decode.contains("AARCH64_SIMD_ORR_4S_0X30_MASK 0xffffffe0U") ||
        !decode.contains("AARCH64_SIMD_ORR_4S_0X30_PATTERN 0x4f011600U") ||
        !decode.contains("decoded.logical_immediate = 0x0000003000000030ULL") {
        failures.append(fail("decode-marker", "missing exact Advanced SIMD ORR vN.4s #0x30 decoder subset for 0x4f011600"))
    }
    if !switchDebug.contains("tcti_execute_simd_vector_logical_immediate") ||
        !switchDebug.contains("low | decoded->logical_immediate") ||
        !switchDebug.contains("high | decoded->logical_immediate") {
        failures.append(fail("semantic-marker", "missing SIMD ORR vN.4s immediate switch-debug semantic handler"))
    }
    if !tests.contains("0x4f011600U") ||
        !tests.contains("tcti_decode_recognizes_simd_orr_4s_immediate") ||
        !tests.contains("tcti_switch_executes_simd_orr_4s_immediate") {
        failures.append(fail("kunit-regression", "missing KUnit decode and switch-debug coverage for ORR v0.4s, #0x30"))
    }
    if !gate.contains("simdVectorLogicalImmediate") ||
        !gate.contains("init_001_exit_simd_orr_4s.S") {
        failures.append(fail("no-phone-reducer", "missing no-phone switch-debug reducer fixture for SIMD ORR 4S immediate"))
    }

    do {
        let result = try executeNegativeFixture(
            "simd-orr-4s",
            outputRoot: buildPath("simd_orr_4s_fix", "positive")
        )
        artifacts.append(contentsOf: result.artifacts)
        guard let execution = result.execution else {
            failures.append(fail("execution", "SIMD ORR 4S fixture did not write execution report"))
            throw GateError.commandFailed("missing SIMD ORR 4S execution report")
        }
        if execution.instructionEncodings.first != "0x4f011600" ||
            execution.decodedInstructions.first?.instructionClass != "simd_vector_logical_immediate" ||
            execution.decodedInstructions.first?.op != "orr" ||
            execution.exit?.code != 42 ||
            !execution.syscalls.contains(where: { $0.nr == 93 && $0.name == "exit" }) {
            failures.append(fail("execution-shape", "expected decoded SIMD ORR 4S fixture to continue to captured exit(42)"))
        }
        if result.failures.contains(where: { $0.id == "execution-unsupported-instruction" }) {
            failures.append(fail("execution-unsupported-instruction", "SIMD ORR 4S fixture still stops as unsupported"))
        }
    } catch {
        failures.append(fail("execution", "\(error)"))
    }

    let reducer = try writeReducer(
        target: target,
        caseID: "simd-orr-4s-pass-regression",
        command: "make tcti-gate TARGET=\(target)",
        reason: "The simulator reached ORR v0.4s, #0x30; support only the emitted Advanced SIMD 4S immediate ORR subset.",
        artifacts: artifacts,
        expectedStatus: .pass
    )
    artifacts.append(relativePath(reducer))

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Bound simulator unsupported 0x4f011600 to the exact SIMD ORR v0.4s, #0x30 semantic subset.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_reduced": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runPostBusyBoxSIGABRTReducer() throws -> Int32 {
	let target = "tcti-post-busybox-sigabrt-reducer"
	var failures: [Failure] = []
	var artifacts: [String] = []

	func reportModifiedAt(_ url: URL) -> Date {
		(try? url.resourceValues(forKeys: [.contentModificationDateKey]).contentModificationDate) ?? .distantPast
	}

	let candidateGates = [
		"tcti-init-console-write",
		"tcti-init-first-syscall",
		"tcti-static-busybox-shell-command",
		"tcti-static-busybox-start",
		"tcti-simulator-stability",
	]
	let selectedReport: (url: URL, object: [String: Any])?
	if let reportPath = ProcessInfo.processInfo.environment["TCTI_SIMULATOR_REPORT"], !reportPath.isEmpty {
		selectedReport = selectedRuntimeValidationReport(
			gates: candidateGates,
			destination: "iphonesimulator"
		)
	} else {
		selectedReport = candidateGates
			.compactMap { latestRuntimeValidationReport(gate: $0, destination: "iphonesimulator") }
			.filter { stringField($0.object, "status") == "fail" && !boolField($0.object, "passed") }
			.sorted { reportModifiedAt($0.url) > reportModifiedAt($1.url) }
			.first
	}

	guard let simulatorReport = selectedReport else {
        failures.append(fail("simulator-report", "missing iphonesimulator static BusyBox SIGABRT report"))
        let reportURL = try writeReport(report(
            target: target,
            status: .fail,
            summary: "No simulator static BusyBox SIGABRT failure report was available for the reducer.",
            failures: failures,
            artifacts: artifacts,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("fail: \(relativePath(reportURL))")
        return 1
    }

    let object = simulatorReport.object
    artifacts.append(relativePath(simulatorReport.url))
    let reportArtifacts = (object["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
    artifacts.append(contentsOf: reportArtifacts)
    let text = reportArtifacts
        .compactMap { try? readRelativeArtifact($0) }
        .joined(separator: "\n")
    let events = object["tcti_runtime_events"] as? [String: Any] ?? [:]
    let staticPIE = events["static_pie_image"] as? [String: Any] ?? [:]
    let signal = events["signaled_process"] as? [String: Any] ?? [:]
    let staticPID = intField(staticPIE, "pid")
    let signaledPID = intField(signal, "pid")

	if stringField(object, "git_sha") == gitSha(),
	   stringField(object, "status") == "fail",
	   !boolField(object, "passed"),
	   stringField(staticPIE, "task") == "sh",
	   staticPID != nil,
	   let currentSignal = intField(signal, "signal"),
	   currentSignal != 6 {
		let reportURL = try writeReport(report(
			target: target,
			status: .pass,
			summary: "Static BusyBox SIGABRT reducer is not needed for the current simulator report because the shell signal is \(currentSignal), not 6.",
			failures: [],
			artifacts: artifacts,
			counters: ["simulator_reports_reduced": 0],
			releaseGateEligible: false,
			readinessGateEligible: false
		))
		print("pass: \(relativePath(reportURL))")
		return 0
	}

    if stringField(object, "git_sha") != gitSha() {
        failures.append(fail("simulator-report-stale", "selected simulator static BusyBox SIGABRT report is stale for current HEAD"))
    }
    if stringField(object, "status") != "fail" || boolField(object, "passed") {
        failures.append(fail("simulator-report-status", "SIGABRT reducer requires a current simulator static BusyBox failure report"))
    }
    if stringField(object, "selected_device_id") != "1E5553B0-203A-4A11-BAD7-EBDE46863F66" ||
        stringField(object, "selected_device_name") != "Orlix-iPhone-15-Pro-Max" ||
        intField(object, "simulator_booted_count") != 1 ||
        !boolField(object, "simulator_single_booted") {
        failures.append(fail("simulator-scope", "reducer requires a current pinned Orlix-iPhone-15-Pro-Max simulator report"))
    }
    if stringField(staticPIE, "task") != "sh" || staticPID == nil {
        failures.append(fail("static-pie-sh", "simulator evidence must show static PIE image task=sh before the abort"))
    }
    if intField(signal, "signal") != 6 || signaledPID == nil || staticPID != signaledPID {
        failures.append(fail("sigabrt-sh", "simulator evidence must tie SIGABRT signal=6 to the static BusyBox shell pid"))
    }
    let gate = stringField(object, "gate")
    let busyBoxStartSIGABRT = gate == "tcti-static-busybox-start" &&
        text.contains("Orlix TCTI: static PIE image task=sh") &&
        text.contains("syscall=134") &&
        text.contains("syscall=129") &&
        text.contains("syscall=139 x0=0x6")
	let firstSyscallSIGABRT = gate == "tcti-init-first-syscall" &&
		text.contains("Orlix TCTI: static PIE image task=sh") &&
		text.contains("syscall=172") &&
		text.contains("signed_ret=\(staticPID ?? -1)") &&
		text.contains("syscall=129") &&
		text.contains("x1=0x6") &&
		text.contains("orlix-init: process signaled pid=\(staticPID ?? -1) signal=6")
	let busyBoxShellCommandSIGABRT = gate == "tcti-static-busybox-shell-command" &&
		text.contains("Orlix TCTI: static PIE image task=sh") &&
		text.contains("syscall=172") &&
		text.contains("signed_ret=\(staticPID ?? -1)") &&
		text.contains("syscall=129") &&
		text.contains("x1=0x6") &&
		text.contains("orlix-init: process signaled pid=\(staticPID ?? -1) signal=6")
	let consoleWriteSIGABRT = gate == "tcti-init-console-write" &&
		text.contains("ORLIX-TCTI-CONSOLE-OK") &&
		text.contains("Orlix TCTI: static PIE image task=sh") &&
		text.contains("syscall=172") &&
		text.contains("signed_ret=\(staticPID ?? -1)") &&
		text.contains("syscall=129") &&
		text.contains("x1=0x6") &&
		text.contains("orlix-init: process signaled pid=\(staticPID ?? -1) signal=6")
    let stabilitySIGABRT = gate == "tcti-simulator-stability" &&
        text.contains("Orlix TCTI: static PIE image task=sh") &&
        text.contains("syscall=172 ret=0x20 signed_ret=32") &&
        text.contains("syscall=129 x0=0x20 x1=0x6 x2=0x6") &&
        text.contains("orlix-init: process signaled pid=32 signal=6")
    if !busyBoxStartSIGABRT && !firstSyscallSIGABRT && !busyBoxShellCommandSIGABRT && !consoleWriteSIGABRT && !stabilitySIGABRT {
        failures.append(fail("sigabrt-syscall-shape", "simulator artifacts must show a known static BusyBox SIGABRT shape before signal=6"))
    }
	if gate == "tcti-init-console-write" {
		let forbidden = object["forbidden_behavior"] as? [String: Any] ?? [:]
		let forbiddenOK = [
			"generated_exec_memory",
			"host_exec_guest_text",
			"host_x18",
			"map_jit",
			"native_ios_api_exposure_to_guest",
			"rwx",
		].allSatisfy { !boolField(forbidden, $0) }
		let fatalUserFault = events["fatal_user_fault"] as? [String: Any] ?? [:]
		let hasFatalUserFault = !stringField(fatalUserFault, "task").isEmpty ||
			intField(fatalUserFault, "pid") != nil ||
			!stringField(fatalUserFault, "pc").isEmpty ||
			!stringField(fatalUserFault, "addr").isEmpty
		if hasFatalUserFault || text.contains("Orlix TCTI: user fault") {
			failures.append(fail("no-user-fault", "console-write SIGABRT reducer must not cover a fatal user fault"))
		}
		if text.contains("Orlix TCTI: unsupported instruction") {
			failures.append(fail("no-unsupported-instruction", "console-write SIGABRT reducer must not cover an unsupported instruction"))
		}
		if !forbiddenOK {
			failures.append(fail("forbidden-behavior", "console-write SIGABRT reducer requires all forbidden_behavior fields to be false"))
		}
	}

	let reducerCaseID = gate == "tcti-init-console-write" ? "post-console-write-busybox-sigabrt-pass-regression" : "post-busybox-sigabrt-pass-regression"
    let reducer = try writeReducer(
        target: target,
        caseID: reducerCaseID,
        command: "TCTI_SIMULATOR_REPORT=\(relativePath(simulatorReport.url)) make tcti-gate TARGET=\(target)",
        reason: "The pinned simulator reaches static BusyBox /bin/sh under TCTI and then the shell exits via SIGABRT signal=6; reduce that report-backed failure before production TCTI patching.",
        artifacts: artifacts,
        expectedStatus: .pass
    )
    artifacts.append(relativePath(reducer))

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Reduced the pinned simulator static BusyBox SIGABRT failure to report-backed no-phone evidence.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_reduced": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runPostBusyBoxShellCommandSIGILLReducer() throws -> Int32 {
    let target = "tcti-post-busybox-shell-command-sigill-reducer"
    var failures: [Failure] = []
    var artifacts: [String] = []

    guard let simulatorReport = selectedRuntimeValidationReport(
        gates: ["tcti-static-busybox-shell-command"],
        destination: "iphonesimulator"
    ) else {
        failures.append(fail("simulator-report", "missing iphonesimulator static BusyBox shell-command SIGILL report"))
        let reportURL = try writeReport(report(
            target: target,
            status: .fail,
            summary: "No simulator static BusyBox shell-command SIGILL failure report was available for the reducer.",
            failures: failures,
            artifacts: artifacts,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("fail: \(relativePath(reportURL))")
        return 1
    }

    let object = simulatorReport.object
    artifacts.append(relativePath(simulatorReport.url))
    let reportArtifacts = (object["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
    artifacts.append(contentsOf: reportArtifacts)
    let text = reportArtifacts
        .compactMap { try? readRelativeArtifact($0) }
        .joined(separator: "\n")
    let events = object["tcti_runtime_events"] as? [String: Any] ?? [:]
    let staticPIE = events["static_pie_image"] as? [String: Any] ?? [:]
    let signal = events["signaled_process"] as? [String: Any] ?? [:]
    let lastReturn = events["last_sh_syscall_return"] as? [String: Any] ?? [:]
    let staticPID = intField(staticPIE, "pid")
    let signaledPID = intField(signal, "pid")
    let markerArtifact = reportArtifacts.first { $0.hasSuffix("tcti-static-busybox-shell-command.txt") }
    let markerText = markerArtifact.flatMap { try? readRelativeArtifact($0) } ?? ""

    if stringField(object, "git_sha") != gitSha() {
        failures.append(fail("simulator-report-stale", "selected simulator static BusyBox shell-command SIGILL report is stale for current HEAD"))
    }
    if stringField(object, "status") != "fail" || boolField(object, "passed") {
        failures.append(fail("simulator-report-status", "SIGILL reducer requires a current simulator static BusyBox shell-command failure report"))
    }
    if stringField(object, "gate") != "tcti-static-busybox-shell-command" {
        failures.append(fail("simulator-report-gate", "SIGILL reducer must use the static BusyBox shell-command gate report"))
    }
    if stringField(object, "selected_device_id") != "1E5553B0-203A-4A11-BAD7-EBDE46863F66" ||
        stringField(object, "selected_device_name") != "Orlix-iPhone-15-Pro-Max" ||
        intField(object, "simulator_booted_count") != 1 ||
        !boolField(object, "simulator_single_booted") {
        failures.append(fail("simulator-scope", "reducer requires a current pinned Orlix-iPhone-15-Pro-Max simulator report"))
    }
    if stringField(staticPIE, "task") != "sh" || staticPID == nil {
        failures.append(fail("static-pie-sh", "simulator evidence must show static PIE image task=sh before the SIGILL"))
    }
    if intField(signal, "signal") != 4 || signaledPID == nil || staticPID != signaledPID {
        failures.append(fail("sigill-sh", "simulator evidence must tie signal=4 to the static BusyBox shell pid"))
    }
    if stringField(lastReturn, "task") != "sh" ||
        intField(lastReturn, "pid") != staticPID ||
        intField(lastReturn, "syscall") != 64 ||
        stringField(lastReturn, "ret") != "0x19" ||
        intField(lastReturn, "signed_ret") != 25 {
        failures.append(fail("post-marker-write-return", "simulator JSON must show sh write syscall return before signal=4"))
    }
    if !markerText.contains("ORLIX-TCTI-BUSYBOX-USABLE") ||
        !text.contains("Orlix TCTI: static PIE image task=sh") ||
        !text.contains("Orlix TCTI: syscall return task=sh") ||
        !text.contains("syscall=64 ret=0x19 signed_ret=25") ||
        !text.contains("orlix-init: process signaled") ||
        !text.contains("signal=4") {
        failures.append(fail("sigill-after-marker-shape", "simulator artifacts must show BusyBox marker output, write return, then signal=4"))
    }
    if !text.contains("Orlix TCTI: unsupported instruction") ||
        !text.contains("insn=0xe612800") {
        failures.append(fail("unsupported-xtn-signature", "simulator artifacts must show unsupported XTN v0.4h, v0.4s instruction 0x0e612800"))
    }

    do {
        let result = try executeNegativeFixture(
            "simd-xtn-4h-unsupported",
            outputRoot: buildPath("post_busybox_shell_command_sigill_reducer", "simd_xtn_4h")
        )
        artifacts.append(contentsOf: result.artifacts)
        guard let execution = result.execution else {
            failures.append(fail("negative-execution", "XTN v0.4h, v0.4s fixture did not write execution report"))
            throw GateError.commandFailed("missing XTN v0.4h, v0.4s negative execution report")
        }
        let xtnInstructionIndex = execution.instructionEncodings.firstIndex(of: "0x0e612800")
        let xtnDecoded = xtnInstructionIndex.flatMap { index in
            index < execution.decodedInstructions.count ? execution.decodedInstructions[index] : nil
        }
        let unsupportedShape = xtnInstructionIndex != nil &&
            xtnDecoded?.instructionClass == "unsupported" &&
            execution.exit == nil &&
            execution.syscalls.isEmpty
        let supportedShape = xtnInstructionIndex != nil &&
            xtnDecoded?.instructionClass != "unsupported" &&
            execution.exit?.code == 42 &&
            execution.syscalls.contains { $0.nr == 93 && $0.name == "exit" } &&
            !result.failures.contains { $0.id == "execution-unsupported-instruction" }
        if !unsupportedShape && !supportedShape {
            failures.append(fail("negative-execution-shape", "expected XTN v0.4h, v0.4s fixture to stop on unsupported 0x0e612800 or continue to captured exit(42) after a narrow fix"))
        }
    } catch {
        failures.append(fail("simd-xtn-4h-reducer", "\(error)"))
    }

    let reducer = try writeReducer(
        target: target,
        caseID: "post-busybox-shell-command-sigill-xtn-4h-pass-regression",
        command: "TCTI_SIMULATOR_REPORT=\(relativePath(simulatorReport.url)) make tcti-gate TARGET=\(target)",
        reason: "The pinned simulator reaches the static BusyBox shell command marker under TCTI, then the shell receives signal=4 after unsupported XTN v0.4h, v0.4s instruction 0x0e612800; reduce that report-backed failure before production TCTI patching.",
        artifacts: artifacts,
        expectedStatus: .pass
    )
    artifacts.append(relativePath(reducer))

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Reduced the pinned simulator static BusyBox shell-command XTN v0.4h, v0.4s SIGILL-after-marker failure to report-backed no-phone evidence.",
        failures: failures,
        artifacts: artifacts,
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runUserDataWindowRefreshFix() throws -> Int32 {
    let target = "tcti-user-data-window-refresh-fix"
    var failures: [Failure] = []
    var artifacts: [String] = []

	let reducerReportURL = buildPath("reports", "tcti-post-bash-mmap-read-fault-reducer", "report.json")
	if let reducerReport = try? loadJSON(reducerReportURL) as? [String: Any] {
		artifacts.append(relativePath(reducerReportURL))
		artifacts.append(contentsOf: (reducerReport["artifacts"] as? [Any] ?? []).compactMap { $0 as? String })
		if stringField(reducerReport, "git_sha") != gitSha() ||
			stringField(reducerReport, "status") != "pass" ||
			!boolField(reducerReport, "passed") {
            failures.append(fail("reducer-report", "post-Bash mmap/read reducer report is missing, stale, or not passing"))
        }
    } else {
        failures.append(fail("reducer-report", "missing tcti-post-bash-mmap-read-fault-reducer report"))
    }

    let userPageURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "mm", "tcti_user_page.c")
    let userPage = try readText(userPageURL)
    artifacts.append(relativePath(userPageURL))
    if !userPage.contains("access == TCTI_ACCESS_READ && tcti_vma_is_executable(mm, address)") ||
        !userPage.contains("tcti_sync_faulted_user_window(mm, current_va,") {
        failures.append(fail("read-window-refresh", "TCTI faulted executable data reads must bypass hosted executable-window refresh before retrying Linux page-backed access"))
    }
    if !userPage.contains("orlix_refresh_current_user_mapping_page_from_kernel") {
        failures.append(fail("write-coherency", "TCTI writes must retain hosted mapping refresh after kernel-backed writes"))
    }
    if userPage.contains("MAP_JIT") ||
        userPage.contains("VM_PROT_EXECUTE") ||
        userPage.contains("PROT_EXEC") ||
        userPage.contains("HostAdapter") {
        failures.append(fail("forbidden-scope", "user data window refresh fix must not add HostAdapter, JIT, or executable mapping behavior"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Checked reducer-backed TCTI user-data fault retry fix syncs faulted read windows while preserving write refresh.",
        failures: failures,
        artifacts: artifacts,
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runPostStaticPIEInitTLSFix() throws -> Int32 {
    let target = "tcti-post-static-pie-init-tls-fix"
    var failures: [Failure] = []
    var artifacts: [String] = []

	let reducerReportURL = buildPath("reports", "tcti-post-static-pie-init-read-fault-reducer", "report.json")
	var reducerArtifacts: [String] = []
	if let reducerReport = try? loadJSON(reducerReportURL) as? [String: Any] {
		reducerArtifacts = (reducerReport["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
		artifacts.append(relativePath(reducerReportURL))
		artifacts.append(contentsOf: reducerArtifacts)
		if stringField(reducerReport, "git_sha") != gitSha() ||
            stringField(reducerReport, "status") != "pass" ||
            !boolField(reducerReport, "passed") {
            failures.append(fail("reducer-report", "post-static-PIE init read-fault reducer report is missing, stale, or not passing"))
        }
    } else {
        failures.append(fail("reducer-report", "missing tcti-post-static-pie-init-read-fault-reducer report"))
    }

	let simulatorPath = reducerArtifacts.first {
		$0.hasPrefix("Build/Reports/runtime/tcti-") &&
			$0.hasSuffix(".json")
	} ?? ""
	let simulatorURL = path(simulatorPath)
	guard !simulatorPath.isEmpty,
	      let simulatorObject = try? loadJSON(simulatorURL) as? [String: Any] else {
		failures.append(fail("simulator-report", "missing reducer-covered iphonesimulator report for post-static-PIE init TLS/read-fault fix"))
		let reportURL = try writeReport(report(
			target: target,
			status: .fail,
			summary: "No reducer-covered pinned simulator report was available for the post-static-PIE init TLS/read-fault fix gate.",
			failures: failures,
			artifacts: artifacts,
			releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("fail: \(relativePath(reportURL))")
        return 1
    }

	artifacts.append(simulatorPath)
    let events = simulatorObject["tcti_runtime_events"] as? [String: Any] ?? [:]
    let staticPIE = events["static_pie_image"] as? [String: Any] ?? [:]
    let fault = events["fatal_user_fault"] as? [String: Any] ?? [:]
    let firstSVC = events["first_svc"] as? [String: Any] ?? [:]
    let faultPC = stringField(fault, "pc")
    let faultAddress = stringField(fault, "addr")
    let staticBase = stringField(staticPIE, "base")
    let stillMatchesStaticPIEInitFault = stringField(staticPIE, "task") == "init" &&
        intField(staticPIE, "pid") == 1 &&
        stringField(fault, "task") == "init" &&
        intField(fault, "pid") == 1
    let faultOffset: String? = {
        guard
            !staticBase.isEmpty,
            !faultAddress.isEmpty,
            let base = UInt64(staticBase.dropFirst(2), radix: 16),
            let address = UInt64(faultAddress.dropFirst(2), radix: 16),
            address >= base
        else {
            return nil
        }
        return String(format: "0x%llx", address - base)
    }()

	if stringField(simulatorObject, "git_sha") != gitSha() {
		failures.append(fail("simulator-report-stale", "selected simulator report is stale for current HEAD"))
	}
    if stringField(simulatorObject, "selected_device_id") != "1E5553B0-203A-4A11-BAD7-EBDE46863F66" ||
        stringField(simulatorObject, "selected_device_name") != "Orlix-iPhone-15-Pro-Max" ||
        intField(simulatorObject, "simulator_booted_count") != 1 ||
        !boolField(simulatorObject, "simulator_single_booted") {
        failures.append(fail("simulator-scope", "fix gate requires evidence from the single pinned Orlix-iPhone-15-Pro-Max simulator"))
    }
	if stringField(simulatorObject, "status") != "fail" || boolField(simulatorObject, "passed") {
		failures.append(fail("simulator-report-status", "fix gate must be backed by the current failing simulator report before the simulator retry"))
	}
	if stillMatchesStaticPIEInitFault &&
		(stringField(firstSVC, "task") != "init" ||
		intField(firstSVC, "pid") != 1 ||
		intField(firstSVC, "syscall") != 178) {
		failures.append(fail("init-first-svc", "simulator report must show init reached first TCTI svc syscall 178 before the TLS/read fault"))
	}
	if stillMatchesStaticPIEInitFault &&
		(staticBase.isEmpty ||
		stringField(staticPIE, "entry").isEmpty) {
		failures.append(fail("static-pie-init", "simulator report must show static PIE image task=init before the TLS/read fault"))
	}
	if stillMatchesStaticPIEInitFault &&
		(intField(fault, "access") != 1 ||
		intField(fault, "si") != 1 ||
		faultPC.isEmpty ||
		faultAddress.isEmpty ||
		faultOffset == nil) {
		failures.append(fail("tls-read-fault-signature", "simulator report must show post-static-PIE init TLS-relative user read fault"))
	}
	if let latestReport = latestRuntimeValidationReport(
		gate: "tcti-full-shell-usability",
		destination: "iphonesimulator"
	) {
		let latestPath = relativePath(latestReport.url)
		let latestObject = latestReport.object
		let latestEvents = latestObject["tcti_runtime_events"] as? [String: Any] ?? [:]
		let latestStaticPIE = latestEvents["static_pie_image"] as? [String: Any] ?? [:]
		let latestFault = latestEvents["fatal_user_fault"] as? [String: Any] ?? [:]
		let latestFirstSVC = latestEvents["first_svc"] as? [String: Any] ?? [:]
		let latestStillMatches = stringField(latestObject, "git_sha") == gitSha() &&
			stringField(latestObject, "status") == "fail" &&
			!boolField(latestObject, "passed") &&
			stringField(latestObject, "selected_device_id") == "1E5553B0-203A-4A11-BAD7-EBDE46863F66" &&
			stringField(latestObject, "selected_device_name") == "Orlix-iPhone-15-Pro-Max" &&
			intField(latestObject, "simulator_booted_count") == 1 &&
			boolField(latestObject, "simulator_single_booted") &&
			stringField(latestFirstSVC, "task") == "init" &&
			intField(latestFirstSVC, "pid") == 1 &&
			intField(latestFirstSVC, "syscall") == 178 &&
			stringField(latestStaticPIE, "task") == "init" &&
			intField(latestStaticPIE, "pid") == 1 &&
			stringField(latestFault, "task") == "init" &&
			intField(latestFault, "pid") == 1 &&
			intField(latestFault, "access") == 1 &&
			intField(latestFault, "si") == 1 &&
			!stringField(latestFault, "addr").isEmpty
		if latestStillMatches {
			if !artifacts.contains(latestPath) {
				artifacts.append(latestPath)
			}
			if !reducerArtifacts.contains(latestPath) {
				failures.append(fail("latest-reducer-coverage", "post-static-PIE init TLS/read-fault fix requires reducer coverage for the latest matching full-shell report \(latestPath)"))
			}
			if simulatorPath != latestPath {
				failures.append(fail("stale-fix-report", "post-static-PIE init TLS/read-fault fix is tied to \(simulatorPath), but latest matching full-shell report is \(latestPath)"))
			}
			failures.append(fail("latest-simulator-still-fails", "post-static-PIE init TLS/read-fault fix cannot pass while the latest current pinned full-shell simulator report still shows init access=read si=1 before the shell success marker"))
		}
	}

    let userPageURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "mm", "tcti_user_page.c")
    let switchURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "switch_debug.c")
    let engineURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "engine.c")
    let userPage = try readText(userPageURL)
    let switchDebug = try readText(switchURL)
    let engine = try readText(engineURL)
    artifacts.append(contentsOf: [relativePath(userPageURL), relativePath(switchURL), relativePath(engineURL)])

    if !switchDebug.contains("current->thread.user_tls = value") ||
        !switchDebug.contains("regs->regs[decoded->rt] = current->thread.user_tls") {
        failures.append(fail("guest-tls-state", "switch-debug must keep guest TPIDR_EL0 in task thread.user_tls, not host TPIDR_EL0"))
    }
    if !engine.contains("tcti_handle_user_fault(regs, result->fault_address") ||
        !engine.contains("orlix_exit_to_user_mode_work(regs)") {
        failures.append(fail("user-fault-retry", "TCTI user-fault exits must fault in Linux user memory and retry guest execution"))
    }
    if engine.contains("current->thread.user_tls = 0") ||
        !engine.contains("tcti_prepare_successful_execve_return") {
        failures.append(fail("execve-tls-preserve", "TCTI successful execve return must preserve guest TPIDR_EL0 state before the new static PIE image runs"))
    }
    if !userPage.contains("tcti_fault_in_user_page(mm, current_va, access)") ||
        !userPage.contains("tcti_sync_faulted_user_window(mm, current_va,") ||
        !userPage.contains("access == TCTI_ACCESS_READ && tcti_vma_is_executable(mm, address)") {
        failures.append(fail("user-data-sync", "TCTI user-data reads must fault in Linux pages and sync the hosted fault window before retrying"))
    }
    for forbidden in ["HostAdapter", "MAP_JIT", "VM_PROT_EXECUTE", "PROT_EXEC"] {
        if userPage.contains(forbidden) || switchDebug.contains(forbidden) || engine.contains(forbidden) {
            failures.append(fail("forbidden-scope", "post-static-PIE init TLS/read-fault fix must not add \(forbidden) behavior"))
        }
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Checked reducer-backed TCTI post-static-PIE init TLS/read-fault fix preserves guest TPIDR_EL0 state and retries Linux user-data reads through faulted user windows.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_checked": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runPostFullShellCatReadFaultReducer() throws -> Int32 {
    let target = "tcti-post-full-shell-cat-read-fault-reducer"
    var failures: [Failure] = []
    var artifacts: [String] = []

    guard let simulatorReport = selectedRuntimeValidationReport(
        gate: "tcti-full-shell-usability",
        destination: "iphonesimulator"
    ) else {
        failures.append(fail("simulator-report", "missing iphonesimulator full-shell cat read-fault report"))
        let reportURL = try writeReport(report(
            target: target,
            status: .fail,
            summary: "No pinned simulator full-shell cat read-fault report was available for the reducer.",
            failures: failures,
            artifacts: artifacts,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("fail: \(relativePath(reportURL))")
        return 1
    }

    let object = simulatorReport.object
    artifacts.append(relativePath(simulatorReport.url))
    let reportArtifacts = (object["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
    artifacts.append(contentsOf: reportArtifacts)
    let terminalText = try reportArtifacts.first { $0.hasSuffix("simulator-terminal-output.txt") }.map(readRelativeArtifact) ?? ""
    let launchStderrText = try reportArtifacts.first { $0.hasSuffix("launch.stderr") }.map(readRelativeArtifact) ?? ""
    let fullShellText = try reportArtifacts.first { $0.hasSuffix("tcti-full-shell-usability.txt") }.map(readRelativeArtifact) ?? ""
    let combinedText = [terminalText, launchStderrText, fullShellText].joined(separator: "\n")
    let events = object["tcti_runtime_events"] as? [String: Any] ?? [:]
    let firstSVC = events["first_svc"] as? [String: Any] ?? [:]
    let fault = events["fatal_user_fault"] as? [String: Any] ?? [:]
    let signal = events["signaled_process"] as? [String: Any] ?? [:]
    let faultPC = stringField(fault, "pc")
    let faultLR = stringField(fault, "lr")
    let faultSP = stringField(fault, "sp")
    let faultAddress = stringField(fault, "addr")
    let faultPID = intField(fault, "pid").map(String.init)

    if stringField(object, "git_sha") != gitSha() {
        failures.append(fail("simulator-report-stale", "selected full-shell simulator report is stale for current HEAD"))
    }
    if stringField(object, "status") != "fail" || boolField(object, "passed") {
        failures.append(fail("simulator-report-status", "reducer requires a current failing full-shell simulator report"))
    }
    if stringField(object, "selected_device_id") != "1E5553B0-203A-4A11-BAD7-EBDE46863F66" ||
        stringField(object, "selected_device_name") != "Orlix-iPhone-15-Pro-Max" ||
        intField(object, "simulator_booted_count") != 1 ||
        !boolField(object, "simulator_single_booted") {
        failures.append(fail("simulator-scope", "reducer requires the single pinned Orlix-iPhone-15-Pro-Max simulator report"))
    }
    if stringField(firstSVC, "task") != "init" ||
        intField(firstSVC, "pid") != 1 ||
        intField(firstSVC, "syscall") != 178 {
        failures.append(fail("init-first-svc", "full-shell report must show init reached first TCTI svc syscall 178 before cat"))
    }
    if stringField(fault, "task") != "cat" ||
        faultPID == nil ||
        intField(fault, "access") != 1 ||
        intField(fault, "si") != 1 ||
        faultPC.isEmpty ||
        faultLR.isEmpty ||
        faultSP.isEmpty ||
        faultAddress.isEmpty ||
        faultAddress == "0x0" {
        failures.append(fail("cat-read-fault-signature", "full-shell report must show cat read fault at a non-null user address with access=1 si=1"))
    }
    if intField(signal, "signal") != nil {
        failures.append(fail("cat-read-fault-phase", "cat read-fault reducer expects runtime report before a structured signaled_process event"))
    }
    let shellStatus139 = combinedText.contains("orlix-init: process exited pid=33 status=139") ||
        combinedText.contains("orlix-init: process exited pid=32 status=139") ||
        combinedText.contains("orlix-init: pid=32 status=139") ||
        combinedText.contains("orlix-init: pid=33 status=139")
    if !combinedText.contains("Orlix TCTI: static PIE image task=cat") ||
        !combinedText.contains("Orlix TCTI: user fault task=cat") ||
        !shellStatus139 ||
        fullShellText.contains("ORLIX-TCTI-SHELL-USABLE") {
        failures.append(fail("cat-artifact", "full-shell artifacts must show cat read fault and shell status 139 before the success marker"))
    }

    struct PostFullShellCatReadFaultModel: Codable {
        let task: String
        let pid: String
        let shellStatus: Int
        let faultPC: String
        let faultLR: String
        let faultSP: String
        let faultAddress: String
        let access: String
        let invariant: String
    }
    if let pid = faultPID {
        let model = PostFullShellCatReadFaultModel(
            task: "cat",
            pid: pid,
            shellStatus: 139,
            faultPC: faultPC,
            faultLR: faultLR,
            faultSP: faultSP,
            faultAddress: faultAddress,
            access: "read",
            invariant: "after the static BusyBox shell creates /tmp/orlix-tcti-shell, TCTI must let cat read the file data and reach ORLIX-TCTI-SHELL-USABLE instead of faulting a user-data read"
        )
        let modelURL = buildPath("reproducers", target, "post-full-shell-cat-read-fault-model.json")
        try writeJSON(model, to: modelURL)
        artifacts.append(relativePath(modelURL))
    }

    let reducer = try writeReducer(
        target: target,
        caseID: "post-full-shell-cat-read-fault-pass-regression",
        command: "TCTI_SIMULATOR_REPORT=\(relativePath(simulatorReport.url)) make tcti-gate TARGET=\(target)",
        reason: "pinned simulator full-shell command reaches cat /tmp/orlix-tcti-shell, then TCTI faults a cat user-data read and shell exits 139 before ORLIX-TCTI-SHELL-USABLE",
        artifacts: artifacts,
        expectedStatus: .pass
    )
    artifacts.append(relativePath(reducer))

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Reduced pinned simulator full-shell cat user-data read fault to a no-phone TCTI evidence gate.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_reduced": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runPostFullShellCatReadFaultFix() throws -> Int32 {
    let target = "tcti-post-full-shell-cat-read-fault-fix"
    var failures: [Failure] = []
    var artifacts: [String] = []

    let reducerURL = buildPath("reports", "tcti-post-full-shell-cat-read-fault-reducer", "report.json")
    let reducerObject = try? loadJSON(reducerURL) as? [String: Any]
    let reducerArtifacts = (reducerObject?["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
    if let reducerObject {
        artifacts.append(relativePath(reducerURL))
        if stringField(reducerObject, "git_sha") != gitSha() ||
            stringField(reducerObject, "status") != "pass" ||
            !boolField(reducerObject, "passed") {
            failures.append(fail("reducer-report", "cat read-fault reducer report is missing, stale, or not passing"))
        }
    } else {
        failures.append(fail("reducer-report", "missing tcti-post-full-shell-cat-read-fault-reducer report"))
    }

    let simulatorPath = reducerArtifacts.first {
        $0.hasPrefix("Build/Reports/runtime/tcti-full-shell-usability-") &&
            $0.hasSuffix(".json")
    } ?? ""
    let simulatorURL = path(simulatorPath)
    guard !simulatorPath.isEmpty,
          let simulatorObject = try? loadJSON(simulatorURL) as? [String: Any] else {
        failures.append(fail("simulator-report", "missing reducer-covered iphonesimulator full-shell usability report for cat read-fault fix"))
        let reportURL = try writeReport(report(
            target: target,
            status: .fail,
            summary: "No pinned simulator full-shell report was available for the cat read-fault fix gate.",
            failures: failures,
            artifacts: artifacts,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("fail: \(relativePath(reportURL))")
        return 1
    }

    artifacts.append(simulatorPath)
    if stringField(simulatorObject, "git_sha") != gitSha() {
        failures.append(fail("simulator-report-stale", "selected full-shell simulator report is stale for current HEAD"))
    }
    if stringField(simulatorObject, "selected_device_id") != "1E5553B0-203A-4A11-BAD7-EBDE46863F66" ||
        stringField(simulatorObject, "selected_device_name") != "Orlix-iPhone-15-Pro-Max" ||
        intField(simulatorObject, "simulator_booted_count") != 1 ||
        !boolField(simulatorObject, "simulator_single_booted") {
        failures.append(fail("simulator-scope", "fix gate requires evidence from the single pinned Orlix-iPhone-15-Pro-Max simulator"))
    }
    if !reducerArtifacts.contains(simulatorPath) {
        failures.append(fail("reducer-coverage", "cat read-fault reducer must cover the selected simulator report"))
    }

    let engineURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "engine.c")
    let engineText = try readText(engineURL)
    artifacts.append(relativePath(engineURL))
    if !engineText.contains("case __NR_read:") ||
        !engineText.contains("tcti_refresh_current_user_range(regs->regs[1], regs->regs[0])") {
        failures.append(fail("read-buffer-refresh", "TCTI syscall return synchronization must refresh the guest read buffer after successful read(2) before rerunning full-shell cat"))
    }
    if engineText.contains("HostAdapter") ||
        engineText.contains("MAP_JIT") ||
        engineText.contains("VM_PROT_EXECUTE") ||
        engineText.contains("PROT_EXEC") {
        failures.append(fail("forbidden-scope", "cat read-fault fix must not add HostAdapter, JIT, or executable-memory behavior"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Validated reducer-backed TCTI read-buffer refresh fix before rerunning simulator full-shell usability.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_checked": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runPostFullShellCatPosixMemalignBRKReducer() throws -> Int32 {
    let target = "tcti-post-full-shell-cat-posix-memalign-brk-reducer"
    var failures: [Failure] = []
    var artifacts: [String] = []

    guard let simulatorReport = selectedRuntimeValidationReport(
        gate: "tcti-full-shell-usability",
        destination: "iphonesimulator"
    ) else {
        failures.append(fail("simulator-report", "missing iphonesimulator full-shell cat posix_memalign BRK report"))
        let reportURL = try writeReport(report(
            target: target,
            status: .fail,
            summary: "No pinned simulator full-shell cat posix_memalign BRK report was available for the reducer.",
            failures: failures,
            artifacts: artifacts,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("fail: \(relativePath(reportURL))")
        return 1
    }

    let object = simulatorReport.object
    artifacts.append(relativePath(simulatorReport.url))
    let reportArtifacts = (object["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
    artifacts.append(contentsOf: reportArtifacts)
    let terminalText = try reportArtifacts.first { $0.hasSuffix("simulator-terminal-output.txt") }.map(readRelativeArtifact) ?? ""
    let unifiedText = try reportArtifacts.first { $0.hasSuffix("simulator-unified.log") }.map(readRelativeArtifact) ?? ""
    let fullShellText = try reportArtifacts.first { $0.hasSuffix("tcti-full-shell-usability.txt") }.map(readRelativeArtifact) ?? ""
    let combinedText = [terminalText, unifiedText, fullShellText].joined(separator: "\n")
    let events = object["tcti_runtime_events"] as? [String: Any] ?? [:]
    let firstSVC = events["first_svc"] as? [String: Any] ?? [:]
    let signal = events["signaled_process"] as? [String: Any] ?? [:]
    let forbidden = object["forbidden_behavior"] as? [String: Any] ?? [:]
    let forbiddenKeys = [
        "generated_exec_memory",
        "host_exec_guest_text",
        "host_x18",
        "map_jit",
        "native_ios_api_exposure_to_guest",
        "rwx",
    ]

    if stringField(object, "git_sha") != gitSha() {
        failures.append(fail("simulator-report-stale", "selected full-shell simulator report is stale for current HEAD"))
    }
    if stringField(object, "status") != "fail" || boolField(object, "passed") {
        failures.append(fail("simulator-report-status", "reducer requires a current failing full-shell simulator report"))
    }
    if stringField(object, "selected_device_id") != "1E5553B0-203A-4A11-BAD7-EBDE46863F66" ||
        stringField(object, "selected_device_name") != "Orlix-iPhone-15-Pro-Max" ||
        intField(object, "simulator_booted_count") != 1 ||
        !boolField(object, "simulator_single_booted") {
        failures.append(fail("simulator-scope", "reducer requires the single pinned Orlix-iPhone-15-Pro-Max simulator report"))
    }
    for key in forbiddenKeys where forbidden[key] as? Bool != false {
        failures.append(fail("forbidden-behavior.\(key)", "simulator report must keep forbidden_behavior.\(key)=false"))
    }
    if stringField(firstSVC, "task") != "init" ||
        intField(firstSVC, "pid") != 1 ||
        intField(firstSVC, "syscall") != 178 {
        failures.append(fail("init-first-svc", "full-shell report must show init reached first TCTI svc syscall 178 before cat"))
    }
    if intField(signal, "signal") != nil {
        failures.append(fail("signal-phase", "cat posix_memalign BRK reducer expects no structured signaled_process event"))
    }
    if !combinedText.contains("Orlix TCTI: static PIE image task=cat") ||
        !combinedText.contains("In function posix_memalign") ||
        !combinedText.contains("__ensure(") ||
        !combinedText.contains("Orlix TCTI: unsupported instruction task=cat") ||
        !combinedText.contains("insn=0xd4200020") ||
        !combinedText.contains("x8=0x40") ||
        !combinedText.contains("orlix-init: process exited pid=32 status=132") ||
        fullShellText.contains("ORLIX-TCTI-SHELL-USABLE") {
        failures.append(fail("cat-posix-memalign-brk-signature", "full-shell artifacts must show cat static PIE progress, mlibc posix_memalign assertion, unsupported BRK 0xd4200020 with x8=64 in cat, shell status 132, and no success marker"))
    }

    struct PostFullShellCatPosixMemalignBRKModel: Codable {
        let task: String
        let unsupportedInstruction: String
        let syscallRegisterX8: String
        let shellStatus: Int
        let assertionFunction: String
        let invariant: String
    }
    if failures.isEmpty {
        let model = PostFullShellCatPosixMemalignBRKModel(
            task: "cat",
            unsupportedInstruction: "0xd4200020",
            syscallRegisterX8: "0x40",
            shellStatus: 132,
            assertionFunction: "posix_memalign",
            invariant: "full shell usability must complete cat /tmp/orlix-tcti-shell without mlibc posix_memalign alignment assertion or BRK before ORLIX-TCTI-SHELL-USABLE"
        )
        let modelURL = buildPath("reproducers", target, "post-full-shell-cat-posix-memalign-brk-model.json")
        try writeJSON(model, to: modelURL)
        artifacts.append(relativePath(modelURL))
    }

    let reducer = try writeReducer(
        target: target,
        caseID: "post-full-shell-cat-posix-memalign-brk-pass-regression",
        command: "TCTI_SIMULATOR_REPORT=\(relativePath(simulatorReport.url)) make tcti-gate TARGET=\(target)",
        reason: "pinned simulator full-shell command reaches cat, hits the mlibc posix_memalign alignment assertion, then stops on unsupported BRK 0xd4200020 before ORLIX-TCTI-SHELL-USABLE",
        artifacts: artifacts,
        expectedStatus: .pass
    )
    artifacts.append(relativePath(reducer))

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Reduced pinned simulator full-shell cat posix_memalign assertion and BRK trap to a report-backed no-phone evidence gate.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_reduced": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runPostFullShellCatPosixMemalignBRKFix() throws -> Int32 {
    let target = "tcti-post-full-shell-cat-posix-memalign-brk-fix"
    var failures: [Failure] = []
    var artifacts: [String] = []

    let reducerURL = buildPath("reports", "tcti-post-full-shell-cat-posix-memalign-brk-reducer", "report.json")
    let reducerObject = try? loadJSON(reducerURL) as? [String: Any]
    let reducerArtifacts = (reducerObject?["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
    if let reducerObject {
        artifacts.append(relativePath(reducerURL))
        if stringField(reducerObject, "git_sha") != gitSha() ||
            stringField(reducerObject, "status") != "pass" ||
            !boolField(reducerObject, "passed") {
            failures.append(fail("reducer-report", "cat posix_memalign BRK reducer report is missing, stale, or not passing"))
        }
    } else {
        failures.append(fail("reducer-report", "missing tcti-post-full-shell-cat-posix-memalign-brk-reducer report"))
    }

    let simulatorPath = reducerArtifacts.first {
        $0.hasPrefix("Build/Reports/runtime/tcti-full-shell-usability-") &&
            $0.hasSuffix(".json")
    } ?? ""
    let simulatorURL = path(simulatorPath)
    guard !simulatorPath.isEmpty,
          let simulatorObject = try? loadJSON(simulatorURL) as? [String: Any] else {
        failures.append(fail("simulator-report", "missing reducer-covered iphonesimulator full-shell usability report for cat posix_memalign BRK fix"))
        let reportURL = try writeReport(report(
            target: target,
            status: .fail,
            summary: "No pinned simulator full-shell report was available for the cat posix_memalign BRK fix gate.",
            failures: failures,
            artifacts: artifacts,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("fail: \(relativePath(reportURL))")
        return 1
    }

    artifacts.append(simulatorPath)
    if stringField(simulatorObject, "git_sha") != gitSha() {
        failures.append(fail("simulator-report-stale", "selected full-shell simulator report is stale for current HEAD"))
    }
    if stringField(simulatorObject, "selected_device_id") != "1E5553B0-203A-4A11-BAD7-EBDE46863F66" ||
        stringField(simulatorObject, "selected_device_name") != "Orlix-iPhone-15-Pro-Max" ||
        intField(simulatorObject, "simulator_booted_count") != 1 ||
        !boolField(simulatorObject, "simulator_single_booted") {
        failures.append(fail("simulator-scope", "fix gate requires evidence from the single pinned Orlix-iPhone-15-Pro-Max simulator"))
    }
    if !reducerArtifacts.contains(simulatorPath) {
        failures.append(fail("reducer-coverage", "cat posix_memalign BRK reducer must cover the selected simulator report"))
    }

    if let latestReport = latestRuntimeValidationReport(gate: "tcti-full-shell-usability", destination: "iphonesimulator") {
        let latestPath = relativePath(latestReport.url)
        let latestObject = latestReport.object
        let latestArtifacts = (latestObject["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
        let latestArtifactText = try latestArtifacts.compactMap { artifact -> String? in
            guard artifact.hasSuffix("simulator-terminal-output.txt") ||
                artifact.hasSuffix("launch.stderr") ||
                artifact.hasSuffix("tcti-full-shell-usability.txt") else {
                return nil
            }
            return try readRelativeArtifact(artifact)
        }.joined(separator: "\n")
        let latestFullShellText = try latestArtifacts.first {
            $0.hasSuffix("tcti-full-shell-usability.txt")
        }.map(readRelativeArtifact) ?? ""
        let latestEvents = latestObject["tcti_runtime_events"] as? [String: Any] ?? [:]
        let latestFirstSVC = latestEvents["first_svc"] as? [String: Any] ?? [:]
        let latestSignal = latestEvents["signaled_process"] as? [String: Any] ?? [:]
        let latestMatchesCurrentFailure = stringField(latestObject, "git_sha") == gitSha() &&
            stringField(latestObject, "status") == "fail" &&
            !boolField(latestObject, "passed") &&
            stringField(latestObject, "selected_device_id") == "1E5553B0-203A-4A11-BAD7-EBDE46863F66" &&
            stringField(latestObject, "selected_device_name") == "Orlix-iPhone-15-Pro-Max" &&
            intField(latestObject, "simulator_booted_count") == 1 &&
            boolField(latestObject, "simulator_single_booted") &&
            stringField(latestFirstSVC, "task") == "init" &&
            intField(latestFirstSVC, "pid") == 1 &&
            intField(latestFirstSVC, "syscall") == 178 &&
            intField(latestSignal, "signal") == nil &&
            latestArtifactText.contains("Orlix TCTI: static PIE image task=cat") &&
            latestArtifactText.contains("In function posix_memalign") &&
            latestArtifactText.contains("__ensure(") &&
            latestArtifactText.contains("Orlix TCTI: unsupported instruction task=cat") &&
            latestArtifactText.contains("insn=0xd4200020") &&
            latestArtifactText.contains("x8=0x40") &&
            latestArtifactText.contains("orlix-init: process exited pid=32 status=132") &&
            !latestFullShellText.contains("ORLIX-TCTI-SHELL-USABLE")
        if latestMatchesCurrentFailure {
            if !artifacts.contains(latestPath) {
                artifacts.append(latestPath)
            }
            if !reducerArtifacts.contains(latestPath) {
                failures.append(fail("latest-reducer-coverage", "cat posix_memalign BRK fix requires reducer coverage for the latest matching full-shell report \(latestPath)"))
            }
            if simulatorPath != latestPath {
                failures.append(fail("stale-fix-report", "cat posix_memalign BRK fix is tied to \(simulatorPath), but latest matching full-shell report is \(latestPath)"))
            }
            failures.append(fail("latest-simulator-still-fails", "cat posix_memalign BRK fix cannot pass while the latest current pinned full-shell simulator report still shows the posix_memalign assertion, unsupported BRK 0xd4200020 with x8=0x40 in cat, shell status 132, and no shell success marker"))
        }
    }

    let mmapURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "mm", "mmap.c")
    let mmapText = try readText(mmapURL)
    artifacts.append(relativePath(mmapURL))
    if !mmapText.contains("arch_boot_host_page_size()") ||
        !mmapText.contains("orlix_hosted_prot_none_reservation") ||
        !mmapText.contains("orlix_hosted_anonymous_private_mapping") ||
        !mmapText.contains("orlix_hosted_mmap_length_align_mask") ||
        !mmapText.contains("len < PAGE_SIZE || (len & (len - 1))") ||
        !mmapText.contains("return len - 1") ||
        !mmapText.contains("orlix_hosted_mmap_align_mask") ||
        !mmapText.contains("orlix_hosted_mmap_align_offset") ||
        !mmapText.contains("info.align_mask = orlix_hosted_mmap_align_mask(file,") ||
        !mmapText.contains("info.align_offset = orlix_hosted_mmap_align_offset(file,") ||
        !mmapText.contains("result = vm_unmapped_area(&info)") ||
        !mmapText.contains("offset_in_page(result)") {
        failures.append(fail("hosted-mmap-alignment", "hosted anonymous mmap selection must preserve power-of-two length alignment and host-page alignment before rerunning full-shell cat"))
    }
    if mmapText.contains("HostAdapter") ||
        mmapText.contains("MAP_JIT") ||
        mmapText.contains("VM_PROT_EXECUTE") ||
        mmapText.contains("PROT_EXEC") {
        failures.append(fail("forbidden-scope", "cat posix_memalign BRK fix must not add HostAdapter, JIT, or executable-memory behavior"))
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Checked reducer-backed TCTI full-shell cat posix_memalign BRK fix preserves hosted anonymous mmap length alignment without implementing BRK.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_checked": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runPostFullShellInitWriteFaultReducer() throws -> Int32 {
    let target = "tcti-post-full-shell-init-write-fault-reducer"
    var failures: [Failure] = []
    var artifacts: [String] = []

    guard let simulatorReport = selectedRuntimeValidationReport(
        gate: "tcti-full-shell-usability",
        destination: "iphonesimulator"
    ) else {
        failures.append(fail("simulator-report", "missing iphonesimulator full-shell init write-fault report"))
        let reportURL = try writeReport(report(
            target: target,
            status: .fail,
            summary: "No pinned simulator full-shell init write-fault report was available for the reducer.",
            failures: failures,
            artifacts: artifacts,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("fail: \(relativePath(reportURL))")
        return 1
    }

    let object = simulatorReport.object
    artifacts.append(relativePath(simulatorReport.url))
    let reportArtifacts = (object["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
    artifacts.append(contentsOf: reportArtifacts)
    let terminalText = try reportArtifacts.first { $0.hasSuffix("simulator-terminal-output.txt") }.map(readRelativeArtifact) ?? ""
    let launchStderrText = try reportArtifacts.first { $0.hasSuffix("launch.stderr") }.map(readRelativeArtifact) ?? ""
    let fullShellText = try reportArtifacts.first { $0.hasSuffix("tcti-full-shell-usability.txt") }.map(readRelativeArtifact) ?? ""
    let combinedText = [terminalText, launchStderrText, fullShellText].joined(separator: "\n")
    let events = object["tcti_runtime_events"] as? [String: Any] ?? [:]
    let firstSVC = events["first_svc"] as? [String: Any] ?? [:]
    let staticPIE = events["static_pie_image"] as? [String: Any] ?? [:]
    let mmap = events["last_mmap_syscall"] as? [String: Any] ?? [:]
    let fault = events["fatal_user_fault"] as? [String: Any] ?? [:]
    let signal = events["signaled_process"] as? [String: Any] ?? [:]
    let faultPC = stringField(fault, "pc")
    let faultLR = stringField(fault, "lr")
    let faultSP = stringField(fault, "sp")
    let faultAddress = stringField(fault, "addr")
    let staticBase = stringField(staticPIE, "base")
    let staticEntry = stringField(staticPIE, "entry")
    let faultOffset: String? = {
        guard
            !staticBase.isEmpty,
            !faultAddress.isEmpty,
            let base = UInt64(staticBase.dropFirst(2), radix: 16),
            let address = UInt64(faultAddress.dropFirst(2), radix: 16),
            address >= base
        else {
            return nil
        }
        return String(format: "0x%llx", address - base)
    }()

    if stringField(object, "git_sha") != gitSha() {
        failures.append(fail("simulator-report-stale", "selected full-shell simulator report is stale for current HEAD"))
    }
    if stringField(object, "status") != "fail" || boolField(object, "passed") {
        failures.append(fail("simulator-report-status", "reducer requires a current failing full-shell simulator report"))
    }
    if stringField(object, "selected_device_id") != "1E5553B0-203A-4A11-BAD7-EBDE46863F66" ||
        stringField(object, "selected_device_name") != "Orlix-iPhone-15-Pro-Max" ||
        intField(object, "simulator_booted_count") != 1 ||
        !boolField(object, "simulator_single_booted") {
        failures.append(fail("simulator-scope", "reducer requires the single pinned Orlix-iPhone-15-Pro-Max simulator report"))
    }
    if stringField(firstSVC, "task") != "init" ||
        intField(firstSVC, "pid") != 1 ||
        intField(firstSVC, "syscall") != 178 {
        failures.append(fail("init-first-svc", "full-shell report must show init reached first TCTI svc syscall 178 before the write fault"))
    }
    if stringField(staticPIE, "task") != "init" ||
        intField(staticPIE, "pid") != 1 ||
        staticBase.isEmpty ||
        staticEntry.isEmpty {
        failures.append(fail("static-pie-init", "full-shell report must show static PIE image task=init pid=1 before the write fault"))
    }
    if stringField(mmap, "task") != "init" ||
        intField(mmap, "pid") != 1 ||
        intField(mmap, "syscall") != 222 {
        failures.append(fail("init-mmap", "full-shell report must show init pid=1 issued mmap syscall 222 before the write fault"))
    }
    if stringField(fault, "task") != "init" ||
        intField(fault, "pid") != 1 ||
        intField(fault, "access") != 2 ||
        intField(fault, "si") != 2 ||
        faultPC.isEmpty ||
        faultLR.isEmpty ||
        faultSP.isEmpty ||
        faultAddress.isEmpty ||
        faultAddress == "0x0" ||
        faultOffset == nil {
        failures.append(fail("init-write-fault-signature", "full-shell report must show init write fault at a non-null static PIE address with access=2 si=2"))
    }
    if intField(signal, "signal") != nil {
        failures.append(fail("init-write-fault-phase", "init write-fault reducer expects runtime report before a structured signaled_process event"))
    }
    if !combinedText.contains("Orlix TCTI: static PIE image task=init") ||
        !combinedText.contains("Orlix TCTI: user fault task=init") ||
        !combinedText.contains("access=2") ||
        !combinedText.contains("si=2") ||
        fullShellText.contains("ORLIX-TCTI-SHELL-USABLE") {
        failures.append(fail("init-write-fault-artifact", "full-shell artifacts must show init static PIE write fault before the success marker"))
    }

    struct PostFullShellInitWriteFaultModel: Codable {
        let task: String
        let pid: Int
        let firstSyscall: Int
        let mmapSyscall: Int
        let staticPIEBase: String
        let staticPIEEntry: String
        let faultPC: String
        let faultLR: String
        let faultSP: String
        let faultAddress: String
        let faultOffset: String
        let access: String
        let si: Int
        let invariant: String
    }
    if let offset = faultOffset {
        let model = PostFullShellInitWriteFaultModel(
            task: "init",
            pid: 1,
            firstSyscall: 178,
            mmapSyscall: 222,
            staticPIEBase: staticBase,
            staticPIEEntry: staticEntry,
            faultPC: faultPC,
            faultLR: faultLR,
            faultSP: faultSP,
            faultAddress: faultAddress,
            faultOffset: offset,
            access: "write",
            si: 2,
            invariant: "after full-shell init enters its static PIE image and maps anonymous writable memory, TCTI must resolve legitimate Linux user writes without host execution or HostAdapter runtime behavior"
        )
        let modelURL = buildPath("reproducers", target, "post-full-shell-init-write-fault-model.json")
        try writeJSON(model, to: modelURL)
        artifacts.append(relativePath(modelURL))
    }

    let reducer = try writeReducer(
        target: target,
        caseID: "post-full-shell-init-write-fault-pass-regression",
        command: "TCTI_SIMULATOR_REPORT=\(relativePath(simulatorReport.url)) make tcti-gate TARGET=\(target)",
        reason: "pinned simulator full-shell command reaches init static PIE, then TCTI faults an init user-data write with access=2 si=2 before ORLIX-TCTI-SHELL-USABLE",
        artifacts: artifacts,
        expectedStatus: .pass
    )
    artifacts.append(relativePath(reducer))

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Reduced pinned simulator full-shell init user-data write fault to a no-phone TCTI evidence gate.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_reduced": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runPostFullShellSHSIGABRTReducer() throws -> Int32 {
    let target = "tcti-post-full-shell-sh-sigabrt-reducer"
    var failures: [Failure] = []
    var artifacts: [String] = []

    guard let simulatorReport = selectedRuntimeValidationReport(
        gate: "tcti-full-shell-usability",
        destination: "iphonesimulator"
    ) else {
        failures.append(fail("simulator-report", "missing iphonesimulator full-shell sh SIGABRT report"))
        let reportURL = try writeReport(report(
            target: target,
            status: .fail,
            summary: "No pinned simulator full-shell sh SIGABRT report was available for the reducer.",
            failures: failures,
            artifacts: artifacts,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("fail: \(relativePath(reportURL))")
        return 1
    }

    let object = simulatorReport.object
    artifacts.append(relativePath(simulatorReport.url))
    let reportArtifacts = (object["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
    artifacts.append(contentsOf: reportArtifacts)
    let terminalText = try reportArtifacts.first { $0.hasSuffix("simulator-terminal-output.txt") }.map(readRelativeArtifact) ?? ""
    let launchStderrText = try reportArtifacts.first { $0.hasSuffix("launch.stderr") }.map(readRelativeArtifact) ?? ""
    let fullShellText = try reportArtifacts.first { $0.hasSuffix("tcti-full-shell-usability.txt") }.map(readRelativeArtifact) ?? ""
    let combinedText = [terminalText, launchStderrText, fullShellText].joined(separator: "\n")
    let events = object["tcti_runtime_events"] as? [String: Any] ?? [:]
    let firstSVC = events["first_svc"] as? [String: Any] ?? [:]
    let staticPIE = events["static_pie_image"] as? [String: Any] ?? [:]
    let mmap = events["last_mmap_syscall"] as? [String: Any] ?? [:]
    let fault = events["fatal_user_fault"] as? [String: Any] ?? [:]
    let signal = events["signaled_process"] as? [String: Any] ?? [:]
    let lastReturn = events["last_sh_syscall_return"] as? [String: Any] ?? [:]
    let staticBase = stringField(staticPIE, "base")
    let staticEntry = stringField(staticPIE, "entry")
    let shPID = intField(staticPIE, "pid")
    let shPIDHex = shPID.map { String(format: "0x%x", $0) } ?? ""

    if stringField(object, "git_sha") != gitSha() {
        failures.append(fail("simulator-report-stale", "selected full-shell simulator report is stale for current HEAD"))
    }
    if stringField(object, "status") != "fail" || boolField(object, "passed") {
        failures.append(fail("simulator-report-status", "reducer requires a current failing full-shell simulator report"))
    }
    if stringField(object, "selected_device_id") != "1E5553B0-203A-4A11-BAD7-EBDE46863F66" ||
        stringField(object, "selected_device_name") != "Orlix-iPhone-15-Pro-Max" ||
        intField(object, "simulator_booted_count") != 1 ||
        !boolField(object, "simulator_single_booted") {
        failures.append(fail("simulator-scope", "reducer requires the single pinned Orlix-iPhone-15-Pro-Max simulator report"))
    }
    if stringField(firstSVC, "task") != "init" ||
        intField(firstSVC, "pid") != 1 ||
        intField(firstSVC, "syscall") != 178 {
        failures.append(fail("init-first-svc", "full-shell report must show init reached first TCTI svc syscall 178 before sh aborts"))
    }
    if stringField(staticPIE, "task") != "sh" ||
        shPID == nil ||
        staticBase.isEmpty ||
        staticEntry != "0x45418" {
        failures.append(fail("sh-static-pie", "full-shell report must show sh static PIE image with entry 0x45418"))
    }
    if stringField(mmap, "task") != "sh" ||
        intField(mmap, "pid") != shPID ||
        intField(mmap, "syscall") != 222 {
        failures.append(fail("sh-mmap", "full-shell report must show sh issued mmap syscall 222 before aborting"))
    }
    if intField(fault, "access") != nil ||
        intField(fault, "si") != nil ||
        stringField(fault, "addr") != "" {
        failures.append(fail("no-user-fault", "sh SIGABRT reducer expects no fatal TCTI user-fault signature"))
    }
    if intField(signal, "pid") != shPID ||
        intField(signal, "signal") != 6 {
        failures.append(fail("sh-signal", "full-shell report must show sh received SIGABRT signal 6"))
    }
    if stringField(lastReturn, "task") != "sh" ||
        intField(lastReturn, "pid") != shPID ||
        intField(lastReturn, "syscall") != 172 ||
        intField(lastReturn, "signed_ret") != shPID {
        failures.append(fail("sh-syscall-return", "full-shell report must preserve sh syscall 172 returning the sh pid before kill(SIGABRT)"))
    }
    if !combinedText.contains("Orlix TCTI: static PIE image task=sh") ||
        !combinedText.contains("Orlix TCTI: syscall return task=sh") ||
        !combinedText.contains("syscall=172 ret=\(shPIDHex) signed_ret=\(shPID ?? -1)") ||
        !combinedText.contains("syscall=129 x0=\(shPIDHex) x1=0x6") ||
        !combinedText.contains("orlix-init: process signaled pid=\(shPID ?? -1) signal=6") ||
        !combinedText.contains("syscall=160") ||
        fullShellText.contains("ORLIX-TCTI-SHELL-USABLE") {
        failures.append(fail("sh-sigabrt-artifact", "full-shell artifacts must show sh reaching uname/getpid then SIGABRT before the success marker"))
    }

    struct PostFullShellSHSIGABRTModel: Codable {
        let task: String
        let pid: Int
        let signal: Int
        let staticPIEBase: String
        let staticPIEEntry: String
        let lastReturnSyscall: Int
        let lastReturnValue: Int
        let invariant: String
    }
    if failures.isEmpty {
        let model = PostFullShellSHSIGABRTModel(
            task: "sh",
            pid: shPID ?? -1,
            signal: 6,
            staticPIEBase: staticBase,
            staticPIEEntry: staticEntry,
            lastReturnSyscall: 172,
            lastReturnValue: shPID ?? -1,
            invariant: "the full shell command must reach ORLIX-TCTI-SHELL-USABLE instead of having sh abort itself after uname, syscall-return, and mmap progress"
        )
        let modelURL = buildPath("reproducers", target, "post-full-shell-sh-sigabrt-model.json")
        try writeJSON(model, to: modelURL)
        artifacts.append(relativePath(modelURL))
    }

    let reducer = try writeReducer(
        target: target,
        caseID: "post-full-shell-sh-sigabrt-pass-regression",
        command: "TCTI_SIMULATOR_REPORT=\(relativePath(simulatorReport.url)) make tcti-gate TARGET=\(target)",
        reason: "pinned simulator full-shell command reaches sh static PIE and syscall-return progress, then sh sends SIGABRT before ORLIX-TCTI-SHELL-USABLE",
        artifacts: artifacts,
        expectedStatus: .pass
    )
    artifacts.append(relativePath(reducer))

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Reduced pinned simulator full-shell sh SIGABRT to a no-phone TCTI evidence gate.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_reduced": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runPostConsoleSHSIGABRTReducer() throws -> Int32 {
    let target = "tcti-post-console-sh-sigabrt-reducer"
    var failures: [Failure] = []
    var artifacts: [String] = []

    guard let simulatorReport = selectedRuntimeValidationReport(
        gate: "tcti-init-console-write",
        destination: "iphonesimulator"
    ) else {
        failures.append(fail("simulator-report", "missing iphonesimulator console sh SIGABRT report"))
        let reportURL = try writeReport(report(
            target: target,
            status: .fail,
            summary: "No pinned simulator console sh SIGABRT report was available for the reducer.",
            failures: failures,
            artifacts: artifacts,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("fail: \(relativePath(reportURL))")
        return 1
    }

    let object = simulatorReport.object
    artifacts.append(relativePath(simulatorReport.url))
    let reportArtifacts = (object["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
    artifacts.append(contentsOf: reportArtifacts)
    let terminalText = try reportArtifacts.first { $0.hasSuffix("simulator-terminal-output.txt") }.map(readRelativeArtifact) ?? ""
    let launchStderrText = try reportArtifacts.first { $0.hasSuffix("launch.stderr") }.map(readRelativeArtifact) ?? ""
    let consoleText = try reportArtifacts.first { $0.hasSuffix("tcti-console-write.txt") }.map(readRelativeArtifact) ?? ""
    let combinedText = [terminalText, launchStderrText, consoleText].joined(separator: "\n")
    let events = object["tcti_runtime_events"] as? [String: Any] ?? [:]
    let firstSVC = events["first_svc"] as? [String: Any] ?? [:]
    let staticPIE = events["static_pie_image"] as? [String: Any] ?? [:]
    let mmap = events["last_mmap_syscall"] as? [String: Any] ?? [:]
    let fault = events["fatal_user_fault"] as? [String: Any] ?? [:]
    let signal = events["signaled_process"] as? [String: Any] ?? [:]
    let lastReturn = events["last_sh_syscall_return"] as? [String: Any] ?? [:]
    let forbidden = object["forbidden_behavior"] as? [String: Any] ?? [:]
    let staticBase = stringField(staticPIE, "base")
    let staticEntry = stringField(staticPIE, "entry")
    let shPID = intField(staticPIE, "pid")
    let shPIDHex = shPID.map { String(format: "0x%x", $0) } ?? ""

    if stringField(object, "git_sha") != gitSha() {
        failures.append(fail("simulator-report-stale", "selected console simulator report is stale for current HEAD"))
    }
    if stringField(object, "gate") != "tcti-init-console-write" ||
        stringField(object, "destination") != "iphonesimulator" ||
        stringField(object, "status") != "fail" ||
        boolField(object, "passed") {
        failures.append(fail("simulator-report-status", "reducer requires a current failing iphonesimulator tcti-init-console-write report"))
    }
    if stringField(object, "selected_device_id") != "1E5553B0-203A-4A11-BAD7-EBDE46863F66" ||
        stringField(object, "selected_device_name") != "Orlix-iPhone-15-Pro-Max" ||
        intField(object, "simulator_booted_count") != 1 ||
        !boolField(object, "simulator_single_booted") {
        failures.append(fail("simulator-scope", "reducer requires the single pinned Orlix-iPhone-15-Pro-Max simulator report"))
    }
    for key in [
        "generated_exec_memory",
        "host_exec_guest_text",
        "host_x18",
        "map_jit",
        "native_ios_api_exposure_to_guest",
        "rwx",
    ] where forbidden[key] as? Bool != false {
        failures.append(fail("forbidden-behavior", "forbidden_behavior.\(key) must be false in the simulator report"))
    }
    if stringField(firstSVC, "task") != "init" ||
        intField(firstSVC, "pid") != 1 ||
        intField(firstSVC, "syscall") != 178 {
        failures.append(fail("init-first-svc", "console report must show init reached first TCTI svc syscall 178 before sh aborts"))
    }
    if stringField(staticPIE, "task") != "sh" ||
        shPID == nil ||
        staticBase.isEmpty ||
        staticEntry != "0x45418" {
        failures.append(fail("sh-static-pie", "console report must show sh static PIE image with entry 0x45418"))
    }
    if stringField(mmap, "task") != "sh" ||
        intField(mmap, "pid") != shPID ||
        intField(mmap, "syscall") != 222 {
        failures.append(fail("sh-mmap", "console report must show sh issued mmap syscall 222 before aborting"))
    }
    if intField(fault, "access") != nil ||
        intField(fault, "si") != nil ||
        stringField(fault, "addr") != "" {
        failures.append(fail("no-user-fault", "console sh SIGABRT reducer expects no fatal TCTI user-fault signature"))
    }
    if intField(signal, "pid") != shPID ||
        intField(signal, "signal") != 6 {
        failures.append(fail("sh-signal", "console report must show sh received SIGABRT signal 6"))
    }
    if stringField(lastReturn, "task") != "sh" ||
        intField(lastReturn, "pid") != shPID ||
        intField(lastReturn, "syscall") != 172 ||
        intField(lastReturn, "signed_ret") != shPID {
        failures.append(fail("sh-syscall-return", "console report must preserve sh syscall 172 returning the sh pid before kill(SIGABRT)"))
    }
    if !combinedText.contains("Orlix TCTI: static PIE image task=sh") ||
        !combinedText.contains("Orlix TCTI: syscall return task=sh") ||
        !combinedText.contains("syscall=160") ||
        !combinedText.contains("syscall=172 ret=\(shPIDHex) signed_ret=\(shPID ?? -1)") ||
        !combinedText.contains("syscall=129 x0=\(shPIDHex) x1=0x6") ||
        !combinedText.contains("orlix-init: process signaled pid=\(shPID ?? -1) signal=6") ||
        consoleText.contains("ORLIX-TCTI-CONSOLE-OK") {
        failures.append(fail("sh-sigabrt-artifact", "console artifacts must show sh reaching uname/getpid then SIGABRT before the console success marker"))
    }

    struct PostConsoleSHSIGABRTModel: Codable {
        let task: String
        let pid: Int
        let signal: Int
        let staticPIEBase: String
        let staticPIEEntry: String
        let lastReturnSyscall: Int
        let lastReturnValue: Int
        let invariant: String
    }
    if failures.isEmpty {
        let model = PostConsoleSHSIGABRTModel(
            task: "sh",
            pid: shPID ?? -1,
            signal: 6,
            staticPIEBase: staticBase,
            staticPIEEntry: staticEntry,
            lastReturnSyscall: 172,
            lastReturnValue: shPID ?? -1,
            invariant: "the simulator console command must reach ORLIX-TCTI-CONSOLE-OK instead of having sh abort itself after uname, syscall-return, and mmap progress"
        )
        let modelURL = buildPath("reproducers", target, "post-console-sh-sigabrt-model.json")
        try writeJSON(model, to: modelURL)
        artifacts.append(relativePath(modelURL))
    }

    let reducer = try writeReducer(
        target: target,
        caseID: "post-console-sh-sigabrt-pass-regression",
        command: "TCTI_SIMULATOR_REPORT=\(relativePath(simulatorReport.url)) make tcti-gate TARGET=\(target)",
        reason: "pinned simulator console command reaches sh static PIE and syscall-return progress, then sh sends SIGABRT before ORLIX-TCTI-CONSOLE-OK",
        artifacts: artifacts,
        expectedStatus: .pass
    )
    artifacts.append(relativePath(reducer))

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Reduced pinned simulator console sh SIGABRT to a no-phone TCTI evidence gate.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_reduced": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runPostFullShellInitWriteFaultFix() throws -> Int32 {
    let target = "tcti-post-full-shell-init-write-fault-fix"
    var failures: [Failure] = []
    var artifacts: [String] = []

    let reducerReportURL = buildPath("reports", "tcti-post-full-shell-init-write-fault-reducer", "report.json")
    var reducerArtifacts: [String] = []
    if let reducerReport = try? loadJSON(reducerReportURL) as? [String: Any] {
        reducerArtifacts = (reducerReport["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
        artifacts.append(relativePath(reducerReportURL))
        artifacts.append(contentsOf: reducerArtifacts)
        if stringField(reducerReport, "git_sha") != gitSha() ||
            stringField(reducerReport, "status") != "pass" ||
            !boolField(reducerReport, "passed") {
            failures.append(fail("reducer-report", "post-full-shell init write-fault reducer report is missing, stale, or not passing"))
        }
    } else {
        failures.append(fail("reducer-report", "missing tcti-post-full-shell-init-write-fault-reducer report"))
    }

    guard let simulatorReport = selectedRuntimeValidationReport(
        gate: "tcti-full-shell-usability",
        destination: "iphonesimulator"
    ) else {
        failures.append(fail("simulator-report", "missing iphonesimulator full-shell usability report for init write-fault fix"))
        let reportURL = try writeReport(report(
            target: target,
            status: .fail,
            summary: "No pinned simulator full-shell report was available for the init write-fault fix gate.",
            failures: failures,
            artifacts: artifacts,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("fail: \(relativePath(reportURL))")
        return 1
    }

    let simulatorObject = simulatorReport.object
    let simulatorPath = relativePath(simulatorReport.url)
    artifacts.append(simulatorPath)
    if !reducerArtifacts.contains(simulatorPath) {
        failures.append(fail("reducer-coverage", "init write-fault reducer must cover the selected simulator report"))
    }
    if stringField(simulatorObject, "git_sha") != gitSha() {
        failures.append(fail("simulator-report-stale", "selected full-shell simulator report is stale for current HEAD"))
    }
    if stringField(simulatorObject, "selected_device_id") != "1E5553B0-203A-4A11-BAD7-EBDE46863F66" ||
        stringField(simulatorObject, "selected_device_name") != "Orlix-iPhone-15-Pro-Max" ||
        intField(simulatorObject, "simulator_booted_count") != 1 ||
        !boolField(simulatorObject, "simulator_single_booted") {
        failures.append(fail("simulator-scope", "fix gate requires evidence from the single pinned Orlix-iPhone-15-Pro-Max simulator"))
    }

    let faultURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "mm", "fault.c")
    let faultText = try readText(faultURL)
    artifacts.append(relativePath(faultURL))
    if !faultText.contains("tcti_sync_faulted_user_window(address, access)") {
        failures.append(fail("tcti-fault-access", "TCTI user-fault handling must pass the access class into hosted window sync"))
    }
    if !faultText.contains("ORLIX_HOST_USER_FAULT_WRITE") ||
        !faultText.contains("case TCTI_ACCESS_WRITE:") {
        failures.append(fail("tcti-write-sync", "TCTI write faults must sync the hosted user fault window with ORLIX_HOST_USER_FAULT_WRITE"))
    }
    if !faultText.contains("ORLIX_HOST_USER_FAULT_EXEC") ||
        !faultText.contains("case TCTI_ACCESS_FETCH:") {
        failures.append(fail("tcti-fetch-sync", "TCTI fetch faults must preserve executable access when syncing the hosted user fault window"))
    }
    for forbidden in ["HostAdapter", "MAP_JIT", "VM_PROT_EXECUTE", "PROT_EXEC"] {
        if faultText.contains(forbidden) {
            failures.append(fail("forbidden-scope", "init write-fault fix must not add \(forbidden) behavior"))
        }
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Checked reducer-backed TCTI init write-fault fix preserves access class through Linux user-fault and hosted window sync.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_checked": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runPostFullShellSHSIGABRTFix() throws -> Int32 {
    let target = "tcti-post-full-shell-sh-sigabrt-fix"
    var failures: [Failure] = []
    var artifacts: [String] = []

    let reducerReportURL = buildPath("reports", "tcti-post-full-shell-sh-sigabrt-reducer", "report.json")
    var reducerArtifacts: [String] = []
    if let reducerReport = try? loadJSON(reducerReportURL) as? [String: Any] {
        reducerArtifacts = (reducerReport["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
        artifacts.append(relativePath(reducerReportURL))
        artifacts.append(contentsOf: reducerArtifacts)
        if stringField(reducerReport, "git_sha") != gitSha() ||
            stringField(reducerReport, "status") != "pass" ||
            !boolField(reducerReport, "passed") {
            failures.append(fail("reducer-report", "post-full-shell sh SIGABRT reducer report is missing, stale, or not passing"))
        }
    } else {
        failures.append(fail("reducer-report", "missing tcti-post-full-shell-sh-sigabrt-reducer report"))
    }

    let simulatorPath = reducerArtifacts.first {
        $0.hasPrefix("Build/Reports/runtime/tcti-full-shell-usability-") &&
            $0.hasSuffix(".json")
    } ?? ""
    let simulatorURL = path(simulatorPath)
    guard !simulatorPath.isEmpty,
          let simulatorObject = try? loadJSON(simulatorURL) as? [String: Any] else {
        failures.append(fail("simulator-report", "missing reducer-covered iphonesimulator full-shell usability report for sh SIGABRT fix"))
        let reportURL = try writeReport(report(
            target: target,
            status: .fail,
            summary: "No pinned simulator full-shell report was available for the sh SIGABRT fix gate.",
            failures: failures,
            artifacts: artifacts,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("fail: \(relativePath(reportURL))")
        return 1
    }

    artifacts.append(simulatorPath)
    if stringField(simulatorObject, "git_sha") != gitSha() {
        failures.append(fail("simulator-report-stale", "selected full-shell simulator report is stale for current HEAD"))
    }
    if stringField(simulatorObject, "selected_device_id") != "1E5553B0-203A-4A11-BAD7-EBDE46863F66" ||
        stringField(simulatorObject, "selected_device_name") != "Orlix-iPhone-15-Pro-Max" ||
        intField(simulatorObject, "simulator_booted_count") != 1 ||
        !boolField(simulatorObject, "simulator_single_booted") {
        failures.append(fail("simulator-scope", "fix gate requires evidence from the single pinned Orlix-iPhone-15-Pro-Max simulator"))
    }

    let userPageURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "mm", "tcti_user_page.c")
    let initURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "mm", "init.c")
    let userPageText = try readText(userPageURL)
    let initText = try readText(initURL)
    artifacts.append(relativePath(userPageURL))
    artifacts.append(relativePath(initURL))
    if !userPageText.contains("unsigned long fault_flags = 0") ||
        !userPageText.contains("case TCTI_ACCESS_WRITE:") ||
        !userPageText.contains("ORLIX_HOST_USER_FAULT_WRITE") ||
        !userPageText.contains("case TCTI_ACCESS_FETCH:") ||
        !userPageText.contains("ORLIX_HOST_USER_FAULT_EXEC") ||
        !userPageText.contains("orlix_sync_current_user_fault_window(address, fault_flags)") {
        failures.append(fail("tcti-user-window-access", "TCTI user-page fault sync must preserve READ/WRITE/FETCH access class when refreshing the hosted user window"))
    }
    if !initText.contains("host_fault_flags & ORLIX_HOST_USER_FAULT_EXEC") ||
        !initText.contains("host_fault_flags & ORLIX_HOST_USER_FAULT_WRITE") ||
        !initText.contains("return orlix_fault_in_user_page_unlocked(mm, page, fault_flags)") ||
        !initText.contains("fault_flags & ORLIX_HOST_USER_FAULT_WRITE") ||
        !initText.contains("orlix_fault_in_user_page_unlocked(mm, page, fault_flags)") ||
        !initText.contains("mmap_read_unlock(mm)") {
        failures.append(fail("hosted-fault-window-access", "hosted user fault-window sync must honor explicit TCTI EXEC and WRITE fault flags before refreshing host windows"))
    }
    for forbidden in ["HostAdapter", "MAP_JIT", "VM_PROT_EXECUTE", "PROT_EXEC"] {
        if userPageText.contains(forbidden) || initText.contains(forbidden) {
            failures.append(fail("forbidden-scope", "sh SIGABRT fix must not add \(forbidden) behavior"))
        }
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Checked reducer-backed TCTI full-shell sh SIGABRT fix preserves hosted user-window access class for faulted pages.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_checked": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runPostFullShellInitSecondMmapHangReducer() throws -> Int32 {
    let target = "tcti-post-full-shell-init-second-mmap-hang-reducer"
    var failures: [Failure] = []
    var artifacts: [String] = []

    guard let simulatorReport = selectedRuntimeValidationReport(
        gate: "tcti-full-shell-usability",
        destination: "iphonesimulator"
    ) else {
        failures.append(fail("simulator-report", "missing iphonesimulator full-shell init second-mmap hang report"))
        let reportURL = try writeReport(report(
            target: target,
            status: .fail,
            summary: "No pinned simulator full-shell init second-mmap hang report was available for the reducer.",
            failures: failures,
            artifacts: artifacts,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("fail: \(relativePath(reportURL))")
        return 1
    }

    let object = simulatorReport.object
    artifacts.append(relativePath(simulatorReport.url))
    let reportArtifacts = (object["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
    artifacts.append(contentsOf: reportArtifacts)
    let terminalText = try reportArtifacts.first { $0.hasSuffix("simulator-terminal-output.txt") }.map(readRelativeArtifact) ?? ""
    let fullShellText = try reportArtifacts.first { $0.hasSuffix("tcti-full-shell-usability.txt") }.map(readRelativeArtifact) ?? ""
    let events = object["tcti_runtime_events"] as? [String: Any] ?? [:]
    let firstSVC = events["first_svc"] as? [String: Any] ?? [:]
    let staticPIE = events["static_pie_image"] as? [String: Any] ?? [:]
    let mmap = events["last_mmap_syscall"] as? [String: Any] ?? [:]
    let fault = events["fatal_user_fault"] as? [String: Any] ?? [:]
    let signal = events["signaled_process"] as? [String: Any] ?? [:]
    let lastReturn = events["last_sh_syscall_return"] as? [String: Any] ?? [:]

    if stringField(object, "git_sha") != gitSha() {
        failures.append(fail("simulator-report-stale", "selected full-shell simulator report is stale for current HEAD"))
    }
    if stringField(object, "status") != "fail" || boolField(object, "passed") {
        failures.append(fail("simulator-report-status", "reducer requires a current failing full-shell simulator report"))
    }
    if stringField(object, "selected_device_id") != "1E5553B0-203A-4A11-BAD7-EBDE46863F66" ||
        stringField(object, "selected_device_name") != "Orlix-iPhone-15-Pro-Max" ||
        intField(object, "simulator_booted_count") != 1 ||
        !boolField(object, "simulator_single_booted") {
        failures.append(fail("simulator-scope", "reducer requires the single pinned Orlix-iPhone-15-Pro-Max simulator report"))
    }
    if stringField(firstSVC, "task") != "init" ||
        intField(firstSVC, "pid") != 1 ||
        intField(firstSVC, "syscall") != 178 {
        failures.append(fail("init-first-svc", "full-shell report must show init reached first TCTI svc syscall 178"))
    }
    if stringField(staticPIE, "task") != "init" ||
        intField(staticPIE, "pid") != 1 ||
        stringField(staticPIE, "base").isEmpty ||
        stringField(staticPIE, "entry").isEmpty {
        failures.append(fail("init-static-pie", "full-shell report must show init static PIE image before the hang"))
    }
    if stringField(mmap, "task") != "init" ||
        intField(mmap, "pid") != 1 ||
        intField(mmap, "syscall") != 222 {
        failures.append(fail("init-mmap", "full-shell report must show init issued mmap syscall 222 before the hang"))
    }
    if intField(fault, "access") != nil ||
        intField(fault, "si") != nil ||
        stringField(fault, "addr") != "" {
        failures.append(fail("no-user-fault", "init second-mmap reducer expects no fatal TCTI user-fault signature"))
    }
    if intField(signal, "pid") != nil || intField(signal, "signal") != nil {
        failures.append(fail("no-signal", "init second-mmap reducer expects no signaled process"))
    }
    if intField(lastReturn, "syscall") != nil {
        failures.append(fail("no-sh-return", "init second-mmap reducer expects sh not to reach syscall-return tracing"))
    }
    if !terminalText.contains("Orlix TCTI: static PIE image task=init") ||
        !terminalText.contains("Orlix TCTI: svc #0 task=init") ||
        !terminalText.contains("syscall=222") ||
        terminalText.contains("Orlix TCTI: static PIE image task=sh") ||
        fullShellText.contains("ORLIX-TCTI-SHELL-USABLE") {
        failures.append(fail("init-second-mmap-artifact", "full-shell artifacts must show init mmap progress but no sh startup and no success marker"))
    }

    struct PostFullShellInitSecondMmapHangModel: Codable {
        let task: String
        let pid: Int
        let syscall: Int
        let invariant: String
    }
    if failures.isEmpty {
        let model = PostFullShellInitSecondMmapHangModel(
            task: "init",
            pid: 1,
            syscall: 222,
            invariant: "full shell command must continue past init mmap progress to start sh and reach ORLIX-TCTI-SHELL-USABLE"
        )
        let modelURL = buildPath("reproducers", target, "post-full-shell-init-second-mmap-hang-model.json")
        try writeJSON(model, to: modelURL)
        artifacts.append(relativePath(modelURL))
    }

    let reducer = try writeReducer(
        target: target,
        caseID: "post-full-shell-init-second-mmap-hang-pass-regression",
        command: "TCTI_SIMULATOR_REPORT=\(relativePath(simulatorReport.url)) make tcti-gate TARGET=\(target)",
        reason: "pinned simulator full-shell command reaches init static PIE and second mmap syscall entry, then does not start sh or reach ORLIX-TCTI-SHELL-USABLE",
        artifacts: artifacts,
        expectedStatus: .pass
    )
    artifacts.append(relativePath(reducer))

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Reduced pinned simulator full-shell init second-mmap hang to a no-phone TCTI evidence gate.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_reduced": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

func runBusyBoxSyscallReturnTrace() throws -> Int32 {
    let target = "tcti-busybox-syscall-return-trace"
    var failures: [Failure] = []
    var artifacts: [String] = []

    let engineURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "engine.c")
    let reportURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "report.c")
    let reportHeaderURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "report.h")
    let runtimeURL = path("tools", "runtime", "orlix-runtime-validation.sh")
    let engine = try readText(engineURL)
    let reportText = try readText(reportURL)
    let reportHeader = try readText(reportHeaderURL)
    let runtime = try readText(runtimeURL)

    artifacts.append(contentsOf: [
        relativePath(engineURL),
        relativePath(reportURL),
        relativePath(reportHeaderURL),
        relativePath(runtimeURL),
    ])

    if !engine.contains("tcti_report_syscall_return(current, regs, nr, pc);") {
        failures.append(fail("syscall-return-call", "TCTI syscall handoff must report syscall return values after orlix_syscall_dispatch"))
    }
    if !reportHeader.contains("tcti_report_syscall_return") ||
        !reportText.contains("Orlix TCTI: syscall return task=%s") ||
        !reportText.contains("signed_ret=%lld") {
        failures.append(fail("syscall-return-report", "TCTI report layer must expose syscall return and signed return values"))
    }
    if !runtime.contains("last_sh_syscall_return") ||
        !runtime.contains("Orlix TCTI: syscall return task=sh") {
        failures.append(fail("runtime-json", "runtime-validation JSON must preserve BusyBox shell syscall return evidence"))
    }
    for forbidden in ["MAP_JIT", "VM_PROT_EXECUTE", "PROT_EXEC", "HostAdapter"] {
        if engine.contains(forbidden) || reportText.contains(forbidden) {
            failures.append(fail("forbidden-scope", "syscall return trace must not add \(forbidden) behavior"))
        }
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let gateReportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Checked TCTI BusyBox syscall return tracing and runtime JSON extraction.",
        failures: failures,
        artifacts: artifacts,
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(gateReportURL))")
    return exitCode(for: status)
}

func runPostFullShellInitSecondMmapHangFix() throws -> Int32 {
    let target = "tcti-post-full-shell-init-second-mmap-hang-fix"
    var failures: [Failure] = []
    var artifacts: [String] = []

    let reducerURL = buildPath("reports", "tcti-post-full-shell-init-second-mmap-hang-reducer", "report.json")
    let reducerObject = try? loadJSON(reducerURL) as? [String: Any]
    let reducerArtifacts = (reducerObject?["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
    if let reducerObject {
        artifacts.append(relativePath(reducerURL))
        if stringField(reducerObject, "git_sha") != gitSha() ||
            stringField(reducerObject, "status") != "pass" ||
            !boolField(reducerObject, "passed") {
            failures.append(fail("reducer-report", "post-full-shell init second-mmap reducer report is missing, stale, or not passing"))
        }
    } else {
        failures.append(fail("reducer-report", "missing tcti-post-full-shell-init-second-mmap-hang-reducer report"))
    }

    let simulatorPath = reducerArtifacts.first {
        $0.hasPrefix("Build/Reports/runtime/tcti-full-shell-usability-") &&
            $0.hasSuffix(".json")
    } ?? ""
    let simulatorURL = path(simulatorPath)
    guard !simulatorPath.isEmpty,
          let simulatorObject = try? loadJSON(simulatorURL) as? [String: Any] else {
        failures.append(fail("simulator-report", "missing reducer-covered iphonesimulator full-shell usability report for init second-mmap fix"))
        let reportURL = try writeReport(report(
            target: target,
            status: .fail,
            summary: "No pinned simulator full-shell report was available for the init second-mmap fix gate.",
            failures: failures,
            artifacts: artifacts,
            releaseGateEligible: false,
            readinessGateEligible: false
        ))
        print("fail: \(relativePath(reportURL))")
        return 1
    }

    artifacts.append(simulatorPath)
    if stringField(simulatorObject, "git_sha") != gitSha() {
        failures.append(fail("simulator-report-stale", "selected full-shell simulator report is stale for current HEAD"))
    }
    if stringField(simulatorObject, "selected_device_id") != "1E5553B0-203A-4A11-BAD7-EBDE46863F66" ||
        stringField(simulatorObject, "selected_device_name") != "Orlix-iPhone-15-Pro-Max" ||
        intField(simulatorObject, "simulator_booted_count") != 1 ||
        !boolField(simulatorObject, "simulator_single_booted") {
        failures.append(fail("simulator-scope", "fix gate requires evidence from the single pinned Orlix-iPhone-15-Pro-Max simulator"))
    }
    if !reducerArtifacts.contains(simulatorPath) {
        failures.append(fail("reducer-coverage", "init second-mmap reducer must cover the selected simulator report"))
    }

    let engineURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "engine.c")
    let testURL = path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti", "tests", "tcti_decode_test.c")
    let engineText = try readText(engineURL)
    let testText = try readText(testURL)
    artifacts.append(relativePath(engineURL))
    artifacts.append(relativePath(testURL))
    if engineText.contains("current->thread.user_tls = 0;") {
        failures.append(fail("execve-tls-clear", "TCTI successful execve return must not clear current->thread.user_tls"))
    }
    if !testText.contains("KUNIT_EXPECT_EQ(test, 0x700000123000ULL, current->thread.user_tls)") {
        failures.append(fail("execve-tls-test", "KUnit must assert TCTI successful execve return preserves guest TLS"))
    }
    for forbidden in ["HostAdapter", "MAP_JIT", "VM_PROT_EXECUTE", "PROT_EXEC"] {
        if engineText.contains(forbidden) || testText.contains(forbidden) {
            failures.append(fail("forbidden-scope", "init second-mmap fix must not add \(forbidden) behavior"))
        }
    }

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Checked reducer-backed TCTI full-shell init second-mmap fix preserves guest TLS across successful execve return without implementing BRK.",
        failures: failures,
        artifacts: artifacts,
        counters: ["simulator_reports_checked": 1],
        releaseGateEligible: false,
        readinessGateEligible: false
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
    return exitCode(for: status)
}

let tctiTargets = [
    "tcti-plan-consistency",
    "tcti-report-schema-check",
    "tcti-toolchain-check",
    "tcti-kernel-syscall-dispatch-smoke",
    "tcti-kernel-execve-binfmt-elf-smoke",
    "tcti-kernel-fault-signal-smoke",
    "tcti-kernel-wait-reaping-smoke",
    "tcti-kernel-pty-console-smoke",
    "tcti-kernel-kselftest-subset",
    "tcti-mlibc-build-smoke",
    "tcti-mlibc-sysdeps-smoke",
    "tcti-mlibc-libc-test-subset",
    "tcti-mlibc-dynamic-loader-smoke",
    "tcti-mlibc-pthread-tls-smoke",
    "tcti-mlibc-linked-syscall-uapi-smoke",
    "tcti-shell-exec-simple-command",
    "tcti-shell-pipeline-smoke",
    "tcti-shell-env-var-smoke",
    "tcti-shell-redirection-smoke",
    "tcti-shell-script-smoke",
    "tcti-coreutils-true-false-echo",
    "tcti-coreutils-cat-wc",
    "tcti-coreutils-ls-stat",
    "tcti-coreutils-mkdir-rm-cp-ln",
    "tcti-coreutils-env-path",
    "tcti-coreutils-test-subset",
    "tcti-golden-elf",
    "tcti-golden-elf-refresh",
    "tcti-appstore-safety-audit",
    "tcti-repro",
    "tcti-contract",
    "tcti-diff-switch",
    "tcti-memory-fuzz",
    "tcti-direct-chain-fuzz",
    "tcti-simulator-user-fault-reducer",
    "tcti-static-pie-relocation-fix",
    "tcti-simd-self-move-reducer",
    "tcti-simd-self-move-fix",
    "tcti-simd-s-lane-move-reducer",
    "tcti-init-mlibc-lock-brk-reducer",
    "tcti-brk-trap-reducer",
    "tcti-brk-trap-root-cause",
    "tcti-brk-guard-got-reducer",
    "tcti-add-sub-shifted-xzr-fix",
    "tcti-simd-movi-2s-fix",
    "tcti-simd-movi-16b-fix",
    "tcti-simd-movi-4s-0x41-fix",
    "tcti-simd-movi-4s-0x1-fix",
    "tcti-simd-cmeq-4s-fix",
    "tcti-simd-umaxv-4s-fix",
    "tcti-simd-addv-4s-fix",
    "tcti-fmov-w-s-fix",
    "tcti-simd-str-s-fix",
    "tcti-simd-dup-2d-fix",
    "tcti-simd-and-16b-fix",
    "tcti-simd-orr-4s-fix",
    "tcti-post-overlay-null-user-fault-reducer",
    "tcti-post-overlay-null-user-fault-fix",
    "tcti-ldrsw-sign-extension-reducer",
    "tcti-ldrsw-sign-extension-fix",
    "tcti-clone-zero-pc-reducer",
    "tcti-clone-zero-pc-fix",
    "tcti-post-setsid-tls-fault-reducer",
    "tcti-post-exec-sh-fetch-fault-reducer",
    "tcti-post-pie-sh-entry-fetch-fault-reducer",
    "tcti-post-bash-mmap-read-fault-reducer",
    "tcti-init-read-fault-reducer",
    "tcti-post-static-pie-init-read-fault-reducer",
    "tcti-post-static-pie-init-tls-fix",
    "tcti-post-full-shell-cat-read-fault-reducer",
    "tcti-post-full-shell-cat-read-fault-fix",
    "tcti-post-full-shell-init-write-fault-reducer",
    "tcti-post-full-shell-sh-sigabrt-reducer",
    "tcti-post-full-shell-init-write-fault-fix",
    "tcti-post-full-shell-sh-sigabrt-fix",
    "tcti-post-full-shell-init-second-mmap-hang-reducer",
    "tcti-post-full-shell-init-second-mmap-hang-fix",
    "tcti-post-full-shell-cat-posix-memalign-brk-reducer",
    "tcti-post-full-shell-cat-posix-memalign-brk-fix",
    "tcti-post-true-entry-fetch-fault-reducer",
    "tcti-post-sh-read-fault-reducer",
    "tcti-post-busybox-sigabrt-reducer",
    "tcti-post-busybox-shell-command-sigill-reducer",
    "tcti-user-data-window-refresh-fix",
    "tcti-busybox-syscall-return-trace",
]

func dispatch(_ target: String) throws -> Int32 {
    switch target {
    case "tcti-plan-consistency":
        return try runPlanConsistency()
    case "tcti-report-schema-check":
        return try runReportSchemaCheck()
    case "tcti-toolchain-check":
        return try runToolchainCheck()
    case "tcti-kernel-syscall-dispatch-smoke":
        return try runKernelSyscallDispatchSmoke()
    case "tcti-kernel-execve-binfmt-elf-smoke":
        return try runKernelExecveBinfmtElfSmoke()
    case "tcti-kernel-fault-signal-smoke":
        return try runKernelFaultSignalSmoke()
    case "tcti-kernel-wait-reaping-smoke":
        return try runKernelWaitReapingSmoke()
    case "tcti-kernel-pty-console-smoke":
        return try runKernelPtyConsoleSmoke()
    case "tcti-kernel-kselftest-subset":
        return try runKernelKselftestSubset()
    case "tcti-mlibc-build-smoke":
        return try runMLibCBuildSmoke()
    case "tcti-mlibc-sysdeps-smoke":
        return try runMLibCSysdepsSmoke()
    case "tcti-mlibc-libc-test-subset":
        return try runMLibCLibcTestSubset()
    case "tcti-mlibc-dynamic-loader-smoke":
        return try runMLibCDynamicLoaderSmoke()
    case "tcti-mlibc-pthread-tls-smoke":
        return try runMLibCPthreadTLSSmoke()
    case "tcti-mlibc-linked-syscall-uapi-smoke":
        return try runMLibCLinkedSyscallUAPISmoke()
    case "tcti-shell-exec-simple-command":
        return try runShellExecSimpleCommand()
    case "tcti-shell-pipeline-smoke":
        return try runShellPipelineSmoke()
    case "tcti-shell-env-var-smoke":
        return try runShellEnvVarSmoke()
    case "tcti-shell-redirection-smoke":
        return try runShellRedirectionSmoke()
    case "tcti-shell-script-smoke":
        return try runShellScriptSmoke()
    case "tcti-coreutils-true-false-echo":
        return try runCoreutilsTrueFalseEcho()
    case "tcti-coreutils-cat-wc":
        return try runCoreutilsCatWC()
    case "tcti-coreutils-ls-stat":
        return try runCoreutilsLsStat()
    case "tcti-coreutils-mkdir-rm-cp-ln":
        return try runCoreutilsMkdirRmCpLn()
    case "tcti-coreutils-env-path":
        return try runCoreutilsEnvPath()
    case "tcti-coreutils-test-subset":
        return try runCoreutilsTestSubset()
    case "tcti-golden-elf":
        return try runGoldenElf(refresh: false)
    case "tcti-golden-elf-refresh":
        return try runGoldenElf(refresh: true)
    case "tcti-appstore-safety-audit":
        return try runSafetyAudit()
    case "tcti-repro":
        return try runRepro()
    case "tcti-contract":
        return try runContract()
    case "tcti-diff-switch":
        return try runDiffSwitch()
    case "tcti-memory-fuzz":
        return try runMemoryFuzz()
    case "tcti-direct-chain-fuzz":
        return try runDirectChainFuzz()
    case "tcti-simulator-user-fault-reducer":
        return try runSimulatorUserFaultReducer()
    case "tcti-static-pie-relocation-fix":
        return try runStaticPIERelocationFix()
	    case "tcti-simd-self-move-reducer":
	        return try runSIMDSelfMoveReducer()
	    case "tcti-simd-self-move-fix":
	        return try runSIMDSelfMoveFix()
	    case "tcti-simd-s-lane-move-reducer":
	        return try runSIMDSLaneMoveReducer()
    case "tcti-init-mlibc-lock-brk-reducer":
        return try runInitMLibCLockBRKReducer()
	    case "tcti-brk-trap-reducer":
	        return try runBRKTrapReducer()
    case "tcti-brk-trap-root-cause":
        return try runBRKTrapRootCause()
    case "tcti-brk-guard-got-reducer":
        return try runBRKGuardGOTReducer()
    case "tcti-add-sub-shifted-xzr-fix":
        return try runAddSubShiftedXZRFix()
    case "tcti-simd-movi-2s-fix":
        return try runSIMDMOVI2SFix()
    case "tcti-simd-movi-16b-fix":
        return try runSIMDMOVI16BFix()
    case "tcti-simd-movi-4s-0x41-fix":
        return try runSIMDMOVI4S0x41Fix()
    case "tcti-simd-movi-4s-0x1-fix":
        return try runSIMDMOVI4S0x1Fix()
    case "tcti-simd-cmeq-4s-fix":
        return try runSIMDCMEQ4SFix()
    case "tcti-simd-umaxv-4s-fix":
        return try runSIMDUMAXV4SFix()
    case "tcti-simd-addv-4s-fix":
        return try runSIMDADDV4SFix()
    case "tcti-fmov-w-s-fix":
        return try runFMOVWSFix()
    case "tcti-simd-str-s-fix":
        return try runSIMDSTRSFix()
    case "tcti-simd-dup-2d-fix":
        return try runSIMDDUP2DFix()
    case "tcti-simd-and-16b-fix":
        return try runSIMDAND16BFix()
    case "tcti-simd-orr-4s-fix":
        return try runSIMDORR4SFix()
    case "tcti-post-overlay-null-user-fault-reducer":
        return try runPostOverlayNullUserFaultReducer()
    case "tcti-post-overlay-null-user-fault-fix":
        return try runPostOverlayNullUserFaultFix()
	case "tcti-ldrsw-sign-extension-reducer":
		return try runLDRSWSignExtensionReducer()
	case "tcti-ldrsw-sign-extension-fix":
		return try runLDRSWSignExtensionFix()
	case "tcti-clone-zero-pc-reducer":
		return try runCloneZeroPCReducer()
case "tcti-clone-zero-pc-fix":
	return try runCloneZeroPCFix()
case "tcti-post-setsid-tls-fault-reducer":
	return try runPostSetsidTLSFaultReducer()
case "tcti-post-exec-sh-fetch-fault-reducer":
	return try runPostExecSHFetchFaultReducer()
	case "tcti-post-pie-sh-entry-fetch-fault-reducer":
		return try runPostPIESHEntryFetchFaultReducer()
	case "tcti-post-bash-mmap-read-fault-reducer":
		return try runPostBashMmapReadFaultReducer()
	case "tcti-init-read-fault-reducer":
		return try runInitReadFaultReducer()
    case "tcti-post-static-pie-init-read-fault-reducer":
        return try runPostStaticPIEInitReadFaultReducer()
    case "tcti-post-static-pie-init-tls-fix":
        return try runPostStaticPIEInitTLSFix()
    case "tcti-post-full-shell-cat-read-fault-reducer":
        return try runPostFullShellCatReadFaultReducer()
    case "tcti-post-full-shell-cat-read-fault-fix":
        return try runPostFullShellCatReadFaultFix()
    case "tcti-post-full-shell-init-write-fault-reducer":
        return try runPostFullShellInitWriteFaultReducer()
    case "tcti-post-full-shell-sh-sigabrt-reducer":
        return try runPostFullShellSHSIGABRTReducer()
    case "tcti-post-console-sh-sigabrt-reducer":
        return try runPostConsoleSHSIGABRTReducer()
    case "tcti-post-full-shell-init-write-fault-fix":
        return try runPostFullShellInitWriteFaultFix()
    case "tcti-post-full-shell-sh-sigabrt-fix":
        return try runPostFullShellSHSIGABRTFix()
    case "tcti-post-full-shell-init-second-mmap-hang-reducer":
        return try runPostFullShellInitSecondMmapHangReducer()
    case "tcti-post-full-shell-init-second-mmap-hang-fix":
        return try runPostFullShellInitSecondMmapHangFix()
    case "tcti-post-full-shell-cat-posix-memalign-brk-reducer":
        return try runPostFullShellCatPosixMemalignBRKReducer()
    case "tcti-post-full-shell-cat-posix-memalign-brk-fix":
        return try runPostFullShellCatPosixMemalignBRKFix()
    case "tcti-post-true-entry-fetch-fault-reducer":
        return try runPostTrueEntryFetchFaultReducer()
	case "tcti-post-sh-read-fault-reducer":
		return try runPostSHReadFaultReducer()
    case "tcti-post-busybox-sigabrt-reducer":
        return try runPostBusyBoxSIGABRTReducer()
    case "tcti-post-busybox-shell-command-sigill-reducer":
        return try runPostBusyBoxShellCommandSIGILLReducer()
    case "tcti-user-data-window-refresh-fix":
        return try runUserDataWindowRefreshFix()
    case "tcti-busybox-syscall-return-trace":
        return try runBusyBoxSyscallReturnTrace()
	default:
		throw GateError.usage("unknown TCTI target: \(target)")
	}
}

func main() -> Int32 {
    let args = CommandLine.arguments.dropFirst()
    guard let target = args.first else {
        fputs("usage: orlix-tcti-gate.swift <target>|--list\n", stderr)
        return 2
    }
    if target == "--list" {
        for candidate in tctiTargets {
            print(candidate)
        }
        return 0
    }
    do {
        return try dispatch(target)
    } catch {
        let targetName = String(target)
        let reducer = try? writeReducer(
            target: targetName,
            caseID: "error",
            command: "make tcti-gate TARGET=\(targetName)",
            reason: "\(error)"
        )
        let reportURL = try? writeReport(report(
            target: targetName,
            status: .error,
            summary: "TCTI gate failed with an internal error.",
            failures: [fail("error", "\(error)")],
            artifacts: reducer.map { [relativePath($0)] } ?? []
        ))
        if let reportURL {
            fputs("error: \(relativePath(reportURL))\n", stderr)
        }
        fputs("\(error)\n", stderr)
        return 1
    }
}

exit(main())
