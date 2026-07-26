import XCTest
@testable import OrlixOSTestApp

final class OrlixPackagesConformanceTests: XCTestCase {
    func testCoreutilsRootfsCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.coreutils)
    }
}
