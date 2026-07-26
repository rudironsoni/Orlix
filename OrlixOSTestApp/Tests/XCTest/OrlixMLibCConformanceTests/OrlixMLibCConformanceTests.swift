import XCTest
@testable import OrlixOSTestApp

final class OrlixMLibCConformanceTests: XCTestCase {
    func testMLibCRootfsCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.mlibc)
    }
}
