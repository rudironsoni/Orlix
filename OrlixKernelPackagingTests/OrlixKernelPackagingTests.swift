import XCTest

final class OrlixKernelPackagingTests: XCTestCase {
    func testFrameworkBundlesRealLinuxPayload() throws {
        let kernelBundle = try Self.kernelBundle()
        let payloadBundle = try XCTUnwrap(
            kernelBundle.url(forResource: "OrlixKernelPayload", withExtension: "bundle"),
            "OrlixKernel.framework must bundle the staged Linux payload bundle"
        )

        let vmlinux = payloadBundle.appendingPathComponent("vmlinux")
        XCTAssertGreaterThan(try Self.fileSize(vmlinux), 0)

        let hash = try String(
            contentsOf: payloadBundle.appendingPathComponent("vmlinux.sha256"),
            encoding: .utf8
        ).trimmingCharacters(in: .whitespacesAndNewlines)
        XCTAssertEqual(hash.count, 64)
    }

    func testFrameworkBundlesClosedProfileDeviceTrees() throws {
        let kernelBundle = try Self.kernelBundle()
        let payloadBundle = try XCTUnwrap(
            kernelBundle.url(forResource: "OrlixKernelPayload", withExtension: "bundle")
        )
        let dtbDirectory = payloadBundle.appendingPathComponent("dtbs", isDirectory: true)

        for profile in ["appstore", "development"] {
            let dtb = dtbDirectory.appendingPathComponent("\(profile).dtb")
            XCTAssertGreaterThan(try Self.fileSize(dtb), 0, "missing non-empty \(profile).dtb")
        }
    }

    func testPayloadRecordsSingleSelectedProfile() throws {
        let kernelBundle = try Self.kernelBundle()
        let payloadBundle = try XCTUnwrap(
            kernelBundle.url(forResource: "OrlixKernelPayload", withExtension: "bundle")
        )
        let selectedProfile = try String(
            contentsOf: payloadBundle.appendingPathComponent("selected_profile.txt"),
            encoding: .utf8
        ).trimmingCharacters(in: .whitespacesAndNewlines)

        XCTAssertTrue(
            ["appstore", "development"].contains(selectedProfile),
            "framework packaging must select exactly one supported Orlix profile"
        )
    }

    private static func kernelBundle() throws -> Bundle {
        if let loadedBundle = Bundle(identifier: "org.orlix.OrlixKernel") {
            return loadedBundle
        }
        if let loadedBundle = Bundle.allFrameworks.first(where: { $0.bundleIdentifier == "org.orlix.OrlixKernel" }) {
            return loadedBundle
        }

        let frameworksURL = try XCTUnwrap(
            Bundle.main.privateFrameworksURL,
            "XCTest host app has no embedded Frameworks directory"
        )
        let frameworkURL = frameworksURL.appendingPathComponent(
            "OrlixKernel.framework",
            isDirectory: true
        )
        return try XCTUnwrap(
            Bundle(url: frameworkURL),
            "OrlixKernel.framework was not embedded in the XCTest host app"
        )
    }

    private static func fileSize(_ url: URL) throws -> UInt64 {
        let attributes = try FileManager.default.attributesOfItem(atPath: url.path)
        return try XCTUnwrap(attributes[.size] as? NSNumber).uint64Value
    }
}
