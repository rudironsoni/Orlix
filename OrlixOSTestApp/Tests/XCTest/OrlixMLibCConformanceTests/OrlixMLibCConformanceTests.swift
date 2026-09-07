import XCTest
@testable import OrlixOSTestApp

final class OrlixMLibCConformanceTests: XCTestCase {
    func testMLibCRootfsCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.mlibc)
    }

    func testFopenCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.mlibcFopen)
    }

    func testPthreadCreateCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.mlibcPthreadCreate)
    }
}
