import UIKit

@main
final class OrlixOSTestApp: UIResponder, UIApplicationDelegate {
    override init() {
        OrlixOSTestAppLaunchRuntimeRunner.runIfRequested()
        OrlixOSTestAppLaunchUpstreamRunner.runIfRequested()
        super.init()
    }

    func application(
        _: UIApplication,
        didFinishLaunchingWithOptions _: [UIApplication.LaunchOptionsKey: Any]? = nil
    ) -> Bool {
        return true
    }
}

final class OrlixOSTestAppSceneDelegate: UIResponder, UIWindowSceneDelegate {}

private enum OrlixOSTestAppLaunchUpstreamRunner {
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
            let message = "ORLIX-OS-TEST-APP-ERROR \(error)\n"
            try? writeOutputArtifact(message)
            NSLog("%@", message)
            write(message, to: .standardError)
            exit(EXIT_FAILURE)
        }
    }

    private static func requestedSpecName() -> String? {
        if let specName = ProcessInfo.processInfo.environment[environmentKey] {
            NSLog("ORLIX-OS-TEST-APP requested environment spec %@", specName)
            return specName
        }

        let arguments = ProcessInfo.processInfo.arguments
        if arguments.contains("--orlix-upstream-test-spec") {
            NSLog("ORLIX-OS-TEST-APP arguments %@", arguments.joined(separator: " "))
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
            throw OrlixOSTestAppLaunchUpstreamRunnerError.unknownSpec(name)
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
        NSLog("ORLIX-OS-TEST-APP output %@", outputURL.path)
    }
}

private enum OrlixOSTestAppLaunchUpstreamRunnerError: Error, CustomStringConvertible {
    case unknownSpec(String)

    var description: String {
        switch self {
        case let .unknownSpec(name):
            return "unknown upstream test spec '\(name)'"
        }
    }
}
