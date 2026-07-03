#!/usr/bin/env swift
import Foundation

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

struct Report: Codable {
    let target: String
    let gate: String
    let status: String
    let passed: Bool
    let summary: String
    let gitSha: String
    let backend: String
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
    let expectedStatus: String?
    let actualReplayStatus: String?
    let execution: ExecutionReport?

    enum CodingKeys: String, CodingKey {
        case target
        case gate
        case status
        case passed
        case summary
        case gitSha = "git_sha"
        case backend
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

func runWithFileBackedOutput(_ arguments: [String], check: Bool = true) throws -> String {
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
    process.waitUntilExit()

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

func report(
    target: String,
    status: GateStatus,
    summary: String,
    failures: [Failure] = [],
    artifacts: [String] = [],
    forbiddenBehavior: [String: Bool] = forbiddenDefaults(),
    counters: [String: Int] = [:],
    coverageWarnings: [String] = [],
    releaseGateEligible: Bool? = nil,
    readinessGateEligible: Bool? = nil,
    autonomousTestsBypassed: Bool = false,
    bypassReason: String = "",
    expectedStatus: String? = nil,
    actualReplayStatus: String? = nil,
    execution: ExecutionReport? = nil
) -> Report {
    Report(
        target: target,
        gate: target,
        status: status.rawValue,
        passed: status.passed,
        summary: summary,
        gitSha: gitSha(),
        backend: "tcti",
        virtualCpuModel: "orlix-aarch64-v1",
        hostPageSize: hostPageSize(),
        guestPageSize: 4096,
        forbiddenBehavior: forbiddenBehavior,
        counters: counters,
        failures: failures,
        artifacts: artifacts,
        releaseGateEligible: releaseGateEligible ?? status.gateEligible,
        readinessGateEligible: readinessGateEligible ?? status.gateEligible,
        autonomousTestsBypassed: autonomousTestsBypassed,
        bypassReason: bypassReason,
        coverageWarnings: coverageWarnings,
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
    - virtual CPU: `\(value.virtualCpuModel)`

    ## Summary

    \(value.summary)

    ## Failures

    \(value.failures.isEmpty ? "- none" : value.failures.map { "- `\($0.id)`: \($0.message)" }.joined(separator: "\n"))

    ## Artifacts

    \(value.artifacts.isEmpty ? "- none" : value.artifacts.map { "- `\($0)`" }.joined(separator: "\n"))

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
    let command = caseID == "todo" ? "make \(target)" : "CASE=\(caseID) make \(target)"
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

func validateReportObject(_ object: Any) -> [String] {
    guard let dictionary = object as? [String: Any] else {
        return ["report must be a JSON object"]
    }
    let required: [(String, Any.Type)] = [
        ("target", String.self),
        ("gate", String.self),
        ("status", String.self),
        ("passed", Bool.self),
        ("summary", String.self),
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
    if status == nil || !allowed.contains(status!) {
        errors.append("status must be one of \(allowed)")
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

func checkReportFile(_ url: URL) -> [String] {
    do {
        return validateReportObject(try loadJSON(url)).map { "\(relativePath(url)): \($0)" }
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
        _ = try run(["make", "tcti-repro", "REPRO=\(relativePath(reducer))"])
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
                command: "make \(target)",
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

    let passFixture = fixtureRoot.appendingPathComponent("report.pass.json")
    checked.append(relativePath(passFixture))
    failures.append(contentsOf: checkReportFile(passFixture).map { fail("schema", $0) })

    for name in ["report.fail.missing-field.json", "report.fail.invalid-status.json"] {
        let fixture = fixtureRoot.appendingPathComponent(name)
        checked.append(relativePath(fixture))
        if checkReportFile(fixture).isEmpty {
            failures.append(fail("schema-fixture", "\(relativePath(fixture)) was expected to fail validation"))
        }
    }

    let tctiReportRoot = buildPath("reports")
    if let enumerator = fileManager.enumerator(at: tctiReportRoot, includingPropertiesForKeys: nil) {
        for case let url as URL in enumerator where url.lastPathComponent == "report.json" {
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

	if (raw & 0xffff_ffe0) == 0x4f01_1600 {
		let rd = Int(raw & 0x1f)
		return .simdVectorLogicalImmediate(raw: raw, pc: pc, op: "orr", rd: rd, imm: 0x0000003000000030, width: 128)
	}

	if (raw & 0xffff_ffe0) == 0x4f06_e7e0 {
		let rd = Int(raw & 0x1f)
		return .simdModifiedImmediate(raw: raw, pc: pc, op: "movi", rd: rd, imm: 0xdfdfdfdfdfdfdfdf, width: 128)
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
    if let reducerReport = try? loadJSON(reducerReportURL) as? [String: Any] {
        artifacts.append(relativePath(reducerReportURL))
        if stringField(reducerReport, "status") != "pass" ||
            !boolField(reducerReport, "passed") {
            failures.append(fail("reducer-report", "static PIE GOT null-read reducer report is missing or not passing"))
        }
    } else {
        failures.append(fail("reducer-report", "missing tcti-simulator-user-fault-reducer report"))
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
    if stringField(simulatorObject, "selected_device_id") != "C47ED88D-0D0A-420D-8C78-D4C1D34A276D" ||
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
    if stringField(simulatorObject, "selected_device_id") != "C47ED88D-0D0A-420D-8C78-D4C1D34A276D" ||
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
        command: "make \(target)",
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

    let reducerReportURL = buildPath("reports", "tcti-ldrsw-sign-extension-reducer", "report.json")
    if let reducerReport = try? loadJSON(reducerReportURL) as? [String: Any] {
        artifacts.append(relativePath(reducerReportURL))
        if stringField(reducerReport, "git_sha") != gitSha() ||
            stringField(reducerReport, "status") != "pass" ||
            !boolField(reducerReport, "passed") {
            failures.append(fail("reducer-report", "tcti-ldrsw-sign-extension-reducer report is missing, stale, or not passing"))
        }
    } else {
        failures.append(fail("reducer-report", "missing tcti-ldrsw-sign-extension-reducer report"))
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
		gate: "tcti-simulator-stability",
		destination: "iphonesimulator"
	) else {
		failures.append(fail("simulator-report", "missing iphonesimulator tcti-simulator-stability report"))
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
	if stringField(simulatorObject, "selected_device_id") != "C47ED88D-0D0A-420D-8C78-D4C1D34A276D" ||
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
		command: "make \(target)",
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
	if stringField(simulatorObject, "selected_device_id") != "C47ED88D-0D0A-420D-8C78-D4C1D34A276D" ||
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
		command: "make \(target)",
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
	if stringField(simulatorObject, "selected_device_id") != "C47ED88D-0D0A-420D-8C78-D4C1D34A276D" ||
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
		command: "make \(target)",
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
	if stringField(simulatorObject, "selected_device_id") != "C47ED88D-0D0A-420D-8C78-D4C1D34A276D" ||
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
		command: "make \(target)",
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
		gate: "tcti-simulator-stability",
		destination: "iphonesimulator"
	) else {
		failures.append(fail("simulator-report", "missing iphonesimulator tcti-simulator-stability report"))
		let reportURL = try writeReport(report(
			target: target,
			status: .fail,
			summary: "No simulator stability failure report was available for the post-Bash mmap/read reducer.",
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
	if stringField(simulatorObject, "selected_device_id") != "C47ED88D-0D0A-420D-8C78-D4C1D34A276D" ||
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
		command: "TCTI_SIMULATOR_REPORT=\(relativePath(simulatorReport.url)) make \(target)",
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
        if stringField(reducerReport, "git_sha") != gitSha() ||
            stringField(reducerReport, "status") != "pass" ||
            !boolField(reducerReport, "passed") {
            failures.append(fail("reducer-report", "SIMD self-move reducer report is missing, stale, or not passing"))
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
    if stringField(simulatorObject, "selected_device_id") != "C47ED88D-0D0A-420D-8C78-D4C1D34A276D" {
        failures.append(fail("simulator-device", "BRK root-cause inspection must use Orlix-iPhone-15-Pro-Max simulator C47ED88D-0D0A-420D-8C78-D4C1D34A276D"))
    }
    if stringField(simulatorObject, "status") != "fail" || boolField(simulatorObject, "passed") {
        failures.append(fail("simulator-report-status", "BRK root-cause inspection requires the current failing simulator stability report"))
    }
    if !simulatorText.contains("syscall=222") ||
        brkLine == nil ||
        !(brkLine?.contains("x8=0x0") ?? false) ||
        !(simulatorText.contains("Attempted to kill init") || simulatorText.contains("Attempted kill init")) {
        failures.append(fail("simulator-brk-signature", "simulator artifacts do not show mmap(222), BRK #1, x8=0, and init-kill panic"))
    }

    if !fileManager.fileExists(atPath: binaryURL.path) {
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
                "--start-address=0x1be14",
                "--stop-address=0x1be28",
                binaryURL.path,
            ])
            relocations = try runWithFileBackedOutput([objdump, "-R", binaryURL.path])
        } catch {
            failures.append(fail("llvm-objdump", "failed to inspect runtime init ELF: \(error)"))
            disassembly = ""
            relocations = ""
        }

        if !disassembly.contains("90000148") ||
            !disassembly.contains("f941a508") ||
            !disassembly.contains("b5000048") ||
            !disassembly.contains("d4200020") ||
            !disassembly.contains("adrp") ||
            !disassembly.contains("ldr") ||
            !disassembly.contains("cbnz") ||
            !disassembly.contains("brk") {
            failures.append(fail("brk-disassembly-shape", "runtime init disassembly does not match ADRP/LDR/CBNZ/BRK guard at ELF VMA 0x1be14..0x1be20"))
        }

        if !relocations.contains("0000000000043340 R_AARCH64_RELATIVE") ||
            !relocations.contains("0000000000043350 R_AARCH64_RELATIVE") ||
            relocations.contains("0000000000043348 R_AARCH64_RELATIVE") {
            failures.append(fail("brk-got-relocation-shape", "runtime init relocations must show neighboring GOT relocations but no relocation for GOT slot 0x43348"))
        }

        do {
            let data = try Data(contentsOf: binaryURL)
            let gotFileOffset = 0x23348
            if data.count < gotFileOffset + 8 {
                failures.append(fail("brk-got-file-offset", "runtime init ELF is too small to contain GOT file offset 0x23348"))
            } else {
                let slot = data[gotFileOffset..<gotFileOffset + 8]
                if slot.contains(where: { $0 != 0 }) {
                    failures.append(fail("brk-got-slot", "GOT slot 0x43348 file bytes are not zero"))
                }
            }
        } catch {
            failures.append(fail("brk-got-slot", "failed to read runtime init GOT slot: \(error)"))
        }

        let rootCauseURL = buildPath("brk_trap_root_cause", "root-cause.md")
        let rootCauseMarkdown = """
        # TCTI BRK Trap Root Cause

        - simulator: Orlix-iPhone-15-Pro-Max `C47ED88D-0D0A-420D-8C78-D4C1D34A276D`
        - runtime BRK PC: `\(runtimeBRKPC)`
        - ELF BRK VMA: `0x1be20`
        - instruction: `0xd4200020`, `brk #0x1`
        - guard sequence: `adrp x8, 0x43000`; `ldr x8, [x8, #0x348]`; `cbnz x8, 0x1be24`; `brk #0x1`
        - guarded GOT slot: ELF VMA `0x43348`, file offset `0x23348`
        - GOT slot file bytes: zero
        - relocation record for `0x43348`: absent
        - neighboring relocations: `0x43340` and `0x43350` are `R_AARCH64_RELATIVE`
        - conclusion: this is an explicit runtime startup guard/trap, not a missing successful BRK semantic

        ## Disassembly

        ```text
        \(disassembly)
        ```

        ## Relocations Around 0x43348

        ```text
        \(relocations.split(separator: "\n").filter { $0.contains("00000000000433") }.joined(separator: "\n"))
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
    if stringField(simulatorObject, "selected_device_id") != "C47ED88D-0D0A-420D-8C78-D4C1D34A276D" {
        failures.append(fail("simulator-device", "BRK guard reducer must use Orlix-iPhone-15-Pro-Max simulator C47ED88D-0D0A-420D-8C78-D4C1D34A276D"))
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
        command: "make \(target)",
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
        replayCommand += " make \(target)"
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
            command: "CASE=init_003_stack EXECUTE=switch-debug make \(target)",
            reason: "init_003_stack switch-debug pass regression: stack STR/LDR execution must continue to exit(42)",
            artifacts: focusedPassArtifacts,
            expectedStatus: .pass
        )
        artifacts.append(relativePath(reducer))
    } else if caseID == "init_011_static_pie_got_byte_load" && executeMode == "switch-debug" && negativeExecution.isEmpty {
        let reducer = try writeReducer(
            target: target,
            caseID: "init_011_static_pie_got_byte_load-switch-debug-pass-regression",
            command: "CASE=init_011_static_pie_got_byte_load EXECUTE=switch-debug make \(target)",
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
            command: "make \(target)",
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
                command: "make \(target)",
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
        command: "make \(target)",
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

    if let simulatorReport = latestRuntimeValidationReport(
        gate: "tcti-simulator-stability",
        destination: "iphonesimulator"
    ) {
        artifacts.append(relativePath(simulatorReport.url))
        let simulatorArtifacts = (simulatorReport.object["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
        artifacts.append(contentsOf: simulatorArtifacts)
        let unifiedText = try simulatorArtifacts.first { $0.hasSuffix("simulator-unified.log") }.map(readRelativeArtifact) ?? ""
        let unsupportedLine = unifiedText
            .split(separator: "\n")
            .map(String.init)
            .last { $0.contains("Orlix TCTI: unsupported instruction") && $0.contains("insn=0xf002420") } ?? ""

        if stringField(simulatorReport.object, "git_sha") != gitSha() {
            failures.append(fail("simulator-report-stale", "latest simulator stability report is stale for current HEAD"))
        }
        if stringField(simulatorReport.object, "status") != "fail" || boolField(simulatorReport.object, "passed") {
            failures.append(fail("simulator-report-status", "SIMD MOVI 2S fix requires the current simulator failure report"))
        }
        if !unsupportedLine.contains("pc=") || !unsupportedLine.contains("insn=0xf002420") {
            failures.append(fail("simulator-unsupported-signature", "latest simulator log does not contain unsupported MOVI v0.2s instruction 0x0f002420"))
        }
    } else {
        failures.append(fail("simulator-report", "missing iphonesimulator tcti-simulator-stability report"))
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
        command: "make \(target)",
        reason: "The simulator reached MOVI v0.2s #0x1 lsl #8; support only the emitted modified-immediate subset.",
        artifacts: artifacts,
        expectedStatus: .pass
    )
    artifacts.append(relativePath(reducer))

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Bound simulator unsupported 0x0f002420 to the exact SIMD MOVI v0.2s #0x100 semantic subset.",
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

func runSIMDSTRSFix() throws -> Int32 {
    let target = "tcti-simd-str-s-fix"
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
        let unsupportedLine = unifiedText
            .split(separator: "\n")
            .map(String.init)
            .last { $0.contains("Orlix TCTI: unsupported instruction") && $0.contains("insn=0xbd01c260") } ?? ""

        if stringField(simulatorReport.object, "git_sha") != gitSha() {
            failures.append(fail("simulator-report-stale", "latest simulator stability report is stale for current HEAD"))
        }
        if stringField(simulatorReport.object, "status") != "fail" || boolField(simulatorReport.object, "passed") {
            failures.append(fail("simulator-report-status", "SIMD STR S fix requires the current simulator failure report"))
        }
        if !unsupportedLine.contains("pc=") ||
            !unsupportedLine.contains("insn=0xbd01c260") ||
            !unsupportedLine.contains("x19=") {
            failures.append(fail("simulator-unsupported-signature", "latest simulator log does not contain unsupported STR s0 instruction 0xbd01c260 with base register context"))
        }
    } else {
        failures.append(fail("simulator-report", "missing iphonesimulator tcti-simulator-stability report"))
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
        command: "make \(target)",
        reason: "The simulator reached STR s0, [x19, #0x1c0]; support only the emitted SIMD/FP S-register unsigned-immediate memory width.",
        artifacts: artifacts,
        expectedStatus: .pass
    )
    artifacts.append(relativePath(reducer))

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Bound simulator unsupported 0xbd01c260 to the exact SIMD/FP STR s0 unsigned-immediate semantic subset.",
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

    if let simulatorReport = latestRuntimeValidationReport(
        gate: "tcti-simulator-stability",
        destination: "iphonesimulator"
    ) {
        artifacts.append(relativePath(simulatorReport.url))
        let simulatorArtifacts = (simulatorReport.object["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
        artifacts.append(contentsOf: simulatorArtifacts)
        let unifiedText = try simulatorArtifacts.first { $0.hasSuffix("simulator-unified.log") }.map(readRelativeArtifact) ?? ""
        let unsupportedLine = unifiedText
            .split(separator: "\n")
            .map(String.init)
            .last { $0.contains("Orlix TCTI: unsupported instruction") && $0.contains("insn=0x4e080d80") } ?? ""

        if stringField(simulatorReport.object, "git_sha") != gitSha() {
            failures.append(fail("simulator-report-stale", "latest simulator stability report is stale for current HEAD"))
        }
        if stringField(simulatorReport.object, "status") != "fail" || boolField(simulatorReport.object, "passed") {
            failures.append(fail("simulator-report-status", "SIMD DUP 2D fix requires the current simulator failure report"))
        }
        if !unsupportedLine.contains("pc=") ||
            !unsupportedLine.contains("insn=0x4e080d80") ||
            !unsupportedLine.contains("x12=") {
            failures.append(fail("simulator-unsupported-signature", "latest simulator log does not contain unsupported DUP v0.2d, x12 instruction 0x4e080d80 with source register context"))
        }
    } else {
        failures.append(fail("simulator-report", "missing iphonesimulator tcti-simulator-stability report"))
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
        command: "make \(target)",
        reason: "The simulator reached DUP v0.2d, x12; support only the emitted 2D GPR-to-vector duplication subset.",
        artifacts: artifacts,
        expectedStatus: .pass
    )
    artifacts.append(relativePath(reducer))

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Bound simulator unsupported 0x4e080d80 to the exact SIMD DUP v0.2d, x12 semantic subset.",
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
        command: "make \(target)",
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
        command: "make \(target)",
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

let tctiTargets = [
    "tcti-plan-consistency",
    "tcti-report-schema-check",
    "tcti-toolchain-check",
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
    "tcti-brk-trap-reducer",
    "tcti-brk-trap-root-cause",
    "tcti-brk-guard-got-reducer",
    "tcti-add-sub-shifted-xzr-fix",
    "tcti-simd-movi-2s-fix",
    "tcti-simd-movi-16b-fix",
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
]

func dispatch(_ target: String) throws -> Int32 {
    switch target {
    case "tcti-plan-consistency":
        return try runPlanConsistency()
    case "tcti-report-schema-check":
        return try runReportSchemaCheck()
    case "tcti-toolchain-check":
        return try runToolchainCheck()
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
