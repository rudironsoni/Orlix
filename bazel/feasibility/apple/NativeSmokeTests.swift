import Metal
import XCTest

final class NativeSmokeTests: XCTestCase {
    func testFeasibilityHarnessLoads() {
        XCTAssertEqual(1 + 1, 2)
    }

    func testMLXMetalLibraryContainsCompiledKernels() throws {
        let device = try XCTUnwrap(MTLCreateSystemDefaultDevice())
        let resources = try XCTUnwrap(Bundle(for: Self.self).url(forResource: "mlx-swift_Cmlx", withExtension: "bundle"))
        let libraryURL = try XCTUnwrap(Bundle(url: resources)?.url(forResource: "default", withExtension: "metallib"))
        let library = try device.makeLibrary(URL: libraryURL)
        XCTAssertNotNil(library.makeFunction(name: "argmin_float32"))
    }
}
