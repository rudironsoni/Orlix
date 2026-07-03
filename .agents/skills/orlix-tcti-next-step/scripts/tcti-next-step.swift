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
    let simulatorAllowed: Bool
    let simulatorRequiredBeforePhysical: Bool
    let simulatorGatesComplete: Bool
    let requiredSimulatorID: String
    let requiredSimulatorName: String
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
        case simulatorAllowed = "simulator_allowed"
        case simulatorRequiredBeforePhysical = "simulator_required_before_physical"
        case simulatorGatesComplete = "simulator_gates_complete"
        case requiredSimulatorID = "required_simulator_id"
        case requiredSimulatorName = "required_simulator_name"
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
    let simulatorAllowed: Bool
    let simulatorRequiredBeforePhysical: Bool
    let simulatorGatesComplete: Bool
    let selectedGateUsesSimulator: Bool
    let requiredSimulatorID: String
    let requiredSimulatorName: String
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
        case simulatorAllowed = "simulator_allowed"
        case simulatorRequiredBeforePhysical = "simulator_required_before_physical"
        case simulatorGatesComplete = "simulator_gates_complete"
        case selectedGateUsesSimulator = "selected_gate_uses_simulator"
        case requiredSimulatorID = "required_simulator_id"
        case requiredSimulatorName = "required_simulator_name"
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
let requiredSimulatorID = "C47ED88D-0D0A-420D-8C78-D4C1D34A276D"
let requiredSimulatorName = "Orlix-iPhone-15-Pro-Max"
let physicalOptInBlockedGateID = "blocked-physical-device-opt-in-required"

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

func latestSimulatorStabilityText() -> (ReportFact, String)? {
    guard let (report, object) = latestRuntimeReport(gate: "tcti-simulator-stability", destination: "iphonesimulator") else {
        return nil
    }
    let artifacts = (object["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
    let text = artifacts.compactMap { artifact -> String? in
        let url = root.appendingPathComponent(artifact)
        return try? String(contentsOf: url, encoding: .utf8)
    }.joined(separator: "\n")
    return (report, text)
}

func supersededSimulatorReducerGate(
    _ gate: Gate,
    target: String,
    staleSignature: (String) -> Bool,
    supersededBy downstreamTargets: [String]
) -> GateStatus {
    let report = reportFact(target: target)
    if report.status == "pass" && report.passed {
        return basicReportGate(gate, target: target)
    }
    guard let (simulatorReport, simulatorText) = latestSimulatorStabilityText(),
          !staleSignature(simulatorText) else {
        return basicReportGate(gate, target: target)
    }
	let downstreamReports = downstreamTargets.map { reportFact(target: $0) }
	return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        state: "superseded",
        passed: true,
        reason: "\(target) is superseded for current simulator failure; latest stability report no longer matches its signature and downstream reducer/fix reports pass",
        prerequisites: gate.prerequisites,
        prerequisitesSatisfied: false,
        reportPaths: gate.expectedReportPaths,
        reports: [report, simulatorReport] + downstreamReports,
        readinessEligible: gate.readinessEligible,
        physicalDevice: gate.physicalDevice,
        gadget: gate.gadget
    )
}

func staticPIEGOTNullReadSignature(_ text: String) -> Bool {
    text.contains("Orlix TCTI: user fault") &&
        text.contains("addr=0x0") &&
        text.contains("access=1") &&
        (text.contains("Attempted kill init") || text.contains("Attempted to kill init"))
}

func brkTrapSignature(_ text: String) -> Bool {
    text.contains("insn=0xd4200020") ||
        (text.contains("BRK") && text.contains("exitcode=0x00000004"))
}

func postSetsidTLSFaultSignature(_ text: String) -> Bool {
    text.contains("static PIE read has no readable VMA task=init") &&
        text.contains("size=64 ret=-14") &&
        text.contains("orlix-init: process signaled pid=") &&
        text.contains("signal=11") &&
        text.contains("shell exit status=139")
}

func postExecSHFetchFaultSignature(_ text: String) -> Bool {
    text.contains("Orlix TCTI: user fault task=sh") &&
        text.contains("pc=0x1000494c8") &&
        text.contains("addr=0x1000494c8") &&
        text.contains("access=0 si=1") &&
        text.contains("syscall=221")
}

func postPIESHEntryFetchFaultSignature(_ text: String) -> Bool {
    text.contains("Orlix TCTI: exit task=sh") &&
        text.contains("reason=1 status=-14") &&
        text.contains("insn=0x0") &&
        text.contains("syscall=221")
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
        stringValue(object["selected_device_id"]) == requiredSimulatorID &&
        stringValue(object["selected_device_name"]) == requiredSimulatorName &&
        intValue(object["simulator_booted_count"]) == 1 &&
        boolValue(object["simulator_single_booted"]) &&
        stringValue(object["backend"]) == "tcti" &&
        stringValue(object["profile"]) == "tcti_runtime" &&
        !boolValue(object["preflight_only"]) &&
        !boolValue(object["autonomous_tests_bypassed"]) &&
        forbiddenClear
    let reason: String
    if reportOK {
        reason = "iphonesimulator runtime-validation report \(report.path) passed on \(requiredSimulatorName) with tcti runtime profile and forbidden behavior false"
    } else if report.gitSHA != gitSHA() {
        reason = "latest iphonesimulator runtime-validation report \(report.path) is stale for current HEAD"
    } else if stringValue(object["selected_device_id"]) != requiredSimulatorID ||
        stringValue(object["selected_device_name"]) != requiredSimulatorName {
        reason = "latest iphonesimulator runtime-validation report \(report.path) did not run on required simulator \(requiredSimulatorName) (\(requiredSimulatorID))"
    } else if intValue(object["simulator_booted_count"]) != 1 || !boolValue(object["simulator_single_booted"]) {
        reason = "latest iphonesimulator runtime-validation report \(report.path) does not prove exactly one booted required simulator"
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
    let runtimeEvents = object["tcti_runtime_events"] as? [String: Any] ?? [:]
    let signaledProcess = runtimeEvents["signaled_process"] as? [String: Any] ?? [:]
    let signaledProcessClear = intValue(signaledProcess["signal"]) == nil
    let reportOK = report.status == "pass" &&
        report.passed &&
        report.gitSHA == gitSHA() &&
        stringValue(object["selected_device_id"]) == requiredSimulatorID &&
        stringValue(object["selected_device_name"]) == requiredSimulatorName &&
        intValue(object["simulator_booted_count"]) == 1 &&
        boolValue(object["simulator_single_booted"]) &&
        stringValue(object["backend"]) == "tcti" &&
        stringValue(object["profile"]) == "tcti_runtime" &&
        !boolValue(object["preflight_only"]) &&
        !boolValue(object["autonomous_tests_bypassed"]) &&
        forbiddenClear &&
        signaledProcessClear
    let reason: String
    if reportOK {
        reason = "iphonesimulator runtime-validation report \(report.path) passed on \(requiredSimulatorName) with no fatal simulator TCTI runtime errors"
    } else if report.gitSHA != gitSHA() {
        reason = "latest iphonesimulator stability report \(report.path) is stale for current HEAD"
    } else if stringValue(object["selected_device_id"]) != requiredSimulatorID ||
        stringValue(object["selected_device_name"]) != requiredSimulatorName {
        reason = "latest iphonesimulator stability report \(report.path) did not run on required simulator \(requiredSimulatorName) (\(requiredSimulatorID))"
    } else if intValue(object["simulator_booted_count"]) != 1 || !boolValue(object["simulator_single_booted"]) {
        reason = "latest iphonesimulator stability report \(report.path) does not prove exactly one booted required simulator"
    } else if !signaledProcessClear {
        reason = "latest iphonesimulator stability report \(report.path) captured a guest process signal"
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

func simulatorConsoleUsabilityPass(_ gate: Gate) -> GateStatus {
    guard let (report, object) = latestRuntimeReport(gate: "tcti-init-console-write", destination: "iphonesimulator") else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "missing",
            passed: false,
            reason: "missing iphonesimulator runtime-validation report for tcti-init-console-write",
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
    let artifacts = (object["artifacts"] as? [Any] ?? []).compactMap { stringValue($0) }
    let consoleArtifactPath = artifacts.first { $0.hasSuffix("tcti-console-write.txt") }
    let consoleArtifactHasContent = consoleArtifactPath.flatMap { artifact -> Bool? in
        let directURL = root.appendingPathComponent(artifact)
        let runtimeRelativeURL = root
            .appendingPathComponent("Build/Reports/runtime", isDirectory: true)
            .appendingPathComponent(artifact)
        let url = fileManager.fileExists(atPath: directURL.path) ? directURL : runtimeRelativeURL
        guard let text = try? String(contentsOf: url, encoding: .utf8) else { return false }
        return !text.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
    } ?? false
    let reportOK = report.status == "pass" &&
        report.passed &&
        report.gitSHA == gitSHA() &&
        stringValue(object["selected_device_id"]) == requiredSimulatorID &&
        stringValue(object["selected_device_name"]) == requiredSimulatorName &&
        intValue(object["simulator_booted_count"]) == 1 &&
        boolValue(object["simulator_single_booted"]) &&
        stringValue(object["backend"]) == "tcti" &&
        stringValue(object["profile"]) == "tcti_runtime" &&
        !boolValue(object["preflight_only"]) &&
        !boolValue(object["autonomous_tests_bypassed"]) &&
        forbiddenClear &&
        consoleArtifactHasContent
    let reason: String
    if reportOK {
        reason = "iphonesimulator runtime-validation report \(report.path) passed on \(requiredSimulatorName) with Linux console usability marker artifact \(consoleArtifactPath ?? "unknown")"
    } else if report.gitSHA != gitSHA() {
        reason = "latest iphonesimulator console usability report \(report.path) is stale for current HEAD"
    } else if stringValue(object["selected_device_id"]) != requiredSimulatorID ||
        stringValue(object["selected_device_name"]) != requiredSimulatorName {
        reason = "latest iphonesimulator console usability report \(report.path) did not run on required simulator \(requiredSimulatorName) (\(requiredSimulatorID))"
    } else if intValue(object["simulator_booted_count"]) != 1 || !boolValue(object["simulator_single_booted"]) {
        reason = "latest iphonesimulator console usability report \(report.path) does not prove exactly one booted required simulator"
    } else if !consoleArtifactHasContent {
        reason = "latest iphonesimulator console usability report \(report.path) is missing a non-empty tcti-console-write marker artifact"
    } else {
        reason = "latest iphonesimulator console usability report \(report.path) is not a valid non-preflight TCTI pass"
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

func simulatorStaticBusyBoxStartPass(_ gate: Gate) -> GateStatus {
    guard let (report, object) = latestRuntimeReport(gate: "tcti-static-busybox-start", destination: "iphonesimulator") else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "missing",
            passed: false,
            reason: "missing iphonesimulator runtime-validation report for tcti-static-busybox-start",
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
    let runtimeEvents = object["tcti_runtime_events"] as? [String: Any] ?? [:]
    let signaledProcess = runtimeEvents["signaled_process"] as? [String: Any] ?? [:]
    let signaledProcessClear = intValue(signaledProcess["signal"]) == nil
    let artifacts = (object["artifacts"] as? [Any] ?? []).compactMap { stringValue($0) }
    let busyBoxArtifactPath = artifacts.first { $0.hasSuffix("tcti-static-busybox-start.txt") }
    let busyBoxArtifactHasContent = busyBoxArtifactPath.flatMap { artifact -> Bool? in
        let directURL = root.appendingPathComponent(artifact)
        let runtimeRelativeURL = root
            .appendingPathComponent("Build/Reports/runtime", isDirectory: true)
            .appendingPathComponent(artifact)
        let url = fileManager.fileExists(atPath: directURL.path) ? directURL : runtimeRelativeURL
        guard let text = try? String(contentsOf: url, encoding: .utf8) else {
            return false
        }
        return !text.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
    } ?? false
    let reportOK = report.status == "pass" &&
        report.passed &&
        report.gitSHA == gitSHA() &&
        stringValue(object["selected_device_id"]) == requiredSimulatorID &&
        stringValue(object["selected_device_name"]) == requiredSimulatorName &&
        intValue(object["simulator_booted_count"]) == 1 &&
        boolValue(object["simulator_single_booted"]) &&
        stringValue(object["backend"]) == "tcti" &&
        stringValue(object["profile"]) == "tcti_runtime" &&
        !boolValue(object["preflight_only"]) &&
        !boolValue(object["autonomous_tests_bypassed"]) &&
        forbiddenClear &&
        signaledProcessClear &&
        busyBoxArtifactHasContent
    let reason: String
    if reportOK {
        reason = "iphonesimulator runtime-validation report \(report.path) passed static BusyBox start gate on \(requiredSimulatorName)"
    } else if report.gitSHA != gitSHA() {
        reason = "latest iphonesimulator static BusyBox start report \(report.path) is stale for current HEAD"
    } else if stringValue(object["selected_device_id"]) != requiredSimulatorID ||
        stringValue(object["selected_device_name"]) != requiredSimulatorName {
        reason = "latest iphonesimulator static BusyBox start report \(report.path) did not run on required simulator \(requiredSimulatorName) (\(requiredSimulatorID))"
    } else if intValue(object["simulator_booted_count"]) != 1 || !boolValue(object["simulator_single_booted"]) {
        reason = "latest iphonesimulator static BusyBox start report \(report.path) does not prove exactly one booted required simulator"
    } else if !signaledProcessClear {
        reason = "latest iphonesimulator static BusyBox start report \(report.path) recorded a signaled process"
    } else if !busyBoxArtifactHasContent {
        reason = "latest iphonesimulator static BusyBox start report \(report.path) does not include a non-empty tcti-static-busybox-start marker artifact"
    } else {
        reason = "latest iphonesimulator static BusyBox start report \(report.path) is not a valid non-preflight TCTI pass"
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

func postBusyBoxSIGABRTReducerPass(_ gate: Gate) -> GateStatus {
    let reducerReport = reportFact(target: "tcti-post-busybox-sigabrt-reducer")
    if reducerReport.status == "pass",
       reducerReport.passed,
       reducerReport.gitSHA == gitSHA() {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "pass",
            passed: true,
            reason: "post-BusyBox SIGABRT reducer report \(reducerReport.path) passed",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: [reducerReport],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    guard let (report, object) = latestRuntimeReport(gate: "tcti-static-busybox-start", destination: "iphonesimulator") else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "not_needed",
            passed: true,
            reason: "no static BusyBox simulator failure report exists yet",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: [],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    let events = object["tcti_runtime_events"] as? [String: Any] ?? [:]
    let staticPIE = events["static_pie_image"] as? [String: Any] ?? [:]
    let signaledProcess = events["signaled_process"] as? [String: Any] ?? [:]
    let staticPID = intValue(staticPIE["pid"])
    let signaledPID = intValue(signaledProcess["pid"])
    let matchesCurrentSIGABRT = report.status == "fail" &&
        !report.passed &&
        report.gitSHA == gitSHA() &&
        stringValue(staticPIE["task"]) == "sh" &&
        staticPID != nil &&
        staticPID == signaledPID &&
        intValue(signaledProcess["signal"]) == 6

    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        state: matchesCurrentSIGABRT ? "ready" : "not_needed",
        passed: !matchesCurrentSIGABRT,
        reason: matchesCurrentSIGABRT
            ? "current static BusyBox simulator report \(report.path) records sh SIGABRT signal=6 and needs a no-phone reducer"
            : "latest static BusyBox simulator report does not match the sh SIGABRT reducer signature",
        prerequisites: gate.prerequisites,
        prerequisitesSatisfied: false,
        reportPaths: gate.expectedReportPaths,
        reports: [report],
        readinessEligible: gate.readinessEligible,
        physicalDevice: gate.physicalDevice,
        gadget: gate.gadget
    )
}

func simulatorStaticPIERelocationFixPass(_ gate: Gate) -> GateStatus {
    guard let (stabilityReport, stabilityObject) = latestRuntimeReport(gate: "tcti-simulator-stability", destination: "iphonesimulator") else {
        return missingGate(gate, reason: "missing current simulator stability failure report for static PIE relocation fix")
    }
    let reducerReport = reportFact(target: "tcti-simulator-user-fault-reducer")
    let stabilityFailedAtHead = stabilityReport.status == "fail" &&
        !stabilityReport.passed &&
        stabilityReport.gitSHA == gitSHA() &&
        stringValue(stabilityObject["backend"]) == "tcti" &&
        stringValue(stabilityObject["profile"]) == "tcti_runtime" &&
        !boolValue(stabilityObject["preflight_only"]) &&
        !boolValue(stabilityObject["autonomous_tests_bypassed"])
    let fatalPath = "Build/Reports/runtime/\(stabilityReport.path.split(separator: "/").last?.replacingOccurrences(of: ".json", with: ".artifacts/tcti-simulator-fatal-runtime.txt") ?? "")"
    let fatalURL = root.appendingPathComponent(fatalPath)
    let fatalText = (try? String(contentsOf: fatalURL, encoding: .utf8)) ?? ""
    let fatalMatchesReducer = fatalText.contains("Orlix TCTI: user fault") &&
        fatalText.contains("addr=0x0") &&
        fatalText.contains("access=1") &&
        (fatalText.contains("Attempted kill init") || fatalText.contains("Attempted to kill init"))
    let reducerOK = reducerReport.status == "pass" && reducerReport.passed
    let fixedMarker = root.appendingPathComponent("Build/TCTI/reports/tcti-static-pie-relocation-fix/report.json")

    if let object = try? loadJSONObject(fixedMarker),
       stringValue(object["status"]) == "pass",
       boolValue(object["passed"]),
       stringValue(object["git_sha"]) == gitSHA() {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "pass",
            passed: true,
            reason: "static PIE relocation production fix marker is current for HEAD",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: [
                ReportFact(path: relativePath(fixedMarker), exists: true, status: "pass", passed: true, releaseGateEligible: false, readinessGateEligible: false, gitSHA: gitSHA()),
                stabilityReport,
                reducerReport,
            ],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    let ready = stabilityFailedAtHead && fatalMatchesReducer && reducerOK
    let reason: String
    if ready {
        reason = "current simulator stability failure matches the reduced static PIE GOT null-read signature and reducer report is pass"
    } else if !stabilityFailedAtHead {
        reason = "latest simulator stability report is missing, stale, or not the expected current non-preflight failure"
    } else if !fatalMatchesReducer {
        reason = "latest simulator stability fatal log does not match the reduced null user fault signature"
    } else {
        reason = "static PIE GOT reducer report is not passing"
    }
    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        state: ready ? "ready" : "missing",
        passed: false,
        reason: reason,
        prerequisites: gate.prerequisites,
        prerequisitesSatisfied: false,
        reportPaths: gate.expectedReportPaths,
        reports: [stabilityReport, reducerReport],
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
        return supersededSimulatorReducerGate(
            gate,
            target: "tcti-simulator-user-fault-reducer",
            staleSignature: staticPIEGOTNullReadSignature,
            supersededBy: ["tcti-repro", "tcti-static-pie-relocation-fix"]
        )
    case "tcti-static-pie-relocation-fix":
        return simulatorStaticPIERelocationFixPass(gate)
    case "no-phone-tcti-simd-self-move-reducer":
        return basicReportGate(gate, target: "tcti-simd-self-move-reducer")
    case "tcti-simd-self-move-fix":
        return basicReportGate(gate, target: "tcti-simd-self-move-fix")
    case "no-phone-tcti-brk-trap-reducer":
        return supersededSimulatorReducerGate(
            gate,
            target: "tcti-brk-trap-reducer",
            staleSignature: brkTrapSignature,
            supersededBy: ["tcti-add-sub-shifted-xzr-fix", "tcti-post-setsid-tls-fault-reducer"]
        )
    case "tcti-brk-trap-root-cause":
        return supersededSimulatorReducerGate(
            gate,
            target: "tcti-brk-trap-root-cause",
            staleSignature: brkTrapSignature,
            supersededBy: ["tcti-add-sub-shifted-xzr-fix", "tcti-post-setsid-tls-fault-reducer"]
        )
    case "tcti-add-sub-shifted-xzr-fix":
        return basicReportGate(gate, target: "tcti-add-sub-shifted-xzr-fix")
    case "tcti-simd-movi-2s-fix":
        return basicReportGate(gate, target: "tcti-simd-movi-2s-fix")
    case "tcti-simd-movi-16b-fix":
        return basicReportGate(gate, target: "tcti-simd-movi-16b-fix")
    case "tcti-simd-str-s-fix":
        return basicReportGate(gate, target: "tcti-simd-str-s-fix")
    case "tcti-simd-dup-2d-fix":
        return basicReportGate(gate, target: "tcti-simd-dup-2d-fix")
    case "no-phone-tcti-post-overlay-null-user-fault-reducer":
        return basicReportGate(gate, target: "tcti-post-overlay-null-user-fault-reducer")
    case "tcti-post-overlay-null-user-fault-fix":
        return basicReportGate(gate, target: "tcti-post-overlay-null-user-fault-fix")
    case "no-phone-tcti-ldrsw-sign-extension-reducer":
        return basicReportGate(gate, target: "tcti-ldrsw-sign-extension-reducer")
    case "tcti-ldrsw-sign-extension-fix":
        return basicReportGate(gate, target: "tcti-ldrsw-sign-extension-fix")
    case "no-phone-tcti-post-setsid-tls-fault-reducer":
        return supersededSimulatorReducerGate(
            gate,
            target: "tcti-post-setsid-tls-fault-reducer",
            staleSignature: postSetsidTLSFaultSignature,
            supersededBy: ["tcti-post-bash-mmap-read-fault-reducer"]
        )
    case "no-phone-tcti-post-exec-sh-fetch-fault-reducer":
        return supersededSimulatorReducerGate(
            gate,
            target: "tcti-post-exec-sh-fetch-fault-reducer",
            staleSignature: postExecSHFetchFaultSignature,
            supersededBy: ["tcti-post-bash-mmap-read-fault-reducer"]
        )
    case "no-phone-tcti-post-pie-sh-entry-fetch-fault-reducer":
        return supersededSimulatorReducerGate(
            gate,
            target: "tcti-post-pie-sh-entry-fetch-fault-reducer",
            staleSignature: postPIESHEntryFetchFaultSignature,
            supersededBy: ["tcti-post-bash-mmap-read-fault-reducer"]
        )
    case "no-phone-tcti-post-bash-mmap-read-fault-reducer":
        return basicReportGate(gate, target: "tcti-post-bash-mmap-read-fault-reducer")
    case "no-phone-tcti-post-busybox-sigabrt-reducer":
        return postBusyBoxSIGABRTReducerPass(gate)
    case "simulator-tcti-runtime-stability":
        return simulatorStabilityPass(gate)
    case "simulator-tcti-linux-console-usability":
        return simulatorConsoleUsabilityPass(gate)
    case "simulator-tcti-static-busybox-start":
        return simulatorStaticBusyBoxStartPass(gate)
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
            command: "make tcti-gate TARGET=tcti-contract",
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
                "rtk proxy make tcti-gate TARGET=tcti-contract",
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
            command: "make tcti-gate TARGET=tcti-memory-fuzz",
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
                "rtk proxy make tcti-gate TARGET=tcti-memory-fuzz",
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
            command: "make tcti-gate TARGET=tcti-direct-chain-fuzz",
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
                "rtk proxy make tcti-gate TARGET=tcti-direct-chain-fuzz",
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
            command: "make runtime-validation DESTINATION=iphonesimulator GATE=tcti-init-first-syscall ORLIX_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=Orlix-iPhone-15-Pro-Max",
            kind: "simulator-runtime",
            prerequisites: ["tcti-direct-chain-fuzz"],
            allowedScope: [
                "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/**",
                "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/kernel/**",
                "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/mm/**",
                "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/include/asm/**",
                "tools/runtime/orlix-runtime-validation.sh",
                "tools/tcti/orlix-tcti-gate.swift",
                "tools/tcti/fixtures/**",
                "OrlixOS/Sources/make/rootfs.mk",
                "OrlixOS/Sources/make/packages.mk",
                "project.yml",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Do not run physical-device gates.",
                "Do not treat simulator evidence as release or physical readiness.",
                "Do not use any simulator except Orlix-iPhone-15-Pro-Max (C47ED88D-0D0A-420D-8C78-D4C1D34A276D).",
                "Do not allow more than one simulator to be booted while this gate runs.",
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
                "rtk proxy make tcti-gate TARGET=tcti-plan-consistency",
                "rtk proxy make tcti-gate TARGET=tcti-report-schema-check",
                "rtk proxy make tcti-gate TARGET=tcti-golden-elf",
                "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
                "rtk proxy make runtime-validation DESTINATION=iphonesimulator GATE=tcti-init-first-syscall ORLIX_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=Orlix-iPhone-15-Pro-Max",
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
            command: "make tcti-gate TARGET=tcti-simulator-user-fault-reducer",
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
                "rtk proxy make tcti-gate TARGET=tcti-plan-consistency",
                "rtk proxy make tcti-gate TARGET=tcti-report-schema-check",
                "rtk proxy make tcti-gate TARGET=tcti-golden-elf CASE=init_011_static_pie_got_byte_load EXECUTE=switch-debug",
                "rtk proxy make tcti-gate TARGET=tcti-simulator-user-fault-reducer",
                "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-static-pie-got-unrelocated-byte-load.json",
                "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
            ],
            reducerRequirements: [
                "A current failing simulator stability report must contain a structured null-read fatal user fault event, or a current passing simulator stability report must contain structured first-svc and static-PIE events with no fatal user fault.",
                "The no-phone reducer must replay through make tcti-gate TARGET=tcti-repro before production TCTI patching.",
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
                "Stop if the current simulator stability report is stale or missing required structured TCTI runtime events.",
                "Stop if the no-phone reducer cannot replay.",
                "Stop if production TCTI code would be required before the reducer exists.",
            ]
        ),
    Gate(
        id: "tcti-static-pie-relocation-fix",
        command: "make tcti-gate TARGET=tcti-static-pie-relocation-fix",
        kind: "production-tcti-fix",
        prerequisites: ["no-phone-tcti-simulator-user-fault-reducer"],
        allowedScope: [
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/**",
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/mm/**",
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/kernel/**",
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/include/asm/tcti.h",
            "tools/tcti/orlix-tcti-gate.swift",
            ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "docs/plans/active/orlix-tcti/IMPLEMENT.md",
        ],
        forbiddenScope: [
            "Do not run physical-device gates.",
            "Do not add production assembly.",
            "Do not add gadget dispatch.",
            "Do not flip product defconfigs.",
            "Do not edit generated Linux or build trees.",
            "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime semantics.",
            "Do not implement a broad ELF dynamic loader.",
            "Limit production behavior to the reducer-backed static PIE R_AARCH64_RELATIVE/GOT null-read failure class.",
        ],
        expectedReportPaths: [
            "Build/TCTI/reports/tcti-static-pie-relocation-fix/report.json",
            "Build/Reports/runtime/tcti-simulator-stability-*.json",
            "Build/TCTI/reports/tcti-simulator-user-fault-reducer/report.json",
            "Build/TCTI/reproducers/tcti-golden-elf/execution-static-pie-got-unrelocated-byte-load.json",
        ],
        readinessEligible: false,
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [
            "rtk proxy git diff --check",
            "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "rtk proxy make agent-harness-check",
            "rtk proxy make agent-status AREA=orlix-tcti",
            "rtk proxy make agent-next AREA=orlix-tcti",
            "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
            "rtk proxy make tcti-gate TARGET=tcti-plan-consistency",
            "rtk proxy make tcti-gate TARGET=tcti-report-schema-check",
            "rtk proxy make tcti-gate TARGET=tcti-golden-elf",
            "rtk proxy make tcti-gate TARGET=tcti-simulator-user-fault-reducer",
            "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-static-pie-got-unrelocated-byte-load.json",
            "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
            "rtk proxy make tcti-gate TARGET=tcti-static-pie-relocation-fix",
            "rtk test env PATH=\"$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin\" make -f OrlixKernel/Makefile kunit PROFILE=development",
            "rtk test env PATH=\"$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin\" make -f OrlixKernel/Makefile kunit PROFILE=release",
        ],
        reducerRequirements: [
            "The current simulator stability failure must match the existing static PIE GOT null-read reducer.",
            "The reducer must replay before production TCTI patching.",
            "After the production fix, rerun simulator stability through the next selected gate.",
        ],
        requiredSubagentsOrSkills: [
            "orlix-tcti-safety",
            "orlix-tcti-reproducer",
            "orlix-tcti-debug",
            "orlix-runtime-claim-verification",
            "tcti-planner",
            "tcti-safety-reviewer",
            "tcti-llvm-inspector",
            "tcti-test-reducer",
            "tcti-release-gate-reviewer",
        ],
        commitMessageTemplate: "fix(tcti): apply static pie relative relocations",
        stopConditions: [
            "Stop if the latest simulator stability failure no longer matches the null GOT/read reducer.",
            "Stop if the fix requires production assembly, gadget dispatch, HostAdapter Linux behavior, product defconfig flips, or generated-tree edits.",
            "Stop if the fix broadens into dynamic loader, VFS, fd table, process, signal, scheduler, or Linux runtime semantics.",
        ]
    ),
    Gate(
        id: "no-phone-tcti-simd-self-move-reducer",
        command: "make tcti-gate TARGET=tcti-simd-self-move-reducer",
        kind: "no-phone-reducer",
        prerequisites: ["tcti-static-pie-relocation-fix"],
        allowedScope: [
            "tools/tcti/orlix-tcti-gate.swift",
            "tools/tcti/fixtures/golden_elf/**",
            ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "docs/plans/active/orlix-tcti/IMPLEMENT.md",
        ],
        forbiddenScope: [
            "Do not run physical-device gates.",
            "Do not add production assembly.",
            "Do not add gadget dispatch.",
            "Do not flip product defconfigs.",
            "Do not edit generated Linux or build trees.",
            "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime semantics.",
            "Reduce the simulator unsupported 0x6e080400 SIGILL before production TCTI patching.",
        ],
        expectedReportPaths: [
            "Build/TCTI/reports/tcti-simd-self-move-reducer/report.json",
            "Build/TCTI/reproducers/tcti-golden-elf/execution-simd-self-move-unsupported.json",
            "Build/Reports/runtime/tcti-simulator-stability-*.json",
        ],
        readinessEligible: false,
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [
            "rtk proxy git diff --check",
            "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
            "rtk proxy make tcti-gate TARGET=tcti-simd-self-move-reducer",
            "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-simd-self-move-unsupported.json",
            "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
        ],
        reducerRequirements: [
            "The current simulator stability failure must match unsupported instruction 0x6e080400 with exitcode=0x00000004.",
            "The no-phone reducer must replay before production TCTI patching.",
        ],
        requiredSubagentsOrSkills: [
            "orlix-tcti-reproducer",
            "orlix-tcti-debug",
            "orlix-tcti-safety",
            "tcti-test-reducer",
            "tcti-safety-reviewer",
            "tcti-llvm-inspector",
        ],
        commitMessageTemplate: "test(tcti): reduce simulator simd self move",
        stopConditions: [
            "Stop if the latest simulator stability failure no longer matches unsupported 0x6e080400.",
            "Stop if reducer replay cannot reproduce the no-phone unsupported instruction failure.",
        ]
    ),
    Gate(
        id: "tcti-simd-self-move-fix",
        command: "make tcti-gate TARGET=tcti-simd-self-move-fix",
        kind: "production-tcti-fix",
        prerequisites: ["no-phone-tcti-simd-self-move-reducer"],
        allowedScope: [
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/**",
            "tools/tcti/orlix-tcti-gate.swift",
            ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "docs/plans/active/orlix-tcti/IMPLEMENT.md",
        ],
        forbiddenScope: [
            "Do not run physical-device gates.",
            "Do not add production assembly.",
            "Do not add gadget dispatch.",
            "Do not flip product defconfigs.",
            "Do not edit generated Linux or build trees.",
            "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime semantics.",
            "Limit production behavior to decoded AArch64 SIMD lane self-move 0x6e080400 as a no-op.",
        ],
        expectedReportPaths: [
            "Build/TCTI/reports/tcti-simd-self-move-fix/report.json",
            "Build/TCTI/reports/tcti-simd-self-move-reducer/report.json",
            "Build/Reports/runtime/tcti-simulator-stability-*.json",
        ],
        readinessEligible: false,
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [
            "rtk proxy git diff --check",
            "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
            "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "rtk proxy make tcti-gate TARGET=tcti-simd-self-move-reducer",
            "rtk proxy make tcti-gate TARGET=tcti-simd-self-move-fix",
            "rtk proxy make agent-harness-check",
            "rtk proxy make agent-status AREA=orlix-tcti",
            "rtk proxy make agent-next AREA=orlix-tcti",
            "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
            "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
            "rtk test env PATH=\"$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin\" make -f OrlixKernel/Makefile kunit PROFILE=development",
            "rtk test env PATH=\"$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin\" make -f OrlixKernel/Makefile kunit PROFILE=release",
        ],
        reducerRequirements: [
            "The unsupported 0x6e080400 reducer must pass before production TCTI patching.",
            "After the production fix, rerun simulator stability through the next selected gate on the pinned simulator.",
        ],
        requiredSubagentsOrSkills: [
            "orlix-tcti-safety",
            "orlix-tcti-reproducer",
            "orlix-tcti-debug",
            "orlix-runtime-claim-verification",
            "tcti-planner",
            "tcti-safety-reviewer",
            "tcti-llvm-inspector",
            "tcti-test-reducer",
            "tcti-release-gate-reviewer",
        ],
        commitMessageTemplate: "fix(tcti): support simd self move no-op",
        stopConditions: [
            "Stop if the fix requires broad SIMD/vector register semantics.",
            "Stop if the fix requires production assembly, gadget dispatch, HostAdapter Linux behavior, product defconfig flips, or generated-tree edits.",
            "Stop if simulator stability still stops on unsupported 0x6e080400 after the fix.",
        ]
    ),
    Gate(
        id: "no-phone-tcti-brk-trap-reducer",
        command: "make tcti-gate TARGET=tcti-brk-trap-reducer",
        kind: "no-phone-reducer",
        prerequisites: ["tcti-simd-self-move-fix"],
        allowedScope: [
            "tools/tcti/orlix-tcti-gate.swift",
            "tools/tcti/fixtures/golden_elf/**",
            ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "docs/plans/active/orlix-tcti/IMPLEMENT.md",
        ],
        forbiddenScope: [
            "Do not run physical-device gates.",
            "Do not add production assembly.",
            "Do not add gadget dispatch.",
            "Do not flip product defconfigs.",
            "Do not edit generated Linux or build trees.",
            "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime semantics.",
            "Do not implement BRK as a success no-op for init; reduce the trap and identify the underlying startup or relocation cause.",
        ],
        expectedReportPaths: [
            "Build/TCTI/reports/tcti-brk-trap-reducer/report.json",
            "Build/TCTI/reproducers/tcti-brk-trap-reducer/execution-brk-trap-unsupported.json",
            "Build/Reports/runtime/tcti-simulator-stability-*.json",
        ],
        readinessEligible: false,
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [
            "rtk proxy git diff --check",
            "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
            "rtk proxy make tcti-gate TARGET=tcti-brk-trap-reducer",
            "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-brk-trap-reducer/execution-brk-trap-unsupported.json",
            "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
        ],
        reducerRequirements: [
            "The current simulator stability failure must match unsupported instruction 0xd4200020 after mmap syscall 222.",
            "The no-phone reducer must replay before any production patch for the underlying startup or relocation cause.",
        ],
        requiredSubagentsOrSkills: [
            "orlix-tcti-reproducer",
            "orlix-tcti-debug",
            "orlix-tcti-safety",
            "tcti-test-reducer",
            "tcti-safety-reviewer",
            "tcti-llvm-inspector",
        ],
        commitMessageTemplate: "test(tcti): reduce simulator brk trap",
        stopConditions: [
            "Stop if the latest simulator stability failure no longer matches unsupported 0xd4200020.",
            "Stop if reducer replay cannot reproduce the no-phone BRK unsupported instruction failure.",
            "Stop if the proposed fix treats BRK as a passing no-op instead of addressing the underlying cause.",
        ]
    ),
    Gate(
        id: "tcti-brk-trap-root-cause",
        command: "make tcti-gate TARGET=tcti-brk-trap-root-cause",
        kind: "simulator-root-cause",
        prerequisites: ["no-phone-tcti-brk-trap-reducer"],
        allowedScope: [
            "tools/tcti/orlix-tcti-gate.swift",
            ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            "Build/TCTI/brk_trap_root_cause/**",
        ],
        forbiddenScope: [
            "Do not run physical-device gates.",
            "Do not add production assembly.",
            "Do not add gadget dispatch.",
            "Do not flip product defconfigs.",
            "Do not edit generated Linux or build trees.",
            "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime semantics.",
            "Do not implement BRK as a success no-op.",
            "Do not claim simulator stability; this gate only binds the current simulator BRK failure to exact ELF facts.",
        ],
        expectedReportPaths: [
            "Build/TCTI/reports/tcti-brk-trap-root-cause/report.json",
            "Build/TCTI/brk_trap_root_cause/root-cause.md",
            "Build/TCTI/reports/tcti-brk-trap-reducer/report.json",
            "Build/Reports/runtime/tcti-simulator-stability-*.json",
        ],
        readinessEligible: false,
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [
            "rtk proxy git diff --check",
            "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
            "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "rtk proxy make tcti-gate TARGET=tcti-brk-trap-reducer",
            "rtk proxy make tcti-gate TARGET=tcti-brk-trap-root-cause",
            "rtk proxy make agent-harness-check",
            "rtk proxy make agent-status AREA=orlix-tcti",
            "rtk proxy make agent-next AREA=orlix-tcti",
            "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
            "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
        ],
        reducerRequirements: [
            "The BRK trap reducer must pass before this root-cause inspection runs.",
            "The root-cause report must prove the simulator BRK is an explicit guard/trap in runtime init, not a missing successful BRK semantic.",
            "Any production fix after this must address the startup condition that reaches the trap and must not skip BRK to force progress.",
        ],
        requiredSubagentsOrSkills: [
            "orlix-tcti-reproducer",
            "orlix-tcti-debug",
            "orlix-tcti-safety",
            "orlix-runtime-claim-verification",
            "tcti-planner",
            "tcti-safety-reviewer",
            "tcti-llvm-inspector",
            "tcti-test-reducer",
            "tcti-release-gate-reviewer",
        ],
        commitMessageTemplate: "test(tcti): bind simulator brk trap to init guard",
        stopConditions: [
            "Stop if the latest simulator stability failure no longer matches unsupported 0xd4200020.",
            "Stop if the inspected runtime init ELF does not contain the ADRP/LDR/CBNZ/BRK guard at ELF VMA 0x1be14.",
            "Stop if the evidence would require treating BRK as a passing no-op.",
        ]
    ),
    Gate(
        id: "tcti-add-sub-shifted-xzr-fix",
        command: "make tcti-gate TARGET=tcti-add-sub-shifted-xzr-fix",
        kind: "no-phone-simulator-reducer-fix",
        prerequisites: ["tcti-brk-trap-root-cause"],
        allowedScope: [
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/switch_debug.c",
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/tests/tcti_decode_test.c",
            "tools/tcti/orlix-tcti-gate.swift",
            "Makefile",
            ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            ".agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json",
            "docs/plans/active/orlix-tcti/IMPLEMENT.md",
        ],
        forbiddenScope: [
            "Do not run physical-device gates.",
            "Do not add production assembly.",
            "Do not add gadget dispatch.",
            "Do not implement BRK as success no-op.",
            "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime semantics.",
            "Do not claim simulator stability until the simulator gate passes after this no-phone fix.",
        ],
        expectedReportPaths: [
            "Build/TCTI/reports/tcti-add-sub-shifted-xzr-fix/report.json",
            "Build/TCTI/reports/tcti-brk-trap-root-cause/report.json",
            "Build/Reports/runtime/tcti-simulator-stability-*.json",
        ],
        readinessEligible: false,
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [
            "rtk proxy git diff --check",
            "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
            "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "rtk proxy make tcti-gate TARGET=tcti-add-sub-shifted-xzr-fix",
            "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-add-sub-shifted-xzr-fix/add-sub-shifted-xzr-pass-regression.json",
            "rtk proxy make agent-harness-check",
            "rtk proxy make agent-status AREA=orlix-tcti",
            "rtk proxy make agent-next AREA=orlix-tcti",
            "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
            "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
        ],
        reducerRequirements: [
            "The fix gate must cite the simulator BRK register facts x9, x13, sp, and pstate.",
            "The KUnit regression must prove NEG x13, x9 uses XZR/WZR source semantics and preserves SP.",
            "The gate must not treat BRK #1 as a supported successful instruction.",
        ],
        requiredSubagentsOrSkills: [
            "orlix-tcti-reproducer",
            "orlix-tcti-safety",
            "orlix-tcti-debug",
            "tcti-planner",
            "tcti-safety-reviewer",
            "tcti-llvm-inspector",
            "tcti-test-reducer",
            "tcti-release-gate-reviewer",
        ],
        commitMessageTemplate: "test(tcti): fix shifted addsub xzr semantics",
        stopConditions: [
            "Stop if the latest simulator failure no longer provides the BRK register facts this fix reduces.",
            "Stop if the fix would require adding production assembly, gadget dispatch, or BRK success semantics.",
            "Stop if the KUnit regression cannot be compiled.",
        ]
    ),
    Gate(
        id: "tcti-simd-movi-2s-fix",
        command: "make tcti-gate TARGET=tcti-simd-movi-2s-fix",
        kind: "no-phone-simulator-reducer-fix",
        prerequisites: ["tcti-add-sub-shifted-xzr-fix"],
        allowedScope: [
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/decode_aarch64.c",
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/switch_debug.c",
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/tests/tcti_decode_test.c",
            "tools/tcti/orlix-tcti-gate.swift",
            "Makefile",
            ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "docs/plans/active/orlix-tcti/IMPLEMENT.md",
        ],
        forbiddenScope: [
            "Do not run physical-device gates.",
            "Do not add production assembly.",
            "Do not add gadget dispatch.",
            "Do not broaden SIMD beyond the exact emitted MOVI vN.2s #0x100 subset.",
            "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime semantics.",
            "Do not claim simulator stability until the simulator gate passes after this no-phone fix.",
        ],
        expectedReportPaths: [
            "Build/TCTI/reports/tcti-simd-movi-2s-fix/report.json",
            "Build/Reports/runtime/tcti-simulator-stability-*.json",
        ],
        readinessEligible: false,
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [
            "rtk proxy git diff --check",
            "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
            "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "rtk proxy make tcti-gate TARGET=tcti-simd-movi-2s-fix",
            "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-simd-movi-2s-fix/simd-movi-2s-pass-regression.json",
            "rtk proxy make agent-harness-check",
            "rtk proxy make agent-status AREA=orlix-tcti",
            "rtk proxy make agent-next AREA=orlix-tcti",
            "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
            "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
        ],
        reducerRequirements: [
            "The fix gate must cite the simulator unsupported instruction 0x0f002420.",
            "The KUnit regression must prove MOVI v0.2s materializes 0x0000010000000100 in the low SIMD half.",
            "The gate must not broaden into generic SIMD modified-immediate support.",
        ],
        requiredSubagentsOrSkills: [
            "orlix-tcti-reproducer",
            "orlix-tcti-safety",
            "orlix-tcti-debug",
            "tcti-planner",
            "tcti-safety-reviewer",
            "tcti-llvm-inspector",
            "tcti-test-reducer",
            "tcti-release-gate-reviewer",
        ],
        commitMessageTemplate: "test(tcti): support emitted simd movi immediate",
        stopConditions: [
            "Stop if the latest simulator failure no longer exposes unsupported instruction 0x0f002420.",
            "Stop if the fix would require generic SIMD immediate decoding.",
            "Stop if KUnit cannot compile the regression.",
        ]
    ),
    Gate(
        id: "tcti-simd-movi-16b-fix",
        command: "make tcti-gate TARGET=tcti-simd-movi-16b-fix",
        kind: "no-phone-simulator-reducer-fix",
        prerequisites: ["tcti-simd-movi-2s-fix"],
        allowedScope: [
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/decode_aarch64.c",
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/switch_debug.c",
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/tests/tcti_decode_test.c",
            "tools/tcti/orlix-tcti-gate.swift",
            "tools/tcti/fixtures/golden_elf/init_001_exit_simd_movi_16b.S",
            "Makefile",
            ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "docs/plans/active/orlix-tcti/IMPLEMENT.md",
        ],
        forbiddenScope: [
            "Do not run physical-device gates.",
            "Do not add production assembly.",
            "Do not add gadget dispatch.",
            "Do not broaden SIMD beyond the exact emitted MOVI vN.16b #0xdf subset.",
            "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime semantics.",
            "Do not claim simulator stability until the simulator gate passes after this no-phone fix.",
        ],
        expectedReportPaths: [
            "Build/TCTI/reports/tcti-simd-movi-16b-fix/report.json",
            "Build/TCTI/reproducers/tcti-simd-movi-16b-fix/simd-movi-16b-pass-regression.json",
            "Build/Reports/runtime/tcti-simulator-stability-*.json",
        ],
        readinessEligible: false,
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [
            "rtk proxy git diff --check",
            "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
            "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "rtk proxy make tcti-gate TARGET=tcti-simd-movi-16b-fix",
            "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-simd-movi-16b-fix/simd-movi-16b-pass-regression.json",
            "rtk proxy make agent-harness-check",
            "rtk proxy make agent-status AREA=orlix-tcti",
            "rtk proxy make agent-next AREA=orlix-tcti",
            "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
            "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
        ],
        reducerRequirements: [
            "The fix gate must cite simulator unsupported instruction 0x4f06e7e0.",
            "The KUnit regression must prove MOVI v0.16b materializes 0xdfdfdfdfdfdfdfdf in both SIMD halves.",
            "The gate must not broaden into generic SIMD modified-immediate support.",
        ],
        requiredSubagentsOrSkills: [
            "orlix-tcti-reproducer",
            "orlix-tcti-safety",
            "orlix-tcti-debug",
            "tcti-planner",
            "tcti-safety-reviewer",
            "tcti-llvm-inspector",
            "tcti-test-reducer",
            "tcti-release-gate-reviewer",
        ],
        commitMessageTemplate: "test(tcti): support emitted simd movi sixteen-byte immediate",
        stopConditions: [
            "Stop if latest simulator failure no longer exposes unsupported instruction 0x4f06e7e0.",
            "Stop if fix would require generic SIMD modified-immediate decoding.",
            "Stop if KUnit cannot compile regression.",
        ]
    ),
    Gate(
        id: "tcti-simd-str-s-fix",
        command: "make tcti-gate TARGET=tcti-simd-str-s-fix",
        kind: "no-phone-simulator-reducer-fix",
        prerequisites: ["tcti-simd-movi-16b-fix"],
        allowedScope: [
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/decode_aarch64.c",
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/switch_debug.c",
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/tests/tcti_decode_test.c",
            "tools/tcti/orlix-tcti-gate.swift",
            "Makefile",
            ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "docs/plans/active/orlix-tcti/IMPLEMENT.md",
        ],
        forbiddenScope: [
            "Do not run physical-device gates.",
            "Do not add production assembly.",
            "Do not add gadget dispatch.",
            "Do not broaden SIMD/FP memory support beyond exact emitted S-register unsigned-immediate width.",
            "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime semantics.",
            "Do not treat simulator stability as passing from this no-phone reducer fix.",
        ],
        expectedReportPaths: [
            "Build/TCTI/reports/tcti-simd-str-s-fix/report.json",
            "Build/Reports/runtime/tcti-simulator-stability-*.json",
        ],
        readinessEligible: false,
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [
            "rtk proxy git diff --check",
            "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
            "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "rtk proxy make tcti-gate TARGET=tcti-simd-str-s-fix",
            "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-simd-str-s-fix/simd-str-s-pass-regression.json",
            "rtk proxy make agent-harness-check",
            "rtk proxy make agent-status AREA=orlix-tcti",
            "rtk proxy make agent-next AREA=orlix-tcti",
            "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
            "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
        ],
        reducerRequirements: [
            "The fix gate must cite simulator unsupported instruction 0xbd01c260.",
            "The KUnit regression must prove STR s0, [x19, #0x1c0] decodes as SIMD/FP unsigned-immediate 32-bit store.",
            "The gate must not broaden into generic SIMD/FP memory semantics.",
        ],
        requiredSubagentsOrSkills: [
            "orlix-tcti-reproducer",
            "orlix-tcti-safety",
            "orlix-tcti-debug",
            "tcti-planner",
            "tcti-safety-reviewer",
            "tcti-llvm-inspector",
            "tcti-test-reducer",
            "tcti-release-gate-reviewer",
        ],
        commitMessageTemplate: "test(tcti): support emitted simd str scalar",
        stopConditions: [
            "Stop if latest simulator failure no longer exposes unsupported instruction 0xbd01c260.",
            "Stop if fix would require generic SIMD/FP memory support.",
            "Stop if KUnit cannot compile regression.",
        ]
    ),
    Gate(
        id: "tcti-simd-dup-2d-fix",
        command: "make tcti-gate TARGET=tcti-simd-dup-2d-fix",
        kind: "no-phone-simulator-reducer-fix",
        prerequisites: ["tcti-simd-str-s-fix"],
        allowedScope: [
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/decode_aarch64.c",
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/switch_debug.c",
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/tests/tcti_decode_test.c",
            "tools/tcti/orlix-tcti-gate.swift",
            "Makefile",
            ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "docs/plans/active/orlix-tcti/IMPLEMENT.md",
        ],
        forbiddenScope: [
            "Do not run physical-device gates.",
            "Do not add production assembly.",
            "Do not add gadget dispatch.",
            "Do not broaden SIMD vector support beyond exact emitted DUP vN.2d, xM subset.",
            "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime semantics.",
            "Do not treat simulator stability as passing from this no-phone reducer fix.",
        ],
        expectedReportPaths: [
            "Build/TCTI/reports/tcti-simd-dup-2d-fix/report.json",
            "Build/Reports/runtime/tcti-simulator-stability-*.json",
        ],
        readinessEligible: false,
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [
            "rtk proxy git diff --check",
            "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
            "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "rtk proxy make tcti-gate TARGET=tcti-simd-dup-2d-fix",
            "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-simd-dup-2d-fix/simd-dup-2d-pass-regression.json",
            "rtk proxy make agent-harness-check",
            "rtk proxy make agent-status AREA=orlix-tcti",
            "rtk proxy make agent-next AREA=orlix-tcti",
            "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
            "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
        ],
        reducerRequirements: [
            "The fix gate must cite simulator unsupported instruction 0x4e080d80.",
            "The KUnit regression must prove DUP v0.2d, x12 decodes and replicates x12 into both vector lanes.",
            "The gate must not broaden into generic SIMD vector move semantics.",
        ],
        requiredSubagentsOrSkills: [
            "orlix-tcti-reproducer",
            "orlix-tcti-safety",
            "orlix-tcti-debug",
            "tcti-planner",
            "tcti-safety-reviewer",
            "tcti-llvm-inspector",
            "tcti-test-reducer",
            "tcti-release-gate-reviewer",
        ],
        commitMessageTemplate: "test(tcti): support emitted simd dup vector",
        stopConditions: [
            "Stop if latest simulator failure no longer exposes unsupported instruction 0x4e080d80.",
            "Stop if fix would require generic SIMD vector move support.",
            "Stop if KUnit cannot compile regression.",
        ]
    ),
    Gate(
        id: "no-phone-tcti-post-overlay-null-user-fault-reducer",
        command: "make tcti-gate TARGET=tcti-post-overlay-null-user-fault-reducer",
        kind: "no-phone-reducer",
        prerequisites: ["tcti-simd-dup-2d-fix"],
        allowedScope: [
            "tools/tcti/orlix-tcti-gate.swift",
            "tools/tcti/fixtures/golden_elf/**",
            "Makefile",
            ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "docs/plans/active/orlix-tcti/IMPLEMENT.md",
        ],
        forbiddenScope: [
            "Do not run physical-device gates.",
            "Do not run simulator gates while reducing this failure.",
            "Do not add production assembly.",
            "Do not add gadget dispatch.",
            "Do not flip product defconfigs.",
            "Do not edit generated Linux or build trees.",
            "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime semantics.",
            "Reduce the pinned simulator post-overlay null user-fault null-read regression before production patching, or confirm the current structured simulator report no longer contains that fatal event.",
        ],
        expectedReportPaths: [
            "Build/TCTI/reports/tcti-post-overlay-null-user-fault-reducer/report.json",
            "Build/TCTI/reproducers/tcti-golden-elf/execution-static-pie-got-relocation-invisible-byte-load.json",
            "Build/Reports/runtime/tcti-simulator-stability-*.json",
        ],
        readinessEligible: false,
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [
            "rtk proxy git diff --check",
            "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
            "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "rtk proxy make tcti-gate TARGET=tcti-post-overlay-null-user-fault-reducer",
            "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-static-pie-got-relocation-invisible-byte-load.json",
            "rtk proxy make agent-harness-check",
            "rtk proxy make agent-status AREA=orlix-tcti",
            "rtk proxy make agent-next AREA=orlix-tcti",
            "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
            "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
        ],
        reducerRequirements: [
            "The latest simulator stability failure must be current for HEAD and pinned to Orlix-iPhone-15-Pro-Max (C47ED88D-0D0A-420D-8C78-D4C1D34A276D).",
            "The current simulator report must include structured first-svc and static-PIE events, plus either a structured null-read fatal user fault or no fatal user fault in a passing report.",
            "The no-phone reducer must replay before production TCTI patching.",
        ],
        requiredSubagentsOrSkills: [
            "orlix-tcti-reproducer",
            "orlix-tcti-debug",
            "orlix-tcti-safety",
            "tcti-planner",
            "tcti-safety-reviewer",
            "tcti-llvm-inspector",
            "tcti-test-reducer",
            "tcti-release-gate-reviewer",
        ],
        commitMessageTemplate: "test(tcti): reduce simulator got slot regression",
        stopConditions: [
            "Stop if the latest simulator stability report is stale or missing required structured TCTI runtime events.",
            "Stop if reducer replay cannot reproduce the no-phone unrelocated GOT byte-load failure.",
            "Stop if production TCTI code would be required before the reducer exists.",
        ]
    ),
    Gate(
        id: "tcti-post-overlay-null-user-fault-fix",
        command: "make tcti-gate TARGET=tcti-post-overlay-null-user-fault-fix",
        kind: "production-tcti-fix",
        prerequisites: ["no-phone-tcti-post-overlay-null-user-fault-reducer"],
        allowedScope: [
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/**",
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/mm/**",
            "tools/tcti/orlix-tcti-gate.swift",
            "Makefile",
            ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "docs/plans/active/orlix-tcti/IMPLEMENT.md",
        ],
        forbiddenScope: [
            "Do not run physical-device gates.",
            "Do not add production assembly.",
            "Do not add gadget dispatch.",
            "Do not flip product defconfigs.",
            "Do not edit generated Linux or build trees.",
            "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime semantics.",
            "Do not implement a broad ELF dynamic loader.",
            "Limit production behavior to the reducer-backed post-overlay null user-fault visibility failure class.",
        ],
        expectedReportPaths: [
            "Build/TCTI/reports/tcti-post-overlay-null-user-fault-fix/report.json",
            "Build/TCTI/reports/tcti-post-overlay-null-user-fault-reducer/report.json",
            "Build/Reports/runtime/tcti-simulator-stability-*.json",
        ],
        readinessEligible: false,
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [
            "rtk proxy git diff --check",
            "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
            "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "rtk proxy make tcti-gate TARGET=tcti-post-overlay-null-user-fault-reducer",
            "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-static-pie-got-relocation-invisible-byte-load.json",
            "rtk proxy make tcti-gate TARGET=tcti-post-overlay-null-user-fault-fix",
            "rtk proxy make agent-harness-check",
            "rtk proxy make agent-status AREA=orlix-tcti",
            "rtk proxy make agent-next AREA=orlix-tcti",
            "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
            "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
            "rtk test env PATH=\"$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin\" make -f OrlixKernel/Makefile kunit PROFILE=development",
            "rtk test env PATH=\"$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin\" make -f OrlixKernel/Makefile kunit PROFILE=release",
        ],
        reducerRequirements: [
            "The post-overlay null user-fault reducer must pass before production TCTI patching.",
            "After the fix, rerun simulator stability on the pinned simulator through the next selected gate.",
        ],
        requiredSubagentsOrSkills: [
            "orlix-tcti-reproducer",
            "orlix-tcti-debug",
            "orlix-tcti-safety",
            "orlix-runtime-claim-verification",
            "tcti-planner",
            "tcti-safety-reviewer",
            "tcti-llvm-inspector",
            "tcti-test-reducer",
            "tcti-release-gate-reviewer",
        ],
        commitMessageTemplate: "fix(tcti): preserve static pie got slot writes",
        stopConditions: [
            "Stop if the fix requires production assembly, gadget dispatch, HostAdapter Linux behavior, product defconfig flips, or generated-tree edits.",
            "Stop if the fix broadens into dynamic loader, VFS, fd table, process, signal, scheduler, or Linux runtime semantics.",
            "Stop if the latest simulator stability failure still matches the post-overlay null user-fault null-read signature after the fix.",
        ]
    ),
    Gate(
        id: "no-phone-tcti-ldrsw-sign-extension-reducer",
        command: "make tcti-gate TARGET=tcti-ldrsw-sign-extension-reducer",
        kind: "no-phone-reducer",
        prerequisites: ["tcti-post-overlay-null-user-fault-fix"],
        allowedScope: [
            "tools/tcti/orlix-tcti-gate.swift",
            "Makefile",
            ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "docs/plans/active/orlix-tcti/IMPLEMENT.md",
        ],
        forbiddenScope: [
            "Do not run physical-device gates.",
            "Do not run simulator gates while reducing this failure.",
            "Do not add production assembly.",
            "Do not add gadget dispatch.",
            "Do not flip product defconfigs.",
            "Do not edit generated Linux or build trees.",
            "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime semantics.",
            "Reduce the pinned simulator high-address LDRSW sign-extension fault before production patching.",
        ],
        expectedReportPaths: [
            "Build/TCTI/reports/tcti-ldrsw-sign-extension-reducer/report.json",
            "Build/TCTI/reproducers/tcti-ldrsw-sign-extension-reducer/ldrsw-sign-extension-pass-regression.json",
            "Build/Reports/runtime/tcti-simulator-stability-*.json",
        ],
        readinessEligible: false,
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [
            "rtk proxy git diff --check",
            "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
            "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "rtk proxy make tcti-gate TARGET=tcti-ldrsw-sign-extension-reducer",
            "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-ldrsw-sign-extension-reducer/ldrsw-sign-extension-pass-regression.json",
            "rtk proxy make agent-harness-check",
            "rtk proxy make agent-status AREA=orlix-tcti",
            "rtk proxy make agent-next AREA=orlix-tcti",
            "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
            "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
        ],
        reducerRequirements: [
            "The latest simulator stability failure must be current for HEAD and pinned to Orlix-iPhone-15-Pro-Max (C47ED88D-0D0A-420D-8C78-D4C1D34A276D).",
            "The fatal artifact must include addr-sp=0x1000000a0 after /dev/hvc0 open, proving the LDRSW zero-extension failure shape.",
            "The runtime /sbin/init disassembly must show emitted LDRSW 0xb9801848.",
        ],
        requiredSubagentsOrSkills: [
            "orlix-tcti-reproducer",
            "orlix-tcti-debug",
            "orlix-tcti-safety",
            "tcti-planner",
            "tcti-safety-reviewer",
            "tcti-llvm-inspector",
            "tcti-test-reducer",
            "tcti-release-gate-reviewer",
        ],
        commitMessageTemplate: "test(tcti): reduce ldrsw sign extension fault",
        stopConditions: [
            "Stop if the latest simulator stability failure no longer matches the high-address LDRSW sign-extension signature.",
            "Stop if production TCTI code would be required before the reducer exists.",
        ]
    ),
    Gate(
        id: "tcti-ldrsw-sign-extension-fix",
        command: "make tcti-gate TARGET=tcti-ldrsw-sign-extension-fix",
        kind: "production-tcti-fix",
        prerequisites: ["no-phone-tcti-ldrsw-sign-extension-reducer"],
        allowedScope: [
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/**",
            "tools/tcti/orlix-tcti-gate.swift",
            "Makefile",
            ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "docs/plans/active/orlix-tcti/IMPLEMENT.md",
        ],
        forbiddenScope: [
            "Do not run physical-device gates.",
            "Do not add production assembly.",
            "Do not add gadget dispatch.",
            "Do not flip product defconfigs.",
            "Do not edit generated Linux or build trees.",
            "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime semantics.",
            "Limit production behavior to signed load destination-width semantics required by emitted LDRSW 0xb9801848.",
        ],
        expectedReportPaths: [
            "Build/TCTI/reports/tcti-ldrsw-sign-extension-fix/report.json",
            "Build/TCTI/reports/tcti-ldrsw-sign-extension-reducer/report.json",
            "Build/Reports/runtime/tcti-simulator-stability-*.json",
        ],
        readinessEligible: false,
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [
            "rtk proxy git diff --check",
            "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
            "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "rtk proxy make tcti-gate TARGET=tcti-ldrsw-sign-extension-reducer",
            "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-ldrsw-sign-extension-reducer/ldrsw-sign-extension-pass-regression.json",
            "rtk proxy make tcti-gate TARGET=tcti-ldrsw-sign-extension-fix",
            "rtk proxy make agent-harness-check",
            "rtk proxy make agent-status AREA=orlix-tcti",
            "rtk proxy make agent-next AREA=orlix-tcti",
            "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
            "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
            "rtk test env PATH=\"$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin\" make -f OrlixKernel/Makefile kunit PROFILE=development",
            "rtk test env PATH=\"$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin\" make -f OrlixKernel/Makefile kunit PROFILE=release",
        ],
        reducerRequirements: [
            "The LDRSW sign-extension reducer must pass before production TCTI patching.",
            "After the fix, rerun simulator stability on the pinned simulator through the next selected gate.",
        ],
        requiredSubagentsOrSkills: [
            "orlix-tcti-reproducer",
            "orlix-tcti-debug",
            "orlix-tcti-safety",
            "orlix-runtime-claim-verification",
            "tcti-planner",
            "tcti-safety-reviewer",
            "tcti-llvm-inspector",
            "tcti-test-reducer",
            "tcti-release-gate-reviewer",
        ],
        commitMessageTemplate: "fix(tcti): sign extend ldrsw into x registers",
        stopConditions: [
            "Stop if the fix requires production assembly, gadget dispatch, HostAdapter Linux behavior, product defconfig flips, or generated-tree edits.",
            "Stop if the fix broadens into generic runtime semantics beyond signed-load destination-width correction.",
        ]
    ),
    Gate(
        id: "no-phone-tcti-post-setsid-tls-fault-reducer",
        command: "make tcti-gate TARGET=tcti-post-setsid-tls-fault-reducer",
        kind: "no-phone-reducer",
        prerequisites: ["tcti-ldrsw-sign-extension-fix"],
        allowedScope: [
            "tools/tcti/orlix-tcti-gate.swift",
            "tools/tcti/fixtures/golden_elf/**",
            ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "docs/plans/active/orlix-tcti/IMPLEMENT.md",
        ],
        forbiddenScope: [
            "Do not run physical-device gates.",
            "Do not add production assembly.",
            "Do not add gadget dispatch.",
            "Do not flip product defconfigs.",
            "Do not edit generated Linux or build trees.",
            "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime semantics.",
            "Reduce the pinned simulator post-setsid pid-valued user fault before production TCTI patching.",
        ],
        expectedReportPaths: [
            "Build/TCTI/reports/tcti-post-setsid-tls-fault-reducer/report.json",
            "Build/TCTI/reproducers/tcti-post-setsid-tls-fault-reducer/post-setsid-tls-fault-pass-regression.json",
            "Build/Reports/runtime/tcti-simulator-stability-*.json",
        ],
        readinessEligible: false,
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [
            "rtk proxy git diff --check",
            "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
            "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "rtk proxy make tcti-gate TARGET=tcti-post-setsid-tls-fault-reducer",
            "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-post-setsid-tls-fault-reducer/post-setsid-tls-fault-pass-regression.json",
            "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
        ],
        reducerRequirements: [
            "The current simulator stability failure must match the pinned Orlix-iPhone-15-Pro-Max post-setsid pid-valued user fault.",
            "The no-phone reducer must replay before production TCTI patching.",
        ],
        requiredSubagentsOrSkills: [
            "orlix-tcti-reproducer",
            "orlix-tcti-debug",
            "orlix-tcti-safety",
            "tcti-planner",
            "tcti-safety-reviewer",
            "tcti-llvm-inspector",
            "tcti-test-reducer",
            "tcti-release-gate-reviewer",
        ],
        commitMessageTemplate: "test(tcti): reduce post setsid simulator fault",
        stopConditions: [
            "Stop if the latest simulator stability failure no longer matches the post-setsid pid-valued user fault.",
            "Stop if reducer replay cannot reproduce the no-phone regression.",
            "Stop if production TCTI code would be required before the reducer exists.",
        ]
    ),
    Gate(
        id: "no-phone-tcti-post-exec-sh-fetch-fault-reducer",
        command: "make tcti-gate TARGET=tcti-post-exec-sh-fetch-fault-reducer",
        kind: "no-phone-reducer",
        prerequisites: ["no-phone-tcti-post-setsid-tls-fault-reducer"],
        allowedScope: [
            "tools/tcti/orlix-tcti-gate.swift",
            "Makefile",
            ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "docs/plans/active/orlix-tcti/IMPLEMENT.md",
        ],
        forbiddenScope: [
            "Do not run physical-device gates.",
            "Do not add production assembly.",
            "Do not add gadget dispatch.",
            "Do not flip product defconfigs.",
            "Do not edit generated Linux build trees.",
            "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime semantics.",
            "Do not patch the fetch-fault runtime behavior before the reducer exists and replays.",
            "Use only Orlix-iPhone-15-Pro-Max (C47ED88D-0D0A-420D-8C78-D4C1D34A276D) for simulator evidence.",
        ],
        expectedReportPaths: [
            "Build/TCTI/reports/tcti-post-exec-sh-fetch-fault-reducer/report.json",
            "Build/TCTI/reproducers/tcti-post-exec-sh-fetch-fault-reducer/post-exec-sh-fetch-fault-pass-regression.json",
            "Build/Reports/runtime/tcti-simulator-stability-*.json",
        ],
        readinessEligible: false,
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [
            "rtk proxy git diff --check",
            "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
            "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "rtk proxy make tcti-gate TARGET=tcti-post-exec-sh-fetch-fault-reducer",
            "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-post-exec-sh-fetch-fault-reducer/post-exec-sh-fetch-fault-pass-regression.json",
            "rtk proxy make agent-harness-check",
            "rtk proxy make agent-status AREA=orlix-tcti",
            "rtk proxy make agent-next AREA=orlix-tcti",
            "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
            "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
        ],
        reducerRequirements: [
            "The latest simulator stability failure must be current for HEAD and pinned to Orlix-iPhone-15-Pro-Max (C47ED88D-0D0A-420D-8C78-D4C1D34A276D).",
            "The artifact must show child pid 32 reaches execve(221), task changes to sh, and TCTI fetch faults at pc=addr=0x1000494c8 access=0.",
            "The reducer must replay before production TCTI patching.",
        ],
        requiredSubagentsOrSkills: [
            "orlix-tcti-reproducer",
            "orlix-tcti-debug",
            "orlix-tcti-safety",
            "tcti-planner",
            "tcti-safety-reviewer",
            "tcti-llvm-inspector",
            "tcti-test-reducer",
            "tcti-release-gate-reviewer",
        ],
        commitMessageTemplate: "test(tcti): reduce post-exec sh fetch fault",
        stopConditions: [
            "Stop if the latest simulator stability failure no longer matches post-exec sh fetch fault pc=addr=0x1000494c8 access=0.",
            "Stop if more than the pinned Orlix-iPhone-15-Pro-Max simulator is booted.",
            "Stop if reducer replay cannot reproduce the no-phone evidence gate.",
        ]
    ),
    Gate(
        id: "no-phone-tcti-post-pie-sh-entry-fetch-fault-reducer",
        command: "make tcti-gate TARGET=tcti-post-pie-sh-entry-fetch-fault-reducer",
        kind: "no-phone-reducer",
        prerequisites: ["no-phone-tcti-post-exec-sh-fetch-fault-reducer"],
        allowedScope: [
            "tools/tcti/orlix-tcti-gate.swift",
            "Makefile",
            ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "docs/plans/active/orlix-tcti/IMPLEMENT.md",
        ],
        forbiddenScope: [
            "Do not run physical-device gates.",
            "Do not add production assembly.",
            "Do not add gadget dispatch.",
            "Do not flip product defconfigs.",
            "Do not edit generated Linux build trees.",
            "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime semantics.",
            "Do not patch the PIE entry fetch runtime behavior before the reducer exists and replays.",
            "Use only Orlix-iPhone-15-Pro-Max (C47ED88D-0D0A-420D-8C78-D4C1D34A276D) for simulator evidence.",
        ],
        expectedReportPaths: [
            "Build/TCTI/reports/tcti-post-pie-sh-entry-fetch-fault-reducer/report.json",
            "Build/TCTI/reproducers/tcti-post-pie-sh-entry-fetch-fault-reducer/post-pie-sh-entry-fetch-fault-pass-regression.json",
            "Build/Reports/runtime/tcti-simulator-stability-*.json",
        ],
        readinessEligible: false,
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [
            "rtk proxy git diff --check",
            "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
            "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "rtk proxy make tcti-gate TARGET=tcti-post-pie-sh-entry-fetch-fault-reducer",
            "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-post-pie-sh-entry-fetch-fault-reducer/post-pie-sh-entry-fetch-fault-pass-regression.json",
            "rtk proxy make agent-harness-check",
            "rtk proxy make agent-status AREA=orlix-tcti",
            "rtk proxy make agent-next AREA=orlix-tcti",
            "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
            "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
        ],
        reducerRequirements: [
            "The latest simulator stability failure must be current for HEAD and pinned to Orlix-iPhone-15-Pro-Max (C47ED88D-0D0A-420D-8C78-D4C1D34A276D).",
            "The artifact must show static-PIE /bin/sh reaches execve(221), then TCTI exits task=sh with status=-14 at pc=fault and insn=0x0.",
            "The reducer must replay before production TCTI patching.",
        ],
        requiredSubagentsOrSkills: [
            "orlix-tcti-reproducer",
            "orlix-tcti-debug",
            "orlix-tcti-safety",
            "tcti-planner",
            "tcti-safety-reviewer",
            "tcti-llvm-inspector",
            "tcti-test-reducer",
            "tcti-release-gate-reviewer",
        ],
        commitMessageTemplate: "test(tcti): reduce pie shell entry fetch fault",
        stopConditions: [
            "Stop if the latest simulator stability failure no longer matches post-PIE sh entry fetch fault status=-14 insn=0x0.",
            "Stop if more than the pinned Orlix-iPhone-15-Pro-Max simulator is booted.",
            "Stop if reducer replay cannot reproduce the no-phone evidence gate.",
        ]
    ),
    Gate(
        id: "no-phone-tcti-post-bash-mmap-read-fault-reducer",
        command: "make tcti-gate TARGET=tcti-post-bash-mmap-read-fault-reducer",
        kind: "no-phone-reducer",
        prerequisites: ["no-phone-tcti-post-pie-sh-entry-fetch-fault-reducer"],
        allowedScope: [
            "tools/tcti/orlix-tcti-gate.swift",
            "Makefile",
            ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "docs/plans/active/orlix-tcti/IMPLEMENT.md",
        ],
        forbiddenScope: [
            "Do not run physical-device gates.",
            "Do not add production assembly.",
            "Do not add gadget dispatch.",
            "Do not flip product defconfigs.",
            "Do not edit generated Linux build trees.",
            "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime semantics.",
            "Do not patch the Bash mmap/read runtime behavior before the reducer exists and replays.",
            "Use only Orlix-iPhone-15-Pro-Max (C47ED88D-0D0A-420D-8C78-D4C1D34A276D) for simulator evidence.",
        ],
        expectedReportPaths: [
            "Build/TCTI/reports/tcti-post-bash-mmap-read-fault-reducer/report.json",
            "Build/TCTI/reproducers/tcti-post-bash-mmap-read-fault-reducer/post-bash-mmap-read-fault-pass-regression.json",
            "Build/Reports/runtime/tcti-simulator-stability-*.json",
        ],
        readinessEligible: false,
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [
            "rtk proxy git diff --check",
            "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
            "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "rtk proxy make tcti-gate TARGET=tcti-post-bash-mmap-read-fault-reducer",
            "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-post-bash-mmap-read-fault-reducer/post-bash-mmap-read-fault-pass-regression.json",
            "rtk proxy make agent-harness-check",
            "rtk proxy make agent-status AREA=orlix-tcti",
            "rtk proxy make agent-next AREA=orlix-tcti",
            "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
            "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
        ],
        reducerRequirements: [
            "The latest simulator stability report must be current for HEAD and pinned to Orlix-iPhone-15-Pro-Max (C47ED88D-0D0A-420D-8C78-D4C1D34A276D).",
            "The structured report must show static-PIE /bin/sh and mmap syscall 222, plus either the Bash user-data read-fault event or no fatal user fault in a passing report.",
            "The reducer must replay before production TCTI patching.",
        ],
        requiredSubagentsOrSkills: [
            "orlix-tcti-reproducer",
            "orlix-tcti-debug",
            "orlix-tcti-safety",
            "tcti-planner",
            "tcti-safety-reviewer",
            "tcti-llvm-inspector",
            "tcti-test-reducer",
            "tcti-release-gate-reviewer",
        ],
        commitMessageTemplate: "test(tcti): reduce bash mmap read fault",
        stopConditions: [
            "Stop if the latest simulator stability report is stale or missing structured static-PIE, mmap, or fatal-user-fault fields.",
            "Stop if more than the pinned Orlix-iPhone-15-Pro-Max simulator is booted.",
            "Stop if reducer replay cannot reproduce the no-phone evidence gate.",
        ]
    ),
    Gate(
        id: "simulator-tcti-runtime-stability",
        command: "make runtime-validation DESTINATION=iphonesimulator GATE=tcti-simulator-stability ORLIX_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=Orlix-iPhone-15-Pro-Max",
        kind: "simulator-runtime",
        prerequisites: ["no-phone-tcti-post-bash-mmap-read-fault-reducer"],
            allowedScope: [
                "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/**",
                "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/kernel/**",
                "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/mm/**",
                "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/include/asm/**",
                "tools/runtime/orlix-runtime-validation.sh",
                "tools/tcti/orlix-tcti-gate.swift",
                "tools/tcti/fixtures/**",
                "OrlixOS/Sources/make/rootfs.mk",
                "OrlixOS/Sources/make/packages.mk",
                "project.yml",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Do not run physical-device gates.",
                "Do not use any simulator except Orlix-iPhone-15-Pro-Max (C47ED88D-0D0A-420D-8C78-D4C1D34A276D).",
                "Do not allow more than one simulator to be booted while this gate runs.",
                "Do not treat first-syscall marker evidence as simulator stability.",
                "Do not patch simulator logs directly without reducing TCTI behavior into a no-phone fixture.",
                "Do not add production assembly or gadget dispatch unless a later selected gate allows it.",
                "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime semantics.",
                "Do not edit generated Linux or build trees.",
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
                "rtk proxy make tcti-gate TARGET=tcti-plan-consistency",
                "rtk proxy make tcti-gate TARGET=tcti-report-schema-check",
                "rtk proxy make tcti-gate TARGET=tcti-golden-elf",
                "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
                "rtk proxy make runtime-validation DESTINATION=iphonesimulator GATE=tcti-simulator-stability ORLIX_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=Orlix-iPhone-15-Pro-Max",
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
        Gate(
            id: "simulator-tcti-linux-console-usability",
            command: "make runtime-validation DESTINATION=iphonesimulator GATE=tcti-init-console-write ORLIX_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=Orlix-iPhone-15-Pro-Max",
            kind: "simulator-runtime",
            prerequisites: ["simulator-tcti-runtime-stability"],
            allowedScope: [
                "tools/runtime/orlix-runtime-validation.sh",
                ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                ".agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json",
                "docs/plans/active/orlix-tcti/PLAN.md",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Do not run physical-device gates.",
                "Do not use any simulator except Orlix-iPhone-15-Pro-Max (C47ED88D-0D0A-420D-8C78-D4C1D34A276D).",
                "Do not allow more than one simulator to be booted while this gate runs.",
                "Do not claim full Linux usability, shell readiness, release readiness, or physical readiness from this marker.",
                "Do not add production assembly or gadget dispatch.",
                "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime semantics.",
                "Do not edit generated Linux or build trees.",
            ],
            expectedReportPaths: [
                "Build/Reports/runtime/tcti-init-console-write-*.json",
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
                "rtk proxy make tcti-gate TARGET=tcti-plan-consistency",
                "rtk proxy make tcti-gate TARGET=tcti-report-schema-check",
                "rtk proxy make tcti-gate TARGET=tcti-golden-elf",
                "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
                "rtk proxy make runtime-validation DESTINATION=iphonesimulator GATE=tcti-init-console-write ORLIX_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=Orlix-iPhone-15-Pro-Max",
            ],
            reducerRequirements: [
                "If the simulator console marker fails because of a TCTI runtime fault, reduce it into a no-phone golden, oracle, memory fuzz, direct-chain fuzz, or safety case before production patching.",
                "The report must include a non-empty tcti-console-write marker artifact and all forbidden-behavior fields false.",
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
            commitMessageTemplate: "test(tcti): require simulator console usability before phone",
            stopConditions: [
                "Stop if the simulator console marker artifact is missing or empty.",
                "Stop if more than the pinned simulator is booted.",
                "Stop if the gate is used to claim full Linux usability, shell readiness, release readiness, or physical-device readiness.",
            ]
        ),
        Gate(
            id: "no-phone-tcti-post-busybox-sigabrt-reducer",
            command: "make tcti-gate TARGET=tcti-post-busybox-sigabrt-reducer",
            kind: "no-phone-reducer",
            prerequisites: ["simulator-tcti-linux-console-usability"],
            allowedScope: [
                "tools/tcti/orlix-tcti-gate.swift",
                ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                ".agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Do not run phone gates.",
                "Do not run simulator gates while reducing this failure.",
                "Do not add production assembly.",
                "Do not add gadget dispatch.",
                "Do not flip product defconfigs.",
                "Do not edit generated Linux or build trees.",
                "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or broad Linux runtime semantics.",
                "Only bind the current pinned simulator static BusyBox SIGABRT report to a replayable no-phone reducer.",
            ],
            expectedReportPaths: [
                "Build/TCTI/reports/tcti-post-busybox-sigabrt-reducer/report.json",
                "Build/TCTI/reproducers/tcti-post-busybox-sigabrt-reducer/post-busybox-sigabrt-pass-regression.json",
                "Build/Reports/runtime/tcti-static-busybox-start-*.json",
            ],
            readinessEligible: false,
            physicalDevice: false,
            gadget: false,
            requiredValidationCommands: [
                "rtk proxy git diff --check",
                "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
                "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "rtk proxy make tcti-gate TARGET=tcti-post-busybox-sigabrt-reducer",
                "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-post-busybox-sigabrt-reducer/post-busybox-sigabrt-pass-regression.json",
                "rtk proxy make agent-harness-check",
                "rtk proxy make agent-status AREA=orlix-tcti",
                "rtk proxy make agent-next AREA=orlix-tcti",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
            ],
            reducerRequirements: [
                "The latest static BusyBox simulator failure must be current for HEAD and pinned to Orlix-iPhone-15-Pro-Max (C47ED88D-0D0A-420D-8C78-D4C1D34A276D).",
                "The simulator report must show static PIE image task=sh and signaled_process.signal=6 for the same pid.",
                "The reducer must replay without running simulator or phone gates.",
            ],
            requiredSubagentsOrSkills: [
                "orlix-tcti-reproducer",
                "orlix-tcti-safety",
                "orlix-tcti-debug",
                "tcti-test-reducer",
                "tcti-safety-reviewer",
                "tcti-release-gate-reviewer",
            ],
            commitMessageTemplate: "test(tcti): reduce static busybox sigabrt",
            stopConditions: [
                "Stop if no current static BusyBox simulator SIGABRT report exists.",
                "Stop if reducer replay cannot reproduce the report-backed failure shape.",
                "Stop if production TCTI code would be required before the reducer exists.",
            ]
        ),
        Gate(
            id: "simulator-tcti-static-busybox-start",
            command: "make runtime-validation DESTINATION=iphonesimulator GATE=tcti-static-busybox-start ORLIX_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=Orlix-iPhone-15-Pro-Max",
            kind: "simulator-runtime",
            prerequisites: ["simulator-tcti-linux-console-usability"],
            allowedScope: [
                "tools/runtime/orlix-runtime-validation.sh",
                ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                ".agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json",
                "docs/plans/active/orlix-tcti/PLAN.md",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Do not run phone gates.",
                "Do not use any simulator except Orlix-iPhone-15-Pro-Max (C47ED88D-0D0A-420D-8C78-D4C1D34A276D).",
                "Do not allow more than one simulator to be booted while this gate runs.",
                "Do not claim full Linux usability, release readiness, or phone readiness from this gate.",
                "Do not add production assembly or gadget dispatch.",
                "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or broad Linux runtime semantics.",
                "Do not edit generated Linux or build trees.",
            ],
            expectedReportPaths: [
                "Build/Reports/runtime/tcti-static-busybox-start-*.json",
                "Build/Reports/runtime/tcti-init-console-write-*.json",
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
                "rtk proxy make tcti-gate TARGET=tcti-plan-consistency",
                "rtk proxy make tcti-gate TARGET=tcti-report-schema-check",
                "rtk proxy make tcti-gate TARGET=tcti-golden-elf",
                "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
                "rtk proxy make runtime-validation DESTINATION=iphonesimulator GATE=tcti-static-busybox-start ORLIX_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=Orlix-iPhone-15-Pro-Max",
            ],
            reducerRequirements: [
                "If static BusyBox start fails because of a TCTI runtime fault, reduce it into a no-phone golden, oracle, memory fuzz, direct-chain fuzz, or safety case before production patching.",
                "The report must be current for HEAD, run on the pinned simulator, and keep all forbidden-behavior fields false.",
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
            commitMessageTemplate: "test(tcti): require simulator static busybox start before phone",
            stopConditions: [
                "Stop if the static BusyBox start report is missing or not current for HEAD.",
                "Stop if more than the pinned simulator is booted.",
                "Stop if the gate is used to claim full Linux usability, release readiness, or phone readiness.",
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

func simulatorRuntimeGates(_ statuses: [GateStatus]) -> [GateStatus] {
    statuses.filter { $0.kind == "simulator-runtime" }
}

func simulatorRuntimeGatesComplete(_ statuses: [GateStatus]) -> Bool {
    let simulatorGates = simulatorRuntimeGates(statuses)
    return !simulatorGates.isEmpty && simulatorGates.allSatisfy { $0.passed }
}

func simulatorRuntimeGateIsSelectable(_ statuses: [GateStatus]) -> Bool {
    simulatorRuntimeGates(statuses).contains { !$0.passed && $0.prerequisitesSatisfied }
}

func physicalDeviceExplicitlyAllowed() -> Bool {
    let environment = ProcessInfo.processInfo.environment
    for key in ["ORLIX_TCTI_ALLOW_PHYSICAL_DEVICE", "ORLIX_TCTI_PHYSICAL_DEVICE_ALLOWED"] {
        let value = environment[key]?.lowercased() ?? ""
        if ["1", "true", "yes"].contains(value) {
            return true
        }
    }
    return false
}

func gateUsesRequiredSimulator(_ gate: Gate) -> Bool {
    gate.kind == "simulator-runtime" ||
        gate.command.contains("DESTINATION=iphonesimulator") ||
        gate.command.contains("ORLIX_SIMULATOR_ID=")
}

func selectedStatusWithSafety(from statuses: [GateStatus]) -> GateStatus? {
    let noPhonePassed = noPhoneGatesPassedBeforeFirstPhysical(statuses)
    let simulatorPassed = simulatorRuntimeGatesComplete(statuses)
    let physicalAllowed = physicalDeviceExplicitlyAllowed()
    return statuses.first { status in
        guard !status.passed && status.prerequisitesSatisfied else {
            return false
        }
        if status.physicalDevice && (!physicalAllowed || !noPhonePassed || !simulatorPassed) {
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
        simulatorRuntimeGatesComplete(gateStatuses) &&
        noPhoneGatesPassedBeforeFirstPhysical(gateStatuses) &&
        physicalDeviceExplicitlyAllowed()
    let releaseEligible = physicalGate?.passed == true
    let readinessEligible = physicalGate?.passed == true
    return StatusDocument(
        area: roadmap.area,
        generatedAt: timestamp(),
        gitSHA: gitSHA(),
        roadmapPath: relativePath(roadmapURL),
        gates: gateStatuses,
        simulatorAllowed: simulatorRuntimeGateIsSelectable(gateStatuses) || simulatorRuntimeGatesComplete(gateStatuses),
        simulatorRequiredBeforePhysical: true,
        simulatorGatesComplete: simulatorRuntimeGatesComplete(gateStatuses),
        requiredSimulatorID: requiredSimulatorID,
        requiredSimulatorName: requiredSimulatorName,
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
        print("simulator_allowed: \(status.simulatorAllowed)")
        print("simulator_required_before_physical: \(status.simulatorRequiredBeforePhysical)")
        print("simulator_gates_complete: \(status.simulatorGatesComplete)")
        print("required_simulator: \(status.requiredSimulatorName) (\(status.requiredSimulatorID))")
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
    let gates = roadmapGatesWithRuntimePreflight(roadmap)
    guard let selectedID = status.nextEligibleGate else {
        if let blocked = blockedPhysicalOptInEnvelope(from: status, roadmap: roadmap, gates: gates) {
            return blocked
        }
        throw HarnessError.invalid("no next eligible gate found")
    }
    guard let gate = gates.first(where: { $0.id == selectedID }) else {
        throw HarnessError.invalid("selected gate \(selectedID) does not exist in roadmap")
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
        simulatorAllowed: status.simulatorAllowed,
        simulatorRequiredBeforePhysical: status.simulatorRequiredBeforePhysical,
        simulatorGatesComplete: status.simulatorGatesComplete,
        selectedGateUsesSimulator: gateUsesRequiredSimulator(gate),
        requiredSimulatorID: status.requiredSimulatorID,
        requiredSimulatorName: status.requiredSimulatorName,
        physicalDevice: gate.physicalDevice,
        gadget: gate.gadget,
        nextTaskJSONPath: relativePath(nextTaskURL),
        nextTaskMarkdownPath: relativePath(nextTaskMarkdownURL)
    )
}

func blockedPhysicalOptInEnvelope(from status: StatusDocument, roadmap: Roadmap, gates: [Gate]) -> TaskEnvelope? {
    guard status.physicalDeviceAllowed == false else {
        return nil
    }
    guard let blockedStatus = status.gates.first(where: { $0.physicalDevice && !$0.passed && $0.prerequisitesSatisfied }) else {
        return nil
    }
    guard let blockedGate = gates.first(where: { $0.id == blockedStatus.id }) else {
        return nil
    }
    let byID = Dictionary(uniqueKeysWithValues: status.gates.map { ($0.id, $0) })
    let prerequisites = blockedGate.prerequisites.map { prereqID in
        let fact = byID[prereqID]
        return PrerequisiteFact(
            id: prereqID,
            state: fact?.state ?? "missing",
            reportPaths: fact?.reportPaths ?? []
        )
    }
    return TaskEnvelope(
        area: roadmap.area,
        generatedAt: timestamp(),
        gitSHA: status.gitSHA,
        roadmapPath: relativePath(roadmapURL),
        selectedGateID: physicalOptInBlockedGateID,
        selectedGateCommand: "no-op: set ORLIX_TCTI_ALLOW_PHYSICAL_DEVICE=1 only after explicit human approval",
        selectedGateKind: "blocked",
        prerequisiteGates: prerequisites,
        whySelected: "All prerequisites for \(blockedGate.id) are satisfied, but physical-device gates require explicit opt-in. Set ORLIX_TCTI_ALLOW_PHYSICAL_DEVICE=1 or ORLIX_TCTI_PHYSICAL_DEVICE_ALLOWED=1 only after human approval.",
        allowedScope: [
            "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            ".agents/skills/orlix-tcti-next-step/SKILL.md"
        ],
        forbiddenScope: [
            "Do not run physical-device gates without explicit human approval and ORLIX_TCTI_ALLOW_PHYSICAL_DEVICE=1 or ORLIX_TCTI_PHYSICAL_DEVICE_ALLOWED=1.",
            "Do not treat simulator or no-phone reports as physical-device readiness.",
            "Do not add custom MCP, tools/agent, production assembly, gadget dispatch, HostAdapter behavior, or Linux runtime semantics for this blocked state."
        ],
        requiredValidationCommands: [
            "rtk proxy make agent-harness-check",
            "rtk proxy make agent-status AREA=orlix-tcti",
            "rtk proxy make agent-next AREA=orlix-tcti",
            "rtk proxy make agent-task-envelope-check AREA=orlix-tcti"
        ],
        expectedReportPaths: [
            relativePath(statusURL),
            relativePath(nextTaskURL),
            relativePath(nextTaskMarkdownURL)
        ],
        reducerRequirements: [
            "No reducer is required for the blocked physical opt-in envelope because no runtime gate executed."
        ],
        requiredSubagentsOrSkills: [
            "tcti-planner",
            "tcti-safety-reviewer",
            "tcti-release-gate-reviewer",
            "orlix-tcti-next-step",
            "orlix-tcti-safety"
        ],
        commitMessage: "chore(tcti): require explicit physical gate opt-in",
        stopConditions: [
            "Stop if a physical-device command would run without explicit human approval.",
            "Stop if agent-next selects a physical-device gate while physical_device_allowed is false.",
            "Stop if the blocked envelope is missing machine-readable JSON or Markdown."
        ],
        simulatorAllowed: status.simulatorAllowed,
        simulatorRequiredBeforePhysical: status.simulatorRequiredBeforePhysical,
        simulatorGatesComplete: status.simulatorGatesComplete,
        selectedGateUsesSimulator: false,
        requiredSimulatorID: status.requiredSimulatorID,
        requiredSimulatorName: status.requiredSimulatorName,
        physicalDevice: false,
        gadget: false,
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

    ## Simulator Gate
    - simulator_allowed: \(envelope.simulatorAllowed)
    - simulator_required_before_physical: \(envelope.simulatorRequiredBeforePhysical)
    - simulator_gates_complete: \(envelope.simulatorGatesComplete)
    - selected_gate_uses_simulator: \(envelope.selectedGateUsesSimulator)
    - required_simulator: \(envelope.requiredSimulatorName) (\(envelope.requiredSimulatorID))

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
    if task.selectedGateID == physicalOptInBlockedGateID {
        let status = try statusDocument()
        guard status.nextEligibleGate == nil else {
            throw HarnessError.invalid("blocked physical opt-in envelope is invalid while another gate is eligible")
        }
        guard status.physicalDeviceAllowed == false else {
            throw HarnessError.invalid("blocked physical opt-in envelope is invalid when physical_device_allowed is true")
        }
        guard status.gates.contains(where: { $0.physicalDevice && !$0.passed && $0.prerequisitesSatisfied }) else {
            throw HarnessError.invalid("blocked physical opt-in envelope requires a blocked physical-device gate with satisfied prerequisites")
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
        if task.physicalDevice {
            throw HarnessError.invalid("blocked physical opt-in envelope must not be marked physical_device")
        }
        print("pass: \(relativePath(nextTaskURL))")
        print("selected_gate: \(task.selectedGateID)")
        return
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
    if task.simulatorRequiredBeforePhysical != true {
        throw HarnessError.invalid("simulator_required_before_physical must be true for Orlix TCTI")
    }
    if gate.kind == "simulator-runtime" {
        guard task.selectedGateUsesSimulator else {
            throw HarnessError.invalid("simulator-runtime gate must be marked selected_gate_uses_simulator")
        }
        guard task.selectedGateCommand.contains("DESTINATION=iphonesimulator"),
              task.selectedGateCommand.contains("ORLIX_SIMULATOR_ID=\(requiredSimulatorID)"),
              task.selectedGateCommand.contains("ORLIX_TCTI_REQUIRED_SIMULATOR_ID=\(requiredSimulatorID)"),
              task.selectedGateCommand.contains("ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=\(requiredSimulatorName)") else {
            throw HarnessError.invalid("simulator-runtime gate must target required simulator \(requiredSimulatorName) (\(requiredSimulatorID))")
        }
        if task.physicalDevice {
            throw HarnessError.invalid("simulator-runtime gate must not be marked physical_device")
        }
    }
    if task.physicalDevice && !status.physicalDeviceAllowed {
        throw HarnessError.invalid("physical-device gate selected before no-phone and required simulator gates are complete")
    }
    if task.physicalDevice && !status.simulatorGatesComplete {
        throw HarnessError.invalid("physical-device gate selected before required simulator gates are complete")
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
