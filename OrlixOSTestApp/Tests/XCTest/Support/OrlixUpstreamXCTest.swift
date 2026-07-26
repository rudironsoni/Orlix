@testable import OrlixOSTestApp
import XCTest

enum OrlixUpstreamXCTest {
    static func run(_ spec: OrlixUpstreamTestRunSpec) throws {
		let completion = XCTestExpectation(
			description: "\(spec.suite.rawValue) upstream test completed"
		)

		_ = try OrlixUpstreamTestSessionRunner(spec: spec).run(
			signalCompletion: { completion.fulfill() },
			waitForCompletion: { timeout in
				XCTWaiter.wait(for: [completion], timeout: timeout) == .completed
			}
		)
    }
}
