import XCTest
@testable import OrlixTestRunner

final class OrlixPackagesConformanceTests: XCTestCase {
    func testCoreutilsRootfsCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.coreutils)
    }
}
