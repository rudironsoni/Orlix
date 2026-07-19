import Foundation
import XCTest

final class ArchitectureInvariantTests: XCTestCase {
    private lazy var root: URL = {
        var url = URL(fileURLWithPath: #filePath)
        while url.path != "/" {
            let candidate = url.deletingLastPathComponent()
            if FileManager.default.fileExists(atPath: candidate.appendingPathComponent("project.yml").path) {
                return candidate
            }
            url = candidate
        }
        XCTFail("could not locate repository root from \(#filePath)")
        return URL(fileURLWithPath: FileManager.default.currentDirectoryPath)
    }()

    func testActiveProductSourcesDoNotUseHistoricalBranding() throws {
        let forbidden = [
            "IXLand",
            "ixland",
            "IXLAND",
            "OrlixKit"
        ]
        let files = try sourceFiles(under: [
            "project.yml",
            "OrlixKernel/Sources",
            "OrlixOS/Sources",
            "OrlixHostAdapter/Sources",
            "Orlix/Sources",
            "OrlixTestRunner/Sources",
            "tools"
        ])

        let hits = try matchingLines(in: files) { line in
            forbidden.contains { line.contains($0) }
        }

        XCTAssertTrue(hits.isEmpty, hits.joined(separator: "\n"))
    }

    func testOrlixKernelLinuxOwnerCodeDoesNotIncludeHostHeaders() throws {
        let runtimeOverlayFiles = try sourceFiles(under: [
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix",
            "OrlixKernel/Sources/ports/orlix/overlay/drivers/orlix"
        ])
        let forbiddenIncludeFragments = [
            "<CoreFoundation/",
            "<Foundation/",
            "<Darwin/",
            "<mach/",
            "<pthread.h>",
            "<unistd.h>",
            "<sys/",
            "<fcntl.h>",
            "<errno.h>",
            "<stdio.h>",
            "<stdlib.h>",
            "<string.h>",
            "\"OrlixHostAdapter",
            "<OrlixHostAdapter"
        ]

        let hits = try matchingLines(in: runtimeOverlayFiles) { line in
            let trimmed = line.trimmingCharacters(in: .whitespaces)
            guard trimmed.hasPrefix("#include") else {
                return false
            }
            return forbiddenIncludeFragments.contains { trimmed.contains($0) }
        }

        XCTAssertTrue(hits.isEmpty, hits.joined(separator: "\n"))
    }

    func testIOSRuntimeTargetsDoNotDependOnAppleContainerOrVirtualizationRuntime() throws {
        let files = try sourceFiles(under: [
            "project.yml",
            "OrlixKernel/Sources",
            "OrlixOS/Sources",
            "OrlixHostAdapter/Sources",
            "Orlix/Sources",
            "OrlixTestRunner/Sources"
        ])
        let forbidden = [
            "AppleContainer",
            "apple/container",
            "Containerization",
            "Virtualization.framework",
            "Virtualization"
        ]

        let hits = try matchingLines(in: files) { line in
            forbidden.contains { line.contains($0) }
        }

        XCTAssertTrue(hits.isEmpty, hits.joined(separator: "\n"))
    }

    func testOrlixKernelOverlayDoesNotDefineLocalLinuxUAPIClones() throws {
        let files = try sourceFiles(under: [
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix",
            "OrlixKernel/Sources/ports/orlix/overlay/drivers/orlix"
        ]).filter {
            !$0.path.contains("/tools/testing/selftests/")
        }

        let clonedUAPIDefine = try NSRegularExpression(
            pattern: #"^\s*#\s*define\s+(__NR_|SYS_|FUTEX_|CLONE_|O_[A-Z0-9_]+|AT_[A-Z0-9_]+|AF_[A-Z0-9_]+|VIRTIO_|VIRTIO_MMIO_|VIRTIO_BLK_)"#
        )
        let hits = try matchingLines(in: files) { line in
            let range = NSRange(line.startIndex..<line.endIndex, in: line)
            return clonedUAPIDefine.firstMatch(in: line, range: range) != nil
        }

        XCTAssertTrue(hits.isEmpty, hits.joined(separator: "\n"))
    }

    func testOrlixKernelProfilesEnableVirtioEnvironmentSubstrate() throws {
        let requiredSymbols = [
            "CONFIG_VIRTIO=y",
            "CONFIG_VIRTIO_MMIO=y",
            "CONFIG_VIRTIO_BLK=y",
            "CONFIG_VIRTIO_CONSOLE=y",
            "CONFIG_HW_RANDOM_VIRTIO=y",
            "CONFIG_VIRTIO_NET=y",
            "CONFIG_FUSE_FS=y",
            "CONFIG_VIRTIO_FS=y",
            "CONFIG_EXT4_FS=y",
            "CONFIG_OVERLAY_FS=y",
            "CONFIG_NAMESPACES=y",
            "CONFIG_NET_NS=y",
            "CONFIG_CGROUPS=y"
        ]
        let profileConfigs = [
            "OrlixKernel/Sources/ports/orlix/configs/release_defconfig",
            "OrlixKernel/Sources/ports/orlix/configs/development_defconfig"
        ]

        for relativePath in profileConfigs {
            let url = root.appendingPathComponent(relativePath)
            let lines = Set(
                try String(contentsOf: url)
                    .components(separatedBy: .newlines)
                    .map { $0.trimmingCharacters(in: .whitespaces) }
            )
            let missing = requiredSymbols.filter { !lines.contains($0) }
            XCTAssertTrue(
                missing.isEmpty,
                "\(relativePath) missing virtio environment substrate symbols: \(missing.joined(separator: ", "))"
            )
        }
    }

    func testOrlixKernelProductSourceListIncludesVirtioEnvironmentDrivers() throws {
        let sourceListPath = "OrlixKernel/Sources/ports/orlix/kbuild/kernel-rules.mk"
        let sourceList = try String(contentsOf: root.appendingPathComponent(sourceListPath))
        let requiredSources = [
            "drivers/block/virtio_blk.c",
            "drivers/char/virtio_console.c",
            "drivers/char/hw_random/virtio-rng.c",
            "drivers/net/net_failover.c",
            "drivers/net/virtio_net.c",
            "fs/fuse/dev.c",
            "fs/fuse/dir.c",
            "fs/fuse/file.c",
            "fs/fuse/inode.c",
            "fs/fuse/control.c",
            "fs/fuse/xattr.c",
            "fs/fuse/acl.c",
            "fs/fuse/readdir.c",
            "fs/fuse/ioctl.c",
            "fs/fuse/iomode.c",
            "fs/fuse/passthrough.c",
            "fs/fuse/virtio_fs.c",
            "net/core/failover.c"
        ]
        let missing = requiredSources.filter { !sourceList.contains($0) }

        XCTAssertTrue(
            missing.isEmpty,
            "\(sourceListPath) missing virtio environment source inputs: \(missing.joined(separator: ", "))"
        )
    }

    func testOrlixKernelSimulatorTargetStaysArm64() throws {
        let projectPath = "project.yml"
        let project = try String(contentsOf: root.appendingPathComponent(projectPath))

        XCTAssertTrue(
            project.contains("ARCHS[sdk=iphonesimulator*]: arm64"),
            "\(projectPath) must keep OrlixKernel simulator builds arm64-only; the kernel archive and HostAdapter trap plane are arm64"
        )
    }

    func testTCTIProductProfilesDisableNativeExecutionAndDebugOracle() throws {
        let productProfiles = [
            "OrlixKernel/Sources/ports/orlix/configs/release_defconfig",
            "OrlixKernel/Sources/ports/orlix/configs/development_defconfig"
        ]
        let configDirectory = root.appendingPathComponent(
            "OrlixKernel/Sources/ports/orlix/configs"
        )
        let observedProfiles = try FileManager.default
            .contentsOfDirectory(atPath: configDirectory.path)
            .filter { $0.hasSuffix("_defconfig") }
            .sorted()

        XCTAssertEqual(
            observedProfiles,
            ["development_defconfig", "release_defconfig"],
            "kernel tests must not introduce a private product profile"
        )

        for relativePath in productProfiles {
            let config = try String(contentsOf: root.appendingPathComponent(relativePath))

            XCTAssertTrue(
                config.contains("# CONFIG_ORLIX_HOSTED_EXEC_NATIVE is not set"),
                "\(relativePath) must not enable direct native guest execution"
            )
            XCTAssertTrue(
                config.contains("CONFIG_ORLIX_HOSTED_EXEC_TCTI=y"),
                "\(relativePath) must use the TCTI hosted execution backend"
            )
            XCTAssertTrue(
                config.contains("# CONFIG_ORLIX_TCTI_DEBUG_SWITCH is not set"),
                "\(relativePath) must exclude the test-only switch debug oracle"
            )
        }
    }

    func testTCTIProductionSourcesDoNotRequestJITRWXOrHostX18() throws {
        let files = try sourceFiles(under: [
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti",
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/include/asm/tcti.h",
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/mm/tcti_user_page.c",
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/mm/tcti_invalidate.c",
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/kernel/hosted_exec.c",
            "OrlixHostAdapter/Sources/OrlixHostAdapter/memory",
            "OrlixHostAdapter/Sources/OrlixHostAdapter/runtime"
        ]).filter { !$0.path.contains("/tests/") }
        let forbiddenToken = try NSRegularExpression(
            pattern: #"(?<![A-Za-z0-9_])(?:MAP_JIT|[wx]18)(?![A-Za-z0-9_])"#
        )
        let rwx = try NSRegularExpression(
            pattern: #"(?:PROT_READ|VM_PROT_READ)[^;\n]*(?:PROT_WRITE|VM_PROT_WRITE)[^;\n]*(?:PROT_EXEC|VM_PROT_EXECUTE)"#
        )

        let hits = try matchingLines(in: files) { line in
            let range = NSRange(line.startIndex..<line.endIndex, in: line)
            return forbiddenToken.firstMatch(in: line, range: range) != nil
                || rwx.firstMatch(in: line, range: range) != nil
        }

        XCTAssertTrue(hits.isEmpty, hits.joined(separator: "\n"))
    }

    func testOrlixKernelDeviceTreesExposeVirtioFsHostFolderNode() throws {
        let deviceTreePaths = [
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/boot/dts/release.dts",
            "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/boot/dts/development.dts"
        ]

        for relativePath in deviceTreePaths {
            let deviceTree = try String(contentsOf: root.appendingPathComponent(relativePath))
            let requiredFragments = [
                "virtio_hostfs: virtio@10001800",
                "compatible = \"virtio,mmio\";",
                "reg = <0x0 0x10001800 0x0 0x200>;",
                "interrupts = <36>;",
                "orlix,device-role = \"host-folder\";"
            ]
            let missing = requiredFragments.filter { !deviceTree.contains($0) }

            XCTAssertTrue(
                missing.isEmpty,
                "\(relativePath) missing virtio-fs host-folder node fragments: \(missing.joined(separator: ", "))"
            )
        }
    }

    func testKernelConformanceXCTestDoesNotDuplicateNativeTAPAssertions() throws {
        let relativePath = "OrlixTestRunner/Tests/XCTest/OrlixKernelConformanceTests/OrlixKernelConformanceTests.swift"
        let source = try String(contentsOf: root.appendingPathComponent(relativePath))
        let supportPath = "OrlixTestRunner/Tests/XCTest/Support/OrlixUpstreamXCTest.swift"
        let support = try String(contentsOf: root.appendingPathComponent(supportPath))

        XCTAssertFalse(
            source.contains("output.contains"),
            "\(relativePath) must launch native KUnit and kselftest proof without parsing Linux assertion text"
        )
        XCTAssertFalse(
            source.contains("let output = try OrlixUpstreamXCTest.run"),
            "\(relativePath) must leave native TAP assertions in their owning kernel suites"
        )
        XCTAssertFalse(
            support.contains("throws -> String"),
            "\(supportPath) must not expose native suite output for XCTest assertion parsing"
        )
    }

    private func sourceFiles(under relativePaths: [String]) throws -> [URL] {
        let manager = FileManager.default
        var urls: [URL] = []
        for relativePath in relativePaths {
            let url = root.appendingPathComponent(relativePath)
            var isDirectory: ObjCBool = false
            guard manager.fileExists(atPath: url.path, isDirectory: &isDirectory) else {
                continue
            }
            if isDirectory.boolValue {
                guard let enumerator = manager.enumerator(
                    at: url,
                    includingPropertiesForKeys: [.isRegularFileKey],
                    options: [.skipsHiddenFiles]
                ) else {
                    continue
                }
                for case let fileURL as URL in enumerator {
                    let relative = fileURL.path.replacingOccurrences(of: root.path + "/", with: "")
                    if shouldSkip(relativePath: relative) {
                        enumerator.skipDescendants()
                        continue
                    }
                    let values = try fileURL.resourceValues(forKeys: [.isRegularFileKey])
                    if values.isRegularFile == true, isScannedSource(fileURL) {
                        urls.append(fileURL)
                    }
                }
            } else if isScannedSource(url) {
                urls.append(url)
            }
        }
        return urls.sorted { $0.path < $1.path }
    }

    private func shouldSkip(relativePath: String) -> Bool {
        relativePath.hasPrefix("Build/")
            || relativePath.contains("/Build/")
            || relativePath.hasPrefix(".git/")
            || relativePath.contains("/.git/")
            || relativePath.hasPrefix(".deriveddata/")
            || relativePath.contains("/.deriveddata/")
    }

    private func isScannedSource(_ url: URL) -> Bool {
        let allowedExtensions = [
            "c",
            "cc",
            "cpp",
            "h",
            "m",
            "mm",
            "swift",
            "yml",
            "yaml",
            "mk",
            "dts",
            "S",
            "sh",
            "pl"
        ]
        return allowedExtensions.contains(url.pathExtension) || url.lastPathComponent == "project.yml"
    }

    private func matchingLines(
        in files: [URL],
        where predicate: (String) -> Bool
    ) throws -> [String] {
        var hits: [String] = []
        for file in files {
            let contents = try String(contentsOf: file)
            let relativePath = file.path.replacingOccurrences(of: root.path + "/", with: "")
            for (index, line) in contents.components(separatedBy: .newlines).enumerated() {
                if predicate(line) {
                    hits.append("\(relativePath):\(index + 1): \(line)")
                }
            }
        }
        return hits
    }
}
