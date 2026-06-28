import XCTest
@testable import OrlixTestRunner

final class OrlixMLibCConformanceTests: XCTestCase {
    func testMLibCRootfsCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.mlibc)
    }
}
