@testable import OrlixTestRunner

enum OrlixUpstreamXCTest {
    static func run(_ spec: OrlixUpstreamTestRunSpec) throws {
        _ = try OrlixUpstreamTestSessionRunner(spec: spec).run()
    }
}
