#!/usr/bin/env swift
import Foundation

let fileManager = FileManager.default
let repoRoot = URL(fileURLWithPath: fileManager.currentDirectoryPath)

struct MCPTool {
    let name: String
    let description: String
    let properties: [String: Any]
    let required: [String]
}

let tools: [MCPTool] = [
    MCPTool(name: "tcti_status", description: "Read concise Orlix TCTI report and git status. Runs no generic command channel.", properties: [:], required: []),
    MCPTool(name: "tcti_next", description: "Summarize the next safe TCTI step from PLAN.md, IMPLEMENT.md, and reports.", properties: [:], required: []),
    MCPTool(name: "tcti_report_read", description: "Read Build/TCTI/reports/<target>/report.json for an allowlisted target.", properties: ["target": ["type": "string"]], required: ["target"]),
    MCPTool(name: "tcti_reproducer_read", description: "Read a reducer JSON under Build/TCTI/reproducers.", properties: ["path": ["type": "string"]], required: ["path"]),
    MCPTool(name: "tcti_golden_list", description: "List checked-in TCTI golden ELF cases.", properties: [:], required: []),
    MCPTool(name: "tcti_golden_validate", description: "Run make tcti-golden-elf for one checked-in case.", properties: ["case_id": ["type": "string"]], required: ["case_id"]),
    MCPTool(name: "tcti_safety_audit", description: "Run make tcti-appstore-safety-audit.", properties: [:], required: []),
    MCPTool(name: "tcti_plan_consistency", description: "Run make tcti-plan-consistency.", properties: [:], required: []),
]

let reportTargets: Set<String> = [
    "tcti-plan-consistency",
    "tcti-report-schema-check",
    "tcti-toolchain-check",
    "tcti-contract",
    "tcti-golden-elf",
    "tcti-diff-switch",
    "tcti-memory-fuzz",
    "tcti-direct-chain-fuzz",
    "tcti-appstore-safety-audit",
]

func jsonData(_ object: Any) -> Data {
    (try? JSONSerialization.data(withJSONObject: object, options: [.sortedKeys])) ?? Data("{}".utf8)
}

func send(_ object: [String: Any]) {
    let data = jsonData(object)
    FileHandle.standardOutput.write(Data("Content-Length: \(data.count)\r\n\r\n".utf8))
    FileHandle.standardOutput.write(data)
}

func readMessage() -> [String: Any]? {
    var header = Data()
    let input = FileHandle.standardInput
    while true {
        let byte = input.readData(ofLength: 1)
        if byte.isEmpty { return nil }
        header.append(byte)
        if header.suffix(4) == Data("\r\n\r\n".utf8) { break }
    }
    guard let headerText = String(data: header, encoding: .utf8) else { return nil }
    let lengthLine = headerText.split(separator: "\r\n").first { $0.lowercased().hasPrefix("content-length:") }
    guard let line = lengthLine,
          let length = Int(line.split(separator: ":", maxSplits: 1).last?.trimmingCharacters(in: .whitespaces) ?? "") else {
        return nil
    }
    let body = input.readData(ofLength: length)
    return (try? JSONSerialization.jsonObject(with: body)) as? [String: Any]
}

func response(id: Any?, result: Any) -> [String: Any] {
    ["jsonrpc": "2.0", "id": id ?? NSNull(), "result": result]
}

func errorResponse(id: Any?, code: Int = -32000, message: String) -> [String: Any] {
    ["jsonrpc": "2.0", "id": id ?? NSNull(), "error": ["code": code, "message": message]]
}

func toolListResult() -> [String: Any] {
    [
        "tools": tools.map { tool in
            [
                "name": tool.name,
                "description": tool.description,
                "inputSchema": [
                    "type": "object",
                    "properties": tool.properties,
                    "required": tool.required,
                    "additionalProperties": false,
                ],
            ]
        },
    ]
}

func path(_ components: String...) -> URL {
    components.reduce(repoRoot) { $0.appendingPathComponent($1) }
}

func readText(_ url: URL) throws -> String {
    try String(contentsOf: url, encoding: .utf8)
}

func safeReproducerURL(_ value: String) throws -> URL {
    let candidate = URL(fileURLWithPath: value, relativeTo: repoRoot).standardizedFileURL
    let root = path("Build", "TCTI", "reproducers").standardizedFileURL.path
    guard candidate.path.hasPrefix(root + "/") else {
        throw NSError(domain: "OrlixTCTIMCP", code: 1, userInfo: [NSLocalizedDescriptionKey: "reproducer path must be under Build/TCTI/reproducers"])
    }
    return candidate
}

func runMake(_ args: [String]) throws -> String {
    let allowed: Set<[String]> = [
        ["tcti-plan-consistency"],
        ["tcti-appstore-safety-audit"],
    ]
    if args.count == 1 {
        guard allowed.contains(args) else {
            throw NSError(domain: "OrlixTCTIMCP", code: 2, userInfo: [NSLocalizedDescriptionKey: "make target not allowlisted"])
        }
    } else if args.count == 2 && args[0] == "tcti-golden-elf" {
        guard goldenCases().contains(args[1].replacingOccurrences(of: "CASE=", with: "")) else {
            throw NSError(domain: "OrlixTCTIMCP", code: 3, userInfo: [NSLocalizedDescriptionKey: "unknown golden case"])
        }
    } else {
        throw NSError(domain: "OrlixTCTIMCP", code: 4, userInfo: [NSLocalizedDescriptionKey: "command shape not allowlisted"])
    }
    let process = Process()
    process.executableURL = URL(fileURLWithPath: "/usr/bin/make")
    process.arguments = args
    process.currentDirectoryURL = repoRoot
    let pipe = Pipe()
    process.standardOutput = pipe
    process.standardError = pipe
    try process.run()
    process.waitUntilExit()
    let data = pipe.fileHandleForReading.readDataToEndOfFile()
    let output = String(data: data, encoding: .utf8) ?? ""
    return "exit=\(process.terminationStatus)\n\(output)"
}

func goldenCases() -> [String] {
    let root = path("OrlixKernel", "Tests", "TCTI", "golden_elf")
    guard let entries = try? fileManager.contentsOfDirectory(at: root, includingPropertiesForKeys: nil) else {
        return []
    }
    return entries.filter { $0.hasDirectoryPath }.map(\.lastPathComponent).sorted()
}

func statusText() -> String {
    let reports = reportTargets.compactMap { target -> String? in
        let report = path("Build", "TCTI", "reports", target, "report.json")
        guard let text = try? readText(report) else { return nil }
        return "\(target): \(text.prefix(400))"
    }
    return reports.isEmpty ? "No TCTI reports found under Build/TCTI/reports." : reports.joined(separator: "\n\n")
}

func callTool(name: String, arguments: [String: Any]) throws -> String {
    switch name {
    case "tcti_status":
        return statusText()
    case "tcti_next":
        let plan = (try? readText(path("docs", "plans", "active", "orlix-tcti", "PLAN.md")).prefix(2000)) ?? ""
        let impl = (try? readText(path("docs", "plans", "active", "orlix-tcti", "IMPLEMENT.md")).suffix(2000)) ?? ""
        return "PLAN excerpt:\n\(plan)\n\nIMPLEMENT tail:\n\(impl)\n\nRun tcti-plan-consistency and codex-harness-check before choosing work."
    case "tcti_report_read":
        guard let target = arguments["target"] as? String, reportTargets.contains(target) else {
            throw NSError(domain: "OrlixTCTIMCP", code: 5, userInfo: [NSLocalizedDescriptionKey: "target is not allowlisted"])
        }
        return try readText(path("Build", "TCTI", "reports", target, "report.json"))
    case "tcti_reproducer_read":
        guard let value = arguments["path"] as? String else {
            throw NSError(domain: "OrlixTCTIMCP", code: 6, userInfo: [NSLocalizedDescriptionKey: "path is required"])
        }
        return try readText(safeReproducerURL(value))
    case "tcti_golden_list":
        return goldenCases().joined(separator: "\n")
    case "tcti_golden_validate":
        guard let caseID = arguments["case_id"] as? String else {
            throw NSError(domain: "OrlixTCTIMCP", code: 7, userInfo: [NSLocalizedDescriptionKey: "case_id is required"])
        }
        return try runMake(["tcti-golden-elf", "CASE=\(caseID)"])
    case "tcti_safety_audit":
        return try runMake(["tcti-appstore-safety-audit"])
    case "tcti_plan_consistency":
        return try runMake(["tcti-plan-consistency"])
    default:
        throw NSError(domain: "OrlixTCTIMCP", code: 8, userInfo: [NSLocalizedDescriptionKey: "unknown tool \(name)"])
    }
}

while let message = readMessage() {
    let id = message["id"]
    let method = message["method"] as? String ?? ""
    if method == "initialize" {
        send(response(id: id, result: [
            "protocolVersion": "2025-06-18",
            "capabilities": ["tools": ["listChanged": false]],
            "serverInfo": ["name": "orlix-tcti-mcp", "version": "0.1.0"],
        ]))
    } else if method == "notifications/initialized" {
        continue
    } else if method == "tools/list" {
        send(response(id: id, result: toolListResult()))
    } else if method == "tools/call" {
        let params = message["params"] as? [String: Any] ?? [:]
        let name = params["name"] as? String ?? ""
        let arguments = params["arguments"] as? [String: Any] ?? [:]
        do {
            let text = try callTool(name: name, arguments: arguments)
            send(response(id: id, result: ["content": [["type": "text", "text": text]], "isError": false]))
        } catch {
            send(response(id: id, result: ["content": [["type": "text", "text": "\(error.localizedDescription)"]], "isError": true]))
        }
    } else {
        send(errorResponse(id: id, message: "unsupported method \(method)"))
    }
}
