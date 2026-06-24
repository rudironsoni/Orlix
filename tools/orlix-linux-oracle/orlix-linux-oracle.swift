#!/usr/bin/env swift

import Foundation

struct OracleCase: Decodable {
    struct Compare: Decodable {
        let stdout: Bool
        let stderr: Bool
        let exitStatus: Bool
        let signal: Bool
        let errnoEvents: Bool
        let statEntries: Bool
        let mutations: Bool
        let observations: Bool
    }

    let id: String
    let description: String
    let fixture: String?
    let command: [String]
    let environment: [String: String]
    let workingDirectory: String?
    let compare: Compare
}

struct OracleResult: Codable {
    struct ErrnoEvent: Codable, Equatable {
        let operation: String
        let path: String
        let errno: Int
        let name: String?
    }

    struct StatEntry: Codable, Equatable {
        let path: String
        let mode: String?
        let uid: Int?
        let gid: Int?
        let type: String?
        let size: Int?
    }

    struct Mutation: Codable, Equatable {
        let path: String
        let operation: String
        let result: String
    }

    let caseID: String
    let runner: String
    let stdout: String
    let stderr: String
    let exitStatus: Int?
    let signal: String?
    let errnoEvents: [ErrnoEvent]
    let statEntries: [StatEntry]
    let mutations: [Mutation]
    let observations: [String: String]
}

enum OracleError: Error, CustomStringConvertible {
    case usage(String)
    case invalidCase(String)
    case invalidLog(String)
    case unsupportedHost(String)
    case runnerFailed(String)
    case caseMismatch(expected: String, linux: String, orlix: String)
    case mismatches([String])

    var description: String {
        switch self {
        case let .usage(message):
            return message
        case let .invalidCase(message):
            return "invalid case: \(message)"
        case let .invalidLog(message):
            return "invalid log: \(message)"
        case let .unsupportedHost(message):
            return "unsupported host: \(message)"
        case let .runnerFailed(message):
            return "runner failed: \(message)"
        case let .caseMismatch(expected, linux, orlix):
            return "case mismatch: expected \(expected), linux \(linux), orlix \(orlix)"
        case let .mismatches(items):
            return (["oracle comparison failed:"] + items.map { "- \($0)" })
                .joined(separator: "\n")
        }
    }
}

func readJSON<T: Decodable>(_ type: T.Type, at path: String) throws -> T {
    let url = URL(fileURLWithPath: path)
    let data = try Data(contentsOf: url)
    return try JSONDecoder().decode(type, from: data)
}

func writeJSON<T: Encodable>(_ value: T, to path: String) throws {
    let encoder = JSONEncoder()
    encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
    let data = try encoder.encode(value)
    try data.write(to: URL(fileURLWithPath: path))
}

func validateCase(_ testCase: OracleCase) throws {
    guard !testCase.id.isEmpty else {
        throw OracleError.invalidCase("id is empty")
    }
    guard !testCase.description.isEmpty else {
        throw OracleError.invalidCase("description is empty")
    }
    guard let executable = testCase.command.first, executable.hasPrefix("/") else {
        throw OracleError.invalidCase("command must start with an absolute Linux path")
    }
    guard testCase.command.allSatisfy({ !$0.contains("\u{0}") }) else {
        throw OracleError.invalidCase("command contains NUL")
    }
    if let workingDirectory = testCase.workingDirectory,
       !workingDirectory.hasPrefix("/") || workingDirectory.contains("\u{0}") {
        throw OracleError.invalidCase("workingDirectory must be an absolute Linux path")
    }
    for (key, value) in testCase.environment {
        if key.isEmpty || key.contains("=") || key.contains("\u{0}") ||
            value.contains("\u{0}") {
            throw OracleError.invalidCase("invalid environment entry \(key)")
        }
    }
}

func compare(_ testCase: OracleCase, linux: OracleResult, orlix: OracleResult) throws {
    guard linux.caseID == testCase.id, orlix.caseID == testCase.id else {
        throw OracleError.caseMismatch(
            expected: testCase.id,
            linux: linux.caseID,
            orlix: orlix.caseID
        )
    }

    var mismatches: [String] = []
    let rules = testCase.compare

    if rules.stdout && linux.stdout != orlix.stdout {
        mismatches.append("stdout differs")
    }
    if rules.stderr && linux.stderr != orlix.stderr {
        mismatches.append("stderr differs")
    }
    if rules.exitStatus && linux.exitStatus != orlix.exitStatus {
        mismatches.append("exitStatus differs: linux=\(String(describing: linux.exitStatus)) orlix=\(String(describing: orlix.exitStatus))")
    }
    if rules.signal && linux.signal != orlix.signal {
        mismatches.append("signal differs: linux=\(String(describing: linux.signal)) orlix=\(String(describing: orlix.signal))")
    }
    if rules.errnoEvents && linux.errnoEvents != orlix.errnoEvents {
        mismatches.append("errnoEvents differ")
    }
    if rules.statEntries && linux.statEntries != orlix.statEntries {
        mismatches.append("statEntries differ")
    }
    if rules.mutations && linux.mutations != orlix.mutations {
        mismatches.append("mutations differ")
    }
    if rules.observations && linux.observations != orlix.observations {
        mismatches.append("observations differ")
    }

    guard mismatches.isEmpty else {
        throw OracleError.mismatches(mismatches)
    }
}

func value(after flag: String, in arguments: [String]) -> String? {
    guard let index = arguments.firstIndex(of: flag),
          arguments.indices.contains(arguments.index(after: index)) else {
        return nil
    }
    return arguments[arguments.index(after: index)]
}

func errnoName(_ value: Int) -> String? {
    switch value {
    case 2:
        return "ENOENT"
    case 20:
        return "ENOTDIR"
    case 40:
        return "ELOOP"
    default:
        return nil
    }
}

struct RawErrnoEvent: Decodable {
    let operation: String
    let path: String
    let errno: Int
    let expected: Int
}

struct RawObservation: Decodable {
    let observation: String
    let value: String
}

func pathErrnoEvents(from jsonLines: [String]) throws -> [OracleResult.ErrnoEvent] {
    let decoder = JSONDecoder()

    return try jsonLines.map { line -> OracleResult.ErrnoEvent in
        let raw = try decoder.decode(
            RawErrnoEvent.self,
            from: Data(line.utf8)
        )
        guard raw.errno == raw.expected else {
            throw OracleError.invalidLog(
                "event \(raw.operation) \(raw.path) errno \(raw.errno) does not match expected \(raw.expected)"
            )
        }
        return OracleResult.ErrnoEvent(
            operation: raw.operation,
            path: raw.path,
            errno: raw.errno,
            name: errnoName(raw.errno)
        )
    }
}

func observations(from jsonLines: [String]) throws -> [String: String] {
    let decoder = JSONDecoder()
    var result: [String: String] = [:]

    for line in jsonLines {
        let raw = try decoder.decode(
            RawObservation.self,
            from: Data(line.utf8)
        )
        guard result[raw.observation] == nil else {
            throw OracleError.invalidLog(
                "duplicate observation \(raw.observation)"
            )
        }
        result[raw.observation] = raw.value
    }

    return result
}

func pathErrnoResult(
    runner: String,
    stdout: String,
    stderr: String,
    exitStatus: Int?,
    signal: String?,
    jsonLines: [String]
) throws -> OracleResult {
    guard jsonLines.count == 4 else {
        throw OracleError.invalidLog("expected 4 path-errno JSON events, found \(jsonLines.count)")
    }

    let events = try pathErrnoEvents(from: jsonLines)
    return OracleResult(
        caseID: "path-errno",
        runner: runner,
        stdout: stdout,
        stderr: stderr,
        exitStatus: exitStatus,
        signal: signal,
        errnoEvents: events,
        statEntries: [],
        mutations: [
            OracleResult.Mutation(
                path: "regular",
                operation: "create-unlink",
                result: "ok"
            ),
            OracleResult.Mutation(
                path: "loop-a",
                operation: "symlink-unlink",
                result: "ok"
            )
        ],
        observations: [
            "filesystem": "tmpfs-or-scratch"
        ]
    )
}

let fdExecObservationKeys: Set<String> = [
    "pipe-created",
    "fd-without-cloexec",
    "fd-marked-cloexec",
    "child-started",
    "inherited-read",
    "cloexec-ebadf",
    "exec-child-exit"
]

let pipePollObservationKeys: Set<String> = [
    "pipe-created",
    "empty-read-eagain",
    "empty-poll-timeout",
    "write-end-writable",
    "read-end-readable",
    "read-payload",
    "read-end-hangup"
]

let pipeSelectObservationKeys: Set<String> = [
    "pipe-created",
    "empty-read-eagain",
    "empty-select-timeout",
    "write-end-writable",
    "read-end-readable",
    "read-payload",
    "readable-after-writer-close",
    "read-eof"
]

let pipeEpollObservationKeys: Set<String> = [
    "pipe-created",
    "epoll-created",
    "empty-read-eagain",
    "read-end-registered",
    "empty-epoll-timeout",
    "write-end-writable",
    "read-end-readable",
    "read-payload",
    "read-end-hangup"
]

let signalWaitObservationKeys: Set<String> = [
    "signal-handler-runs",
    "blocked-signal-pending",
    "unblocked-pending-handler",
    "waitpid-signal-termination"
]

let pseudoFSObservationKeys: Set<String> = [
    "procfs-mounted",
    "sysfs-mounted",
    "devtmpfs-mounted",
    "devpts-mounted",
    "tmpfs-mounted",
    "proc-self-readable",
    "dev-core-char",
    "devpts-directory",
    "ptmx-allocates",
    "sysfs-virtio-directory"
]
let mountNamespaceObservationKeys: Set<String> = [
    "mountpoint-exists",
    "child-started",
    "child-mountinfo-tmpfs",
    "parent-marker-hidden"
]
let cgroupV2ObservationKeys: Set<String> = [
    "proc-self-cgroup-v2-root",
    "mountinfo-cgroup2-root",
    "controllers-readable",
    "subtree-control-readable",
    "procs-readable",
    "root-procs-accepts-self",
    "child-cgroup-created",
    "child-procs-accepts-self",
    "child-cgroup-removed"
]
let networkNamespaceObservationKeys: Set<String> = [
    "proc-net-readable",
    "rtnetlink-opens",
    "rtnetlink-loopback-link",
    "loopback-udp-datagram",
    "child-newnet-inode-changed",
    "child-proc-net-readable",
    "child-rtnetlink-local",
    "child-route-error-linux-shaped"
]

func fdExecResult(
    runner: String,
    stdout: String,
    stderr: String,
    exitStatus: Int?,
    signal: String?,
    observations: [String: String]
) throws -> OracleResult {
    let missing = fdExecObservationKeys.subtracting(observations.keys)
    guard missing.isEmpty else {
        throw OracleError.invalidLog(
            "missing fd-exec observations: \(missing.sorted().joined(separator: ", "))"
        )
    }

    return OracleResult(
        caseID: "fd-exec",
        runner: runner,
        stdout: stdout,
        stderr: stderr,
        exitStatus: exitStatus,
        signal: signal,
        errnoEvents: [],
        statEntries: [],
        mutations: [],
        observations: observations
    )
}

func pipePollResult(
    runner: String,
    stdout: String,
    stderr: String,
    exitStatus: Int?,
    signal: String?,
    observations: [String: String]
) throws -> OracleResult {
    let missing = pipePollObservationKeys.subtracting(observations.keys)
    guard missing.isEmpty else {
        throw OracleError.invalidLog(
            "missing pipe-poll observations: \(missing.sorted().joined(separator: ", "))"
        )
    }

    return OracleResult(
        caseID: "pipe-poll",
        runner: runner,
        stdout: stdout,
        stderr: stderr,
        exitStatus: exitStatus,
        signal: signal,
        errnoEvents: [],
        statEntries: [],
        mutations: [],
        observations: observations
    )
}

func pipeSelectResult(
    runner: String,
    stdout: String,
    stderr: String,
    exitStatus: Int?,
    signal: String?,
    observations: [String: String]
) throws -> OracleResult {
    let missing = pipeSelectObservationKeys.subtracting(observations.keys)
    guard missing.isEmpty else {
        throw OracleError.invalidLog(
            "missing pipe-select observations: \(missing.sorted().joined(separator: ", "))"
        )
    }

    return OracleResult(
        caseID: "pipe-select",
        runner: runner,
        stdout: stdout,
        stderr: stderr,
        exitStatus: exitStatus,
        signal: signal,
        errnoEvents: [],
        statEntries: [],
        mutations: [],
        observations: observations
    )
}

func pipeEpollResult(
    runner: String,
    stdout: String,
    stderr: String,
    exitStatus: Int?,
    signal: String?,
    observations: [String: String]
) throws -> OracleResult {
    let missing = pipeEpollObservationKeys.subtracting(observations.keys)
    guard missing.isEmpty else {
        throw OracleError.invalidLog(
            "missing pipe-epoll observations: \(missing.sorted().joined(separator: ", "))"
        )
    }

    return OracleResult(
        caseID: "pipe-epoll",
        runner: runner,
        stdout: stdout,
        stderr: stderr,
        exitStatus: exitStatus,
        signal: signal,
        errnoEvents: [],
        statEntries: [],
        mutations: [],
        observations: observations
    )
}

func signalWaitResult(
    runner: String,
    stdout: String,
    stderr: String,
    exitStatus: Int?,
    signal: String?,
    observations: [String: String]
) throws -> OracleResult {
    let missing = signalWaitObservationKeys.subtracting(observations.keys)
    guard missing.isEmpty else {
        throw OracleError.invalidLog(
            "missing signal-wait observations: \(missing.sorted().joined(separator: ", "))"
        )
    }

    return OracleResult(
        caseID: "signal-wait",
        runner: runner,
        stdout: stdout,
        stderr: stderr,
        exitStatus: exitStatus,
        signal: signal,
        errnoEvents: [],
        statEntries: [],
        mutations: [],
        observations: observations
    )
}

func pseudoFSResult(
    runner: String,
    stdout: String,
    stderr: String,
    exitStatus: Int?,
    signal: String?,
    observations: [String: String]
) throws -> OracleResult {
    let missing = pseudoFSObservationKeys.subtracting(observations.keys)
    guard missing.isEmpty else {
        throw OracleError.invalidLog(
            "missing pseudo-fs observations: \(missing.sorted().joined(separator: ", "))"
        )
    }

    return OracleResult(
        caseID: "pseudo-fs",
        runner: runner,
        stdout: stdout,
        stderr: stderr,
        exitStatus: exitStatus,
        signal: signal,
        errnoEvents: [],
        statEntries: [],
        mutations: [],
        observations: observations
    )
}

func mountNamespaceResult(
    runner: String,
    stdout: String,
    stderr: String,
    exitStatus: Int?,
    signal: String?,
    observations: [String: String]
) throws -> OracleResult {
    let missing = mountNamespaceObservationKeys.subtracting(observations.keys)
    guard missing.isEmpty else {
        throw OracleError.invalidLog(
            "missing mount-namespace observations: \(missing.sorted().joined(separator: ", "))"
        )
    }
    return OracleResult(
        caseID: "mount-namespace",
        runner: runner,
        stdout: stdout,
        stderr: stderr,
        exitStatus: exitStatus,
        signal: signal,
        errnoEvents: [],
        statEntries: [],
        mutations: [],
        observations: observations
    )
}

func cgroupV2Result(
    runner: String,
    stdout: String,
    stderr: String,
    exitStatus: Int?,
    signal: String?,
    observations: [String: String]
) throws -> OracleResult {
    let missing = cgroupV2ObservationKeys.subtracting(observations.keys)
    guard missing.isEmpty else {
        throw OracleError.invalidLog(
            "missing cgroup-v2 observations: \(missing.sorted().joined(separator: ", "))"
        )
    }
    return OracleResult(
        caseID: "cgroup-v2",
        runner: runner,
        stdout: stdout,
        stderr: stderr,
        exitStatus: exitStatus,
        signal: signal,
        errnoEvents: [],
        statEntries: [],
        mutations: [],
        observations: observations
    )
}

func networkNamespaceResult(
    runner: String,
    stdout: String,
    stderr: String,
    exitStatus: Int?,
    signal: String?,
    observations: [String: String]
) throws -> OracleResult {
    let missing = networkNamespaceObservationKeys.subtracting(observations.keys)
    guard missing.isEmpty else {
        throw OracleError.invalidLog(
            "missing network-namespace observations: \(missing.sorted().joined(separator: ", "))"
        )
    }
    return OracleResult(
        caseID: "network-namespace",
        runner: runner,
        stdout: stdout,
        stderr: stderr,
        exitStatus: exitStatus,
        signal: signal,
        errnoEvents: [],
        statEntries: [],
        mutations: [],
        observations: observations
    )
}

func oracleBlock(caseID: String, in log: String) throws -> [String] {
    let begin = "ORLIX-ORACLE-BEGIN \(caseID)"
    let end = "ORLIX-ORACLE-END \(caseID)"
    guard let beginRange = log.range(of: begin) else {
        throw OracleError.invalidLog("missing \(begin)")
    }
    guard let endRange = log.range(of: end, range: beginRange.upperBound..<log.endIndex) else {
        throw OracleError.invalidLog("missing \(end)")
    }

    let block = String(log[beginRange.upperBound..<endRange.lowerBound])
    return block
        .split(separator: "\n", omittingEmptySubsequences: false)
        .compactMap { rawLine -> String? in
            guard let start = rawLine.firstIndex(of: "{"),
                  let end = rawLine.lastIndex(of: "}") else {
                return nil
            }
            return String(rawLine[start...end])
        }
}

func pathErrnoResultFromOrlixLog(_ log: String) throws -> OracleResult {
    let jsonLines = try oracleBlock(caseID: "path-errno", in: log)
    return try pathErrnoResult(
        runner: "orlix",
        stdout: jsonLines.joined(separator: "\n") + "\n",
        stderr: "",
        exitStatus: 0,
        signal: nil,
        jsonLines: jsonLines
    )
}

func resultFromLinuxFixture(
    testCase: OracleCase,
    stdout: String,
    stderr: String,
    exitStatus: Int?,
    signal: String?
) throws -> OracleResult {
    switch testCase.id {
    case "path-errno":
        return try pathErrnoResult(
            runner: "linux",
            stdout: stdout,
            stderr: stderr,
            exitStatus: exitStatus,
            signal: signal,
            jsonLines: jsonObjectLines(from: stdout)
        )
    case "fd-exec":
        return try fdExecResult(
            runner: "linux",
            stdout: stdout,
            stderr: stderr,
            exitStatus: exitStatus,
            signal: signal,
            observations: observations(from: jsonObjectLines(from: stdout))
        )
    case "pipe-poll":
        return try pipePollResult(
            runner: "linux",
            stdout: stdout,
            stderr: stderr,
            exitStatus: exitStatus,
            signal: signal,
            observations: observations(from: jsonObjectLines(from: stdout))
        )
    case "pipe-select":
        return try pipeSelectResult(
            runner: "linux",
            stdout: stdout,
            stderr: stderr,
            exitStatus: exitStatus,
            signal: signal,
            observations: observations(from: jsonObjectLines(from: stdout))
        )
    case "pipe-epoll":
        return try pipeEpollResult(
            runner: "linux",
            stdout: stdout,
            stderr: stderr,
            exitStatus: exitStatus,
            signal: signal,
            observations: observations(from: jsonObjectLines(from: stdout))
        )
    case "signal-wait":
        return try signalWaitResult(
            runner: "linux",
            stdout: stdout,
            stderr: stderr,
            exitStatus: exitStatus,
            signal: signal,
            observations: observations(from: jsonObjectLines(from: stdout))
        )
    case "pseudo-fs":
        return try pseudoFSResult(
            runner: "linux",
            stdout: stdout,
            stderr: stderr,
            exitStatus: exitStatus,
            signal: signal,
            observations: observations(from: jsonObjectLines(from: stdout))
        )
    case "mount-namespace":
        return try mountNamespaceResult(
            runner: "linux",
            stdout: stdout,
            stderr: stderr,
            exitStatus: exitStatus,
            signal: signal,
            observations: observations(from: jsonObjectLines(from: stdout))
        )
    case "cgroup-v2":
        return try cgroupV2Result(
            runner: "linux",
            stdout: stdout,
            stderr: stderr,
            exitStatus: exitStatus,
            signal: signal,
            observations: observations(from: jsonObjectLines(from: stdout))
        )
    case "network-namespace":
        return try networkNamespaceResult(
            runner: "linux",
            stdout: stdout,
            stderr: stderr,
            exitStatus: exitStatus,
            signal: signal,
            observations: observations(from: jsonObjectLines(from: stdout))
        )
    default:
        throw OracleError.invalidCase(
            "linux-result-from-fixture does not support \(testCase.id)"
        )
    }
}

func resultFromOrlixLog(testCase: OracleCase, log: String) throws -> OracleResult {
    switch testCase.id {
    case "path-errno":
        return try pathErrnoResultFromOrlixLog(log)
    case "fd-exec":
        return try fdExecResultFromOrlixLog(log)
    case "pipe-poll":
        return try pipePollResultFromOrlixLog(log)
    case "pipe-select":
        return try pipeSelectResultFromOrlixLog(log)
    case "pipe-epoll":
        return try pipeEpollResultFromOrlixLog(log)
    case "signal-wait":
        return try signalWaitResultFromOrlixLog(log)
    case "pseudo-fs":
        return try pseudoFSResultFromOrlixLog(log)
    case "mount-namespace":
        return try mountNamespaceResultFromOrlixLog(log)
    case "cgroup-v2":
        return try cgroupV2ResultFromOrlixLog(log)
    case "network-namespace":
        return try networkNamespaceResultFromOrlixLog(log)
    default:
        throw OracleError.invalidCase(
            "orlix-result-from-log does not support \(testCase.id)"
        )
    }
}

func fdExecObservation(
    _ log: String,
    _ marker: String,
    _ observation: String
) -> (String, String) {
    (observation, log.contains(marker) ? "ok" : "missing")
}

func fdExecResultFromOrlixLog(_ log: String) throws -> OracleResult {
    var observed = Dictionary(
        uniqueKeysWithValues: [
            fdExecObservation(
                log,
                "pipe creates descriptor pairs",
                "pipe-created"
            ),
            fdExecObservation(
                log,
                "fcntl reports descriptor without close-on-exec",
                "fd-without-cloexec"
            ),
            fdExecObservation(
                log,
                "fcntl marks selected descriptor close-on-exec",
                "fd-marked-cloexec"
            ),
            fdExecObservation(log, "ORLIX-FD-EXEC-CHILD", "child-started"),
            fdExecObservation(
                log,
                "ORLIX-FD-INHERITED-READ-OK",
                "inherited-read"
            ),
            fdExecObservation(
                log,
                "ORLIX-FD-CLOEXEC-EBADF-OK",
                "cloexec-ebadf"
            ),
            fdExecObservation(
                log,
                "exec closes close-on-exec descriptor",
                "exec-child-exit"
            )
        ]
    )

    if !log.contains("fd_exec_probe") || !log.contains("ORLIX-FD-EXEC-PROBE") {
        observed["fd-exec-log"] = "missing"
    }

    return try fdExecResult(
        runner: "orlix",
        stdout: "",
        stderr: "",
        exitStatus: 0,
        signal: nil,
        observations: observed
    )
}

func pipePollResultFromOrlixLog(_ log: String) throws -> OracleResult {
    var observed = Dictionary(
        uniqueKeysWithValues: [
            fdExecObservation(
                log,
                "pipe creates nonblocking read descriptor",
                "pipe-created"
            ),
            fdExecObservation(
                log,
                "empty nonblocking pipe read returns EAGAIN",
                "empty-read-eagain"
            ),
            fdExecObservation(
                log,
                "empty pipe read poll times out",
                "empty-poll-timeout"
            ),
            fdExecObservation(
                log,
                "pipe write end polls writable",
                "write-end-writable"
            ),
            fdExecObservation(
                log,
                "pipe read end polls readable after write",
                "read-end-readable"
            ),
            fdExecObservation(
                log,
                "pipe read returns written payload",
                "read-payload"
            ),
            fdExecObservation(
                log,
                "pipe read end polls hangup after writer closes",
                "read-end-hangup"
            )
        ]
    )

    if !log.contains("pipe_poll_probe") || !log.contains("ORLIX-PIPE-POLL-PROBE") {
        observed["pipe-poll-log"] = "missing"
    }

    return try pipePollResult(
        runner: "orlix",
        stdout: "",
        stderr: "",
        exitStatus: 0,
        signal: nil,
        observations: observed
    )
}

func pipeSelectResultFromOrlixLog(_ log: String) throws -> OracleResult {
    var observed = Dictionary(
        uniqueKeysWithValues: [
            fdExecObservation(
                log,
                "pipe creates nonblocking read descriptor select",
                "pipe-created"
            ),
            fdExecObservation(
                log,
                "empty nonblocking pipe read returns EAGAIN before select",
                "empty-read-eagain"
            ),
            fdExecObservation(
                log,
                "empty pipe read select times out",
                "empty-select-timeout"
            ),
            fdExecObservation(
                log,
                "pipe write end selects writable",
                "write-end-writable"
            ),
            fdExecObservation(
                log,
                "pipe read end selects readable after write",
                "read-end-readable"
            ),
            fdExecObservation(
                log,
                "pipe read returns selected payload",
                "read-payload"
            ),
            fdExecObservation(
                log,
                "pipe read end selects readable after writer closes",
                "readable-after-writer-close"
            ),
            fdExecObservation(
                log,
                "pipe read returns EOF after selected writer close",
                "read-eof"
            )
        ]
    )

    if !log.contains("pipe_select_probe") || !log.contains("ORLIX-PIPE-SELECT-PROBE") {
        observed["pipe-select-log"] = "missing"
    }

    return try pipeSelectResult(
        runner: "orlix",
        stdout: "",
        stderr: "",
        exitStatus: 0,
        signal: nil,
        observations: observed
    )
}

func pipeEpollResultFromOrlixLog(_ log: String) throws -> OracleResult {
    var observed = Dictionary(
        uniqueKeysWithValues: [
            fdExecObservation(
                log,
                "pipe creates nonblocking read descriptor epoll",
                "pipe-created"
            ),
            fdExecObservation(
                log,
                "epoll_create1 returns epoll descriptor",
                "epoll-created"
            ),
            fdExecObservation(
                log,
                "empty nonblocking pipe read returns EAGAIN before epoll",
                "empty-read-eagain"
            ),
            fdExecObservation(
                log,
                "epoll_ctl adds pipe read end",
                "read-end-registered"
            ),
            fdExecObservation(
                log,
                "empty pipe read epoll times out",
                "empty-epoll-timeout"
            ),
            fdExecObservation(
                log,
                "pipe write end epolls writable",
                "write-end-writable"
            ),
            fdExecObservation(
                log,
                "pipe read end epolls readable after write",
                "read-end-readable"
            ),
            fdExecObservation(
                log,
                "pipe read returns epoll payload",
                "read-payload"
            ),
            fdExecObservation(
                log,
                "pipe read end epolls hangup after writer closes",
                "read-end-hangup"
            )
        ]
    )

    if !log.contains("pipe_epoll_probe") || !log.contains("ORLIX-PIPE-EPOLL-PROBE") {
        observed["pipe-epoll-log"] = "missing"
    }

    return try pipeEpollResult(
        runner: "orlix",
        stdout: "",
        stderr: "",
        exitStatus: 0,
        signal: nil,
        observations: observed
    )
}

func signalWaitResultFromOrlixLog(_ log: String) throws -> OracleResult {
    var observed = Dictionary(
        uniqueKeysWithValues: [
            fdExecObservation(
                log,
                "signal handler runs delivered signal",
                "signal-handler-runs"
            ),
            fdExecObservation(
                log,
                "blocked signal remains pending",
                "blocked-signal-pending"
            ),
            fdExecObservation(
                log,
                "unblocked pending signal runs handler",
                "unblocked-pending-handler"
            ),
            fdExecObservation(
                log,
                "waitpid observes signal termination status",
                "waitpid-signal-termination"
            )
        ]
    )

    if !log.contains("signal_wait_probe") || !log.contains("ORLIX-SIGNAL-WAIT-PROBE") {
        observed["signal-wait-log"] = "missing"
    }

    return try signalWaitResult(
        runner: "orlix",
        stdout: "",
        stderr: "",
        exitStatus: 0,
        signal: nil,
        observations: observed
    )
}

func pseudoFSResultFromOrlixLog(_ log: String) throws -> OracleResult {
    var observed = Dictionary(
        uniqueKeysWithValues: [
            fdExecObservation(
                log,
                "mountinfo exposes procfs at /proc",
                "procfs-mounted"
            ),
            fdExecObservation(
                log,
                "mountinfo exposes sysfs at /sys",
                "sysfs-mounted"
            ),
            fdExecObservation(
                log,
                "mountinfo exposes devtmpfs at /dev",
                "devtmpfs-mounted"
            ),
            fdExecObservation(
                log,
                "mountinfo exposes devpts at /dev/pts",
                "devpts-mounted"
            ),
            fdExecObservation(
                log,
                "mountinfo exposes tmpfs at /tmp",
                "tmpfs-mounted"
            ),
            fdExecObservation(
                log,
                "proc self status fd mounts readable",
                "proc-self-readable"
            ),
            fdExecObservation(
                log,
                "core dev nodes are Linux character devices",
                "dev-core-char"
            ),
            fdExecObservation(
                log,
                "devpts mountpoint is directory",
                "devpts-directory"
            ),
            fdExecObservation(
                log,
                "devpts allocates a PTY master through ptmx",
                "ptmx-allocates"
            ),
            fdExecObservation(
                log,
                "sysfs exposes virtio device directory",
                "sysfs-virtio-directory"
            )
        ]
    )

    if !log.contains("pseudo_fs_probe") || !log.contains("ORLIX-PSEUDO-FS-PROBE") {
        observed["pseudo-fs-log"] = "missing"
    }

    return try pseudoFSResult(
        runner: "orlix",
        stdout: "",
        stderr: "",
        exitStatus: 0,
        signal: nil,
        observations: observed
    )
}

func mountNamespaceResultFromOrlixLog(_ log: String) throws -> OracleResult {
    var observed = Dictionary(
        uniqueKeysWithValues: [
            fdExecObservation(
                log,
                "mount namespace probe mountpoint exists",
                "mountpoint-exists"
            ),
            fdExecObservation(
                log,
                "mount namespace child started",
                "child-started"
            ),
            fdExecObservation(
                log,
                "mount namespace child verified mountinfo",
                "child-mountinfo-tmpfs"
            ),
            fdExecObservation(
                log,
                "child tmpfs mount hidden parent",
                "parent-marker-hidden"
            )
        ]
    )
    if !log.contains("mount_namespace_probe") {
        observed["mount-namespace-log"] = "missing"
    }
    return try mountNamespaceResult(
        runner: "orlix",
        stdout: "",
        stderr: "",
        exitStatus: 0,
        signal: nil,
        observations: observed
    )
}

func cgroupV2ResultFromOrlixLog(_ log: String) throws -> OracleResult {
    var observed = Dictionary(
        uniqueKeysWithValues: [
            fdExecObservation(
                log,
                "proc self cgroup reports v2 root",
                "proc-self-cgroup-v2-root"
            ),
            fdExecObservation(
                log,
                "mountinfo reports cgroup2 at /sys/fs/cgroup",
                "mountinfo-cgroup2-root"
            ),
            fdExecObservation(
                log,
                "cgroup v2 controllers file is readable",
                "controllers-readable"
            ),
            fdExecObservation(
                log,
                "cgroup v2 subtree control file is readable",
                "subtree-control-readable"
            ),
            fdExecObservation(
                log,
                "cgroup v2 procs file is readable",
                "procs-readable"
            ),
            fdExecObservation(
                log,
                "cgroup v2 procs accepts current task at root",
                "root-procs-accepts-self"
            ),
            fdExecObservation(
                log,
                "cgroup v2 child cgroup directory can be created",
                "child-cgroup-created"
            ),
            fdExecObservation(
                log,
                "cgroup v2 child cgroup accepts current task",
                "child-procs-accepts-self"
            ),
            fdExecObservation(
                log,
                "cgroup v2 empty child cgroup can be removed",
                "child-cgroup-removed"
            )
        ]
    )
    if !log.contains("cgroup_v2_probe") || !log.contains("ORLIX-CGROUP-V2-PROBE") {
        observed["cgroup-v2-log"] = "missing"
    }
    return try cgroupV2Result(
        runner: "orlix",
        stdout: "",
        stderr: "",
        exitStatus: 0,
        signal: nil,
        observations: observed
    )
}

func networkNamespaceResultFromOrlixLog(_ log: String) throws -> OracleResult {
    var observed = Dictionary(
        uniqueKeysWithValues: [
            fdExecObservation(
                log,
                "procfs exposes network state",
                "proc-net-readable"
            ),
            fdExecObservation(
                log,
                "rtnetlink sockets open in current network namespace",
                "rtnetlink-opens"
            ),
            fdExecObservation(
                log,
                "RTM_GETLINK reports loopback interface",
                "rtnetlink-loopback-link"
            ),
            fdExecObservation(
                log,
                "loopback UDP exchanges local datagrams",
                "loopback-udp-datagram"
            ),
            fdExecObservation(
                log,
                "network namespace child enters isolated net namespace",
                "child-newnet-inode-changed"
            ),
            fdExecObservation(
                log,
                "new network namespace keeps procfs network state readable",
                "child-proc-net-readable"
            ),
            fdExecObservation(
                log,
                "new network namespace keeps rtnetlink socket local",
                "child-rtnetlink-local"
            ),
            fdExecObservation(
                log,
                "new network namespace rejects incomplete route with Linux error",
                "child-route-error-linux-shaped"
            )
        ]
    )
    if !log.contains("network_namespace_probe") {
        observed["network-namespace-log"] = "missing"
    }
    return try networkNamespaceResult(
        runner: "orlix",
        stdout: "",
        stderr: "",
        exitStatus: 0,
        signal: nil,
        observations: observed
    )
}

func jsonObjectLines(from output: String) -> [String] {
    output
        .split(separator: "\n", omittingEmptySubsequences: false)
        .compactMap { rawLine -> String? in
            guard let start = rawLine.firstIndex(of: "{"),
                  let end = rawLine.lastIndex(of: "}") else {
                return nil
            }
            return String(rawLine[start...end])
        }
}

func requireLinuxHost() throws {
    #if os(Linux)
    return
    #else
    throw OracleError.unsupportedHost(
        "linux-result-from-fixture must run inside a real Linux environment"
    )
    #endif
}

func processOutput(_ pipe: Pipe) -> String {
    let data = pipe.fileHandleForReading.readDataToEndOfFile()
    return String(data: data, encoding: .utf8) ?? ""
}

func runLinuxFixture(
    testCase: OracleCase,
    fixturePath: String,
    workdirPath: String
) throws -> OracleResult {
    try requireLinuxHost()

    try FileManager.default.createDirectory(
        atPath: workdirPath,
        withIntermediateDirectories: true
    )

    let process = Process()
    let stdoutPipe = Pipe()
    let stderrPipe = Pipe()
    var environment = ProcessInfo.processInfo.environment

    for (key, value) in testCase.environment {
        environment[key] = value
    }
    process.executableURL = URL(fileURLWithPath: fixturePath)
    process.arguments = Array(testCase.command.dropFirst())
    process.currentDirectoryURL = URL(fileURLWithPath: workdirPath)
    process.environment = environment
    process.standardOutput = stdoutPipe
    process.standardError = stderrPipe

    try process.run()
    process.waitUntilExit()

    let stdout = processOutput(stdoutPipe)
    let stderr = processOutput(stderrPipe)
    let status = Int(process.terminationStatus)
    let signal = process.terminationReason == .uncaughtSignal ?
        "SIG\(status)" : nil
    let exitStatus = process.terminationReason == .exit ? status : nil

    if process.terminationReason == .exit && status != 0 {
        throw OracleError.runnerFailed(
            "fixture exited with status \(status); stderr: \(stderr)"
        )
    }
    if process.terminationReason == .uncaughtSignal {
        throw OracleError.runnerFailed(
            "fixture terminated by signal \(status); stderr: \(stderr)"
        )
    }

    return try resultFromLinuxFixture(
        testCase: testCase,
        stdout: stdout,
        stderr: stderr,
        exitStatus: exitStatus,
        signal: signal
    )
}

func repositoryPath(_ relativePath: String) -> String {
    URL(fileURLWithPath: FileManager.default.currentDirectoryPath)
        .appendingPathComponent(relativePath)
        .path
}

func expectComparisonFailure(
    _ testCase: OracleCase,
    linux: OracleResult,
    orlix: OracleResult
) throws {
    do {
        try compare(testCase, linux: linux, orlix: orlix)
    } catch OracleError.mismatches {
        return
    }

    throw OracleError.runnerFailed(
        "expected oracle drift for case \(testCase.id)"
    )
}

func selfTestCase(
    casePath: String,
    linuxResultPath: String,
    orlixResultPath: String,
    driftResultPath: String,
    orlixLogPath: String
) throws {
    let testCase = try readJSON(OracleCase.self, at: casePath)
    try validateCase(testCase)

    let linux = try readJSON(OracleResult.self, at: linuxResultPath)
    let orlix = try readJSON(OracleResult.self, at: orlixResultPath)
    try compare(testCase, linux: linux, orlix: orlix)

    let drift = try readJSON(OracleResult.self, at: driftResultPath)
    try expectComparisonFailure(testCase, linux: linux, orlix: drift)

    let log = try String(contentsOfFile: orlixLogPath, encoding: .utf8)
    let converted = try resultFromOrlixLog(testCase: testCase, log: log)
    try compare(testCase, linux: linux, orlix: converted)
}

func runSelfTest() throws {
    try selfTestCase(
        casePath: repositoryPath("tools/orlix-linux-oracle/cases/path-errno.json"),
        linuxResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/path-errno.linux.json"
        ),
        orlixResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/path-errno.orlix.json"
        ),
        driftResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/path-errno.orlix-drift.json"
        ),
        orlixLogPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/path-errno.orlix-kselftest.log"
        )
    )
    try selfTestCase(
        casePath: repositoryPath("tools/orlix-linux-oracle/cases/fd-exec.json"),
        linuxResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/fd-exec.linux.json"
        ),
        orlixResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/fd-exec.orlix.json"
        ),
        driftResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/fd-exec.orlix-drift.json"
        ),
        orlixLogPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/fd-exec.orlix-kselftest.log"
        )
    )

    try selfTestCase(
        casePath: repositoryPath("tools/orlix-linux-oracle/cases/pipe-poll.json"),
        linuxResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/pipe-poll.linux.json"
        ),
        orlixResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/pipe-poll.orlix.json"
        ),
        driftResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/pipe-poll.orlix-drift.json"
        ),
        orlixLogPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/pipe-poll.orlix-kselftest.log"
        )
    )
    try selfTestCase(
        casePath: repositoryPath("tools/orlix-linux-oracle/cases/pipe-select.json"),
        linuxResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/pipe-select.linux.json"
        ),
        orlixResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/pipe-select.orlix.json"
        ),
        driftResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/pipe-select.orlix-drift.json"
        ),
        orlixLogPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/pipe-select.orlix-kselftest.log"
        )
    )
    try selfTestCase(
        casePath: repositoryPath("tools/orlix-linux-oracle/cases/pipe-epoll.json"),
        linuxResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/pipe-epoll.linux.json"
        ),
        orlixResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/pipe-epoll.orlix.json"
        ),
        driftResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/pipe-epoll.orlix-drift.json"
        ),
        orlixLogPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/pipe-epoll.orlix-kselftest.log"
        )
    )
    try selfTestCase(
        casePath: repositoryPath("tools/orlix-linux-oracle/cases/signal-wait.json"),
        linuxResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/signal-wait.linux.json"
        ),
        orlixResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/signal-wait.orlix.json"
        ),
        driftResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/signal-wait.orlix-drift.json"
        ),
        orlixLogPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/signal-wait.orlix-kselftest.log"
        )
    )
    try selfTestCase(
        casePath: repositoryPath("tools/orlix-linux-oracle/cases/pseudo-fs.json"),
        linuxResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/pseudo-fs.linux.json"
        ),
        orlixResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/pseudo-fs.orlix.json"
        ),
        driftResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/pseudo-fs.orlix-drift.json"
        ),
        orlixLogPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/pseudo-fs.orlix-kselftest.log"
        )
    )

    try selfTestCase(
        casePath: repositoryPath("tools/orlix-linux-oracle/cases/mount-namespace.json"),
        linuxResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/mount-namespace.linux.json"
        ),
        orlixResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/mount-namespace.orlix.json"
        ),
        driftResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/mount-namespace.orlix-drift.json"
        ),
        orlixLogPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/mount-namespace.orlix-kselftest.log"
        )
    )
    try selfTestCase(
        casePath: repositoryPath("tools/orlix-linux-oracle/cases/cgroup-v2.json"),
        linuxResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/cgroup-v2.linux.json"
        ),
        orlixResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/cgroup-v2.orlix.json"
        ),
        driftResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/cgroup-v2.orlix-drift.json"
        ),
        orlixLogPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/cgroup-v2.orlix-kselftest.log"
        )
    )
    try selfTestCase(
        casePath: repositoryPath("tools/orlix-linux-oracle/cases/network-namespace.json"),
        linuxResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/network-namespace.linux.json"
        ),
        orlixResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/network-namespace.orlix.json"
        ),
        driftResultPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/network-namespace.orlix-drift.json"
        ),
        orlixLogPath: repositoryPath(
            "tools/orlix-linux-oracle/samples/network-namespace.orlix-kselftest.log"
        )
    )

    print("oracle self-test passed")
}

func run(arguments: [String]) throws {
    guard let command = arguments.first else {
        throw OracleError.usage("usage: self-test | validate-case <case.json> | linux-result-from-fixture --case <case.json> --fixture <binary> --workdir <dir> --output <result.json> | orlix-result-from-log --case <case.json> --log <log.txt> --output <result.json> | compare --case <case.json> --linux-result <result.json> --orlix-result <result.json>")
    }

    switch command {
    case "self-test":
        guard arguments.count == 1 else {
            throw OracleError.usage("usage: self-test")
        }
        try runSelfTest()
    case "validate-case":
        guard arguments.count == 2 else {
            throw OracleError.usage("usage: validate-case <case.json>")
        }
        let testCase = try readJSON(OracleCase.self, at: arguments[1])
        try validateCase(testCase)
        print("case \(testCase.id) is valid")
    case "linux-result-from-fixture":
        guard let casePath = value(after: "--case", in: arguments),
              let fixturePath = value(after: "--fixture", in: arguments),
              let workdirPath = value(after: "--workdir", in: arguments),
              let outputPath = value(after: "--output", in: arguments)
        else {
            throw OracleError.usage("usage: linux-result-from-fixture --case <case.json> --fixture <binary> --workdir <dir> --output <result.json>")
        }
        let testCase = try readJSON(OracleCase.self, at: casePath)
        try validateCase(testCase)
        let result = try runLinuxFixture(
            testCase: testCase,
            fixturePath: fixturePath,
            workdirPath: workdirPath
        )
        try writeJSON(result, to: outputPath)
        print("wrote Linux result for case \(testCase.id): \(outputPath)")
    case "orlix-result-from-log":
        guard let casePath = value(after: "--case", in: arguments),
              let logPath = value(after: "--log", in: arguments),
              let outputPath = value(after: "--output", in: arguments)
        else {
            throw OracleError.usage("usage: orlix-result-from-log --case <case.json> --log <log.txt> --output <result.json>")
        }
        let testCase = try readJSON(OracleCase.self, at: casePath)
        try validateCase(testCase)
        let log = try String(contentsOfFile: logPath, encoding: .utf8)
        let result = try resultFromOrlixLog(testCase: testCase, log: log)
        try writeJSON(result, to: outputPath)
        print("wrote Orlix result for case \(testCase.id): \(outputPath)")
    case "compare":
        guard let casePath = value(after: "--case", in: arguments),
              let linuxPath = value(after: "--linux-result", in: arguments),
              let orlixPath = value(after: "--orlix-result", in: arguments)
        else {
            throw OracleError.usage("usage: compare --case <case.json> --linux-result <result.json> --orlix-result <result.json>")
        }
        let testCase = try readJSON(OracleCase.self, at: casePath)
        try validateCase(testCase)
        let linux = try readJSON(OracleResult.self, at: linuxPath)
        let orlix = try readJSON(OracleResult.self, at: orlixPath)
        try compare(testCase, linux: linux, orlix: orlix)
        print("case \(testCase.id) matches")
    default:
        throw OracleError.usage("unknown command: \(command)")
    }
}

do {
    try run(arguments: Array(CommandLine.arguments.dropFirst()))
} catch let error as OracleError {
    FileHandle.standardError.write(Data((error.description + "\n").utf8))
    exit(2)
} catch {
    FileHandle.standardError.write(Data(("error: \(error)\n").utf8))
    exit(2)
}
