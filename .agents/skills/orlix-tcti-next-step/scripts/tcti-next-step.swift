#!/usr/bin/env swift
import Foundation

struct Roadmap: Decodable {
    let area: String
    let description: String
    let gates: [Gate]
}

struct EnvironmentPolicy: Decodable {
    let requiredSimulatorID: String
    let requiredSimulatorName: String

    enum CodingKeys: String, CodingKey {
        case requiredSimulatorID = "required_simulator_id"
        case requiredSimulatorName = "required_simulator_name"
    }
}

struct Gate: Codable {
    let id: String
    let command: String
    let kind: String
    let proofTier: String
    let acceptanceWeight: String
    let realStackRequired: Bool
    let canClaimRuntimeReadiness: Bool
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
        case proofTier = "proof_tier"
        case acceptanceWeight = "acceptance_weight"
        case realStackRequired = "real_stack_required"
        case canClaimRuntimeReadiness = "can_claim_runtime_readiness"
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

    init(
        id: String,
        command: String,
        kind: String,
        proofTier: String? = nil,
        acceptanceWeight: String? = nil,
        realStackRequired: Bool? = nil,
        canClaimRuntimeReadiness: Bool? = nil,
        prerequisites: [String],
        allowedScope: [String],
        forbiddenScope: [String],
        expectedReportPaths: [String],
        readinessEligible: Bool,
        physicalDevice: Bool,
        gadget: Bool,
        requiredValidationCommands: [String],
        reducerRequirements: [String],
        requiredSubagentsOrSkills: [String],
        commitMessageTemplate: String,
        stopConditions: [String]
    ) {
        self.id = id
        self.command = command
        self.kind = kind
        self.proofTier = proofTier ?? Gate.defaultProofTier(id: id, kind: kind, physicalDevice: physicalDevice)
        self.acceptanceWeight = acceptanceWeight ?? Gate.defaultAcceptanceWeight(id: id, kind: kind, readinessEligible: readinessEligible, physicalDevice: physicalDevice)
        self.realStackRequired = realStackRequired ?? Gate.defaultRealStackRequired(id: id, kind: kind, proofTier: self.proofTier, physicalDevice: physicalDevice)
        self.canClaimRuntimeReadiness = canClaimRuntimeReadiness ?? (readinessEligible && self.realStackRequired && self.proofTier != "seed")
        self.prerequisites = prerequisites
        self.allowedScope = allowedScope
        self.forbiddenScope = forbiddenScope
        self.expectedReportPaths = expectedReportPaths
        self.readinessEligible = readinessEligible
        self.physicalDevice = physicalDevice
        self.gadget = gadget
        self.requiredValidationCommands = requiredValidationCommands
        self.reducerRequirements = reducerRequirements
        self.requiredSubagentsOrSkills = requiredSubagentsOrSkills
        self.commitMessageTemplate = commitMessageTemplate
        self.stopConditions = stopConditions
    }

    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        let id = try container.decode(String.self, forKey: .id)
        let command = try container.decode(String.self, forKey: .command)
        let kind = try container.decode(String.self, forKey: .kind)
        let readinessEligible = try container.decode(Bool.self, forKey: .readinessEligible)
        let physicalDevice = try container.decode(Bool.self, forKey: .physicalDevice)
        self.init(
            id: id,
            command: command,
            kind: kind,
            proofTier: try container.decode(String.self, forKey: .proofTier),
            acceptanceWeight: try container.decode(String.self, forKey: .acceptanceWeight),
            realStackRequired: try container.decode(Bool.self, forKey: .realStackRequired),
            canClaimRuntimeReadiness: try container.decode(Bool.self, forKey: .canClaimRuntimeReadiness),
            prerequisites: try container.decode([String].self, forKey: .prerequisites),
            allowedScope: try container.decode([String].self, forKey: .allowedScope),
            forbiddenScope: try container.decode([String].self, forKey: .forbiddenScope),
            expectedReportPaths: try container.decode([String].self, forKey: .expectedReportPaths),
            readinessEligible: readinessEligible,
            physicalDevice: physicalDevice,
            gadget: try container.decode(Bool.self, forKey: .gadget),
            requiredValidationCommands: try container.decode([String].self, forKey: .requiredValidationCommands),
            reducerRequirements: try container.decode([String].self, forKey: .reducerRequirements),
            requiredSubagentsOrSkills: try container.decode([String].self, forKey: .requiredSubagentsOrSkills),
            commitMessageTemplate: try container.decode(String.self, forKey: .commitMessageTemplate),
            stopConditions: try container.decode([String].self, forKey: .stopConditions)
        )
    }

    static func defaultProofTier(id: String, kind: String, physicalDevice: Bool) -> String {
        if physicalDevice { return "device" }
        if id.hasPrefix("simulator-tcti-") { return "simulator" }
        if kind == "simulator-runtime" { return "simulator" }
        if kind == "safety" { return "safety" }
        if kind == "rail" { return "rail" }
        if kind.contains("production") { return "rail" }
        return "seed"
    }

    static func defaultAcceptanceWeight(id: String, kind: String, readinessEligible: Bool, physicalDevice: Bool) -> String {
        if physicalDevice { return "blocker" }
        if kind == "safety" { return "blocker" }
        if id.hasPrefix("simulator-tcti-") { return "readiness" }
        if readinessEligible { return "readiness" }
        return "probe"
    }

    static func defaultRealStackRequired(id: String, kind: String, proofTier: String, physicalDevice: Bool) -> Bool {
        if physicalDevice { return true }
        if id.hasPrefix("simulator-tcti-") { return true }
        if kind == "simulator-runtime" { return true }
        return !["seed", "rail", "safety"].contains(proofTier)
    }
}

struct ReportFailureFact: Codable {
    let id: String
    let message: String
}

struct ExecutionFreshness: Codable {
    let reportGitSHA: String?
    let currentGitSHA: String
    let executionFresh: Bool
    let statusRecomputed: Bool
    let changedPathsSinceReport: [String]
    let ignoredNonExecutionPaths: [String]
    let invalidatingPaths: [String]
    let reason: String

    enum CodingKeys: String, CodingKey {
        case reportGitSHA = "report_git_sha"
        case currentGitSHA = "current_git_sha"
        case executionFresh = "execution_fresh"
        case statusRecomputed = "status_recomputed"
        case changedPathsSinceReport = "changed_paths_since_report"
        case ignoredNonExecutionPaths = "ignored_non_execution_paths"
        case invalidatingPaths = "invalidating_paths"
        case reason
    }
}

struct ReportFact: Codable {
    let path: String
    let exists: Bool
    let status: String
    let passed: Bool
    let proofTier: String?
    let acceptanceWeight: String?
    let realStackRequired: Bool?
    let canClaimRuntimeReadiness: Bool?
    let releaseGateEligible: Bool
    let readinessGateEligible: Bool
    let gitSHA: String?
    let executionFreshness: ExecutionFreshness?
    let failures: [ReportFailureFact]
    let forbiddenBehaviorViolations: [String]

    init(
        path: String,
        exists: Bool,
        status: String,
        passed: Bool,
        proofTier: String?,
        acceptanceWeight: String?,
        realStackRequired: Bool?,
        canClaimRuntimeReadiness: Bool?,
        releaseGateEligible: Bool,
        readinessGateEligible: Bool,
        gitSHA: String?,
        executionFreshness: ExecutionFreshness? = nil,
        failures: [ReportFailureFact] = [],
        forbiddenBehaviorViolations: [String] = []
    ) {
        self.path = path
        self.exists = exists
        self.status = status
        self.passed = passed
        self.proofTier = proofTier
        self.acceptanceWeight = acceptanceWeight
        self.realStackRequired = realStackRequired
        self.canClaimRuntimeReadiness = canClaimRuntimeReadiness
        self.releaseGateEligible = releaseGateEligible
        self.readinessGateEligible = readinessGateEligible
        self.gitSHA = gitSHA
        self.executionFreshness = executionFreshness
        self.failures = failures
        self.forbiddenBehaviorViolations = forbiddenBehaviorViolations
    }

    func withExecutionFreshness(_ freshness: ExecutionFreshness?) -> ReportFact {
        ReportFact(
            path: path,
            exists: exists,
            status: status,
            passed: passed,
            proofTier: proofTier,
            acceptanceWeight: acceptanceWeight,
            realStackRequired: realStackRequired,
            canClaimRuntimeReadiness: canClaimRuntimeReadiness,
            releaseGateEligible: releaseGateEligible,
            readinessGateEligible: readinessGateEligible,
            gitSHA: gitSHA,
            executionFreshness: freshness,
            failures: failures,
            forbiddenBehaviorViolations: forbiddenBehaviorViolations
        )
    }

    enum CodingKeys: String, CodingKey {
        case path
        case exists
        case status
        case passed
        case proofTier = "proof_tier"
        case acceptanceWeight = "acceptance_weight"
        case realStackRequired = "real_stack_required"
        case canClaimRuntimeReadiness = "can_claim_runtime_readiness"
        case releaseGateEligible = "release_gate_eligible"
        case readinessGateEligible = "readiness_gate_eligible"
        case gitSHA = "git_sha"
        case executionFreshness = "execution_freshness"
        case failures
        case forbiddenBehaviorViolations = "forbidden_behavior_violations"
    }
}

struct GateResultPolicy: Codable, Equatable {
    let classification: String
    let runtimePatchAllowed: Bool
    let harnessPatchAllowed: Bool
    let continueRefreshAllowed: Bool
    let mustStop: Bool
    let requiredNextAction: String
    let reason: String
    let owningLayer: String

    enum CodingKeys: String, CodingKey {
        case classification = "result_classification"
        case runtimePatchAllowed = "runtime_patch_allowed"
        case harnessPatchAllowed = "harness_patch_allowed"
        case continueRefreshAllowed = "continue_refresh_allowed"
        case mustStop = "must_stop"
        case requiredNextAction = "required_next_action"
        case reason
        case owningLayer = "owning_layer"
    }
}

struct GateStatus: Encodable {
    let id: String
    let command: String
    let kind: String
    let proofTier: String
    let acceptanceWeight: String
    let realStackRequired: Bool
    let canClaimRuntimeReadiness: Bool
    let state: String
    let passed: Bool
    let satisfiesPrerequisite: Bool
    let reason: String
    let prerequisites: [String]
    let prerequisitesSatisfied: Bool
    let reportPaths: [String]
    let reports: [ReportFact]
    let readinessEligible: Bool
    let physicalDevice: Bool
    let gadget: Bool

    init(
        id: String,
        command: String,
        kind: String,
        proofTier: String? = nil,
        acceptanceWeight: String? = nil,
        realStackRequired: Bool? = nil,
        canClaimRuntimeReadiness: Bool? = nil,
        state: String,
        passed: Bool,
        satisfiesPrerequisite: Bool? = nil,
        reason: String,
        prerequisites: [String],
        prerequisitesSatisfied: Bool,
        reportPaths: [String],
        reports: [ReportFact],
        readinessEligible: Bool,
        physicalDevice: Bool,
        gadget: Bool
    ) {
        self.id = id
        self.command = command
        self.kind = kind
        self.proofTier = proofTier ?? Gate.defaultProofTier(id: id, kind: kind, physicalDevice: physicalDevice)
        self.acceptanceWeight = acceptanceWeight ?? Gate.defaultAcceptanceWeight(id: id, kind: kind, readinessEligible: readinessEligible, physicalDevice: physicalDevice)
        self.realStackRequired = realStackRequired ?? Gate.defaultRealStackRequired(id: id, kind: kind, proofTier: self.proofTier, physicalDevice: physicalDevice)
        self.canClaimRuntimeReadiness = canClaimRuntimeReadiness ?? (readinessEligible && self.realStackRequired && self.proofTier != "seed")
        self.state = state
        self.passed = passed
        self.satisfiesPrerequisite = satisfiesPrerequisite ?? passed
        self.reason = reason
        self.prerequisites = prerequisites
        self.prerequisitesSatisfied = prerequisitesSatisfied
        self.reportPaths = reportPaths
        self.reports = reports
        self.readinessEligible = readinessEligible
        self.physicalDevice = physicalDevice
        self.gadget = gadget
    }

    var evidenceKind: String {
        if kind.contains("reducer") {
            return "reducer_for_failure"
        }
        if kind == "simulator-runtime" {
            return "runtime_validation"
        }
        if kind == "blocked" {
            return "blocked_policy"
        }
        return "gate_report"
    }

    var doesNotAdvanceRuntimeReadiness: Bool {
        kind.contains("reducer") || state == "superseded" || state == "not_needed"
    }

    var readinessGateMember: Bool {
        readinessEligible
    }

    var currentlyReadinessEligible: Bool {
        readinessEligible && passed && prerequisitesSatisfied &&
            !doesNotAdvanceRuntimeReadiness
    }

    var simulatorReadinessMember: Bool {
        simulatorReadinessGateIDs.contains(id)
    }

    var currentlySimulatorReadinessSatisfied: Bool {
        simulatorReadinessMember &&
            kind == "simulator-runtime" &&
            passed &&
            prerequisitesSatisfied &&
            !doesNotAdvanceRuntimeReadiness
    }

    var resultPolicy: GateResultPolicy {
        classifyGateResult(self)
    }

    enum CodingKeys: String, CodingKey {
        case id
        case command
        case kind
        case proofTier = "proof_tier"
        case acceptanceWeight = "acceptance_weight"
        case realStackRequired = "real_stack_required"
        case canClaimRuntimeReadiness = "can_claim_runtime_readiness"
        case state
        case passed
        case satisfiesPrerequisite = "satisfies_prerequisite"
        case reason
        case evidenceKind = "evidence_kind"
        case doesNotAdvanceRuntimeReadiness = "does_not_advance_runtime_readiness"
        case prerequisites
        case prerequisitesSatisfied = "prerequisites_satisfied"
        case reportPaths = "report_paths"
        case reports
        case readinessEligible = "readiness_eligible"
        case readinessGateMember = "readiness_gate_member"
        case currentlyReadinessEligible = "currently_readiness_eligible"
        case simulatorReadinessMember = "simulator_readiness_member"
        case currentlySimulatorReadinessSatisfied = "currently_simulator_readiness_satisfied"
        case physicalDevice = "physical_device"
        case gadget
        case resultPolicy = "result_policy"
    }

    func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(id, forKey: .id)
        try container.encode(command, forKey: .command)
        try container.encode(kind, forKey: .kind)
        try container.encode(proofTier, forKey: .proofTier)
        try container.encode(acceptanceWeight, forKey: .acceptanceWeight)
        try container.encode(realStackRequired, forKey: .realStackRequired)
        try container.encode(canClaimRuntimeReadiness, forKey: .canClaimRuntimeReadiness)
        try container.encode(state, forKey: .state)
        try container.encode(passed, forKey: .passed)
        try container.encode(satisfiesPrerequisite, forKey: .satisfiesPrerequisite)
        try container.encode(reason, forKey: .reason)
        try container.encode(evidenceKind, forKey: .evidenceKind)
        try container.encode(doesNotAdvanceRuntimeReadiness, forKey: .doesNotAdvanceRuntimeReadiness)
        try container.encode(prerequisites, forKey: .prerequisites)
        try container.encode(prerequisitesSatisfied, forKey: .prerequisitesSatisfied)
        try container.encode(reportPaths, forKey: .reportPaths)
        try container.encode(reports, forKey: .reports)
        try container.encode(readinessEligible, forKey: .readinessEligible)
        try container.encode(readinessGateMember, forKey: .readinessGateMember)
        try container.encode(currentlyReadinessEligible, forKey: .currentlyReadinessEligible)
        try container.encode(simulatorReadinessMember, forKey: .simulatorReadinessMember)
        try container.encode(currentlySimulatorReadinessSatisfied, forKey: .currentlySimulatorReadinessSatisfied)
        try container.encode(physicalDevice, forKey: .physicalDevice)
        try container.encode(gadget, forKey: .gadget)
        try container.encode(resultPolicy, forKey: .resultPolicy)
    }
}

struct SimulatorReadinessCapability: Codable, Equatable {
    let id: String
    let gateID: String
    let description: String

    enum CodingKeys: String, CodingKey {
        case id
        case gateID = "gate_id"
        case description
    }
}

struct StatusDocument: Encodable {
    let area: String
    let generatedAt: String
    let gitSHA: String
    let roadmapPath: String
    let gates: [GateStatus]
    let simulatorAllowed: Bool
    let simulatorRequiredBeforePhysical: Bool
    let simulatorGatesComplete: Bool
    let simulatorReadinessCapabilities: [SimulatorReadinessCapability]
    let simulatorReadinessGateIDs: [String]
    let simulatorReadinessMissingGateIDs: [String]
    let requiredSimulatorID: String
    let requiredSimulatorName: String
    let physicalDeviceAllowed: Bool
    let physicalDeviceBlockers: [String]
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
        case simulatorReadinessCapabilities = "simulator_readiness_capabilities"
        case simulatorReadinessGateIDs = "simulator_readiness_gate_ids"
        case simulatorReadinessMissingGateIDs = "simulator_readiness_missing_gate_ids"
        case requiredSimulatorID = "required_simulator_id"
        case requiredSimulatorName = "required_simulator_name"
        case physicalDeviceAllowed = "physical_device_allowed"
        case physicalDeviceBlockers = "physical_device_blockers"
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
    let selectedGateProofTier: String
    let selectedGateAcceptanceWeight: String
    let selectedGateRealStackRequired: Bool
    let selectedGateCanClaimRuntimeReadiness: Bool
    let selectedGateResultClassification: String
    let runtimePatchAllowed: Bool
    let harnessPatchAllowed: Bool
    let continueRefreshAllowed: Bool
    let mustStop: Bool
    let requiredNextAction: String
    let resultClassificationReason: String
    let owningLayer: String
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
    let simulatorReadinessCapabilities: [SimulatorReadinessCapability]
    let simulatorReadinessGateIDs: [String]
    let simulatorReadinessMissingGateIDs: [String]
    let selectedGateUsesSimulator: Bool
    let requiredSimulatorID: String
    let requiredSimulatorName: String
    let physicalDeviceAllowed: Bool
    let physicalDeviceBlockers: [String]
    let releaseGateEligible: Bool
    let readinessGateEligible: Bool
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
        case selectedGateProofTier = "selected_gate_proof_tier"
        case selectedGateAcceptanceWeight = "selected_gate_acceptance_weight"
        case selectedGateRealStackRequired = "selected_gate_real_stack_required"
        case selectedGateCanClaimRuntimeReadiness = "selected_gate_can_claim_runtime_readiness"
        case selectedGateResultClassification = "selected_gate_result_classification"
        case runtimePatchAllowed = "runtime_patch_allowed"
        case harnessPatchAllowed = "harness_patch_allowed"
        case continueRefreshAllowed = "continue_refresh_allowed"
        case mustStop = "must_stop"
        case requiredNextAction = "required_next_action"
        case resultClassificationReason = "result_classification_reason"
        case owningLayer = "owning_layer"
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
        case simulatorReadinessCapabilities = "simulator_readiness_capabilities"
        case simulatorReadinessGateIDs = "simulator_readiness_gate_ids"
        case simulatorReadinessMissingGateIDs = "simulator_readiness_missing_gate_ids"
        case selectedGateUsesSimulator = "selected_gate_uses_simulator"
        case requiredSimulatorID = "required_simulator_id"
        case requiredSimulatorName = "required_simulator_name"
        case physicalDeviceAllowed = "physical_device_allowed"
        case physicalDeviceBlockers = "physical_device_blockers"
        case releaseGateEligible = "release_gate_eligible"
        case readinessGateEligible = "readiness_gate_eligible"
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

func supportedTCTIGateTargets() throws -> Set<String> {
    guard let output = run("/usr/bin/env", ["make", "-s", "tcti-gate-list"]) else {
        throw HarnessError.invalid("could not read supported tcti-gate targets from make tcti-gate-list")
    }
    return Set(output.split(whereSeparator: \.isNewline).map(String.init))
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
let tctiBuildRoot = ProcessInfo.processInfo.environment["ORLIX_TCTI_BUILD_ROOT"].map {
    URL(fileURLWithPath: $0, relativeTo: root).standardizedFileURL
} ?? root.appendingPathComponent("Build/TCTI", isDirectory: true)
let roadmapURL = ProcessInfo.processInfo.environment["ORLIX_TCTI_ROADMAP_PATH"].map {
    URL(fileURLWithPath: $0, relativeTo: root).standardizedFileURL
} ?? root.appendingPathComponent(".agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json")
let environmentPolicyURL = root.appendingPathComponent(".agents/skills/orlix-tcti-next-step/references/environment-policy.json")
let statusURL = outputRoot.appendingPathComponent("status.json")
let nextTaskURL = outputRoot.appendingPathComponent("next-task.json")
let nextTaskMarkdownURL = outputRoot.appendingPathComponent("next-task.md")
let environmentPolicy = loadEnvironmentPolicy()
let requiredSimulatorID = ProcessInfo.processInfo.environment["ORLIX_TCTI_REQUIRED_SIMULATOR_ID"] ?? environmentPolicy.requiredSimulatorID
let requiredSimulatorName = ProcessInfo.processInfo.environment["ORLIX_TCTI_REQUIRED_SIMULATOR_NAME"] ?? environmentPolicy.requiredSimulatorName
let simulatorReadinessCapabilities: [SimulatorReadinessCapability] = {
    let prefix = "sim" + "ulator-" + "tcti-"
    return [
        SimulatorReadinessCapability(id: "first_syscall", gateID: prefix + "init-first-syscall", description: "TCTI reaches the first Linux syscall on the pinned simulator."),
        SimulatorReadinessCapability(id: "runtime_stability", gateID: prefix + "runtime-stability", description: "Post-launch TCTI runtime has no panic, init death, user fault, BUG, Oops, SIGSEGV, fatal error, crash, or signaled-process marker."),
        SimulatorReadinessCapability(id: "linux_console_usability", gateID: prefix + "linux-console-usability", description: "Pinned simulator captures the Linux console usability marker from TCTI execution."),
        SimulatorReadinessCapability(id: "static_busybox_start", gateID: prefix + "static-busybox-start", description: "Static BusyBox starts under TCTI on the pinned simulator."),
        SimulatorReadinessCapability(id: "static_busybox_shell_command", gateID: prefix + "static-busybox-shell-command", description: "Static BusyBox shell executes a command under TCTI on the pinned simulator."),
        SimulatorReadinessCapability(id: "full_shell_usability", gateID: prefix + "full-shell-usability", description: "Requires a current passing runtime-validation gate to prove full shell usability."),
        SimulatorReadinessCapability(id: "package_behavior", gateID: prefix + "package-behavior", description: "Requires a current passing runtime-validation gate to prove package behavior."),
        SimulatorReadinessCapability(id: "dynamic_loader_support", gateID: prefix + "dynamic-loader-support", description: "Requires a current passing runtime-validation gate to prove dynamic-loader support."),
        SimulatorReadinessCapability(id: "signals", gateID: prefix + "signals", description: "Requires a current passing runtime-validation gate to prove Linux signal behavior."),
        SimulatorReadinessCapability(id: "vfs_completeness", gateID: prefix + "vfs-completeness", description: "Requires a current passing runtime-validation gate to prove VFS completeness."),
        SimulatorReadinessCapability(id: "full_linux_runtime_readiness", gateID: prefix + "full-linux-runtime-readiness", description: "Requires a current passing runtime-validation gate to prove full Linux runtime readiness before any phone gate can be selected."),
    ]
}()
let simulatorReadinessGateIDs = simulatorReadinessCapabilities.map(\.gateID)
let physicalOptInBlockedGateID = "blocked-physical-device-opt-in-required"

func relativePath(_ url: URL) -> String {
    let rootPath = root.path.hasSuffix("/") ? root.path : root.path + "/"
    if url.path.hasPrefix(rootPath) {
        return String(url.path.dropFirst(rootPath.count))
    }
    return url.path
}

func loadRoadmap(from url: URL = roadmapURL) throws -> Roadmap {
    let data = try Data(contentsOf: url)
    let roadmap = try JSONDecoder().decode(Roadmap.self, from: data)
    guard roadmap.area == "orlix-tcti" else {
        throw HarnessError.invalid("roadmap area must be orlix-tcti")
    }
    return roadmap
}

func loadEnvironmentPolicy() -> EnvironmentPolicy {
    do {
        let data = try Data(contentsOf: environmentPolicyURL)
        return try JSONDecoder().decode(EnvironmentPolicy.self, from: data)
    } catch {
        fputs("error: missing or invalid \(relativePath(environmentPolicyURL)): \(error)\n", stderr)
        exit(2)
    }
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

func pathMatches(_ path: String, _ pattern: String) -> Bool {
    if pattern.hasSuffix("/**") {
        let prefix = String(pattern.dropLast(3))
        return path == prefix || path.hasPrefix(prefix + "/")
    }
    if pattern == "docs/plans/active/*/IMPLEMENT.md" {
        let components = path.split(separator: "/").map(String.init)
        return components.count == 5 &&
            components[0] == "docs" &&
            components[1] == "plans" &&
            components[2] == "active" &&
            components[4] == "IMPLEMENT.md"
    }
    return path == pattern
}

let allGateExecutionInvalidationPatterns = [
    "tools/tcti/**",
    "tools/runtime/**",
    ".agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json",
    ".agents/skills/orlix-tcti-next-step/references/environment-policy.json",
]

let statusOnlyInvalidationPatterns = [
    ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
    ".agents/skills/orlix-tcti-next-step/scripts/status",
    ".agents/skills/orlix-tcti-next-step/scripts/next",
    ".agents/skills/orlix-tcti-next-step/scripts/task-envelope-check",
    ".agents/skills/orlix-tcti-next-step/scripts/goal-loop",
    ".codex/**",
    "docs/harness/**",
    "AGENTS.md",
    "docs/goals/active/**",
    "docs/plans/active/*/IMPLEMENT.md",
]

let runtimeSimulatorInvalidationPatterns = [
    "tools/runtime/orlix-runtime-validation.sh",
    ".agents/skills/orlix-tcti-next-step/references/environment-policy.json",
    "project.yml",
    "Orlix/**",
    "OrlixOS/**",
    "OrlixKernel/Sources/ports/orlix/**",
    "OrlixMLibC/**",
]

let goldenNoPhoneInvalidationPatterns = [
    "tools/tcti/orlix-tcti-gate.swift",
    "OrlixKernel/Tests/TCTI/**",
    "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/**",
]

let kernelProofInvalidationPatterns = [
    "OrlixKernel/Sources/ports/orlix/**",
    "OrlixKernel/Makefile",
    "tools/tcti/orlix-tcti-gate.swift",
]

let mlibcProofInvalidationPatterns = [
    "OrlixMLibC/**",
    "OrlixKernel/Sources/ports/orlix/**",
    "tools/tcti/**",
]

let shellCoreutilsOCIInvalidationPatterns = [
    "OrlixOS/**",
    "OrlixMLibC/**",
    "OrlixKernel/Sources/ports/orlix/**",
    "tools/runtime/**",
    "tools/tcti/**",
]

func matchesAny(_ path: String, _ patterns: [String]) -> Bool {
    patterns.contains { pathMatches(path, $0) }
}

func isRuntimeSimulatorGate(_ gate: Gate) -> Bool {
    gate.kind == "simulator-runtime" || gate.id.hasPrefix("simulator-tcti-") || gate.id.hasPrefix("tcti-simulator-")
}

func isGoldenOrNoPhoneGate(_ gate: Gate) -> Bool {
    gate.id.hasPrefix("golden-") ||
        gate.id.hasPrefix("switch-") ||
        gate.id.hasPrefix("diff-switch-") ||
        gate.id.hasPrefix("no-phone-") ||
        gate.kind.contains("no-phone") ||
        gate.kind.contains("golden") ||
        gate.command.contains("tcti-gate")
}

func isKernelProofGate(_ gate: Gate) -> Bool {
    gate.proofTier == "kernel" || gate.id.contains("kernel")
}

func isMLibCProofGate(_ gate: Gate) -> Bool {
    gate.proofTier.contains("mlibc") || gate.id.contains("mlibc")
}

func isShellCoreutilsOCIGate(_ gate: Gate) -> Bool {
    gate.id.contains("shell") ||
        gate.id.contains("busybox") ||
        gate.id.contains("coreutils") ||
        gate.id.contains("oci") ||
        gate.id.contains("package") ||
        gate.id.contains("dynamic-loader") ||
        gate.proofTier.contains("shell") ||
        gate.proofTier.contains("package")
}

func makefileCanAffectGateExecution(_ gate: Gate) -> Bool {
    gate.command.contains("make ") || gate.command.hasPrefix("make ")
}

func doesChangedPathInvalidateGate(_ gate: Gate, changedPath: String) -> Bool {
    if matchesAny(changedPath, allGateExecutionInvalidationPatterns) {
        return true
    }
    if changedPath == "Makefile" {
        return makefileCanAffectGateExecution(gate)
    }
    if isRuntimeSimulatorGate(gate) && matchesAny(changedPath, runtimeSimulatorInvalidationPatterns) {
        return true
    }
    if isGoldenOrNoPhoneGate(gate) && matchesAny(changedPath, goldenNoPhoneInvalidationPatterns) {
        return true
    }
    if isKernelProofGate(gate) && matchesAny(changedPath, kernelProofInvalidationPatterns) {
        return true
    }
    if isMLibCProofGate(gate) && matchesAny(changedPath, mlibcProofInvalidationPatterns) {
        return true
    }
    if isShellCoreutilsOCIGate(gate) && matchesAny(changedPath, shellCoreutilsOCIInvalidationPatterns) {
        return true
    }
    return false
}

func changedPathsSinceReport(reportGitSHA: String, currentGitSHA: String) -> [String]? {
    guard let output = run("/usr/bin/env", ["git", "diff", "--name-only", "\(reportGitSHA)..\(currentGitSHA)"]) else {
        return nil
    }
    return output
        .split(whereSeparator: \.isNewline)
        .map(String.init)
        .filter { !$0.isEmpty }
}

func executionFreshness(for gate: Gate, reportGitSHA: String?) -> ExecutionFreshness {
    let current = gitSHA()
    guard let reportGitSHA, !reportGitSHA.isEmpty else {
        return ExecutionFreshness(
            reportGitSHA: reportGitSHA,
            currentGitSHA: current,
            executionFresh: false,
            statusRecomputed: true,
            changedPathsSinceReport: [],
            ignoredNonExecutionPaths: [],
            invalidatingPaths: [],
            reason: "report lacks git_sha provenance"
        )
    }
    if reportGitSHA == current {
        return ExecutionFreshness(
            reportGitSHA: reportGitSHA,
            currentGitSHA: current,
            executionFresh: true,
            statusRecomputed: true,
            changedPathsSinceReport: [],
            ignoredNonExecutionPaths: [],
            invalidatingPaths: [],
            reason: "report git_sha matches current HEAD"
        )
    }
    guard let changed = changedPathsSinceReport(reportGitSHA: reportGitSHA, currentGitSHA: current) else {
        return ExecutionFreshness(
            reportGitSHA: reportGitSHA,
            currentGitSHA: current,
            executionFresh: false,
            statusRecomputed: true,
            changedPathsSinceReport: [],
            ignoredNonExecutionPaths: [],
            invalidatingPaths: [],
            reason: "unable to compute changed paths since report git_sha"
        )
    }
    let invalidating = changed.filter { doesChangedPathInvalidateGate(gate, changedPath: $0) }
    let ignored = changed.filter { !invalidating.contains($0) }
    if invalidating.isEmpty {
        return ExecutionFreshness(
            reportGitSHA: reportGitSHA,
            currentGitSHA: current,
            executionFresh: true,
            statusRecomputed: true,
            changedPathsSinceReport: changed,
            ignoredNonExecutionPaths: ignored,
            invalidatingPaths: [],
            reason: "report git_sha differs from HEAD only by non-execution paths"
        )
    }
    return ExecutionFreshness(
        reportGitSHA: reportGitSHA,
        currentGitSHA: current,
        executionFresh: false,
        statusRecomputed: true,
        changedPathsSinceReport: changed,
        ignoredNonExecutionPaths: ignored,
        invalidatingPaths: invalidating,
        reason: "changed paths invalidate gate execution: \(invalidating.joined(separator: ", "))"
    )
}

func reportExecutionFresh(_ report: ReportFact) -> Bool {
    report.executionFreshness?.executionFresh ?? (report.gitSHA == gitSHA())
}

func reportFreshnessReason(_ report: ReportFact) -> String {
    report.executionFreshness?.reason ?? "report git_sha \(report.gitSHA == gitSHA() ? "matches" : "does not match") current HEAD"
}

func reportFailures(_ value: Any?) -> [ReportFailureFact] {
    guard let rawFailures = value as? [Any] else { return [] }
    return rawFailures.enumerated().map { index, raw in
        if let string = raw as? String {
            return ReportFailureFact(id: "failure-\(index + 1)", message: string)
        }
        if let dictionary = raw as? [String: Any] {
            let id = stringValue(dictionary["id"]) ??
                stringValue(dictionary["kind"]) ??
                stringValue(dictionary["name"]) ??
                "failure-\(index + 1)"
            let message = stringValue(dictionary["message"]) ??
                stringValue(dictionary["summary"]) ??
                stringValue(dictionary["reason"]) ??
                id
            return ReportFailureFact(id: id, message: message)
        }
        return ReportFailureFact(id: "failure-\(index + 1)", message: "\(raw)")
    }
}

func forbiddenBehaviorViolations(_ value: Any?) -> [String] {
    guard let forbidden = value as? [String: Any] else { return [] }
    return forbidden.keys.sorted().filter { boolValue(forbidden[$0]) }
}

func lowercasedEvidenceText(_ status: GateStatus) -> String {
    let failures = status.reports.flatMap(\.failures).map { "\($0.id) \($0.message)" }
    let reportPaths = status.reports.map(\.path)
    return ([status.id, status.kind, status.state, status.reason] + failures + reportPaths)
        .joined(separator: " ")
        .lowercased()
}

func targetName(from command: String) -> String? {
    command.split(separator: " ").first { $0.hasPrefix("TARGET=") }
        .map { String($0.dropFirst("TARGET=".count)) }
}

func reportCarriesSelectedGateMetadata(_ report: ReportFact, for status: GateStatus) -> Bool {
    if report.path.hasPrefix("Build/Reports/runtime/") {
        return status.kind == "simulator-runtime" || status.physicalDevice
    }

    if report.path.hasPrefix("Build/TCTI/reports/") {
        guard let target = targetName(from: status.command) else {
            return true
        }
        return report.path == "Build/TCTI/reports/\(target)/report.json"
    }

    return false
}

func reportMetadataDrift(_ status: GateStatus) -> [String] {
    let existingReports = status.reports
        .filter(\.exists)
        .filter { reportCarriesSelectedGateMetadata($0, for: status) }
    return existingReports.flatMap { report -> [String] in
        var drift: [String] = []
        if let value = report.proofTier, value != status.proofTier {
            drift.append("\(report.path): proof_tier=\(value) expected \(status.proofTier)")
        }
        if let value = report.acceptanceWeight, value != status.acceptanceWeight {
            drift.append("\(report.path): acceptance_weight=\(value) expected \(status.acceptanceWeight)")
        }
        if let value = report.realStackRequired, value != status.realStackRequired {
            drift.append("\(report.path): real_stack_required=\(value) expected \(status.realStackRequired)")
        }
        if let value = report.canClaimRuntimeReadiness, value != status.canClaimRuntimeReadiness {
            drift.append("\(report.path): can_claim_runtime_readiness=\(value) expected \(status.canClaimRuntimeReadiness)")
        }
        if report.releaseGateEligible && status.proofTier != "release" {
            drift.append("\(report.path): release_gate_eligible=true for non-release gate")
        }
        if report.readinessGateEligible && !status.readinessEligible {
            drift.append("\(report.path): readiness_gate_eligible=true but roadmap gate is not readiness-eligible")
        }
        return drift
    }
}

func policy(
    classification: String,
    runtimePatchAllowed: Bool,
    harnessPatchAllowed: Bool,
    continueRefreshAllowed: Bool,
    mustStop: Bool,
    requiredNextAction: String,
    reason: String,
    owningLayer: String
) -> GateResultPolicy {
    GateResultPolicy(
        classification: classification,
        runtimePatchAllowed: runtimePatchAllowed,
        harnessPatchAllowed: harnessPatchAllowed,
        continueRefreshAllowed: continueRefreshAllowed,
        mustStop: mustStop,
        requiredNextAction: requiredNextAction,
        reason: reason,
        owningLayer: owningLayer
    )
}

func selectedCommandCanGenerateProof(_ status: GateStatus) -> Bool {
    guard status.prerequisitesSatisfied,
          !status.physicalDevice,
          !status.command.isEmpty else {
        return false
    }

    if status.command.hasPrefix("make tcti-gate TARGET=") {
        return true
    }

    return status.kind == "simulator-runtime" &&
        status.command.hasPrefix("make runtime-validation DESTINATION=iphonesimulator ") &&
        status.command.contains("ORLIX_SIMULATOR_ID=\(requiredSimulatorID)") &&
        status.command.contains("ORLIX_TCTI_REQUIRED_SIMULATOR_ID=\(requiredSimulatorID)") &&
        status.command.contains("ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=\(requiredSimulatorName)")
}

func classifyGateResult(_ status: GateStatus) -> GateResultPolicy {
    if status.state == "stale" {
        return policy(
            classification: "stale_proof_refresh",
            runtimePatchAllowed: false,
            harnessPatchAllowed: false,
            continueRefreshAllowed: true,
            mustStop: false,
            requiredNextAction: "refresh the selected gate report at current HEAD",
            reason: status.reason,
            owningLayer: "proof refresh"
        )
    }

    let forbidden = status.reports.flatMap(\.forbiddenBehaviorViolations)
    if !forbidden.isEmpty {
        return policy(
            classification: "forbidden_behavior_violation",
            runtimePatchAllowed: false,
            harnessPatchAllowed: false,
            continueRefreshAllowed: false,
            mustStop: true,
            requiredNextAction: "stop and reduce forbidden behavior before any implementation work",
            reason: "Forbidden behavior fields were true: \(Array(Set(forbidden)).sorted().joined(separator: ", ")).",
            owningLayer: "safety boundary"
        )
    }

    let drift = reportMetadataDrift(status)
    if !drift.isEmpty {
        return policy(
            classification: "proof_tier_report_status_metadata_drift",
            runtimePatchAllowed: false,
            harnessPatchAllowed: true,
            continueRefreshAllowed: false,
            mustStop: true,
            requiredNextAction: "repair the harness/report metadata contract before rerunning product work",
            reason: drift.joined(separator: "; "),
            owningLayer: "TCTI harness/report contract"
        )
    }

    if status.passed && status.currentlySimulatorReadinessSatisfied {
        return policy(
            classification: "readiness_gate_pass",
            runtimePatchAllowed: false,
            harnessPatchAllowed: false,
            continueRefreshAllowed: true,
            mustStop: false,
            requiredNextAction: "regenerate harness state and continue only if the next selected gate is stale or missing proof refresh",
            reason: status.reason,
            owningLayer: "readiness proof"
        )
    }

    if status.passed {
        return policy(
            classification: "stale_proof_refresh",
            runtimePatchAllowed: false,
            harnessPatchAllowed: false,
            continueRefreshAllowed: true,
            mustStop: false,
            requiredNextAction: "regenerate harness state and follow the next selected gate",
            reason: status.reason,
            owningLayer: "proof refresh"
        )
    }

    let evidenceText = lowercasedEvidenceText(status)
    let railContractTerms = [
        "historical",
        "non-durable",
        "obsolete",
        "superseded",
        "stale generated",
        "simulator-unsupported-signature",
        "dynamic-loader-scope",
        "simulator-static-pie-event",
        "no recorded simulator stability report",
        "does not include structured",
        "expected reducer/report path",
        "reducer-report",
        "reducer-evidence",
        "missing reducer report or pass-regression evidence",
        "simulator-report-stale",
        "not execution-fresh for this rail",
    ]
    let railLike = status.kind == "rail" || status.kind.contains("production")
    if railLike && railContractTerms.contains(where: { evidenceText.contains($0) }) {
        return policy(
            classification: "rail_evidence_contract_bug",
            runtimePatchAllowed: false,
            harnessPatchAllowed: true,
            continueRefreshAllowed: false,
            mustStop: true,
            requiredNextAction: "repair the selected rail evidence contract or map it to durable current evidence",
            reason: status.reason,
            owningLayer: "TCTI harness/report contract"
        )
    }

    let environmentTerms = [
        "environment",
        "coresimulator",
        "simctl",
        "xcodebuild",
        "bootstatus",
        "booted simulator",
        "storage",
        "runner attach",
        "preflight",
        "destination",
        "selected_device",
        "required simulator",
    ]
    if environmentTerms.contains(where: { evidenceText.contains($0) }) {
        return policy(
            classification: "environment_only_failure",
            runtimePatchAllowed: false,
            harnessPatchAllowed: false,
            continueRefreshAllowed: false,
            mustStop: true,
            requiredNextAction: "fix or rerun the environment/simulator setup before changing product code",
            reason: status.reason,
            owningLayer: "environment"
        )
    }

    let missingReports = status.reports.contains { !$0.exists } ||
        status.reportPaths.contains { $0.contains("/reproducers/") || $0.contains("*.json") }
    let reducerLike = status.kind.contains("reducer") || evidenceText.contains("reducer")
    if status.state == "missing" && (missingReports || reducerLike) {
        let canGenerate = selectedCommandCanGenerateProof(status)
        return policy(
            classification: "missing_generated_artifact",
            runtimePatchAllowed: false,
            harnessPatchAllowed: false,
            continueRefreshAllowed: canGenerate,
            mustStop: !canGenerate,
            requiredNextAction: "generate or refresh the required proof artifact before selecting implementation work",
            reason: status.reason,
            owningLayer: "proof artifact"
        )
    }

    let currentFailure = status.reports.contains {
        $0.exists && !$0.passed && reportExecutionFresh($0)
    }
    let realProductFailure = status.realStackRequired ||
        status.kind == "simulator-runtime" ||
        status.kind.contains("production")
    if ["fail", "ready"].contains(status.state) && currentFailure && realProductFailure {
        return policy(
            classification: "current_runtime_product_failure",
            runtimePatchAllowed: true,
            harnessPatchAllowed: false,
            continueRefreshAllowed: false,
            mustStop: true,
            requiredNextAction: "stop, reduce the current failure, and patch only the owning runtime/product layer",
            reason: status.reason,
            owningLayer: status.kind == "simulator-runtime" ? "runtime-validation selected stack" : "selected gate owning layer"
        )
    }

    if status.state == "missing" {
        let canGenerate = selectedCommandCanGenerateProof(status)
        return policy(
            classification: "missing_generated_artifact",
            runtimePatchAllowed: false,
            harnessPatchAllowed: false,
            continueRefreshAllowed: canGenerate,
            mustStop: !canGenerate,
            requiredNextAction: "produce the missing report or artifact named by the selected gate",
            reason: status.reason,
            owningLayer: "proof artifact"
        )
    }

    return policy(
        classification: "current_runtime_product_failure",
        runtimePatchAllowed: false,
        harnessPatchAllowed: false,
        continueRefreshAllowed: false,
        mustStop: true,
        requiredNextAction: "stop and classify the selected gate manually because the evidence did not match a known safe policy",
        reason: status.reason,
        owningLayer: "unclassified selected gate"
    )
}

func runtimeArtifactURL(_ artifact: String) -> URL {
    let directURL = root.appendingPathComponent(artifact)
    if fileManager.fileExists(atPath: directURL.path) {
        return directURL
    }
    return root
        .appendingPathComponent("Build/Reports/runtime", isDirectory: true)
        .appendingPathComponent(artifact)
}

func runtimeArtifactText(_ object: [String: Any], suffix: String) -> String? {
    let artifacts = (object["artifacts"] as? [Any] ?? []).compactMap { stringValue($0) }
    guard let artifact = artifacts.first(where: { $0.hasSuffix(suffix) }) else {
        return nil
    }
    return try? String(contentsOf: runtimeArtifactURL(artifact), encoding: .utf8)
}

func runtimeArtifactContains(_ object: [String: Any], suffix: String, marker: String) -> Bool {
    runtimeArtifactText(object, suffix: suffix)?.contains(marker) == true
}

func runtimeReportFatalFree(_ object: [String: Any]) -> Bool {
    let fatalPatterns = [
        "Kernel panic",
        "Attempted to kill init",
        "Attempted kill init",
        "Orlix TCTI: user fault",
        "panic - not syncing",
        "BUG:",
        "Oops",
        "SIGSEGV",
        "fatal error",
        "Fatal error",
        "crash",
        "Crash",
        "orlix-init: process signaled ",
    ]
    let logSuffixes = [
        "launch-console.log",
        "launch.log",
        "simulator-terminal-output.txt",
        "simulator-unified.log",
    ]

    for suffix in logSuffixes {
        guard let text = runtimeArtifactText(object, suffix: suffix) else {
            return false
        }
        if fatalPatterns.contains(where: { text.contains($0) }) {
            return false
        }
    }
    if let text = runtimeArtifactText(object, suffix: "tcti-simulator-fatal-runtime.txt"),
       !text.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
        return false
    }
    return true
}

func reportFact(target: String) -> ReportFact {
    let url = tctiBuildRoot.appendingPathComponent("reports/\(target)/report.json")
    guard fileManager.fileExists(atPath: url.path) else {
        return ReportFact(
            path: relativePath(url),
            exists: false,
            status: "missing",
            passed: false,
            proofTier: nil,
            acceptanceWeight: nil,
            realStackRequired: nil,
            canClaimRuntimeReadiness: nil,
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
            proofTier: stringValue(object["proof_tier"]),
            acceptanceWeight: stringValue(object["acceptance_weight"]),
            realStackRequired: object["real_stack_required"].map(boolValue),
            canClaimRuntimeReadiness: object["can_claim_runtime_readiness"].map(boolValue),
            releaseGateEligible: boolValue(object["release_gate_eligible"]),
            readinessGateEligible: boolValue(object["readiness_gate_eligible"]),
            gitSHA: stringValue(object["git_sha"]),
            failures: reportFailures(object["failures"]),
            forbiddenBehaviorViolations: forbiddenBehaviorViolations(object["forbidden_behavior"])
        )
    } catch {
        return ReportFact(
            path: relativePath(url),
            exists: true,
            status: "error",
            passed: false,
            proofTier: nil,
            acceptanceWeight: nil,
            realStackRequired: nil,
            canClaimRuntimeReadiness: nil,
            releaseGateEligible: false,
            readinessGateEligible: false,
            gitSHA: nil
        )
    }
}

func reportFact(target: String, gate: Gate) -> ReportFact {
    let report = reportFact(target: target)
    guard report.exists else { return report }
    return report.withExecutionFreshness(executionFreshness(for: gate, reportGitSHA: report.gitSHA))
}

func currentReportPassed(_ report: ReportFact) -> Bool {
    report.status == "pass" && report.passed && reportExecutionFresh(report)
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
        let artifactGitSHA = stringValue(object["git_sha"])
        guard artifactGitSHA == gitSHA() else {
            return (false, "\(validation) is stale or lacks git_sha for current HEAD", [])
        }
        if hasHashes && hasBinary {
            return (true, "\(caseID) structural validation artifact is current for HEAD with source and binary hashes", [])
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
    let report = reportFact(target: target, gate: gate)
    let passed = currentReportPassed(report)
    let state = passed ? "pass" : (report.status == "pass" ? "stale" : report.status)
    let reason: String
    if !report.exists {
        reason = "\(target) report missing"
    } else if passed {
        reason = "\(target) report status=\(report.status), passed=\(report.passed), execution_fresh=true; \(reportFreshnessReason(report))"
    } else if report.status == "pass" && report.passed && !reportExecutionFresh(report) {
        reason = "\(target) report is execution-stale for current HEAD; \(reportFreshnessReason(report))"
    } else {
        reason = "\(target) report status=\(report.status), passed=\(report.passed)"
    }
    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        proofTier: gate.proofTier,
        acceptanceWeight: gate.acceptanceWeight,
        realStackRequired: gate.realStackRequired,
        canClaimRuntimeReadiness: gate.canClaimRuntimeReadiness,
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
    if currentReportPassed(report) {
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
        passed: false,
        satisfiesPrerequisite: true,
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

func simdSelfMoveSignature(_ text: String) -> Bool {
    text.contains("insn=0x6e144401") &&
        text.contains("unsupported instruction") &&
        text.contains("exitcode=0x00000004")
}

func simdMOVI2SSignature(_ text: String) -> Bool {
    text.contains("unsupported instruction") &&
        (text.contains("insn=0xf002420") || text.contains("insn=0x0f002420"))
}

func simdSTRSSignature(_ text: String) -> Bool {
    text.contains("unsupported instruction") &&
        text.contains("insn=0xbd01c260") &&
        text.contains("x19=")
}

func simdDUP2DSignature(_ text: String) -> Bool {
    text.contains("unsupported instruction") &&
        text.contains("insn=0x4e080d80") &&
        text.contains("x12=")
}

func ldrswSignExtensionSignature(_ text: String) -> Bool {
    text.contains("Orlix TCTI: user fault") &&
        text.contains("access=1") &&
        text.contains("/dev/hvc0") &&
        (text.contains("Attempted kill init") || text.contains("Attempted to kill init"))
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

func postBashMmapReadFaultReducerPass(_ gate: Gate) -> GateStatus {
	let reducerReport = reportFact(target: "tcti-post-bash-mmap-read-fault-reducer")
	let reducerObject = try? loadJSONObject(root.appendingPathComponent(reducerReport.path))
	let reducerArtifacts = (reducerObject?["artifacts"] as? [Any] ?? []).compactMap { stringValue($0) }
	let runtimeGates = [
		"tcti-static-busybox-shell-command",
		"tcti-simulator-stability",
	]
	var matchingReport: ReportFact?
	for runtimeGate in runtimeGates {
		guard let (report, object) = latestRuntimeReport(gate: runtimeGate, destination: "iphonesimulator") else {
			continue
		}
		let events = object["tcti_runtime_events"] as? [String: Any] ?? [:]
		let staticPIE = events["static_pie_image"] as? [String: Any] ?? [:]
		let mmap = events["last_mmap_syscall"] as? [String: Any] ?? [:]
		let fault = events["fatal_user_fault"] as? [String: Any] ?? [:]
		let signal = events["signaled_process"] as? [String: Any] ?? [:]
		let staticPID = intValue(staticPIE["pid"])
		let faultPID = intValue(fault["pid"])
		let signaledPID = intValue(signal["pid"])
		let matchesCurrentFault = report.status == "fail" &&
			!report.passed &&
			report.gitSHA == gitSHA() &&
			stringValue(staticPIE["task"]) == "sh" &&
			staticPID != nil &&
			stringValue(mmap["task"]) == "sh" &&
			intValue(mmap["pid"]) == staticPID &&
			intValue(mmap["syscall"]) == 222 &&
			stringValue(fault["task"]) == "sh" &&
			faultPID == staticPID &&
			intValue(fault["access"]) == 1 &&
			intValue(fault["si"]) == 1 &&
			!(stringValue(fault["addr"]) ?? "").isEmpty &&
			stringValue(fault["addr"]) != "0x0" &&
			intValue(signal["signal"]) == 11 &&
			signaledPID == staticPID
		if matchesCurrentFault {
			matchingReport = report
			break
		}
	}

	if let report = matchingReport {
		let reducerCoversReport = reducerArtifacts.contains(report.path)
		if reducerReport.status == "pass",
		   reducerReport.passed,
		   reducerReport.gitSHA == gitSHA(),
		   reducerCoversReport {
			return GateStatus(
				id: gate.id,
				command: gate.command,
				kind: gate.kind,
				state: "pass",
				passed: true,
				reason: "post-Bash mmap/read-fault reducer report \(reducerReport.path) covers current simulator report \(report.path)",
				prerequisites: gate.prerequisites,
				prerequisitesSatisfied: false,
				reportPaths: gate.expectedReportPaths,
				reports: [report, reducerReport],
				readinessEligible: gate.readinessEligible,
				physicalDevice: gate.physicalDevice,
				gadget: gate.gadget
			)
		}
		return GateStatus(
			id: gate.id,
			command: gate.command,
			kind: gate.kind,
			state: "ready",
			passed: false,
			reason: "current simulator report \(report.path) records sh mmap syscall 222 followed by read fault and SIGSEGV signal=11; reduce that exact report before retrying simulator",
			prerequisites: gate.prerequisites,
			prerequisitesSatisfied: false,
			reportPaths: gate.expectedReportPaths,
			reports: reducerReport.exists ? [report, reducerReport] : [report],
			readinessEligible: gate.readinessEligible,
			physicalDevice: gate.physicalDevice,
			gadget: gate.gadget
		)
	}

	if reducerReport.status == "pass",
	   reducerReport.passed,
	   reducerReport.gitSHA == gitSHA() {
		return basicReportGate(gate, target: "tcti-post-bash-mmap-read-fault-reducer")
	}
	return GateStatus(
		id: gate.id,
		command: gate.command,
		kind: gate.kind,
		state: "not_needed",
		passed: false,
        satisfiesPrerequisite: true,
		reason: "latest simulator reports do not match the post-Bash mmap/read-fault reducer signature",
		prerequisites: gate.prerequisites,
		prerequisitesSatisfied: false,
		reportPaths: gate.expectedReportPaths,
		reports: reducerReport.exists ? [reducerReport] : [],
		readinessEligible: gate.readinessEligible,
		physicalDevice: gate.physicalDevice,
		gadget: gate.gadget
	)
}

func userDataWindowRefreshFixPass(_ gate: Gate) -> GateStatus {
	let fixReport = reportFact(target: "tcti-user-data-window-refresh-fix")
	let fixObject = try? loadJSONObject(root.appendingPathComponent(fixReport.path))
	let fixArtifacts = (fixObject?["artifacts"] as? [Any] ?? []).compactMap { stringValue($0) }
	let reducerReport = reportFact(target: "tcti-post-bash-mmap-read-fault-reducer")
	let reducerObject = try? loadJSONObject(root.appendingPathComponent(reducerReport.path))
	let reducerArtifacts = (reducerObject?["artifacts"] as? [Any] ?? []).compactMap { stringValue($0) }
	let runtimeGates = [
		"tcti-static-busybox-shell-command",
		"tcti-simulator-stability",
	]
	var matchingReport: ReportFact?
	for runtimeGate in runtimeGates {
		guard let (report, object) = latestRuntimeReport(gate: runtimeGate, destination: "iphonesimulator") else {
			continue
		}
		let events = object["tcti_runtime_events"] as? [String: Any] ?? [:]
		let staticPIE = events["static_pie_image"] as? [String: Any] ?? [:]
		let mmap = events["last_mmap_syscall"] as? [String: Any] ?? [:]
		let fault = events["fatal_user_fault"] as? [String: Any] ?? [:]
		let signal = events["signaled_process"] as? [String: Any] ?? [:]
		let staticPID = intValue(staticPIE["pid"])
		let matchesCurrentFault = report.status == "fail" &&
			!report.passed &&
			report.gitSHA == gitSHA() &&
			stringValue(staticPIE["task"]) == "sh" &&
			staticPID != nil &&
			stringValue(mmap["task"]) == "sh" &&
			intValue(mmap["pid"]) == staticPID &&
			intValue(mmap["syscall"]) == 222 &&
			stringValue(fault["task"]) == "sh" &&
			intValue(fault["pid"]) == staticPID &&
			intValue(fault["access"]) == 1 &&
			intValue(fault["si"]) == 1 &&
			!(stringValue(fault["addr"]) ?? "").isEmpty &&
			stringValue(fault["addr"]) != "0x0" &&
			intValue(signal["signal"]) == 11 &&
			intValue(signal["pid"]) == staticPID
		if matchesCurrentFault {
			matchingReport = report
			break
		}
	}

	if let report = matchingReport {
		let reducerCoversReport = reducerArtifacts.contains(report.path)
		let fixCoversReport = fixArtifacts.contains(report.path)
		if fixReport.status == "pass",
		   fixReport.passed,
		   fixReport.gitSHA == gitSHA(),
		   reducerReport.status == "pass",
		   reducerReport.passed,
		   reducerReport.gitSHA == gitSHA(),
		   reducerCoversReport,
		   fixCoversReport {
			return GateStatus(
				id: gate.id,
				command: gate.command,
				kind: gate.kind,
				state: "pass",
				passed: true,
				reason: "user-data window refresh fix report \(fixReport.path) covers current simulator report \(report.path)",
				prerequisites: gate.prerequisites,
				prerequisitesSatisfied: false,
				reportPaths: gate.expectedReportPaths,
				reports: [report, reducerReport, fixReport],
				readinessEligible: gate.readinessEligible,
				physicalDevice: gate.physicalDevice,
				gadget: gate.gadget
			)
		}
		return GateStatus(
			id: gate.id,
			command: gate.command,
			kind: gate.kind,
			state: "ready",
			passed: false,
			reason: "current simulator report \(report.path) records the reducer-backed post-Bash mmap/read fault and the fix report does not cover that report yet",
			prerequisites: gate.prerequisites,
			prerequisitesSatisfied: false,
			reportPaths: gate.expectedReportPaths,
			reports: [report, reducerReport, fixReport],
			readinessEligible: gate.readinessEligible,
			physicalDevice: gate.physicalDevice,
			gadget: gate.gadget
		)
	}

	return basicReportGate(gate, target: "tcti-user-data-window-refresh-fix")
}

func postBusyBoxShellCommandSIGILLReducerPass(_ gate: Gate) -> GateStatus {
	let reducerReport = reportFact(target: "tcti-post-busybox-shell-command-sigill-reducer")
	let reducerObject = try? loadJSONObject(root.appendingPathComponent(reducerReport.path))
	let reducerArtifacts = (reducerObject?["artifacts"] as? [Any] ?? []).compactMap { stringValue($0) }
	guard let (report, object) = latestRuntimeReport(gate: "tcti-static-busybox-shell-command", destination: "iphonesimulator") else {
		return basicReportGate(gate, target: "tcti-post-busybox-shell-command-sigill-reducer")
	}
	let artifacts = (object["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
	let text = artifacts.compactMap { artifact -> String? in
		let url = root.appendingPathComponent("Build/Reports/runtime").appendingPathComponent(artifact)
		if let value = try? String(contentsOf: url, encoding: .utf8) {
			return value
		}
		return try? String(contentsOf: root.appendingPathComponent(artifact), encoding: .utf8)
	}.joined(separator: "\n")
	let matchesCurrentSIGILL = report.status == "fail" &&
		!report.passed &&
		report.gitSHA == gitSHA() &&
		text.contains("ORLIX-TCTI-BUSYBOX-USABLE") &&
		text.contains("Orlix TCTI: unsupported instruction task=sh") &&
		text.contains("signal=4")
	if matchesCurrentSIGILL {
		let reducerCoversReport = reducerArtifacts.contains(report.path)
		if reducerReport.status == "pass",
		   reducerReport.passed,
		   reducerReport.gitSHA == gitSHA(),
		   reducerCoversReport {
			return GateStatus(
				id: gate.id,
				command: gate.command,
				kind: gate.kind,
				state: "pass",
				passed: true,
				reason: "post-marker SIGILL reducer report \(reducerReport.path) covers current simulator report \(report.path)",
				prerequisites: gate.prerequisites,
				prerequisitesSatisfied: false,
				reportPaths: gate.expectedReportPaths,
				reports: [report, reducerReport],
				readinessEligible: gate.readinessEligible,
				physicalDevice: gate.physicalDevice,
				gadget: gate.gadget
			)
		}
		return GateStatus(
			id: gate.id,
			command: gate.command,
			kind: gate.kind,
			state: "ready",
			passed: false,
			reason: "current simulator report \(report.path) records BusyBox marker followed by unsupported instruction SIGILL; reduce that exact report before retrying simulator",
			prerequisites: gate.prerequisites,
			prerequisitesSatisfied: false,
			reportPaths: gate.expectedReportPaths,
			reports: reducerReport.exists ? [report, reducerReport] : [report],
			readinessEligible: gate.readinessEligible,
			physicalDevice: gate.physicalDevice,
			gadget: gate.gadget
		)
	}
	return basicReportGate(gate, target: "tcti-post-busybox-shell-command-sigill-reducer")
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
            proofTier: stringValue(object["proof_tier"]),
            acceptanceWeight: stringValue(object["acceptance_weight"]),
            realStackRequired: object["real_stack_required"].map(boolValue),
            canClaimRuntimeReadiness: object["can_claim_runtime_readiness"].map(boolValue),
            releaseGateEligible: boolValue(object["release_gate_eligible"]),
            readinessGateEligible: boolValue(object["readiness_gate_eligible"]),
            gitSHA: stringValue(object["git_sha"]),
            failures: reportFailures(object["failures"]),
            forbiddenBehaviorViolations: forbiddenBehaviorViolations(object["forbidden_behavior"])
        )
        return (fact, object)
    }
    return nil
}

func runtimeGateState(report: ReportFact, passed: Bool) -> String {
    if passed {
        return "pass"
    }
    if report.status == "pass" {
        return "stale"
    }
    return report.status
}

func simulatorFirstSyscallPass(_ gate: Gate) -> GateStatus {
    guard let latest = latestRuntimeReport(gate: "tcti-init-first-syscall", destination: "iphonesimulator") else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            proofTier: gate.proofTier,
            acceptanceWeight: gate.acceptanceWeight,
            realStackRequired: gate.realStackRequired,
            canClaimRuntimeReadiness: gate.canClaimRuntimeReadiness,
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
    let report = latest.0.withExecutionFreshness(executionFreshness(for: gate, reportGitSHA: latest.0.gitSHA))
    let object = latest.1

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
        reportExecutionFresh(report) &&
        stringValue(object["selected_device_id"]) == requiredSimulatorID &&
        stringValue(object["selected_device_name"]) == requiredSimulatorName &&
        intValue(object["simulator_booted_count"]) == 1 &&
        boolValue(object["simulator_single_booted"]) &&
        stringValue(object["backend"]) == "tcti" &&
        stringValue(object["profile"]) == "tcti_runtime" &&
        !boolValue(object["preflight_only"]) &&
        !boolValue(object["autonomous_tests_bypassed"]) &&
        forbiddenClear &&
        runtimeArtifactContains(object, suffix: "tcti-first-syscall.txt", marker: "Orlix TCTI: svc #0")
    let reason: String
    if reportOK {
        reason = "iphonesimulator runtime-validation report \(report.path) passed on \(requiredSimulatorName) with tcti runtime profile and forbidden behavior false"
    } else if !reportExecutionFresh(report) {
        reason = "latest iphonesimulator runtime-validation report \(report.path) is execution-stale for current HEAD; \(reportFreshnessReason(report))"
    } else if stringValue(object["selected_device_id"]) != requiredSimulatorID ||
        stringValue(object["selected_device_name"]) != requiredSimulatorName {
        reason = "latest iphonesimulator runtime-validation report \(report.path) did not run on required simulator \(requiredSimulatorName) (\(requiredSimulatorID))"
    } else if intValue(object["simulator_booted_count"]) != 1 || !boolValue(object["simulator_single_booted"]) {
        reason = "latest iphonesimulator runtime-validation report \(report.path) does not prove exactly one booted required simulator"
    } else if !runtimeArtifactContains(object, suffix: "tcti-first-syscall.txt", marker: "Orlix TCTI: svc #0") {
        reason = "latest iphonesimulator runtime-validation report \(report.path) did not include the first TCTI svc marker artifact"
    } else {
        reason = "latest iphonesimulator runtime-validation report \(report.path) is not a valid non-preflight TCTI pass"
    }
    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        proofTier: gate.proofTier,
        acceptanceWeight: gate.acceptanceWeight,
        realStackRequired: gate.realStackRequired,
        canClaimRuntimeReadiness: gate.canClaimRuntimeReadiness,
        state: runtimeGateState(report: report, passed: reportOK),
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
    guard let latest = latestRuntimeReport(gate: "tcti-simulator-stability", destination: "iphonesimulator") else {
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
    let report = latest.0.withExecutionFreshness(executionFreshness(for: gate, reportGitSHA: latest.0.gitSHA))
    let object = latest.1

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
        reportExecutionFresh(report) &&
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
        runtimeReportFatalFree(object)
    let reason: String
    if reportOK {
        reason = "iphonesimulator runtime-validation report \(report.path) passed on \(requiredSimulatorName) with no fatal simulator TCTI runtime errors"
    } else if !reportExecutionFresh(report) {
        reason = "latest iphonesimulator stability report \(report.path) is execution-stale for current HEAD; \(reportFreshnessReason(report))"
    } else if stringValue(object["selected_device_id"]) != requiredSimulatorID ||
        stringValue(object["selected_device_name"]) != requiredSimulatorName {
        reason = "latest iphonesimulator stability report \(report.path) did not run on required simulator \(requiredSimulatorName) (\(requiredSimulatorID))"
    } else if intValue(object["simulator_booted_count"]) != 1 || !boolValue(object["simulator_single_booted"]) {
        reason = "latest iphonesimulator stability report \(report.path) does not prove exactly one booted required simulator"
    } else if !signaledProcessClear {
        reason = "latest iphonesimulator stability report \(report.path) captured a guest process signal"
    } else if !runtimeReportFatalFree(object) {
        reason = "latest iphonesimulator stability report \(report.path) captured fatal simulator runtime evidence"
    } else {
        reason = "latest iphonesimulator stability report \(report.path) is not a valid non-preflight TCTI pass"
    }
    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        proofTier: gate.proofTier,
        acceptanceWeight: gate.acceptanceWeight,
        realStackRequired: gate.realStackRequired,
        canClaimRuntimeReadiness: gate.canClaimRuntimeReadiness,
        state: runtimeGateState(report: report, passed: reportOK),
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
    guard let latest = latestRuntimeReport(gate: "tcti-init-console-write", destination: "iphonesimulator") else {
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
    let report = latest.0.withExecutionFreshness(executionFreshness(for: gate, reportGitSHA: latest.0.gitSHA))
    let object = latest.1

    let forbidden = object["forbidden_behavior"] as? [String: Any] ?? [:]
    let forbiddenClear = [
        "generated_exec_memory",
        "host_exec_guest_text",
        "host_x18",
        "map_jit",
        "native_ios_api_exposure_to_guest",
        "rwx",
    ].allSatisfy { !boolValue(forbidden[$0]) }
    let consoleArtifactHasMarker = runtimeArtifactContains(
        object,
        suffix: "tcti-console-write.txt",
        marker: "ORLIX-TCTI-CONSOLE-OK"
    )
    let reportOK = report.status == "pass" &&
        report.passed &&
        reportExecutionFresh(report) &&
        stringValue(object["selected_device_id"]) == requiredSimulatorID &&
        stringValue(object["selected_device_name"]) == requiredSimulatorName &&
        intValue(object["simulator_booted_count"]) == 1 &&
        boolValue(object["simulator_single_booted"]) &&
        stringValue(object["backend"]) == "tcti" &&
        stringValue(object["profile"]) == "tcti_runtime" &&
        !boolValue(object["preflight_only"]) &&
        !boolValue(object["autonomous_tests_bypassed"]) &&
        forbiddenClear &&
        consoleArtifactHasMarker &&
        runtimeReportFatalFree(object)
    let reason: String
    if reportOK {
        reason = "iphonesimulator runtime-validation report \(report.path) passed on \(requiredSimulatorName) with Linux console usability marker artifact"
    } else if !reportExecutionFresh(report) {
        reason = "latest iphonesimulator console usability report \(report.path) is execution-stale for current HEAD; \(reportFreshnessReason(report))"
    } else if stringValue(object["selected_device_id"]) != requiredSimulatorID ||
        stringValue(object["selected_device_name"]) != requiredSimulatorName {
        reason = "latest iphonesimulator console usability report \(report.path) did not run on required simulator \(requiredSimulatorName) (\(requiredSimulatorID))"
    } else if intValue(object["simulator_booted_count"]) != 1 || !boolValue(object["simulator_single_booted"]) {
        reason = "latest iphonesimulator console usability report \(report.path) does not prove exactly one booted required simulator"
    } else if !consoleArtifactHasMarker {
        reason = "latest iphonesimulator console usability report \(report.path) is missing ORLIX-TCTI-CONSOLE-OK in tcti-console-write marker artifact"
    } else if !runtimeReportFatalFree(object) {
        reason = "latest iphonesimulator console usability report \(report.path) captured fatal simulator runtime evidence"
    } else {
        reason = "latest iphonesimulator console usability report \(report.path) is missing required pinned simulator, TCTI profile, or forbidden-behavior fields"
    }
    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        state: runtimeGateState(report: report, passed: reportOK),
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
    guard let latest = latestRuntimeReport(gate: "tcti-static-busybox-start", destination: "iphonesimulator") else {
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
    let report = latest.0.withExecutionFreshness(executionFreshness(for: gate, reportGitSHA: latest.0.gitSHA))
    let object = latest.1

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
        reportExecutionFresh(report) &&
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
        busyBoxArtifactHasContent &&
        runtimeReportFatalFree(object)
    let reason: String
    if reportOK {
        reason = "iphonesimulator runtime-validation report \(report.path) passed static BusyBox start gate on \(requiredSimulatorName)"
    } else if !reportExecutionFresh(report) {
        reason = "latest iphonesimulator static BusyBox start report \(report.path) is execution-stale for current HEAD; \(reportFreshnessReason(report))"
    } else if stringValue(object["selected_device_id"]) != requiredSimulatorID ||
        stringValue(object["selected_device_name"]) != requiredSimulatorName {
        reason = "latest iphonesimulator static BusyBox start report \(report.path) did not run on required simulator \(requiredSimulatorName) (\(requiredSimulatorID))"
    } else if intValue(object["simulator_booted_count"]) != 1 || !boolValue(object["simulator_single_booted"]) {
        reason = "latest iphonesimulator static BusyBox start report \(report.path) does not prove exactly one booted required simulator"
    } else if !signaledProcessClear {
        reason = "latest iphonesimulator static BusyBox start report \(report.path) recorded a signaled process"
    } else if !busyBoxArtifactHasContent {
        reason = "latest iphonesimulator static BusyBox start report \(report.path) does not include a non-empty tcti-static-busybox-start marker artifact"
    } else if !runtimeReportFatalFree(object) {
        reason = "latest iphonesimulator static BusyBox start report \(report.path) captured fatal simulator runtime evidence"
    } else {
        reason = "latest iphonesimulator static BusyBox start report \(report.path) is not a valid non-preflight TCTI pass"
    }
    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        state: runtimeGateState(report: report, passed: reportOK),
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

func simulatorStaticBusyBoxShellCommandPass(_ gate: Gate) -> GateStatus {
    guard let latest = latestRuntimeReport(gate: "tcti-static-busybox-shell-command", destination: "iphonesimulator") else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "missing",
            passed: false,
            reason: "missing iphonesimulator runtime-validation report for tcti-static-busybox-shell-command",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: [],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }
    let report = latest.0.withExecutionFreshness(executionFreshness(for: gate, reportGitSHA: latest.0.gitSHA))
    let object = latest.1

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
    let markerArtifactPath = artifacts.first { $0.hasSuffix("tcti-static-busybox-shell-command.txt") }
    let markerArtifactHasContent = markerArtifactPath.flatMap { artifact -> Bool? in
        let directURL = root.appendingPathComponent(artifact)
        let runtimeRelativeURL = root
            .appendingPathComponent("Build/Reports/runtime", isDirectory: true)
            .appendingPathComponent(artifact)
        let url = fileManager.fileExists(atPath: directURL.path) ? directURL : runtimeRelativeURL
        guard let text = try? String(contentsOf: url, encoding: .utf8) else {
            return false
        }
        return text.contains("ORLIX-TCTI-BUSYBOX-USABLE")
    } ?? false
    let reportOK = report.status == "pass" &&
        report.passed &&
        reportExecutionFresh(report) &&
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
        markerArtifactHasContent &&
        runtimeReportFatalFree(object)
    let reason: String
    if reportOK {
        reason = "iphonesimulator runtime-validation report \(report.path) passed static BusyBox shell command gate on \(requiredSimulatorName)"
    } else if !reportExecutionFresh(report) {
        reason = "latest iphonesimulator static BusyBox shell command report \(report.path) is execution-stale for current HEAD; \(reportFreshnessReason(report))"
    } else if stringValue(object["selected_device_id"]) != requiredSimulatorID ||
        stringValue(object["selected_device_name"]) != requiredSimulatorName {
        reason = "latest iphonesimulator static BusyBox shell command report \(report.path) did not run on required simulator \(requiredSimulatorName) (\(requiredSimulatorID))"
    } else if intValue(object["simulator_booted_count"]) != 1 || !boolValue(object["simulator_single_booted"]) {
        reason = "latest iphonesimulator static BusyBox shell command report \(report.path) does not prove exactly one booted required simulator"
    } else if !signaledProcessClear {
        reason = "latest iphonesimulator static BusyBox shell command report \(report.path) recorded a signaled process"
    } else if !markerArtifactHasContent {
        reason = "latest iphonesimulator static BusyBox shell command report \(report.path) does not include the BusyBox command marker"
    } else if !runtimeReportFatalFree(object) {
        reason = "latest iphonesimulator static BusyBox shell command report \(report.path) captured fatal simulator runtime evidence"
    } else {
        reason = "latest iphonesimulator static BusyBox shell command report \(report.path) is not a valid non-preflight TCTI pass"
    }
    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        state: runtimeGateState(report: report, passed: reportOK),
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

func simulatorRuntimeMarkerPass(_ gate: Gate, runtimeGate: String, marker: String, artifactSuffix: String) -> GateStatus {
    guard let latest = latestRuntimeReport(gate: runtimeGate, destination: "iphonesimulator") else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "missing",
            passed: false,
            reason: "missing iphonesimulator runtime-validation report for \(runtimeGate)",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: [],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }
    let report = latest.0.withExecutionFreshness(executionFreshness(for: gate, reportGitSHA: latest.0.gitSHA))
    let object = latest.1

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
    let markerArtifactPath = artifacts.first { $0.hasSuffix(artifactSuffix) }
    let markerArtifactHasContent = markerArtifactPath.flatMap { artifact -> Bool? in
        let directURL = root.appendingPathComponent(artifact)
        let runtimeRelativeURL = root
            .appendingPathComponent("Build/Reports/runtime", isDirectory: true)
            .appendingPathComponent(artifact)
        let url = fileManager.fileExists(atPath: directURL.path) ? directURL : runtimeRelativeURL
        guard let text = try? String(contentsOf: url, encoding: .utf8) else {
            return false
        }
        return text.contains(marker)
    } ?? false
    let reportOK = report.status == "pass" &&
        report.passed &&
        reportExecutionFresh(report) &&
        stringValue(object["selected_device_id"]) == requiredSimulatorID &&
        stringValue(object["selected_device_name"]) == requiredSimulatorName &&
        intValue(object["simulator_booted_count"]) == 1 &&
        boolValue(object["simulator_single_booted"]) &&
        stringValue(object["backend"]) == "tcti" &&
        stringValue(object["profile"]) == "tcti_runtime" &&
        !boolValue(object["preflight_only"]) &&
        !boolValue(object["autonomous_tests_bypassed"]) &&
        forbiddenClear &&
        runtimeReportFatalFree(object) &&
        signaledProcessClear &&
        markerArtifactHasContent
    let reason: String
    if reportOK {
        reason = "iphonesimulator runtime-validation report \(report.path) passed \(runtimeGate) on \(requiredSimulatorName)"
    } else if !reportExecutionFresh(report) {
        reason = "latest iphonesimulator \(runtimeGate) report \(report.path) is execution-stale for current HEAD; \(reportFreshnessReason(report))"
    } else if stringValue(object["selected_device_id"]) != requiredSimulatorID ||
        stringValue(object["selected_device_name"]) != requiredSimulatorName {
        reason = "latest iphonesimulator \(runtimeGate) report \(report.path) did not run on required simulator \(requiredSimulatorName) (\(requiredSimulatorID))"
    } else if intValue(object["simulator_booted_count"]) != 1 || !boolValue(object["simulator_single_booted"]) {
        reason = "latest iphonesimulator \(runtimeGate) report \(report.path) does not prove exactly one booted required simulator"
    } else if !runtimeReportFatalFree(object) {
        reason = "latest iphonesimulator \(runtimeGate) report \(report.path) includes fatal simulator runtime evidence"
    } else if !signaledProcessClear {
        reason = "latest iphonesimulator \(runtimeGate) report \(report.path) recorded a signaled process"
    } else if !markerArtifactHasContent {
        reason = "latest iphonesimulator \(runtimeGate) report \(report.path) does not include marker \(marker)"
    } else {
        reason = "latest iphonesimulator \(runtimeGate) report \(report.path) is not a valid non-preflight TCTI pass"
    }
    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        proofTier: gate.proofTier,
        acceptanceWeight: gate.acceptanceWeight,
        realStackRequired: gate.realStackRequired,
        canClaimRuntimeReadiness: gate.canClaimRuntimeReadiness,
        state: runtimeGateState(report: report, passed: reportOK),
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
	let reducerObject = try? loadJSONObject(root.appendingPathComponent(reducerReport.path))
	let reducerArtifacts = (reducerObject?["artifacts"] as? [Any] ?? []).compactMap { stringValue($0) }
	let runtimeGates = [
		"tcti-init-first-syscall",
		"tcti-static-busybox-shell-command",
		"tcti-static-busybox-start",
		"tcti-simulator-stability",
	]
	var matchingReport: ReportFact?
	for runtimeGate in runtimeGates {
		guard let (report, object) = latestRuntimeReport(gate: runtimeGate, destination: "iphonesimulator") else {
			continue
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
		if matchesCurrentSIGABRT {
			matchingReport = report
			break
		}
	}

	if let report = matchingReport {
		let reducerCoversReport = reducerArtifacts.contains(report.path)
		if reducerReport.status == "pass",
		   reducerReport.passed,
		   reducerReport.gitSHA == gitSHA(),
		   reducerCoversReport {
			return GateStatus(
				id: gate.id,
				command: gate.command,
				kind: gate.kind,
				state: "pass",
				passed: true,
				reason: "post-BusyBox SIGABRT reducer report \(reducerReport.path) passed for latest failure \(report.path)",
				prerequisites: gate.prerequisites,
				prerequisitesSatisfied: false,
				reportPaths: gate.expectedReportPaths,
				reports: [report, reducerReport],
				readinessEligible: gate.readinessEligible,
				physicalDevice: gate.physicalDevice,
				gadget: gate.gadget
			)
		}
		return GateStatus(
			id: gate.id,
			command: gate.command,
			kind: gate.kind,
			state: "ready",
			passed: false,
			reason: "current simulator report \(report.path) records sh SIGABRT signal=6 and needs a reducer for that exact report",
			prerequisites: gate.prerequisites,
			prerequisitesSatisfied: false,
			reportPaths: gate.expectedReportPaths,
			reports: reducerReport.exists ? [report, reducerReport] : [report],
			readinessEligible: gate.readinessEligible,
			physicalDevice: gate.physicalDevice,
			gadget: gate.gadget
		)
	}

	guard latestRuntimeReport(gate: "tcti-static-busybox-start", destination: "iphonesimulator") != nil ||
		latestRuntimeReport(gate: "tcti-static-busybox-shell-command", destination: "iphonesimulator") != nil ||
		latestRuntimeReport(gate: "tcti-simulator-stability", destination: "iphonesimulator") != nil else {
		return GateStatus(
			id: gate.id,
			command: gate.command,
			kind: gate.kind,
            state: "not_needed",
            passed: false,
        satisfiesPrerequisite: true,
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

	return GateStatus(
		id: gate.id,
		command: gate.command,
		kind: gate.kind,
		state: "not_needed",
		passed: false,
        satisfiesPrerequisite: true,
		reason: "latest BusyBox simulator reports do not match the sh SIGABRT reducer signature",
		prerequisites: gate.prerequisites,
		prerequisitesSatisfied: false,
		reportPaths: gate.expectedReportPaths,
		reports: reducerReport.exists ? [reducerReport] : [],
		readinessEligible: gate.readinessEligible,
		physicalDevice: gate.physicalDevice,
		gadget: gate.gadget
    )
}

func initReadFaultReducerPass(_ gate: Gate) -> GateStatus {
    let reducerReport = reportFact(target: "tcti-init-read-fault-reducer")
    if reducerReport.status == "pass",
       reducerReport.passed,
       reducerReport.gitSHA == gitSHA() {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "pass",
            passed: true,
            reason: "init read-fault reducer report \(reducerReport.path) passed",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: [reducerReport],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    guard let (report, object) = latestRuntimeReport(gate: "tcti-simulator-stability", destination: "iphonesimulator") else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "not_needed",
            passed: false,
        satisfiesPrerequisite: true,
            reason: "no simulator stability report exists for init read-fault reducer",
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
    let firstSVC = events["first_svc"] as? [String: Any] ?? [:]
    let fault = events["fatal_user_fault"] as? [String: Any] ?? [:]
    let staticPIE = events["static_pie_image"] as? [String: Any] ?? [:]
    let signal = events["signaled_process"] as? [String: Any] ?? [:]
    let matchesCurrentFault = report.status == "fail" &&
        !report.passed &&
        report.gitSHA == gitSHA() &&
        stringValue(firstSVC["task"]) == "init" &&
        intValue(firstSVC["pid"]) == 1 &&
        intValue(firstSVC["syscall"]) == 178 &&
        stringValue(fault["task"]) == "init" &&
        intValue(fault["pid"]) == 1 &&
        intValue(fault["access"]) == 1 &&
        intValue(fault["si"]) == 1 &&
        !(stringValue(fault["addr"]) ?? "").isEmpty &&
        stringValue(fault["addr"]) != "0x0" &&
        (stringValue(staticPIE["task"]) ?? "").isEmpty &&
        intValue(staticPIE["pid"]) == nil &&
        intValue(signal["signal"]) == nil

    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        state: matchesCurrentFault ? "ready" : "not_needed",
        passed: !matchesCurrentFault,
        reason: matchesCurrentFault
            ? "current simulator stability report \(report.path) records init first-svc syscall 178 followed by non-null init read fault before static PIE or shell progress"
            : "latest simulator stability report does not match the init read-fault reducer signature",
        prerequisites: gate.prerequisites,
        prerequisitesSatisfied: false,
        reportPaths: gate.expectedReportPaths,
        reports: [report, reducerReport],
        readinessEligible: gate.readinessEligible,
        physicalDevice: gate.physicalDevice,
        gadget: gate.gadget
    )
}

func postStaticPIEInitReadFaultReport() -> (ReportFact, [String: Any])? {
    if let (report, object) = latestRuntimeReport(gate: "tcti-full-shell-usability", destination: "iphonesimulator") {
        let events = object["tcti_runtime_events"] as? [String: Any] ?? [:]
        let firstSVC = events["first_svc"] as? [String: Any] ?? [:]
        let staticPIE = events["static_pie_image"] as? [String: Any] ?? [:]
        let fault = events["fatal_user_fault"] as? [String: Any] ?? [:]
        let signal = events["signaled_process"] as? [String: Any] ?? [:]
        let matchesCurrentFault = report.status == "fail" &&
            !report.passed &&
            report.gitSHA == gitSHA() &&
            stringValue(firstSVC["task"]) == "init" &&
            intValue(firstSVC["pid"]) == 1 &&
            intValue(firstSVC["syscall"]) == 178 &&
            stringValue(staticPIE["task"]) == "init" &&
            intValue(staticPIE["pid"]) == 1 &&
            !(stringValue(staticPIE["base"]) ?? "").isEmpty &&
            !(stringValue(staticPIE["entry"]) ?? "").isEmpty &&
            stringValue(fault["task"]) == "init" &&
            intValue(fault["pid"]) == 1 &&
            intValue(fault["access"]) == 1 &&
            intValue(fault["si"]) == 1 &&
            !(stringValue(fault["addr"]) ?? "").isEmpty &&
            stringValue(fault["addr"]) != "0x0" &&
            intValue(signal["signal"]) == nil
        return matchesCurrentFault ? (report, object) : nil
    }

    for runtimeGate in [
		"tcti-static-busybox-shell-command",
		"tcti-init-console-write",
		"tcti-simulator-stability",
		"tcti-init-first-syscall",
    ] {
        guard let (report, object) = latestRuntimeReport(gate: runtimeGate, destination: "iphonesimulator") else {
            continue
        }
        let events = object["tcti_runtime_events"] as? [String: Any] ?? [:]
        let firstSVC = events["first_svc"] as? [String: Any] ?? [:]
        let staticPIE = events["static_pie_image"] as? [String: Any] ?? [:]
        let fault = events["fatal_user_fault"] as? [String: Any] ?? [:]
        let signal = events["signaled_process"] as? [String: Any] ?? [:]
        let matchesCurrentFault = report.status == "fail" &&
            !report.passed &&
            report.gitSHA == gitSHA() &&
            stringValue(firstSVC["task"]) == "init" &&
            intValue(firstSVC["pid"]) == 1 &&
            intValue(firstSVC["syscall"]) == 178 &&
            stringValue(staticPIE["task"]) == "init" &&
            intValue(staticPIE["pid"]) == 1 &&
            !(stringValue(staticPIE["base"]) ?? "").isEmpty &&
            !(stringValue(staticPIE["entry"]) ?? "").isEmpty &&
            stringValue(fault["task"]) == "init" &&
            intValue(fault["pid"]) == 1 &&
            intValue(fault["access"]) == 1 &&
            intValue(fault["si"]) == 1 &&
            !(stringValue(fault["addr"]) ?? "").isEmpty &&
            stringValue(fault["addr"]) != "0x0" &&
            intValue(signal["signal"]) == nil
        if matchesCurrentFault {
            return (report, object)
        }
    }
    return nil
}

func postStaticPIEInitReadFaultReducerPass(_ gate: Gate) -> GateStatus {
    let reducerReport = reportFact(target: "tcti-post-static-pie-init-read-fault-reducer")
    let reducerObject = try? loadJSONObject(root.appendingPathComponent(reducerReport.path))
    let reducerArtifacts = (reducerObject?["artifacts"] as? [Any] ?? []).compactMap { stringValue($0) }

    guard let (report, _) = postStaticPIEInitReadFaultReport() else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "not_needed",
            passed: false,
        satisfiesPrerequisite: true,
            reason: "latest pinned simulator reports do not match the post-static-PIE init read-fault reducer signature",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: reducerReport.exists ? [reducerReport] : [],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    let reducerCoversReport = reducerArtifacts.contains(report.path)
    if reducerReport.status == "pass",
       reducerReport.passed,
       reducerReport.gitSHA == gitSHA(),
       reducerCoversReport {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "pass",
            passed: true,
            reason: "post-static-PIE init read-fault reducer \(reducerReport.path) covers current simulator report \(report.path)",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: [report, reducerReport],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    return GateStatus(
        id: gate.id,
        command: "TCTI_SIMULATOR_REPORT=\(report.path) \(gate.command)",
        kind: gate.kind,
        state: "ready",
        passed: false,
        reason: "current pinned simulator report \(report.path) records init first-svc syscall 178, static PIE image, then non-null init read fault before full shell usability; reduce it before patching runtime behavior",
        prerequisites: gate.prerequisites,
        prerequisitesSatisfied: false,
        reportPaths: gate.expectedReportPaths,
        reports: reducerReport.exists ? [report, reducerReport] : [report],
        readinessEligible: gate.readinessEligible,
        physicalDevice: gate.physicalDevice,
        gadget: gate.gadget
    )
}

func postStaticPIEInitTLSFixPass(_ gate: Gate) -> GateStatus {
    let fixReport = reportFact(target: "tcti-post-static-pie-init-tls-fix")
    let fixObject = try? loadJSONObject(root.appendingPathComponent(fixReport.path))
    let fixArtifacts = (fixObject?["artifacts"] as? [Any] ?? []).compactMap { stringValue($0) }

    guard let (report, _) = postStaticPIEInitReadFaultReport() else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "not_needed",
            passed: false,
        satisfiesPrerequisite: true,
            reason: "latest pinned simulator reports do not require the post-static-PIE init TLS/read-fault fix gate",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: fixReport.exists ? [fixReport] : [],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    if fixReport.status == "pass",
       fixReport.passed,
       fixReport.gitSHA == gitSHA(),
       fixArtifacts.contains(report.path) {
        return basicReportGate(gate, target: "tcti-post-static-pie-init-tls-fix")
    }

    let reducerStatus = postStaticPIEInitReadFaultReducerPass(gate)
    guard reducerStatus.passed else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "missing",
            passed: false,
            reason: "post-static-PIE init TLS/read-fault fix requires a passing reducer for current report \(report.path)",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: reducerStatus.reports + (fixReport.exists ? [fixReport] : []),
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        state: "ready",
        passed: false,
        reason: "current simulator report \(report.path) is reduced; implement the scoped post-static-PIE init guest TLS/read-fault fix before rerunning full-shell usability",
        prerequisites: gate.prerequisites,
        prerequisitesSatisfied: false,
        reportPaths: gate.expectedReportPaths,
        reports: reducerStatus.reports + (fixReport.exists ? [fixReport] : []),
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
    let fixReport = reportFact(target: "tcti-static-pie-relocation-fix", gate: gate)

    if fixReport.status == "pass", fixReport.passed, reportExecutionFresh(fixReport) {
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
            reports: [fixReport, stabilityReport],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }
    if fixReport.status == "pass", fixReport.passed, fixReport.exists {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "stale",
            passed: false,
            reason: "static PIE relocation production fix report is not current for HEAD: \(reportFreshnessReason(fixReport))",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: true,
            reportPaths: gate.expectedReportPaths,
            reports: [fixReport, stabilityReport],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    if stabilityFailedAtHead && !fatalMatchesReducer {
        return artifactStatus(gate, passed: true, reason: "current simulator stability failure does not match the reduced static PIE GOT null-read signature")
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

func postFullShellCatReadFaultReport() -> (ReportFact, [String: Any])? {
    guard let (report, object) = latestRuntimeReport(gate: "tcti-full-shell-usability", destination: "iphonesimulator") else {
        return nil
    }
    let events = object["tcti_runtime_events"] as? [String: Any] ?? [:]
    let firstSVC = events["first_svc"] as? [String: Any] ?? [:]
    let fault = events["fatal_user_fault"] as? [String: Any] ?? [:]
    let signal = events["signaled_process"] as? [String: Any] ?? [:]
    let artifacts = (object["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
    let text = artifacts.compactMap { artifact -> String? in
        let url = root.appendingPathComponent("Build/Reports/runtime").appendingPathComponent(artifact)
        if let value = try? String(contentsOf: url, encoding: .utf8) {
            return value
        }
        return try? String(contentsOf: root.appendingPathComponent(artifact), encoding: .utf8)
    }.joined(separator: "\n")
    let fullShellText = artifacts.first { $0.hasSuffix("tcti-full-shell-usability.txt") }.flatMap { artifact -> String? in
        let url = root.appendingPathComponent("Build/Reports/runtime").appendingPathComponent(artifact)
        if let value = try? String(contentsOf: url, encoding: .utf8) {
            return value
        }
        return try? String(contentsOf: root.appendingPathComponent(artifact), encoding: .utf8)
    } ?? ""
    let matchesCurrentFault = report.status == "fail" &&
        !report.passed &&
        report.gitSHA == gitSHA() &&
        stringValue(firstSVC["task"]) == "init" &&
        intValue(firstSVC["pid"]) == 1 &&
        intValue(firstSVC["syscall"]) == 178 &&
        stringValue(fault["task"]) == "cat" &&
        intValue(fault["access"]) == 1 &&
        intValue(fault["si"]) == 1 &&
        !(stringValue(fault["addr"]) ?? "").isEmpty &&
        stringValue(fault["addr"]) != "0x0" &&
        intValue(signal["signal"]) == nil &&
        text.contains("Orlix TCTI: static PIE image task=cat") &&
        text.contains("Orlix TCTI: user fault task=cat") &&
        (text.contains("orlix-init: process exited pid=33 status=139") ||
         text.contains("orlix-init: process exited pid=32 status=139") ||
         text.contains("orlix-init: pid=32 status=139") ||
         text.contains("orlix-init: pid=33 status=139")) &&
        !fullShellText.contains("ORLIX-TCTI-SHELL-USABLE")
    return matchesCurrentFault ? (report, object) : nil
}

func postFullShellCatReadFaultReducerPass(_ gate: Gate) -> GateStatus {
    let reducerReport = reportFact(target: "tcti-post-full-shell-cat-read-fault-reducer")
    let reducerObject = try? loadJSONObject(root.appendingPathComponent(reducerReport.path))
    let reducerArtifacts = (reducerObject?["artifacts"] as? [Any] ?? []).compactMap { stringValue($0) }

    guard let (report, _) = postFullShellCatReadFaultReport() else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "not_needed",
            passed: false,
        satisfiesPrerequisite: true,
            reason: "latest pinned simulator full-shell report does not match the cat read-fault reducer signature",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: reducerReport.exists ? [reducerReport] : [],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    let reducerCoversReport = reducerArtifacts.contains(report.path)
    if reducerReport.status == "pass",
       reducerReport.passed,
       reducerReport.gitSHA == gitSHA(),
       reducerCoversReport {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "pass",
            passed: true,
            reason: "cat read-fault reducer \(reducerReport.path) covers current simulator report \(report.path)",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: [report, reducerReport],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        state: "ready",
        passed: false,
        reason: "current pinned simulator full-shell report \(report.path) reaches cat then faults on a user-data read; reduce it before patching runtime behavior",
        prerequisites: gate.prerequisites,
        prerequisitesSatisfied: false,
        reportPaths: gate.expectedReportPaths,
        reports: reducerReport.exists ? [report, reducerReport] : [report],
        readinessEligible: gate.readinessEligible,
        physicalDevice: gate.physicalDevice,
        gadget: gate.gadget
    )
}

func postFullShellCatReadFaultFixPass(_ gate: Gate) -> GateStatus {
    let fixReport = reportFact(target: "tcti-post-full-shell-cat-read-fault-fix")
    let fixObject = try? loadJSONObject(root.appendingPathComponent(fixReport.path))
    let fixArtifacts = (fixObject?["artifacts"] as? [Any] ?? []).compactMap { stringValue($0) }

    guard let (report, _) = postFullShellCatReadFaultReport() else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "not_needed",
            passed: false,
        satisfiesPrerequisite: true,
            reason: "latest pinned simulator full-shell report does not require cat read-fault fix gate",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: fixReport.exists ? [fixReport] : [],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    let reducerGate = Gate(
        id: "no-phone-tcti-post-full-shell-cat-read-fault-reducer",
        command: "make tcti-gate TARGET=tcti-post-full-shell-cat-read-fault-reducer",
        kind: "no-phone-reducer",
        prerequisites: [],
        allowedScope: [],
        forbiddenScope: [],
        expectedReportPaths: [],
        readinessEligible: false,
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [],
        reducerRequirements: [],
        requiredSubagentsOrSkills: [],
        commitMessageTemplate: "",
        stopConditions: []
    )
    let reducerStatus = postFullShellCatReadFaultReducerPass(reducerGate)
    guard reducerStatus.passed else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "missing",
            passed: false,
            reason: "cat read-fault fix requires passing reducer for current report \(report.path)",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: reducerStatus.reports + (fixReport.exists ? [fixReport] : []),
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    if fixReport.status == "pass",
       fixReport.passed,
       fixReport.gitSHA == gitSHA(),
       fixArtifacts.contains(report.path) {
        return basicReportGate(gate, target: "tcti-post-full-shell-cat-read-fault-fix")
    }

    let engineURL = root.appendingPathComponent("OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/engine.c")
    let engineText = (try? String(contentsOf: engineURL, encoding: .utf8)) ?? ""
    let sourceLooksFixed = engineText.contains("case __NR_read:") &&
        engineText.contains("tcti_refresh_current_user_range(regs->regs[1], regs->regs[0])")

    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        state: sourceLooksFixed ? "ready" : "missing",
        passed: false,
        reason: sourceLooksFixed
            ? "cat read-fault source fix is present; run make tcti-gate TARGET=tcti-post-full-shell-cat-read-fault-fix before rerunning simulator full-shell usability"
            : "cat read-fault fix must refresh the guest read buffer after successful read(2)",
        prerequisites: gate.prerequisites,
        prerequisitesSatisfied: false,
        reportPaths: gate.expectedReportPaths,
        reports: [report] + reducerStatus.reports + (fixReport.exists ? [fixReport] : []),
        readinessEligible: gate.readinessEligible,
        physicalDevice: gate.physicalDevice,
        gadget: gate.gadget
    )
}

func postFullShellCatPosixMemalignBRKReport() -> (ReportFact, [String: Any])? {
    guard let (report, object) = latestRuntimeReport(gate: "tcti-full-shell-usability", destination: "iphonesimulator") else {
        return nil
    }
    let events = object["tcti_runtime_events"] as? [String: Any] ?? [:]
    let firstSVC = events["first_svc"] as? [String: Any] ?? [:]
    let signal = events["signaled_process"] as? [String: Any] ?? [:]
    let artifacts = (object["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
    let text = artifacts.compactMap { artifact -> String? in
        let url = root.appendingPathComponent("Build/Reports/runtime").appendingPathComponent(artifact)
        if let value = try? String(contentsOf: url, encoding: .utf8) {
            return value
        }
        return try? String(contentsOf: root.appendingPathComponent(artifact), encoding: .utf8)
    }.joined(separator: "\n")
    let fullShellText = artifacts.first { $0.hasSuffix("tcti-full-shell-usability.txt") }.flatMap { artifact -> String? in
        let url = root.appendingPathComponent("Build/Reports/runtime").appendingPathComponent(artifact)
        if let value = try? String(contentsOf: url, encoding: .utf8) {
            return value
        }
        return try? String(contentsOf: root.appendingPathComponent(artifact), encoding: .utf8)
    } ?? ""
    let matchesCurrentFailure = report.status == "fail" &&
        !report.passed &&
        report.gitSHA == gitSHA() &&
        stringValue(firstSVC["task"]) == "init" &&
        intValue(firstSVC["pid"]) == 1 &&
        intValue(firstSVC["syscall"]) == 178 &&
        intValue(signal["signal"]) == nil &&
        text.contains("Orlix TCTI: static PIE image task=cat") &&
        text.contains("In function posix_memalign") &&
        text.contains("__ensure(") &&
        text.contains("Orlix TCTI: unsupported instruction task=cat") &&
        text.contains("insn=0xd4200020") &&
        text.contains("orlix-init: process exited pid=32 status=132") &&
        !fullShellText.contains("ORLIX-TCTI-SHELL-USABLE")
    return matchesCurrentFailure ? (report, object) : nil
}

func postFullShellCatPosixMemalignBRKReducerPass(_ gate: Gate) -> GateStatus {
    let reducerReport = reportFact(target: "tcti-post-full-shell-cat-posix-memalign-brk-reducer")
    let reducerObject = try? loadJSONObject(root.appendingPathComponent(reducerReport.path))
    let reducerArtifacts = (reducerObject?["artifacts"] as? [Any] ?? []).compactMap { stringValue($0) }

    guard let (report, _) = postFullShellCatPosixMemalignBRKReport() else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "not_needed",
            passed: false,
        satisfiesPrerequisite: true,
            reason: "latest pinned simulator full-shell report does not match the cat posix_memalign BRK reducer signature",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: reducerReport.exists ? [reducerReport] : [],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    let reducerCoversReport = reducerArtifacts.contains(report.path)
    if reducerReport.status == "pass",
       reducerReport.passed,
       reducerReport.gitSHA == gitSHA(),
       reducerCoversReport {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "pass",
            passed: true,
            reason: "cat posix_memalign BRK reducer \(reducerReport.path) covers current simulator report \(report.path)",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: [report, reducerReport],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        state: "ready",
        passed: false,
        reason: "current pinned simulator full-shell report \(report.path) reaches cat, hits mlibc posix_memalign alignment assertion, then stops on BRK 0xd4200020; reduce it before patching runtime behavior",
        prerequisites: gate.prerequisites,
        prerequisitesSatisfied: false,
        reportPaths: gate.expectedReportPaths,
        reports: reducerReport.exists ? [report, reducerReport] : [report],
        readinessEligible: gate.readinessEligible,
        physicalDevice: gate.physicalDevice,
        gadget: gate.gadget
    )
}

func postFullShellCatPosixMemalignBRKFixPass(_ gate: Gate) -> GateStatus {
    let fixReport = reportFact(target: "tcti-post-full-shell-cat-posix-memalign-brk-fix")
    let fixObject = try? loadJSONObject(root.appendingPathComponent(fixReport.path))
    let fixArtifacts = (fixObject?["artifacts"] as? [Any] ?? []).compactMap { stringValue($0) }

    guard let (report, _) = postFullShellCatPosixMemalignBRKReport() else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "not_needed",
            passed: false,
        satisfiesPrerequisite: true,
            reason: "latest pinned simulator full-shell report does not require cat posix_memalign BRK fix gate",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: fixReport.exists ? [fixReport] : [],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    let reducerGate = Gate(
        id: "no-phone-tcti-post-full-shell-cat-posix-memalign-brk-reducer",
        command: "make tcti-gate TARGET=tcti-post-full-shell-cat-posix-memalign-brk-reducer",
        kind: "no-phone-reducer",
        prerequisites: [],
        allowedScope: [],
        forbiddenScope: [],
        expectedReportPaths: [],
        readinessEligible: false,
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [],
        reducerRequirements: [],
        requiredSubagentsOrSkills: [],
        commitMessageTemplate: "",
        stopConditions: []
    )
    let reducerStatus = postFullShellCatPosixMemalignBRKReducerPass(reducerGate)
    guard reducerStatus.passed else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "missing",
            passed: false,
            reason: "cat posix_memalign BRK fix requires passing reducer for current report \(report.path)",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: reducerStatus.reports + (fixReport.exists ? [fixReport] : []),
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    if fixReport.status == "pass",
       fixReport.passed,
       fixReport.gitSHA == gitSHA(),
       fixArtifacts.contains(report.path) {
        return basicReportGate(gate, target: "tcti-post-full-shell-cat-posix-memalign-brk-fix")
    }

    let mmapURL = root.appendingPathComponent("OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/mm/mmap.c")
    let mmapText = (try? String(contentsOf: mmapURL, encoding: .utf8)) ?? ""
    let sourceLooksFixed = mmapText.contains("arch_boot_host_page_size()") &&
        mmapText.contains("orlix_hosted_prot_none_reservation") &&
        mmapText.contains("orlix_hosted_mmap_align_mask") &&
        mmapText.contains("orlix_hosted_mmap_align_offset") &&
        mmapText.contains("info.align_mask = orlix_hosted_mmap_align_mask(file,") &&
        mmapText.contains("info.align_offset = orlix_hosted_mmap_align_offset(file,") &&
        mmapText.contains("result = vm_unmapped_area(&info)") &&
        mmapText.contains("offset_in_page(result)")

    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        state: sourceLooksFixed ? "ready" : "missing",
        passed: false,
        reason: sourceLooksFixed ? "source contains hosted mmap host-page alignment fix for \(report.path), but fix gate report \(fixReport.path) is not current passing" : "source does not yet preserve hosted mmap host-page alignment for \(report.path)",
        prerequisites: gate.prerequisites,
        prerequisitesSatisfied: true,
        reportPaths: gate.expectedReportPaths,
        reports: reducerStatus.reports + (fixReport.exists ? [fixReport] : []),
        readinessEligible: gate.readinessEligible,
        physicalDevice: gate.physicalDevice,
        gadget: gate.gadget
    )
}

func postFullShellInitWriteFaultReport() -> (ReportFact, [String: Any])? {
    guard let (report, object) = latestRuntimeReport(gate: "tcti-full-shell-usability", destination: "iphonesimulator") else {
        return nil
    }
    let events = object["tcti_runtime_events"] as? [String: Any] ?? [:]
    let firstSVC = events["first_svc"] as? [String: Any] ?? [:]
    let staticPIE = events["static_pie_image"] as? [String: Any] ?? [:]
    let mmap = events["last_mmap_syscall"] as? [String: Any] ?? [:]
    let fault = events["fatal_user_fault"] as? [String: Any] ?? [:]
    let signal = events["signaled_process"] as? [String: Any] ?? [:]
    let artifacts = (object["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
    let text = artifacts.compactMap { artifact -> String? in
        let url = root.appendingPathComponent("Build/Reports/runtime").appendingPathComponent(artifact)
        if let value = try? String(contentsOf: url, encoding: .utf8) {
            return value
        }
        return try? String(contentsOf: root.appendingPathComponent(artifact), encoding: .utf8)
    }.joined(separator: "\n")
    let fullShellText = artifacts.first { $0.hasSuffix("tcti-full-shell-usability.txt") }.flatMap { artifact -> String? in
        let url = root.appendingPathComponent("Build/Reports/runtime").appendingPathComponent(artifact)
        if let value = try? String(contentsOf: url, encoding: .utf8) {
            return value
        }
        return try? String(contentsOf: root.appendingPathComponent(artifact), encoding: .utf8)
    } ?? ""
    let matchesCurrentFault = report.status == "fail" &&
        !report.passed &&
        report.gitSHA == gitSHA() &&
        stringValue(firstSVC["task"]) == "init" &&
        intValue(firstSVC["pid"]) == 1 &&
        intValue(firstSVC["syscall"]) == 178 &&
        stringValue(staticPIE["task"]) == "init" &&
        intValue(staticPIE["pid"]) == 1 &&
        !(stringValue(staticPIE["base"]) ?? "").isEmpty &&
        !(stringValue(staticPIE["entry"]) ?? "").isEmpty &&
        stringValue(mmap["task"]) == "init" &&
        intValue(mmap["pid"]) == 1 &&
        intValue(mmap["syscall"]) == 222 &&
        stringValue(fault["task"]) == "init" &&
        intValue(fault["pid"]) == 1 &&
        intValue(fault["access"]) == 2 &&
        intValue(fault["si"]) == 2 &&
        !(stringValue(fault["pc"]) ?? "").isEmpty &&
        !(stringValue(fault["lr"]) ?? "").isEmpty &&
        !(stringValue(fault["sp"]) ?? "").isEmpty &&
        !(stringValue(fault["addr"]) ?? "").isEmpty &&
        stringValue(fault["addr"]) != "0x0" &&
        intValue(signal["signal"]) == nil &&
        text.contains("Orlix TCTI: static PIE image task=init") &&
        text.contains("Orlix TCTI: user fault task=init") &&
        text.contains("access=2") &&
        text.contains("si=2") &&
        !fullShellText.contains("ORLIX-TCTI-SHELL-USABLE")
    return matchesCurrentFault ? (report, object) : nil
}

func postFullShellInitWriteFaultReducerPass(_ gate: Gate) -> GateStatus {
    let reducerReport = reportFact(target: "tcti-post-full-shell-init-write-fault-reducer")
    let reducerObject = try? loadJSONObject(root.appendingPathComponent(reducerReport.path))
    let reducerArtifacts = (reducerObject?["artifacts"] as? [Any] ?? []).compactMap { stringValue($0) }

    guard let (report, _) = postFullShellInitWriteFaultReport() else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "not_needed",
            passed: false,
        satisfiesPrerequisite: true,
            reason: "latest pinned simulator full-shell report does not match init write-fault reducer signature",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: reducerReport.exists ? [reducerReport] : [],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    let reducerCoversReport = reducerArtifacts.contains(report.path)
    if reducerReport.status == "pass",
       reducerReport.passed,
       reducerReport.gitSHA == gitSHA(),
       reducerCoversReport {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "pass",
            passed: true,
            reason: "init write-fault reducer \(reducerReport.path) covers current simulator report \(report.path)",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: [report, reducerReport],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        state: "ready",
        passed: false,
        reason: "current pinned simulator report \(report.path) records init first-svc syscall 178, static PIE image, mmap syscall 222, then non-null init write fault access=2 si=2 before full shell usability; reduce it before patching runtime behavior",
        prerequisites: gate.prerequisites,
        prerequisitesSatisfied: false,
        reportPaths: gate.expectedReportPaths,
        reports: reducerReport.exists ? [report, reducerReport] : [report],
        readinessEligible: gate.readinessEligible,
        physicalDevice: gate.physicalDevice,
        gadget: gate.gadget
    )
}

func postFullShellSHSIGABRTReport() -> (ReportFact, [String: Any])? {
    guard let (report, object) = latestRuntimeReport(gate: "tcti-full-shell-usability", destination: "iphonesimulator") else {
        return nil
    }
    let events = object["tcti_runtime_events"] as? [String: Any] ?? [:]
    let firstSVC = events["first_svc"] as? [String: Any] ?? [:]
    let staticPIE = events["static_pie_image"] as? [String: Any] ?? [:]
    let mmap = events["last_mmap_syscall"] as? [String: Any] ?? [:]
    let fault = events["fatal_user_fault"] as? [String: Any] ?? [:]
    let signal = events["signaled_process"] as? [String: Any] ?? [:]
    let lastReturn = events["last_sh_syscall_return"] as? [String: Any] ?? [:]
    let shPID = intValue(staticPIE["pid"])
    let shPIDHex = shPID.map { String(format: "0x%x", $0) } ?? ""
    let artifacts = (object["artifacts"] as? [Any] ?? []).compactMap { $0 as? String }
    let text = artifacts.compactMap { artifact -> String? in
        let url = root.appendingPathComponent("Build/Reports/runtime").appendingPathComponent(artifact)
        if let value = try? String(contentsOf: url, encoding: .utf8) {
            return value
        }
        return try? String(contentsOf: root.appendingPathComponent(artifact), encoding: .utf8)
    }.joined(separator: "\n")
    let fullShellText = artifacts.first { $0.hasSuffix("tcti-full-shell-usability.txt") }.flatMap { artifact -> String? in
        let url = root.appendingPathComponent("Build/Reports/runtime").appendingPathComponent(artifact)
        if let value = try? String(contentsOf: url, encoding: .utf8) {
            return value
        }
        return try? String(contentsOf: root.appendingPathComponent(artifact), encoding: .utf8)
    } ?? ""
    let matchesCurrentFailure = report.status == "fail" &&
        !report.passed &&
        report.gitSHA == gitSHA() &&
        stringValue(firstSVC["task"]) == "init" &&
        intValue(firstSVC["pid"]) == 1 &&
        intValue(firstSVC["syscall"]) == 178 &&
        stringValue(staticPIE["task"]) == "sh" &&
        shPID != nil &&
        stringValue(staticPIE["entry"]) == "0x45418" &&
        !(stringValue(staticPIE["base"]) ?? "").isEmpty &&
        stringValue(mmap["task"]) == "sh" &&
        intValue(mmap["pid"]) == shPID &&
        intValue(mmap["syscall"]) == 222 &&
        intValue(fault["access"]) == nil &&
        intValue(fault["si"]) == nil &&
        (stringValue(fault["addr"]) ?? "").isEmpty &&
        intValue(signal["pid"]) == shPID &&
        intValue(signal["signal"]) == 6 &&
        stringValue(lastReturn["task"]) == "sh" &&
        intValue(lastReturn["pid"]) == shPID &&
        intValue(lastReturn["syscall"]) == 172 &&
        intValue(lastReturn["signed_ret"]) == shPID &&
        text.contains("Orlix TCTI: static PIE image task=sh") &&
        text.contains("Orlix TCTI: syscall return task=sh") &&
        text.contains("syscall=172 ret=\(shPIDHex) signed_ret=\(shPID ?? -1)") &&
        text.contains("syscall=129 x0=\(shPIDHex) x1=0x6") &&
        text.contains("syscall=160") &&
        text.contains("orlix-init: process signaled pid=\(shPID ?? -1) signal=6") &&
        !fullShellText.contains("ORLIX-TCTI-SHELL-USABLE")
    return matchesCurrentFailure ? (report, object) : nil
}

func postFullShellSHSIGABRTReducerPass(_ gate: Gate) -> GateStatus {
    let reducerReport = reportFact(target: "tcti-post-full-shell-sh-sigabrt-reducer")
    let reducerObject = try? loadJSONObject(root.appendingPathComponent(reducerReport.path))
    let reducerArtifacts = (reducerObject?["artifacts"] as? [Any] ?? []).compactMap { stringValue($0) }

    guard let (report, _) = postFullShellSHSIGABRTReport() else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "not_needed",
            passed: false,
        satisfiesPrerequisite: true,
            reason: "latest pinned simulator full-shell report does not match sh SIGABRT reducer signature",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: reducerReport.exists ? [reducerReport] : [],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    let reducerCoversReport = reducerArtifacts.contains(report.path)
    if reducerReport.status == "pass",
       reducerReport.passed,
       reducerReport.gitSHA == gitSHA(),
       reducerCoversReport {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "pass",
            passed: true,
            reason: "sh SIGABRT reducer \(reducerReport.path) covers current simulator report \(report.path)",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: [report, reducerReport],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        state: "ready",
        passed: false,
        reason: "current pinned simulator report \(report.path) reaches sh static PIE and syscall-return progress, then sh receives SIGABRT before full shell usability; reduce it before patching runtime behavior",
        prerequisites: gate.prerequisites,
        prerequisitesSatisfied: false,
        reportPaths: gate.expectedReportPaths,
        reports: reducerReport.exists ? [report, reducerReport] : [report],
        readinessEligible: gate.readinessEligible,
        physicalDevice: gate.physicalDevice,
        gadget: gate.gadget
    )
}

func postFullShellSHReadFaultReport() -> (ReportFact, [String: Any])? {
    guard let (report, object) = latestRuntimeReport(gate: "tcti-full-shell-usability", destination: "iphonesimulator") else {
        return nil
    }
    let events = object["tcti_runtime_events"] as? [String: Any] ?? [:]
    let firstSVC = events["first_svc"] as? [String: Any] ?? [:]
    let staticPIE = events["static_pie_image"] as? [String: Any] ?? [:]
    let mmap = events["last_mmap_syscall"] as? [String: Any] ?? [:]
    let fault = events["fatal_user_fault"] as? [String: Any] ?? [:]
    let signal = events["signaled_process"] as? [String: Any] ?? [:]
    let shPID = intValue(fault["pid"])
    let fullShellText = runtimeArtifactText(object, suffix: "tcti-full-shell-usability.txt") ?? ""
    let matchesCurrentFailure = report.status == "fail" &&
        !report.passed &&
        report.gitSHA == gitSHA() &&
        stringValue(object["selected_device_id"]) == requiredSimulatorID &&
        stringValue(object["selected_device_name"]) == requiredSimulatorName &&
        intValue(object["simulator_booted_count"]) == 1 &&
        boolValue(object["simulator_single_booted"]) &&
        stringValue(firstSVC["task"]) == "init" &&
        intValue(firstSVC["pid"]) == 1 &&
        intValue(firstSVC["syscall"]) == 178 &&
        stringValue(staticPIE["task"]) == "sh" &&
        intValue(staticPIE["pid"]) == shPID &&
        !(stringValue(staticPIE["base"]) ?? "").isEmpty &&
        !(stringValue(staticPIE["entry"]) ?? "").isEmpty &&
        stringValue(mmap["task"]) == "sh" &&
        intValue(mmap["pid"]) == shPID &&
        intValue(mmap["syscall"]) == 222 &&
        stringValue(fault["task"]) == "sh" &&
        shPID != nil &&
        intValue(fault["access"]) == 1 &&
        intValue(fault["si"]) == 1 &&
        !(stringValue(fault["pc"]) ?? "").isEmpty &&
        !(stringValue(fault["addr"]) ?? "").isEmpty &&
        stringValue(fault["addr"]) != "0x0" &&
        intValue(signal["pid"]) == shPID &&
        intValue(signal["signal"]) == 11 &&
        !fullShellText.contains("ORLIX-TCTI-SHELL-USABLE")
    return matchesCurrentFailure ? (report, object) : nil
}

func postFullShellSHReadFaultReducerPass(_ gate: Gate) -> GateStatus {
    let reducerReport = reportFact(target: "tcti-post-sh-read-fault-reducer")
    let reducerObject = try? loadJSONObject(root.appendingPathComponent(reducerReport.path))
    let reducerArtifacts = (reducerObject?["artifacts"] as? [Any] ?? []).compactMap { stringValue($0) }

    guard let (report, _) = postFullShellSHReadFaultReport() else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "not_needed",
            passed: false,
        satisfiesPrerequisite: true,
            reason: "latest pinned simulator full-shell report does not match sh read-fault reducer signature",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: reducerReport.exists ? [reducerReport] : [],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    let reducerCoversReport = reducerArtifacts.contains(report.path)
    if reducerReport.status == "pass",
       reducerReport.passed,
       reducerReport.gitSHA == gitSHA(),
       reducerCoversReport {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "pass",
            passed: true,
            reason: "sh read-fault reducer \(reducerReport.path) covers current simulator report \(report.path)",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: [report, reducerReport],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        state: "ready",
        passed: false,
        reason: "current pinned simulator report \(report.path) reaches sh static PIE and mmap progress, then sh receives SIGSEGV on a nonzero user-data read before full shell usability; reduce it before patching runtime behavior",
        prerequisites: gate.prerequisites,
        prerequisitesSatisfied: false,
        reportPaths: gate.expectedReportPaths,
        reports: reducerReport.exists ? [report, reducerReport] : [report],
        readinessEligible: gate.readinessEligible,
        physicalDevice: gate.physicalDevice,
        gadget: gate.gadget
    )
}

func postConsoleSHSIGABRTReport() -> (ReportFact, [String: Any])? {
    guard let (report, object) = latestRuntimeReport(gate: "tcti-init-console-write", destination: "iphonesimulator") else {
        return nil
    }
    let events = object["tcti_runtime_events"] as? [String: Any] ?? [:]
    let firstSVC = events["first_svc"] as? [String: Any] ?? [:]
    let staticPIE = events["static_pie_image"] as? [String: Any] ?? [:]
    let mmap = events["last_mmap_syscall"] as? [String: Any] ?? [:]
    let fault = events["fatal_user_fault"] as? [String: Any] ?? [:]
    let signal = events["signaled_process"] as? [String: Any] ?? [:]
    let lastReturn = events["last_sh_syscall_return"] as? [String: Any] ?? [:]
    let shPID = intValue(staticPIE["pid"])
    let shPIDHex = shPID.map { String(format: "0x%x", $0) } ?? ""
    let terminalText = runtimeArtifactText(object, suffix: "simulator-terminal-output.txt") ?? ""
    let consoleText = runtimeArtifactText(object, suffix: "tcti-console-write.txt") ?? ""
    let matchesCurrentFailure = report.status == "fail" &&
        !report.passed &&
        report.gitSHA == gitSHA() &&
        stringValue(object["selected_device_id"]) == requiredSimulatorID &&
        stringValue(object["selected_device_name"]) == requiredSimulatorName &&
        intValue(object["simulator_booted_count"]) == 1 &&
        boolValue(object["simulator_single_booted"]) &&
        stringValue(firstSVC["task"]) == "init" &&
        intValue(firstSVC["pid"]) == 1 &&
        intValue(firstSVC["syscall"]) == 178 &&
        stringValue(staticPIE["task"]) == "sh" &&
        shPID != nil &&
        stringValue(staticPIE["entry"]) == "0x45418" &&
        !(stringValue(staticPIE["base"]) ?? "").isEmpty &&
        stringValue(mmap["task"]) == "sh" &&
        intValue(mmap["pid"]) == shPID &&
        intValue(mmap["syscall"]) == 222 &&
        intValue(fault["access"]) == nil &&
        intValue(fault["si"]) == nil &&
        (stringValue(fault["addr"]) ?? "").isEmpty &&
        intValue(signal["pid"]) == shPID &&
        intValue(signal["signal"]) == 6 &&
        stringValue(lastReturn["task"]) == "sh" &&
        intValue(lastReturn["pid"]) == shPID &&
        intValue(lastReturn["syscall"]) == 172 &&
        intValue(lastReturn["signed_ret"]) == shPID &&
        terminalText.contains("Orlix TCTI: static PIE image task=sh") &&
        terminalText.contains("Orlix TCTI: syscall return task=sh") &&
        terminalText.contains("syscall=160") &&
        terminalText.contains("syscall=172 ret=\(shPIDHex) signed_ret=\(shPID ?? -1)") &&
        terminalText.contains("syscall=129 x0=\(shPIDHex) x1=0x6") &&
        terminalText.contains("orlix-init: process signaled pid=\(shPID ?? -1) signal=6") &&
        !consoleText.contains("ORLIX-TCTI-CONSOLE-OK")
    return matchesCurrentFailure ? (report, object) : nil
}

func postConsoleSHSIGABRTReducerPass(_ gate: Gate) -> GateStatus {
    let reducerReport = reportFact(target: "tcti-post-console-sh-sigabrt-reducer")
    let reducerObject = try? loadJSONObject(root.appendingPathComponent(reducerReport.path))
    let reducerArtifacts = (reducerObject?["artifacts"] as? [Any] ?? []).compactMap { stringValue($0) }

    guard let (report, _) = postConsoleSHSIGABRTReport() else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "not_needed",
            passed: false,
        satisfiesPrerequisite: true,
            reason: "latest pinned simulator console report does not match sh SIGABRT reducer signature",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: reducerReport.exists ? [reducerReport] : [],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    let reducerCoversReport = reducerArtifacts.contains(report.path)
    if reducerReport.status == "pass",
       reducerReport.passed,
       reducerReport.gitSHA == gitSHA(),
       reducerCoversReport {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "pass",
            passed: true,
            reason: "console sh SIGABRT reducer \(reducerReport.path) covers current simulator report \(report.path)",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: [report, reducerReport],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        state: "ready",
        passed: false,
        reason: "current pinned simulator console report \(report.path) reaches sh static PIE and syscall-return progress, then sh receives SIGABRT before ORLIX-TCTI-CONSOLE-OK; reduce it before patching runtime behavior",
        prerequisites: gate.prerequisites,
        prerequisitesSatisfied: false,
        reportPaths: gate.expectedReportPaths,
        reports: reducerReport.exists ? [report, reducerReport] : [report],
        readinessEligible: gate.readinessEligible,
        physicalDevice: gate.physicalDevice,
        gadget: gate.gadget
    )
}

func postFullShellInitWriteFaultFixPass(_ gate: Gate) -> GateStatus {
    let fixReport = reportFact(target: "tcti-post-full-shell-init-write-fault-fix")
    if fixReport.status == "pass",
       fixReport.passed,
       fixReport.gitSHA == gitSHA() {
        return basicReportGate(gate, target: "tcti-post-full-shell-init-write-fault-fix")
    }

    guard let (report, _) = postFullShellInitWriteFaultReport() else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "not_needed",
            passed: false,
        satisfiesPrerequisite: true,
            reason: "latest pinned simulator full-shell report does not require the init write-fault fix gate",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: fixReport.exists ? [fixReport] : [],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    let reducerGate = Gate(
        id: "no-phone-tcti-post-full-shell-init-write-fault-reducer",
        command: "make tcti-gate TARGET=tcti-post-full-shell-init-write-fault-reducer",
        kind: "no-phone-reducer",
        prerequisites: [],
        allowedScope: [],
        forbiddenScope: [],
        expectedReportPaths: [],
        readinessEligible: false,
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [],
        reducerRequirements: [],
        requiredSubagentsOrSkills: [],
        commitMessageTemplate: "",
        stopConditions: []
    )
    let reducerStatus = postFullShellInitWriteFaultReducerPass(reducerGate)
    guard reducerStatus.passed else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "missing",
            passed: false,
            reason: "init write-fault fix requires a passing reducer for current report \(report.path)",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: reducerStatus.reports + (fixReport.exists ? [fixReport] : []),
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        state: "ready",
        passed: false,
        reason: "current simulator report \(report.path) is reduced; implement the scoped TCTI init write-fault access-sync fix before rerunning full-shell usability",
        prerequisites: gate.prerequisites,
        prerequisitesSatisfied: false,
        reportPaths: gate.expectedReportPaths,
        reports: reducerStatus.reports + (fixReport.exists ? [fixReport] : []),
        readinessEligible: gate.readinessEligible,
        physicalDevice: gate.physicalDevice,
        gadget: gate.gadget
    )
}

func postFullShellSHSIGABRTFixPass(_ gate: Gate) -> GateStatus {
    let fixReport = reportFact(target: "tcti-post-full-shell-sh-sigabrt-fix")
    let fixObject = try? loadJSONObject(root.appendingPathComponent(fixReport.path))
    let fixArtifacts = (fixObject?["artifacts"] as? [Any] ?? []).compactMap { stringValue($0) }

    guard let (report, _) = postFullShellSHSIGABRTReport() else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "not_needed",
            passed: false,
        satisfiesPrerequisite: true,
            reason: "latest pinned simulator full-shell report does not require sh SIGABRT fix gate",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: fixReport.exists ? [fixReport] : [],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    let reducerGate = Gate(
        id: "no-phone-tcti-post-full-shell-sh-sigabrt-reducer",
        command: "make tcti-gate TARGET=tcti-post-full-shell-sh-sigabrt-reducer",
        kind: "no-phone-reducer",
        prerequisites: [],
        allowedScope: [],
        forbiddenScope: [],
        expectedReportPaths: [],
        readinessEligible: false,
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [],
        reducerRequirements: [],
        requiredSubagentsOrSkills: [],
        commitMessageTemplate: "",
        stopConditions: []
    )
    let reducerStatus = postFullShellSHSIGABRTReducerPass(reducerGate)
    guard reducerStatus.passed else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "missing",
            passed: false,
            reason: "sh SIGABRT fix requires passing reducer for current report \(report.path)",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: reducerStatus.reports + (fixReport.exists ? [fixReport] : []),
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    if fixReport.status == "pass",
       fixReport.passed,
       fixReport.gitSHA == gitSHA(),
       fixArtifacts.contains(report.path) {
        return basicReportGate(gate, target: "tcti-post-full-shell-sh-sigabrt-fix")
    }

    let userPageURL = root.appendingPathComponent("OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/mm/tcti_user_page.c")
    let initURL = root.appendingPathComponent("OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/mm/init.c")
    let userPageText = (try? String(contentsOf: userPageURL, encoding: .utf8)) ?? ""
    let initText = (try? String(contentsOf: initURL, encoding: .utf8)) ?? ""
    let sourceLooksFixed = userPageText.contains("unsigned long fault_flags = 0") &&
        userPageText.contains("case TCTI_ACCESS_WRITE:") &&
        userPageText.contains("ORLIX_HOST_USER_FAULT_WRITE") &&
        userPageText.contains("case TCTI_ACCESS_FETCH:") &&
        userPageText.contains("ORLIX_HOST_USER_FAULT_EXEC") &&
        userPageText.contains("orlix_sync_current_user_fault_window(address, fault_flags)") &&
        initText.contains("host_fault_flags & ORLIX_HOST_USER_FAULT_EXEC") &&
        initText.contains("host_fault_flags & ORLIX_HOST_USER_FAULT_WRITE") &&
        initText.contains("return orlix_fault_in_user_page_unlocked(mm, page, fault_flags)") &&
        initText.contains("fault_flags & ORLIX_HOST_USER_FAULT_WRITE") &&
        initText.contains("orlix_fault_in_user_page_unlocked(mm, page, fault_flags)") &&
        initText.contains("mmap_read_unlock(mm)")

    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        state: sourceLooksFixed ? "ready" : "missing",
        passed: false,
        reason: sourceLooksFixed ? "source contains hosted user-window access-class fix for \(report.path), but fix gate report \(fixReport.path) is not current passing" : "source does not yet preserve TCTI READ/WRITE/FETCH access class when syncing hosted user window for \(report.path)",
        prerequisites: gate.prerequisites,
        prerequisitesSatisfied: true,
        reportPaths: gate.expectedReportPaths,
        reports: reducerStatus.reports + (fixReport.exists ? [fixReport] : []),
        readinessEligible: gate.readinessEligible,
        physicalDevice: gate.physicalDevice,
        gadget: gate.gadget
    )
}

func postFullShellInitSecondMmapHangReport() -> (ReportFact, [String: Any])? {
    guard let (report, object) = latestRuntimeReport(gate: "tcti-full-shell-usability", destination: "iphonesimulator") else {
        return nil
    }
    let events = object["tcti_runtime_events"] as? [String: Any] ?? [:]
    let firstSVC = events["first_svc"] as? [String: Any] ?? [:]
    let staticPIE = events["static_pie_image"] as? [String: Any] ?? [:]
    let mmap = events["last_mmap_syscall"] as? [String: Any] ?? [:]
    let fault = events["fatal_user_fault"] as? [String: Any] ?? [:]
    let signal = events["signaled_process"] as? [String: Any] ?? [:]
    let lastReturn = events["last_sh_syscall_return"] as? [String: Any] ?? [:]
    let terminalText = runtimeArtifactText(object, suffix: "simulator-terminal-output.txt") ?? ""
    let fullShellText = runtimeArtifactText(object, suffix: "tcti-full-shell-usability.txt") ?? ""
    let matchesCurrentFailure = report.status == "fail" &&
        !report.passed &&
        report.gitSHA == gitSHA() &&
        stringValue(firstSVC["task"]) == "init" &&
        intValue(firstSVC["pid"]) == 1 &&
        intValue(firstSVC["syscall"]) == 178 &&
        stringValue(staticPIE["task"]) == "init" &&
        intValue(staticPIE["pid"]) == 1 &&
        !(stringValue(staticPIE["base"]) ?? "").isEmpty &&
        stringValue(mmap["task"]) == "init" &&
        intValue(mmap["pid"]) == 1 &&
        intValue(mmap["syscall"]) == 222 &&
        intValue(fault["access"]) == nil &&
        intValue(fault["si"]) == nil &&
        (stringValue(fault["addr"]) ?? "").isEmpty &&
        intValue(signal["pid"]) == nil &&
        intValue(signal["signal"]) == nil &&
        intValue(lastReturn["syscall"]) == nil &&
        terminalText.contains("Orlix TCTI: static PIE image task=init") &&
        terminalText.contains("syscall=222") &&
        !terminalText.contains("Orlix TCTI: static PIE image task=sh") &&
        !fullShellText.contains("ORLIX-TCTI-SHELL-USABLE")
    return matchesCurrentFailure ? (report, object) : nil
}

func postFullShellInitSecondMmapHangReducerPass(_ gate: Gate) -> GateStatus {
    let reducerReport = reportFact(target: "tcti-post-full-shell-init-second-mmap-hang-reducer")
    let reducerObject = try? loadJSONObject(root.appendingPathComponent(reducerReport.path))
    let reducerArtifacts = (reducerObject?["artifacts"] as? [Any] ?? []).compactMap { stringValue($0) }

    guard let (report, _) = postFullShellInitSecondMmapHangReport() else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "not_needed",
            passed: false,
        satisfiesPrerequisite: true,
            reason: "latest pinned simulator full-shell report does not match init second-mmap hang reducer signature",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: reducerReport.exists ? [reducerReport] : [],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    let reducerCoversReport = reducerArtifacts.contains(report.path)
    if reducerReport.status == "pass",
       reducerReport.passed,
       reducerReport.gitSHA == gitSHA(),
       reducerCoversReport {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "pass",
            passed: true,
            reason: "init second-mmap hang reducer \(reducerReport.path) covers current simulator report \(report.path)",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: [report, reducerReport],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        state: "missing",
        passed: false,
        reason: "current pinned simulator report \(report.path) records init second-mmap hang before full shell usability; reduce it before patching runtime behavior",
        prerequisites: gate.prerequisites,
        prerequisitesSatisfied: false,
        reportPaths: gate.expectedReportPaths,
        reports: reducerReport.exists ? [report, reducerReport] : [report],
        readinessEligible: gate.readinessEligible,
        physicalDevice: gate.physicalDevice,
        gadget: gate.gadget
    )
}

func postFullShellInitSecondMmapHangFixPass(_ gate: Gate) -> GateStatus {
    let fixReport = reportFact(target: "tcti-post-full-shell-init-second-mmap-hang-fix")
    let fixObject = try? loadJSONObject(root.appendingPathComponent(fixReport.path))
    let fixArtifacts = (fixObject?["artifacts"] as? [Any] ?? []).compactMap { stringValue($0) }

    guard let (report, _) = postFullShellInitSecondMmapHangReport() else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "not_needed",
            passed: false,
        satisfiesPrerequisite: true,
            reason: "latest pinned simulator full-shell report does not require init second-mmap fix gate",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: fixReport.exists ? [fixReport] : [],
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    let reducerGate = Gate(
        id: "no-phone-tcti-post-full-shell-init-second-mmap-hang-reducer",
        command: "make tcti-gate TARGET=tcti-post-full-shell-init-second-mmap-hang-reducer",
        kind: "no-phone-reducer",
        prerequisites: [],
        allowedScope: [],
        forbiddenScope: [],
        expectedReportPaths: [],
        readinessEligible: false,
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [],
        reducerRequirements: [],
        requiredSubagentsOrSkills: [],
        commitMessageTemplate: "",
        stopConditions: []
    )
    let reducerStatus = postFullShellInitSecondMmapHangReducerPass(reducerGate)
    guard reducerStatus.passed else {
        return GateStatus(
            id: gate.id,
            command: gate.command,
            kind: gate.kind,
            state: "missing",
            passed: false,
            reason: "init second-mmap fix requires passing reducer for current report \(report.path)",
            prerequisites: gate.prerequisites,
            prerequisitesSatisfied: false,
            reportPaths: gate.expectedReportPaths,
            reports: reducerStatus.reports + (fixReport.exists ? [fixReport] : []),
            readinessEligible: gate.readinessEligible,
            physicalDevice: gate.physicalDevice,
            gadget: gate.gadget
        )
    }

    if fixReport.status == "pass",
       fixReport.passed,
       fixReport.gitSHA == gitSHA(),
       fixArtifacts.contains(report.path) {
        return basicReportGate(gate, target: "tcti-post-full-shell-init-second-mmap-hang-fix")
    }

    let engineURL = root.appendingPathComponent("OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/engine.c")
    let testURL = root.appendingPathComponent("OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/tests/tcti_decode_test.c")
    let engineText = (try? String(contentsOf: engineURL, encoding: .utf8)) ?? ""
    let testText = (try? String(contentsOf: testURL, encoding: .utf8)) ?? ""
    let sourceLooksFixed = !engineText.contains("current->thread.user_tls = 0;") &&
        testText.contains("KUNIT_EXPECT_EQ(test, 0x700000123000ULL, current->thread.user_tls)")

    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        state: sourceLooksFixed ? "ready" : "missing",
        passed: false,
        reason: sourceLooksFixed ? "source preserves guest TLS across TCTI successful execve return for \(report.path), but fix gate report \(fixReport.path) is not current passing" : "source still clears or does not test guest TLS preservation across TCTI successful execve return for \(report.path)",
        prerequisites: gate.prerequisites,
        prerequisitesSatisfied: true,
        reportPaths: gate.expectedReportPaths,
        reports: reducerStatus.reports + (fixReport.exists ? [fixReport] : []),
        readinessEligible: gate.readinessEligible,
        physicalDevice: gate.physicalDevice,
        gadget: gate.gadget
    )
}

func addSubShiftedXZRFixPass(_ gate: Gate) -> GateStatus {
    let report = reportFact(target: "tcti-add-sub-shifted-xzr-fix")
    if currentReportPassed(report) {
        return basicReportGate(gate, target: "tcti-add-sub-shifted-xzr-fix")
    }

    if let (_, simulatorText) = latestSimulatorStabilityText(),
       brkTrapSignature(simulatorText) {
        return basicReportGate(gate, target: "tcti-add-sub-shifted-xzr-fix")
    }

    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        state: "superseded",
        passed: false,
        satisfiesPrerequisite: true,
        reason: "latest simulator stability report no longer matches the BRK/XZR shifted-addsub signature; do not block the current simulator ladder on stale BRK reducer evidence",
        prerequisites: gate.prerequisites,
        prerequisitesSatisfied: false,
        reportPaths: gate.expectedReportPaths,
        reports: report.exists ? [report] : [],
        readinessEligible: gate.readinessEligible,
        physicalDevice: gate.physicalDevice,
        gadget: gate.gadget
    )
}

func simdMOVI2SFixPass(_ gate: Gate) -> GateStatus {
    let report = reportFact(target: "tcti-simd-movi-2s-fix")
    if currentReportPassed(report) {
        return basicReportGate(gate, target: "tcti-simd-movi-2s-fix")
    }

    if let (_, simulatorText) = latestSimulatorStabilityText(),
       simdMOVI2SSignature(simulatorText) {
        return basicReportGate(gate, target: "tcti-simd-movi-2s-fix")
    }

    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        state: "superseded",
        passed: false,
        satisfiesPrerequisite: true,
        reason: "latest simulator stability report no longer matches the SIMD MOVI v0.2s unsupported-instruction signature; do not block the current simulator ladder on stale MOVI 2S evidence",
        prerequisites: gate.prerequisites,
        prerequisitesSatisfied: false,
        reportPaths: gate.expectedReportPaths,
        reports: report.exists ? [report] : [],
        readinessEligible: gate.readinessEligible,
        physicalDevice: gate.physicalDevice,
        gadget: gate.gadget
    )
}

func simdSTRSFixPass(_ gate: Gate) -> GateStatus {
    let report = reportFact(target: "tcti-simd-str-s-fix")
    if currentReportPassed(report) {
        return basicReportGate(gate, target: "tcti-simd-str-s-fix")
    }

    if let (_, simulatorText) = latestSimulatorStabilityText(),
       simdSTRSSignature(simulatorText) {
        return basicReportGate(gate, target: "tcti-simd-str-s-fix")
    }

    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        state: "superseded",
        passed: false,
        satisfiesPrerequisite: true,
        reason: "latest simulator stability report no longer matches the SIMD/FP STR s0 unsupported-instruction signature; do not block the current simulator ladder on stale STR S evidence",
        prerequisites: gate.prerequisites,
        prerequisitesSatisfied: false,
        reportPaths: gate.expectedReportPaths,
        reports: report.exists ? [report] : [],
        readinessEligible: gate.readinessEligible,
        physicalDevice: gate.physicalDevice,
        gadget: gate.gadget
    )
}

func simdDUP2DFixPass(_ gate: Gate) -> GateStatus {
    let report = reportFact(target: "tcti-simd-dup-2d-fix")
    if currentReportPassed(report) {
        return basicReportGate(gate, target: "tcti-simd-dup-2d-fix")
    }

    if let (_, simulatorText) = latestSimulatorStabilityText(),
       simdDUP2DSignature(simulatorText) {
        return basicReportGate(gate, target: "tcti-simd-dup-2d-fix")
    }

    return GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        state: "superseded",
        passed: false,
        satisfiesPrerequisite: true,
        reason: "latest simulator stability report no longer matches the SIMD DUP v0.2d unsupported-instruction signature; do not block the current simulator ladder on stale DUP 2D evidence",
        prerequisites: gate.prerequisites,
        prerequisitesSatisfied: false,
        reportPaths: gate.expectedReportPaths,
        reports: report.exists ? [report] : [],
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
    case "tcti-simulator-kernel-first-syscall", "simulator-tcti-init-first-syscall":
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
        return supersededSimulatorReducerGate(
            gate,
            target: "tcti-simd-self-move-reducer",
            staleSignature: simdSelfMoveSignature,
            supersededBy: ["tcti-simd-self-move-fix", "tcti-simd-movi-4s-0x1-fix"]
        )
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
        return addSubShiftedXZRFixPass(gate)
    case "tcti-simd-movi-2s-fix":
        return simdMOVI2SFixPass(gate)
    case "tcti-simd-movi-16b-fix":
        return basicReportGate(gate, target: "tcti-simd-movi-16b-fix")
    case "tcti-simd-movi-4s-0x1-fix":
        return basicReportGate(gate, target: "tcti-simd-movi-4s-0x1-fix")
    case "tcti-simd-cmeq-4s-fix":
        return basicReportGate(gate, target: "tcti-simd-cmeq-4s-fix")
    case "tcti-simd-umaxv-4s-fix":
        return basicReportGate(gate, target: "tcti-simd-umaxv-4s-fix")
    case "tcti-simd-str-s-fix":
        return simdSTRSFixPass(gate)
    case "tcti-simd-dup-2d-fix":
        return simdDUP2DFixPass(gate)
    case "no-phone-tcti-post-overlay-null-user-fault-reducer":
        return basicReportGate(gate, target: "tcti-post-overlay-null-user-fault-reducer")
    case "tcti-post-overlay-null-user-fault-fix":
        return basicReportGate(gate, target: "tcti-post-overlay-null-user-fault-fix")
    case "no-phone-tcti-ldrsw-sign-extension-reducer":
        return supersededSimulatorReducerGate(
            gate,
            target: "tcti-ldrsw-sign-extension-reducer",
            staleSignature: ldrswSignExtensionSignature,
            supersededBy: ["tcti-ldrsw-sign-extension-fix", "tcti-post-setsid-tls-fault-reducer"]
        )
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
        return postBashMmapReadFaultReducerPass(gate)
    case "no-phone-tcti-init-read-fault-reducer":
        return initReadFaultReducerPass(gate)
    case "no-phone-tcti-post-static-pie-init-read-fault-reducer":
        return postStaticPIEInitReadFaultReducerPass(gate)
    case "tcti-post-static-pie-init-tls-fix":
        return postStaticPIEInitTLSFixPass(gate)
    case "no-phone-tcti-post-full-shell-cat-read-fault-reducer":
        return postFullShellCatReadFaultReducerPass(gate)
    case "tcti-post-full-shell-cat-read-fault-fix":
        return postFullShellCatReadFaultFixPass(gate)
    case "no-phone-tcti-post-full-shell-init-write-fault-reducer":
        return postFullShellInitWriteFaultReducerPass(gate)
    case "tcti-post-full-shell-init-write-fault-fix":
        return postFullShellInitWriteFaultFixPass(gate)
    case "no-phone-tcti-post-full-shell-sh-sigabrt-reducer":
        return postFullShellSHSIGABRTReducerPass(gate)
    case "tcti-post-full-shell-sh-sigabrt-fix":
        return postFullShellSHSIGABRTFixPass(gate)
    case "no-phone-tcti-post-full-shell-sh-read-fault-reducer":
        return postFullShellSHReadFaultReducerPass(gate)
    case "no-phone-tcti-post-full-shell-init-second-mmap-hang-reducer":
        return postFullShellInitSecondMmapHangReducerPass(gate)
    case "tcti-post-full-shell-init-second-mmap-hang-fix":
        return postFullShellInitSecondMmapHangFixPass(gate)
    case "no-phone-tcti-post-full-shell-cat-posix-memalign-brk-reducer":
        return postFullShellCatPosixMemalignBRKReducerPass(gate)
    case "tcti-post-full-shell-cat-posix-memalign-brk-fix":
        return postFullShellCatPosixMemalignBRKFixPass(gate)
    case "no-phone-tcti-post-busybox-sigabrt-reducer":
        return postBusyBoxSIGABRTReducerPass(gate)
    case "tcti-user-data-window-refresh-fix":
        return userDataWindowRefreshFixPass(gate)
    case "tcti-busybox-syscall-return-trace":
        return basicReportGate(gate, target: "tcti-busybox-syscall-return-trace")
    case "no-phone-tcti-post-busybox-shell-command-sigill-reducer":
        return postBusyBoxShellCommandSIGILLReducerPass(gate)
    case "simulator-tcti-runtime-stability":
        return simulatorStabilityPass(gate)
    case "no-phone-tcti-post-console-sh-sigabrt-reducer":
        return postConsoleSHSIGABRTReducerPass(gate)
    case "simulator-tcti-linux-console-usability":
        return simulatorConsoleUsabilityPass(gate)
    case "simulator-tcti-static-busybox-start":
        return simulatorStaticBusyBoxStartPass(gate)
    case "simulator-tcti-static-busybox-shell-command":
        return simulatorStaticBusyBoxShellCommandPass(gate)
    case "tcti-simulator-mlibc-smoke":
        return simulatorRuntimeMarkerPass(gate, runtimeGate: "tcti-mlibc-smoke", marker: "ORLIX-TCTI-MLIBC-SMOKE-OK", artifactSuffix: "tcti-mlibc-smoke.txt")
    case "tcti-simulator-coreutils-smoke":
        return simulatorRuntimeMarkerPass(gate, runtimeGate: "tcti-coreutils-smoke", marker: "ORLIX-TCTI-COREUTILS-SMOKE-OK", artifactSuffix: "tcti-coreutils-smoke.txt")
    case "tcti-simulator-oci-rootfs-command":
        return simulatorRuntimeMarkerPass(gate, runtimeGate: "tcti-oci-rootfs-command", marker: "ORLIX-TCTI-OCI-ROOTFS-COMMAND-OK", artifactSuffix: "tcti-oci-rootfs-command.txt")
    case "tcti-simulator-interactive-terminal-smoke":
        return simulatorRuntimeMarkerPass(gate, runtimeGate: "tcti-interactive-terminal-smoke", marker: "ORLIX-TCTI-INTERACTIVE-TERMINAL-OK", artifactSuffix: "tcti-interactive-terminal-smoke.txt")
    case "simulator-tcti-full-shell-usability":
        return simulatorRuntimeMarkerPass(gate, runtimeGate: "tcti-full-shell-usability", marker: "ORLIX-TCTI-SHELL-USABLE", artifactSuffix: "tcti-full-shell-usability.txt")
    case "simulator-tcti-package-behavior":
        return simulatorRuntimeMarkerPass(gate, runtimeGate: "tcti-package-behavior", marker: "ORLIX-TCTI-PACKAGE-BEHAVIOR-OK", artifactSuffix: "tcti-package-behavior.txt")
    case "simulator-tcti-dynamic-loader-support":
        return simulatorRuntimeMarkerPass(gate, runtimeGate: "tcti-dynamic-loader-support", marker: "ORLIX-TCTI-DYNAMIC-LOADER-OK", artifactSuffix: "tcti-dynamic-loader-support.txt")
    case "simulator-tcti-signals":
        return simulatorRuntimeMarkerPass(gate, runtimeGate: "tcti-signals", marker: "ORLIX-TCTI-SIGNALS-OK", artifactSuffix: "tcti-signals.txt")
    case "simulator-tcti-vfs-completeness":
        return simulatorRuntimeMarkerPass(gate, runtimeGate: "tcti-vfs-completeness", marker: "ORLIX-TCTI-VFS-OK", artifactSuffix: "tcti-vfs-completeness.txt")
    case "simulator-tcti-full-linux-runtime-readiness":
        return simulatorRuntimeMarkerPass(gate, runtimeGate: "tcti-full-linux-runtime-readiness", marker: "ORLIX-TCTI-FULL-RUNTIME-OK", artifactSuffix: "tcti-full-linux-runtime-readiness.txt")
    case "physical-tcti-init-first-syscall":
        return missingGate(gate, reason: "physical first-syscall gate is not allowed until no-phone and simulator prerequisites pass")
    default:
        if let target = tctiGateTarget(in: gate.command) {
            return basicReportGate(gate, target: target)
        }
        return missingGate(gate, reason: "unknown roadmap gate")
    }
}

func artifactStatus(_ gate: Gate, passed: Bool, reason: String) -> GateStatus {
    GateStatus(
        id: gate.id,
        command: gate.command,
        kind: gate.kind,
        proofTier: gate.proofTier,
        acceptanceWeight: gate.acceptanceWeight,
        realStackRequired: gate.realStackRequired,
        canClaimRuntimeReadiness: gate.canClaimRuntimeReadiness,
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

func simulatorProofGate(id: String, runtimeGate: String, marker: String, prerequisite: String, label: String) -> Gate {
    let command = "make runtime-validation DESTINATION=iphonesimulator GATE=\(runtimeGate) ORLIX_SIMULATOR_ID=\(requiredSimulatorID) ORLIX_TCTI_REQUIRED_SIMULATOR_ID=\(requiredSimulatorID) ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=\(requiredSimulatorName)"
    return Gate(
        id: id,
        command: command,
        kind: "simulator-runtime",
        proofTier: "simulator",
        acceptanceWeight: "readiness",
        realStackRequired: true,
        canClaimRuntimeReadiness: true,
        prerequisites: [prerequisite],
        allowedScope: [
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/**",
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/mm/**",
            "OrlixOS/Sources/make/**",
            "tools/runtime/orlix-runtime-validation.sh",
            "tools/tcti/orlix-tcti-gate.swift",
            "tools/tcti/fixtures/**",
            ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            ".agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json",
            "docs/plans/active/orlix-tcti/PLAN.md",
            "docs/plans/active/orlix-tcti/IMPLEMENT.md",
        ],
        forbiddenScope: [
            "Do not run phone gates.",
            "Do not use any simulator except Orlix-iPhone-15-Pro-Max.",
            "Do not treat an earlier simulator pass as proof for \(label).",
            "Do not add production assembly.",
            "Do not add gadget dispatch.",
            "Do not add HostAdapter, Darwin syscall, fd table, process, signal, scheduler, or broad Linux runtime semantics unless this gate envelope is extended first.",
            "Do not edit generated Linux or build trees.",
        ],
        expectedReportPaths: [
            "Build/Reports/runtime/\(runtimeGate)-*.json",
            "Build/Reports/runtime/tcti-static-busybox-shell-command-*.json",
            "Build/Reports/runtime/tcti-static-busybox-start-*.json",
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
            "rtk proxy \(command)",
        ],
        reducerRequirements: [
            "If \(label) fails, reduce it into a no-phone TCTI fixture or report-specific reducer before patching production behavior.",
        ],
        requiredSubagentsOrSkills: [
            "orlix-tcti-safety",
            "orlix-tcti-debug",
            "tcti-planner",
            "tcti-safety-reviewer",
            "tcti-llvm-inspector",
            "tcti-test-reducer",
            "tcti-release-gate-reviewer",
        ],
        commitMessageTemplate: "test(tcti): prove \(label) on simulator",
        stopConditions: [
            "Stop if the marker \(marker) is missing.",
            "Stop if more than the pinned simulator is booted.",
            "Stop if this gate is used to claim broader readiness than \(label).",
        ]
    )
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
            command: "make runtime-validation DESTINATION=iphonesimulator GATE=tcti-init-first-syscall ORLIX_SIMULATOR_ID=\(requiredSimulatorID) ORLIX_TCTI_REQUIRED_SIMULATOR_ID=\(requiredSimulatorID) ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=\(requiredSimulatorName)",
            kind: "simulator-runtime",
            prerequisites: ["tcti-simulator-kernel-first-syscall"],
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
                "Do not use any simulator except Orlix-iPhone-15-Pro-Max (ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3).",
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
                "rtk proxy make runtime-validation DESTINATION=iphonesimulator GATE=tcti-init-first-syscall ORLIX_SIMULATOR_ID=\(requiredSimulatorID) ORLIX_TCTI_REQUIRED_SIMULATOR_ID=\(requiredSimulatorID) ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=\(requiredSimulatorName)",
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
            "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
            "rtk proxy make tcti-gate TARGET=tcti-static-pie-relocation-fix",
            "rtk test env PATH=\"$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin\" make -f OrlixKernel/Makefile kunit PROFILE=development",
            "rtk test env PATH=\"$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin\" make -f OrlixKernel/Makefile kunit PROFILE=release",
        ],
        reducerRequirements: [
            "A current simulator stability pass must include structured static PIE image evidence and no fatal null GOT read signature.",
            "If simulator stability regresses to the old null GOT read failure, regenerate a current reducer before production TCTI patching.",
            "After the production fix marker passes, rerun simulator stability through the next selected gate.",
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
            "Build/TCTI/reproducers/tcti-simd-self-move-fix/simd-self-move-pass-regression.json",
            "Build/Reports/runtime/tcti-simulator-stability-*.json",
        ],
        readinessEligible: false,
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [
            "rtk proxy git diff --check",
            "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
            "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
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
            "The rail must not require the old generated simulator SIGILL report as current proof.",
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
        commitMessageTemplate: "fix(tcti): repair simd self move rail evidence contract",
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
        id: "tcti-simd-movi-4s-0x1-fix",
        command: "make tcti-gate TARGET=tcti-simd-movi-4s-0x1-fix",
        kind: "no-phone-simulator-reducer-fix",
        prerequisites: ["no-phone-tcti-post-busybox-shell-command-sigill-reducer"],
        allowedScope: [
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/decode_aarch64.c",
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/switch_debug.c",
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/tests/tcti_decode_test.c",
            "tools/tcti/orlix-tcti-gate.swift",
            "tools/tcti/fixtures/golden_elf/init_001_exit_simd_movi_4s_0x1.S",
            "Makefile",
            ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "docs/plans/active/orlix-tcti/IMPLEMENT.md",
        ],
        forbiddenScope: [
            "Do not run physical-device gates.",
            "Do not add production assembly.",
            "Do not add gadget dispatch.",
            "Do not broaden SIMD beyond the exact emitted MOVI vN.4s #0x1 subset.",
            "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime semantics.",
            "Do not claim simulator shell usability until the simulator gate passes after this no-phone fix.",
        ],
        expectedReportPaths: [
            "Build/TCTI/reports/tcti-simd-movi-4s-0x1-fix/report.json",
            "Build/TCTI/reproducers/tcti-simd-movi-4s-0x1-fix/simd-movi-4s-0x1-pass-regression.json",
            "Build/Reports/runtime/tcti-static-busybox-shell-command-*.json",
        ],
        readinessEligible: false,
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [
            "rtk proxy git diff --check",
            "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
            "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "rtk proxy make tcti-gate TARGET=tcti-simd-movi-4s-0x1-fix",
            "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-simd-movi-4s-0x1-fix/simd-movi-4s-0x1-pass-regression.json",
            "rtk proxy make agent-harness-check",
            "rtk proxy make agent-status AREA=orlix-tcti",
            "rtk proxy make agent-next AREA=orlix-tcti",
            "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
            "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
        ],
        reducerRequirements: [
            "The fix gate must cite simulator unsupported instruction 0x4f000421.",
            "The KUnit regression must prove MOVI v1.4s materializes 0x0000000100000001 in both SIMD halves for v1.",
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
        commitMessageTemplate: "test(tcti): support emitted simd movi shell immediate",
        stopConditions: [
            "Stop if latest simulator failure no longer exposes unsupported instruction 0x4f000421.",
            "Stop if fix would require generic SIMD modified-immediate decoding.",
            "Stop if KUnit cannot compile regression.",
        ]
    ),
    Gate(
        id: "tcti-simd-cmeq-4s-fix",
        command: "make tcti-gate TARGET=tcti-simd-cmeq-4s-fix",
        kind: "no-phone-simulator-reducer-fix",
        prerequisites: ["tcti-simd-movi-4s-0x1-fix"],
        allowedScope: [
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/decode_aarch64.h",
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/decode_aarch64.c",
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/switch_debug.c",
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/tests/tcti_decode_test.c",
            "tools/tcti/orlix-tcti-gate.swift",
            "tools/tcti/fixtures/golden_elf/init_001_exit_simd_cmeq_4s.S",
            ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "docs/plans/active/orlix-tcti/IMPLEMENT.md",
        ],
        forbiddenScope: [
            "Do not run physical-device gates.",
            "Do not add production assembly.",
            "Do not add gadget dispatch.",
            "Do not broaden SIMD beyond the exact emitted CMEQ vD.4s, vN.4s, vM.4s subset.",
            "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime semantics.",
            "Do not claim simulator shell usability until the simulator gate passes after this no-phone fix.",
        ],
        expectedReportPaths: [
            "Build/TCTI/reports/tcti-simd-cmeq-4s-fix/report.json",
            "Build/TCTI/reproducers/tcti-simd-cmeq-4s-fix/simd-cmeq-4s-pass-regression.json",
            "Build/Reports/runtime/tcti-static-busybox-shell-command-*.json",
        ],
        readinessEligible: false,
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [
            "rtk proxy git diff --check",
            "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
            "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "rtk proxy make tcti-gate TARGET=tcti-simd-cmeq-4s-fix",
            "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-simd-cmeq-4s-fix/simd-cmeq-4s-pass-regression.json",
            "rtk proxy make agent-harness-check",
            "rtk proxy make agent-status AREA=orlix-tcti",
            "rtk proxy make agent-next AREA=orlix-tcti",
            "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
            "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
        ],
        reducerRequirements: [
            "The fix gate must cite simulator unsupported instruction 0x6ea18c64 from the pinned static BusyBox shell-command report.",
            "The KUnit regression must prove CMEQ v4.4s, v3.4s, v1.4s produces 0xffffffff for equal 32-bit lanes and zero otherwise.",
            "The gate must not broaden into generic SIMD compare support.",
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
        commitMessageTemplate: "test(tcti): support emitted simd cmeq shell compare",
        stopConditions: [
            "Stop if current simulator failure no longer exposes unsupported instruction 0x6ea18c64.",
            "Stop if fix would require generic SIMD compare decoding.",
            "Stop if KUnit cannot compile regression.",
        ]
    ),
    Gate(
        id: "tcti-simd-umaxv-4s-fix",
        command: "make tcti-gate TARGET=tcti-simd-umaxv-4s-fix",
        kind: "no-phone-simulator-reducer-fix",
        prerequisites: ["tcti-simd-cmeq-4s-fix"],
        allowedScope: [
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/decode_aarch64.h",
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/decode_aarch64.c",
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/switch_debug.c",
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/tests/tcti_decode_test.c",
            "tools/tcti/orlix-tcti-gate.swift",
            "tools/tcti/fixtures/golden_elf/init_001_exit_simd_umaxv_4s.S",
            ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "docs/plans/active/orlix-tcti/IMPLEMENT.md",
        ],
        forbiddenScope: [
            "Do not run physical-device gates.",
            "Do not add production assembly.",
            "Do not add gadget dispatch.",
            "Do not broaden SIMD beyond the exact emitted UMAXV Sd, Vn.4s subset.",
            "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime semantics.",
            "Do not claim simulator shell usability until the simulator gate passes after this no-phone fix.",
        ],
        expectedReportPaths: [
            "Build/TCTI/reports/tcti-simd-umaxv-4s-fix/report.json",
            "Build/TCTI/reproducers/tcti-simd-umaxv-4s-fix/simd-umaxv-4s-pass-regression.json",
            "Build/Reports/runtime/tcti-static-busybox-shell-command-*.json",
        ],
        readinessEligible: false,
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [
            "rtk proxy git diff --check",
            "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
            "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "rtk proxy make tcti-gate TARGET=tcti-simd-umaxv-4s-fix",
            "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-simd-umaxv-4s-fix/simd-umaxv-4s-pass-regression.json",
            "rtk proxy make agent-harness-check",
            "rtk proxy make agent-status AREA=orlix-tcti",
            "rtk proxy make agent-next AREA=orlix-tcti",
            "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
            "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
        ],
        reducerRequirements: [
            "The fix gate must cite simulator unsupported instruction 0x6eb0a885 from the pinned static BusyBox shell-command report.",
            "The KUnit regression must prove UMAXV s5, v4.4s writes the unsigned maximum 32-bit lane to the scalar destination.",
            "The gate must not broaden into generic SIMD reduction support.",
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
        commitMessageTemplate: "test(tcti): support emitted simd max reduction",
        stopConditions: [
            "Stop if current simulator failure no longer exposes unsupported instruction 0x6eb0a885.",
            "Stop if fix would require generic SIMD reduction decoding.",
            "Stop if KUnit cannot compile regression.",
        ]
    ),
    Gate(
        id: "tcti-simd-str-s-fix",
        command: "make tcti-gate TARGET=tcti-simd-str-s-fix",
        kind: "no-phone-simulator-reducer-fix",
        prerequisites: ["tcti-simd-movi-4s-0x1-fix"],
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
            "The latest simulator stability failure must be current for HEAD and pinned to Orlix-iPhone-15-Pro-Max (ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3).",
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
        id: "tcti-ldrsw-sign-extension-fix",
        command: "make tcti-gate TARGET=tcti-ldrsw-sign-extension-fix",
        kind: "production-tcti-fix",
        prerequisites: ["tcti-post-overlay-null-user-fault-fix"],
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
            "Build/TCTI/reproducers/tcti-ldrsw-sign-extension-fix/ldrsw-sign-extension-pass-regression.json",
            "Build/Reports/runtime/tcti-simulator-stability-*.json",
        ],
        readinessEligible: false,
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [
            "rtk proxy git diff --check",
            "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
            "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
            "rtk proxy make tcti-gate TARGET=tcti-ldrsw-sign-extension-fix",
            "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-ldrsw-sign-extension-fix/ldrsw-sign-extension-pass-regression.json",
            "rtk proxy make agent-harness-check",
            "rtk proxy make agent-status AREA=orlix-tcti",
            "rtk proxy make agent-next AREA=orlix-tcti",
            "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
            "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
            "rtk test env PATH=\"$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin\" make -f OrlixKernel/Makefile kunit PROFILE=development",
            "rtk test env PATH=\"$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin\" make -f OrlixKernel/Makefile kunit PROFILE=release",
        ],
        reducerRequirements: [
            "The exact emitted LDRSW 0xb9801848 must execute through switch-debug and prove 32-bit sign extension into an X register before exit(42).",
            "The gate must write and replay a pass-regression reproducer without requiring a historical failing simulator report.",
            "The latest execution-fresh pinned simulator stability report must pass without the old high-address LDRSW fault signature.",
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
            "Use only Orlix-iPhone-15-Pro-Max (ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3) for simulator evidence.",
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
            "The latest simulator stability failure must be current for HEAD and pinned to Orlix-iPhone-15-Pro-Max (ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3).",
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
            "Use only Orlix-iPhone-15-Pro-Max (ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3) for simulator evidence.",
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
            "The latest simulator stability failure must be current for HEAD and pinned to Orlix-iPhone-15-Pro-Max (ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3).",
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
            "Use only Orlix-iPhone-15-Pro-Max (ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3) for simulator evidence.",
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
            "The latest simulator stability report must be current for HEAD and pinned to Orlix-iPhone-15-Pro-Max (ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3).",
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
            id: "no-phone-tcti-init-read-fault-reducer",
            command: "make tcti-gate TARGET=tcti-init-read-fault-reducer",
            kind: "no-phone-reducer",
            prerequisites: ["no-phone-tcti-post-bash-mmap-read-fault-reducer"],
            allowedScope: [
                "tools/tcti/orlix-tcti-gate.swift",
                ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Do not run physical-device gates.",
                "Do not run simulator gates while reducing this report-backed failure.",
                "Do not add production assembly.",
                "Do not add gadget dispatch.",
                "Do not flip product defconfigs.",
                "Do not edit generated Linux or build trees.",
                "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime semantics.",
                "Only bind the current pinned simulator init read-fault report to a replayable no-phone reducer.",
            ],
            expectedReportPaths: [
                "Build/TCTI/reports/tcti-init-read-fault-reducer/report.json",
                "Build/TCTI/reproducers/tcti-init-read-fault-reducer/init-read-fault-pass-regression.json",
                "Build/Reports/runtime/tcti-simulator-stability-*.json",
            ],
            readinessEligible: false,
            physicalDevice: false,
            gadget: false,
            requiredValidationCommands: [
                "rtk proxy git diff --check",
                "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
                "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "rtk proxy make tcti-gate TARGET=tcti-init-read-fault-reducer",
                "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-init-read-fault-reducer/init-read-fault-pass-regression.json",
                "rtk proxy make agent-harness-check",
                "rtk proxy make agent-status AREA=orlix-tcti",
                "rtk proxy make agent-next AREA=orlix-tcti",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
                "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
            ],
            reducerRequirements: [
                "The latest simulator stability failure must be current for HEAD and pinned to Orlix-iPhone-15-Pro-Max (ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3).",
                "The simulator report must show init first-svc syscall 178, then a non-null init read fault before static PIE or shell progress.",
                "The reducer must replay without running simulator or physical-device gates.",
            ],
            requiredSubagentsOrSkills: [
                "orlix-tcti-reproducer",
                "orlix-tcti-safety",
                "orlix-tcti-debug",
                "tcti-test-reducer",
                "tcti-safety-reviewer",
                "tcti-release-gate-reviewer",
            ],
            commitMessageTemplate: "test(tcti): reduce init read fault before simulator stability",
            stopConditions: [
                "Stop if no current simulator init read-fault report exists.",
                "Stop if reducer replay cannot reproduce the report-backed failure shape.",
                "Stop if production TCTI code would be required before the reducer exists.",
            ]
        ),
        Gate(
            id: "simulator-tcti-runtime-stability",
        command: "make runtime-validation DESTINATION=iphonesimulator GATE=tcti-simulator-stability ORLIX_SIMULATOR_ID=\(requiredSimulatorID) ORLIX_TCTI_REQUIRED_SIMULATOR_ID=\(requiredSimulatorID) ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=\(requiredSimulatorName)",
        kind: "simulator-runtime",
        prerequisites: ["no-phone-tcti-init-read-fault-reducer"],
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
                "Do not use any simulator except Orlix-iPhone-15-Pro-Max (ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3).",
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
                "rtk proxy make runtime-validation DESTINATION=iphonesimulator GATE=tcti-simulator-stability ORLIX_SIMULATOR_ID=\(requiredSimulatorID) ORLIX_TCTI_REQUIRED_SIMULATOR_ID=\(requiredSimulatorID) ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=\(requiredSimulatorName)",
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
            id: "no-phone-tcti-post-console-sh-sigabrt-reducer",
            command: "make tcti-gate TARGET=tcti-post-console-sh-sigabrt-reducer",
            kind: "no-phone-reducer",
            prerequisites: ["simulator-tcti-runtime-stability"],
            allowedScope: [
                "tools/tcti/orlix-tcti-gate.swift",
                ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                ".agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Do not patch runtime behavior before the console SIGABRT reducer exists and replays.",
                "Do not run physical-device gates.",
                "Do not run any simulator except Orlix-iPhone-15-Pro-Max.",
                "Do not add production TCTI assembly or gadget dispatch.",
                "Do not add HostAdapter Linux behavior or Darwin guest syscall behavior.",
                "Do not implement VFS, fd table, process, signal, scheduler, or broad Linux runtime semantics in the harness.",
            ],
            expectedReportPaths: [
                "Build/TCTI/reports/tcti-post-console-sh-sigabrt-reducer/report.json",
                "Build/TCTI/reproducers/tcti-post-console-sh-sigabrt-reducer/post-console-sh-sigabrt-pass-regression.json",
                "Build/Reports/runtime/tcti-init-console-write-*.json",
            ],
            readinessEligible: false,
            physicalDevice: false,
            gadget: false,
            requiredValidationCommands: [
                "rtk proxy git diff --check",
                "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
                "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "rtk proxy make tcti-gate TARGET=tcti-post-console-sh-sigabrt-reducer",
                "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-post-console-sh-sigabrt-reducer/post-console-sh-sigabrt-pass-regression.json",
                "rtk proxy make agent-harness-check",
                "rtk proxy make agent-status AREA=orlix-tcti",
                "rtk proxy make agent-next AREA=orlix-tcti",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
                "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
            ],
            reducerRequirements: [
                "The latest console simulator report must be current for HEAD and pinned to Orlix-iPhone-15-Pro-Max.",
                "The report must show sh static PIE entry 0x45418, mmap syscall 222, uname syscall 160, syscall 172 returning sh pid, then syscall 129 same pid signal 6 before ORLIX-TCTI-CONSOLE-OK.",
                "The reducer must replay before production TCTI patching.",
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
            commitMessageTemplate: "test(tcti): reduce simulator console sh sigabrt",
            stopConditions: [
                "Stop if latest console simulator report is stale or no longer matches sh SIGABRT signature.",
                "Stop if more than the pinned Orlix-iPhone-15-Pro-Max simulator is booted.",
                "Stop if reducer replay cannot reproduce the no-phone evidence gate.",
            ]
        ),
        Gate(
            id: "simulator-tcti-linux-console-usability",
            command: "make runtime-validation DESTINATION=iphonesimulator GATE=tcti-init-console-write ORLIX_SIMULATOR_ID=\(requiredSimulatorID) ORLIX_TCTI_REQUIRED_SIMULATOR_ID=\(requiredSimulatorID) ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=\(requiredSimulatorName)",
            kind: "simulator-runtime",
            prerequisites: ["no-phone-tcti-post-console-sh-sigabrt-reducer"],
            allowedScope: [
                "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/**",
                "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/mm/**",
                "OrlixOS/Sources/make/**",
                "tools/runtime/orlix-runtime-validation.sh",
                "tools/tcti/orlix-tcti-gate.swift",
                "tools/tcti/fixtures/**",
                ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                ".agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json",
                "docs/plans/active/orlix-tcti/PLAN.md",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Do not run physical-device gates.",
                "Do not use any simulator except Orlix-iPhone-15-Pro-Max (ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3).",
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
                "rtk proxy make runtime-validation DESTINATION=iphonesimulator GATE=tcti-init-console-write ORLIX_SIMULATOR_ID=\(requiredSimulatorID) ORLIX_TCTI_REQUIRED_SIMULATOR_ID=\(requiredSimulatorID) ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=\(requiredSimulatorName)",
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
                "The latest static BusyBox simulator failure must be current for HEAD and pinned to Orlix-iPhone-15-Pro-Max (ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3).",
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
            id: "tcti-user-data-window-refresh-fix",
            command: "make tcti-gate TARGET=tcti-user-data-window-refresh-fix",
            kind: "production-tcti-fix",
            prerequisites: ["no-phone-tcti-post-busybox-sigabrt-reducer"],
            allowedScope: [
                "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/mm/tcti_user_page.c",
                "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/tests/**",
                "tools/tcti/orlix-tcti-gate.swift",
                ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                ".agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Do not run phone gates.",
                "Do not run simulator gates as proof for this no-phone fix gate.",
                "Do not add production assembly.",
                "Do not add gadget dispatch.",
                "Do not flip product defconfigs.",
                "Do not edit generated Linux or build trees.",
                "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or broad Linux runtime semantics.",
                "Do not make TCTI data access depend on native hosted executable-window refresh.",
                "Do not claim simulator static BusyBox start until the simulator gate passes after this no-phone fix.",
            ],
            expectedReportPaths: [
                "Build/TCTI/reports/tcti-user-data-window-refresh-fix/report.json",
                "Build/TCTI/reports/tcti-post-busybox-sigabrt-reducer/report.json",
                "Build/TCTI/reports/tcti-repro/report.json",
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
                "rtk proxy make tcti-gate TARGET=tcti-user-data-window-refresh-fix",
                "rtk proxy make agent-harness-check",
                "rtk proxy make agent-status AREA=orlix-tcti",
                "rtk proxy make agent-next AREA=orlix-tcti",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
                "rtk proxy make tcti-gate TARGET=tcti-plan-consistency",
                "rtk proxy make tcti-gate TARGET=tcti-report-schema-check",
                "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
            ],
            reducerRequirements: [
                "The post-BusyBox SIGABRT reducer must pass for the current git SHA before this fix gate passes.",
                "The fix gate must not run simulator or phone gates; simulator proof happens in simulator-tcti-static-busybox-start after the no-phone fix gate passes.",
            ],
            requiredSubagentsOrSkills: [
                "orlix-tcti-safety",
                "orlix-tcti-reproducer",
                "orlix-tcti-debug",
                "tcti-planner",
                "tcti-safety-reviewer",
                "tcti-test-reducer",
                "tcti-release-gate-reviewer",
            ],
            commitMessageTemplate: "fix(tcti): keep data reads off hosted window refresh",
            stopConditions: [
                "Stop if the BusyBox SIGABRT reducer report is missing, stale, or failing.",
                "Stop if the fix requires HostAdapter Linux behavior, Darwin guest syscall behavior, production assembly, gadget dispatch, or generated-tree edits.",
                "Stop if simulator static BusyBox start is claimed before the pinned simulator gate reruns and passes.",
            ]
        ),
        Gate(
            id: "tcti-busybox-syscall-return-trace",
            command: "make tcti-gate TARGET=tcti-busybox-syscall-return-trace",
            kind: "production-tcti-diagnostic",
            prerequisites: ["tcti-user-data-window-refresh-fix"],
            allowedScope: [
                "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/engine.c",
                "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/report.c",
                "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/report.h",
                "tools/runtime/orlix-runtime-validation.sh",
                "tools/tcti/orlix-tcti-gate.swift",
                ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                ".agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Do not run phone gates.",
                "Do not add production assembly.",
                "Do not add gadget dispatch.",
                "Do not flip product defconfigs.",
                "Do not edit generated Linux or build trees.",
                "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or broad Linux runtime semantics.",
                "Do not change syscall semantics in this diagnostic gate.",
                "Do not claim simulator static BusyBox start until the simulator gate reruns and passes with the return trace.",
            ],
            expectedReportPaths: [
                "Build/TCTI/reports/tcti-busybox-syscall-return-trace/report.json",
                "Build/TCTI/reports/tcti-user-data-window-refresh-fix/report.json",
                "Build/Reports/runtime/tcti-static-busybox-start-*.json",
            ],
            readinessEligible: false,
            physicalDevice: false,
            gadget: false,
            requiredValidationCommands: [
                "rtk proxy git diff --check",
                "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
                "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "rtk proxy make tcti-gate TARGET=tcti-user-data-window-refresh-fix",
                "rtk proxy make tcti-gate TARGET=tcti-busybox-syscall-return-trace",
                "rtk proxy make agent-harness-check",
                "rtk proxy make agent-status AREA=orlix-tcti",
                "rtk proxy make agent-next AREA=orlix-tcti",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
                "rtk proxy make tcti-gate TARGET=tcti-plan-consistency",
                "rtk proxy make tcti-gate TARGET=tcti-report-schema-check",
                "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
            ],
            reducerRequirements: [
                "The latest static BusyBox simulator failure must remain captured as JSON before this diagnostic gate.",
                "The next simulator static BusyBox run must expose BusyBox shell syscall returns in runtime JSON if it still fails.",
            ],
            requiredSubagentsOrSkills: [
                "orlix-tcti-safety",
                "orlix-tcti-debug",
                "tcti-planner",
                "tcti-safety-reviewer",
                "tcti-llvm-inspector",
                "tcti-test-reducer",
                "tcti-release-gate-reviewer",
            ],
            commitMessageTemplate: "test(tcti): trace busybox syscall returns",
            stopConditions: [
                "Stop if return tracing changes syscall behavior instead of reporting it.",
                "Stop if the diagnostic adds HostAdapter Linux behavior, Darwin guest syscall behavior, production assembly, gadget dispatch, or generated-tree edits.",
                "Stop if simulator static BusyBox start is claimed before the pinned simulator gate reruns and passes.",
            ]
        ),
        Gate(
            id: "no-phone-tcti-post-busybox-shell-command-sigill-reducer",
            command: "make tcti-gate TARGET=tcti-post-busybox-shell-command-sigill-reducer",
            kind: "no-phone-reducer",
            prerequisites: ["tcti-user-data-window-refresh-fix"],
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
                "Only bind the current pinned simulator BusyBox marker-then-SIGILL report to a replayable no-phone reducer.",
            ],
            expectedReportPaths: [
                "Build/TCTI/reports/tcti-post-busybox-shell-command-sigill-reducer/report.json",
                "Build/TCTI/reproducers/tcti-post-busybox-shell-command-sigill-reducer/post-busybox-shell-command-sigill-after-marker-pass-regression.json",
                "Build/Reports/runtime/tcti-static-busybox-shell-command-*.json",
            ],
            readinessEligible: false,
            physicalDevice: false,
            gadget: false,
            requiredValidationCommands: [
                "rtk proxy git diff --check",
                "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
                "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "rtk proxy make tcti-gate TARGET=tcti-post-busybox-shell-command-sigill-reducer",
                "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-post-busybox-shell-command-sigill-reducer/post-busybox-shell-command-sigill-after-marker-pass-regression.json",
                "rtk proxy make agent-harness-check",
                "rtk proxy make agent-status AREA=orlix-tcti",
                "rtk proxy make agent-next AREA=orlix-tcti",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
                "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
            ],
            reducerRequirements: [
                "The latest static BusyBox shell-command simulator failure must be current for HEAD and pinned to Orlix-iPhone-15-Pro-Max (ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3).",
                "The report must show the ORLIX-TCTI-BUSYBOX-USABLE marker before unsupported instruction SIGILL signal=4.",
                "The reducer must replay without running simulator or phone gates.",
            ],
            requiredSubagentsOrSkills: [
                "orlix-tcti-reproducer",
                "orlix-tcti-debug",
                "orlix-tcti-safety",
                "tcti-planner",
                "tcti-safety-reviewer",
                "tcti-test-reducer",
                "tcti-release-gate-reviewer",
            ],
            commitMessageTemplate: "test(tcti): reduce busybox shell command sigill",
            stopConditions: [
                "Stop if no current static BusyBox shell-command SIGILL report exists.",
                "Stop if reducer replay cannot reproduce the report-backed failure shape.",
                "Stop if production TCTI code would be required before the reducer exists.",
            ]
        ),
        Gate(
            id: "simulator-tcti-static-busybox-start",
            command: "make runtime-validation DESTINATION=iphonesimulator GATE=tcti-static-busybox-start ORLIX_SIMULATOR_ID=\(requiredSimulatorID) ORLIX_TCTI_REQUIRED_SIMULATOR_ID=\(requiredSimulatorID) ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=\(requiredSimulatorName)",
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
                "Do not use any simulator except Orlix-iPhone-15-Pro-Max (ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3).",
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
                "rtk proxy make runtime-validation DESTINATION=iphonesimulator GATE=tcti-static-busybox-start ORLIX_SIMULATOR_ID=\(requiredSimulatorID) ORLIX_TCTI_REQUIRED_SIMULATOR_ID=\(requiredSimulatorID) ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=\(requiredSimulatorName)",
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
        Gate(
            id: "simulator-tcti-static-busybox-shell-command",
            command: "make runtime-validation DESTINATION=iphonesimulator GATE=tcti-static-busybox-shell-command ORLIX_SIMULATOR_ID=\(requiredSimulatorID) ORLIX_TCTI_REQUIRED_SIMULATOR_ID=\(requiredSimulatorID) ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=\(requiredSimulatorName)",
            kind: "simulator-runtime",
            prerequisites: ["simulator-tcti-static-busybox-start", "tcti-simd-umaxv-4s-fix"],
            allowedScope: [
                "tools/runtime/orlix-runtime-validation.sh",
                ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                ".agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json",
                "docs/plans/active/orlix-tcti/PLAN.md",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Do not run phone gates.",
                "Do not use any simulator except Orlix-iPhone-15-Pro-Max.",
                "Do not treat shell start or one shell command as full shell usability.",
                "Do not claim package behavior, dynamic loader support, signals, VFS completeness, or full Linux runtime readiness from this gate.",
                "Do not add production assembly.",
                "Do not add gadget dispatch.",
                "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or broad Linux runtime semantics.",
                "Do not edit generated Linux or build trees.",
            ],
            expectedReportPaths: [
                "Build/Reports/runtime/tcti-static-busybox-shell-command-*.json",
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
                "rtk proxy make runtime-validation DESTINATION=iphonesimulator GATE=tcti-static-busybox-shell-command ORLIX_SIMULATOR_ID=\(requiredSimulatorID) ORLIX_TCTI_REQUIRED_SIMULATOR_ID=\(requiredSimulatorID) ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=\(requiredSimulatorName)",
            ],
            reducerRequirements: [
                "If the BusyBox shell command fails, reduce it into a no-phone TCTI fixture or report-specific reducer before patching production behavior.",
            ],
            requiredSubagentsOrSkills: [
                "orlix-tcti-safety",
                "orlix-tcti-debug",
                "tcti-planner",
                "tcti-safety-reviewer",
                "tcti-llvm-inspector",
                "tcti-test-reducer",
                "tcti-release-gate-reviewer",
            ],
            commitMessageTemplate: "test(tcti): prove static busybox shell command on simulator",
            stopConditions: [
                "Stop if the marker ORLIX-TCTI-BUSYBOX-USABLE is missing.",
                "Stop if more than the pinned simulator is booted.",
                "Stop if the gate is used to claim full Linux usability, release readiness, or phone readiness.",
            ]
        ),
        Gate(
            id: "no-phone-tcti-post-static-pie-init-read-fault-reducer",
            command: "make tcti-gate TARGET=tcti-post-static-pie-init-read-fault-reducer",
            kind: "no-phone-reducer",
            prerequisites: ["simulator-tcti-static-busybox-shell-command"],
            allowedScope: [
                "tools/tcti/orlix-tcti-gate.swift",
                ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                ".agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Do not run phone gates.",
                "Do not run simulator gates while reducing this report-backed failure.",
                "Do not add production assembly.",
                "Do not add gadget dispatch.",
                "Do not flip product defconfigs.",
                "Do not edit generated Linux or build trees.",
                "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or broad Linux runtime semantics.",
                "Only bind the current pinned simulator post-static-PIE init read-fault report to a replayable no-phone reducer.",
            ],
		expectedReportPaths: [
			"Build/TCTI/reports/tcti-post-static-pie-init-read-fault-reducer/report.json",
			"Build/TCTI/reproducers/tcti-post-static-pie-init-read-fault-reducer/post-static-pie-init-read-fault-pass-regression.json",
			"Build/Reports/runtime/tcti-static-busybox-shell-command-*.json",
			"Build/Reports/runtime/tcti-full-shell-usability-*.json",
		],
            readinessEligible: false,
            physicalDevice: false,
            gadget: false,
            requiredValidationCommands: [
                "rtk proxy git diff --check",
                "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
                "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "rtk proxy make tcti-gate TARGET=tcti-post-static-pie-init-read-fault-reducer",
                "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-post-static-pie-init-read-fault-reducer/post-static-pie-init-read-fault-pass-regression.json",
                "rtk proxy make agent-harness-check",
                "rtk proxy make agent-status AREA=orlix-tcti",
                "rtk proxy make agent-next AREA=orlix-tcti",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
                "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
            ],
            reducerRequirements: [
                "The latest full-shell simulator failure must be current for HEAD and pinned to Orlix-iPhone-15-Pro-Max (ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3).",
                "The simulator report must show init first-svc syscall 178, static PIE image task=init, then a non-null init read fault.",
                "The reducer must replay without running simulator or phone gates.",
            ],
            requiredSubagentsOrSkills: [
                "orlix-tcti-reproducer",
                "orlix-tcti-safety",
                "orlix-tcti-debug",
                "tcti-planner",
                "tcti-safety-reviewer",
                "tcti-test-reducer",
                "tcti-release-gate-reviewer",
            ],
            commitMessageTemplate: "test(tcti): reduce post static pie init read fault",
            stopConditions: [
                "Stop if no current pinned simulator post-static-PIE init read-fault report exists.",
                "Stop if reducer replay cannot reproduce the report-backed failure shape.",
                "Stop if production TCTI code would be required before the reducer exists.",
            ]
        ),
        Gate(
            id: "tcti-post-static-pie-init-tls-fix",
            command: "make tcti-gate TARGET=tcti-post-static-pie-init-tls-fix",
            kind: "production-tcti-fix",
            prerequisites: ["no-phone-tcti-post-static-pie-init-read-fault-reducer"],
            allowedScope: [
                "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/**",
                "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/kernel/**",
                "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/include/asm/**",
                "tools/tcti/orlix-tcti-gate.swift",
                ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Do not run physical-device gates.",
                "Do not run simulator gates as proof for this no-phone fix gate.",
                "Do not add production assembly.",
                "Do not add gadget dispatch.",
                "Do not flip product defconfigs.",
                "Do not edit generated Linux or build trees.",
                "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or broad Linux runtime semantics.",
                "Do not use host TPIDR_EL0 as guest state.",
                "Do not claim full-shell usability until the pinned simulator full-shell gate reruns and passes.",
            ],
            expectedReportPaths: [
                "Build/TCTI/reports/tcti-post-static-pie-init-tls-fix/report.json",
                "Build/TCTI/reports/tcti-post-static-pie-init-read-fault-reducer/report.json",
                "Build/Reports/runtime/tcti-full-shell-usability-*.json",
            ],
            readinessEligible: false,
            physicalDevice: false,
            gadget: false,
            requiredValidationCommands: [
                "rtk proxy git diff --check",
                "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
                "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "rtk proxy make tcti-gate TARGET=tcti-post-static-pie-init-read-fault-reducer",
                "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-post-static-pie-init-read-fault-reducer/post-static-pie-init-read-fault-pass-regression.json",
                "rtk proxy make tcti-gate TARGET=tcti-post-static-pie-init-tls-fix",
                "rtk proxy make agent-harness-check",
                "rtk proxy make agent-status AREA=orlix-tcti",
                "rtk proxy make agent-next AREA=orlix-tcti",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
                "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
                "rtk test env PATH=\"$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin\" make -f OrlixKernel/Makefile kunit PROFILE=development",
                "rtk test env PATH=\"$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin\" make -f OrlixKernel/Makefile kunit PROFILE=release",
            ],
            reducerRequirements: [
                "The post-static-PIE init read-fault reducer must pass for the current report before runtime code changes.",
                "The fix gate must emit a JSON report and must not use simulator success as its own no-phone pass criterion.",
                "After this fix gate passes, rerun simulator full-shell usability on the pinned simulator through agent-next.",
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
            commitMessageTemplate: "fix(tcti): initialize guest tls for static pie init",
            stopConditions: [
                "Stop if the reducer report is missing, stale, failing, or does not cover the current simulator report.",
                "Stop if the fix requires host TPIDR_EL0, HostAdapter Linux behavior, Darwin guest syscall behavior, production assembly, gadget dispatch, product defconfig flips, or generated-tree edits.",
                "Stop if full-shell usability is claimed before the pinned simulator gate reruns and passes.",
            ]
        ),
        Gate(
            id: "no-phone-tcti-post-full-shell-cat-read-fault-reducer",
            command: "make tcti-gate TARGET=tcti-post-full-shell-cat-read-fault-reducer",
            kind: "no-phone-reducer",
            prerequisites: ["tcti-post-static-pie-init-tls-fix"],
            allowedScope: [
                "tools/tcti/orlix-tcti-gate.swift",
                ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Do not run physical-device gates.",
                "Do not run simulator gates as proof for this no-phone reducer gate.",
                "Do not add production assembly.",
                "Do not add gadget dispatch.",
                "Do not flip product defconfigs.",
                "Do not edit generated Linux or build trees.",
                "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or broad Linux runtime semantics.",
                "Do not patch the cat user-data read-fault runtime behavior before the reducer exists and replays.",
                "Use only Orlix-iPhone-15-Pro-Max (ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3) for simulator evidence.",
            ],
            expectedReportPaths: [
                "Build/TCTI/reports/tcti-post-full-shell-cat-read-fault-reducer/report.json",
                "Build/TCTI/reproducers/tcti-post-full-shell-cat-read-fault-reducer/post-full-shell-cat-read-fault-pass-regression.json",
                "Build/Reports/runtime/tcti-full-shell-usability-*.json",
            ],
            readinessEligible: false,
            physicalDevice: false,
            gadget: false,
            requiredValidationCommands: [
                "rtk proxy git diff --check",
                "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
                "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "rtk proxy make tcti-gate TARGET=tcti-post-full-shell-cat-read-fault-reducer",
                "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-post-full-shell-cat-read-fault-reducer/post-full-shell-cat-read-fault-pass-regression.json",
                "rtk proxy make agent-harness-check",
                "rtk proxy make agent-status AREA=orlix-tcti",
                "rtk proxy make agent-next AREA=orlix-tcti",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
                "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
            ],
            reducerRequirements: [
                "The latest full-shell simulator report must be current for HEAD and pinned to Orlix-iPhone-15-Pro-Max.",
                "The report must show the shell reached cat /tmp/orlix-tcti-shell, then cat faulted on a non-null user-data read and the shell exited 139 before ORLIX-TCTI-SHELL-USABLE.",
                "The reducer must replay before production TCTI patching.",
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
            commitMessageTemplate: "test(tcti): reduce full shell cat read fault",
            stopConditions: [
                "Stop if the latest full-shell simulator report is stale or no longer matches the cat user-data read-fault signature.",
                "Stop if more than the pinned Orlix-iPhone-15-Pro-Max simulator is booted.",
                "Stop if reducer replay cannot reproduce the no-phone evidence gate.",
            ]
        ),
        Gate(
            id: "tcti-post-full-shell-cat-read-fault-fix",
            command: "make tcti-gate TARGET=tcti-post-full-shell-cat-read-fault-fix",
            kind: "no-phone-fix",
            prerequisites: ["no-phone-tcti-post-full-shell-cat-read-fault-reducer"],
            allowedScope: [
                "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/engine.c",
                "tools/tcti/orlix-tcti-gate.swift",
                ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Do not run physical-device gates.",
                "Do not treat this no-phone fix gate as full shell usability proof.",
                "Do not add production assembly.",
                "Do not add gadget dispatch.",
                "Do not flip product defconfigs.",
                "Do not edit generated Linux or build trees.",
                "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or broad Linux runtime semantics.",
                "Do not call host read/write or implement Linux syscall behavior in TCTI.",
                "Use only Orlix-iPhone-15-Pro-Max (ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3) for subsequent simulator evidence.",
            ],
            expectedReportPaths: [
                "Build/TCTI/reports/tcti-post-full-shell-cat-read-fault-fix/report.json",
                "Build/TCTI/reports/tcti-post-full-shell-cat-read-fault-reducer/report.json",
                "Build/TCTI/reproducers/tcti-post-full-shell-cat-read-fault-reducer/post-full-shell-cat-read-fault-pass-regression.json",
                "Build/Reports/runtime/tcti-full-shell-usability-*.json",
            ],
            readinessEligible: false,
            physicalDevice: false,
            gadget: false,
            requiredValidationCommands: [
                "rtk proxy git diff --check",
                "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
                "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "rtk proxy make tcti-gate TARGET=tcti-post-full-shell-cat-read-fault-reducer",
                "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-post-full-shell-cat-read-fault-reducer/post-full-shell-cat-read-fault-pass-regression.json",
                "rtk proxy make tcti-gate TARGET=tcti-post-full-shell-cat-read-fault-fix",
                "rtk proxy make agent-harness-check",
                "rtk proxy make agent-status AREA=orlix-tcti",
                "rtk proxy make agent-next AREA=orlix-tcti",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
                "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
                "rtk test env PATH=\"$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin\" make -f OrlixKernel/Makefile kunit PROFILE=development",
                "rtk test env PATH=\"$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin\" make -f OrlixKernel/Makefile kunit PROFILE=release",
            ],
            reducerRequirements: [
                "The cat read-fault reducer must pass for the current pinned simulator report before runtime code changes.",
                "The fix gate must prove TCTI refreshes the guest read buffer after successful read(2) without adding Linux runtime semantics.",
                "After this fix gate passes, rerun simulator full-shell usability on the pinned simulator through agent-next.",
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
            commitMessageTemplate: "fix(tcti): refresh read buffer after syscall return",
            stopConditions: [
                "Stop if the reducer report is missing, stale, failing, or does not cover the current simulator report.",
                "Stop if the fix requires HostAdapter Linux behavior, Darwin guest syscall behavior, VFS, fd tables, process model, signal handling, scheduler behavior, production assembly, gadget dispatch, product defconfig flips, or generated-tree edits.",
                "Stop if full-shell usability is claimed before the pinned simulator gate reruns and passes.",
            ]
        ),
        Gate(
            id: "no-phone-tcti-post-full-shell-init-write-fault-reducer",
            command: "make tcti-gate TARGET=tcti-post-full-shell-init-write-fault-reducer",
            kind: "no-phone-reducer",
            prerequisites: ["tcti-post-full-shell-cat-read-fault-fix"],
            allowedScope: [
                "tools/tcti/orlix-tcti-gate.swift",
                ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Do not run physical-device gates.",
                "Do not run simulator gates as proof for this no-phone reducer gate.",
                "Do not add production assembly.",
                "Do not add gadget dispatch.",
                "Do not flip product defconfigs.",
                "Do not edit generated Linux or build trees.",
                "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or broad Linux runtime semantics.",
                "Do not patch the init user-data write-fault runtime behavior before the reducer exists and replays.",
                "Use only Orlix-iPhone-15-Pro-Max (ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3) for simulator evidence.",
            ],
            expectedReportPaths: [
                "Build/TCTI/reports/tcti-post-full-shell-init-write-fault-reducer/report.json",
                "Build/TCTI/reproducers/tcti-post-full-shell-init-write-fault-reducer/post-full-shell-init-write-fault-pass-regression.json",
                "Build/Reports/runtime/tcti-full-shell-usability-*.json",
            ],
            readinessEligible: false,
            physicalDevice: false,
            gadget: false,
            requiredValidationCommands: [
                "rtk proxy git diff --check",
                "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
                "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "rtk proxy make tcti-gate TARGET=tcti-post-full-shell-init-write-fault-reducer",
                "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-post-full-shell-init-write-fault-reducer/post-full-shell-init-write-fault-pass-regression.json",
                "rtk proxy make agent-harness-check",
                "rtk proxy make agent-status AREA=orlix-tcti",
                "rtk proxy make agent-next AREA=orlix-tcti",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
                "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
            ],
            reducerRequirements: [
                "The latest full-shell simulator report must be current for HEAD and pinned to Orlix-iPhone-15-Pro-Max.",
                "The report must show init pid=1 reached first TCTI svc syscall 178, entered a static PIE image, issued mmap syscall 222, then faulted a non-null user-data write with access=2 si=2 before ORLIX-TCTI-SHELL-USABLE.",
                "The reducer must replay before production TCTI patching.",
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
            commitMessageTemplate: "test(tcti): reduce full shell init write fault",
            stopConditions: [
                "Stop if the latest full-shell simulator report is stale or no longer matches the init user-data write-fault signature.",
                "Stop if more than the pinned Orlix-iPhone-15-Pro-Max simulator is booted.",
                "Stop if reducer replay cannot reproduce the no-phone evidence gate.",
            ]
        ),
        Gate(
            id: "tcti-post-full-shell-init-write-fault-fix",
            command: "make tcti-gate TARGET=tcti-post-full-shell-init-write-fault-fix",
            kind: "no-phone-fix",
            prerequisites: ["no-phone-tcti-post-full-shell-init-write-fault-reducer"],
            allowedScope: [
                "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/mm/fault.c",
                "tools/tcti/orlix-tcti-gate.swift",
                ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Do not run physical-device gates.",
                "Do not treat this no-phone fix gate as simulator full-shell success.",
                "Do not add production assembly.",
                "Do not add gadget dispatch.",
                "Do not flip product defconfigs.",
                "Do not edit generated Linux or build trees.",
                "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or broad Linux runtime semantics.",
                "Do not map guest text host-executable or add JIT/MAP_JIT/RWX/prot_exec behavior.",
            ],
            expectedReportPaths: [
                "Build/TCTI/reports/tcti-post-full-shell-init-write-fault-fix/report.json",
                "Build/TCTI/reports/tcti-post-full-shell-init-write-fault-reducer/report.json",
                "Build/Reports/runtime/tcti-full-shell-usability-*.json",
            ],
            readinessEligible: false,
            physicalDevice: false,
            gadget: false,
            requiredValidationCommands: [
                "rtk proxy git diff --check",
                "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
                "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "rtk proxy make tcti-gate TARGET=tcti-post-full-shell-init-write-fault-reducer",
                "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-post-full-shell-init-write-fault-reducer/post-full-shell-init-write-fault-pass-regression.json",
                "rtk proxy make tcti-gate TARGET=tcti-post-full-shell-init-write-fault-fix",
                "rtk proxy make agent-harness-check",
                "rtk proxy make agent-status AREA=orlix-tcti",
                "rtk proxy make agent-next AREA=orlix-tcti",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
                "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
                "rtk test env PATH=\"$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin\" make -f OrlixKernel/Makefile kunit PROFILE=development",
                "rtk test env PATH=\"$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin\" make -f OrlixKernel/Makefile kunit PROFILE=release",
            ],
            reducerRequirements: [
                "The init write-fault reducer must pass for the current report before runtime code changes.",
                "The fix gate must emit JSON and must not use simulator success as its own no-phone pass criterion.",
                "After this fix gate passes, rerun simulator full-shell usability on the pinned simulator through agent-next.",
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
            commitMessageTemplate: "fix(tcti): sync write fault windows for full shell",
            stopConditions: [
                "Stop if the reducer report is missing, stale, failing, or does not cover the current simulator report.",
                "Stop if the fix requires HostAdapter Linux behavior, Darwin guest syscall behavior, production assembly, gadget dispatch, product defconfig flips, generated-tree edits, or executable guest text mappings.",
                "Stop if full-shell usability is claimed before the pinned simulator gate reruns and passes.",
            ]
        ),
        Gate(
            id: "no-phone-tcti-post-full-shell-sh-sigabrt-reducer",
            command: "make tcti-gate TARGET=tcti-post-full-shell-sh-sigabrt-reducer",
            kind: "no-phone-reducer",
            prerequisites: ["tcti-post-full-shell-init-write-fault-fix"],
            allowedScope: [
                "tools/tcti/orlix-tcti-gate.swift",
                ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Do not run physical-device gates.",
                "Do not rerun the simulator as proof for this no-phone reducer gate.",
                "Do not add production assembly.",
                "Do not add gadget dispatch.",
                "Do not flip product defconfigs.",
                "Do not edit generated Linux or build trees.",
                "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or broad Linux runtime semantics.",
                "Do not patch the sh SIGABRT runtime behavior before the reducer exists and replays.",
                "Use only Orlix-iPhone-15-Pro-Max (ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3) for simulator evidence.",
            ],
            expectedReportPaths: [
                "Build/TCTI/reports/tcti-post-full-shell-sh-sigabrt-reducer/report.json",
                "Build/TCTI/reproducers/tcti-post-full-shell-sh-sigabrt-reducer/post-full-shell-sh-sigabrt-pass-regression.json",
                "Build/Reports/runtime/tcti-full-shell-usability-*.json",
            ],
            readinessEligible: false,
            physicalDevice: false,
            gadget: false,
            requiredValidationCommands: [
                "rtk proxy git diff --check",
                "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
                "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "rtk proxy make tcti-gate TARGET=tcti-post-full-shell-sh-sigabrt-reducer",
                "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-post-full-shell-sh-sigabrt-reducer/post-full-shell-sh-sigabrt-pass-regression.json",
                "rtk proxy make agent-harness-check",
                "rtk proxy make agent-status AREA=orlix-tcti",
                "rtk proxy make agent-next AREA=orlix-tcti",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
                "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
            ],
            reducerRequirements: [
                "The latest full-shell simulator report must be current for HEAD and pinned to Orlix-iPhone-15-Pro-Max.",
                "The report must show sh static PIE entry 0x45418, mmap syscall 222, uname syscall 160, syscall 172 returning the sh pid, then syscall 129 with the same pid and signal 6 before ORLIX-TCTI-SHELL-USABLE.",
                "The reducer must replay before production TCTI patching.",
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
            commitMessageTemplate: "test(tcti): reduce full shell sh sigabrt",
            stopConditions: [
                "Stop if the latest full-shell simulator report is stale or no longer matches the sh SIGABRT signature.",
                "Stop if more than the pinned Orlix-iPhone-15-Pro-Max simulator is booted.",
                "Stop if reducer replay cannot reproduce the no-phone evidence gate.",
            ]
        ),
        Gate(
            id: "tcti-post-full-shell-sh-sigabrt-fix",
            command: "make tcti-gate TARGET=tcti-post-full-shell-sh-sigabrt-fix",
            kind: "no-phone-fix",
            prerequisites: [
                "no-phone-tcti-post-full-shell-sh-sigabrt-reducer",
            ],
            allowedScope: [
                "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/mm/tcti_user_page.c",
                "tools/tcti/orlix-tcti-gate.swift",
                ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Do not run physical-device gates.",
                "Do not add HostAdapter Linux behavior.",
                "Do not call Darwin syscalls as guest side effects.",
                "Do not implement VFS, fd table, process, signal, scheduler, or Linux runtime semantics in the harness.",
                "Do not add production TCTI assembly or gadget dispatch.",
                "Do not edit generated Linux trees.",
                "Do not add MAP_JIT, JIT, RWX, vm_protect EXECUTE, or host-executable guest text.",
            ],
            expectedReportPaths: [
                "Build/TCTI/reports/tcti-post-full-shell-sh-sigabrt-fix/report.json",
                "Build/TCTI/reports/tcti-post-full-shell-sh-sigabrt-reducer/report.json",
                "Build/TCTI/reproducers/tcti-post-full-shell-sh-sigabrt-reducer/post-full-shell-sh-sigabrt-pass-regression.json",
                "Build/Reports/runtime/tcti-full-shell-usability-*.json",
            ],
            readinessEligible: false,
            physicalDevice: false,
            gadget: false,
            requiredValidationCommands: [
                "rtk proxy git diff --check",
                "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
                "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "rtk proxy make tcti-gate TARGET=tcti-post-full-shell-sh-sigabrt-reducer",
                "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-post-full-shell-sh-sigabrt-reducer/post-full-shell-sh-sigabrt-pass-regression.json",
                "rtk proxy make tcti-gate TARGET=tcti-post-full-shell-sh-sigabrt-fix",
                "rtk proxy make agent-harness-check",
                "rtk proxy make agent-status AREA=orlix-tcti",
                "rtk proxy make agent-next AREA=orlix-tcti",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
                "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
                "rtk test env PATH=\"$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin\" make -f OrlixKernel/Makefile kunit PROFILE=development",
                "rtk test env PATH=\"$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin\" make -f OrlixKernel/Makefile kunit PROFILE=release",
            ],
            reducerRequirements: [
                "The sh SIGABRT reducer must pass for the current pinned simulator report before runtime code changes.",
                "The fix gate must emit JSON and must not treat simulator success as its own no-phone pass criterion.",
                "After fix gate passes, rerun simulator full-shell usability on the pinned simulator through agent-next.",
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
            commitMessageTemplate: "fix(tcti): preserve fault window access class",
            stopConditions: [
                "Stop if reducer report is missing, stale, failing, or does not cover the current simulator report.",
                "Stop if the fix requires HostAdapter Linux behavior or Darwin guest syscall behavior.",
                "Stop if the fix requires physical-device execution.",
            ]
        ),
        Gate(
            id: "no-phone-tcti-post-full-shell-sh-read-fault-reducer",
            command: "make tcti-gate TARGET=tcti-post-sh-read-fault-reducer",
            kind: "no-phone-reducer",
            prerequisites: [
                "tcti-post-full-shell-sh-sigabrt-fix",
            ],
            allowedScope: [
                "tools/tcti/orlix-tcti-gate.swift",
                ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Do not run physical-device gates.",
                "Do not rerun the simulator as proof for this no-phone reducer gate.",
                "Do not add production assembly.",
                "Do not add gadget dispatch.",
                "Do not flip product defconfigs.",
                "Do not edit generated Linux or build trees.",
                "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or broad Linux runtime semantics.",
                "Do not patch the sh read-fault runtime behavior before the reducer exists and replays.",
                "Use only Orlix-iPhone-15-Pro-Max (ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3) for simulator evidence.",
            ],
            expectedReportPaths: [
                "Build/TCTI/reports/tcti-post-sh-read-fault-reducer/report.json",
                "Build/TCTI/reproducers/tcti-post-sh-read-fault-reducer/post-sh-read-fault-pass-regression.json",
                "Build/Reports/runtime/tcti-full-shell-usability-*.json",
            ],
            readinessEligible: false,
            physicalDevice: false,
            gadget: false,
            requiredValidationCommands: [
                "rtk proxy git diff --check",
                "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
                "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "rtk proxy make tcti-gate TARGET=tcti-post-sh-read-fault-reducer",
                "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-post-sh-read-fault-reducer/post-sh-read-fault-pass-regression.json",
                "rtk proxy make agent-harness-check",
                "rtk proxy make agent-status AREA=orlix-tcti",
                "rtk proxy make agent-next AREA=orlix-tcti",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
                "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
            ],
            reducerRequirements: [
                "The latest full-shell simulator report must be current for HEAD and pinned to Orlix-iPhone-15-Pro-Max.",
                "The report must show sh static PIE, mmap syscall 222, a nonzero sh user-data read fault, and signal 11 before ORLIX-TCTI-SHELL-USABLE.",
                "The reducer must replay before production TCTI patching.",
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
            commitMessageTemplate: "test(tcti): reduce full shell sh read fault",
            stopConditions: [
                "Stop if the latest full-shell simulator report is stale or no longer matches the sh read-fault signature.",
                "Stop if more than the pinned Orlix-iPhone-15-Pro-Max simulator is booted.",
                "Stop if reducer replay cannot reproduce the no-phone evidence gate.",
            ]
        ),
        Gate(
            id: "no-phone-tcti-post-full-shell-init-second-mmap-hang-reducer",
            command: "make tcti-gate TARGET=tcti-post-full-shell-init-second-mmap-hang-reducer",
            kind: "no-phone-reducer",
            prerequisites: [
                "no-phone-tcti-post-full-shell-sh-read-fault-reducer",
            ],
            allowedScope: [
                "tools/tcti/orlix-tcti-gate.swift",
                ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Do not patch runtime behavior before this reducer exists and replays.",
                "Do not run physical-device gates.",
                "Do not add production TCTI assembly or gadget dispatch.",
                "Do not add HostAdapter Linux behavior or Darwin guest syscall behavior.",
                "Do not implement VFS, fd table, process, signal, scheduler, or Linux runtime semantics in the harness.",
            ],
            expectedReportPaths: [
                "Build/TCTI/reports/tcti-post-full-shell-init-second-mmap-hang-reducer/report.json",
                "Build/TCTI/reproducers/tcti-post-full-shell-init-second-mmap-hang-reducer/post-full-shell-init-second-mmap-hang-pass-regression.json",
                "Build/Reports/runtime/tcti-full-shell-usability-*.json",
            ],
            readinessEligible: false,
            physicalDevice: false,
            gadget: false,
            requiredValidationCommands: [
                "rtk proxy git diff --check",
                "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
                "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "rtk proxy make tcti-gate TARGET=tcti-post-full-shell-init-second-mmap-hang-reducer",
                "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-post-full-shell-init-second-mmap-hang-reducer/post-full-shell-init-second-mmap-hang-pass-regression.json",
                "rtk proxy make agent-harness-check",
                "rtk proxy make agent-status AREA=orlix-tcti",
                "rtk proxy make agent-next AREA=orlix-tcti",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
                "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
            ],
            reducerRequirements: [
                "The latest full-shell simulator report must be current for HEAD and pinned to Orlix-iPhone-15-Pro-Max.",
                "The report must show init static PIE, first svc syscall 178, init mmap syscall 222, no sh startup, no signal, no fatal user fault, and no ORLIX-TCTI-SHELL-USABLE marker.",
                "The reducer must replay before any further runtime patching.",
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
            commitMessageTemplate: "test(tcti): reduce full shell init mmap hang",
            stopConditions: [
                "Stop if the latest full-shell simulator report is stale or no longer matches the init second-mmap hang signature.",
                "Stop if more than the pinned Orlix-iPhone-15-Pro-Max simulator is booted.",
                "Stop if reducer replay cannot reproduce the no-phone evidence gate.",
            ]
        ),
        Gate(
            id: "tcti-post-full-shell-init-second-mmap-hang-fix",
            command: "make tcti-gate TARGET=tcti-post-full-shell-init-second-mmap-hang-fix",
            kind: "no-phone-fix",
            prerequisites: ["no-phone-tcti-post-full-shell-init-second-mmap-hang-reducer"],
            allowedScope: [
                "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/engine.c",
                "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/tests/tcti_decode_test.c",
                "tools/tcti/orlix-tcti-gate.swift",
                ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Do not run physical-device gates.",
                "Do not rerun the simulator as proof for this no-phone fix gate.",
                "Do not implement BRK as a supported TCTI instruction for this assertion symptom.",
                "Do not add production assembly.",
                "Do not add gadget dispatch.",
                "Do not flip product defconfigs.",
                "Do not edit generated Linux or build trees.",
                "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or broad Linux runtime semantics.",
                "Do not map guest text host-executable or add JIT/MAP_JIT/RWX/prot_exec behavior.",
            ],
            expectedReportPaths: [
                "Build/TCTI/reports/tcti-post-full-shell-init-second-mmap-hang-fix/report.json",
                "Build/TCTI/reports/tcti-post-full-shell-init-second-mmap-hang-reducer/report.json",
                "Build/TCTI/reproducers/tcti-post-full-shell-init-second-mmap-hang-reducer/post-full-shell-init-second-mmap-hang-pass-regression.json",
                "Build/Reports/runtime/tcti-full-shell-usability-*.json",
            ],
            readinessEligible: false,
            physicalDevice: false,
            gadget: false,
            requiredValidationCommands: [
                "rtk proxy git diff --check",
                "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
                "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "rtk proxy make tcti-gate TARGET=tcti-post-full-shell-init-second-mmap-hang-reducer",
                "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-post-full-shell-init-second-mmap-hang-reducer/post-full-shell-init-second-mmap-hang-pass-regression.json",
                "rtk proxy make tcti-gate TARGET=tcti-post-full-shell-init-second-mmap-hang-fix",
                "rtk proxy make agent-harness-check",
                "rtk proxy make agent-status AREA=orlix-tcti",
                "rtk proxy make agent-next AREA=orlix-tcti",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
                "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
            ],
            reducerRequirements: [
                "The init second-mmap reducer must pass and cover the current pinned full-shell simulator report.",
                "The fix gate must prove TCTI successful execve return preserves guest TLS instead of clearing TPIDR_EL0 state.",
                "The fix gate must keep forbidden behavior false and leave physical-device work blocked.",
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
            commitMessageTemplate: "fix(tcti): preserve guest tls across execve return",
            stopConditions: [
                "Stop if the reducer does not cover the current pinned full-shell simulator report.",
                "Stop if the fix requires BRK support instead of preventing the mlibc assertion.",
                "Stop if more than the pinned Orlix-iPhone-15-Pro-Max simulator is booted.",
                "Stop if the next simulator full-shell report still lacks ORLIX-TCTI-SHELL-USABLE.",
            ]
        ),
        Gate(
            id: "no-phone-tcti-post-full-shell-cat-posix-memalign-brk-reducer",
            command: "make tcti-gate TARGET=tcti-post-full-shell-cat-posix-memalign-brk-reducer",
            kind: "no-phone-reducer",
            prerequisites: ["tcti-post-full-shell-init-second-mmap-hang-fix"],
            allowedScope: [
                "tools/tcti/orlix-tcti-gate.swift",
                ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Do not run physical-device gates.",
                "Do not rerun the simulator as proof for this no-phone reducer gate.",
                "Do not add production assembly.",
                "Do not add gadget dispatch.",
                "Do not flip product defconfigs.",
                "Do not edit generated Linux or build trees.",
                "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or broad Linux runtime semantics.",
                "Do not patch the cat posix_memalign BRK runtime behavior before the reducer exists and replays.",
                "Use only Orlix-iPhone-15-Pro-Max (ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3) for simulator evidence.",
            ],
            expectedReportPaths: [
                "Build/TCTI/reports/tcti-post-full-shell-cat-posix-memalign-brk-reducer/report.json",
                "Build/TCTI/reproducers/tcti-post-full-shell-cat-posix-memalign-brk-reducer/post-full-shell-cat-posix-memalign-brk-pass-regression.json",
                "Build/Reports/runtime/tcti-full-shell-usability-*.json",
            ],
            readinessEligible: false,
            physicalDevice: false,
            gadget: false,
            requiredValidationCommands: [
                "rtk proxy git diff --check",
                "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
                "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "rtk proxy make tcti-gate TARGET=tcti-post-full-shell-cat-posix-memalign-brk-reducer",
                "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-post-full-shell-cat-posix-memalign-brk-reducer/post-full-shell-cat-posix-memalign-brk-pass-regression.json",
                "rtk proxy make agent-harness-check",
                "rtk proxy make agent-status AREA=orlix-tcti",
                "rtk proxy make agent-next AREA=orlix-tcti",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
                "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
            ],
            reducerRequirements: [
                "The latest full-shell simulator report must be current for HEAD and pinned to Orlix-iPhone-15-Pro-Max.",
                "The report must show cat running under TCTI, an mlibc posix_memalign alignment assertion, unsupported BRK instruction 0xd4200020 in cat, shell status 132, and no ORLIX-TCTI-SHELL-USABLE marker.",
                "The reducer must replay before production TCTI patching.",
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
            commitMessageTemplate: "test(tcti): reduce full shell cat brk assertion",
            stopConditions: [
                "Stop if the latest full-shell simulator report is stale or no longer matches the cat posix_memalign BRK signature.",
                "Stop if more than the pinned Orlix-iPhone-15-Pro-Max simulator is booted.",
                "Stop if reducer replay cannot reproduce the no-phone evidence gate.",
            ]
        ),
        Gate(
            id: "tcti-post-full-shell-cat-posix-memalign-brk-fix",
            command: "make tcti-gate TARGET=tcti-post-full-shell-cat-posix-memalign-brk-fix",
            kind: "no-phone-fix",
            prerequisites: ["no-phone-tcti-post-full-shell-cat-posix-memalign-brk-reducer"],
            allowedScope: [
                "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/mm/mmap.c",
                "tools/tcti/orlix-tcti-gate.swift",
                ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "docs/plans/active/orlix-tcti/IMPLEMENT.md",
            ],
            forbiddenScope: [
                "Do not run physical-device gates.",
                "Do not rerun the simulator as proof for this no-phone fix gate.",
                "Do not implement BRK as a supported TCTI instruction for this assertion symptom.",
                "Do not add production assembly.",
                "Do not add gadget dispatch.",
                "Do not flip product defconfigs.",
                "Do not edit generated Linux or build trees.",
                "Do not add HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or broad Linux runtime semantics.",
                "Use only Orlix-iPhone-15-Pro-Max (ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3) for simulator evidence.",
            ],
            expectedReportPaths: [
                "Build/TCTI/reports/tcti-post-full-shell-cat-posix-memalign-brk-fix/report.json",
                "Build/TCTI/reports/tcti-post-full-shell-cat-posix-memalign-brk-reducer/report.json",
                "Build/TCTI/reproducers/tcti-post-full-shell-cat-posix-memalign-brk-reducer/post-full-shell-cat-posix-memalign-brk-pass-regression.json",
                "Build/Reports/runtime/tcti-full-shell-usability-*.json",
            ],
            readinessEligible: false,
            physicalDevice: false,
            gadget: false,
            requiredValidationCommands: [
                "rtk proxy git diff --check",
                "rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift",
                "rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift",
                "rtk proxy make tcti-gate TARGET=tcti-post-full-shell-cat-posix-memalign-brk-reducer",
                "rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-post-full-shell-cat-posix-memalign-brk-reducer/post-full-shell-cat-posix-memalign-brk-pass-regression.json",
                "rtk proxy make tcti-gate TARGET=tcti-post-full-shell-cat-posix-memalign-brk-fix",
                "rtk proxy make agent-harness-check",
                "rtk proxy make agent-status AREA=orlix-tcti",
                "rtk proxy make agent-next AREA=orlix-tcti",
                "rtk proxy make agent-task-envelope-check AREA=orlix-tcti",
                "rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit",
            ],
            reducerRequirements: [
                "The cat posix_memalign BRK reducer must pass and cover the current pinned full-shell simulator report.",
                "The fix gate must prove hosted anonymous mmap selection preserves host-page alignment without implementing BRK as a supported instruction.",
                "The fix gate must keep forbidden behavior false and leave physical-device work blocked.",
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
            commitMessageTemplate: "fix(tcti): preserve mmap alignment for full shell cat",
            stopConditions: [
                "Stop if the reducer does not cover the current pinned full-shell simulator report.",
                "Stop if the fix requires BRK support instead of preventing the mlibc assertion.",
                "Stop if more than the pinned Orlix-iPhone-15-Pro-Max simulator is booted.",
                "Stop if the next simulator full-shell report still lacks ORLIX-TCTI-SHELL-USABLE.",
            ]
        ),
        simulatorProofGate(
            id: "simulator-tcti-full-shell-usability",
            runtimeGate: "tcti-full-shell-usability",
            marker: "ORLIX-TCTI-SHELL-USABLE",
            prerequisite: "tcti-post-full-shell-cat-posix-memalign-brk-fix",
            label: "full shell usability"
        ),
        simulatorProofGate(
            id: "simulator-tcti-package-behavior",
            runtimeGate: "tcti-package-behavior",
            marker: "ORLIX-TCTI-PACKAGE-BEHAVIOR-OK",
            prerequisite: "simulator-tcti-full-shell-usability",
            label: "package behavior"
        ),
        simulatorProofGate(
            id: "simulator-tcti-dynamic-loader-support",
            runtimeGate: "tcti-dynamic-loader-support",
            marker: "ORLIX-TCTI-DYNAMIC-LOADER-OK",
            prerequisite: "simulator-tcti-package-behavior",
            label: "dynamic loader support"
        ),
        simulatorProofGate(
            id: "simulator-tcti-signals",
            runtimeGate: "tcti-signals",
            marker: "ORLIX-TCTI-SIGNALS-OK",
            prerequisite: "simulator-tcti-dynamic-loader-support",
            label: "signal behavior"
        ),
        simulatorProofGate(
            id: "simulator-tcti-vfs-completeness",
            runtimeGate: "tcti-vfs-completeness",
            marker: "ORLIX-TCTI-VFS-OK",
            prerequisite: "simulator-tcti-signals",
            label: "VFS completeness"
        ),
        simulatorProofGate(
            id: "simulator-tcti-full-linux-runtime-readiness",
            runtimeGate: "tcti-full-linux-runtime-readiness",
            marker: "ORLIX-TCTI-FULL-RUNTIME-OK",
            prerequisite: "simulator-tcti-vfs-completeness",
            label: "full Linux runtime readiness"
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
        let satisfied = status.prerequisites.allSatisfy { byID[$0]?.satisfiesPrerequisite == true }
        return GateStatus(
            id: status.id,
            command: status.command,
            kind: status.kind,
            proofTier: status.proofTier,
            acceptanceWeight: status.acceptanceWeight,
            realStackRequired: status.realStackRequired,
            canClaimRuntimeReadiness: status.canClaimRuntimeReadiness,
            state: status.state,
            passed: status.passed,
            satisfiesPrerequisite: status.satisfiesPrerequisite,
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
    statuses.first { !$0.satisfiesPrerequisite && $0.prerequisitesSatisfied }
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
    noPhoneGatesBeforeFirstPhysical(statuses).allSatisfy { $0.satisfiesPrerequisite }
}

func simulatorRuntimeGates(_ statuses: [GateStatus]) -> [GateStatus] {
    statuses.filter { $0.kind == "simulator-runtime" }
}

func simulatorReadinessStatuses(_ statuses: [GateStatus]) -> [GateStatus] {
    let byID = Dictionary(uniqueKeysWithValues: statuses.map { ($0.id, $0) })
    return simulatorReadinessGateIDs.compactMap { byID[$0] }
}

func simulatorReadinessMissingGateIDs(_ statuses: [GateStatus]) -> [String] {
    let byID = Dictionary(uniqueKeysWithValues: statuses.map { ($0.id, $0) })
    return simulatorReadinessGateIDs.filter { gateID in
        guard let status = byID[gateID] else {
            return true
        }
        return !status.currentlySimulatorReadinessSatisfied
    }
}

func simulatorRuntimeGatesComplete(_ statuses: [GateStatus]) -> Bool {
    simulatorReadinessMissingGateIDs(statuses).isEmpty &&
        simulatorReadinessStatuses(statuses).count == simulatorReadinessGateIDs.count
}

func physicalGateMissingSimulatorPrerequisites(_ gate: Gate) -> [String] {
    let prerequisites = Set(gate.prerequisites)
    return simulatorReadinessGateIDs.filter { !prerequisites.contains($0) }
}

func runtimeGateID(for simulatorReadinessGateID: String) -> String {
    switch simulatorReadinessGateID {
    case "simulator-tcti-runtime-stability":
        return "tcti-simulator-stability"
    case "simulator-tcti-linux-console-usability":
        return "tcti-init-console-write"
    default:
        return simulatorReadinessGateID.replacingOccurrences(of: "simulator-", with: "")
    }
}

func physicalGateMissingSimulatorReportPaths(_ gate: Gate) -> [String] {
    let reports = Set(gate.expectedReportPaths)
    return simulatorReadinessGateIDs.compactMap { gateID in
        let runtimeGate = runtimeGateID(for: gateID)
        let expected = "Build/Reports/runtime/\(runtimeGate)-*.json"
        return reports.contains(expected) ? nil : expected
    }
}

func physicalGateMissingSimulatorValidationCommands(_ gate: Gate) -> [String] {
    simulatorReadinessGateIDs.compactMap { gateID in
        let runtimeGate = runtimeGateID(for: gateID)
        let hasCommand = gate.requiredValidationCommands.contains { command in
            command.contains("DESTINATION=iphonesimulator") &&
                command.contains("GATE=\(runtimeGate)") &&
                command.contains("ORLIX_SIMULATOR_ID=\(requiredSimulatorID)") &&
                command.contains("ORLIX_TCTI_REQUIRED_SIMULATOR_ID=\(requiredSimulatorID)") &&
                command.contains("ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=\(requiredSimulatorName)")
        }
        return hasCommand ? nil : runtimeGate
    }
}

let allowedProofTiers: Set<String> = [
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

let allowedAcceptanceWeights: Set<String> = [
    "probe",
    "blocker",
    "readiness",
    "release",
]

func gatePolicyText(_ gate: Gate) -> String {
    ([gate.id, gate.command, gate.kind] +
        gate.prerequisites +
        gate.allowedScope +
        gate.forbiddenScope +
        gate.expectedReportPaths +
        gate.requiredValidationCommands +
        gate.reducerRequirements +
        gate.requiredSubagentsOrSkills +
        gate.stopConditions).joined(separator: " ")
}

func gateHasRealStackPrerequisite(_ gate: Gate, byID: [String: Gate]) -> Bool {
    gate.prerequisites.contains { prerequisite in
        guard let prerequisiteGate = byID[prerequisite] else { return false }
        return prerequisiteGate.realStackRequired && prerequisiteGate.proofTier != "seed"
    }
}

func validateProofTierPolicy(_ gates: [Gate]) throws {
    let byID = Dictionary(uniqueKeysWithValues: gates.map { ($0.id, $0) })
    for gate in gates {
        guard allowedProofTiers.contains(gate.proofTier) else {
            throw HarnessError.invalid("gate \(gate.id) has unsupported proof_tier=\(gate.proofTier)")
        }
        guard allowedAcceptanceWeights.contains(gate.acceptanceWeight) else {
            throw HarnessError.invalid("gate \(gate.id) has unsupported acceptance_weight=\(gate.acceptanceWeight)")
        }
        if gate.proofTier == "seed" {
            if gate.readinessEligible || gate.canClaimRuntimeReadiness || gate.acceptanceWeight == "readiness" || gate.acceptanceWeight == "release" {
                throw HarnessError.invalid("seed gate \(gate.id) must not claim runtime readiness")
            }
            if gate.realStackRequired {
                throw HarnessError.invalid("seed gate \(gate.id) must not be marked real_stack_required")
            }
        }
        if gate.canClaimRuntimeReadiness && !gate.realStackRequired {
            throw HarnessError.invalid("gate \(gate.id) cannot claim runtime readiness without real_stack_required=true")
        }
        if ["rail", "safety"].contains(gate.proofTier) {
            if gate.realStackRequired || gate.canClaimRuntimeReadiness || gate.readinessEligible || gate.acceptanceWeight == "readiness" || gate.acceptanceWeight == "release" {
                throw HarnessError.invalid("\(gate.proofTier) gate \(gate.id) must not claim real-stack runtime readiness")
            }
        }
        if gate.readinessEligible && !gate.canClaimRuntimeReadiness {
            throw HarnessError.invalid("gate \(gate.id) is readiness_eligible but can_claim_runtime_readiness=false")
        }
        if gate.proofTier == "coreutils" {
            let nonSeedPrerequisites = gate.prerequisites.compactMap { byID[$0] }.filter { $0.proofTier != "seed" }
            if nonSeedPrerequisites.isEmpty {
                throw HarnessError.invalid("Coreutils gate \(gate.id) must not depend only on seed or golden ELF probes")
            }
            let text = gatePolicyText(gate)
            if !text.contains("Coreutils") && !text.contains("coreutils") {
                throw HarnessError.invalid("Coreutils gate \(gate.id) must name real Coreutils or upstream package behavior")
            }
        }
        if gate.proofTier == "oci" {
            let text = gatePolicyText(gate)
            guard text.contains("OrlixOS"),
                  text.contains("rootfs"),
                  text.contains("session") else {
                throw HarnessError.invalid("OCI gate \(gate.id) must reference OrlixOS rootfs/session proof")
            }
        }
        if gate.proofTier == "device" || gate.physicalDevice {
            let missingPrerequisites = physicalGateMissingSimulatorPrerequisites(gate)
            if !missingPrerequisites.isEmpty {
                throw HarnessError.invalid("device gate \(gate.id) is missing simulator prerequisites: \(missingPrerequisites.joined(separator: ","))")
            }
            if !gateHasRealStackPrerequisite(gate, byID: byID) {
                throw HarnessError.invalid("device gate \(gate.id) must depend on at least one real-stack prerequisite")
            }
        }
        if gate.proofTier == "release" && gate.acceptanceWeight == "release" {
            if !gate.realStackRequired {
                throw HarnessError.invalid("release gate \(gate.id) must require real-stack proof")
            }
            let prerequisiteGates = gate.prerequisites.compactMap { byID[$0] }
            if !prerequisiteGates.contains(where: { $0.proofTier == "device" }) {
                throw HarnessError.invalid("release gate \(gate.id) must depend on device proof")
            }
            if !prerequisiteGates.contains(where: { $0.kind == "safety" || $0.id.contains("safety") }) {
                throw HarnessError.invalid("release gate \(gate.id) must depend on safety proof")
            }
        }
        if gate.id.contains("product-default") || gate.id.contains("default-flip") {
            let prerequisiteGates = gate.prerequisites.compactMap { byID[$0] }
            if !gate.realStackRequired || !prerequisiteGates.contains(where: { $0.proofTier == "device" }) {
                throw HarnessError.invalid("product-default gate \(gate.id) must require real-stack and device gates")
            }
        }
    }
}

func validateReportProofTierMetadata(_ object: [String: Any], path: String) throws {
    let status = stringValue(object["status"]) ?? "missing"
    let passed = boolValue(object["passed"])
    guard status == "pass" || passed else {
        return
    }
    guard let proofTier = stringValue(object["proof_tier"]),
          allowedProofTiers.contains(proofTier) else {
        throw HarnessError.invalid("\(path) status=pass report lacks valid proof_tier")
    }
    guard let acceptanceWeight = stringValue(object["acceptance_weight"]),
          allowedAcceptanceWeights.contains(acceptanceWeight) else {
        throw HarnessError.invalid("\(path) status=pass report lacks valid acceptance_weight")
    }
    guard object["real_stack_required"] != nil else {
        throw HarnessError.invalid("\(path) status=pass report lacks real_stack_required")
    }
    guard object["can_claim_runtime_readiness"] != nil else {
        throw HarnessError.invalid("\(path) status=pass report lacks can_claim_runtime_readiness")
    }
    if proofTier == "seed" && boolValue(object["can_claim_runtime_readiness"]) {
        throw HarnessError.invalid("\(path) seed report must not claim runtime readiness")
    }
    if boolValue(object["can_claim_runtime_readiness"]) && !boolValue(object["real_stack_required"]) {
        throw HarnessError.invalid("\(path) report claims runtime readiness without real_stack_required=true")
    }
}

func validateRoadmapSimulatorPolicy(_ roadmap: Roadmap) throws {
    let gates = roadmapGatesWithRuntimePreflight(roadmap)
    try validateProofTierPolicy(gates)
    let ids = Set(gates.map(\.id))
    let missingReadinessGates = simulatorReadinessGateIDs.filter { !ids.contains($0) }
    if !missingReadinessGates.isEmpty {
        throw HarnessError.invalid("roadmap missing simulator readiness gates: \(missingReadinessGates.joined(separator: ","))")
    }

    for gate in gates where gate.kind == "simulator-runtime" {
        guard !gate.physicalDevice else {
            throw HarnessError.invalid("simulator runtime gate \(gate.id) must not be marked physical_device")
        }
        guard gate.command.contains("DESTINATION=iphonesimulator"),
              gate.command.contains("ORLIX_SIMULATOR_ID=\(requiredSimulatorID)"),
              gate.command.contains("ORLIX_TCTI_REQUIRED_SIMULATOR_ID=\(requiredSimulatorID)"),
              gate.command.contains("ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=\(requiredSimulatorName)") else {
            throw HarnessError.invalid("simulator runtime gate \(gate.id) must target required simulator \(requiredSimulatorName) (\(requiredSimulatorID))")
        }
        guard !gate.command.contains("DESTINATION=iphoneos") else {
            throw HarnessError.invalid("simulator runtime gate \(gate.id) must not contain iphoneos destination")
        }
    }

    for gate in gates where gate.physicalDevice {
        let missingPrerequisites = physicalGateMissingSimulatorPrerequisites(gate)
        if !missingPrerequisites.isEmpty {
            throw HarnessError.invalid("physical-device gate \(gate.id) is missing required simulator prerequisites: \(missingPrerequisites.joined(separator: ","))")
        }
        let missingReports = physicalGateMissingSimulatorReportPaths(gate)
        if !missingReports.isEmpty {
            throw HarnessError.invalid("physical-device gate \(gate.id) is missing required simulator report paths: \(missingReports.joined(separator: ","))")
        }
        let missingCommands = physicalGateMissingSimulatorValidationCommands(gate)
        if !missingCommands.isEmpty {
            throw HarnessError.invalid("physical-device gate \(gate.id) is missing required pinned simulator validation commands: \(missingCommands.joined(separator: ","))")
        }
    }
}

func simulatorRuntimeGateIsSelectable(_ statuses: [GateStatus]) -> Bool {
    simulatorRuntimeGates(statuses).contains { !$0.passed && $0.prerequisitesSatisfied }
}

func dirtyRuntimeOrHarnessWorktree() -> Bool {
    let paths = [
        ".agents/skills/orlix-tcti-next-step",
        ".agents/skills/orlix-tcti-safety",
        ".codex/rules",
        "tools/runtime",
        "tools/tcti",
        "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix",
        "docs/plans/active/orlix-tcti/PLAN.md",
        "docs/plans/active/orlix-tcti/IMPLEMENT.md",
    ]
    guard let output = run("/usr/bin/env", ["git", "status", "--short", "--"] + paths) else {
        return true
    }
    return !output.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
}

func physicalBlockers(statuses: [GateStatus], preflightPassed: Bool) -> [String] {
    var blockers: [String] = []
    let missingSimulator = simulatorReadinessMissingGateIDs(statuses)
    if !missingSimulator.isEmpty {
        blockers.append("missing_or_failing_simulator_readiness_gates=\(missingSimulator.joined(separator: ","))")
    }
    if !preflightPassed {
        blockers.append("autonomous_no_phone_preflight_not_passed")
    }
    if !noPhoneGatesPassedBeforeFirstPhysical(statuses) {
        blockers.append("no_phone_gates_before_first_physical_not_passed")
    }
    if !physicalDeviceExplicitlyAllowed() {
        blockers.append("explicit_physical_opt_in_missing")
    }
    if dirtyRuntimeOrHarnessWorktree() {
        blockers.append("dirty_runtime_or_harness_worktree")
    }
    return blockers
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
    let simulatorPassed = simulatorRuntimeGatesComplete(statuses)
    let preflightGateIDs = Set(runtimePreflightGates().map(\.id))
    let preflightPassed = statuses
        .filter { preflightGateIDs.contains($0.id) }
        .allSatisfy { $0.satisfiesPrerequisite }
    let physicalAllowed = physicalBlockers(
        statuses: statuses,
        preflightPassed: preflightPassed
    ).isEmpty
    let eligible = statuses.filter { status in
        guard !status.satisfiesPrerequisite && status.prerequisitesSatisfied else {
            return false
        }
        if status.physicalDevice && !physicalAllowed {
            return false
        }
        if status.gadget && !simulatorPassed {
            return false
        }
        return true
    }
    if eligible.contains(where: { $0.reason.contains("simulator stability report") && $0.reason.contains("stale") }),
       let simulatorStability = eligible.first(where: { $0.id == "simulator-tcti-runtime-stability" }) {
        return simulatorStability
    }
    if let currentReadyGate = eligible.first(where: { $0.state == "ready" }) {
        return currentReadyGate
    }
    if let realStackGate = eligible.first(where: {
        $0.realStackRequired &&
            !$0.physicalDevice &&
            $0.proofTier != "simulator" &&
            $0.proofTier != "device" &&
            $0.proofTier != "release"
    }) {
        return realStackGate
    }
    if let currentSimulatorFailure = eligible.first(where: { $0.kind == "simulator-runtime" && ($0.state == "fail" || $0.state == "missing") }) {
        return currentSimulatorFailure
    }
    return eligible.first
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
    try validateRoadmapSimulatorPolicy(roadmap)
    let gateStatuses = statuses(for: roadmap)
    let physicalGate = gateStatuses.first { $0.physicalDevice }
    let next = selectedStatusWithSafety(from: gateStatuses)
    let preflightGateIDs = Set(runtimePreflightGates().map(\.id))
    let preflightPassed = gateStatuses
        .filter { preflightGateIDs.contains($0.id) }
        .allSatisfy { $0.satisfiesPrerequisite }
    let missingSimulatorReadiness = simulatorReadinessMissingGateIDs(gateStatuses)
    let blockers = physicalBlockers(statuses: gateStatuses, preflightPassed: preflightPassed)
    let dirtyRuntimeOrHarness = dirtyRuntimeOrHarnessWorktree()
    let physicalAllowed = physicalGate?.prerequisitesSatisfied == true &&
        preflightPassed &&
        missingSimulatorReadiness.isEmpty &&
        noPhoneGatesPassedBeforeFirstPhysical(gateStatuses) &&
        physicalDeviceExplicitlyAllowed() &&
        !dirtyRuntimeOrHarness
    let releaseEligible = gateStatuses.contains {
        $0.proofTier == "release" &&
            $0.acceptanceWeight == "release" &&
            $0.passed &&
            $0.prerequisitesSatisfied
    }
    let readinessEligible = gateStatuses.contains {
        $0.canClaimRuntimeReadiness &&
            $0.realStackRequired &&
            $0.currentlyReadinessEligible
    }
    return StatusDocument(
        area: roadmap.area,
        generatedAt: timestamp(),
        gitSHA: gitSHA(),
        roadmapPath: relativePath(roadmapURL),
        gates: gateStatuses,
        simulatorAllowed: simulatorRuntimeGateIsSelectable(gateStatuses) || simulatorRuntimeGatesComplete(gateStatuses),
        simulatorRequiredBeforePhysical: true,
        simulatorGatesComplete: missingSimulatorReadiness.isEmpty,
        simulatorReadinessCapabilities: simulatorReadinessCapabilities,
        simulatorReadinessGateIDs: simulatorReadinessGateIDs,
        simulatorReadinessMissingGateIDs: missingSimulatorReadiness,
        requiredSimulatorID: requiredSimulatorID,
        requiredSimulatorName: requiredSimulatorName,
        physicalDeviceAllowed: physicalAllowed,
        physicalDeviceBlockers: physicalAllowed ? [] : blockers,
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
        print("simulator_readiness_gates: \(status.simulatorReadinessGateIDs.joined(separator: ","))")
        print("simulator_readiness_missing: \(status.simulatorReadinessMissingGateIDs.joined(separator: ","))")
        print("simulator_readiness_capabilities:")
        for capability in status.simulatorReadinessCapabilities {
            print("- \(capability.id): \(capability.gateID)")
        }
        print("required_simulator: \(status.requiredSimulatorName) (\(status.requiredSimulatorID))")
        print("physical_device_allowed: \(status.physicalDeviceAllowed)")
        print("physical_device_blockers: \(status.physicalDeviceBlockers.joined(separator: ","))")
        print("release_gate_eligible: \(status.releaseGateEligible)")
        print("readiness_gate_eligible: \(status.readinessGateEligible)")
        print("next_eligible_gate: \(status.nextEligibleGate ?? "none")")
        print("gates:")
        for gate in status.gates {
            print("- \(gate.id): \(gate.state) proof_tier=\(gate.proofTier) acceptance_weight=\(gate.acceptanceWeight) real_stack_required=\(gate.realStackRequired) can_claim_runtime_readiness=\(gate.canClaimRuntimeReadiness) prerequisites_satisfied=\(gate.prerequisitesSatisfied) report_paths=\(gate.reportPaths.joined(separator: ","))")
        }
    }
    return status
}

func whySelected(for gate: Gate, prerequisites: [PrerequisiteFact]) -> String {
    let evidence = prerequisites.isEmpty ? "none" : prerequisites.map { "\($0.id)=\($0.state)" }.joined(separator: ", ")
    switch gate.proofTier {
    case "seed":
        return "Selected \(gate.id) as a seed probe or reducer. This can unlock implementation work but cannot claim Linux runtime readiness. Evidence: \(evidence)."
    case "kernel":
        return "Selected \(gate.id) because the next missing proof is Linux-owned kernel/TCTI behavior, not another golden ELF seed. Evidence: \(evidence)."
    case "kselftest":
        return "Selected \(gate.id) because kernel-interface proof must advance through a real kselftest subset after local kernel/TCTI probes. Evidence: \(evidence)."
    case "mlibc", "mlibc-uapi":
        return "Selected \(gate.id) because no current OrlixMLibC-linked binary proof at this tier satisfies the harness. Evidence: \(evidence)."
    case "shell":
        return "Selected \(gate.id) because ordinary POSIX shell behavior is the next missing real-userspace proof. Evidence: \(evidence)."
    case "coreutils":
        return "Selected \(gate.id) because upstream/Coreutils package behavior must run through the real stack before readiness claims. Evidence: \(evidence)."
    case "oci":
        return "Selected \(gate.id) because OrlixOS OCI/rootfs/session materialization is the next missing real-stack proof. Evidence: \(evidence)."
    case "simulator":
        return "Selected \(gate.id) because app-hosted simulator proof is required before any physical-device work. Evidence: \(evidence)."
    case "device":
        return "Selected \(gate.id) only after no-phone and simulator real-stack prerequisites are satisfied and explicit physical-device opt-in is present. Evidence: \(evidence)."
    case "release":
        return "Selected \(gate.id) because release gating must follow real-stack device and safety evidence. Evidence: \(evidence)."
    default:
        return "Selected \(gate.id) because it is the first eligible roadmap gate with proof_tier=\(gate.proofTier). Evidence: \(evidence)."
    }
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
    guard let selectedStatus = byID[selectedID] else {
        throw HarnessError.invalid("selected gate \(selectedID) has no status entry")
    }
    let selectedPolicy = selectedStatus.resultPolicy
    let prerequisites = gate.prerequisites.map { prereqID in
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
        selectedGateID: gate.id,
        selectedGateCommand: selectedStatus.command,
        selectedGateKind: gate.kind,
        selectedGateProofTier: gate.proofTier,
        selectedGateAcceptanceWeight: gate.acceptanceWeight,
        selectedGateRealStackRequired: gate.realStackRequired,
        selectedGateCanClaimRuntimeReadiness: gate.canClaimRuntimeReadiness,
        selectedGateResultClassification: selectedPolicy.classification,
        runtimePatchAllowed: selectedPolicy.runtimePatchAllowed,
        harnessPatchAllowed: selectedPolicy.harnessPatchAllowed,
        continueRefreshAllowed: selectedPolicy.continueRefreshAllowed,
        mustStop: selectedPolicy.mustStop,
        requiredNextAction: selectedPolicy.requiredNextAction,
        resultClassificationReason: selectedPolicy.reason,
        owningLayer: selectedPolicy.owningLayer,
        prerequisiteGates: prerequisites,
        whySelected: whySelected(for: gate, prerequisites: prerequisites),
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
        simulatorReadinessCapabilities: status.simulatorReadinessCapabilities,
        simulatorReadinessGateIDs: status.simulatorReadinessGateIDs,
        simulatorReadinessMissingGateIDs: status.simulatorReadinessMissingGateIDs,
        selectedGateUsesSimulator: gateUsesRequiredSimulator(gate),
        requiredSimulatorID: status.requiredSimulatorID,
        requiredSimulatorName: status.requiredSimulatorName,
        physicalDeviceAllowed: status.physicalDeviceAllowed,
        physicalDeviceBlockers: status.physicalDeviceBlockers,
        releaseGateEligible: status.releaseGateEligible,
        readinessGateEligible: status.readinessGateEligible,
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
    let blockedPolicy = GateResultPolicy(
        classification: "physical_device_blocked",
        runtimePatchAllowed: false,
        harnessPatchAllowed: false,
        continueRefreshAllowed: false,
        mustStop: true,
        requiredNextAction: "stop before physical-device work until simulator readiness and explicit human opt-in are present",
        reason: "physical_device_allowed=false; blockers=\(status.physicalDeviceBlockers.joined(separator: ", "))",
        owningLayer: "physical-device gate policy"
    )
    return TaskEnvelope(
        area: roadmap.area,
        generatedAt: timestamp(),
        gitSHA: status.gitSHA,
        roadmapPath: relativePath(roadmapURL),
        selectedGateID: physicalOptInBlockedGateID,
        selectedGateCommand: "no-op: set ORLIX_TCTI_ALLOW_PHYSICAL_DEVICE=1 only after explicit human approval",
        selectedGateKind: "blocked",
        selectedGateProofTier: "device",
        selectedGateAcceptanceWeight: "blocker",
        selectedGateRealStackRequired: true,
        selectedGateCanClaimRuntimeReadiness: false,
        selectedGateResultClassification: blockedPolicy.classification,
        runtimePatchAllowed: blockedPolicy.runtimePatchAllowed,
        harnessPatchAllowed: blockedPolicy.harnessPatchAllowed,
        continueRefreshAllowed: blockedPolicy.continueRefreshAllowed,
        mustStop: blockedPolicy.mustStop,
        requiredNextAction: blockedPolicy.requiredNextAction,
        resultClassificationReason: blockedPolicy.reason,
        owningLayer: blockedPolicy.owningLayer,
        prerequisiteGates: prerequisites,
        whySelected: "The roadmap has no safer eligible non-phone gate. Phone work stays blocked until the full pinned simulator readiness ladder passes and explicit human opt-in is present. Missing simulator readiness gates: \(status.simulatorReadinessMissingGateIDs.isEmpty ? "none" : status.simulatorReadinessMissingGateIDs.joined(separator: ", ")).",
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
            "Stop if any simulator readiness gate is missing, stale, failing, evidence-only, or emergency-override.",
            "Stop if the blocked envelope is missing machine-readable JSON or Markdown."
        ],
        simulatorAllowed: status.simulatorAllowed,
        simulatorRequiredBeforePhysical: status.simulatorRequiredBeforePhysical,
        simulatorGatesComplete: status.simulatorGatesComplete,
        simulatorReadinessCapabilities: status.simulatorReadinessCapabilities,
        simulatorReadinessGateIDs: status.simulatorReadinessGateIDs,
        simulatorReadinessMissingGateIDs: status.simulatorReadinessMissingGateIDs,
        selectedGateUsesSimulator: false,
        requiredSimulatorID: status.requiredSimulatorID,
        requiredSimulatorName: status.requiredSimulatorName,
        physicalDeviceAllowed: status.physicalDeviceAllowed,
        physicalDeviceBlockers: status.physicalDeviceBlockers,
        releaseGateEligible: status.releaseGateEligible,
        readinessGateEligible: status.readinessGateEligible,
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
    let simulatorCapabilities = envelope.simulatorReadinessCapabilities
        .map { "- \($0.id): \($0.gateID) - \($0.description)" }
        .joined(separator: "\n")
    return """
    # Orlix TCTI Next Task

    Selected gate: `\(envelope.selectedGateID)`

    Command: `\(envelope.selectedGateCommand)`

    Proof tier: `\(envelope.selectedGateProofTier)`

    Acceptance weight: `\(envelope.selectedGateAcceptanceWeight)`

    Real stack required: \(envelope.selectedGateRealStackRequired)

    Can claim runtime readiness: \(envelope.selectedGateCanClaimRuntimeReadiness)

    Result classification: `\(envelope.selectedGateResultClassification)`

    Runtime patch allowed: \(envelope.runtimePatchAllowed)

    Harness patch allowed: \(envelope.harnessPatchAllowed)

    Continue refresh allowed: \(envelope.continueRefreshAllowed)

    Must stop: \(envelope.mustStop)

    Required next action: \(envelope.requiredNextAction)

    Owning layer: \(envelope.owningLayer)

    Classification reason: \(envelope.resultClassificationReason)

    Why selected: \(envelope.whySelected)

    ## Simulator Gate
    - simulator_allowed: \(envelope.simulatorAllowed)
    - simulator_required_before_physical: \(envelope.simulatorRequiredBeforePhysical)
    - simulator_gates_complete: \(envelope.simulatorGatesComplete)
    - simulator_readiness_gates: \(envelope.simulatorReadinessGateIDs.joined(separator: ", "))
    - simulator_readiness_missing: \(envelope.simulatorReadinessMissingGateIDs.isEmpty ? "none" : envelope.simulatorReadinessMissingGateIDs.joined(separator: ", "))
    - simulator_readiness_capabilities:
    \(simulatorCapabilities)
    - selected_gate_uses_simulator: \(envelope.selectedGateUsesSimulator)
    - required_simulator: \(envelope.requiredSimulatorName) (\(envelope.requiredSimulatorID))
    - physical_device_allowed: \(envelope.physicalDeviceAllowed)
    - physical_device_blockers: \(envelope.physicalDeviceBlockers.isEmpty ? "none" : envelope.physicalDeviceBlockers.joined(separator: ", "))
    - release_gate_eligible: \(envelope.releaseGateEligible)
    - readiness_gate_eligible: \(envelope.readinessGateEligible)

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
    if ProcessInfo.processInfo.environment["ORLIX_TCTI_HARNESS_QUIET"] != "1" {
        print("Orlix TCTI next task")
        print("next_task_json: \(relativePath(nextTaskURL))")
        print("next_task_md: \(relativePath(nextTaskMarkdownURL))")
        print("selected_gate: \(task.selectedGateID)")
        print("selected_command: \(task.selectedGateCommand)")
        print("commit_message: \(task.commitMessage)")
    }
    return task
}

func validateTaskPolicy(_ task: TaskEnvelope, expected policy: GateResultPolicy) throws {
    if task.selectedGateResultClassification != policy.classification ||
        task.runtimePatchAllowed != policy.runtimePatchAllowed ||
        task.harnessPatchAllowed != policy.harnessPatchAllowed ||
        task.continueRefreshAllowed != policy.continueRefreshAllowed ||
        task.mustStop != policy.mustStop ||
        task.requiredNextAction != policy.requiredNextAction ||
        task.resultClassificationReason != policy.reason ||
        task.owningLayer != policy.owningLayer {
        throw HarnessError.invalid("next-task result classification/action policy is stale; run make agent-next AREA=orlix-tcti")
    }
}

func validateEnvelope() throws {
    guard area == "orlix-tcti" else {
        throw HarnessError.usage("unsupported AREA=\(area); expected AREA=orlix-tcti")
    }
    guard fileManager.fileExists(atPath: nextTaskURL.path) else {
        throw HarnessError.invalid("missing \(relativePath(nextTaskURL)); run make agent-next AREA=orlix-tcti")
    }
    let roadmap = try loadRoadmap()
    try validateRoadmapSimulatorPolicy(roadmap)
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
    let freshStatus = try statusDocument()
    if let selected = freshStatus.nextEligibleGate,
       task.selectedGateID != selected {
        throw HarnessError.invalid("next-task selected gate \(task.selectedGateID) is stale; run make agent-next AREA=orlix-tcti to regenerate \(selected)")
    }
    if task.selectedGateID == physicalOptInBlockedGateID {
        let status = freshStatus
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
    if task.simulatorReadinessGateIDs != simulatorReadinessGateIDs {
        throw HarnessError.invalid("blocked physical opt-in envelope simulator readiness gate list is stale or incomplete")
    }
    if task.simulatorReadinessCapabilities != simulatorReadinessCapabilities {
        throw HarnessError.invalid("blocked physical opt-in envelope simulator readiness capabilities are stale or incomplete")
    }
    if task.simulatorReadinessMissingGateIDs != status.simulatorReadinessMissingGateIDs {
        throw HarnessError.invalid("blocked physical opt-in envelope simulator missing list is stale; run make agent-next AREA=orlix-tcti")
    }
        if task.physicalDeviceAllowed != status.physicalDeviceAllowed ||
            task.physicalDeviceBlockers != status.physicalDeviceBlockers ||
            task.releaseGateEligible != status.releaseGateEligible ||
            task.readinessGateEligible != status.readinessGateEligible {
            throw HarnessError.invalid("blocked physical opt-in envelope physical/readiness state is stale; run make agent-next AREA=orlix-tcti")
        }
        let blockedPolicy = GateResultPolicy(
            classification: "physical_device_blocked",
            runtimePatchAllowed: false,
            harnessPatchAllowed: false,
            continueRefreshAllowed: false,
            mustStop: true,
            requiredNextAction: "stop before physical-device work until simulator readiness and explicit human opt-in are present",
            reason: "physical_device_allowed=false; blockers=\(status.physicalDeviceBlockers.joined(separator: ", "))",
            owningLayer: "physical-device gate policy"
        )
        try validateTaskPolicy(task, expected: blockedPolicy)
        print("pass: \(relativePath(nextTaskURL))")
        print("selected_gate: \(task.selectedGateID)")
        return
    }
    guard let gate = roadmapGatesWithRuntimePreflight(roadmap).first(where: { $0.id == task.selectedGateID }) else {
        throw HarnessError.invalid("selected gate \(task.selectedGateID) does not exist in roadmap")
    }
    if let target = tctiGateTarget(in: task.selectedGateCommand) {
        let supportedTargets = try supportedTCTIGateTargets()
        if !supportedTargets.contains(target) {
            throw HarnessError.invalid("selected gate command references unsupported tcti-gate target: \(target)")
        }
    }
    let status = freshStatus
    let byID = Dictionary(uniqueKeysWithValues: status.gates.map { ($0.id, $0) })
    guard let selectedStatus = byID[gate.id] else {
        throw HarnessError.invalid("selected gate \(gate.id) has no status entry")
    }
    try validateTaskPolicy(task, expected: selectedStatus.resultPolicy)
    for prerequisite in gate.prerequisites {
        guard byID[prerequisite]?.satisfiesPrerequisite == true else {
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
    if task.simulatorReadinessGateIDs != simulatorReadinessGateIDs {
        throw HarnessError.invalid("next-task simulator readiness gate list is stale or incomplete")
    }
    if task.simulatorReadinessCapabilities != simulatorReadinessCapabilities {
        throw HarnessError.invalid("next-task simulator readiness capabilities are stale or incomplete")
    }
    if task.simulatorReadinessMissingGateIDs != status.simulatorReadinessMissingGateIDs {
        throw HarnessError.invalid("next-task simulator readiness missing list is stale; run make agent-next AREA=orlix-tcti")
    }
    if task.physicalDeviceAllowed != status.physicalDeviceAllowed ||
        task.physicalDeviceBlockers != status.physicalDeviceBlockers ||
        task.releaseGateEligible != status.releaseGateEligible ||
        task.readinessGateEligible != status.readinessGateEligible {
        throw HarnessError.invalid("next-task physical/readiness state is stale; run make agent-next AREA=orlix-tcti")
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
    if task.physicalDevice && !status.simulatorReadinessMissingGateIDs.isEmpty {
        throw HarnessError.invalid("physical-device gate selected before full simulator readiness ladder passed: \(status.simulatorReadinessMissingGateIDs.joined(separator: ","))")
    }
    if task.physicalDevice {
        let missing = physicalGateMissingSimulatorPrerequisites(gate)
        if !missing.isEmpty {
            throw HarnessError.invalid("physical-device gate \(gate.id) is missing required simulator prerequisites: \(missing.joined(separator: ","))")
        }
    }
    if task.gadget {
        guard status.simulatorGatesComplete,
              status.simulatorReadinessMissingGateIDs.isEmpty else {
            throw HarnessError.invalid("gadget gate selected before full pinned simulator readiness ladder passed: \(status.simulatorReadinessMissingGateIDs.joined(separator: ","))")
        }
        guard byID["switch-init-001-exit"]?.satisfiesPrerequisite == true,
              byID["switch-init-002-write"]?.satisfiesPrerequisite == true else {
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

func semanticFreshnessFixtureGate(id: String, command: String, kind: String) -> Gate {
    Gate(
        id: id,
        command: command,
        kind: kind,
        prerequisites: [],
        allowedScope: [],
        forbiddenScope: [],
        expectedReportPaths: [],
        readinessEligible: id.hasPrefix("simulator-tcti-"),
        physicalDevice: false,
        gadget: false,
        requiredValidationCommands: [],
        reducerRequirements: [],
        requiredSubagentsOrSkills: [],
        commitMessageTemplate: "",
        stopConditions: []
    )
}

func validateSemanticFreshnessFixtures() throws {
    let runtimeGate = semanticFreshnessFixtureGate(
        id: "simulator-tcti-full-shell-usability",
        command: "make runtime-validation DESTINATION=iphonesimulator GATE=tcti-full-shell-usability",
        kind: "simulator-runtime"
    )
    let tctiGate = semanticFreshnessFixtureGate(
        id: "tcti-kernel-syscall-dispatch-smoke",
        command: "make tcti-gate TARGET=tcti-kernel-syscall-dispatch-smoke",
        kind: "kernel"
    )
    let kernelGate = semanticFreshnessFixtureGate(
        id: "tcti-kernel-execve-binfmt-elf-smoke",
        command: "make tcti-gate TARGET=tcti-kernel-execve-binfmt-elf-smoke",
        kind: "kernel"
    )
    let selectorGate = semanticFreshnessFixtureGate(
        id: "tcti-shell-exec-simple-command",
        command: "make tcti-gate TARGET=tcti-shell-exec-simple-command",
        kind: "runtime"
    )
    let cases: [(String, Gate, String, Bool)] = [
        ("implement-checkpoint-does-not-rerun-runtime", runtimeGate, "docs/plans/active/orlix-tcti/IMPLEMENT.md", false),
        ("harness-doc-does-not-rerun-runtime", runtimeGate, "docs/harness/ORLIX_TCTI_AGENT_HARNESS.md", false),
        ("runtime-tool-reruns-runtime", runtimeGate, "tools/runtime/orlix-runtime-validation.sh", true),
        ("tcti-tool-reruns-tcti-gate", tctiGate, "tools/tcti/orlix-tcti-gate.swift", true),
        ("kernel-port-reruns-kernel-and-runtime", kernelGate, "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/kernel/syscall.c", true),
        ("environment-policy-reruns-simulator", runtimeGate, ".agents/skills/orlix-tcti-next-step/references/environment-policy.json", true),
        ("selector-script-recomputes-status-only", selectorGate, ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift", false),
    ]
    for (name, gate, path, expected) in cases {
        let actual = doesChangedPathInvalidateGate(gate, changedPath: path)
        if actual != expected {
            throw HarnessError.invalid("semantic freshness fixture \(name) expected invalidates=\(expected) for \(path), got \(actual)")
        }
    }
    print("pass: semantic-freshness-check")
}

func policyFixtureReport(
    path: String,
    exists: Bool = true,
    passed: Bool = false,
    proofTier: String? = "seed",
    acceptanceWeight: String? = nil,
    realStackRequired: Bool? = nil,
    canClaimRuntimeReadiness: Bool? = nil,
    forbiddenBehaviorViolations: [String] = []
) -> ReportFact {
    ReportFact(
        path: path,
        exists: exists,
        status: passed ? "pass" : "fail",
        passed: passed,
        proofTier: proofTier,
        acceptanceWeight: acceptanceWeight,
        realStackRequired: realStackRequired,
        canClaimRuntimeReadiness: canClaimRuntimeReadiness,
        releaseGateEligible: false,
        readinessGateEligible: false,
        gitSHA: gitSHA(),
        forbiddenBehaviorViolations: forbiddenBehaviorViolations
    )
}

func policyFixtureStatus(
    id: String,
    command: String = "make tcti-gate TARGET=tcti-fixture",
    kind: String = "rail",
    proofTier: String = "seed",
    acceptanceWeight: String = "probe",
    realStackRequired: Bool = false,
    canClaimRuntimeReadiness: Bool = false,
    state: String,
    passed: Bool = false,
    reason: String,
    reports: [ReportFact] = [],
    readinessEligible: Bool = false
) -> GateStatus {
    GateStatus(
        id: id,
        command: command,
        kind: kind,
        proofTier: proofTier,
        acceptanceWeight: acceptanceWeight,
        realStackRequired: realStackRequired,
        canClaimRuntimeReadiness: canClaimRuntimeReadiness,
        state: state,
        passed: passed,
        reason: reason,
        prerequisites: [],
        prerequisitesSatisfied: true,
        reportPaths: reports.map(\.path),
        reports: reports,
        readinessEligible: readinessEligible,
        physicalDevice: false,
        gadget: false
    )
}

func validateGateResultPolicyFixtures() throws {
    let fixtures: [(String, GateStatus, String, Bool, Bool, Bool, Bool)] = [
        ("stale-proof-refresh", policyFixtureStatus(id: "stale", state: "stale", reason: "report git_sha is stale"), "stale_proof_refresh", false, false, true, false),
        ("missing-generated-artifact", policyFixtureStatus(id: "golden-init-001-structural", command: "make tcti-gate TARGET=tcti-golden-elf CASE=init_001_exit", kind: "golden-structural", state: "missing", reason: "validation artifact missing", reports: [policyFixtureReport(path: "Build/TCTI/golden_elf/init_001_exit/validation.json", exists: false)]), "missing_generated_artifact", false, false, true, false),
        ("missing-artifact-without-safe-generator", policyFixtureStatus(id: "unknown", command: "", state: "missing", reason: "artifact missing", reports: [policyFixtureReport(path: "Build/TCTI/unknown.json", exists: false)]), "missing_generated_artifact", false, false, false, true),
        ("rail-evidence-contract-bug", policyFixtureStatus(id: "rail", state: "fail", reason: "historical generated report is obsolete"), "rail_evidence_contract_bug", false, true, false, true),
        ("rail-simulator-freshness-contract-bug", policyFixtureStatus(id: "rail-stale", state: "fail", reason: "simulator-report-stale: latest simulator stability report is not execution-fresh for this rail"), "rail_evidence_contract_bug", false, true, false, true),
        ("rail-missing-reducer-contract-bug", policyFixtureStatus(id: "rail-reducer", kind: "production-tcti-fix", state: "fail", reason: "reducer-evidence: missing reducer report or pass-regression evidence"), "rail_evidence_contract_bug", false, true, false, true),
        ("metadata-drift", policyFixtureStatus(id: "drift", kind: "rail", proofTier: "rail", state: "fail", reason: "metadata mismatch", reports: [policyFixtureReport(path: "Build/TCTI/reports/tcti-fixture/report.json", proofTier: "seed")]), "proof_tier_report_status_metadata_drift", false, true, false, true),
        ("runtime-product-failure", policyFixtureStatus(id: "runtime", kind: "kernel", proofTier: "kernel", acceptanceWeight: "blocker", realStackRequired: true, state: "fail", reason: "guest syscall failed", reports: [policyFixtureReport(path: "Build/TCTI/reports/tcti-fixture/report.json", proofTier: "kernel")]), "current_runtime_product_failure", true, false, false, true),
        ("environment-only-failure", policyFixtureStatus(id: "environment", state: "fail", reason: "CoreSimulator bootstatus failed"), "environment_only_failure", false, false, false, true),
        ("forbidden-behavior-violation", policyFixtureStatus(id: "forbidden", state: "fail", reason: "safety report failed", reports: [policyFixtureReport(path: "Build/TCTI/reports/tcti-fixture/report.json", forbiddenBehaviorViolations: ["map_jit"])]), "forbidden_behavior_violation", false, false, false, true),
        ("readiness-gate-pass", policyFixtureStatus(id: "simulator-tcti-runtime-stability", kind: "simulator-runtime", proofTier: "simulator", acceptanceWeight: "readiness", realStackRequired: true, canClaimRuntimeReadiness: true, state: "pass", passed: true, reason: "current simulator readiness report passed", readinessEligible: true), "readiness_gate_pass", false, false, true, false),
    ]

    for (name, status, classification, runtimePatchAllowed, harnessPatchAllowed, continueRefreshAllowed, mustStop) in fixtures {
        let actual = classifyGateResult(status)
        guard actual.classification == classification,
              actual.runtimePatchAllowed == runtimePatchAllowed,
              actual.harnessPatchAllowed == harnessPatchAllowed,
              actual.continueRefreshAllowed == continueRefreshAllowed,
              actual.mustStop == mustStop else {
            throw HarnessError.invalid("gate result policy fixture \(name) produced unexpected policy")
        }
    }
    print("pass: gate-result-policy-check")
}

let mode = CommandLine.arguments.dropFirst().first ?? "status"

do {
    switch mode {
    case "status":
        _ = try writeStatus(
            printHuman: ProcessInfo.processInfo.environment["ORLIX_TCTI_HARNESS_QUIET"] != "1"
        )
    case "next":
        _ = try writeNext()
    case "check":
        try validateEnvelope()
    case "validate-roadmap":
        let path = CommandLine.arguments.dropFirst(2).first
        let url = path.map { URL(fileURLWithPath: $0, relativeTo: root).standardizedFileURL } ?? roadmapURL
        let roadmap = try loadRoadmap(from: url)
        try validateRoadmapSimulatorPolicy(roadmap)
        print("pass: \(relativePath(url))")
    case "validate-report":
        guard let path = CommandLine.arguments.dropFirst(2).first else {
            throw HarnessError.usage("usage: tcti-next-step.swift validate-report <report.json>")
        }
        let url = URL(fileURLWithPath: path, relativeTo: root).standardizedFileURL
        let object = try loadJSONObject(url)
        try validateReportProofTierMetadata(object, path: relativePath(url))
        print("pass: \(relativePath(url))")
    case "semantic-freshness-check":
        try validateSemanticFreshnessFixtures()
    case "gate-result-policy-check":
        try validateGateResultPolicyFixtures()
    default:
        throw HarnessError.usage("usage: tcti-next-step.swift [status|next|check|validate-roadmap [roadmap.json]|validate-report <report.json>|semantic-freshness-check|gate-result-policy-check]")
    }
} catch {
    fputs("agent next-step error: \(error)\n", stderr)
    exit(1)
}
