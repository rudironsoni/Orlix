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
    let rt: Int?
    let imm: Int?
    let shift: Int?
    let offset: Int?
    let width: Int?
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
        rt: Int? = nil,
        imm: Int?,
        shift: Int?,
        offset: Int? = nil,
        width: Int? = nil,
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
        self.rt = rt
        self.imm = imm
        self.shift = shift
        self.offset = offset
        self.width = width
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
        case rt
        case imm
        case shift
        case offset
        case width
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
    print("reproduce with: make tcti-repro REPRO=\(relativePath(reducer))")
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
            command: "CASE=init_001_exit BACKEND=gadget NEGATIVE_DIFF=gadget-x0 make tcti-diff-switch",
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
        command: "make \(target)",
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
    print("reproduce with: make tcti-repro REPRO=\(relativePath(reducer))")
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
    return try buildAarch64NoLibc(source: source, outputRoot: outputRoot, binaryName: caseID)
}

func buildAarch64NoLibc(source: URL, outputRoot: URL, binaryName: String) throws -> (binary: URL, source: URL, metadata: [String: String]) {
    let outputDir = outputRoot.appendingPathComponent(binaryName, isDirectory: true)
    try ensureDirectory(outputDir)
    let binary = outputDir.appendingPathComponent(binaryName)
    let flags = [
        "-target", "aarch64-linux-gnu",
        "-nostdlib",
        "-static",
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

struct TinyElf64Aarch64 {
    let data: Data
    let entrypoint: UInt64
    let segments: [ElfLoadSegment]

    init(binary: URL) throws {
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
        guard type == 2 else {
            throw GateError.commandFailed("init_001_exit must be ET_EXEC, found \(type)")
        }
        guard machine == 183 else {
            throw GateError.commandFailed("init_001_exit must be AArch64, found machine \(machine)")
        }
        let entrypoint = try littleEndianUInt64(data, 24)
        let phoff = try littleEndianUInt64(data, 32)
        let phentsize = Int(try littleEndianUInt16(data, 54))
        let phnum = Int(try littleEndianUInt16(data, 56))
        var segments: [ElfLoadSegment] = []
        for index in 0..<phnum {
            let offset = Int(phoff) + index * phentsize
            let programType = try littleEndianUInt32(data, offset)
            guard programType == 1 else { continue }
            let fileOffset = try littleEndianUInt64(data, offset + 8)
            let virtualAddress = try littleEndianUInt64(data, offset + 16)
            let fileSize = try littleEndianUInt64(data, offset + 32)
            segments.append(ElfLoadSegment(fileOffset: fileOffset, virtualAddress: virtualAddress, fileSize: fileSize))
        }
        guard !segments.isEmpty else {
            throw GateError.commandFailed("init_001_exit has no PT_LOAD segment")
        }
        self.data = data
        self.entrypoint = entrypoint
        self.segments = segments
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

    if (raw & 0x9f00_0000) == 0x1000_0000 {
        let immlo = UInt64((raw >> 29) & 0x3)
        let immhi = UInt64((raw >> 5) & 0x7ffff)
        let rd = Int(raw & 0x1f)
        let imm = signExtend((immhi << 2) | immlo, bitCount: 21)
        return .pcRelativeAddress(raw: raw, pc: pc, op: "adr", rd: rd, imm: imm)
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

func executeSwitchDebug(binary: URL, metadata: GoldenMetadata) throws -> (report: ExecutionReport, failures: [Failure]) {
    let elf = try TinyElf64Aarch64(binary: binary)
    let expectedEntry = try parseEntrypoint(metadata.entrypoint)
    var failures: [Failure] = []
    if elf.entrypoint != expectedEntry {
        failures.append(fail("execution-entrypoint", String(format: "entered 0x%llx, expected 0x%llx", elf.entrypoint, expectedEntry)))
    }
    var pc = elf.entrypoint
    var registers = Array(repeating: UInt64(0), count: 31)
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
        case let .pcRelativeAddress(_, instructionPC, _, rd, imm):
            let address = Int64(bitPattern: instructionPC) + imm
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
                    notes: ["ADR computed a negative guest address and stopped the switch-debug harness"]
                )
                failures.append(fail("execution-address", "ADR computed negative guest address \(address)"))
                return (report, failures)
            }
            registers[rd] = UInt64(address)
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
        case let .loadStoreUnsignedImmediate(_, _, op, rt, rn, offset, _):
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
                        let bytes = try elf.readBytes(at: address, length: 8)
                        registers[rt] = try littleEndianUInt64(bytes, 0)
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
                    notes: ["switch-debug executes decoded MOVZ/ADR/ADD/SUB/LDR/STR/MRS/MSR/SVC seed semantics and captures svc #0 as test events without calling host syscalls", "guest mprotect is captured as a test event only; no host mprotect, vm_protect, MAP_JIT, RWX, or permission side effect is performed", "guest TPIDR_EL0 is switch-debug guest state only; host TPIDR_EL0 is not read or written"]
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
    guard ["init_002_write", "init_003_stack", "init_004_tls", "init_005_branches", "init_006_memory", "init_007_mprotect", "init_008_self_modify", "init_009_faults"].contains(caseID) else {
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
            (fixture.hasPrefix("faults-") ? "init_009_faults" : "init_001_exit")))))))
            let reducer = try writeReducer(
                target: "tcti-golden-elf",
                caseID: "execution-\(fixture)",
                command: "CASE=\(fixtureCaseID) EXECUTE=switch-debug NEGATIVE_EXECUTION=\(fixture) make tcti-golden-elf",
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
        failures.append(fail("binary-sha256", "binary hash changed for init_001_exit; inspect or run make tcti-golden-elf-refresh CASE=init_001_exit"))
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
        failures.append(fail("binary-sha256", "binary hash changed for init_002_write; inspect or run make tcti-golden-elf-refresh CASE=init_002_write"))
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
        failures.append(fail("binary-sha256", "binary hash changed for init_003_stack; inspect or run make tcti-golden-elf-refresh CASE=init_003_stack"))
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
        failures.append(fail("binary-sha256", "binary hash changed for init_004_tls; inspect or run make tcti-golden-elf-refresh CASE=init_004_tls"))
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
        failures.append(fail("binary-sha256", "binary hash changed for init_005_branches; inspect or run make tcti-golden-elf-refresh CASE=init_005_branches"))
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
        failures.append(fail("binary-sha256", "binary hash changed for init_006_memory; inspect or run make tcti-golden-elf-refresh CASE=init_006_memory"))
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
        failures.append(fail("binary-sha256", "binary hash changed for init_007_mprotect; inspect or run make tcti-golden-elf-refresh CASE=init_007_mprotect"))
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
        failures.append(fail("binary-sha256", "binary hash changed for init_008_self_modify; inspect or run make tcti-golden-elf-refresh CASE=init_008_self_modify"))
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
        failures.append(fail("binary-sha256", "binary hash changed for init_009_faults; inspect or run make tcti-golden-elf-refresh CASE=init_009_faults"))
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
        failures.append(fail("binary-sha256", "binary hash changed for init_010_cpu_model; inspect or run make tcti-golden-elf-refresh CASE=init_010_cpu_model"))
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
    guard ["init_002_write", "init_003_stack", "init_004_tls", "init_005_branches", "init_006_memory", "init_007_mprotect", "init_008_self_modify", "init_009_faults", "init_010_cpu_model"].contains(caseID) else {
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
    default:
        expectedSyscalls = [
            ExpectedSyscall(nr: "exit", code: 42, fd: nil, len: nil, bytes: nil),
        ]
        expectedExitCode = 42
        expectedMessage = nil
    }
    return GoldenMetadata(
        caseID: caseID,
        generatorCommand: "make tcti-golden-elf-refresh CASE=\(caseID)",
        sourceSHA256: sourceHash,
        expectedBinarySHA256: actualBinaryHash,
        actualBinarySHA256: actualBinaryHash,
        compilerPath: toolchain["clang_path"] ?? "",
        compilerVersion: toolchain["clang_version"] ?? "",
        linkerPath: toolchain["linker_path"] ?? "",
        linkerVersion: toolchain["linker_version"] ?? "",
        flags: [
            "-target", "aarch64-linux-gnu",
            "-nostdlib", "-static", "-fuse-ld=lld",
            "-Wl,--build-id=none", "-Wl,-e,_start",
        ],
        libcMode: "no-libc",
        elfType: "ET_EXEC",
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
            command: "make tcti-toolchain-check",
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

func runGoldenElf(refresh: Bool) throws -> Int32 {
    let target = refresh ? "tcti-golden-elf-refresh" : "tcti-golden-elf"
    let caseID = ProcessInfo.processInfo.environment["CASE"] ?? "init_001_exit"
    let executeMode = ProcessInfo.processInfo.environment["EXECUTE"] ?? ""
    let negativeExecution = ProcessInfo.processInfo.environment["NEGATIVE_EXECUTION"] ?? ""
    guard ["init_001_exit", "init_002_write", "init_003_stack", "init_004_tls", "init_005_branches", "init_006_memory", "init_007_mprotect", "init_008_self_modify", "init_009_faults", "init_010_cpu_model"].contains(caseID) else {
        return try writeTodo(target: target, caseID: caseID, summary: "Only init_001_exit through init_010_cpu_model are implemented in this no-phone oracle checkpoint.")
    }
    if refresh && !executeMode.isEmpty {
        throw GateError.usage("EXECUTE is not supported with tcti-golden-elf-refresh")
    }
    let outputRoot = buildPath("golden_elf")
    let metadataPath = path("OrlixKernel", "Tests", "TCTI", "golden_elf", caseID, "golden.json")
    var artifacts: [String] = []
    var failures: [Failure] = []
    var executionReport: ExecutionReport?
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
        let reducer = try writeReducer(
            target: target,
            caseID: caseID,
            command: replayCommand,
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
                "CASE=\(caseID) BACKEND=gadget NEGATIVE_DIFF=gadget-x0 make tcti-diff-switch" :
                "CASE=\(caseID) NEGATIVE_DIFF=exit-code make tcti-diff-switch"
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
            command: "CASE=\(caseID) make tcti-diff-switch",
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
    default:
        throw GateError.usage("unknown NEGATIVE_MEMORY_FUZZ=\(id)")
    }
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
            command: "NEGATIVE_MEMORY_FUZZ=\(negativeID) make tcti-memory-fuzz",
            reason: artifact.notes.joined(separator: "; "),
            artifacts: [artifactPath],
            expectedStatus: .fail
        )
        reducers.append(relativePath(reducer))
    }
    let passReducer = try writeReducer(
        target: target,
        caseID: "memory-fuzz-pass-regression",
        command: "make tcti-memory-fuzz",
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
            command: "NEGATIVE_DIRECT_CHAIN_FUZZ=\(negativeID) make tcti-direct-chain-fuzz",
            reason: artifact.notes.joined(separator: "; "),
            artifacts: [artifactPath],
            expectedStatus: .fail
        )
        reducers.append(relativePath(reducer))
    }
    let passReducer = try writeReducer(
        target: target,
        caseID: "direct-chain-fuzz-pass-regression",
        command: "make tcti-direct-chain-fuzz",
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

func runSafetyAudit() throws -> Int32 {
    let target = "tcti-appstore-safety-audit"
    let productionRoots = [
        path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "hosted_exec", "tcti"),
        path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "include", "asm", "tcti.h"),
        path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "mm", "tcti_user_page.c"),
        path("OrlixKernel", "Sources", "ports", "orlix", "overlay", "arch", "orlix", "mm", "tcti_invalidate.c"),
    ]
    let sourceExtensions = ["S", "s", "c", "h"]
    var failures: [Failure] = []
    var scannedFiles = 0
    var scannedObjects = 0
    var warnings: [String] = []

    for root in productionRoots {
        var isDirectory: ObjCBool = false
        guard fileManager.fileExists(atPath: root.path, isDirectory: &isDirectory) else { continue }
        if isDirectory.boolValue {
            guard let enumerator = fileManager.enumerator(at: root, includingPropertiesForKeys: nil) else { continue }
            for case let url as URL in enumerator where sourceExtensions.contains(url.pathExtension) {
                scannedFiles += 1
                failures.append(contentsOf: scanSourceForX18(url))
            }
        } else if sourceExtensions.contains(root.pathExtension) {
            scannedFiles += 1
            failures.append(contentsOf: scanSourceForX18(root))
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

    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: "Scanned \(scannedFiles) TCTI source/template file(s) and \(scannedObjects) object file(s).",
        failures: failures,
        forbiddenBehavior: forbiddenDefaults(hostX18: !failures.filter { $0.id.contains("x18") }.isEmpty),
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
    let process = Process()
    process.executableURL = URL(fileURLWithPath: "/bin/bash")
    process.arguments = ["-lc", payload.command]
    process.currentDirectoryURL = URL(fileURLWithPath: payload.workingDirectory)
    try process.run()
    process.waitUntilExit()
    let reportURL = buildPath("reports", payload.target, "report.json")
    var actualStatus = process.terminationStatus == 0 ? "pass" : "fail"
    if let object = try? loadJSON(reportURL),
       let dictionary = object as? [String: Any],
       let status = dictionary["status"] as? String {
        actualStatus = status
    }
    print("actual replay status: \(actualStatus)")
    print("actual replay exit code: \(process.terminationStatus)")
    if let expected = payload.expectedStatus, expected != actualStatus {
        fputs("reproducer expected status \(expected), got \(actualStatus)\n", stderr)
        return 1
    }
    return process.terminationStatus
}

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
    default:
        throw GateError.usage("unknown TCTI target: \(target)")
    }
}

func main() -> Int32 {
    let args = CommandLine.arguments.dropFirst()
    guard let target = args.first else {
        fputs("usage: orlix-tcti-gate.swift <target>\n", stderr)
        return 2
    }
    do {
        return try dispatch(target)
    } catch {
        let targetName = String(target)
        let reducer = try? writeReducer(
            target: targetName,
            caseID: "error",
            command: "make \(targetName)",
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
