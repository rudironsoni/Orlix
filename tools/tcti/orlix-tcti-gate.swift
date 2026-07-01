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
    }
}

struct Reproducer: Codable {
    let target: String
    let caseID: String
    let command: String
    let workingDirectory: String
    let artifacts: [String]
    let reason: String

    enum CodingKeys: String, CodingKey {
        case target
        case caseID = "case_id"
        case command
        case workingDirectory = "working_directory"
        case artifacts
        case reason
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
        case forbiddenBehavior = "forbidden_behavior"
    }
}

struct ExpectedSyscall: Codable {
    let nr: String
    let code: Int?
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
    autonomousTestsBypassed: Bool = false,
    bypassReason: String = ""
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
        releaseGateEligible: status.gateEligible,
        readinessGateEligible: status.gateEligible,
        autonomousTestsBypassed: autonomousTestsBypassed,
        bypassReason: bypassReason,
        coverageWarnings: coverageWarnings
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
func writeReducer(target: String, caseID: String, command: String, reason: String, artifacts: [String] = []) throws -> URL {
    let reducer = Reproducer(
        target: target,
        caseID: caseID,
        command: command,
        workingDirectory: repoRoot().path,
        artifacts: artifacts,
        reason: reason
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
    let reducer = try writeReducer(
        target: target,
        caseID: caseID,
        command: "make \(target)",
        reason: summary
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
    let outputDir = outputRoot.appendingPathComponent("init_001_exit", isDirectory: true)
    try ensureDirectory(outputDir)
    let binary = outputDir.appendingPathComponent("init_001_exit")
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
    return (
        binary,
        source,
        [
            "source_sha256": try sha256(source),
            "binary_sha256": try sha256(binary),
            "file_output": fileOutput,
            "objdump_header": objdumpOutput,
            "flags": flags.joined(separator: " "),
        ]
    )
}

func goldenMetadata(actualBinaryHash: String, sourceHash: String, toolchain: [String: String]) -> GoldenMetadata {
    GoldenMetadata(
        caseID: "init_001_exit",
        generatorCommand: "make tcti-golden-elf-refresh CASE=init_001_exit",
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
        entrypoint: "0x0000000000210120",
        machine: "AArch64",
        expectedSyscalls: [ExpectedSyscall(nr: "exit", code: 42)],
        expectedExitCode: 42,
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
    guard caseID == "init_001_exit" else {
        return try writeTodo(target: target, caseID: caseID, summary: "Only init_001_exit is implemented in this rails checkpoint.")
    }
    let outputRoot = buildPath("golden_elf")
    let metadataPath = path("OrlixKernel", "Tests", "TCTI", "golden_elf", caseID, "golden.json")
    var artifacts: [String] = []
    var failures: [Failure] = []
    do {
        let toolchain = try toolchainInfo()
        let built = try buildInit001(outputRoot: outputRoot)
        artifacts.append(relativePath(built.binary))
        let sourceHash: String
        if let recordedSourceHash = built.metadata["source_sha256"] {
            sourceHash = recordedSourceHash
        } else {
            sourceHash = try sha256(built.source)
        }
        let binaryHash: String
        if let recordedBinaryHash = built.metadata["binary_sha256"] {
            binaryHash = recordedBinaryHash
        } else {
            binaryHash = try sha256(built.binary)
        }
        if refresh {
            let metadata = goldenMetadata(actualBinaryHash: binaryHash, sourceHash: sourceHash, toolchain: toolchain)
            try writeJSON(metadata, to: metadataPath)
            artifacts.append(relativePath(metadataPath))
        } else {
            let expected = try decoder.decode(GoldenMetadata.self, from: Data(contentsOf: metadataPath))
            if expected.sourceSHA256 != sourceHash {
                failures.append(fail("source-sha256", "source hash changed for \(caseID)"))
            }
            if expected.expectedBinarySHA256 != binaryHash {
                failures.append(fail("binary-sha256", "binary hash changed for \(caseID); inspect or run make tcti-golden-elf-refresh CASE=\(caseID)"))
            }
        }
    } catch {
        failures.append(fail("golden-elf", "\(error)"))
    }
    if !failures.isEmpty {
        let reducer = try writeReducer(
            target: target,
            caseID: caseID,
            command: "make \(target) CASE=\(caseID)",
            reason: failures.map(\.message).joined(separator: "; "),
            artifacts: artifacts
        )
        artifacts.append(relativePath(reducer))
    }
    let status: GateStatus = failures.isEmpty ? .pass : .fail
    let reportURL = try writeReport(report(
        target: target,
        status: status,
        summary: refresh ? "Refreshed \(caseID) golden metadata." : "Built and verified \(caseID) golden ELF.",
        failures: failures,
        artifacts: artifacts
    ))
    print("\(status.rawValue): \(relativePath(reportURL))")
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
                let disassembly = try run(["xcrun", "llvm-objdump", "-d", url.path])
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
    print("case_id: \(payload.caseID)")
    print("command: \(payload.command)")
    print("reason: \(payload.reason)")
    let process = Process()
    process.executableURL = URL(fileURLWithPath: "/bin/bash")
    process.arguments = ["-lc", payload.command]
    process.currentDirectoryURL = URL(fileURLWithPath: payload.workingDirectory)
    try process.run()
    process.waitUntilExit()
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
    case "tcti-contract", "tcti-diff-switch", "tcti-memory-fuzz", "tcti-direct-chain-fuzz":
        return try writeTodo(target: target, summary: "\(target) rail exists, but the real no-phone TCTI test implementation is not complete yet.")
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
