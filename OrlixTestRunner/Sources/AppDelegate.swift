import UIKit

@main
final class AppDelegate: UIResponder, UIApplicationDelegate {
    override init() {
        OrlixAppLaunchRuntimeRunner.runIfRequested()
        OrlixAppLaunchUpstreamRunner.runIfRequested()
        super.init()
    }

    func application(
        _: UIApplication,
        didFinishLaunchingWithOptions _: [UIApplication.LaunchOptionsKey: Any]? = nil
    ) -> Bool {
        return true
    }
}

private enum OrlixAppLaunchUpstreamRunner {
    private static let environmentKey = "ORLIX_UPSTREAM_TEST_SPEC"

    static func runIfRequested() {
        guard let specName = requestedSpecName() else {
            return
        }

        do {
            let spec = try self.spec(named: specName)
            let output = try OrlixUpstreamTestSessionRunner(spec: spec).run()
            try writeOutputArtifact(output)
            write(output, to: .standardOutput)
            exit(EXIT_SUCCESS)
        } catch {
            let message = "ORLIX-APP-RUNNER-ERROR \(error)\n"
            try? writeOutputArtifact(message)
            NSLog("%@", message)
            write(message, to: .standardError)
            exit(EXIT_FAILURE)
        }
    }

    private static func requestedSpecName() -> String? {
        if let specName = ProcessInfo.processInfo.environment[environmentKey] {
            NSLog("ORLIX-APP-RUNNER requested environment spec %@", specName)
            return specName
        }

        let arguments = ProcessInfo.processInfo.arguments
        if arguments.contains("--orlix-upstream-test-spec") {
            NSLog("ORLIX-APP-RUNNER arguments %@", arguments.joined(separator: " "))
        }
        guard let optionIndex = arguments.firstIndex(of: "--orlix-upstream-test-spec") else {
            return nil
        }
        let valueIndex = arguments.index(after: optionIndex)
        guard valueIndex < arguments.endIndex else {
            return nil
        }
        return arguments[valueIndex]
    }

    private static func spec(named name: String) throws -> OrlixUpstreamTestRunSpec {
        switch name {
        case "kernelVirtioFSMount":
            return .kernelVirtioFSMount
        case "kernelRandomDevice":
            return .kernelRandomDevice
        default:
            throw OrlixAppLaunchUpstreamRunnerError.unknownSpec(name)
        }
    }

    private static func write(_ string: String, to handle: FileHandle) {
        guard let data = string.data(using: .utf8) else {
            return
        }
        handle.write(data)
    }

    private static func writeOutputArtifact(_ output: String) throws {
        let outputURL = URL(fileURLWithPath: NSTemporaryDirectory())
            .appendingPathComponent("orlix-upstream-test-output.txt")
        try output.write(to: outputURL, atomically: true, encoding: .utf8)
        NSLog("ORLIX-APP-RUNNER output %@", outputURL.path)
    }
}

private enum OrlixAppLaunchUpstreamRunnerError: Error, CustomStringConvertible {
    case unknownSpec(String)

    var description: String {
        switch self {
        case let .unknownSpec(name):
            return "unknown upstream test spec '\(name)'"
        }
    }
}
