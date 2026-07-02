#!/usr/bin/env swift
import Foundation

struct Roadmap: Decodable {
    let area: String
    let description: String
    let gates: [Gate]
}

struct Gate: Codable {
    let id: String
    let command: String
    let kind: String
    let prerequisites: [String]
    let allowedScope: [String]
    let forbiddenScope: [String]
    let expectedReportPaths: [String]
    let readinessEligible: Bool
    let physicalDevice: Bool
    let gadget: Bool
    let requiredValidationCommands: [String]
    let reducerRequirements: [String]
    let requiredSubagentsOrSkills: [String]
    let commitMessageTemplate: String
    let stopConditions: [String]

    enum CodingKeys: String, CodingKey {
        case id
        case command
        case kind
        case prerequisites
        case allowedScope = "allowed_scope"
        case forbiddenScope = "forbidden_scope"
        case expectedReportPaths = "expected_report_paths"
        case readinessEligible = "readiness_eligible"
        case physicalDevice = "physical_device"
        case gadget
        case requiredValidationCommands = "required_validation_commands"
        case reducerRequirements = "reducer_requirements"
        case requiredSubagentsOrSkills = "required_subagents_or_skills"
        case commitMessageTemplate = "commit_message_template"
        case stopConditions = "stop_conditions"
    }
}

struct ReportFact: Codable {
    let path: String
    let exists: Bool
    let status: String
    let passed: Bool
    let releaseGateEligible: Bool
    let readinessGateEligible: Bool
    let gitSHA: String?

    enum CodingKeys: String, CodingKey {
        case path
        case exists
        case status
        case passed
        case releaseGateEligible = "release_gate_eligible"
        case readinessGateEligible = "readiness_gate_eligible"
        case gitSHA = "git_sha"
    }
}

struct GateStatus: Codable {
    let id: String
    let command: String
    let kind: String
    let state: String
    let passed: Bool
    let reason: String
    let prerequisites: [String]
    let prerequisitesSatisfied: Bool
    let reportPaths: [String]
    let reports: [ReportFact]
    let readinessEligible: Bool
    let physicalDevice: Bool
    let gadget: Bool

    enum CodingKeys: String, CodingKey {
        case id
        case command
        case kind
        case state
        case passed
        case reason
        case prerequisites
        case prerequisitesSatisfied = "prerequisites_satisfied"
        case reportPaths = "report_paths"
        case reports
        case readinessEligible = "readiness_eligible"
        case physicalDevice = "physical_device"
        case gadget
    }
}

struct StatusDocument: Codable {
    let area: String
    let generatedAt: String
    let gitSHA: String
    let roadmapPath: String
    let gates: [GateStatus]
    let physicalDeviceAllowed: Bool
    let releaseGateEligible: Bool
    let readinessGateEligible: Bool
    let nextEligibleGate: String?
    let statusPath: String

    enum CodingKeys: String, CodingKey {
        case area
        case generatedAt = "generated_at"
        case gitSHA = "git_sha"
        case roadmapPath = "roadmap_path"
        case gates
        case physicalDeviceAllowed = "physical_device_allowed"
        case releaseGateEligible = "release_gate_eligible"
        case readinessGateEligible = "readiness_gate_eligible"
        case nextEligibleGate = "next_eligible_gate"
        case statusPath = "status_path"
    }
}

struct PrerequisiteFact: Codable {
    let id: String
    let state: String
    let reportPaths: [String]

    enum CodingKeys: String, CodingKey {
        case id
        case state
        case reportPaths = "report_paths"
    }
}

struct TaskEnvelope: Codable {
    let area: String
    let generatedAt: String
    let gitSHA: String
    let roadmapPath: String
    let selectedGateID: String
    let selectedGateCommand: String
    let selectedGateKind: String
    let prerequisiteGates: [PrerequisiteFact]
    let whySelected: String
    let allowedScope: [String]
    let forbiddenScope: [String]
    let requiredValidationCommands: [String]
    let expectedReportPaths: [String]
    let reducerRequirements: [String]
    let requiredSubagentsOrSkills: [String]
    let commitMessage: String
    let stopConditions: [String]
    let physicalDevice: Bool
    let gadget: Bool
    let nextTaskJSONPath: String
    let nextTaskMarkdownPath: String

    enum CodingKeys: String, CodingKey {
        case area
        case generatedAt = "generated_at"
        case gitSHA = "git_sha"
        case roadmapPath = "roadmap_path"
        case selectedGateID = "selected_gate_id"
        case selectedGateCommand = "selected_gate_command"
        case selectedGateKind = "selected_gate_kind"
        case prerequisiteGates = "prerequisite_gates"
        case whySelected = "why_selected"
        case allowedScope = "allowed_scope"
        case forbiddenScope = "forbidden_scope"
        case requiredValidationCommands = "required_validation_commands"
        case expectedReportPaths = "expected_report_paths"
        case reducerRequirements = "reducer_requirements"
        case requiredSubagentsOrSkills = "required_subagents_or_skills"
        case commitMessage = "commit_message"
        case stopConditions = "stop_conditions"
        case physicalDevice = "physical_device"
        case gadget
        case nextTaskJSONPath = "next_task_json_path"
        case nextTaskMarkdownPath = "next_task_markdown_path"
    }
}

enum HarnessError: Error, CustomStringConvertible {
    case usage(String)
    case invalid(String)

    var description: String {
        switch self {
        case .usage(let message), .invalid(let message):
            return message
        }
    }
}

let fileManager = FileManager.default

func run(_ executable: String, _ arguments: [String]) -> String? {
    let process = Process()
    process.executableURL = URL(fileURLWithPath: executable)
    process.arguments = arguments
    let pipe = Pipe()
    process.standardOutput = pipe
    process.standardError = Pipe()
    do {
        try process.run()
        process.waitUntilExit()
        guard process.terminationStatus == 0 else { return nil }
        let data = pipe.fileHandleForReading.readDataToEndOfFile()
        return String(data: data, encoding: .utf8)?.trimmingCharacters(in: .whitespacesAndNewlines)
    } catch {
        return nil
    }
}

func repositoryRoot() -> URL {
    if let gitRoot = run("/usr/bin/env", ["git", "rev-parse", "--show-toplevel"]), !gitRoot.isEmpty {
        return URL(fileURLWithPath: gitRoot, isDirectory: true)
    }
    return URL(fileURLWithPath: fileManager.currentDirectoryPath, isDirectory: true)
}

let root = repositoryRoot()
let area = ProcessInfo.processInfo.environment["AREA"] ?? "orlix-tcti"
let outputRoot = root.appendingPathComponent("Build/AgentHarness/orlix-tcti", isDirectory: true)
let roadmapURL = root.appendingPathComponent(".agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json")
let statusURL = outputRoot.appendingPathComponent("status.json")
let nextTaskURL = outputRoot.appendingPathComponent("next-task.json")
let nextTaskMarkdownURL = outputRoot.appendingPathComponent("next-task.md")

func relativePath(_ url: URL) -> String {
    let rootPath = root.path.hasSuffix("/") ? root.path : root.path + "/"
    if url.path.hasPrefix(rootPath) {
        return String(url.path.dropFirst(rootPath.count))
    }
    return url.path
}

func loadRoadmap() throws -> Roadmap {
    let data = try Data(contentsOf: roadmapURL)
    let roadmap = try JSONDecoder().decode(Roadmap.self, from: data)
    guard roadmap.area == "orlix-tcti" else {
        throw HarnessError.invalid("roadmap area must be orlix-tcti")
    }
    return roadmap
}

func loadJSONObject(_ url: URL) throws -> [String: Any] {
    let data = try Data(contentsOf: url)
    let object = try JSONSerialization.jsonObject(with: data)
    guard let dictionary = object as? [String: Any] else {
        throw HarnessError.invalid("\(relativePath(url)) is not a JSON object")
    }
    return dictionary
}

func stringValue(_ value: Any?) -> String? {
    if let string = value as? String { return string }
    if let number = value as? NSNumber { return number.stringValue }
    return nil
}

func boolValue(_ value: Any?) -> Bool {
    if let bool = value as? Bool { return bool }
    if let number = value as? NSNumber { return number.boolValue }
    if let string = value as? String { return string == "true" || string == "1" }
    return false
}

func intValue(_ value: Any?) -> Int? {
    if let int = value as? Int { return int }
    if let number = value as? NSNumber { return number.intValue }
    if let string = value as? String { return Int(string) }
    return nil
}

func reportFact(target: String) -> ReportFact {
    let url = root.appendingPathComponent("Build/TCTI/reports/\(target)/report.json")
    guard fileManager.fileExists(atPath: url.path) else {
        return ReportFact(
            path: relativePath(url),
            exists: false,
            status: "missing",
            passed: false,
            releaseGateEligible: false,
            readinessGateEligible: false,
            gitSHA: nil
        )
    }
    do {
        let object = try loadJSONObject(url)
        return ReportFact(
            path: relativePath(url),
            exists: true,
            status: stringValue(object["status"]) ?? "malformed",
            passed: boolValue(object["passed"]),
            releaseGateEligible: boolValue(object["release_gate_eligible"]),
            readinessGateEligible: boolValue(object["readiness_gate_eligible"]),
            gitSHA: stringValue(object["git_sha"])
        )
    } catch {
        return ReportFact(
            path: relativePath(url),
            exists: true,
            status: "error",
            passed: false,
            releaseGateEligible: false,
            readinessGateEligible: false,
            gitSHA: nil
        )
    }
}

func pathExists(_ relative: String) -> Bool {
    fileManager.fileExists(atPath: root.appendingPathComponent(relative).path)
}

func structuralCasePass(_ caseID: String) -> (Bool, String, [ReportFact]) {
    let source = "OrlixKernel/Tests/TCTI/golden_elf/\(caseID)/\(caseID).S"
    let metadata = "OrlixKernel/Tests/TCTI/golden_elf/\(caseID)/golden.json"
    let validation = "Build/TCTI/golden_elf/\(caseID)/validation.json"
    guard pathExists(source), pathExists(metadata) else {
        return (false, "missing checked-in golden source or metadata for \(caseID)", [])
    }
    let validationURL = root.appendingPathComponent(validation)
    guard fileManager.fileExists(atPath: validationURL.path) else {
        return (false, "missing \(validation)", [])
    }
    do {
        let object = try loadJSONObject(validationURL)
        let hasHashes = stringValue(object["source_sha256"]) != nil && stringValue(object["binary_sha256"]) != nil
        let hasBinary = stringValue(object["binary"]) != nil
        if hasHashes && hasBinary {
            return (true, "\(caseID) structural validation artifact exists with source and binary hashes", [])
        }
        return (false, "\(validation) is missing source/binary hash fields", [])
    } catch {
        return (false, "\(validation) is malformed: \(error)", [])
    }
}

let structuralGateCases: [String: String] = [
    "golden-init-001-structural": "init_001_exit",
    "golden-init-002-write-structural": "init_002_write",
    "golden-init-003-stack-structural": "init_003_stack",
    "golden-init-004-tls-structural": "init_004_tls",
    "golden-init-005-branches-structural": "init_005_branches",
    "golden-init-006-memory-structural": "init_006_memory",
    "golden-init-007-mprotect-structural": "init_007_mprotect",
    "golden-init-008-self-modify-structural": "init_008_self_modify",
    "golden-init-009-faults-structural": "init_009_faults",
    "golden-init-010-cpu-model-structural": "init_010_cpu_model",
    "golden-init-011-static-pie-got-byte-load-structural": "init_011_static_pie_got_byte_load",
]

func syscalls(_ object: [String: Any]) -> [[String: Any]] {
    guard let array = object["syscalls"] as? [Any] else { return [] }
    return array.compactMap { $0 as? [String: Any] }
}

func executionObject(_ caseID: String) throws -> [String: Any] {
    let url = root.appendingPathComponent("Build/TCTI/golden_elf/\(caseID)/execution.json")
    return try loadJSONObject(url)
}

func switchExit001Pass() -> (Bool, String) {
    do {
        let object = try executionObject("init_001_exit")
        let exit = object["exit"] as? [String: Any]
        let entered = boolValue(object["entered_entrypoint"])
        let backend = stringValue(object["backend"]) == "switch-debug"
        let exitOK = stringValue(exit?["kind"]) == "guest_exit_syscall" && intValue(exit?["code"]) == 42
        let syscallOK = syscalls(object).contains { syscall in
            guard stringValue(syscall["name"]) == "exit",
                  intValue(syscall["nr"]) == 93,
                  boolValue(syscall["captured"]),
                  let args = syscall["args"] as? [Any],
                  let first = args.first else {
                return false
            }
            return intValue(first) == 42
        }
        if entered && backend && exitOK && syscallOK {
            return (true, "init_001_exit switch-debug execution captured exit(42)")
        }
        return (false, "init_001_exit execution artifact does not capture exit(42)")
    } catch {
        return (false, "missing or malformed init_001_exit execution artifact: \(error)")
    }
}

func switchWrite002Pass() -> (Bool, String) {
    do {
        let object = try executionObject("init_002_write")
        let exit = object["exit"] as? [String: Any]
        let entered = boolValue(object["entered_entrypoint"])
        let backend = stringValue(object["backend"]) == "switch-debug"
        let exitOK = stringValue(exit?["kind"]) == "guest_exit_syscall" && intValue(exit?["code"]) == 0
        let writeOK = syscalls(object).contains { syscall in
            guard stringValue(syscall["name"]) == "write",
                  intValue(syscall["nr"]) == 64,
                  boolValue(syscall["captured"]),
                  stringValue(syscall["captured_bytes"]) == "hello\n",
                  let args = syscall["args"] as? [Any],
                  args.count >= 3 else {
                return false
            }
            return intValue(args[0]) == 1 && intValue(args[2]) == 6
        }
        if entered && backend && exitOK && writeOK {
            return (true, "init_002_write switch-debug execution captured write(1, hello, 6) and exit(0)")
        }
        return (false, "init_002_write execution artifact does not capture write plus exit")
    } catch {
        return (false, "missing or malformed init_002_write execution artifact: \(error)")
    }
}

func switchStack003Pass() -> (Bool, String) {
    do {
        let object = try executionObject("init_003_stack")
        let exit = object["exit"] as? [String: Any]
        let entered = boolValue(object["entered_entrypoint"])
        let backend = stringValue(object["backend"]) == "switch-debug"
        let instructionCountOK = intValue(object["guest_instructions_executed"]) == 8
        let exitOK = stringValue(exit?["kind"]) == "guest_exit_syscall" && intValue(exit?["code"]) == 42
        let syscallOK = syscalls(object).contains { syscall in
            guard stringValue(syscall["name"]) == "exit",
                  intValue(syscall["nr"]) == 93,
                  boolValue(syscall["captured"]),
                  let args = syscall["args"] as? [Any],
                  let first = args.first else {
                return false
            }
            return intValue(first) == 42
        }
        let decoded = object["decoded_instructions"] as? [Any] ?? []
        let hasStackOps = decoded.contains { item in
            guard let instruction = item as? [String: Any] else { return false }
            return stringValue(instruction["class"]) == "load_store_unsigned_immediate"
        }
        if entered && backend && instructionCountOK && exitOK && syscallOK && hasStackOps {
            return (true, "init_003_stack switch-debug execution captured stack-derived exit(42)")
        }
        return (false, "init_003_stack execution artifact does not capture stack-derived exit(42)")
    } catch {
        return (false, "missing or malformed init_003_stack execution artifact: \(error)")
    }
}

func switchTLS004Pass() -> (Bool, String) {
    do {
        let object = try executionObject("init_004_tls")
        let exit = object["exit"] as? [String: Any]
        let entered = boolValue(object["entered_entrypoint"])
        let backend = stringValue(object["backend"]) == "switch-debug"
        let instructionCountOK = intValue(object["guest_instructions_executed"]) == 6
        let exitOK = stringValue(exit?["kind"]) == "guest_exit_syscall" && intValue(exit?["code"]) == 42
        let syscallOK = syscalls(object).contains { syscall in
            guard stringValue(syscall["name"]) == "exit",
                  intValue(syscall["nr"]) == 93,
                  boolValue(syscall["captured"]),
                  let args = syscall["args"] as? [Any],
                  let first = args.first else {
                return false
            }
            return intValue(first) == 42
        }
        let decoded = object["decoded_instructions"] as? [Any] ?? []
        let hasMSR = decoded.contains { item in
            guard let instruction = item as? [String: Any] else { return false }
            return stringValue(instruction["class"]) == "system_register" &&
                stringValue(instruction["op"]) == "msr" &&
                stringValue(instruction["sysreg"]) == "tpidr_el0"
        }
        let hasMRS = decoded.contains { item in
            guard let instruction = item as? [String: Any] else { return false }
            return stringValue(instruction["class"]) == "system_register" &&
                stringValue(instruction["op"]) == "mrs" &&
                stringValue(instruction["sysreg"]) == "tpidr_el0"
        }
        if entered && backend && instructionCountOK && exitOK && syscallOK && hasMSR && hasMRS {
            return (true, "init_004_tls switch-debug execution captured guest TPIDR_EL0-derived exit(42)")
        }
        return (false, "init_004_tls execution artifact does not capture guest TPIDR_EL0-derived exit(42)")
    } catch {
        return (false, "missing or malformed init_004_tls execution artifact: \(error)")
    }
}

func switchBranches005Pass() -> (Bool, String) {
    do {
        let object = try executionObject("init_005_branches")
        let exit = object["exit"] as? [String: Any]
        let entered = boolValue(object["entered_entrypoint"])
        let backend = stringValue(object["backend"]) == "switch-debug"
        let instructionCountOK = intValue(object["guest_instructions_executed"]) == 5
        let exitOK = stringValue(exit?["kind"]) == "guest_exit_syscall" && intValue(exit?["code"]) == 42
        let syscallOK = syscalls(object).contains { syscall in
            guard stringValue(syscall["name"]) == "exit",
                  intValue(syscall["nr"]) == 93,
                  boolValue(syscall["captured"]),
                  let args = syscall["args"] as? [Any],
                  let first = args.first else {
                return false
            }
            return intValue(first) == 42
        }
        let decoded = object["decoded_instructions"] as? [Any] ?? []
        let hasCBZ = decoded.contains { item in
            guard let instruction = item as? [String: Any] else { return false }
            return stringValue(instruction["class"]) == "compare_and_branch_immediate" &&
                stringValue(instruction["op"]) == "cbz" &&
                intValue(instruction["rt"]) == 0
        }
        if entered && backend && instructionCountOK && exitOK && syscallOK && hasCBZ {
            return (true, "init_005_branches switch-debug execution captured branch-derived exit(42)")
        }
        return (false, "init_005_branches execution artifact does not capture branch-derived exit(42)")
    } catch {
        return (false, "missing or malformed init_005_branches execution artifact: \(error)")
    }
}

func switchMemory006Pass() -> (Bool, String) {
    do {
        let object = try executionObject("init_006_memory")
 let exit = object["exit"] as? [String: Any]
 let entered = boolValue(object["entered_entrypoint"])
 let backend = stringValue(object["backend"]) == "switch-debug"
 let instructionCountOK = intValue(object["guest_instructions_executed"]) == 4
 let exitOK = stringValue(exit?["kind"]) == "guest_exit_syscall" && intValue(exit?["code"]) == 42
 let syscallOK = syscalls(object).contains { syscall in
 guard stringValue(syscall["name"]) == "exit",
 intValue(syscall["nr"]) == 93,
 boolValue(syscall["captured"]),
 let args = syscall["args"] as? [Any],
 let first = args.first else {
 return false
 }
 return intValue(first) == 42
 }
 let decoded = object["decoded_instructions"] as? [Any] ?? []
 let hasLoad = decoded.contains { item in
 guard let instruction = item as? [String: Any] else { return false }
 return stringValue(instruction["class"]) == "load_store_unsigned_immediate" &&
 stringValue(instruction["op"]) == "ldr" &&
 intValue(instruction["rt"]) == 0 &&
 intValue(instruction["rn"]) == 1 &&
 intValue(instruction["offset"]) == 0 &&
 intValue(instruction["width"]) == 64 &&
 stringValue(instruction["effective_address"]) == "0x0000000000210130"
 }
 if entered && backend && instructionCountOK && exitOK && syscallOK && hasLoad {
 return (true, "init_006_memory switch-debug execution captured file-backed PT_LOAD memory-derived exit(42)")
 }
 return (false, "init_006_memory execution artifact does not show file-backed memory-derived exit(42)")
 } catch {
        return (false, "missing malformed init_006_memory execution artifact: \(error)")
    }
}

func switchMprotect007Pass() -> (Bool, String) {
    do {
        let object = try executionObject("init_007_mprotect")
        let exit = object["exit"] as? [String: Any]
        let entered = boolValue(object["entered_entrypoint"])
        let backend = stringValue(object["backend"]) == "switch-debug"
        let instructionCountOK = intValue(object["guest_instructions_executed"]) == 8
        let exitOK = stringValue(exit?["kind"]) == "guest_exit_syscall" && intValue(exit?["code"]) == 0
        let mprotectOK = syscalls(object).contains { syscall in
            guard stringValue(syscall["name"]) == "mprotect",
                  intValue(syscall["nr"]) == 226,
                  boolValue(syscall["captured"]),
                  let args = syscall["args"] as? [Any],
                  args.count >= 3
            else {
                return false
            }
            return stringValue(args[0]) == "0x0000000000212000" &&
                intValue(args[1]) == 4096 &&
                intValue(args[2]) == 1
        }
        let exitSyscallOK = syscalls(object).contains { syscall in
            guard stringValue(syscall["name"]) == "exit",
                  intValue(syscall["nr"]) == 93,
                  boolValue(syscall["captured"]),
                  let args = syscall["args"] as? [Any],
                  let first = args.first
            else {
                return false
            }
            return intValue(first) == 0
        }
        let notes = object["notes"] as? [Any] ?? []
        let captureOnlyNote = notes.contains { item in
            guard let note = item as? String else { return false }
            return note.contains("no host mprotect") && note.contains("permission side effect")
        }
        if entered && backend && instructionCountOK && exitOK && mprotectOK && exitSyscallOK && captureOnlyNote {
            return (true, "init_007_mprotect switch-debug execution captured mprotect(PROT_READ) and exit(0) without host permission side effects")
        }
        return (false, "init_007_mprotect execution artifact does not capture mprotect(PROT_READ) then exit(0)")
    } catch {
        return (false, "missing malformed init_007_mprotect execution artifact: \(error)")
    }
}

func memoryWrites(_ object: [String: Any]) -> [[String: Any]] {
    guard let array = object["memory_writes"] as? [Any] else { return [] }
    return array.compactMap { $0 as? [String: Any] }
}

func switchSelfModify008Pass() -> (Bool, String) {
    do {
        let object = try executionObject("init_008_self_modify")
        let exit = object["exit"] as? [String: Any]
        let entered = boolValue(object["entered_entrypoint"])
        let backend = stringValue(object["backend"]) == "switch-debug"
        let instructionCountOK = intValue(object["guest_instructions_executed"]) == 6
        let exitOK = stringValue(exit?["kind"]) == "guest_exit_syscall" && intValue(exit?["code"]) == 0
        let syscallOK = syscalls(object).contains { syscall in
            guard stringValue(syscall["name"]) == "exit",
                  intValue(syscall["nr"]) == 93,
                  boolValue(syscall["captured"]),
                  let args = syscall["args"] as? [Any],
                  let first = args.first
            else {
                return false
            }
            return intValue(first) == 0
        }
        let writeOK = memoryWrites(object).contains { write in
            stringValue(write["address"]) == "0x0000000000210138" &&
                intValue(write["width"]) == 64 &&
                stringValue(write["value"]) == "0x000000000000002a" &&
                boolValue(write["captured"])
        }
        let decoded = object["decoded_instructions"] as? [Any] ?? []
        let hasSTR = decoded.contains { item in
            guard let instruction = item as? [String: Any] else { return false }
            return stringValue(instruction["class"]) == "load_store_unsigned_immediate" &&
                stringValue(instruction["op"]) == "str" &&
                intValue(instruction["rt"]) == 0 &&
                intValue(instruction["rn"]) == 1 &&
                stringValue(instruction["effective_address"]) == "0x0000000000210138"
        }
        if entered && backend && instructionCountOK && exitOK && syscallOK && writeOK && hasSTR {
            return (true, "init_008_self_modify switch-debug captured file-backed patch_slot write and exit(0)")
        }
        return (false, "init_008_self_modify execution artifact does not capture patch_slot write plus exit(0)")
    } catch {
        return (false, "missing malformed init_008_self_modify execution artifact: \(error)")
    }
}

func switchFaults009Pass() -> (Bool, String) {
    do {
        let object = try executionObject("init_009_faults")
        let entered = boolValue(object["entered_entrypoint"])
        let backend = stringValue(object["backend"]) == "switch-debug"
        let instructionCountOK = intValue(object["guest_instructions_executed"]) == 2
        let fault = object["fault"] as? [String: Any]
        let faultOK = stringValue(fault?["kind"]) == "guest_memory_fault" &&
            stringValue(fault?["address"]) == "0x0000000000000000" &&
            stringValue(fault?["access"]) == "read" &&
            boolValue(fault?["captured"])
        let noSyscall = syscalls(object).isEmpty
        let noExit = object["exit"] == nil
        let decoded = object["decoded_instructions"] as? [Any] ?? []
        let hasFaultingLoad = decoded.contains { item in
            guard let instruction = item as? [String: Any] else { return false }
            return stringValue(instruction["class"]) == "load_store_unsigned_immediate" &&
                stringValue(instruction["op"]) == "ldr" &&
                intValue(instruction["rt"]) == 0 &&
                intValue(instruction["rn"]) == 1 &&
                stringValue(instruction["effective_address"]) == "0x0000000000000000"
        }
        if entered && backend && instructionCountOK && faultOK && noSyscall && noExit && hasFaultingLoad {
            return (true, "init_009_faults switch-debug captured read fault at guest address 0x0 before syscall")
        }
        return (false, "init_009_faults execution artifact does not capture the expected guest memory fault")
    } catch {
        return (false, "missing malformed init_009_faults execution artifact: \(error)")
    }
}

func switchCPUModel010Pass() -> (Bool, String) {
    do {
        let object = try executionObject("init_010_cpu_model")
        let exit = object["exit"] as? [String: Any]
        let entered = boolValue(object["entered_entrypoint"])
        let backend = stringValue(object["backend"]) == "switch-debug"
        let instructionCountOK = intValue(object["guest_instructions_executed"]) == 4
        let exitOK = stringValue(exit?["kind"]) == "guest_exit_syscall" && intValue(exit?["code"]) == 0
        let syscallOK = syscalls(object).contains { syscall in
            guard stringValue(syscall["name"]) == "exit",
                  intValue(syscall["nr"]) == 93,
                  boolValue(syscall["captured"]),
                  let args = syscall["args"] as? [Any],
                  let first = args.first
            else {
                return false
            }
            return intValue(first) == 0
        }
        let decoded = object["decoded_instructions"] as? [Any] ?? []
        let hasADR = decoded.contains { item in
            guard let instruction = item as? [String: Any] else { return false }
            return stringValue(instruction["class"]) == "pc_relative_address" &&
                stringValue(instruction["op"]) == "adr" &&
                intValue(instruction["rd"]) == 1 &&
                intValue(instruction["imm"]) == 16
        }
        let notes = object["notes"] as? [Any] ?? []
        let hasModelNote = notes.contains { item in
            guard let note = item as? String else { return false }
            return note.contains("virtual CPU model payload orlix-aarch64-v1")
        }
        if entered && backend && instructionCountOK && exitOK && syscallOK && hasADR && hasModelNote {
            return (true, "init_010_cpu_model switch-debug captured fixed virtual CPU model payload and exit(0)")
        }
        return (false, "init_010_cpu_model execution artifact does not capture fixed virtual CPU model payload plus exit(0)")
    } catch {
        return (false, "missing malformed init_010_cpu_model execution artifact: \(error)")
    }
}

func switchStaticPIEGOTByte011Pass() -> (Bool, String) {
    do {
        let object = try executionObject("init_011_static_pie_got_byte_load")
        let exit = object["exit"] as? [String: Any]
        let entered = boolValue(object["entered_entrypoint"])
        let backend = stringValue(object["backend"]) == "switch-debug"
        let instructionCountOK = intValue(object["guest_instructions_executed"]) == 5
        let exitOK = stringValue(exit?["kind"]) == "guest_exit_syscall" && intValue(exit?["code"]) == 42
        let syscallOK = syscalls(object).contains { syscall in
            guard stringValue(syscall["name"]) == "exit",
                  intValue(syscall["nr"]) == 93,
                  boolValue(syscall["captured"]),
                  let args = syscall["args"] as? [Any],
                  let first = args.first
            else {
                return false
            }
            return intValue(first) == 42
        }
        let decoded = object["decoded_instructions"] as? [Any] ?? []
        let hasADRP = decoded.contains { item in
            guard let instruction = item as? [String: Any] else { return false }
            return stringValue(instruction["class"]) == "pc_relative_address" &&
                stringValue(instruction["op"]) == "adrp" &&
                intValue(instruction["rd"]) == 8 &&
                stringValue(instruction["effective_address"]) == "0x0000000000021000"
        }
        let hasGOTLoad = decoded.contains { item in
            guard let instruction = item as? [String: Any] else { return false }
            return stringValue(instruction["class"]) == "load_store_unsigned_immediate" &&
                stringValue(instruction["op"]) == "ldr" &&
                intValue(instruction["rt"]) == 8 &&
                intValue(instruction["rn"]) == 8 &&
                intValue(instruction["offset"]) == 216 &&
                intValue(instruction["width"]) == 64 &&
                stringValue(instruction["effective_address"]) == "0x00000000000210d8"
        }
        let hasByteLoad = decoded.contains { item in
            guard let instruction = item as? [String: Any] else { return false }
            return stringValue(instruction["class"]) == "load_store_unsigned_immediate" &&
                stringValue(instruction["op"]) == "ldrb" &&
                intValue(instruction["rt"]) == 0 &&
                intValue(instruction["rn"]) == 8 &&
                intValue(instruction["width"]) == 8 &&
                stringValue(instruction["effective_address"]) == "0x0000000000001000"
        }
        if entered && backend && instructionCountOK && exitOK && syscallOK && hasADRP && hasGOTLoad && hasByteLoad {
            return (true, "init_011_static_pie_got_byte_load switch-debug captured static PIE GOT byte-derived exit(42)")
        }
        return (false, "init_011_static_pie_got_byte_load execution artifact does not capture static PIE GOT byte-derived exit(42)")
    } catch {
        return (false, "missing malformed init_011_static_pie_got_byte_load execution artifact: \(error)")
    }
}

func firstGadgetExit001Pass() -> (Bool, String) {
    let report = reportFact(target: "tcti-diff-switch")
    guard report.exists, report.status == "pass", report.passed else {
        return (false, "tcti-diff-switch report is not passing for first gadget gate")
    }
    let diffURL = root.appendingPathComponent("Build/TCTI/diff_switch/init_001_exit/diff.json")
    do {
        let object = try loadJSONObject(diffURL)
        let divergent = object["divergent_fields"] as? [Any] ?? []
        let modeOK = stringValue(object["mode"]) == "switch-vs-gadget"
        let candidateOK = stringValue(object["candidate_backend"]) == "gadget-data-program"
        let gadgetOK = boolValue(object["gadget_dispatch_executed"])
        let assemblyOK = !boolValue(object["production_assembly_executed"])
        if modeOK && candidateOK && gadgetOK && assemblyOK && divergent.isEmpty {
            return (true, "init_001_exit gadget data-program diff matches switch-debug with zero divergent fields")
        }
        return (false, "init_001_exit gadget diff artifact is not a passing switch-vs-gadget data-program diff")
    } catch {
        return (false, "missing or malformed init_001_exit gadget diff artifact: \(error)")
    }
}

func diffSwitchExit001Pass() -> (Bool, String) {
    let report = reportFact(target: "tcti-diff-switch")
    guard report.exists, report.status == "pass", report.passed else {
        return (false, "tcti-diff-switch report is not passing for switch diff gate")
    }
    let diffURL = root.appendingPathComponent("Build/TCTI/diff_switch/init_001_exit/diff.json")
    do {
        let object = try loadJSONObject(diffURL)
        let divergent = object["divergent_fields"] as? [Any] ?? []
        let mode = stringValue(object["mode"])
        let modeOK = mode == "switch-debug-diff-preparation" || mode == "switch-vs-gadget"
        let referenceOK = stringValue(object["reference_backend"]) == "switch-debug"
        let fields = object["fields_checked"] as? [Any] ?? []
        let requiredFields = [
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
        let fieldStrings = Set(fields.compactMap { stringValue($0) })
        let fieldsOK = requiredFields.allSatisfy { fieldStrings.contains($0) }
        if modeOK && referenceOK && fieldsOK && divergent.isEmpty {
            return (true, "init_001_exit switch-debug diff baseline has zero divergent fields")
        }
        return (false, "init_001_exit diff artifact is not a passing switch-debug baseline")
    } catch {
        return (false, "missing or malformed init_001_exit diff artifact: \(error)")
    }
}

func basicReportGate(_ gate: Gate, target: String) -> GateStatus {
    let report = reportFact(target: target)
    let state = report.status
    let passed = report.status == "pass" && report.passed
    let reason = report.exists ? "\(target) report status=\(report.status), passed=\(report.passed)" : "\(target) report missing"
    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        state: state,
        passed: passed,
        reason: reason,
        prerequisites: gate.prerequisites,
        prerequisitesSatisfied: false,
        reportPaths: gate.expectedReportPaths,
        reports: [report],
        readinessEligible: gate.readinessEligible,
        physicalDevice: gate.physicalDevice,
        gadget: gate.gadget
    )
}

func latestRuntimeReport(gate gateName: String, destination: String) -> (ReportFact, [String: Any])? {
    let runtimeURL = root.appendingPathComponent("Build/Reports/runtime", isDirectory: true)
    guard let entries = try? fileManager.contentsOfDirectory(at: runtimeURL, includingPropertiesForKeys: nil) else {
        return nil
    }
    let candidates = entries
        .filter { $0.lastPathComponent.hasPrefix("\(gateName)-") && $0.pathExtension == "json" }
        .sorted { $0.lastPathComponent > $1.lastPathComponent }
    for url in candidates {
        guard let object = try? loadJSONObject(url),
              stringValue(object["gate"]) == gateName,
              stringValue(object["destination"]) == destination
        else {
            continue
        }
        let fact = ReportFact(
            path: relativePath(url),
            exists: true,
            status: stringValue(object["status"]) ?? "malformed",
            passed: boolValue(object["passed"]),
            releaseGateEligible: boolValue(object["release_gate_eligible"]),
            readinessGateEligible: boolValue(object["readiness_gate_eligible"]),
            gitSHA: stringValue(object["git_sha"])
        )
        return (fact, object)
    }
    return nil
}

func simulatorFirstSyscallPass(_ gate: Gate) -> GateStatus {
    guard let (report, object) = latestRuntimeReport(gate: "tcti-init-first-syscall", destination: "iphonesimulator") else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "missing",
            passed: false,
            reason: "missing iphonesimulator runtime-validation report for tcti-init-first-syscall",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: [],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    let forbidden = object["forbidden_behavior"] as? [String: Any] ?? [:]
    let forbiddenClear = [
        "generated_exec_memory",
        "host_exec_guest_text",
        "host_x18",
        "map_jit",
        "native_ios_api_exposure_to_guest",
        "rwx",
    ].allSatisfy { !boolValue(forbidden[$0]) }
    let reportOK = report.status == "pass" &&
        report.passed &&
        report.gitSHA == gitSHA() &&
        stringValue(object["backend"]) == "tcti" &&
        stringValue(object["profile"]) == "tcti_runtime" &&
        !boolValue(object["preflight_only"]) &&
        !boolValue(object["autonomous_tests_bypassed"]) &&
        forbiddenClear
    let reason: String
    if reportOK {
        reason = "iphonesimulator runtime-validation report \(report.path) passed with tcti runtime profile and forbidden behavior false"
    } else if report.gitSHA != gitSHA() {
        reason = "latest iphonesimulator runtime-validation report \(report.path) is stale for current HEAD"
    } else {
        reason = "latest iphonesimulator runtime-validation report \(report.path) is not a valid non-preflight TCTI pass"
    }
    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        state: report.status,
        passed: reportOK,
        reason: reason,
        prerequisites: gate.prerequisites,
        prerequisitesSatisfied: false,
        reportPaths: gate.expectedReportPaths,
        reports: [report],
        readinessEligible: gate.readinessEligible,
        physicalDevice: gate.physicalDevice,
        gadget: gate.gadget
    )
}

func simulatorStabilityPass(_ gate: Gate) -> GateStatus {
    guard let (report, object) = latestRuntimeReport(gate: "tcti-simulator-stability", destination: "iphonesimulator") else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "missing",
            passed: false,
            reason: "missing iphonesimulator runtime-validation report for tcti-simulator-stability",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: [],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    let forbidden = object["forbidden_behavior"] as? [String: Any] ?? [:]
    let forbiddenClear = [
        "generated_exec_memory",
        "host_exec_guest_text",
        "host_x18",
        "map_jit",
        "native_ios_api_exposure_to_guest",
        "rwx",
    ].allSatisfy { !boolValue(forbidden[$0]) }
    let reportOK = report.status == "pass" &&
        report.passed &&
        report.gitSHA == gitSHA() &&
        stringValue(object["backend"]) == "tcti" &&
        stringValue(object["profile"]) == "tcti_runtime" &&
        !boolValue(object["preflight_only"]) &&
        !boolValue(object["autonomous_tests_bypassed"]) &&
        forbiddenClear
    let reason: String
    if reportOK {
        reason = "iphonesimulator runtime-validation report \(report.path) passed with no fatal simulator TCTI runtime errors"
    } else if report.gitSHA != gitSHA() {
        reason = "latest iphonesimulator stability report \(report.path) is stale for current HEAD"
    } else {
        reason = "latest iphonesimulator stability report \(report.path) is not a valid non-preflight TCTI pass"
    }
    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        state: report.status,
        passed: reportOK,
        reason: reason,
        prerequisites: gate.prerequisites,
        prerequisitesSatisfied: false,
        reportPaths: gate.expectedReportPaths,
        reports: [report],
        readinessEligible: gate.readinessEligible,
        physicalDevice: gate.physicalDevice,
        gadget: gate.gadget
    )
}

func baseGateStatus(_ gate: Gate) -> GateStatus {
    if let caseID = structuralGateCases[gate.id] {
        let check = structuralCasePass(caseID)
        return artifactStatus(gate, passed: check.0, reason: check.1)
    }

    switch gate.id {
    case "rails-defconfig-safety":
        return basicReportGate(gate, target: "tcti-plan-consistency")
    case "report-schema":
        return basicReportGate(gate, target: "tcti-report-schema-check")
    case "toolchain":
        return basicReportGate(gate, target: "tcti-toolchain-check")
    case "appstore-safety":
        return basicReportGate(gate, target: "tcti-appstore-safety-audit")
    case "switch-init-001-exit":
        let check = switchExit001Pass()
        return artifactStatus(gate, passed: check.0, reason: check.1)
    case "switch-init-002-write":
        let check = switchWrite002Pass()
        return artifactStatus(gate, passed: check.0, reason: check.1)
    case "switch-init-003-stack":
        let check = switchStack003Pass()
        return artifactStatus(gate, passed: check.0, reason: check.1)
    case "switch-init-004-tls":
        let check = switchTLS004Pass()
        return artifactStatus(gate, passed: check.0, reason: check.1)
    case "switch-init-005-branches":
        let check = switchBranches005Pass()
        return artifactStatus(gate, passed: check.0, reason: check.1)
    case "switch-init-006-memory":
        let check = switchMemory006Pass()
        return artifactStatus(gate, passed: check.0, reason: check.1)
    case "switch-init-007-mprotect":
        let check = switchMprotect007Pass()
        return artifactStatus(gate, passed: check.0, reason: check.1)
    case "switch-init-008-self-modify":
        let check = switchSelfModify008Pass()
        return artifactStatus(gate, passed: check.0, reason: check.1)
    case "switch-init-009-faults":
        let check = switchFaults009Pass()
        return artifactStatus(gate, passed: check.0, reason: check.1)
    case "switch-init-010-cpu-model":
        let check = switchCPUModel010Pass()
        return artifactStatus(gate, passed: check.0, reason: check.1)
    case "switch-init-011-static-pie-got-byte-load":
        let check = switchStaticPIEGOTByte011Pass()
        return artifactStatus(gate, passed: check.0, reason: check.1)
    case "diff-switch-init-001-exit":
        let check = diffSwitchExit001Pass()
        return artifactStatus(gate, passed: check.0, reason: check.1)
    case "first-gadget-init-001-exit":
        let check = firstGadgetExit001Pass()
        return artifactStatus(gate, passed: check.0, reason: check.1)
    case "tcti-contract":
        return basicReportGate(gate, target: "tcti-contract")
    case "tcti-memory-fuzz":
        return basicReportGate(gate, target: "tcti-memory-fuzz")
    case "tcti-direct-chain-fuzz":
        return basicReportGate(gate, target: "tcti-direct-chain-fuzz")
    case "simulator-tcti-init-first-syscall":
        return simulatorFirstSyscallPass(gate)
    case "no-phone-tcti-simulator-user-fault-reducer":
        return basicReportGate(gate, target: "tcti-simulator-user-fault-reducer")
    case "simulator-tcti-runtime-stability":
        return simulatorStabilityPass(gate)
    case "physical-tcti-init-first-syscall":
        return missingGate(gate, reason: "physical first-syscall gate is not allowed until no-phone and simulator prerequisites pass")
    default:
        return missingGate(gate, reason: "unknown roadmap gate")
    }
}

func artifactStatus(_ gate: Gate, passed: Bool, reason: String) -> GateStatus {
    GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        state: passed ? "pass" : "missing",
        passed: passed,
        reason: reason,
        prerequisites: gate.prerequisites,
        prerequisitesSatisfied: false,
        reportPaths: gate.expectedReportPaths,
        reports: [],
        readinessEligible: gate.readinessEligible,
        physicalDevice: gate.physicalDevice,
        gadget: gate.gadget
    )
}

func missingGate(_ gate: Gate, reason: String) -> GateStatus {
    artifactStatus(gate, passed: false, reason: reason)
}

func runtimePreflightGates() -> [Gate] {
    [
        Gate(
            id: "tcti-contract",
            command: "make tcti-contract",
            kind: "no-phone-contract",
            prerequisites: ["first-gadget-init-001-exit"],
            allowedScope: [
                "tools/tcti/orlix-tcti-gate.swift",
                "tools/tcti/fixtures/**",
                "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/**",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Stay no-phone.",
                "Do not mark TODO contract groups as pass.",
                "Do not broaden beyond the smallest missing no-phone contract group selected by the report.",
            ],
            expectedReportPaths: ["Build/TCTI/reports/tcti-contract/report.json"],
            readinessEligible: false,
            physicalDevice: false,
            gadget: false,
            requiredValidationCommands: [
                "rtk proxy make tcti-contract",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
            ],
            reducerRequirements: ["Any failing contract group must produce a replayable reducer."],
            requiredSubagentsOrSkills: [
                "orlix-tcti-oracle",
                "orlix-tcti-safety",
                "tcti-test-reducer",
            ],
            commitMessageTemplate: "test(tcti): complete no-phone contract gate",
            stopConditions: [
                "Stop if the contract report remains TODO without selecting the smallest missing no-phone group.",
                "Stop if a TODO group is reported as pass.",
            ]
        ),
        Gate(
            id: "tcti-memory-fuzz",
            command: "make tcti-memory-fuzz",
            kind: "no-phone-fuzz",
            prerequisites: ["tcti-contract"],
            allowedScope: [
                "tools/tcti/orlix-tcti-gate.swift",
                "tools/tcti/fixtures/**",
                "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/**",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Stay no-phone.",
                "Do not patch from certification logs.",
                "Do not bypass executable-memory safety policy.",
            ],
            expectedReportPaths: ["Build/TCTI/reports/tcti-memory-fuzz/report.json"],
            readinessEligible: false,
            physicalDevice: false,
            gadget: false,
            requiredValidationCommands: [
                "rtk proxy make tcti-memory-fuzz",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
            ],
            reducerRequirements: ["Any memory fuzz failure must produce a replayable reducer."],
            requiredSubagentsOrSkills: [
                "orlix-tcti-oracle",
                "orlix-tcti-safety",
                "tcti-test-reducer",
            ],
            commitMessageTemplate: "test(tcti): add no-phone memory fuzz gate",
            stopConditions: [
                "Stop if memory permissions or backing semantics are unclear.",
                "Stop if the failure cannot be reduced before implementation.",
            ]
        ),
        Gate(
            id: "tcti-direct-chain-fuzz",
            command: "make tcti-direct-chain-fuzz",
            kind: "no-phone-fuzz",
            prerequisites: ["tcti-memory-fuzz"],
            allowedScope: [
                "tools/tcti/orlix-tcti-gate.swift",
                "tools/tcti/fixtures/**",
                "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/**",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Stay no-phone.",
                "Do not implement production assembly unless a separate selected gate allows it.",
                "Do not bypass executable-memory safety policy.",
            ],
            expectedReportPaths: ["Build/TCTI/reports/tcti-direct-chain-fuzz/report.json"],
            readinessEligible: false,
            physicalDevice: false,
            gadget: false,
            requiredValidationCommands: [
                "rtk proxy make tcti-direct-chain-fuzz",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
            ],
            reducerRequirements: ["Any direct-chain fuzz failure must produce a replayable reducer."],
            requiredSubagentsOrSkills: [
                "orlix-tcti-oracle",
                "orlix-tcti-safety",
                "tcti-test-reducer",
            ],
            commitMessageTemplate: "test(tcti): add no-phone direct-chain fuzz gate",
            stopConditions: [
                "Stop if direct chaining would require production assembly before its selected gate.",
                "Stop if the failure cannot be reduced before implementation.",
            ]
        ),
        Gate(
            id: "simulator-tcti-init-first-syscall",
            command: "make runtime-validation DESTINATION=iphonesimulator GATE=tcti-init-first-syscall",
            kind: "simulator-runtime",
            prerequisites: ["tcti-direct-chain-fuzz"],
            allowedScope: [
                "tools/runtime/orlix-runtime-validation.sh",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Do not run physical-device gates.",
                "Do not treat simulator evidence as release or physical readiness.",
                "Do not use emergency override or preflight-only evidence as pass.",
                "Do not patch simulator logs directly without reducing TCTI behavior into a no-phone fixture.",
            ],
            expectedReportPaths: [
                "Build/Reports/runtime/tcti-init-first-syscall-*.json",
                "Build/TCTI/reports/tcti-plan-consistency/report.json",
                "Build/TCTI/reports/tcti-report-schema-check/report.json",
                "Build/TCTI/reports/tcti-golden-elf/report.json",
                "Build/TCTI/reports/tcti-appstore-safety-audit/report.json",
            ],
            readinessEligible: false,
            physicalDevice: false,
            gadget: false,
            requiredValidationCommands: [
                "rtk proxy make tcti-plan-consistency",
                "rtk proxy make tcti-report-schema-check",
                "rtk proxy make tcti-golden-elf",
                "rtk proxy make tcti-appstore-safety-audit",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
                "rtk proxy make runtime-validation DESTINATION=iphonesimulator GATE=tcti-init-first-syscall",
            ],
            reducerRequirements: [
                "Any simulator failure after app launch must be reduced into a no-phone golden, oracle, memory fuzz, direct-chain fuzz, or safety case before production patching.",
                "Environment or simulator boot failures must be reported as infrastructure blockers, not TCTI passes.",
            ],
            requiredSubagentsOrSkills: [
                "orlix-tcti-safety",
                "orlix-runtime-claim-verification",
                "tcti-planner",
                "tcti-safety-reviewer",
                "tcti-test-reducer",
                "tcti-release-gate-reviewer",
            ],
            commitMessageTemplate: "test(tcti): certify first simulator syscall gate",
            stopConditions: [
                "Stop if no-phone reports are missing, todo, evidence, or fail.",
                "Stop if simulator runtime-validation uses preflight-only or emergency override evidence.",
                "Stop if a simulator pass is claimed as physical, release, or readiness eligibility.",
            ]
        ),
        Gate(
            id: "no-phone-tcti-simulator-user-fault-reducer",
            command: "make tcti-simulator-user-fault-reducer",
            kind: "no-phone-reducer",
            prerequisites: ["simulator-tcti-init-first-syscall"],
            allowedScope: [
                "tools/tcti/orlix-tcti-gate.swift",
                "Makefile",
                ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Do not run phone gates.",
                "Do not add production TCTI assembly.",
                "Do not add gadget dispatch.",
                "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime semantics.",
                "Do not edit generated upstream or build trees.",
                "Do not flip product defconfigs.",
            ],
            expectedReportPaths: [
                "Build/TCTI/reports/tcti-simulator-user-fault-reducer/report.json",
                "Build/TCTI/reproducers/tcti-golden-elf/execution-static-pie-got-unrelocated-byte-load.json",
                "Build/TCTI/reports/tcti-repro/report.json",
            ],
            readinessEligible: false,
            physicalDevice: false,
            gadget: false,
            requiredValidationCommands: [
                "rtk proxy make tcti-plan-consistency",
                "rtk proxy make tcti-report-schema-check",
                "rtk proxy make tcti-golden-elf CASE=init_011_static_pie_got_byte_load EXECUTE=switch-debug",
                "rtk proxy make tcti-simulator-user-fault-reducer",
                "rtk proxy make tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-static-pie-got-unrelocated-byte-load.json",
                "rtk proxy make tcti-appstore-safety-audit",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
            ],
            reducerRequirements: [
                "A current failing simulator stability report must contain the null user fault and init-kill panic signature.",
                "The no-phone reducer must replay through make tcti-repro before production TCTI patching.",
            ],
            requiredSubagentsOrSkills: [
                "orlix-tcti-safety",
                "orlix-tcti-reproducer",
                "orlix-tcti-golden-elf",
                "tcti-planner",
                "tcti-safety-reviewer",
                "tcti-test-reducer",
                "tcti-release-gate-reviewer",
            ],
            commitMessageTemplate: "test(tcti): add simulator fault got reducer",
            stopConditions: [
                "Stop if the current simulator stability report is stale or missing the fatal signature.",
                "Stop if the no-phone reducer cannot replay.",
                "Stop if production TCTI code would be required before the reducer exists.",
            ]
        ),
        Gate(
            id: "simulator-tcti-runtime-stability",
            command: "make runtime-validation DESTINATION=iphonesimulator GATE=tcti-simulator-stability",
            kind: "simulator-runtime",
            prerequisites: ["no-phone-tcti-simulator-user-fault-reducer"],
            allowedScope: [
                "tools/runtime/orlix-runtime-validation.sh",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Do not run physical-device gates.",
                "Do not treat first-syscall marker evidence as simulator stability.",
                "Do not patch simulator logs directly without reducing TCTI behavior into a no-phone fixture.",
                "Do not add production assembly or gadget dispatch unless a later selected gate allows it.",
                "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime semantics.",
            ],
            expectedReportPaths: [
                "Build/Reports/runtime/tcti-simulator-stability-*.json",
                "Build/TCTI/reports/tcti-plan-consistency/report.json",
                "Build/TCTI/reports/tcti-report-schema-check/report.json",
                "Build/TCTI/reports/tcti-golden-elf/report.json",
                "Build/TCTI/reports/tcti-appstore-safety-audit/report.json",
            ],
            readinessEligible: false,
            physicalDevice: false,
            gadget: false,
            requiredValidationCommands: [
                "rtk proxy make tcti-plan-consistency",
                "rtk proxy make tcti-report-schema-check",
                "rtk proxy make tcti-golden-elf",
                "rtk proxy make tcti-appstore-safety-audit",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
                "rtk proxy make runtime-validation DESTINATION=iphonesimulator GATE=tcti-simulator-stability",
            ],
            reducerRequirements: [
                "Any simulator fatal runtime error must be reduced into a no-phone golden, oracle, memory fuzz, direct-chain fuzz, or safety case before production patching.",
                "The stability report must include the fatal log artifact when the gate fails.",
            ],
            requiredSubagentsOrSkills: [
                "orlix-tcti-safety",
                "orlix-runtime-claim-verification",
                "orlix-tcti-reproducer",
                "tcti-planner",
                "tcti-safety-reviewer",
                "tcti-test-reducer",
                "tcti-release-gate-reviewer",
            ],
            commitMessageTemplate: "test(tcti): require simulator runtime stability gate",
            stopConditions: [
                "Stop if the simulator captures Kernel panic, Attempted to kill init, Attempted kill init, user fault, BUG, Oops, SIGSEGV, fatal error, or crash.",
                "Stop if the failure cannot be reduced before implementation.",
                "Stop if a simulator pass is claimed as physical, release, or readiness eligibility.",
            ]
        ),
    ]
}

func roadmapGatesWithRuntimePreflight(_ roadmap: Roadmap) -> [Gate] {
    let inserts = runtimePreflightGates()
    var result: [Gate] = []
    var inserted = false
    for gate in roadmap.gates {
        if !inserted && gate.kind == "physical-device" {
            result.append(contentsOf: inserts)
            inserted = true
        }
        result.append(gate)
    }
    if !inserted {
        result.append(contentsOf: inserts)
    }
    return result
}

func statuses(for roadmap: Roadmap) -> [GateStatus] {
    let bases = roadmapGatesWithRuntimePreflight(roadmap).map(baseGateStatus)
    let byID = Dictionary(uniqueKeysWithValues: bases.map { ($0.id, $0) })
    return bases.map { status in
        let satisfied = status.prerequisites.allSatisfy { byID[$0]?.passed == true }
        return GateStatus(
            id: status.id,
            command: status.command,
            kind: status.kind,
            state: status.state,
            passed: status.passed,
            reason: status.reason,
            prerequisites: status.prerequisites,
            prerequisitesSatisfied: satisfied,
            reportPaths: status.reportPaths,
            reports: status.reports,
            readinessEligible: status.readinessEligible,
            physicalDevice: status.physicalDevice,
            gadget: status.gadget
        )
    }
}

func selectedStatus(from statuses: [GateStatus]) -> GateStatus? {
    statuses.first { !$0.passed && $0.prerequisitesSatisfied }
}

func noPhoneGatesBeforeFirstPhysical(_ statuses: [GateStatus]) -> [GateStatus] {
    var gates: [GateStatus] = []
    for status in statuses {
        if status.physicalDevice {
            break
        }
        if !status.physicalDevice {
            gates.append(status)
        }
    }
    return gates
}

func noPhoneGatesPassedBeforeFirstPhysical(_ statuses: [GateStatus]) -> Bool {
    noPhoneGatesBeforeFirstPhysical(statuses).allSatisfy { $0.passed }
}

func selectedStatusWithSafety(from statuses: [GateStatus]) -> GateStatus? {
    let noPhonePassed = noPhoneGatesPassedBeforeFirstPhysical(statuses)
    return statuses.first { status in
        guard !status.passed && status.prerequisitesSatisfied else {
            return false
        }
        if status.physicalDevice && !noPhonePassed {
            return false
        }
        return true
    }
}

func gitSHA() -> String {
    run("/usr/bin/env", ["git", "rev-parse", "HEAD"]) ?? "unknown"
}

func timestamp() -> String {
    ISO8601DateFormatter().string(from: Date())
}

func ensureOutputDirectory() throws {
    try fileManager.createDirectory(at: outputRoot, withIntermediateDirectories: true)
}

func writeJSON<T: Encodable>(_ value: T, to url: URL) throws {
    let encoder = JSONEncoder()
    encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
    let data = try encoder.encode(value)
    try data.write(to: url, options: [.atomic])
}

func statusDocument() throws -> StatusDocument {
    let roadmap = try loadRoadmap()
    let gateStatuses = statuses(for: roadmap)
    let physicalGate = gateStatuses.first { $0.physicalDevice }
    let next = selectedStatusWithSafety(from: gateStatuses)
    let preflightGateIDs = Set(runtimePreflightGates().map(\.id))
    let preflightPassed = gateStatuses
        .filter { preflightGateIDs.contains($0.id) }
        .allSatisfy { $0.passed }
    let physicalAllowed = physicalGate?.prerequisitesSatisfied == true &&
        preflightPassed &&
        noPhoneGatesPassedBeforeFirstPhysical(gateStatuses)
    let releaseEligible = physicalGate?.passed == true
    let readinessEligible = physicalGate?.passed == true
    return StatusDocument(
        area: roadmap.area,
        generatedAt: timestamp(),
        gitSHA: gitSHA(),
        roadmapPath: relativePath(roadmapURL),
        gates: gateStatuses,
        physicalDeviceAllowed: physicalAllowed,
        releaseGateEligible: releaseEligible,
        readinessGateEligible: readinessEligible,
        nextEligibleGate: next?.id,
        statusPath: relativePath(statusURL)
    )
}

func writeStatus(printHuman: Bool) throws -> StatusDocument {
    guard area == "orlix-tcti" else {
        throw HarnessError.usage("unsupported AREA=\(area); expected AREA=orlix-tcti")
    }
    try ensureOutputDirectory()
    let status = try statusDocument()
    try writeJSON(status, to: statusURL)
    if printHuman {
        print("Orlix TCTI agent status")
        print("status_json: \(relativePath(statusURL))")
        print("physical_device_allowed: \(status.physicalDeviceAllowed)")
        print("release_gate_eligible: \(status.releaseGateEligible)")
        print("readiness_gate_eligible: \(status.readinessGateEligible)")
        print("next_eligible_gate: \(status.nextEligibleGate ?? "none")")
        print("gates:")
        for gate in status.gates {
            print("- \(gate.id): \(gate.state) prerequisites_satisfied=\(gate.prerequisitesSatisfied) report_paths=\(gate.reportPaths.joined(separator: ","))")
        }
    }
    return status
}

func envelope(from status: StatusDocument) throws -> TaskEnvelope {
    let roadmap = try loadRoadmap()
    guard let selectedID = status.nextEligibleGate,
          let gate = roadmapGatesWithRuntimePreflight(roadmap).first(where: { $0.id == selectedID }) else {
        throw HarnessError.invalid("no next eligible gate found")
    }
    let byID = Dictionary(uniqueKeysWithValues: status.gates.map { ($0.id, $0) })
    let prerequisites = gate.prerequisites.map { prereqID in
        let fact = byID[prereqID]
        return PrerequisiteFact(
            id: prereqID,
            state: fact?.state ?? "missing",
            reportPaths: fact?.reportPaths ?? []
        )
    }
    let why = "Selected \(gate.id) because it is the first roadmap gate that is not pass and every prerequisite is pass. Evidence: \(prerequisites.map { "\($0.id)=\($0.state)" }.joined(separator: ", "))."
    return TaskEnvelope(
        area: roadmap.area,
        generatedAt: timestamp(),
        gitSHA: status.gitSHA,
        roadmapPath: relativePath(roadmapURL),
        selectedGateID: gate.id,
        selectedGateCommand: gate.command,
        selectedGateKind: gate.kind,
        prerequisiteGates: prerequisites,
        whySelected: why,
        allowedScope: gate.allowedScope,
        forbiddenScope: gate.forbiddenScope,
        requiredValidationCommands: gate.requiredValidationCommands,
        expectedReportPaths: gate.expectedReportPaths,
        reducerRequirements: gate.reducerRequirements,
        requiredSubagentsOrSkills: gate.requiredSubagentsOrSkills,
        commitMessage: gate.commitMessageTemplate,
        stopConditions: gate.stopConditions,
        physicalDevice: gate.physicalDevice,
        gadget: gate.gadget,
        nextTaskJSONPath: relativePath(nextTaskURL),
        nextTaskMarkdownPath: relativePath(nextTaskMarkdownURL)
    )
}

func markdown(for envelope: TaskEnvelope) -> String {
    func bulletList(_ values: [String]) -> String {
        values.map { "- \($0)" }.joined(separator: "\n")
    }
    let prereqs = envelope.prerequisiteGates.map { "- \($0.id): \($0.state) (\($0.reportPaths.joined(separator: ", ")))" }.joined(separator: "\n")
    return """
    # Orlix TCTI Next Task

    Selected gate: `\(envelope.selectedGateID)`

    Command: `\(envelope.selectedGateCommand)`

    Why selected: \(envelope.whySelected)

    ## Prerequisites
    \(prereqs.isEmpty ? "- none" : prereqs)

    ## Allowed Scope
    \(bulletList(envelope.allowedScope))

    ## Forbidden Scope
    \(bulletList(envelope.forbiddenScope))

    ## Required Validation Commands
    \(bulletList(envelope.requiredValidationCommands))

    ## Expected Report Paths
    \(bulletList(envelope.expectedReportPaths))

    ## Reducer Requirements
    \(bulletList(envelope.reducerRequirements))

    ## Required Subagents Or Skills
    \(bulletList(envelope.requiredSubagentsOrSkills))

    ## Commit Message
    `\(envelope.commitMessage)`

    ## Stop Conditions
    \(bulletList(envelope.stopConditions))
    """
}

func writeNext() throws -> TaskEnvelope {
    let status = try writeStatus(printHuman: false)
    let task = try envelope(from: status)
    try writeJSON(task, to: nextTaskURL)
    try markdown(for: task).write(to: nextTaskMarkdownURL, atomically: true, encoding: .utf8)
    print("Orlix TCTI next task")
    print("next_task_json: \(relativePath(nextTaskURL))")
    print("next_task_md: \(relativePath(nextTaskMarkdownURL))")
    print("selected_gate: \(task.selectedGateID)")
    print("selected_command: \(task.selectedGateCommand)")
    print("commit_message: \(task.commitMessage)")
    return task
}

func validateEnvelope() throws {
    guard area == "orlix-tcti" else {
        throw HarnessError.usage("unsupported AREA=\(area); expected AREA=orlix-tcti")
    }
    guard fileManager.fileExists(atPath: nextTaskURL.path) else {
        throw HarnessError.invalid("missing \(relativePath(nextTaskURL)); run make agent-next AREA=orlix-tcti")
    }
    let roadmap = try loadRoadmap()
    let data = try Data(contentsOf: nextTaskURL)
    guard let text = String(data: data, encoding: .utf8) else {
        throw HarnessError.invalid("next-task JSON is not UTF-8")
    }
    for forbidden in ["orlix-tcti-mcp", "tools/mcp/orlix", "mcp_servers.orlix-tcti", "tools/agent"] {
        if text.contains(forbidden) {
            throw HarnessError.invalid("next-task envelope references forbidden \(forbidden)")
        }
    }
    let task = try JSONDecoder().decode(TaskEnvelope.self, from: data)
    guard task.area == "orlix-tcti" else {
        throw HarnessError.invalid("next-task area must be orlix-tcti")
    }
    guard let gate = roadmapGatesWithRuntimePreflight(roadmap).first(where: { $0.id == task.selectedGateID }) else {
        throw HarnessError.invalid("selected gate \(task.selectedGateID) does not exist in roadmap")
    }
    let status = try statusDocument()
    let byID = Dictionary(uniqueKeysWithValues: status.gates.map { ($0.id, $0) })
    for prerequisite in gate.prerequisites {
        guard byID[prerequisite]?.passed == true else {
            throw HarnessError.invalid("prerequisite \(prerequisite) is not satisfied")
        }
    }
    if task.forbiddenScope.isEmpty {
        throw HarnessError.invalid("forbidden scope must be non-empty")
    }
    if task.requiredValidationCommands.isEmpty {
        throw HarnessError.invalid("validation commands must be present")
    }
    if task.expectedReportPaths.isEmpty {
        throw HarnessError.invalid("expected report paths must be present")
    }
    if task.physicalDevice && !status.physicalDeviceAllowed {
        throw HarnessError.invalid("physical-device gate selected while no-phone gates are incomplete")
    }
    if task.gadget {
        guard byID["switch-init-001-exit"]?.passed == true,
              byID["switch-init-002-write"]?.passed == true else {
            throw HarnessError.invalid("gadget gate selected before switch-debug oracle coverage exists")
        }
    }
    let requiredScripts = [
        ".agents/skills/orlix-tcti-next-step/scripts/status",
        ".agents/skills/orlix-tcti-next-step/scripts/next",
        ".agents/skills/orlix-tcti-next-step/scripts/task-envelope-check"
    ]
    for script in requiredScripts {
        guard pathExists(script) else {
            throw HarnessError.invalid("missing skill-local script \(script)")
        }
    }
    print("pass: \(relativePath(nextTaskURL))")
    print("selected_gate: \(task.selectedGateID)")
}

let mode = CommandLine.arguments.dropFirst().first ?? "status"

do {
    switch mode {
    case "status":
        _ = try writeStatus(printHuman: true)
    case "next":
        _ = try writeNext()
    case "check":
        try validateEnvelope()
    default:
        throw HarnessError.usage("usage: tcti-next-step.swift [status|next|check]")
    }
} catch {
    fputs("agent next-step error: \(error)\n", stderr)
    exit(1)
}
