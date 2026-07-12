#!/usr/bin/env swift
import Foundation

struct Roadmap: Decodable {
    let area: String
    let description: String
    let gates: [Gate]
    let runtimePreflightGateIDs: [String]

    enum CodingKeys: String, CodingKey {
        case area
        case description
        case gates
        case runtimePreflightGateIDs = "runtime_preflight_gate_ids"
    }
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
    let reportProductVersion: String?
    let reportProductBuildID: String?
    let currentProductVersion: String
    let currentProductBuildID: String
    let executionFresh: Bool
    let statusRecomputed: Bool
    let changedPathsSinceReport: [String]
    let ignoredNonExecutionPaths: [String]
    let invalidatingPaths: [String]
    let reason: String

    enum CodingKeys: String, CodingKey {
        case reportGitSHA = "report_git_sha"
        case currentGitSHA = "current_git_sha"
        case reportProductVersion = "report_product_version"
        case reportProductBuildID = "report_product_build_id"
        case currentProductVersion = "current_product_version"
        case currentProductBuildID = "current_product_build_id"
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
    let productVersion: String?
    let productBuildID: String?
    let simulatorRuntimeIdentifier: String?
    let simulatorRuntimeVersion: String?
    let simulatorRuntimeBuild: String?
    let executionFreshness: ExecutionFreshness?
    let failures: [ReportFailureFact]
    let forbiddenBehaviorViolations: [String]
    let failureStage: String?
    let failureKind: String?
    let failureExitStatus: String?
    let failureTimeoutSeconds: String?
    let productLaunchAttempted: Bool?

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
        productVersion: String? = nil,
        productBuildID: String? = nil,
        simulatorRuntimeIdentifier: String? = nil,
        simulatorRuntimeVersion: String? = nil,
        simulatorRuntimeBuild: String? = nil,
        executionFreshness: ExecutionFreshness? = nil,
        failures: [ReportFailureFact] = [],
        forbiddenBehaviorViolations: [String] = [],
        failureStage: String? = nil,
        failureKind: String? = nil,
        failureExitStatus: String? = nil,
        failureTimeoutSeconds: String? = nil,
        productLaunchAttempted: Bool? = nil
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
        self.productVersion = productVersion
        self.productBuildID = productBuildID
        self.simulatorRuntimeIdentifier = simulatorRuntimeIdentifier
        self.simulatorRuntimeVersion = simulatorRuntimeVersion
        self.simulatorRuntimeBuild = simulatorRuntimeBuild
        self.executionFreshness = executionFreshness
        self.failures = failures
        self.forbiddenBehaviorViolations = forbiddenBehaviorViolations
        self.failureStage = failureStage
        self.failureKind = failureKind
        self.failureExitStatus = failureExitStatus
        self.failureTimeoutSeconds = failureTimeoutSeconds
        self.productLaunchAttempted = productLaunchAttempted
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
            productVersion: productVersion,
            productBuildID: productBuildID,
            simulatorRuntimeIdentifier: simulatorRuntimeIdentifier,
            simulatorRuntimeVersion: simulatorRuntimeVersion,
            simulatorRuntimeBuild: simulatorRuntimeBuild,
            executionFreshness: freshness,
            failures: failures,
            forbiddenBehaviorViolations: forbiddenBehaviorViolations,
            failureStage: failureStage,
            failureKind: failureKind,
            failureExitStatus: failureExitStatus,
            failureTimeoutSeconds: failureTimeoutSeconds,
            productLaunchAttempted: productLaunchAttempted
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
        case productVersion = "product_version"
        case productBuildID = "product_build_id"
        case simulatorRuntimeIdentifier = "simulator_runtime_identifier"
        case simulatorRuntimeVersion = "simulator_runtime_version"
        case simulatorRuntimeBuild = "simulator_runtime_build"
        case executionFreshness = "execution_freshness"
        case failures
        case forbiddenBehaviorViolations = "forbidden_behavior_violations"
        case failureStage = "failure_stage"
        case failureKind = "failure_kind"
        case failureExitStatus = "failure_exit_status"
        case failureTimeoutSeconds = "failure_timeout_seconds"
        case productLaunchAttempted = "product_launch_attempted"
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

let productExecutionInvalidationPatterns = [
    "project.yml",
    "OrlixKernel/Makefile",
    "OrlixKernel/Sources/boot/**",
    "OrlixKernel/Sources/include/**",
    "OrlixKernel/Sources/Support/**",
    "OrlixKernel/Sources/ports/orlix/**",
    "OrlixHostAdapter/Makefile",
    "OrlixHostAdapter/Sources/**",
    "OrlixMLibC/Makefile",
    "OrlixMLibC/Sources/**",
    "OrlixOS/Makefile",
    "OrlixOS/Sources/**",
    "Orlix/Makefile",
    "Orlix/Sources/**",
    "OrlixTestRunner/Sources/**",
]

func matchesAny(_ path: String, _ patterns: [String]) -> Bool {
    patterns.contains { pathMatches(path, $0) }
}

func doesChangedPathInvalidateGate(_ gate: Gate, changedPath: String) -> Bool {
    if changedPath == "project.yml" {
        return gate.kind == "simulator-runtime" ||
            gate.kind == "simulator-runtime-real-stack" ||
            gate.physicalDevice
    }
    if gate.realStackRequired {
        return matchesAny(changedPath, productExecutionInvalidationPatterns)
    }
    guard !changedPath.hasPrefix("docs/") else { return false }
    return matchesAny(changedPath, gate.allowedScope)
}

func changedPathsSinceReport(reportGitSHA: String, currentGitSHA: String) -> [String]? {
    let cacheKey = "\(reportGitSHA)..\(currentGitSHA)"
    if let cached = changedPathsByCommitRange[cacheKey] { return cached }
    guard let output = run("/usr/bin/env", ["git", "diff", "--name-only", "\(reportGitSHA)..\(currentGitSHA)"]) else {
        return nil
    }
    let paths = output
        .split(whereSeparator: \.isNewline)
        .map(String.init)
        .filter { !$0.isEmpty }
    changedPathsByCommitRange[cacheKey] = paths
    return paths
}

struct ProjectVersion {
    let marketingVersion: String
    let buildID: String
}

var cachedCurrentProjectVersion: ProjectVersion?
var didLoadCurrentProjectVersion = false
var projectVersionByGitSHA: [String: ProjectVersion] = [:]
var cachedWorkingTreeChangedPaths: [String]?
var changedPathsByCommitRange: [String: [String]] = [:]

func projectVersion(from contents: String) -> ProjectVersion? {
    var marketingVersion: String?
    var buildID: String?
    for line in contents.split(whereSeparator: \.isNewline).map(String.init) {
        let fields = line.split(separator: ":", maxSplits: 1).map(String.init)
        guard fields.count == 2 else { continue }
        let key = fields[0].trimmingCharacters(in: .whitespaces)
        let value = fields[1]
            .trimmingCharacters(in: .whitespaces)
            .trimmingCharacters(in: CharacterSet(charactersIn: "\"'"))
        if key == "MARKETING_VERSION" { marketingVersion = value }
        if key == "CURRENT_PROJECT_VERSION" { buildID = value }
    }
    guard let marketingVersion, let buildID, !marketingVersion.isEmpty, !buildID.isEmpty else {
        return nil
    }
    return ProjectVersion(marketingVersion: marketingVersion, buildID: buildID)
}

func currentProjectVersion() -> ProjectVersion? {
    if didLoadCurrentProjectVersion { return cachedCurrentProjectVersion }
    didLoadCurrentProjectVersion = true
    let url = root.appendingPathComponent("project.yml")
    guard let contents = try? String(contentsOf: url, encoding: .utf8) else { return nil }
    cachedCurrentProjectVersion = projectVersion(from: contents)
    return cachedCurrentProjectVersion
}

struct SimulatorRuntimeIdentity {
    let identifier: String
    let version: String
    let build: String
}

var cachedSimulatorRuntimeIdentity: SimulatorRuntimeIdentity??

func currentSimulatorRuntimeIdentity() -> SimulatorRuntimeIdentity? {
    if let cachedSimulatorRuntimeIdentity { return cachedSimulatorRuntimeIdentity }
    let helper = root.appendingPathComponent("tools/runtime/orlix-simulator-runtime-identity.py").path
    guard let output = run("/usr/bin/env", ["python3", helper, "--device-id", requiredSimulatorID]),
          let data = output.data(using: .utf8),
          let object = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
          let identifier = stringValue(object["identifier"]),
          let version = stringValue(object["version"]),
          let build = stringValue(object["build"]) else {
        cachedSimulatorRuntimeIdentity = .some(nil)
        return nil
    }
    let identity = SimulatorRuntimeIdentity(identifier: identifier, version: version, build: build)
    cachedSimulatorRuntimeIdentity = .some(identity)
    return identity
}

func projectVersion(at gitSHA: String) -> ProjectVersion? {
    if let cached = projectVersionByGitSHA[gitSHA] { return cached }
    guard let contents = run("/usr/bin/env", ["git", "show", "\(gitSHA):project.yml"]) else {
        return nil
    }
    guard let version = projectVersion(from: contents) else { return nil }
    projectVersionByGitSHA[gitSHA] = version
    return version
}

func currentWorkingTreeChangedPaths() -> [String] {
    if let cachedWorkingTreeChangedPaths { return cachedWorkingTreeChangedPaths }
    guard let output = run("/usr/bin/env", ["git", "status", "--porcelain=v1", "--untracked-files=all"]) else {
        return []
    }
    let paths: [String] = output.split(whereSeparator: \.isNewline).compactMap { rawLine -> String? in
        let line = String(rawLine)
        guard line.count > 3 else { return nil }
        let path = String(line.dropFirst(3))
        if let renameSeparator = path.range(of: " -> ") {
            return String(path[renameSeparator.upperBound...])
        }
        return path
    }
    cachedWorkingTreeChangedPaths = paths
    return paths
}

func executionFreshness(
    reportGitSHA: String?,
    reportProductVersion: String? = nil,
    reportProductBuildID: String? = nil
) -> ExecutionFreshness {
    let current = gitSHA()
    guard let currentVersion = currentProjectVersion() else {
        return ExecutionFreshness(
            reportGitSHA: reportGitSHA,
            currentGitSHA: current,
            reportProductVersion: nil,
            reportProductBuildID: nil,
            currentProductVersion: "unknown",
            currentProductBuildID: "unknown",
            executionFresh: false,
            statusRecomputed: true,
            changedPathsSinceReport: [],
            ignoredNonExecutionPaths: [],
            invalidatingPaths: [],
            reason: "project.yml lacks MARKETING_VERSION or CURRENT_PROJECT_VERSION"
        )
    }
    guard let reportGitSHA, !reportGitSHA.isEmpty else {
        return ExecutionFreshness(
            reportGitSHA: reportGitSHA,
            currentGitSHA: current,
            reportProductVersion: nil,
            reportProductBuildID: nil,
            currentProductVersion: currentVersion.marketingVersion,
            currentProductBuildID: currentVersion.buildID,
            executionFresh: false,
            statusRecomputed: true,
            changedPathsSinceReport: [],
            ignoredNonExecutionPaths: [],
            invalidatingPaths: [],
            reason: "report lacks git_sha provenance"
        )
    }
    let explicitReportVersion: ProjectVersion? = {
        guard let reportProductVersion, !reportProductVersion.isEmpty,
              let reportProductBuildID, !reportProductBuildID.isEmpty else { return nil }
        return ProjectVersion(marketingVersion: reportProductVersion, buildID: reportProductBuildID)
    }()
    guard let reportVersion = explicitReportVersion ?? projectVersion(at: reportGitSHA) else {
        return ExecutionFreshness(
            reportGitSHA: reportGitSHA,
            currentGitSHA: current,
            reportProductVersion: nil,
            reportProductBuildID: nil,
            currentProductVersion: currentVersion.marketingVersion,
            currentProductBuildID: currentVersion.buildID,
            executionFresh: false,
            statusRecomputed: true,
            changedPathsSinceReport: [],
            ignoredNonExecutionPaths: [],
            invalidatingPaths: [],
            reason: "unable to read product version and build ID from project.yml at report git_sha"
        )
    }
    guard let committedChanged = changedPathsSinceReport(reportGitSHA: reportGitSHA, currentGitSHA: current) else {
        return ExecutionFreshness(
            reportGitSHA: reportGitSHA,
            currentGitSHA: current,
            reportProductVersion: reportVersion.marketingVersion,
            reportProductBuildID: reportVersion.buildID,
            currentProductVersion: currentVersion.marketingVersion,
            currentProductBuildID: currentVersion.buildID,
            executionFresh: false,
            statusRecomputed: true,
            changedPathsSinceReport: [],
            ignoredNonExecutionPaths: [],
            invalidatingPaths: [],
            reason: "unable to compute changed paths since report git_sha"
        )
    }
    let workingChanged = currentWorkingTreeChangedPaths()
    let changed = Array(Set(committedChanged + workingChanged)).sorted()
    let committedInvalidating = committedChanged.filter { matchesAny($0, productExecutionInvalidationPatterns) }
    let workingInvalidating = workingChanged.filter { matchesAny($0, productExecutionInvalidationPatterns) }
    let invalidating = changed.filter { matchesAny($0, productExecutionInvalidationPatterns) }
    let ignored = changed.filter { !invalidating.contains($0) }
    let versionMatches = reportVersion.marketingVersion == currentVersion.marketingVersion
    let buildMatches = reportVersion.buildID == currentVersion.buildID
    let committedVersion = projectVersion(at: current)
    let pendingBuildBump = committedVersion?.marketingVersion != currentVersion.marketingVersion ||
        committedVersion?.buildID != currentVersion.buildID
    let provenanceVersion = projectVersion(at: reportGitSHA)
    let reportTestedPendingBuild = provenanceVersion?.marketingVersion != reportVersion.marketingVersion ||
        provenanceVersion?.buildID != reportVersion.buildID
    if explicitReportVersion != nil && versionMatches && buildMatches &&
        (workingInvalidating.isEmpty || pendingBuildBump) &&
        (committedInvalidating.isEmpty || reportTestedPendingBuild) {
        return ExecutionFreshness(
            reportGitSHA: reportGitSHA,
            currentGitSHA: current,
            reportProductVersion: reportVersion.marketingVersion,
            reportProductBuildID: reportVersion.buildID,
            currentProductVersion: currentVersion.marketingVersion,
            currentProductBuildID: currentVersion.buildID,
            executionFresh: true,
            statusRecomputed: true,
            changedPathsSinceReport: changed,
            ignoredNonExecutionPaths: ignored,
            invalidatingPaths: [],
            reason: pendingBuildBump || reportTestedPendingBuild
                ? "report matches pending product version/build; report git_sha is provenance only"
                : "product version/build match; report git_sha is provenance only"
        )
    }
    if versionMatches && buildMatches && invalidating.isEmpty {
        return ExecutionFreshness(
            reportGitSHA: reportGitSHA,
            currentGitSHA: current,
            reportProductVersion: reportVersion.marketingVersion,
            reportProductBuildID: reportVersion.buildID,
            currentProductVersion: currentVersion.marketingVersion,
            currentProductBuildID: currentVersion.buildID,
            executionFresh: true,
            statusRecomputed: true,
            changedPathsSinceReport: changed,
            ignoredNonExecutionPaths: ignored,
            invalidatingPaths: [],
            reason: "product version/build match; git_sha differs only by proof or harness paths"
        )
    }
    let reason: String
    if versionMatches && buildMatches {
        reason = "product inputs changed without bumping CURRENT_PROJECT_VERSION: \(invalidating.joined(separator: ", "))"
    } else {
        reason = "product version/build changed from \(reportVersion.marketingVersion) (\(reportVersion.buildID)) to \(currentVersion.marketingVersion) (\(currentVersion.buildID))"
    }
    return ExecutionFreshness(
        reportGitSHA: reportGitSHA,
        currentGitSHA: current,
        reportProductVersion: reportVersion.marketingVersion,
        reportProductBuildID: reportVersion.buildID,
        currentProductVersion: currentVersion.marketingVersion,
        currentProductBuildID: currentVersion.buildID,
        executionFresh: false,
        statusRecomputed: true,
        changedPathsSinceReport: changed,
        ignoredNonExecutionPaths: ignored,
        invalidatingPaths: invalidating,
        reason: reason
    )
}

func executionFreshness(
    for gate: Gate,
    reportGitSHA: String?,
    reportProductVersion: String? = nil,
    reportProductBuildID: String? = nil
) -> ExecutionFreshness {
    let base = executionFreshness(
        reportGitSHA: reportGitSHA,
        reportProductVersion: reportProductVersion,
        reportProductBuildID: reportProductBuildID
    )
    guard !base.executionFresh,
          gate.kind != "simulator-runtime",
          gate.kind != "simulator-runtime-real-stack",
          !gate.physicalDevice,
          let reportGitSHA, !reportGitSHA.isEmpty,
          let committedChanged = changedPathsSinceReport(
              reportGitSHA: reportGitSHA,
              currentGitSHA: base.currentGitSHA
          ) else {
        return base
    }
    let changed = Array(Set(committedChanged + currentWorkingTreeChangedPaths())).sorted()
    let invalidating = changed.filter { doesChangedPathInvalidateGate(gate, changedPath: $0) }
    guard invalidating.isEmpty else { return base }
    return ExecutionFreshness(
        reportGitSHA: base.reportGitSHA,
        currentGitSHA: base.currentGitSHA,
        reportProductVersion: base.reportProductVersion,
        reportProductBuildID: base.reportProductBuildID,
        currentProductVersion: base.currentProductVersion,
        currentProductBuildID: base.currentProductBuildID,
        executionFresh: true,
        statusRecomputed: true,
        changedPathsSinceReport: changed,
        ignoredNonExecutionPaths: changed,
        invalidatingPaths: [],
        reason: "gate inputs unchanged; product version/build does not invalidate this non-runtime proof"
    )
}

func executionFreshness(for gate: Gate, report: ReportFact) -> ExecutionFreshness {
    let base = executionFreshness(
        for: gate,
        reportGitSHA: report.gitSHA,
        reportProductVersion: report.productVersion,
        reportProductBuildID: report.productBuildID
    )
    guard base.executionFresh, gate.kind == "simulator-runtime", !gate.physicalDevice else {
        return base
    }
    guard let current = currentSimulatorRuntimeIdentity(),
          report.simulatorRuntimeIdentifier == current.identifier,
          report.simulatorRuntimeBuild == current.build else {
        let reportIdentity = "\(report.simulatorRuntimeIdentifier ?? "missing")/\(report.simulatorRuntimeBuild ?? "missing")"
        let currentIdentity = currentSimulatorRuntimeIdentity().map { "\($0.identifier)/\($0.build)" } ?? "unavailable"
        return ExecutionFreshness(
            reportGitSHA: base.reportGitSHA,
            currentGitSHA: base.currentGitSHA,
            reportProductVersion: base.reportProductVersion,
            reportProductBuildID: base.reportProductBuildID,
            currentProductVersion: base.currentProductVersion,
            currentProductBuildID: base.currentProductBuildID,
            executionFresh: false,
            statusRecomputed: true,
            changedPathsSinceReport: base.changedPathsSinceReport,
            ignoredNonExecutionPaths: base.ignoredNonExecutionPaths,
            invalidatingPaths: base.invalidatingPaths,
            reason: "simulator runtime changed from \(reportIdentity) to \(currentIdentity)"
        )
    }
    return base
}

func reportExecutionFresh(_ report: ReportFact) -> Bool {
    (report.executionFreshness ?? executionFreshness(
        reportGitSHA: report.gitSHA,
        reportProductVersion: report.productVersion,
        reportProductBuildID: report.productBuildID
    )).executionFresh
}

func reportFreshnessReason(_ report: ReportFact) -> String {
    (report.executionFreshness ?? executionFreshness(
        reportGitSHA: report.gitSHA,
        reportProductVersion: report.productVersion,
        reportProductBuildID: report.productBuildID
    )).reason
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
    let failureContext = status.reports.flatMap {
        [$0.failureStage, $0.failureKind, $0.failureExitStatus, $0.failureTimeoutSeconds].compactMap { $0 }
    }
    return ([status.id, status.kind, status.state, status.reason] + failures + reportPaths + failureContext)
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
    let simulatorInstallTimeout = status.reports.contains { report in
        guard report.exists && !report.passed,
              report.failureStage?.lowercased() == "simulator-install",
              report.productLaunchAttempted != true else {
            return false
        }
        if report.failureKind?.lowercased() == "timeout" {
            return true
        }
        return report.failureKind == nil &&
            report.failureExitStatus == "124" &&
            (Int(report.failureTimeoutSeconds ?? "") ?? 0) > 0
    }
    if simulatorInstallTimeout {
        return policy(
            classification: "environment_only_failure",
            runtimePatchAllowed: false,
            harnessPatchAllowed: false,
            continueRefreshAllowed: false,
            mustStop: true,
            requiredNextAction: "fix or rerun the environment/simulator setup before changing product code",
            reason: "The current report timed out during simulator installation before product launch or runtime evidence.",
            owningLayer: "environment"
        )
    }
    let currentPassReducerRefresh = status.kind == "no-phone-reducer" &&
        status.state == "ready" &&
        evidenceText.contains("current passing static busybox report") &&
        evidenceText.contains("refresh exact report linkage") &&
        selectedCommandCanGenerateProof(status)
    if currentPassReducerRefresh {
        return policy(
            classification: "stale_proof_refresh",
            runtimePatchAllowed: false,
            harnessPatchAllowed: false,
            continueRefreshAllowed: true,
            mustStop: false,
            requiredNextAction: "refresh the selected reducer against the exact current passing runtime report",
            reason: status.reason,
            owningLayer: "proof refresh"
        )
    }

    let currentPassUserDataRailRefresh = status.id == "tcti-user-data-window-refresh-fix" &&
        status.state == "ready" &&
        evidenceText.contains("user-data window rail requires the current post-busybox reducer and fix reports to cover") &&
        selectedCommandCanGenerateProof(status)
    if currentPassUserDataRailRefresh {
        return policy(
            classification: "stale_proof_refresh",
            runtimePatchAllowed: false,
            harnessPatchAllowed: false,
            continueRefreshAllowed: true,
            mustStop: false,
            requiredNextAction: "refresh the selected rail against the exact current passing runtime report and reducer",
            reason: status.reason,
            owningLayer: "proof refresh"
        )
    }

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
    let failureIDs = status.reports.flatMap(\.failures).map(\.id)
    let exclusivelyStaleNoPhoneReducer = status.kind == "no-phone-reducer" &&
        !failureIDs.isEmpty &&
        Set(failureIDs) == Set(["simulator-report-stale"])
    let railLike = status.kind == "rail" || status.kind.contains("production")
    if exclusivelyStaleNoPhoneReducer ||
        (railLike && railContractTerms.contains(where: { evidenceText.contains($0) })) {
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
        "bootstatus",
        "booted simulator",
        "storage",
        "runner attach",
        "test runner hung before establishing connection",
        "connection to remote process was not established",
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
            productVersion: stringValue(object["product_version"]),
            productBuildID: stringValue(object["product_build_id"]),
            simulatorRuntimeIdentifier: stringValue(object["simulator_runtime_identifier"]),
            simulatorRuntimeVersion: stringValue(object["simulator_runtime_version"]),
            simulatorRuntimeBuild: stringValue(object["simulator_runtime_build"]),
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
    return report.withExecutionFreshness(executionFreshness(for: gate, report: report))
}

func currentReportPassed(_ report: ReportFact) -> Bool {
    report.status == "pass" && report.passed && reportExecutionFresh(report)
}

func pathExists(_ relative: String) -> Bool {
    fileManager.fileExists(atPath: root.appendingPathComponent(relative).path)
}

func sha256(_ path: String) -> String? {
    let url = path.hasPrefix("/") ? URL(fileURLWithPath: path) : root.appendingPathComponent(path)
    return run("/usr/bin/shasum", ["-a", "256", url.path])?
        .split(whereSeparator: { $0.isWhitespace })
        .first
        .map(String.init)
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
        guard let expectedSourceHash = stringValue(object["source_sha256"]),
              let expectedBinaryHash = stringValue(object["binary_sha256"]),
              let binary = stringValue(object["binary"]) else {
            return (false, "\(validation) is missing source/binary hash fields", [])
        }
        guard sha256(source) == expectedSourceHash else {
            return (false, "\(validation) source hash does not match \(source)", [])
        }
        guard sha256(binary) == expectedBinaryHash else {
            return (false, "\(validation) binary hash does not match \(binary)", [])
        }
        return (true, "\(caseID) structural validation source and binary hashes match", [])
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
			reportExecutionFresh(report) &&
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
		   reportExecutionFresh(reducerReport),
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
	   reportExecutionFresh(reducerReport) {
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

func currentStaticBusyBoxPassEvidence() -> (ReportFact, [String: Any])? {
	guard let (report, object) = latestRuntimeReport(gate: "tcti-static-busybox-start", destination: "iphonesimulator"),
		report.status == "pass",
		report.passed,
		reportExecutionFresh(report),
		stringValue(object["selected_device_id"]) == "ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3",
		stringValue(object["selected_device_name"]) == "Orlix-iPhone-15-Pro-Max",
		intValue(object["simulator_booted_count"]) == 1,
		boolValue(object["simulator_single_booted"]),
		object["autonomous_tests_bypassed"] as? Bool == false,
		object["preflight_only"] as? Bool == false,
		let forbidden = object["forbidden_behavior"] as? [String: Any],
		["generated_exec_memory", "host_exec_guest_text", "host_x18", "map_jit", "native_ios_api_exposure_to_guest", "rwx"].allSatisfy({ forbidden[$0] as? Bool == false }),
		let events = object["tcti_runtime_events"] as? [String: Any],
		let staticPIE = events["static_pie_image"] as? [String: Any],
		stringValue(staticPIE["task"]) == "sh",
		intValue(staticPIE["pid"]) != nil,
		let signal = events["signaled_process"] as? [String: Any],
		signal["pid"] is NSNull,
		signal["signal"] is NSNull,
		let fault = events["fatal_user_fault"] as? [String: Any],
		["task", "pid", "pc", "lr", "sp", "addr", "access", "si"].allSatisfy({ fault[$0] is NSNull })
	else {
		return nil
	}
	return (report, object)
}

let postBusyBoxSIGABRTPassRegressionPath = "Build/TCTI/reproducers/tcti-post-busybox-sigabrt-reducer/post-busybox-sigabrt-pass-regression.json"

func postBusyBoxSIGABRTReducerCoversPass(
	reducerReport: ReportFact,
	reducerObject: [String: Any]?,
	reducerArtifacts: [String],
	busyBoxReportPath: String
) -> Bool {
	currentReportPassed(reducerReport) &&
		stringValue(reducerObject?["target"]) == "tcti-post-busybox-sigabrt-reducer" &&
		stringValue(reducerObject?["gate"]) == "tcti-post-busybox-sigabrt-reducer" &&
		reducerArtifacts.contains(busyBoxReportPath) &&
		reducerArtifacts.contains(postBusyBoxSIGABRTPassRegressionPath)
}

func userDataWindowRefreshFixPass(_ gate: Gate) -> GateStatus {
	let fixReport = reportFact(target: "tcti-user-data-window-refresh-fix")
	let fixObject = try? loadJSONObject(root.appendingPathComponent(fixReport.path))
	let fixArtifacts = (fixObject?["artifacts"] as? [Any] ?? []).compactMap { stringValue($0) }
	let reducerReport = reportFact(target: "tcti-post-busybox-sigabrt-reducer")
	let reducerObject = try? loadJSONObject(root.appendingPathComponent(reducerReport.path))
	let reducerArtifacts = (reducerObject?["artifacts"] as? [Any] ?? []).compactMap { stringValue($0) }
	let expectedReducerPath = postBusyBoxSIGABRTPassRegressionPath
	let currentBusyBoxEvidence = currentStaticBusyBoxPassEvidence()
	let currentBusyBoxPath = currentBusyBoxEvidence?.0.path
	let reducerIdentityOK = stringValue(reducerObject?["target"]) == "tcti-post-busybox-sigabrt-reducer" &&
		stringValue(reducerObject?["gate"]) == "tcti-post-busybox-sigabrt-reducer"
	let reducerCoversCurrentBusyBox = currentBusyBoxPath.map { reducerArtifacts.contains($0) && reducerArtifacts.contains(expectedReducerPath) } ?? false
	let fixCoversCurrentBusyBox = currentBusyBoxPath.map { fixArtifacts.contains($0) } ?? false
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
			reportExecutionFresh(report) &&
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
		   reportExecutionFresh(fixReport),
		   reducerReport.status == "pass",
		   reducerReport.passed,
		   reportExecutionFresh(reducerReport),
		   currentBusyBoxEvidence != nil,
		   reducerIdentityOK,
		   reducerCoversCurrentBusyBox,
		   fixCoversCurrentBusyBox,
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
			reason: "current simulator report \(report.path) records the reducer-backed user-data window fault and the fix report does not cover that report yet",
			prerequisites: gate.prerequisites,
			prerequisitesSatisfied: false,
			reportPaths: gate.expectedReportPaths,
			reports: [report, reducerReport, fixReport],
			readinessEligible: gate.readinessEligible,
			physicalDevice: gate.physicalDevice,
			gadget: gate.gadget
		)
	}

	guard let (currentBusyBoxReport, _) = currentBusyBoxEvidence else {
		return GateStatus(
			id: gate.id,
			command: gate.command,
			kind: gate.kind,
			state: "ready",
			passed: false,
			reason: "current pinned static BusyBox pass evidence is missing or incomplete for the user-data window rail",
			prerequisites: gate.prerequisites,
			prerequisitesSatisfied: false,
			reportPaths: gate.expectedReportPaths,
			reports: [reducerReport, fixReport].filter(\.exists),
			readinessEligible: gate.readinessEligible,
			physicalDevice: gate.physicalDevice,
			gadget: gate.gadget
		)
	}
	if !reducerIdentityOK || !currentReportPassed(reducerReport) || !reducerCoversCurrentBusyBox || !fixCoversCurrentBusyBox {
		return GateStatus(
			id: gate.id,
			command: gate.command,
			kind: gate.kind,
			state: "ready",
			passed: false,
			reason: "user-data window rail requires the current post-BusyBox reducer and fix reports to cover \(currentBusyBoxReport.path)",
			prerequisites: gate.prerequisites,
			prerequisitesSatisfied: false,
			reportPaths: gate.expectedReportPaths,
			reports: [currentBusyBoxReport, reducerReport, fixReport],
			readinessEligible: gate.readinessEligible,
			physicalDevice: gate.physicalDevice,
			gadget: gate.gadget
		)
	}

	return basicReportGate(gate, target: "tcti-user-data-window-refresh-fix")
}

func passingRuntimeReportDefersFailureReducer(_ report: ReportFact) -> Bool {
	report.status == "pass" && report.passed
}

func postBusyBoxShellCommandSIGILLReducerPass(_ gate: Gate) -> GateStatus {
	let reducerReport = reportFact(target: "tcti-post-busybox-shell-command-sigill-reducer")
	let reducerObject = try? loadJSONObject(root.appendingPathComponent(reducerReport.path))
	let reducerArtifacts = (reducerObject?["artifacts"] as? [Any] ?? []).compactMap { stringValue($0) }
	guard let (report, object) = latestRuntimeReport(gate: "tcti-static-busybox-shell-command", destination: "iphonesimulator") else {
		return GateStatus(
			id: gate.id,
			command: gate.command,
			kind: gate.kind,
			state: "not_needed",
			passed: false,
			satisfiesPrerequisite: true,
			reason: "static BusyBox shell-command simulator evidence is missing; run the simulator producer before selecting a SIGILL reducer",
			prerequisites: gate.prerequisites,
			prerequisitesSatisfied: false,
			reportPaths: gate.expectedReportPaths,
			reports: reducerReport.exists ? [reducerReport] : [],
			readinessEligible: gate.readinessEligible,
			physicalDevice: gate.physicalDevice,
			gadget: gate.gadget
		)
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
		reportExecutionFresh(report) &&
		text.contains("ORLIX-TCTI-BUSYBOX-USABLE") &&
		text.contains("Orlix TCTI: unsupported instruction task=sh") &&
		text.contains("signal=4")
	if matchesCurrentSIGILL {
		let reducerCoversReport = reducerArtifacts.contains(report.path)
		if reducerReport.status == "pass",
		   reducerReport.passed,
		   reportExecutionFresh(reducerReport),
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
	if passingRuntimeReportDefersFailureReducer(report) {
		return GateStatus(
			id: gate.id,
			command: gate.command,
			kind: gate.kind,
			state: "not_needed",
			passed: false,
			satisfiesPrerequisite: true,
			reason: reportExecutionFresh(report)
				? "current static BusyBox shell-command simulator report passes without the marker-then-SIGILL signature"
				: "passing static BusyBox shell-command simulator evidence is stale; refresh the simulator producer before selecting a SIGILL reducer",
			prerequisites: gate.prerequisites,
			prerequisitesSatisfied: false,
			reportPaths: gate.expectedReportPaths,
			reports: [report],
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
    let currentRuntime = destination == "iphonesimulator" ? currentSimulatorRuntimeIdentity() : nil
    for url in candidates {
        guard let object = try? loadJSONObject(url),
              stringValue(object["gate"]) == gateName,
              stringValue(object["destination"]) == destination
        else {
            continue
        }
        if let currentRuntime,
           (stringValue(object["simulator_runtime_identifier"]) != currentRuntime.identifier ||
            stringValue(object["simulator_runtime_build"]) != currentRuntime.build) {
            continue
        }
        let failureContext = object["failure_context"] as? [String: Any]
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
            productVersion: stringValue(object["product_version"]),
            productBuildID: stringValue(object["product_build_id"]),
            simulatorRuntimeIdentifier: stringValue(object["simulator_runtime_identifier"]),
            simulatorRuntimeVersion: stringValue(object["simulator_runtime_version"]),
            simulatorRuntimeBuild: stringValue(object["simulator_runtime_build"]),
            failures: reportFailures(object["failures"]),
            forbiddenBehaviorViolations: forbiddenBehaviorViolations(object["forbidden_behavior"]),
            failureStage: stringValue(failureContext?["stage"]),
            failureKind: stringValue(failureContext?["kind"]),
            failureExitStatus: stringValue(failureContext?["exit_status"]),
            failureTimeoutSeconds: stringValue(failureContext?["timeout_seconds"]),
            productLaunchAttempted: failureContext?["product_launch_attempted"].map(boolValue)
        )
        return (fact, object)
    }
    return nil
}

func runtimeGateState(report: ReportFact, passed: Bool) -> String {
    if passed {
        return "pass"
    }
    if !reportExecutionFresh(report) || report.status == "pass" {
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
    let report = latest.0.withExecutionFreshness(executionFreshness(for: gate, report: latest.0))
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

func physicalFirstSyscallPass(_ gate: Gate) -> GateStatus {
    guard let latest = latestRuntimeReport(gate: "tcti-init-first-syscall", destination: "iphoneos") else {
        return missingGate(gate, reason: "missing iphoneos runtime-validation report for tcti-init-first-syscall")
    }
    let report = latest.0.withExecutionFreshness(executionFreshness(for: gate, report: latest.0))
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
        !(stringValue(object["selected_device_id"]) ?? "").isEmpty &&
        stringValue(object["backend"]) == "tcti" &&
        stringValue(object["profile"]) == "tcti_runtime" &&
        !boolValue(object["preflight_only"]) &&
        !boolValue(object["autonomous_tests_bypassed"]) &&
        forbiddenClear &&
        runtimeArtifactContains(object, suffix: "tcti-first-syscall.txt", marker: "Orlix TCTI: svc #0")
    let reason: String
    if reportOK {
        reason = "iphoneos runtime-validation report \(report.path) passed on device \(stringValue(object["selected_device_id"]) ?? "unknown") with tcti runtime profile and forbidden behavior false"
    } else if !reportExecutionFresh(report) {
        reason = "latest iphoneos runtime-validation report \(report.path) is execution-stale; \(reportFreshnessReason(report))"
    } else if !runtimeArtifactContains(object, suffix: "tcti-first-syscall.txt", marker: "Orlix TCTI: svc #0") {
        reason = "latest iphoneos runtime-validation report \(report.path) did not include the first TCTI svc marker artifact"
    } else {
        reason = "latest iphoneos runtime-validation report \(report.path) is not a valid non-preflight TCTI pass"
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
    let report = latest.0.withExecutionFreshness(executionFreshness(for: gate, report: latest.0))
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
    let report = latest.0.withExecutionFreshness(executionFreshness(for: gate, report: latest.0))
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
    let report = latest.0.withExecutionFreshness(executionFreshness(for: gate, report: latest.0))
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
    let report = latest.0.withExecutionFreshness(executionFreshness(for: gate, report: latest.0))
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
    let report = latest.0.withExecutionFreshness(executionFreshness(for: gate, report: latest.0))
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
			reportExecutionFresh(report) &&
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
		   reportExecutionFresh(reducerReport),
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

	if let (currentBusyBoxReport, _) = currentStaticBusyBoxPassEvidence() {
		if postBusyBoxSIGABRTReducerCoversPass(
			reducerReport: reducerReport,
			reducerObject: reducerObject,
			reducerArtifacts: reducerArtifacts,
			busyBoxReportPath: currentBusyBoxReport.path
		) {
			return GateStatus(
				id: gate.id,
				command: gate.command,
				kind: gate.kind,
				state: "pass",
				passed: true,
				reason: "post-BusyBox SIGABRT reducer report \(reducerReport.path) covers current passing static BusyBox report \(currentBusyBoxReport.path)",
				prerequisites: gate.prerequisites,
				prerequisitesSatisfied: false,
				reportPaths: gate.expectedReportPaths,
				reports: [currentBusyBoxReport, reducerReport],
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
			reason: "current passing static BusyBox report \(currentBusyBoxReport.path) requires the post-BusyBox SIGABRT reducer to refresh exact report linkage",
			prerequisites: gate.prerequisites,
			prerequisitesSatisfied: false,
			reportPaths: gate.expectedReportPaths,
			reports: reducerReport.exists ? [currentBusyBoxReport, reducerReport] : [currentBusyBoxReport],
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
       reportExecutionFresh(reducerReport) {
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
        reportExecutionFresh(report) &&
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
            reportExecutionFresh(report) &&
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
            reportExecutionFresh(report) &&
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
       reportExecutionFresh(reducerReport),
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
       reportExecutionFresh(fixReport),
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
        reportExecutionFresh(stabilityReport) &&
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
        reportExecutionFresh(report) &&
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
       reportExecutionFresh(reducerReport),
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
       reportExecutionFresh(fixReport),
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
        reportExecutionFresh(report) &&
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
       reportExecutionFresh(reducerReport),
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
       reportExecutionFresh(fixReport),
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
        reportExecutionFresh(report) &&
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
       reportExecutionFresh(reducerReport),
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
        reportExecutionFresh(report) &&
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
       reportExecutionFresh(reducerReport),
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
        reportExecutionFresh(report) &&
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
       reportExecutionFresh(reducerReport),
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
        reportExecutionFresh(report) &&
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
       reportExecutionFresh(reducerReport),
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
       reportExecutionFresh(fixReport) {
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
       reportExecutionFresh(fixReport),
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
        reportExecutionFresh(report) &&
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
       reportExecutionFresh(reducerReport),
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
       reportExecutionFresh(fixReport),
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
        return physicalFirstSyscallPass(gate)
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

func roadmapGatesWithRuntimePreflight(_ roadmap: Roadmap) -> [Gate] {
    roadmap.gates
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
    let missingPreflightGates = roadmap.runtimePreflightGateIDs.filter { !ids.contains($0) }
    if !missingPreflightGates.isEmpty {
        throw HarnessError.invalid("roadmap runtime preflight list references missing gates: \(missingPreflightGates.joined(separator: ","))")
    }
    if Set(roadmap.runtimePreflightGateIDs).count != roadmap.runtimePreflightGateIDs.count {
        throw HarnessError.invalid("roadmap runtime preflight gate IDs must be unique")
    }
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

func physicalBlockers(statuses: [GateStatus]) -> [String] {
    var blockers: [String] = []
    let missingSimulator = simulatorReadinessMissingGateIDs(statuses)
    if !missingSimulator.isEmpty {
        blockers.append("missing_or_failing_simulator_readiness_gates=\(missingSimulator.joined(separator: ","))")
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

func selectedStatusWithSafety(from statuses: [GateStatus], runtimePreflightGateIDs: [String]) -> GateStatus? {
    let simulatorPassed = simulatorRuntimeGatesComplete(statuses)
    _ = runtimePreflightGateIDs
    let physicalAllowed = physicalBlockers(statuses: statuses).isEmpty
    if let finalRuntime = statuses.first(where: {
        $0.id == "simulator-tcti-full-linux-runtime-readiness" &&
            $0.currentlySimulatorReadinessSatisfied
    }), finalRuntime.currentlySimulatorReadinessSatisfied,
       let missingReadiness = simulatorReadinessStatuses(statuses).first(where: {
           !$0.currentlySimulatorReadinessSatisfied
       }) {
        return missingReadiness
    }
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
    if physicalAllowed, let physicalGate = eligible.first(where: { $0.physicalDevice }) {
        return physicalGate
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
    let next = selectedStatusWithSafety(from: gateStatuses, runtimePreflightGateIDs: roadmap.runtimePreflightGateIDs)
    let missingSimulatorReadiness = simulatorReadinessMissingGateIDs(gateStatuses)
    let blockers = physicalBlockers(statuses: gateStatuses)
    let dirtyRuntimeOrHarness = dirtyRuntimeOrHarnessWorktree()
    let physicalAllowed = physicalGate?.prerequisitesSatisfied == true &&
        missingSimulatorReadiness.isEmpty &&
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

func semanticFreshnessFixtureGate(
    id: String,
    command: String,
    kind: String,
    allowedScope: [String] = [],
    realStackRequired: Bool? = nil
) -> Gate {
    Gate(
        id: id,
        command: command,
        kind: kind,
        realStackRequired: realStackRequired,
        prerequisites: [],
        allowedScope: allowedScope,
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
        kind: "simulator-runtime",
        realStackRequired: true
    )
    let tctiGate = semanticFreshnessFixtureGate(
        id: "tcti-kernel-syscall-dispatch-smoke",
        command: "make tcti-gate TARGET=tcti-kernel-syscall-dispatch-smoke",
        kind: "kernel",
        realStackRequired: true
    )
    let kernelGate = semanticFreshnessFixtureGate(
        id: "tcti-kernel-execve-binfmt-elf-smoke",
        command: "make tcti-gate TARGET=tcti-kernel-execve-binfmt-elf-smoke",
        kind: "kernel",
        realStackRequired: true
    )
    let selectorGate = semanticFreshnessFixtureGate(
        id: "tcti-shell-exec-simple-command",
        command: "make tcti-gate TARGET=tcti-shell-exec-simple-command",
        kind: "runtime",
        realStackRequired: true
    )
    let toolchainGate = semanticFreshnessFixtureGate(
        id: "toolchain",
        command: "make tcti-gate TARGET=tcti-toolchain-check",
        kind: "rail",
        allowedScope: ["tools/tcti/orlix-tcti-gate.swift", "OrlixKernel/Tests/TCTI/golden_elf/**"]
    )
    let cases: [(String, Gate, String, Bool)] = [
        ("implement-checkpoint-does-not-rerun-runtime", runtimeGate, "docs/plans/active/orlix-tcti/IMPLEMENT.md", false),
        ("harness-doc-does-not-rerun-runtime", runtimeGate, "docs/harness/ORLIX_TCTI_AGENT_HARNESS.md", false),
        ("runtime-tool-does-not-rebuild-product", runtimeGate, "tools/runtime/orlix-runtime-validation.sh", false),
        ("tcti-tool-does-not-rebuild-product", tctiGate, "tools/tcti/orlix-tcti-gate.swift", false),
        ("kernel-port-reruns-kernel-and-runtime", kernelGate, "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/kernel/syscall.c", true),
        ("host-adapter-reruns-product", runtimeGate, "OrlixHostAdapter/Sources/OrlixHostAdapter/runtime/runtime.c", true),
        ("mlibc-reruns-product", runtimeGate, "OrlixMLibC/Sources/patches/0007-example.patch", true),
        ("coreutils-input-reruns-product", runtimeGate, "OrlixOS/Sources/make/packages.mk", true),
        ("app-source-reruns-product", runtimeGate, "Orlix/Sources/OrlixApp.swift", true),
        ("project-build-id-reruns-product", runtimeGate, "project.yml", true),
        ("project-build-id-does-not-rerun-kernel-proof", kernelGate, "project.yml", false),
        ("environment-policy-does-not-rebuild-product", runtimeGate, ".agents/skills/orlix-tcti-next-step/references/environment-policy.json", false),
        ("selector-script-recomputes-status-only", selectorGate, ".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift", false),
        ("product-build-id-does-not-rerun-toolchain", toolchainGate, "project.yml", false),
        ("golden-input-reruns-toolchain", toolchainGate, "OrlixKernel/Tests/TCTI/golden_elf/init_001_exit.S", true),
    ]
	for (name, gate, path, expected) in cases {
        let actual = doesChangedPathInvalidateGate(gate, changedPath: path)
        if actual != expected {
            throw HarnessError.invalid("semantic freshness fixture \(name) expected invalidates=\(expected) for \(path), got \(actual)")
		}
	}

	let busyBoxPath = "Build/Reports/runtime/tcti-static-busybox-start-current.json"
	let reducerReport = policyFixtureReport(
		path: "Build/TCTI/reports/tcti-post-busybox-sigabrt-reducer/report.json",
		passed: true
	)
	let reducerObject: [String: Any] = [
		"target": "tcti-post-busybox-sigabrt-reducer",
		"gate": "tcti-post-busybox-sigabrt-reducer",
	]
	let reducerLinkageCases: [(String, [String], Bool)] = [
		("missing-current-runtime-report", [postBusyBoxSIGABRTPassRegressionPath], false),
		("missing-pass-regression", [busyBoxPath], false),
		("exact-current-pass-linkage", [busyBoxPath, postBusyBoxSIGABRTPassRegressionPath], true),
	]
	for (name, artifacts, expected) in reducerLinkageCases {
		let actual = postBusyBoxSIGABRTReducerCoversPass(
			reducerReport: reducerReport,
			reducerObject: reducerObject,
			reducerArtifacts: artifacts,
			busyBoxReportPath: busyBoxPath
		)
		if actual != expected {
			throw HarnessError.invalid("post-BusyBox reducer linkage fixture \(name) expected covers=\(expected), got \(actual)")
		}
	}
	let stalePassingRuntimeReport = policyFixtureReport(
		path: "Build/Reports/runtime/tcti-static-busybox-shell-command-stale.json",
		passed: true
	)
	if !passingRuntimeReportDefersFailureReducer(stalePassingRuntimeReport) {
		throw HarnessError.invalid("passing stale runtime producer must defer its failure reducer")
	}
	let failingRuntimeReport = policyFixtureReport(
		path: "Build/Reports/runtime/tcti-static-busybox-shell-command-current.json"
	)
	if passingRuntimeReportDefersFailureReducer(failingRuntimeReport) {
		throw HarnessError.invalid("failing runtime producer must remain eligible for reducer matching")
	}
	let scriptURL = root.appendingPathComponent(".agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift")
	let scriptText = try String(contentsOf: scriptURL, encoding: .utf8)
	let exactHeadComparison = ".gitSHA == " + "gitSHA()"
	if scriptText.contains(exactHeadComparison) {
		throw HarnessError.invalid("report freshness must use reportExecutionFresh instead of exact HEAD equality")
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
    failures: [ReportFailureFact] = [],
    forbiddenBehaviorViolations: [String] = [],
    failureStage: String? = nil,
    failureKind: String? = nil,
    failureExitStatus: String? = nil,
    failureTimeoutSeconds: String? = nil,
    productLaunchAttempted: Bool? = nil
) -> ReportFact {
    return ReportFact(
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
        gitSHA: "fixture-current-head",
        productVersion: "fixture-version",
        productBuildID: "fixture-build",
        executionFreshness: ExecutionFreshness(
            reportGitSHA: "fixture-current-head",
            currentGitSHA: "fixture-current-head",
            reportProductVersion: "fixture-version",
            reportProductBuildID: "fixture-build",
            currentProductVersion: "fixture-version",
            currentProductBuildID: "fixture-build",
            executionFresh: true,
            statusRecomputed: false,
            changedPathsSinceReport: [],
            ignoredNonExecutionPaths: [],
            invalidatingPaths: [],
            reason: "classifier fixture explicitly models current execution evidence"
        ),
        failures: failures,
        forbiddenBehaviorViolations: forbiddenBehaviorViolations,
        failureStage: failureStage,
        failureKind: failureKind,
        failureExitStatus: failureExitStatus,
        failureTimeoutSeconds: failureTimeoutSeconds,
        productLaunchAttempted: productLaunchAttempted
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
    let staleFailedRuntimeReport = policyFixtureReport(
        path: "Build/Reports/runtime/tcti-stale-failure.json"
    ).withExecutionFreshness(ExecutionFreshness(
        reportGitSHA: "old",
        currentGitSHA: "current",
        reportProductVersion: "0.1",
        reportProductBuildID: "16",
        currentProductVersion: "0.1",
        currentProductBuildID: "17",
        executionFresh: false,
        statusRecomputed: true,
        changedPathsSinceReport: ["project.yml"],
        ignoredNonExecutionPaths: [],
        invalidatingPaths: ["project.yml"],
        reason: "product build changed"
    ))
    guard runtimeGateState(report: staleFailedRuntimeReport, passed: false) == "stale" else {
        throw HarnessError.invalid("stale failing runtime report must remain refreshable")
    }

    let fixtures: [(String, GateStatus, String, Bool, Bool, Bool, Bool)] = [
        ("stale-proof-refresh", policyFixtureStatus(id: "stale", state: "stale", reason: "report git_sha is stale"), "stale_proof_refresh", false, false, true, false),
        ("current-pass-reducer-refresh", policyFixtureStatus(id: "no-phone-tcti-post-busybox-sigabrt-reducer", command: "make tcti-gate TARGET=tcti-post-busybox-sigabrt-reducer", kind: "no-phone-reducer", state: "ready", reason: "current passing static BusyBox report Build/Reports/runtime/tcti-static-busybox-start-current.json requires the post-BusyBox SIGABRT reducer to refresh exact report linkage"), "stale_proof_refresh", false, false, true, false),
        ("current-pass-user-data-rail-refresh", policyFixtureStatus(id: "tcti-user-data-window-refresh-fix", command: "make tcti-gate TARGET=tcti-user-data-window-refresh-fix", kind: "production-tcti-fix", proofTier: "rail", state: "ready", reason: "user-data window rail requires the current post-BusyBox reducer and fix reports to cover Build/Reports/runtime/tcti-static-busybox-start-current.json"), "stale_proof_refresh", false, false, true, false),
        ("missing-generated-artifact", policyFixtureStatus(id: "golden-init-001-structural", command: "make tcti-gate TARGET=tcti-golden-elf CASE=init_001_exit", kind: "golden-structural", state: "missing", reason: "validation artifact missing", reports: [policyFixtureReport(path: "Build/TCTI/golden_elf/init_001_exit/validation.json", exists: false)]), "missing_generated_artifact", false, false, true, false),
        ("missing-artifact-without-safe-generator", policyFixtureStatus(id: "unknown", command: "", state: "missing", reason: "artifact missing", reports: [policyFixtureReport(path: "Build/TCTI/unknown.json", exists: false)]), "missing_generated_artifact", false, false, false, true),
        ("rail-evidence-contract-bug", policyFixtureStatus(id: "rail", state: "fail", reason: "historical generated report is obsolete"), "rail_evidence_contract_bug", false, true, false, true),
        ("rail-simulator-freshness-contract-bug", policyFixtureStatus(id: "rail-stale", state: "fail", reason: "simulator-report-stale: latest simulator stability report is not execution-fresh for this rail"), "rail_evidence_contract_bug", false, true, false, true),
        ("no-phone-reducer-simulator-freshness-contract-bug", policyFixtureStatus(id: "reducer-stale", kind: "no-phone-reducer", state: "fail", reason: "simulator-report-stale", reports: [policyFixtureReport(path: "Build/TCTI/reports/tcti-fixture/report.json", failures: [ReportFailureFact(id: "simulator-report-stale", message: "latest simulator stability report is not execution-fresh for this rail")])]), "rail_evidence_contract_bug", false, true, false, true),
        ("no-phone-reducer-mixed-failure-stops-unclassified", policyFixtureStatus(id: "reducer-mixed", kind: "no-phone-reducer", state: "fail", reason: "simulator report stale and negative execution shape failed", reports: [policyFixtureReport(path: "Build/TCTI/reports/tcti-fixture/report.json", failures: [ReportFailureFact(id: "simulator-report-stale", message: "latest simulator stability report is not execution-fresh for this rail"), ReportFailureFact(id: "negative-execution-shape", message: "negative execution shape failed")])]), "current_runtime_product_failure", false, false, false, true),
        ("rail-missing-reducer-contract-bug", policyFixtureStatus(id: "rail-reducer", kind: "production-tcti-fix", state: "fail", reason: "reducer-evidence: missing reducer report or pass-regression evidence"), "rail_evidence_contract_bug", false, true, false, true),
        ("metadata-drift", policyFixtureStatus(id: "drift", kind: "rail", proofTier: "rail", state: "fail", reason: "metadata mismatch", reports: [policyFixtureReport(path: "Build/TCTI/reports/tcti-fixture/report.json", proofTier: "seed")]), "proof_tier_report_status_metadata_drift", false, true, false, true),
        ("runtime-product-failure", policyFixtureStatus(id: "runtime", kind: "kernel", proofTier: "kernel", acceptanceWeight: "blocker", realStackRequired: true, state: "fail", reason: "guest syscall failed", reports: [policyFixtureReport(path: "Build/TCTI/reports/tcti-fixture/report.json", proofTier: "kernel")]), "current_runtime_product_failure", true, false, false, true),
        ("xcodebuild-product-failure", policyFixtureStatus(id: "xcodebuild-product", kind: "kernel", proofTier: "kernel", acceptanceWeight: "blocker", realStackRequired: true, state: "fail", reason: "xcodebuild reached the app-hosted test but OrlixOS package construction failed", reports: [policyFixtureReport(path: "Build/TCTI/reports/tcti-fixture/report.json", proofTier: "kernel")]), "current_runtime_product_failure", true, false, false, true),
        ("environment-only-failure", policyFixtureStatus(id: "environment", state: "fail", reason: "CoreSimulator bootstatus failed"), "environment_only_failure", false, false, false, true),
        ("xctest-runner-connection-failure", policyFixtureStatus(id: "xctest-runner", kind: "kernel", proofTier: "kernel", acceptanceWeight: "blocker", realStackRequired: true, state: "fail", reason: "The test runner hung before establishing connection", reports: [policyFixtureReport(path: "Build/TCTI/reports/tcti-fixture/report.json", proofTier: "kernel")]), "environment_only_failure", false, false, false, true),
        ("remote-process-connection-failure", policyFixtureStatus(id: "xctest-remote-process", kind: "kernel", proofTier: "kernel", acceptanceWeight: "blocker", realStackRequired: true, state: "fail", reason: "Connection to remote process was not established", reports: [policyFixtureReport(path: "Build/TCTI/reports/tcti-fixture/report.json", proofTier: "kernel")]), "environment_only_failure", false, false, false, true),
        ("simulator-install-timeout", policyFixtureStatus(id: "simulator-tcti-runtime-stability", kind: "simulator-runtime", proofTier: "simulator", acceptanceWeight: "readiness", realStackRequired: true, state: "fail", reason: "current simulator report failed before launch", reports: [policyFixtureReport(path: "Build/Reports/runtime/tcti-simulator-stability-current.json", proofTier: "simulator", acceptanceWeight: "readiness", realStackRequired: true, canClaimRuntimeReadiness: false, failureStage: "simulator-install", failureKind: "timeout", failureExitStatus: "124", failureTimeoutSeconds: "120", productLaunchAttempted: false)]), "environment_only_failure", false, false, false, true),
        ("simulator-install-product-rejection", policyFixtureStatus(id: "simulator-tcti-runtime-stability", kind: "simulator-runtime", proofTier: "simulator", acceptanceWeight: "readiness", realStackRequired: true, state: "fail", reason: "simulator rejected the built app", reports: [policyFixtureReport(path: "Build/Reports/runtime/tcti-simulator-stability-current.json", proofTier: "simulator", acceptanceWeight: "readiness", realStackRequired: true, canClaimRuntimeReadiness: false, failureStage: "simulator-install", failureKind: "command-failure", failureExitStatus: "1", productLaunchAttempted: false)]), "current_runtime_product_failure", true, false, false, true),
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
    let installTimeoutPolicy = classifyGateResult(fixtures.first { $0.0 == "simulator-install-timeout" }!.1)
    guard installTimeoutPolicy.owningLayer == "environment" else {
        throw HarnessError.invalid("simulator install timeout must remain owned by the environment")
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
