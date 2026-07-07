@_spi(OrlixPrivateTesting) @testable import OrlixOS
import CryptoKit
import Foundation
import XCTest

final class OrlixEnvironmentRootRuntimeTests: XCTestCase {
    func testOCIRuntimeProcessDefaultsExecuteThroughOrlixOSTerminalSession()
        throws
    {
        let descriptor = try Self.makeOCIRuntimeProcessDefaultsDescriptor()
        XCTAssertEqual(
            descriptor.defaultCommand,
            ["/bin/sh"]
        )
        XCTAssertEqual(descriptor.defaultEnvironment["ORLIX_DESCRIPTOR_MESSAGE"], "descriptor value without path")
        XCTAssertEqual(descriptor.defaultWorkingDirectory, "/tmp")
        XCTAssertEqual(descriptor.defaultUserID, 1000)
        XCTAssertEqual(descriptor.defaultGroupID, 100)

        let runner = OrlixEnvironmentRootRuntimeProofRunner(
            fixture: .ociDerived,
            proof: .descriptorExecution
        )
        let output = try runner.run()

        XCTAssertTrue(output.contains("ORLIX_ENV_EXEC_BEGIN"))
    }

    func testOCIUserNamespaceMappingsApplyThroughOrlixOSTerminalSession()
        throws
    {
        let runner = OrlixEnvironmentRootRuntimeProofRunner(
            fixture: .ociDerived,
            proof: .userNamespaceMappings
        )
        let output = try runner.run()

        XCTAssertTrue(output.contains("ORLIX_ENV_USERNS_BEGIN"))
        XCTAssertTrue(output.contains("ORLIX_ENV_USERNS_UID_MAP_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_USERNS_GID_MAP_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_USERNS_SETGROUPS_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_USERNS_DONE"))
    }

    func testOCITimeNamespaceOffsetsApplyThroughOrlixOSTerminalSession()
        throws
    {
        let runner = OrlixEnvironmentRootRuntimeProofRunner(
            fixture: .ociDerived,
            proof: .timeNamespaceOffsets
        )
        let output = try runner.run()

        XCTAssertTrue(output.contains("ORLIX_ENV_TIMENS_BEGIN"))
        XCTAssertTrue(output.contains("ORLIX_ENV_TIMENS_MONOTONIC_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_TIMENS_BOOTTIME_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_TIMENS_DONE"))
    }

    func testOCIMaskedAndReadonlyPathsApplyThroughOrlixOSTerminalSession()
        throws
    {
        let runner = OrlixEnvironmentRootRuntimeProofRunner(
            fixture: .ociDerived,
            proof: .maskedReadonlyPaths
        )
        let output = try runner.run()

        XCTAssertTrue(output.contains("ORLIX_ENV_PATHS_BEGIN"))
        XCTAssertTrue(output.contains("ORLIX_ENV_MASKED_FILE_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_READONLY_DIR_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_PATHS_DONE"))
    }

    func testOCICgroupPidsLimitAppliesThroughOrlixOSTerminalSession()
        throws
    {
        let runner = OrlixEnvironmentRootRuntimeProofRunner(
            fixture: .ociDerived,
            proof: .cgroupPidsLimit
        )
        let output = try runner.run()

        XCTAssertTrue(output.contains("ORLIX_ENV_CGROUP_BEGIN"))
        XCTAssertTrue(output.contains("ORLIX_ENV_CGROUP_PATH_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_CGROUP_PIDS_MAX_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_CGROUP_PROCS_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_CGROUP_DONE"))
    }

    private static func makeOCIRuntimeProcessDefaultsDescriptor()
        throws -> OrlixEnvironmentDescriptor
    {
        let config = Data(
            #"""
            {
              "ociVersion": "1.1.0",
              "process": {
                "terminal": true,
                "args": ["/bin/sh"],
                "env": [
                  "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
                  "TERM=xterm-256color",
                  "ORLIX_DESCRIPTOR_MESSAGE=descriptor value without path"
                ],
                "cwd": "/tmp",
                "user": { "uid": 1000, "gid": 100 }
              },
              "root": { "path": "rootfs" }
            }
            """#.utf8
        )
        let parsed = try OrlixOCIRuntimeConfigParser().parse(config)
        return try parsed.environmentDescriptor(
            id: "orlix.oci-runtime-defaults",
            rootMount: .defaultOverlay
        )
    }

    func testTarDerivedMaterializedRootBootsAndExposesOSRelease() throws {
        let runner = OrlixEnvironmentRootRuntimeProofRunner(
            fixture: .tarDerived
        )
        let output = try runner.run()

        XCTAssertTrue(output.contains("ORLIX_ENV_OS_RELEASE_BEGIN"))
        XCTAssertTrue(output.contains("ID=orlix-tar-runtime-test-fixture"))
        XCTAssertTrue(output.contains("ORLIX_ENV_OS_RELEASE_DONE"))
    }

    func testTarDerivedMaterializedRootExposesLinuxPseudoFilesystems()
        throws
    {
        let runner = OrlixEnvironmentRootRuntimeProofRunner(
            fixture: .tarDerived,
            proof: .pseudoFilesystems
        )
        let output = try runner.run()

        XCTAssertTrue(output.contains("ORLIX_ENV_PSEUDOFS_BEGIN"))
        XCTAssertTrue(output.contains("ORLIX_ENV_PROC_MOUNTS_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_PROC_SELF_STATUS_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_PROC_SELF_FD_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_DEV_NULL_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_DEV_URANDOM_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_DEV_PTMX_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_DEV_PTS_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_SYS_BLOCK_VDA_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_SYS_BLOCK_VDB_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_PSEUDOFS_DONE"))
    }

    func testTarDerivedMaterializedRootUsesLinuxRuntimeTmpfsMounts()
        throws
    {
        let runner = OrlixEnvironmentRootRuntimeProofRunner(
            fixture: .tarDerived,
            proof: .runtimeTmpfs
        )
        let output = try runner.runOnMutableFixtureCopy()

        XCTAssertTrue(output.contains("ORLIX_ENV_TMPFS_BEGIN"))
        XCTAssertTrue(output.contains("ORLIX_ENV_TMP_MOUNT_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_RUN_MOUNT_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_DEV_SHM_MOUNT_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_TMP_WRITE_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_RUN_WRITE_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_DEV_SHM_WRITE_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_TMPFS_DONE"))
    }

    func testCopiedNamedEnvironmentMaterializedRootBootsAndExposesOSRelease()
        throws
    {
        let runner = OrlixEnvironmentRootRuntimeProofRunner(
            fixture: .tarDerived
        )
        let output = try runner.runCopiedNamedEnvironment()

        XCTAssertTrue(output.contains("ORLIX_ENV_OS_RELEASE_BEGIN"))
        XCTAssertTrue(output.contains("ID=orlix-tar-runtime-test-fixture"))
        XCTAssertTrue(output.contains("ORLIX_ENV_OS_RELEASE_DONE"))
    }

    func testCopiedNamedEnvironmentOverlayMutationDoesNotChangeParent()
        throws
    {
        let runner = OrlixEnvironmentRootRuntimeProofRunner(
            fixture: .tarDerived,
            proof: .overlayMutation
        )
        let output = try runner
            .runCopiedNamedEnvironmentValidatingParentIsolation()

        XCTAssertTrue(output.contains("ORLIX_ENV_OVERLAY_MUTATION_BEGIN"))
        XCTAssertTrue(output.contains("ORLIX_ENV_OVERLAY_COPYUP_WRITE_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_OVERLAY_COPYUP_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_OVERLAY_UNLINK_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_OVERLAY_MUTATION_DONE"))
    }

    func testCopiedNamedEnvironmentSessionSelectionEntersRootAndDescriptor()
        throws
    {
        let runner = OrlixEnvironmentRootRuntimeProofRunner(
            fixture: .ociDerived,
            proof: .descriptorExecution
        )
        let output = try runner.runCopiedNamedEnvironmentThroughSessionSelection()

        XCTAssertTrue(output.contains("ORLIX_ENV_EXEC_BEGIN"))
        XCTAssertTrue(output.contains("argv0=orlix-descriptor-xxxx"))
        XCTAssertTrue(output.contains("argv1=argument with spaces"))
        XCTAssertTrue(output.contains("env=descriptor value with spaces"))
        let pwdRange = try XCTUnwrap(output.range(of: "pwd="))
        XCTAssertNotNil(output.range(of: "/tmp", range: pwdRange.upperBound..<output.endIndex))
        XCTAssertTrue(output.contains("Uid:\t1000"))
        XCTAssertTrue(output.contains("Gid:\t100"))
        XCTAssertTrue(output.contains("ID=orlix-oci-runtime-test-fixture"))
        XCTAssertTrue(output.contains("ORLIX_ENV_EXEC_DONE"))
    }

    func testCopiedNamedEnvironmentSessionSelectionRunsPackagedCoreutilsCommand()
        throws
    {
        let runner = OrlixEnvironmentRootRuntimeProofRunner(
            fixture: .ociDerived,
            proof: .coreutilsCommand
        )
        let output = try runner.runCopiedNamedEnvironmentThroughSessionSelection()

        XCTAssertTrue(output.contains("ORLIX_ENV_COREUTILS_COMMAND_BEGIN"))
        XCTAssertTrue(output.contains("coreutils-command-ok"))
        XCTAssertTrue(output.contains("ORLIX_ENV_COREUTILS_STDOUT_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_COREUTILS_STDERR_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_COREUTILS_EXIT_STATUS_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_COREUTILS_COMMAND_DONE"))
        XCTAssertTrue(output.contains("orlix-init: process exited pid="))
        XCTAssertTrue(output.contains("status=0"))
    }

    func testTarDerivedNamedEnvironmentCrossBootWrite() throws {
        let runner = OrlixEnvironmentRootRuntimeProofRunner(
            fixture: .tarDerived,
            proof: .crossBootWrite
        )
        let output = try runner.runPersistentCopiedNamedEnvironmentCrossBootWrite()

        XCTAssertTrue(output.contains("ORLIX_ENV_CROSSBOOT_WRITE_BEGIN"))
        XCTAssertTrue(output.contains("ORLIX_ENV_CROSSBOOT_WRITE_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_CROSSBOOT_SYNC_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_CROSSBOOT_REREAD_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_CROSSBOOT_WRITE_DONE"))
    }

    func testTarDerivedNamedEnvironmentCrossBootVerify() throws {
        let runner = OrlixEnvironmentRootRuntimeProofRunner(
            fixture: .tarDerived,
            proof: .crossBootVerify
        )
        let output = try runner.runPersistentCopiedNamedEnvironmentCrossBootVerify()

        XCTAssertTrue(output.contains("ORLIX_ENV_CROSSBOOT_VERIFY_BEGIN"))
        XCTAssertTrue(output.contains("ORLIX_ENV_CROSSBOOT_SURVIVED_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_CROSSBOOT_CLEANUP_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_CROSSBOOT_VERIFY_DONE"))
    }

    func testOCIDerivedNamedEnvironmentCrossBootWrite() throws {
        let runner = OrlixEnvironmentRootRuntimeProofRunner(
            fixture: .ociDerived,
            proof: .crossBootWrite
        )
        let output = try runner.runPersistentCopiedNamedEnvironmentCrossBootWrite()

        XCTAssertTrue(output.contains("ORLIX_ENV_CROSSBOOT_WRITE_BEGIN"))
        XCTAssertTrue(output.contains("ORLIX_ENV_CROSSBOOT_WRITE_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_CROSSBOOT_SYNC_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_CROSSBOOT_REREAD_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_CROSSBOOT_WRITE_DONE"))
    }

    func testOCIDerivedNamedEnvironmentCrossBootVerify() throws {
        let runner = OrlixEnvironmentRootRuntimeProofRunner(
            fixture: .ociDerived,
            proof: .crossBootVerify
        )
        let output = try runner.runPersistentCopiedNamedEnvironmentCrossBootVerify()

        XCTAssertTrue(output.contains("ORLIX_ENV_CROSSBOOT_VERIFY_BEGIN"))
        XCTAssertTrue(output.contains("ORLIX_ENV_CROSSBOOT_SURVIVED_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_CROSSBOOT_CLEANUP_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_CROSSBOOT_VERIFY_DONE"))
    }

    func testOCIDerivedMaterializedRootBootsAndExposesOSRelease() throws {
        let runner = OrlixEnvironmentRootRuntimeProofRunner(
            fixture: .ociDerived
        )
        let output = try runner.run()

        XCTAssertTrue(output.contains("ORLIX_ENV_OS_RELEASE_BEGIN"))
        XCTAssertTrue(output.contains("ID=orlix-oci-runtime-test-fixture"))
        XCTAssertTrue(output.contains("ORLIX_ENV_OS_RELEASE_DONE"))
    }

    func testOCIDerivedMaterializedRootUsesLinuxOverlayCopyUpAndWhiteout() throws {
        let runner = OrlixEnvironmentRootRuntimeProofRunner(
            fixture: .ociDerived,
            proof: .overlayMutation
        )
        let output = try runner.runOnMutableFixtureCopy()

        XCTAssertTrue(output.contains("ORLIX_ENV_OVERLAY_MUTATION_BEGIN"))
        XCTAssertTrue(output.contains("ORLIX_ENV_OVERLAY_COPYUP_WRITE_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_OVERLAY_COPYUP_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_OVERLAY_UNLINK_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_OVERLAY_MUTATION_DONE"))
    }

    func testOCIDerivedOverlayMutationLeavesBaseImageStableAndChangesStateImage()
        throws
    {
        let runner = OrlixEnvironmentRootRuntimeProofRunner(
            fixture: .ociDerived,
            proof: .overlayMutation
        )
        let output = try runner.runOnMutableFixtureCopyValidatingOverlayStorage()

        XCTAssertTrue(output.contains("ORLIX_ENV_OVERLAY_MUTATION_BEGIN"))
        XCTAssertTrue(output.contains("ORLIX_ENV_OVERLAY_COPYUP_WRITE_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_OVERLAY_COPYUP_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_OVERLAY_UNLINK_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_OVERLAY_MUTATION_DONE"))
    }

    func testOCIDerivedMaterializedRootBindsDescriptorExecutionDefaults()
        throws
    {
        let runner = OrlixEnvironmentRootRuntimeProofRunner(
            fixture: .ociDerived,
            proof: .descriptorExecution
        )
        let output = try runner.runOnMutableFixtureCopy()

        XCTAssertTrue(output.contains("ORLIX_ENV_EXEC_BEGIN"))
        XCTAssertTrue(output.contains("argv0=orlix-descriptor-xxxx"))
        XCTAssertTrue(output.contains("argv1=argument with spaces"))
        XCTAssertTrue(output.contains("env=descriptor value with spaces"))
        XCTAssertTrue(output.contains("pwd=/tmp"))
        XCTAssertTrue(output.contains("Uid:\t1000"))
        XCTAssertTrue(output.contains("Gid:\t100"))
        XCTAssertTrue(output.contains("ORLIX_ENV_EXEC_DONE"))
    }

    func testOCIDerivedMaterializedRootBindsLongDescriptorExecutionDefaults()
        throws
    {
        let runner = OrlixEnvironmentRootRuntimeProofRunner(
            fixture: .ociDerived,
            proof: .longDescriptorExecution
        )
        let output = try runner.runOnMutableFixtureCopy()

        XCTAssertTrue(output.contains("ORLIX_ENV_EXEC_BEGIN"))
        XCTAssertTrue(output.contains("argv0=orlix-descriptor-xxxxxxxx"))
        XCTAssertTrue(output.contains("argv1=argument with spaces"))
        XCTAssertTrue(output.contains("env=descriptor value with spaces"))
        XCTAssertTrue(output.contains("pwd=/tmp"))
        XCTAssertTrue(output.contains("Uid:\t1000"))
        XCTAssertTrue(output.contains("Gid:\t100"))
        XCTAssertTrue(output.contains("ORLIX_ENV_EXEC_DONE"))
    }

    func testOCIDerivedMaterializedRootRunsLinuxPathDescriptorDefaults()
        throws
    {
        let runner = OrlixEnvironmentRootRuntimeProofRunner(
            fixture: .ociDerived,
            proof: .linuxPathDescriptorExecution
        )
        let output = try runner.runOnMutableFixtureCopy()

        XCTAssertTrue(output.contains("ORLIX_ENV_EXEC_BEGIN"))
        XCTAssertTrue(output.contains("argv0=linux-path-argv0"))
        XCTAssertTrue(output.contains("argv1="))
        XCTAssertTrue(output.contains("argv2=argument after empty"))
        XCTAssertTrue(output.contains("pwd=/"))
        XCTAssertTrue(output.contains("ORLIX_ENV_EXEC_DONE"))
    }

    func testOCIDerivedMaterializedRootResolvesDescriptorCommandThroughPath()
        throws
    {
        let runner = OrlixEnvironmentRootRuntimeProofRunner(
            fixture: .ociDerived,
            proof: .pathLookupDescriptorExecution
        )
        let output = try runner.runOnMutableFixtureCopy()

        XCTAssertTrue(output.contains("ORLIX_ENV_EXEC_BEGIN"))
        XCTAssertTrue(output.contains("argv0=path-lookup-argv0"))
        XCTAssertTrue(output.contains("argv1=argument after path lookup"))
        XCTAssertTrue(output.contains("pwd=/"))
        XCTAssertTrue(output.contains("ORLIX_ENV_EXEC_DONE"))
    }

    func testOCIDerivedMaterializedRootResolvesDescriptorCommandThroughFallbackPath()
        throws
    {
        let runner = OrlixEnvironmentRootRuntimeProofRunner(
            fixture: .ociDerived,
            proof: .pathLookupWithoutPATHDescriptorExecution
        )
        let output = try runner.runOnMutableFixtureCopy()

        XCTAssertTrue(output.contains("ORLIX_ENV_EXEC_BEGIN"))
        XCTAssertTrue(output.contains("argv0=path-fallback-argv0"))
        XCTAssertTrue(output.contains("argv1=argument after fallback path lookup"))
        XCTAssertTrue(output.contains("env=descriptor value without path"))
        XCTAssertTrue(output.contains("pwd=/"))
        XCTAssertTrue(output.contains("ORLIX_ENV_EXEC_DONE"))
    }

    func testOCIDerivedMaterializedRootExposesLinuxPseudoFilesystems()
        throws
    {
        let runner = OrlixEnvironmentRootRuntimeProofRunner(
            fixture: .ociDerived,
            proof: .pseudoFilesystems
        )
        let output = try runner.run()

        XCTAssertTrue(output.contains("ORLIX_ENV_PSEUDOFS_BEGIN"))
        XCTAssertTrue(output.contains("ORLIX_ENV_PROC_MOUNTS_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_PROC_SELF_STATUS_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_PROC_SELF_FD_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_DEV_NULL_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_DEV_URANDOM_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_DEV_PTMX_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_DEV_PTS_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_SYS_BLOCK_VDA_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_SYS_BLOCK_VDB_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_PSEUDOFS_DONE"))
    }

	func testOCIDerivedMaterializedRootUsesLinuxPTYStdio() throws {
		let runner = OrlixEnvironmentRootRuntimeProofRunner(
			fixture: .ociDerived,
			proof: .ptyStdio
        )
        let output = try runner.run()

        XCTAssertTrue(output.contains("ORLIX_ENV_PTY_BEGIN"))
        XCTAssertTrue(output.contains("ORLIX_ENV_PTY_STDIN_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_PTY_STDOUT_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_PTY_STDERR_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_PTY_PATH_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_PTY_WAITING_FOR_INPUT"))
        XCTAssertTrue(output.contains("ORLIX_ENV_PTY_DELAYED_INPUT_OK"))
		XCTAssertTrue(output.contains("/dev/pts/"))
		XCTAssertTrue(output.contains("ORLIX_ENV_PTY_DONE"))
	}

	func testOCIDerivedRuntimeTerminalFalseUsesInheritedStdio() throws {
		let runner = OrlixEnvironmentRootRuntimeProofRunner(
			fixture: .ociDerived,
			proof: .stdioExecution
		)
		let output = try runner.runOCITerminalFalseOnMutableFixtureCopy()

		XCTAssertTrue(output.contains("ORLIX_ENV_STDIO_BEGIN"))
		XCTAssertTrue(output.contains("ORLIX_ENV_STDIO_STDOUT_OK"))
		XCTAssertTrue(output.contains("ORLIX_ENV_STDIO_STDERR_OK"))
		XCTAssertTrue(output.contains("ORLIX_ENV_STDIO_NOT_PTY_OK"))
		XCTAssertTrue(output.contains("ORLIX_ENV_STDIO_DONE"))
	}

	func testOCIDerivedRuntimeLifecycleIsObservedFromLinuxInitOutput() throws {
		let runner = OrlixEnvironmentRootRuntimeProofRunner(
			fixture: .ociDerived,
			proof: .stdioExecution
		)
		let result = try runner.runOCITerminalFalseObservedOnMutableFixtureCopy()

		XCTAssertTrue(result.output.contains("orlix-init: process started pid="))
		XCTAssertTrue(result.output.contains("orlix-init: process exited pid="))
		XCTAssertTrue(result.output.contains("ORLIX_ENV_STDIO_DONE"))
		XCTAssertEqual(result.run.startedStateReport.status, .running)
		XCTAssertNotNil(result.run.startedStateReport.pid)
		XCTAssertEqual(result.run.completedStateReport.status, .stopped)
		XCTAssertEqual(result.run.completedStateReport.exitStatus, 0)
		XCTAssertEqual(result.finalState.status, .stopped)
		XCTAssertEqual(result.finalState.exitStatus, 0)
		XCTAssertEqual(result.deletedEnvironment.lifecycleState, .deleted)
		XCTAssertFalse(FileManager.default.fileExists(atPath: result.lifecycleRecordURL.path))
		XCTAssertFalse(FileManager.default.fileExists(atPath: result.environmentDirectoryURL.path))
	}

	func testOCIDerivedMaterializedRootUsesLinuxRuntimeTmpfsMounts()
	throws
	{
        let runner = OrlixEnvironmentRootRuntimeProofRunner(
            fixture: .ociDerived,
            proof: .runtimeTmpfs
        )
        let output = try runner.runOnMutableFixtureCopy()

        XCTAssertTrue(output.contains("ORLIX_ENV_TMPFS_BEGIN"))
        XCTAssertTrue(output.contains("ORLIX_ENV_TMP_MOUNT_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_RUN_MOUNT_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_DEV_SHM_MOUNT_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_TMP_WRITE_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_RUN_WRITE_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_DEV_SHM_WRITE_OK"))
        XCTAssertTrue(output.contains("ORLIX_ENV_TMPFS_DONE"))
    }
}

private struct OrlixEnvironmentObservedRuntimeResult {
	let output: String
	let run: OrlixOCIEnvironmentRunResult
	let finalState: OrlixOCIRuntimeStateReport
	let deletedEnvironment: OrlixOCIEnvironmentDeleteResult
	let lifecycleRecordURL: URL
	let environmentDirectoryURL: URL
}

private final class OrlixEnvironmentRootRuntimeProofRunner: @unchecked Sendable {
    private static let readyFile = ".ready"
    private static let firstOutputTimeout: TimeInterval = 30
    private static let timeout: TimeInterval = 600
    fileprivate static let osReleaseCommandScript = [
        #"printf '%s%s\n' ORLIX_ENV_ OS_RELEASE_BEGIN"#,
        #"/bin/cat /etc/os-release"#,
        #"printf '%s%s\n' ORLIX_ENV_ OS_RELEASE_DONE"#,
    ].joined(separator: "\r") + "\r"
    private let fixture: RuntimeFixture
    private let proof: RuntimeProof

    init(
        fixture: RuntimeFixture,
        proof: RuntimeProof = .osRelease
    ) {
        self.fixture = fixture
        self.proof = proof
    }

    func run() throws -> String {
        let fixtureRoot = try Self.fixtureRoot(for: fixture)
        return try run(fixtureRoot: fixtureRoot)
    }

	func runOnMutableFixtureCopy() throws -> String {
		let sourceRoot = try Self.fixtureRoot(for: fixture)
		let copiedRoot = try Self.mutableFixtureCopy(of: sourceRoot, fixture: fixture)
		defer {
			try? FileManager.default.removeItem(at: copiedRoot.root)
        }

		return try run(fixtureRoot: copiedRoot)
	}

	func runOCITerminalFalseOnMutableFixtureCopy() throws -> String {
		let sourceRoot = try Self.fixtureRoot(for: fixture)
		let copiedRoot = try Self.mutableFixtureCopy(of: sourceRoot, fixture: fixture)
		defer {
			try? FileManager.default.removeItem(at: copiedRoot.root)
		}

		return try run(
			fixtureRoot: copiedRoot,
			descriptor: descriptor(
				environmentID: fixture.environmentID,
				source: fixture.source,
				rootImageIdentifier: fixture.rootImageIdentifier
			),
			ociTerminal: false
		)
	}

	func runOCITerminalFalseObservedOnMutableFixtureCopy()
	throws -> OrlixEnvironmentObservedRuntimeResult
	{
		let sourceRoot = try Self.fixtureRoot(for: fixture)
		let copiedRoot = try Self.mutableFixtureCopy(of: sourceRoot, fixture: fixture)
		defer {
			try? FileManager.default.removeItem(at: copiedRoot.root)
		}

		return try runObserved(
			fixtureRoot: copiedRoot,
			descriptor: descriptor(
				environmentID: fixture.environmentID,
				source: fixture.source,
				rootImageIdentifier: fixture.rootImageIdentifier
			)
		)
	}

	func runOnMutableFixtureCopyValidatingOverlayStorage() throws -> String {
		let sourceRoot = try Self.fixtureRoot(for: fixture)
		let copiedRoot = try Self.mutableFixtureCopy(of: sourceRoot, fixture: fixture)
        defer {
            try? FileManager.default.removeItem(at: copiedRoot.root)
        }

        let descriptor = descriptor(
            environmentID: fixture.environmentID,
            source: fixture.source,
            rootImageIdentifier: fixture.rootImageIdentifier
        )
        let layout = try OrlixEnvironmentStorageLayout.layout(
            forEnvironmentID: descriptor.id,
            linuxStateRoot: copiedRoot.linuxStateRoot,
            cacheRoot: copiedRoot.cacheRoot,
            scratchRoot: copiedRoot.scratchRoot
        )
        let baseBefore = try Self.sha256Hex(of: layout.baseImageURL)
        let stateBefore = try Self.sha256Hex(of: layout.stateImageURL)

        let output = try run(fixtureRoot: copiedRoot, descriptor: descriptor)

        XCTAssertEqual(
            try Self.sha256Hex(of: layout.baseImageURL),
            baseBefore,
            "Overlay mutation must not rewrite the read-only lower base image."
        )
        XCTAssertNotEqual(
            try Self.sha256Hex(of: layout.stateImageURL),
            stateBefore,
            "Overlay mutation must persist into the writable state image."
        )
        return output
    }

    func runCopiedNamedEnvironment() throws -> String {
        let sourceRoot = try Self.fixtureRoot(for: fixture)
        let copiedRoot = try Self.mutableFixtureCopy(of: sourceRoot, fixture: fixture)
        defer {
            try? FileManager.default.removeItem(at: copiedRoot.root)
        }

        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: copiedRoot.linuxStateRoot,
            cacheRoot: copiedRoot.cacheRoot,
            scratchRoot: copiedRoot.scratchRoot
        )
        let parentDescriptor = descriptor(
            environmentID: fixture.environmentID,
            source: fixture.source,
            rootImageIdentifier: fixture.rootImageIdentifier
        )
        try registry.save(parentDescriptor)
        let copiedDescriptor = try registry.copyEnvironment(
            from: fixture.environmentID,
            to: fixture.copiedEnvironmentID,
            rootImageIdentifier: fixture.copiedRootImageIdentifier
        )

        return try run(fixtureRoot: copiedRoot, descriptor: copiedDescriptor)
    }

    func runCopiedNamedEnvironmentValidatingParentIsolation() throws -> String {
        let sourceRoot = try Self.fixtureRoot(for: fixture)
        let copiedRoot = try Self.mutableFixtureCopy(of: sourceRoot, fixture: fixture)
        defer {
            try? FileManager.default.removeItem(at: copiedRoot.root)
        }

        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: copiedRoot.linuxStateRoot,
            cacheRoot: copiedRoot.cacheRoot,
            scratchRoot: copiedRoot.scratchRoot
        )
        let parentDescriptor = descriptor(
            environmentID: fixture.environmentID,
            source: fixture.source,
            rootImageIdentifier: fixture.rootImageIdentifier
        )
        try registry.save(parentDescriptor)
        let parentLayout = try registry.layout(forEnvironmentID: fixture.environmentID)
        let parentBaseBefore = try Self.sha256Hex(of: parentLayout.baseImageURL)
        let parentStateBefore = try Self.sha256Hex(of: parentLayout.stateImageURL)

        let copiedDescriptor = try registry.copyEnvironment(
            from: fixture.environmentID,
            to: fixture.copiedEnvironmentID,
            rootImageIdentifier: fixture.copiedRootImageIdentifier
        )
        let copiedLayout = try registry.layout(
            forEnvironmentID: fixture.copiedEnvironmentID
        )
        let copiedBaseBefore = try Self.sha256Hex(of: copiedLayout.baseImageURL)
        let copiedStateBefore = try Self.sha256Hex(of: copiedLayout.stateImageURL)

        let output = try run(fixtureRoot: copiedRoot, descriptor: copiedDescriptor)

        XCTAssertEqual(
            try Self.sha256Hex(of: parentLayout.baseImageURL),
            parentBaseBefore,
            "Copied environment mutation must not rewrite the parent base image."
        )
        XCTAssertEqual(
            try Self.sha256Hex(of: parentLayout.stateImageURL),
            parentStateBefore,
            "Copied environment mutation must not rewrite the parent state image."
        )
        XCTAssertEqual(
            try Self.sha256Hex(of: copiedLayout.baseImageURL),
            copiedBaseBefore,
            "Copied environment mutation must not rewrite its read-only base image."
        )
        XCTAssertNotEqual(
            try Self.sha256Hex(of: copiedLayout.stateImageURL),
            copiedStateBefore,
            "Copied environment mutation must persist into the copied writable state image."
        )
        return output
    }

    func runCopiedNamedEnvironmentThroughSessionSelection() throws -> String {
        let sourceRoot = try Self.fixtureRoot(for: fixture)
        let copiedRoot = try Self.mutableFixtureCopy(of: sourceRoot, fixture: fixture)
        defer {
            try? FileManager.default.removeItem(at: copiedRoot.root)
        }

        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: copiedRoot.linuxStateRoot,
            cacheRoot: copiedRoot.cacheRoot,
            scratchRoot: copiedRoot.scratchRoot
        )
        let parentDescriptor = descriptor(
            environmentID: fixture.environmentID,
            source: fixture.source,
            rootImageIdentifier: fixture.rootImageIdentifier
        )
        try registry.save(parentDescriptor)
        let copiedDescriptor = try registry.copyEnvironment(
            from: fixture.environmentID,
            to: fixture.copiedEnvironmentID,
            rootImageIdentifier: fixture.copiedRootImageIdentifier
        )

        return try run(
            fixtureRoot: copiedRoot,
            descriptor: copiedDescriptor,
            launchThroughSessionSelection: true
        )
    }

    func runPersistentCopiedNamedEnvironmentCrossBootWrite() throws -> String {
        let fixtureRoot = try Self.preparePersistentCrossBootFixtureCopy(
            source: Self.fixtureRoot(for: fixture),
            fixture: fixture
        )
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: fixtureRoot.linuxStateRoot,
            cacheRoot: fixtureRoot.cacheRoot,
            scratchRoot: fixtureRoot.scratchRoot
        )
        let parentDescriptor = descriptor(
            environmentID: fixture.environmentID,
            source: fixture.source,
            rootImageIdentifier: fixture.rootImageIdentifier
        )
        try registry.save(parentDescriptor)
        let copiedDescriptor = try registry.copyEnvironment(
            from: fixture.environmentID,
            to: fixture.copiedEnvironmentID,
            rootImageIdentifier: fixture.copiedRootImageIdentifier
        )

        return try run(
            fixtureRoot: fixtureRoot,
            descriptor: copiedDescriptor,
            launchThroughSessionSelection: true
        )
    }

    func runPersistentCopiedNamedEnvironmentCrossBootVerify() throws -> String {
        let fixtureRoot = try Self.persistentCrossBootFixtureCopy(for: fixture)
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: fixtureRoot.linuxStateRoot,
            cacheRoot: fixtureRoot.cacheRoot,
            scratchRoot: fixtureRoot.scratchRoot
        )
        let descriptor = try registry.load(environmentID: fixture.copiedEnvironmentID)

        return try run(
            fixtureRoot: fixtureRoot,
            descriptor: descriptor,
            launchThroughSessionSelection: true
        )
    }

    private func run(fixtureRoot: EnvironmentRootFixture) throws -> String {
        try run(
            fixtureRoot: fixtureRoot,
            descriptor: descriptor(
                environmentID: fixture.environmentID,
                source: fixture.source,
                rootImageIdentifier: fixture.rootImageIdentifier
            )
        )
    }

	private func run(
		fixtureRoot: EnvironmentRootFixture,
		descriptor: OrlixEnvironmentDescriptor,
		launchThroughSessionSelection: Bool = false,
		ociTerminal: Bool? = nil
	) throws -> String {
        let registry = OrlixEnvironmentRegistry(
            linuxStateRoot: fixtureRoot.linuxStateRoot,
            cacheRoot: fixtureRoot.cacheRoot,
            scratchRoot: fixtureRoot.scratchRoot
        )
        let layout = try OrlixEnvironmentStorageLayout.layout(
            forEnvironmentID: descriptor.id,
            linuxStateRoot: fixtureRoot.linuxStateRoot,
            cacheRoot: fixtureRoot.cacheRoot,
            scratchRoot: fixtureRoot.scratchRoot
		)
		let terminal = OrlixTerminalSession()
		let session: OrlixLinuxSession
		if let ociTerminal {
			try registry.save(descriptor)
			let ociRuntimeSession = OrlixOCIRuntimeSessionDescriptor(
				id: descriptor.id,
				lifecycleState: .created,
				terminal: ociTerminal,
				consoleSize: nil,
				environment: descriptor
			)
			session = try OrlixLinuxSession(
				ociRuntimeSession: ociRuntimeSession,
				registry: registry,
				terminal: terminal
			)
		} else if launchThroughSessionSelection {
			session = try OrlixLinuxSession(
				environmentID: descriptor.id,
				registry: registry,
                terminal: terminal
            )
        } else {
            let rootImage = try OrlixEnvironmentRootImage.materialized(
                descriptor: descriptor,
                layout: layout
            )
            session = OrlixLinuxSession(
                materializedRootImage: rootImage,
                terminal: terminal
            )
        }
        let terminalLog = EnvironmentRootTerminalLog()
        terminalLog.writeLine("fixture=\(fixtureRoot.root.path)")
        terminalLog.writeLine("base=\(layout.baseImageURL.path)")
        terminalLog.writeLine("state=\(layout.stateImageURL.path)")
		terminalLog.writeLine(
			"rootImageIdentifier=\(session.bootConfig.rootImageIdentifier)"
		)

		let recorder = EnvironmentRootOutputRecorder(terminalLog: terminalLog)
        let bootStatus = EnvironmentRootBootStatusRecorder()
        let completion = DispatchSemaphore(value: 0)
        let output = session.terminal.attachOutput { data in
            recorder.append(data)
            let text = recorder.text

            if self.proof.sendsInteractiveCommands,
               !recorder.hasSentProofCommands,
               Self.containsShellPrompt(text) {
                terminalLog.writeLine("shell prompt detected; sending environment proof commands")
                recorder.markProofCommandsSent()
                session.terminal.send(Data(self.proof.commandScript.utf8))
            }

            if let delayedInput = self.proof.delayedInput,
               !recorder.hasSentDelayedInput,
               text.contains(self.proof.delayedInputPrompt) {
                terminalLog.writeLine("delayed PTY input prompt detected; sending payload")
                recorder.markDelayedInputSent()
                session.terminal.send(Data(delayedInput.utf8))
            }

            if let postDelayedInputScript = self.proof.postDelayedInputScript,
               !recorder.hasSentPostDelayedInputScript,
               text.contains(self.proof.delayedInputSuccessMarker) {
                terminalLog.writeLine("delayed PTY input accepted; sending completion command")
                recorder.markPostDelayedInputScriptSent()
                session.terminal.send(Data(postDelayedInputScript.utf8))
            }

            if Self.containsTerminalCondition(text, proof: self.proof) {
                completion.signal()
            }
        }
        defer { output.cancel() }

        DispatchQueue.global(qos: .userInitiated).async {
            terminalLog.writeLine("boot starting")
            let status = session.boot()
            terminalLog.writeLine("boot returned status=\(status.message)")
            bootStatus.set(status)
            if status != .ok {
                completion.signal()
            }
        }

        let deadline = Date().addingTimeInterval(Self.timeout)
        let firstOutputDeadline = Date().addingTimeInterval(Self.firstOutputTimeout)
        while completion.wait(timeout: .now() + .seconds(1)) != .success {
            if recorder.byteCount == 0,
               Date() >= firstOutputDeadline {
                throw OrlixEnvironmentRootRuntimeProofError.noTerminalOutput(
                    Self.firstOutputTimeout,
                    terminalLog.url
                )
            }
            if Date() >= deadline {
                let text = recorder.text
                if let marker = Self.firstFatalMarker(in: text) {
                    throw OrlixEnvironmentRootRuntimeProofError.fatalMarker(marker)
                }
                throw OrlixEnvironmentRootRuntimeProofError.timeout(
                    Self.timeout,
                    text,
                    terminalLog.url,
                    terminalLog.tail()
                )
            }
        }

        if let status = bootStatus.value, status != .ok {
            if status == .alreadyStarted {
                throw XCTSkip(
                    "OrlixBoot is one boot per XCTest app process; run this runtime proof as a focused test for Linux execution evidence."
                )
            }
            throw OrlixEnvironmentRootRuntimeProofError.bootFailed(status)
        }

		let text = Self.normalized(recorder.text)
		try validate(text, terminalLog: terminalLog)
		return text
	}

	private func runObserved(
		fixtureRoot: EnvironmentRootFixture,
		descriptor: OrlixEnvironmentDescriptor
	) throws -> OrlixEnvironmentObservedRuntimeResult {
		let registry = OrlixEnvironmentRegistry(
			linuxStateRoot: fixtureRoot.linuxStateRoot,
			cacheRoot: fixtureRoot.cacheRoot,
			scratchRoot: fixtureRoot.scratchRoot
		)
		let layout = try OrlixEnvironmentStorageLayout.layout(
			forEnvironmentID: descriptor.id,
			linuxStateRoot: fixtureRoot.linuxStateRoot,
			cacheRoot: fixtureRoot.cacheRoot,
			scratchRoot: fixtureRoot.scratchRoot
		)
		let terminal = OrlixTerminalSession()
		let terminalLog = EnvironmentRootTerminalLog()
		terminalLog.writeLine("fixture=\(fixtureRoot.root.path)")
		terminalLog.writeLine("base=\(layout.baseImageURL.path)")
		terminalLog.writeLine("state=\(layout.stateImageURL.path)")
		terminalLog.writeLine(
			"rootImageIdentifier=\(descriptor.rootImageIdentifier)"
		)
		let recorder = EnvironmentRootOutputRecorder(terminalLog: terminalLog)
		let output = terminal.attachOutput { data in
			recorder.append(data)
		}
		defer {
			output.cancel()
		}

		try writeOCIRuntimeConfig(
			terminal: false,
			rootPath: "imported-root",
			to: fixtureRoot.root
		)
		let runtime = OrlixOCIRuntime(registry: registry)
		let lifecycle = try OrlixOCIRuntimeBundle
			.load(from: fixtureRoot.root)
			.lifecycleController(id: descriptor.id)
			.create()
		let processHandle = try OrlixOCIRuntimeProcessHandle(
			lifecycle: lifecycle,
			rootMount: .defaultOverlay,
			rootImageIdentifier: descriptor.rootImageIdentifier
		)
		try registry.save(processHandle.sessionDescriptor.environment)
		try runtime.lifecycleStore.save(lifecycle)
		let installer = OrlixOCIEnvironmentInstaller(registry: registry)
		let run = try installer.run(
			id: descriptor.id,
			terminal: terminal,
			observationTimeout: 60
		)
		let finalState = try installer.state(id: descriptor.id)
		let lifecycleRecordURL = try runtime.lifecycleStore.recordURL(
			forID: descriptor.id
		)
		let environmentDirectoryURL = layout.rootDirectory
		let deletedEnvironment = try installer.delete(id: descriptor.id)
		let text = Self.normalized(recorder.text)
		try validate(text, terminalLog: terminalLog)
		return OrlixEnvironmentObservedRuntimeResult(
			output: text,
			run: run,
			finalState: finalState,
			deletedEnvironment: deletedEnvironment,
			lifecycleRecordURL: lifecycleRecordURL,
			environmentDirectoryURL: environmentDirectoryURL
		)
	}

	private func writeOCIRuntimeConfig(
		terminal: Bool,
		rootPath: String,
		to bundleRoot: URL
	) throws {
		let environment = proof.defaultEnvironment
			.sorted { $0.key < $1.key }
			.map { "\($0.key)=\($0.value)" }
		let process: [String: Any] = [
			"terminal": terminal,
			"args": proof.defaultCommand,
			"env": environment,
			"cwd": proof.defaultWorkingDirectory,
			"user": [
				"uid": proof.defaultUserID,
				"gid": proof.defaultGroupID
			]
		]
		let document: [String: Any] = [
			"ociVersion": "1.1.0",
			"process": process,
			"root": [
				"path": rootPath
			]
		]
		let data = try JSONSerialization.data(
			withJSONObject: document,
			options: [.prettyPrinted, .sortedKeys]
		)
		try data.write(to: bundleRoot.appendingPathComponent("config.json"))
	}

	private func descriptor(
		environmentID: String,
		source: OrlixEnvironmentSource,
        rootImageIdentifier: String
    ) -> OrlixEnvironmentDescriptor {
        OrlixEnvironmentDescriptor(
            id: environmentID,
            source: source,
            platform: "linux/arm64",
            rootImageIdentifier: rootImageIdentifier,
            defaultCommand: proof.defaultCommand,
            defaultEnvironment: proof.defaultEnvironment,
            defaultWorkingDirectory: proof.defaultWorkingDirectory,
            defaultUserID: proof.defaultUserID,
            defaultGroupID: proof.defaultGroupID,
            maskedPaths: proof.maskedPaths,
            readonlyPaths: proof.readonlyPaths,
            cgroupsPath: proof.cgroupsPath,
            cgroupPidsLimit: proof.cgroupPidsLimit,
            timeOffsets: proof.timeOffsets,
            uidMappings: proof.uidMappings,
            gidMappings: proof.gidMappings,
            namespaces: proof.namespaces
        )
    }

    private static func mutableFixtureCopy(
        of sourceRoot: EnvironmentRootFixture,
        fixture: RuntimeFixture
    ) throws -> EnvironmentRootFixture {
        let root = FileManager.default.temporaryDirectory
            .appendingPathComponent(
                "orlix-\(fixture.directoryName)-\(UUID().uuidString)",
                isDirectory: true
            )
        try FileManager.default.copyItem(at: sourceRoot.root, to: root)
        return EnvironmentRootFixture(root: root)
    }

    private static func persistentCrossBootFixtureCopy(
        for fixture: RuntimeFixture
    ) throws -> EnvironmentRootFixture {
        let root = try persistentCrossBootFixtureRoot(for: fixture)
        guard FileManager.default.fileExists(atPath: root.path) else {
            throw XCTSkip(
                "missing persistent \(fixture.description) cross-boot fixture: \(root.path)"
            )
        }
        return EnvironmentRootFixture(root: root)
    }

    private static func preparePersistentCrossBootFixtureCopy(
        source sourceRoot: EnvironmentRootFixture,
        fixture: RuntimeFixture
    ) throws -> EnvironmentRootFixture {
        let root = try persistentCrossBootFixtureRoot(for: fixture)
        if FileManager.default.fileExists(atPath: root.path) {
            try FileManager.default.removeItem(at: root)
        }
        try FileManager.default.createDirectory(
            at: root.deletingLastPathComponent(),
            withIntermediateDirectories: true
        )
        try FileManager.default.copyItem(at: sourceRoot.root, to: root)
        return EnvironmentRootFixture(root: root)
    }

    private static func persistentCrossBootFixtureRoot(
        for fixture: RuntimeFixture
    ) throws -> URL {
        try FileManager.default.url(
            for: .applicationSupportDirectory,
            in: .userDomainMask,
            appropriateFor: nil,
            create: true
        )
        .appendingPathComponent("OrlixEnvironmentCrossBoot", isDirectory: true)
        .appendingPathComponent(fixture.directoryName, isDirectory: true)
    }

    private static func fixtureRoot(
        for fixture: RuntimeFixture
    ) throws -> EnvironmentRootFixture {
        let repoRoot = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent()
            .deletingLastPathComponent()
            .deletingLastPathComponent()
            .deletingLastPathComponent()
            .deletingLastPathComponent()
        let environment = ProcessInfo.processInfo.environment
        let buildRoot = environment["ORLIX_BUILD_ROOT"].flatMap {
            $0.isEmpty ? nil : URL(fileURLWithPath: $0, isDirectory: true)
        }
        let repoBuildRoot = repoRoot.appendingPathComponent("Build", isDirectory: true)
        var candidates: [(root: URL, buildRoot: URL?)] = []

        if let override = environment["ORLIX_RUNTIME_FIXTURE_ROOT"],
           !override.isEmpty {
            let overrideRoot = URL(fileURLWithPath: override, isDirectory: true)
            let root = overrideRoot.lastPathComponent == fixture.directoryName
                ? overrideRoot
                : overrideRoot.appendingPathComponent(fixture.directoryName, isDirectory: true)
            candidates.append((root, buildRoot ?? Self.inferredBuildRoot(from: root)))
        }

        let resourceBundles = [
            Bundle.main,
            Bundle(for: OrlixEnvironmentRootRuntimeTests.self),
        ]
        for bundle in resourceBundles {
            if let resourceRoot = bundle.resourceURL?
                .appendingPathComponent("EnvironmentRuntimeTestFixtures", isDirectory: true)
                .appendingPathComponent(fixture.directoryName, isDirectory: true) {
                candidates.append((resourceRoot, buildRoot))
            }
        }

        candidates.append((
            repoBuildRoot
                .appendingPathComponent("OrlixOS", isDirectory: true)
                .appendingPathComponent("environment-runtime-test-fixtures", isDirectory: true)
                .appendingPathComponent(fixture.directoryName, isDirectory: true),
            repoBuildRoot
        ))

        var checkedMarkers: [String] = []
        for candidate in candidates {
            let marker = candidate.root.appendingPathComponent(readyFile, isDirectory: false)
            checkedMarkers.append(marker.path)
            guard FileManager.default.fileExists(atPath: marker.path) else {
                continue
            }
            try verifyFixtureReadyFile(
                marker: marker,
                fixture: fixture,
                buildRoot: candidate.buildRoot
            )

            return EnvironmentRootFixture(root: candidate.root)
        }

        throw XCTSkip(
            "missing \(fixture.description) materialized root fixture; checked: \(checkedMarkers.joined(separator: ", "))"
        )
    }

    private static func inferredBuildRoot(from fixtureRoot: URL) -> URL? {
        let components = fixtureRoot.standardizedFileURL.pathComponents
        guard let buildIndex = components.lastIndex(of: "Build") else {
            return nil
        }
        return URL(
            fileURLWithPath: NSString.path(
                withComponents: Array(components.prefix(through: buildIndex))
            ),
            isDirectory: true
        )
    }

    private static func verifyFixtureReadyFile(
        marker: URL,
        fixture: RuntimeFixture,
        buildRoot: URL?
    ) throws {
        let ready = try parseReadyFile(marker)
        let expectedProfile = ProcessInfo.processInfo.environment["ORLIX_PROFILE"] ??
            ready["profile"] ??
            "release"

        guard ready["profile"] == expectedProfile else {
            throw OrlixEnvironmentRootRuntimeProofError.staleFixture(
                "fixture \(fixture.description) was not generated for \(expectedProfile) profile"
            )
        }
        guard ready["fixture"] == fixture.directoryName else {
            throw OrlixEnvironmentRootRuntimeProofError.staleFixture(
                "fixture marker does not match \(fixture.directoryName)"
            )
        }
        guard ready["environment"] == fixture.environmentID else {
            throw OrlixEnvironmentRootRuntimeProofError.staleFixture(
                "fixture marker does not match \(fixture.environmentID)"
            )
        }
        guard let buildRoot else {
            return
        }

        let initURL = buildRoot
            .appendingPathComponent("OrlixOS", isDirectory: true)
            .appendingPathComponent("packages", isDirectory: true)
            .appendingPathComponent(expectedProfile, isDirectory: true)
            .appendingPathComponent("sbin", isDirectory: true)
            .appendingPathComponent("init", isDirectory: false)
        guard FileManager.default.fileExists(atPath: initURL.path) else {
            throw OrlixEnvironmentRootRuntimeProofError.staleFixture(
                "missing packaged init used to verify fixture freshness: \(initURL.path)"
            )
        }

        let expectedHash = try sha256Hex(of: initURL)
        guard ready["init_sha256"] == expectedHash else {
            throw OrlixEnvironmentRootRuntimeProofError.staleFixture(
                "fixture \(fixture.description) was generated from a stale init"
            )
        }
    }

    private static func parseReadyFile(_ marker: URL) throws -> [String: String] {
        let contents = try String(contentsOf: marker, encoding: .utf8)
        return contents.split(separator: "\n").reduce(into: [:]) { result, line in
            let parts = line.split(separator: "=", maxSplits: 1)
            guard parts.count == 2 else {
                return
            }
            result[String(parts[0])] = String(parts[1])
        }
    }

    private static func sha256Hex(of url: URL) throws -> String {
        let digest = SHA256.hash(data: try Data(contentsOf: url))
        return digest.map { String(format: "%02x", $0) }.joined()
    }

    private static func containsTerminalCondition(
        _ rawOutput: String,
        proof: RuntimeProof
    ) -> Bool {
        let output = normalized(rawOutput)
        let proofCompleted = output.contains(proof.doneMarker) &&
            (!proof.requiresProcessExitStatus ||
                output.range(
                    of: #"(?m)orlix-init: process exited pid=[0-9]+ status=0"#,
                    options: .regularExpression
                ) != nil)

        return proofCompleted ||
            firstFatalMarker(in: output) != nil
    }

    private func validate(
        _ output: String,
        terminalLog: EnvironmentRootTerminalLog
    ) throws {
        if let marker = firstFatalMarker(in: output) {
            throw OrlixEnvironmentRootRuntimeProofError.fatalMarker(marker)
        }
        for marker in proof.requiredMarkers(for: fixture) {
            guard output.contains(marker) else {
                throw OrlixEnvironmentRootRuntimeProofError.missingMarker(
                    marker,
                    terminalLog.url
                )
            }
        }
        for markerGroup in proof.requiredOrderedMarkerGroups(for: fixture) {
            guard Self.outputContainsMarkersInOrder(output, markerGroup) else {
                throw OrlixEnvironmentRootRuntimeProofError.missingMarker(
                    markerGroup.joined(separator: " ... "),
                    terminalLog.url
                )
            }
        }
    }

    private static func outputContainsMarkersInOrder(
        _ output: String,
        _ markers: [String]
    ) -> Bool {
        var searchStart = output.startIndex
        for marker in markers {
            guard let range = output.range(
                of: marker,
                range: searchStart..<output.endIndex
            ) else {
                return false
            }
            searchStart = range.upperBound
        }
        return true
    }

    private static func containsShellPrompt(_ rawOutput: String) -> Bool {
        let tail = String(normalized(rawOutput).suffix(4096))

        return tail.contains("sh-5.3# ") ||
            tail.range(
                of: #"[A-Za-z_-]*sh-[0-9][^\n#]*# "#,
                options: .regularExpression
            ) != nil
    }

    private static func normalized(_ text: String) -> String {
        stripANSI(
            text.replacingOccurrences(of: "\r\n", with: "\n")
                .replacingOccurrences(of: "\r", with: "\n")
        )
    }

    private static func stripANSI(_ text: String) -> String {
        var output = ""
        var scalars = text.unicodeScalars.makeIterator()

        while let scalar = scalars.next() {
            if scalar.value != 0x1b {
                output.unicodeScalars.append(scalar)
                continue
            }

            guard let introducer = scalars.next() else {
                break
            }
            if introducer.value != 0x5b {
                continue
            }

            while let sequenceScalar = scalars.next() {
                if sequenceScalar.value >= 0x40 &&
                    sequenceScalar.value <= 0x7e {
                    break
                }
            }
        }

        return output
    }

    private static func firstFatalMarker(in output: String) -> String? {
        [
            "Kernel panic",
            "kernel panic",
            "panic:",
            "Out of memory",
            "oom-kill",
            "Killed process",
            "orlix-init: open PTY master failed",
            "orlix-init: unlock PTY failed",
            "orlix-init: get PTY number failed",
            "orlix-init: shell TIOCSCTTY failed",
            "orlix-init: PTY shell session ended",
            "VFS: Cannot open root device",
            "No working init found",
            "ORLIX_ENV_OVERLAY_PROOF_FAILED_BASE_READ",
            "ORLIX_ENV_OVERLAY_PROOF_FAILED_COPYUP_WRITE",
            "ORLIX_ENV_OVERLAY_PROOF_FAILED_COPYUP_READ",
            "ORLIX_ENV_OVERLAY_PROOF_FAILED_UNLINK",
            "ORLIX_ENV_PSEUDOFS_PROOF_FAILED_PROC_DIR",
            "ORLIX_ENV_PSEUDOFS_PROOF_FAILED_PROC_MOUNTS",
            "ORLIX_ENV_PSEUDOFS_PROOF_FAILED_PROC_SELF_STATUS",
            "ORLIX_ENV_PSEUDOFS_PROOF_FAILED_PROC_SELF_FD",
            "ORLIX_ENV_PSEUDOFS_PROOF_FAILED_DEV_DIR",
            "ORLIX_ENV_PSEUDOFS_PROOF_FAILED_DEV_NULL",
            "ORLIX_ENV_PSEUDOFS_PROOF_FAILED_DEV_URANDOM",
            "ORLIX_ENV_PSEUDOFS_PROOF_FAILED_DEV_TTY",
            "ORLIX_ENV_PSEUDOFS_PROOF_FAILED_DEV_PTMX",
            "ORLIX_ENV_PSEUDOFS_PROOF_FAILED_DEV_PTS",
            "ORLIX_ENV_PSEUDOFS_PROOF_FAILED_SYS_DIR",
            "ORLIX_ENV_PSEUDOFS_PROOF_FAILED_SYS_BLOCK_VDA",
            "ORLIX_ENV_PSEUDOFS_PROOF_FAILED_SYS_BLOCK_VDB",
            "ORLIX_ENV_PTY_PROOF_FAILED_STDIN",
            "ORLIX_ENV_PTY_PROOF_FAILED_STDOUT",
			"ORLIX_ENV_PTY_PROOF_FAILED_STDERR",
			"ORLIX_ENV_PTY_PROOF_FAILED_TTY_COMMAND",
			"ORLIX_ENV_PTY_PROOF_FAILED_PATH",
			"ORLIX_ENV_STDIO_PROOF_FAILED_PTY",
			"ORLIX_ENV_TMPFS_PROOF_FAILED_TMP_MOUNT",
            "ORLIX_ENV_TMPFS_PROOF_FAILED_RUN_MOUNT",
            "ORLIX_ENV_TMPFS_PROOF_FAILED_DEV_SHM_MOUNT",
        "ORLIX_ENV_TMPFS_PROOF_FAILED_TMP_WRITE",
        "ORLIX_ENV_TMPFS_PROOF_FAILED_RUN_WRITE",
        "ORLIX_ENV_TMPFS_PROOF_FAILED_DEV_SHM_WRITE",
        "ORLIX_ENV_USERNS_PROOF_FAILED_UID_MAP",
        "ORLIX_ENV_USERNS_PROOF_FAILED_GID_MAP",
        "ORLIX_ENV_USERNS_PROOF_FAILED_SETGROUPS",
        "ORLIX_ENV_TIMENS_PROOF_FAILED_MONOTONIC",
        "ORLIX_ENV_TIMENS_PROOF_FAILED_BOOTTIME",
        "ORLIX_ENV_PATHS_PROOF_FAILED_MASKED_FILE",
        "ORLIX_ENV_PATHS_PROOF_FAILED_READONLY_DIR",
        "ORLIX_ENV_CGROUP_PROOF_FAILED_PATH",
        "ORLIX_ENV_CGROUP_PROOF_FAILED_PIDS_MAX",
        "ORLIX_ENV_CGROUP_PROOF_FAILED_PROCS",
        "ORLIX_ENV_CROSSBOOT_PROOF_FAILED_WRITE",
        "ORLIX_ENV_CROSSBOOT_PROOF_FAILED_SYNC",
        "ORLIX_ENV_CROSSBOOT_PROOF_FAILED_REREAD",
        "ORLIX_ENV_CROSSBOOT_PROOF_FAILED_VERIFY",
        "ORLIX_ENV_CROSSBOOT_PROOF_FAILED_CLEANUP",
        "ORLIX_ENV_COREUTILS_PROOF_FAILED_EXIT_STATUS",
        ].first { output.contains($0) }
    }

    private func firstFatalMarker(in output: String) -> String? {
        let oneShotCompleted = !proof.sendsInteractiveCommands &&
            output.contains(proof.doneMarker)
        let marker = Self.firstFatalMarker(in: output)

        if oneShotCompleted &&
            marker == "orlix-init: PTY shell session ended" {
            return nil
        }
        return marker
    }
}

private enum RuntimeProof: Sendable {
    case osRelease
    case overlayMutation
    case descriptorExecution
    case longDescriptorExecution
	case linuxPathDescriptorExecution
	case pathLookupDescriptorExecution
	case pathLookupWithoutPATHDescriptorExecution
	case stdioExecution
	case pseudoFilesystems
    case ptyStdio
    case runtimeTmpfs
    case userNamespaceMappings
    case timeNamespaceOffsets
    case maskedReadonlyPaths
    case cgroupPidsLimit
    case crossBootWrite
    case crossBootVerify
    case coreutilsCommand

    var defaultCommand: [String] {
        switch self {
        case .osRelease, .overlayMutation, .pseudoFilesystems, .ptyStdio,
             .runtimeTmpfs, .crossBootWrite, .crossBootVerify:
            return ["/bin/sh"]
        case .descriptorExecution, .longDescriptorExecution:
            return [
                "/bin/sh",
                "-c",
                descriptorExecutionScript,
                descriptorExecutionArgument0,
                "argument with spaces"
            ]
        case .linuxPathDescriptorExecution:
            return [
                "/bin/../bin/sh",
                "-c",
                descriptorExecutionScript,
                "linux-path-argv0",
                "",
                "argument after empty"
            ]
        case .pathLookupDescriptorExecution:
            return [
                "sh",
                "-c",
                descriptorExecutionScript,
                "path-lookup-argv0",
                "argument after path lookup"
            ]
		case .pathLookupWithoutPATHDescriptorExecution:
			return [
				"sh",
				"-c",
				descriptorExecutionScript,
				"path-fallback-argv0",
				"argument after fallback path lookup"
			]
		case .stdioExecution:
			return [
				"/bin/sh",
				"-c",
				Self.stdioExecutionScript
			]
        case .coreutilsCommand:
            return [
                "/bin/sh",
                "-c",
                Self.coreutilsCommandScript
            ]
		case .userNamespaceMappings:
			return [
				"/bin/sh",
                "-c",
                Self.userNamespaceMappingScript
            ]
        case .timeNamespaceOffsets:
            return [
                "/bin/sh",
                "-c",
                Self.timeNamespaceOffsetScript
            ]
        case .maskedReadonlyPaths:
            return [
                "/bin/sh",
                "-c",
                Self.maskedReadonlyPathScript
            ]
        case .cgroupPidsLimit:
            return [
                "/bin/sh",
                "-c",
                Self.cgroupPidsLimitScript
            ]
        }
    }

    var defaultEnvironment: [String: String] {
        if self == .pathLookupWithoutPATHDescriptorExecution {
            return [
                "HOME": "/root",
                "ORLIX_DESCRIPTOR_MESSAGE": "descriptor value without path",
                "TERM": "xterm-256color"
            ]
        }

        var environment = [
            "HOME": "/root",
            "PATH": "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
            "TERM": "xterm-256color"
        ]
        if self == .descriptorExecution ||
            self == .longDescriptorExecution ||
            self == .linuxPathDescriptorExecution ||
            self == .pathLookupDescriptorExecution {
            environment["ORLIX_DESCRIPTOR_MESSAGE"] = "descriptor value with spaces"
        }
        return environment
    }

    var defaultWorkingDirectory: String {
        switch self {
		case .osRelease, .overlayMutation, .pseudoFilesystems, .ptyStdio,
			.stdioExecution,
			.runtimeTmpfs, .userNamespaceMappings, .timeNamespaceOffsets,
			.maskedReadonlyPaths, .cgroupPidsLimit, .crossBootWrite,
			.crossBootVerify, .coreutilsCommand:
            return "/"
        case .descriptorExecution, .longDescriptorExecution:
            return "/tmp"
        case .linuxPathDescriptorExecution, .pathLookupDescriptorExecution,
             .pathLookupWithoutPATHDescriptorExecution:
            return "/tmp/.."
        }
    }

    var defaultUserID: UInt32 {
        switch self {
		case .osRelease, .overlayMutation, .linuxPathDescriptorExecution,
			.pathLookupDescriptorExecution,
			.pathLookupWithoutPATHDescriptorExecution, .pseudoFilesystems,
			.ptyStdio, .stdioExecution, .runtimeTmpfs, .userNamespaceMappings,
			.timeNamespaceOffsets, .maskedReadonlyPaths, .cgroupPidsLimit,
			.crossBootWrite, .crossBootVerify, .coreutilsCommand:
            return 0
        case .descriptorExecution, .longDescriptorExecution:
            return 1000
        }
    }

    var defaultGroupID: UInt32 {
        switch self {
		case .osRelease, .overlayMutation, .linuxPathDescriptorExecution,
			.pathLookupDescriptorExecution,
			.pathLookupWithoutPATHDescriptorExecution, .pseudoFilesystems,
			.ptyStdio, .stdioExecution, .runtimeTmpfs, .userNamespaceMappings,
			.timeNamespaceOffsets, .maskedReadonlyPaths, .cgroupPidsLimit,
			.crossBootWrite, .crossBootVerify, .coreutilsCommand:
            return 0
        case .descriptorExecution, .longDescriptorExecution:
            return 100
        }
    }

    var namespaces: [String] {
        switch self {
        case .userNamespaceMappings:
            return ["user"]
        case .timeNamespaceOffsets:
            return ["time"]
        case .osRelease, .overlayMutation, .descriptorExecution,
             .longDescriptorExecution, .linuxPathDescriptorExecution,
             .pathLookupDescriptorExecution,
			.pathLookupWithoutPATHDescriptorExecution, .pseudoFilesystems,
			.ptyStdio, .stdioExecution, .runtimeTmpfs, .maskedReadonlyPaths,
             .cgroupPidsLimit,
             .crossBootWrite, .crossBootVerify, .coreutilsCommand:
            return []
        }
    }

    var uidMappings: [OrlixEnvironmentIDMapping] {
        switch self {
        case .userNamespaceMappings:
            return [OrlixEnvironmentIDMapping(containerID: 0, hostID: 0, size: 1)]
        case .osRelease, .overlayMutation, .descriptorExecution,
             .longDescriptorExecution, .linuxPathDescriptorExecution,
             .pathLookupDescriptorExecution,
			.pathLookupWithoutPATHDescriptorExecution, .pseudoFilesystems,
			.ptyStdio, .stdioExecution, .runtimeTmpfs, .timeNamespaceOffsets,
             .maskedReadonlyPaths, .cgroupPidsLimit,
             .crossBootWrite, .crossBootVerify, .coreutilsCommand:
            return []
        }
    }

    var maskedPaths: [String] {
        switch self {
        case .maskedReadonlyPaths:
            return ["/etc/os-release"]
        case .osRelease, .overlayMutation, .descriptorExecution,
             .longDescriptorExecution, .linuxPathDescriptorExecution,
             .pathLookupDescriptorExecution,
			.pathLookupWithoutPATHDescriptorExecution, .pseudoFilesystems,
			.ptyStdio, .stdioExecution, .runtimeTmpfs, .userNamespaceMappings,
             .timeNamespaceOffsets, .cgroupPidsLimit,
             .crossBootWrite, .crossBootVerify, .coreutilsCommand:
            return []
        }
    }

    var readonlyPaths: [String] {
        switch self {
        case .maskedReadonlyPaths:
            return ["/etc"]
        case .osRelease, .overlayMutation, .descriptorExecution,
             .longDescriptorExecution, .linuxPathDescriptorExecution,
             .pathLookupDescriptorExecution,
			.pathLookupWithoutPATHDescriptorExecution, .pseudoFilesystems,
			.ptyStdio, .stdioExecution, .runtimeTmpfs, .userNamespaceMappings,
             .timeNamespaceOffsets, .cgroupPidsLimit,
             .crossBootWrite, .crossBootVerify, .coreutilsCommand:
            return []
        }
    }

    var cgroupsPath: String? {
        switch self {
        case .cgroupPidsLimit:
            return "/orlix/runtime-proof"
        case .osRelease, .overlayMutation, .descriptorExecution,
             .longDescriptorExecution, .linuxPathDescriptorExecution,
             .pathLookupDescriptorExecution,
             .pathLookupWithoutPATHDescriptorExecution, .pseudoFilesystems,
 .ptyStdio, .stdioExecution, .runtimeTmpfs, .userNamespaceMappings,
             .timeNamespaceOffsets, .maskedReadonlyPaths,
             .crossBootWrite, .crossBootVerify, .coreutilsCommand:
            return nil
        }
    }

    var cgroupPidsLimit: Int64? {
        switch self {
        case .cgroupPidsLimit:
            return 32
        case .osRelease, .overlayMutation, .descriptorExecution,
             .longDescriptorExecution, .linuxPathDescriptorExecution,
             .pathLookupDescriptorExecution,
             .pathLookupWithoutPATHDescriptorExecution, .pseudoFilesystems,
 .ptyStdio, .stdioExecution, .runtimeTmpfs, .userNamespaceMappings,
             .timeNamespaceOffsets, .maskedReadonlyPaths,
             .crossBootWrite, .crossBootVerify, .coreutilsCommand:
            return nil
        }
    }

    var timeOffsets: [OrlixEnvironmentTimeOffset] {
        switch self {
        case .timeNamespaceOffsets:
            return [
                OrlixEnvironmentTimeOffset(clock: "monotonic", secs: 12, nanosecs: 500000000),
                OrlixEnvironmentTimeOffset(clock: "boottime", secs: 3, nanosecs: 250)
            ]
        case .osRelease, .overlayMutation, .descriptorExecution,
             .longDescriptorExecution, .linuxPathDescriptorExecution,
             .pathLookupDescriptorExecution,
             .pathLookupWithoutPATHDescriptorExecution, .pseudoFilesystems,
 .ptyStdio, .stdioExecution, .runtimeTmpfs, .userNamespaceMappings,
             .maskedReadonlyPaths, .cgroupPidsLimit,
             .crossBootWrite, .crossBootVerify, .coreutilsCommand:
            return []
        }
    }

    var gidMappings: [OrlixEnvironmentIDMapping] {
        switch self {
        case .userNamespaceMappings:
            return [OrlixEnvironmentIDMapping(containerID: 0, hostID: 0, size: 1)]
        case .osRelease, .overlayMutation, .descriptorExecution,
             .longDescriptorExecution, .linuxPathDescriptorExecution,
             .pathLookupDescriptorExecution,
             .pathLookupWithoutPATHDescriptorExecution, .pseudoFilesystems,
 .ptyStdio, .stdioExecution, .runtimeTmpfs, .timeNamespaceOffsets,
             .maskedReadonlyPaths, .cgroupPidsLimit,
             .crossBootWrite, .crossBootVerify, .coreutilsCommand:
            return []
        }
    }

    var sendsInteractiveCommands: Bool {
        switch self {
        case .osRelease, .overlayMutation, .pseudoFilesystems, .ptyStdio,
             .runtimeTmpfs, .crossBootWrite, .crossBootVerify:
            return true
		case .descriptorExecution, .longDescriptorExecution,
			.linuxPathDescriptorExecution, .pathLookupDescriptorExecution,
			.pathLookupWithoutPATHDescriptorExecution, .userNamespaceMappings,
			.timeNamespaceOffsets, .maskedReadonlyPaths, .cgroupPidsLimit,
			.stdioExecution, .coreutilsCommand:
            return false
        }
    }

    var requiresProcessExitStatus: Bool {
        switch self {
        case .coreutilsCommand:
            return true
        case .osRelease, .overlayMutation, .descriptorExecution,
             .longDescriptorExecution, .linuxPathDescriptorExecution,
             .pathLookupDescriptorExecution,
             .pathLookupWithoutPATHDescriptorExecution, .pseudoFilesystems,
             .ptyStdio, .stdioExecution, .runtimeTmpfs, .userNamespaceMappings,
             .timeNamespaceOffsets, .maskedReadonlyPaths, .cgroupPidsLimit,
             .crossBootWrite, .crossBootVerify:
            return false
        }
    }

    var commandScript: String {
        switch self {
        case .osRelease:
            return OrlixEnvironmentRootRuntimeProofRunner.osReleaseCommandScript
        case .overlayMutation:
            return [
                #"printf '%s%s\n' ORLIX_ENV_ OVERLAY_MUTATION_BEGIN"#,
                #"/bin/cat /proc/mounts"#,
                #"if ! /bin/cat /etc/os-release; then printf '%s%s\n' ORLIX_ENV_OVERLAY_ PROOF_FAILED_BASE_READ; fi"#,
                #"if printf '%s\n' ID=orlix-overlay-copyup-proof > /etc/os-release; then printf '%s%s\n' ORLIX_ENV_ OVERLAY_COPYUP_WRITE_OK; else printf '%s%s\n' ORLIX_ENV_OVERLAY_ PROOF_FAILED_COPYUP_WRITE; fi"#,
                #"if /bin/cat /etc/os-release; then printf '%s%s\n' ORLIX_ENV_ OVERLAY_COPYUP_OK; else printf '%s%s\n' ORLIX_ENV_OVERLAY_ PROOF_FAILED_COPYUP_READ; fi"#,
                #"/bin/rm /etc/os-release"#,
                #"if /bin/test ! -e /etc/os-release; then printf '%s%s\n' ORLIX_ENV_ OVERLAY_UNLINK_OK; else printf '%s%s\n' ORLIX_ENV_OVERLAY_ PROOF_FAILED_UNLINK; fi"#,
                #"/bin/sync"#,
                #"printf '%s%s\n' ORLIX_ENV_ OVERLAY_MUTATION_DONE"#,
            ].joined(separator: "\r") + "\r"
        case .pseudoFilesystems:
            return [
                #"printf '%s%s\n' ORLIX_ENV_ PSEUDOFS_BEGIN"#,
                #"if /bin/test -d /proc; then printf '%s%s\n' ORLIX_ENV_ PROC_DIR_OK; else printf '%s%s\n' ORLIX_ENV_PSEUDOFS_ PROOF_FAILED_PROC_DIR; fi"#,
                #"if /bin/test -r /proc/mounts; then printf '%s%s\n' ORLIX_ENV_ PROC_MOUNTS_OK; else printf '%s%s\n' ORLIX_ENV_PSEUDOFS_ PROOF_FAILED_PROC_MOUNTS; fi"#,
                #"if /bin/test -r /proc/self/status; then printf '%s%s\n' ORLIX_ENV_ PROC_SELF_STATUS_OK; else printf '%s%s\n' ORLIX_ENV_PSEUDOFS_ PROOF_FAILED_PROC_SELF_STATUS; fi"#,
                #"if /bin/test -d /proc/self/fd; then printf '%s%s\n' ORLIX_ENV_ PROC_SELF_FD_OK; else printf '%s%s\n' ORLIX_ENV_PSEUDOFS_ PROOF_FAILED_PROC_SELF_FD; fi"#,
                #"if /bin/test -d /dev; then printf '%s%s\n' ORLIX_ENV_ DEV_DIR_OK; else printf '%s%s\n' ORLIX_ENV_PSEUDOFS_ PROOF_FAILED_DEV_DIR; fi"#,
                #"if /bin/test -c /dev/null; then printf '%s%s\n' ORLIX_ENV_ DEV_NULL_OK; else printf '%s%s\n' ORLIX_ENV_PSEUDOFS_ PROOF_FAILED_DEV_NULL; fi"#,
                #"if /bin/test -c /dev/urandom; then printf '%s%s\n' ORLIX_ENV_ DEV_URANDOM_OK; else printf '%s%s\n' ORLIX_ENV_PSEUDOFS_ PROOF_FAILED_DEV_URANDOM; fi"#,
                #"if /bin/test -c /dev/tty; then printf '%s%s\n' ORLIX_ENV_ DEV_TTY_OK; else printf '%s%s\n' ORLIX_ENV_PSEUDOFS_ PROOF_FAILED_DEV_TTY; fi"#,
                #"if /bin/test -e /dev/ptmx; then printf '%s%s\n' ORLIX_ENV_ DEV_PTMX_OK; else printf '%s%s\n' ORLIX_ENV_PSEUDOFS_ PROOF_FAILED_DEV_PTMX; fi"#,
                #"if /bin/test -d /dev/pts; then printf '%s%s\n' ORLIX_ENV_ DEV_PTS_OK; else printf '%s%s\n' ORLIX_ENV_PSEUDOFS_ PROOF_FAILED_DEV_PTS; fi"#,
                #"if /bin/test -d /sys; then printf '%s%s\n' ORLIX_ENV_ SYS_DIR_OK; else printf '%s%s\n' ORLIX_ENV_PSEUDOFS_ PROOF_FAILED_SYS_DIR; fi"#,
                #"if /bin/test -r /sys/block/vda/ro; then printf '%s%s\n' ORLIX_ENV_ SYS_BLOCK_VDA_OK; else printf '%s%s\n' ORLIX_ENV_PSEUDOFS_ PROOF_FAILED_SYS_BLOCK_VDA; fi"#,
                #"if /bin/test -r /sys/block/vdb/ro; then printf '%s%s\n' ORLIX_ENV_ SYS_BLOCK_VDB_OK; else printf '%s%s\n' ORLIX_ENV_PSEUDOFS_ PROOF_FAILED_SYS_BLOCK_VDB; fi"#,
                #"/bin/cat /proc/self/status"#,
                #"/bin/cat /proc/mounts"#,
                #"printf '%s%s\n' ORLIX_ENV_ PSEUDOFS_DONE"#,
            ].joined(separator: "\r") + "\r"
        case .ptyStdio:
            return [
                #"printf '%s%s\n' ORLIX_ENV_ PTY_BEGIN"#,
                #"if /bin/test -t 0; then printf '%s%s\n' ORLIX_ENV_ PTY_STDIN_OK; else printf '%s%s\n' ORLIX_ENV_PTY_ PROOF_FAILED_STDIN; fi"#,
                #"if /bin/test -t 1; then printf '%s%s\n' ORLIX_ENV_ PTY_STDOUT_OK; else printf '%s%s\n' ORLIX_ENV_PTY_ PROOF_FAILED_STDOUT; fi"#,
                #"if /bin/test -t 2; then printf '%s%s\n' ORLIX_ENV_ PTY_STDERR_OK; else printf '%s%s\n' ORLIX_ENV_PTY_ PROOF_FAILED_STDERR; fi"#,
                #"if command -v tty >/dev/null 2>&1; then tty_path=$(tty); elif /bin/test -x /bin/tty; then tty_path=$(/bin/tty); elif /bin/test -x /usr/bin/tty; then tty_path=$(/usr/bin/tty); else tty_path=missing-tty-command; fi"#,
                #"printf '%s\n' "$tty_path""#,
                #"case "$tty_path" in /dev/pts/*) printf '%s%s\n' ORLIX_ENV_ PTY_PATH_OK;; missing-tty-command) printf '%s%s\n' ORLIX_ENV_PTY_ PROOF_FAILED_TTY_COMMAND;; *) printf '%s%s\n' ORLIX_ENV_PTY_ PROOF_FAILED_PATH;; esac"#,
                #"printf '%s%s\n' ORLIX_ENV_ PTY_WAITING_FOR_INPUT; IFS= read -r pty_delayed_input; if [ "$pty_delayed_input" = "orlix-pty-delayed-input" ]; then printf '%s%s\n' ORLIX_ENV_ PTY_DELAYED_INPUT_OK; else printf '%s%s\n' ORLIX_ENV_PTY_ PROOF_FAILED_DELAYED_INPUT; fi"#,
            ].joined(separator: "\r") + "\r"
        case .runtimeTmpfs:
            return [
                #"printf '%s%s\n' ORLIX_ENV_ TMPFS_BEGIN"#,
                #"mounts=$(/bin/cat /proc/mounts)"#,
                #"case "$mounts" in *" /tmp tmpfs "*) printf '%s%s\n' ORLIX_ENV_ TMP_MOUNT_OK;; *) printf '%s%s\n' ORLIX_ENV_TMPFS_ PROOF_FAILED_TMP_MOUNT;; esac"#,
                #"case "$mounts" in *" /run tmpfs "*) printf '%s%s\n' ORLIX_ENV_ RUN_MOUNT_OK;; *) printf '%s%s\n' ORLIX_ENV_TMPFS_ PROOF_FAILED_RUN_MOUNT;; esac"#,
                #"case "$mounts" in *" /dev/shm tmpfs "*) printf '%s%s\n' ORLIX_ENV_ DEV_SHM_MOUNT_OK;; *) printf '%s%s\n' ORLIX_ENV_TMPFS_ PROOF_FAILED_DEV_SHM_MOUNT;; esac"#,
                #"if printf '%s\n' tmpfs-proof > /tmp/orlix-tmpfs-proof; then printf '%s%s\n' ORLIX_ENV_ TMP_WRITE_OK; else printf '%s%s\n' ORLIX_ENV_TMPFS_ PROOF_FAILED_TMP_WRITE; fi"#,
                #"if printf '%s\n' run-proof > /run/orlix-tmpfs-proof; then printf '%s%s\n' ORLIX_ENV_ RUN_WRITE_OK; else printf '%s%s\n' ORLIX_ENV_TMPFS_ PROOF_FAILED_RUN_WRITE; fi"#,
                #"if printf '%s\n' shm-proof > /dev/shm/orlix-tmpfs-proof; then printf '%s%s\n' ORLIX_ENV_ DEV_SHM_WRITE_OK; else printf '%s%s\n' ORLIX_ENV_TMPFS_ PROOF_FAILED_DEV_SHM_WRITE; fi"#,
                #"/bin/cat /proc/mounts"#,
                #"printf '%s%s\n' ORLIX_ENV_ TMPFS_DONE"#,
            ].joined(separator: "\r") + "\r"
        case .crossBootWrite:
            return [
                #"printf '%s%s\n' ORLIX_ENV_ CROSSBOOT_WRITE_BEGIN"#,
                #"if printf '%s\n' orlix-imported-crossboot-ok > /etc/orlix-crossboot-marker; then printf '%s%s\n' ORLIX_ENV_ CROSSBOOT_WRITE_OK; else printf '%s%s\n' ORLIX_ENV_CROSSBOOT_ PROOF_FAILED_WRITE; fi"#,
                #"if /bin/sync; then printf '%s%s\n' ORLIX_ENV_ CROSSBOOT_SYNC_OK; else printf '%s%s\n' ORLIX_ENV_CROSSBOOT_ PROOF_FAILED_SYNC; fi"#,
                #"marker=$(/bin/cat /etc/orlix-crossboot-marker 2>/dev/null)"#,
                #"if [ "$marker" = "orlix-imported-crossboot-ok" ]; then printf '%s%s\n' ORLIX_ENV_ CROSSBOOT_REREAD_OK; else printf '%s%s\n' ORLIX_ENV_CROSSBOOT_ PROOF_FAILED_REREAD; fi"#,
                #"printf '%s%s\n' ORLIX_ENV_ CROSSBOOT_WRITE_DONE"#,
            ].joined(separator: "\r") + "\r"
        case .crossBootVerify:
            return [
                #"printf '%s%s\n' ORLIX_ENV_ CROSSBOOT_VERIFY_BEGIN"#,
                #"marker=$(/bin/cat /etc/orlix-crossboot-marker 2>/dev/null)"#,
                #"if [ "$marker" = "orlix-imported-crossboot-ok" ]; then printf '%s%s\n' ORLIX_ENV_ CROSSBOOT_SURVIVED_OK; else printf '%s%s\n' ORLIX_ENV_CROSSBOOT_ PROOF_FAILED_VERIFY; fi"#,
                #"if /bin/rm /etc/orlix-crossboot-marker && /bin/sync; then printf '%s%s\n' ORLIX_ENV_ CROSSBOOT_CLEANUP_OK; else printf '%s%s\n' ORLIX_ENV_CROSSBOOT_ PROOF_FAILED_CLEANUP; fi"#,
                #"printf '%s%s\n' ORLIX_ENV_ CROSSBOOT_VERIFY_DONE"#,
            ].joined(separator: "\r") + "\r"
        case .coreutilsCommand:
            return ""
		case .descriptorExecution, .longDescriptorExecution,
			.linuxPathDescriptorExecution, .pathLookupDescriptorExecution,
			.pathLookupWithoutPATHDescriptorExecution, .userNamespaceMappings,
			.timeNamespaceOffsets, .maskedReadonlyPaths, .cgroupPidsLimit,
			.stdioExecution:
            return ""
        }
    }

    var doneMarker: String {
        switch self {
        case .osRelease:
            return "ORLIX_ENV_OS_RELEASE_DONE"
        case .overlayMutation:
            return "ORLIX_ENV_OVERLAY_MUTATION_DONE"
        case .descriptorExecution, .longDescriptorExecution,
             .linuxPathDescriptorExecution, .pathLookupDescriptorExecution,
             .pathLookupWithoutPATHDescriptorExecution:
            return "ORLIX_ENV_EXEC_DONE"
        case .pseudoFilesystems:
            return "ORLIX_ENV_PSEUDOFS_DONE"
		case .ptyStdio:
			return "ORLIX_ENV_PTY_DONE"
		case .stdioExecution:
			return "ORLIX_ENV_STDIO_DONE"
		case .runtimeTmpfs:
			return "ORLIX_ENV_TMPFS_DONE"
        case .userNamespaceMappings:
            return "ORLIX_ENV_USERNS_DONE"
        case .timeNamespaceOffsets:
            return "ORLIX_ENV_TIMENS_DONE"
        case .maskedReadonlyPaths:
            return "ORLIX_ENV_PATHS_DONE"
        case .cgroupPidsLimit:
            return "ORLIX_ENV_CGROUP_DONE"
        case .crossBootWrite:
            return "ORLIX_ENV_CROSSBOOT_WRITE_DONE"
        case .crossBootVerify:
            return "ORLIX_ENV_CROSSBOOT_VERIFY_DONE"
        case .coreutilsCommand:
            return "ORLIX_ENV_COREUTILS_COMMAND_DONE"
        }
    }

    var delayedInputPrompt: String {
        switch self {
        case .ptyStdio:
            return "ORLIX_ENV_PTY_WAITING_FOR_INPUT"
		case .osRelease, .overlayMutation, .pseudoFilesystems,
			.runtimeTmpfs, .userNamespaceMappings, .timeNamespaceOffsets,
			.maskedReadonlyPaths, .cgroupPidsLimit, .crossBootWrite,
			.crossBootVerify,
			.descriptorExecution, .longDescriptorExecution,
			.linuxPathDescriptorExecution, .pathLookupDescriptorExecution,
			.pathLookupWithoutPATHDescriptorExecution, .stdioExecution,
            .coreutilsCommand:
            return ""
        }
    }

    var delayedInput: String? {
        switch self {
        case .ptyStdio:
            return "orlix-pty-delayed-input\r"
		case .osRelease, .overlayMutation, .pseudoFilesystems,
			.runtimeTmpfs, .userNamespaceMappings, .timeNamespaceOffsets,
			.maskedReadonlyPaths, .cgroupPidsLimit, .crossBootWrite,
			.crossBootVerify,
			.descriptorExecution, .longDescriptorExecution,
			.linuxPathDescriptorExecution, .pathLookupDescriptorExecution,
			.pathLookupWithoutPATHDescriptorExecution, .stdioExecution,
            .coreutilsCommand:
            return nil
        }
    }

    var delayedInputSuccessMarker: String {
        switch self {
        case .ptyStdio:
            return "ORLIX_ENV_PTY_DELAYED_INPUT_OK"
		case .osRelease, .overlayMutation, .pseudoFilesystems,
			.runtimeTmpfs, .userNamespaceMappings, .timeNamespaceOffsets,
			.maskedReadonlyPaths, .cgroupPidsLimit, .crossBootWrite,
			.crossBootVerify,
			.descriptorExecution, .longDescriptorExecution,
			.linuxPathDescriptorExecution, .pathLookupDescriptorExecution,
			.pathLookupWithoutPATHDescriptorExecution, .stdioExecution,
            .coreutilsCommand:
            return ""
        }
    }

    var postDelayedInputScript: String? {
        switch self {
        case .ptyStdio:
            return #"printf '%s%s\n' ORLIX_ENV_ PTY_DONE"# + "\r"
		case .osRelease, .overlayMutation, .pseudoFilesystems,
			.runtimeTmpfs, .userNamespaceMappings, .timeNamespaceOffsets,
			.maskedReadonlyPaths, .cgroupPidsLimit, .crossBootWrite,
			.crossBootVerify,
			.descriptorExecution, .longDescriptorExecution,
			.linuxPathDescriptorExecution, .pathLookupDescriptorExecution,
			.pathLookupWithoutPATHDescriptorExecution, .stdioExecution,
            .coreutilsCommand:
            return nil
        }
    }

    func requiredMarkers(for fixture: RuntimeFixture) -> [String] {
        switch self {
        case .osRelease:
            return [
                "ORLIX_ENV_OS_RELEASE_BEGIN",
                fixture.expectedOSReleaseID,
                "ORLIX_ENV_OS_RELEASE_DONE"
            ]
        case .overlayMutation:
            return [
                "ORLIX_ENV_OVERLAY_MUTATION_BEGIN",
                "ORLIX_ENV_OVERLAY_COPYUP_WRITE_OK",
                "ORLIX_ENV_OVERLAY_COPYUP_OK",
                "ORLIX_ENV_OVERLAY_UNLINK_OK",
                "ORLIX_ENV_OVERLAY_MUTATION_DONE"
            ]
        case .descriptorExecution, .longDescriptorExecution:
            return [
                "ORLIX_ENV_EXEC_BEGIN",
                "argv0=\(descriptorExecutionArgument0)",
                "argv1=argument with spaces",
                "env=descriptor value with spaces",
                "Uid:\t1000",
                "Gid:\t100",
                "ORLIX_ENV_EXEC_DONE"
            ]
        case .linuxPathDescriptorExecution:
            return [
                "ORLIX_ENV_EXEC_BEGIN",
                "argv0=linux-path-argv0",
                "argv1=",
                "argv2=argument after empty",
                "pwd=/",
                "ORLIX_ENV_EXEC_DONE"
            ]
        case .pathLookupDescriptorExecution:
            return [
                "ORLIX_ENV_EXEC_BEGIN",
                "argv0=path-lookup-argv0",
                "argv1=argument after path lookup",
                "pwd=/",
                "ORLIX_ENV_EXEC_DONE"
            ]
        case .pathLookupWithoutPATHDescriptorExecution:
            return [
                "ORLIX_ENV_EXEC_BEGIN",
                "argv0=path-fallback-argv0",
                "argv1=argument after fallback path lookup",
                "env=descriptor value without path",
                "pwd=/",
                "ORLIX_ENV_EXEC_DONE"
            ]
        case .userNamespaceMappings:
            return [
                "ORLIX_ENV_USERNS_BEGIN",
                "ORLIX_ENV_USERNS_UID_MAP_OK",
                "ORLIX_ENV_USERNS_GID_MAP_OK",
                "ORLIX_ENV_USERNS_SETGROUPS_OK",
                "ORLIX_ENV_USERNS_DONE"
            ]
        case .timeNamespaceOffsets:
            return [
                "ORLIX_ENV_TIMENS_BEGIN",
                "ORLIX_ENV_TIMENS_MONOTONIC_OK",
                "ORLIX_ENV_TIMENS_BOOTTIME_OK",
                "ORLIX_ENV_TIMENS_DONE"
            ]
        case .maskedReadonlyPaths:
            return [
                "ORLIX_ENV_PATHS_BEGIN",
                "ORLIX_ENV_MASKED_FILE_OK",
                "ORLIX_ENV_READONLY_DIR_OK",
                "ORLIX_ENV_PATHS_DONE"
            ]
        case .cgroupPidsLimit:
            return [
                "ORLIX_ENV_CGROUP_BEGIN",
                "ORLIX_ENV_CGROUP_PATH_OK",
                "ORLIX_ENV_CGROUP_PIDS_MAX_OK",
                "ORLIX_ENV_CGROUP_PROCS_OK",
                "ORLIX_ENV_CGROUP_DONE"
            ]
        case .pseudoFilesystems:
            return [
                "ORLIX_ENV_PSEUDOFS_BEGIN",
                "ORLIX_ENV_PROC_DIR_OK",
                "ORLIX_ENV_PROC_MOUNTS_OK",
                "ORLIX_ENV_PROC_SELF_STATUS_OK",
                "ORLIX_ENV_PROC_SELF_FD_OK",
                "ORLIX_ENV_DEV_DIR_OK",
                "ORLIX_ENV_DEV_NULL_OK",
                "ORLIX_ENV_DEV_URANDOM_OK",
                "ORLIX_ENV_DEV_TTY_OK",
                "ORLIX_ENV_DEV_PTMX_OK",
                "ORLIX_ENV_DEV_PTS_OK",
                "ORLIX_ENV_SYS_DIR_OK",
                "ORLIX_ENV_SYS_BLOCK_VDA_OK",
                "ORLIX_ENV_SYS_BLOCK_VDB_OK",
                "ORLIX_ENV_PSEUDOFS_DONE"
            ]
		case .ptyStdio:
			return [
				"ORLIX_ENV_PTY_BEGIN",
				"ORLIX_ENV_PTY_STDIN_OK",
                "ORLIX_ENV_PTY_STDOUT_OK",
                "ORLIX_ENV_PTY_STDERR_OK",
                "ORLIX_ENV_PTY_PATH_OK",
                "ORLIX_ENV_PTY_WAITING_FOR_INPUT",
				"ORLIX_ENV_PTY_DELAYED_INPUT_OK",
				"ORLIX_ENV_PTY_DONE"
			]
		case .stdioExecution:
			return [
				"ORLIX_ENV_STDIO_BEGIN",
				"ORLIX_ENV_STDIO_STDOUT_OK",
				"ORLIX_ENV_STDIO_STDERR_OK",
				"ORLIX_ENV_STDIO_NOT_PTY_OK",
				"ORLIX_ENV_STDIO_DONE"
			]
		case .runtimeTmpfs:
			return [
				"ORLIX_ENV_TMPFS_BEGIN",
                "ORLIX_ENV_TMP_MOUNT_OK",
                "ORLIX_ENV_RUN_MOUNT_OK",
                "ORLIX_ENV_DEV_SHM_MOUNT_OK",
                "ORLIX_ENV_TMP_WRITE_OK",
                "ORLIX_ENV_RUN_WRITE_OK",
                "ORLIX_ENV_DEV_SHM_WRITE_OK",
                "ORLIX_ENV_TMPFS_DONE"
            ]
        case .crossBootWrite:
            return [
                "ORLIX_ENV_CROSSBOOT_WRITE_BEGIN",
                "ORLIX_ENV_CROSSBOOT_WRITE_OK",
                "ORLIX_ENV_CROSSBOOT_SYNC_OK",
                "ORLIX_ENV_CROSSBOOT_REREAD_OK",
                "ORLIX_ENV_CROSSBOOT_WRITE_DONE"
            ]
        case .crossBootVerify:
            return [
                "ORLIX_ENV_CROSSBOOT_VERIFY_BEGIN",
                "ORLIX_ENV_CROSSBOOT_SURVIVED_OK",
                "ORLIX_ENV_CROSSBOOT_CLEANUP_OK",
                "ORLIX_ENV_CROSSBOOT_VERIFY_DONE"
            ]
        case .coreutilsCommand:
            return [
                "ORLIX_ENV_COREUTILS_COMMAND_BEGIN",
                "coreutils-command-ok",
                "ORLIX_ENV_COREUTILS_STDOUT_OK",
                "ORLIX_ENV_COREUTILS_STDERR_OK",
                "ORLIX_ENV_COREUTILS_EXIT_STATUS_OK",
                "ORLIX_ENV_COREUTILS_COMMAND_DONE"
            ]
        }
    }

    private var descriptorExecutionScript: String {
        switch self {
        case .descriptorExecution, .longDescriptorExecution,
             .linuxPathDescriptorExecution, .pathLookupDescriptorExecution,
             .pathLookupWithoutPATHDescriptorExecution:
            return Self.descriptorExecutionScript
		case .osRelease, .overlayMutation, .pseudoFilesystems, .ptyStdio,
			.stdioExecution, .runtimeTmpfs, .userNamespaceMappings, .timeNamespaceOffsets,
			.maskedReadonlyPaths, .cgroupPidsLimit, .crossBootWrite,
			.crossBootVerify, .coreutilsCommand:
            return ""
        }
    }

    func requiredOrderedMarkerGroups(for fixture: RuntimeFixture) -> [[String]] {
        switch self {
        case .descriptorExecution, .longDescriptorExecution:
            return [["pwd=", "/tmp"]]
        default:
            return []
        }
    }

    private var descriptorExecutionArgument0: String {
        switch self {
        case .longDescriptorExecution:
            return "orlix-descriptor-" + String(repeating: "x", count: 256)
        case .descriptorExecution:
            return "orlix-descriptor-" + String(repeating: "x", count: 16)
		case .osRelease, .overlayMutation, .linuxPathDescriptorExecution,
			.pathLookupDescriptorExecution,
			.pathLookupWithoutPATHDescriptorExecution, .pseudoFilesystems,
			.ptyStdio, .stdioExecution, .runtimeTmpfs, .userNamespaceMappings,
			.timeNamespaceOffsets, .maskedReadonlyPaths, .cgroupPidsLimit,
			.crossBootWrite, .crossBootVerify, .coreutilsCommand:
            return ""
        }
    }

    private static let userNamespaceMappingScript = [
        #"printf '%s%s\n' ORLIX_ENV_ USERNS_BEGIN"#,
        #"set -- $(/bin/cat /proc/self/uid_map)"#,
        #"if [ "$1:$2:$3" = "0:0:1" ]; then printf '%s%s\n' ORLIX_ENV_ USERNS_UID_MAP_OK; else printf '%s%s\n' ORLIX_ENV_USERNS_ PROOF_FAILED_UID_MAP; fi"#,
        #"set -- $(/bin/cat /proc/self/gid_map)"#,
        #"if [ "$1:$2:$3" = "0:0:1" ]; then printf '%s%s\n' ORLIX_ENV_ USERNS_GID_MAP_OK; else printf '%s%s\n' ORLIX_ENV_USERNS_ PROOF_FAILED_GID_MAP; fi"#,
        #"if /bin/test -e /proc/self/setgroups; then printf '%s%s\n' ORLIX_ENV_ USERNS_SETGROUPS_OK; else printf '%s%s\n' ORLIX_ENV_USERNS_ PROOF_FAILED_SETGROUPS; fi"#,
        #"printf '%s%s\n' ORLIX_ENV_ USERNS_DONE"#,
    ].joined(separator: "\n")

    private static let timeNamespaceOffsetScript = [
        #"printf '%s%s\n' ORLIX_ENV_ TIMENS_BEGIN"#,
        #"set -- $(/bin/cat /proc/self/timens_offsets)"#,
        #"if [ "$1:$2:$3" = "monotonic:12:500000000" ]; then printf '%s%s\n' ORLIX_ENV_ TIMENS_MONOTONIC_OK; else printf '%s%s\n' ORLIX_ENV_TIMENS_ PROOF_FAILED_MONOTONIC; fi"#,
        #"if [ "$4:$5:$6" = "boottime:3:250" ]; then printf '%s%s\n' ORLIX_ENV_ TIMENS_BOOTTIME_OK; else printf '%s%s\n' ORLIX_ENV_TIMENS_ PROOF_FAILED_BOOTTIME; fi"#,
        #"printf '%s%s\n' ORLIX_ENV_ TIMENS_DONE"#,
    ].joined(separator: "\n")

    private static let maskedReadonlyPathScript = [
        #"printf '%s%s\n' ORLIX_ENV_ PATHS_BEGIN"#,
        #"if /bin/test ! -s /etc/os-release; then printf '%s%s\n' ORLIX_ENV_ MASKED_FILE_OK; else printf '%s%s\n' ORLIX_ENV_PATHS_ PROOF_FAILED_MASKED_FILE; fi"#,
        #"if /bin/touch /etc/orlix-readonly-proof 2>/dev/null; then /bin/rm -f /etc/orlix-readonly-proof; printf '%s%s\n' ORLIX_ENV_PATHS_ PROOF_FAILED_READONLY_DIR; else printf '%s%s\n' ORLIX_ENV_ READONLY_DIR_OK; fi"#,
        #"printf '%s%s\n' ORLIX_ENV_ PATHS_DONE"#,
    ].joined(separator: "\n")

    private static let cgroupPidsLimitScript = [
        #"printf '%s%s\n' ORLIX_ENV_ CGROUP_BEGIN"#,
        #"cgroup_path=$(/bin/cat /proc/self/cgroup)"#,
        #"case "$cgroup_path" in *"0::/orlix/runtime-proof"*) printf '%s%s\n' ORLIX_ENV_ CGROUP_PATH_OK;; *) printf '%s%s\n' ORLIX_ENV_CGROUP_ PROOF_FAILED_PATH;; esac"#,
        #"pids_max=$(/bin/cat /sys/fs/cgroup/orlix/runtime-proof/pids.max)"#,
        #"if [ "$pids_max" = "32" ]; then printf '%s%s\n' ORLIX_ENV_ CGROUP_PIDS_MAX_OK; else printf '%s%s\n' ORLIX_ENV_CGROUP_ PROOF_FAILED_PIDS_MAX; fi"#,
        #"found_self=0; for pid in $(/bin/cat /sys/fs/cgroup/orlix/runtime-proof/cgroup.procs); do if [ "$pid" = "$$" ]; then found_self=1; fi; done; if [ "$found_self" = 1 ]; then printf '%s%s\n' ORLIX_ENV_ CGROUP_PROCS_OK; else printf '%s%s\n' ORLIX_ENV_CGROUP_ PROOF_FAILED_PROCS; fi"#,
        #"printf '%s%s\n' ORLIX_ENV_ CGROUP_DONE"#,
    ].joined(separator: "\n")

	private static let descriptorExecutionScript = (
		[": descriptor-start"] + descriptorExecutionLines
	).joined(separator: "\n")

	private static let stdioExecutionScript = [
		#"printf '%s%s\n' ORLIX_ENV_ STDIO_BEGIN"#,
		#"printf '%s%s\n' ORLIX_ENV_STDIO_ STDOUT_OK"#,
		#"printf '%s%s\n' ORLIX_ENV_STDIO_ STDERR_OK >&2"#,
		#"if command -v tty >/dev/null 2>&1; then tty_path=$(tty); elif /bin/test -x /bin/tty; then tty_path=$(/bin/tty); elif /bin/test -x /usr/bin/tty; then tty_path=$(/usr/bin/tty); else tty_path=missing-tty-command; fi"#,
		#"printf 'stdio_tty=%s\n' "$tty_path""#,
		#"case "$tty_path" in /dev/pts/*) printf '%s%s\n' ORLIX_ENV_STDIO_PROOF_ FAILED_PTY;; *) printf '%s%s\n' ORLIX_ENV_STDIO_ NOT_PTY_OK;; esac"#,
		#"printf '%s%s\n' ORLIX_ENV_ STDIO_DONE"#,
	].joined(separator: "\n")

	private static let coreutilsCommandScript = [
		#"printf '%s%s\n' ORLIX_ENV_ COREUTILS_COMMAND_BEGIN"#,
		#"/bin/echo coreutils-command-ok"#,
		#"/bin/echo ORLIX_ENV_COREUTILS_STDOUT_OK"#,
		#"/bin/echo ORLIX_ENV_COREUTILS_STDERR_OK >&2"#,
		#"/bin/true"#,
		#"true_status=$?"#,
		#"/bin/false"#,
		#"false_status=$?"#,
		#"if [ "$true_status:$false_status" = "0:1" ]; then printf '%s%s\n' ORLIX_ENV_ COREUTILS_EXIT_STATUS_OK; else printf '%s%s\n' ORLIX_ENV_COREUTILS_ PROOF_FAILED_EXIT_STATUS; fi"#,
		#"printf '%s%s\n' ORLIX_ENV_ COREUTILS_COMMAND_DONE"#,
	].joined(separator: "\n")

	private static let descriptorExecutionLines = [
        #"printf '%s%s\n' ORLIX_ENV_ EXEC_BEGIN"#,
        #"printf 'argv0=%s\n' "$0""#,
        #"printf 'argv1=%s\n' "$1""#,
        #"printf 'argv2=%s\n' "$2""#,
        #"printf 'env=%s\n' "$ORLIX_DESCRIPTOR_MESSAGE""#,
        #"printf pwd=; pwd"#,
        #"/bin/cat /etc/os-release"#,
        #"/bin/cat /proc/self/status"#,
        #"printf '%s%s\n' ORLIX_ENV_ EXEC_DONE"#,
    ]
}

private enum RuntimeFixture: Sendable {
    case tarDerived
    case ociDerived

    var directoryName: String {
        switch self {
        case .tarDerived:
            return "tar-imported"
        case .ociDerived:
            return "oci-imported"
        }
    }

    var description: String {
        switch self {
        case .tarDerived:
            return "tar-derived"
        case .ociDerived:
            return "OCI-derived"
        }
    }

    var environmentID: String {
        switch self {
        case .tarDerived:
            return "tar-imported-runtime-test-fixture"
        case .ociDerived:
            return "oci-imported-runtime-test-fixture"
        }
    }

    var copiedEnvironmentID: String {
        "\(environmentID)-copy"
    }

    var source: OrlixEnvironmentSource {
        switch self {
        case .tarDerived:
            return .rootfsTar
        case .ociDerived:
            return .ociLayout
        }
    }

    var rootImageIdentifier: String {
        switch self {
        case .tarDerived:
            return "orlix.test.environment.tar-runtime-test-fixture"
        case .ociDerived:
            return "orlix.test.environment.oci-runtime-test-fixture"
        }
    }

    var copiedRootImageIdentifier: String {
        "\(rootImageIdentifier).copy"
    }

    var expectedOSReleaseID: String {
        switch self {
        case .tarDerived:
            return "ID=orlix-tar-runtime-test-fixture"
        case .ociDerived:
            return "ID=orlix-oci-runtime-test-fixture"
        }
    }

}

private struct EnvironmentRootFixture {
    let root: URL

    var linuxStateRoot: URL {
        root.appendingPathComponent("state", isDirectory: true)
    }

    var cacheRoot: URL {
        root.appendingPathComponent("cache", isDirectory: true)
    }

    var scratchRoot: URL {
        root.appendingPathComponent("scratch", isDirectory: true)
    }
}

private enum OrlixEnvironmentRootRuntimeProofError:
    Error,
    CustomStringConvertible
{
    case bootFailed(OrlixBootStatus)
    case noTerminalOutput(TimeInterval, URL)
    case timeout(TimeInterval, String, URL, String)
    case fatalMarker(String)
    case missingMarker(String, URL)
    case staleFixture(String)

    var description: String {
        switch self {
        case let .bootFailed(status):
            return "Orlix imported environment boot failed: \(status.message)"
        case let .noTerminalOutput(timeout, logURL):
            return "no Linux terminal output after \(Int(timeout)) seconds; terminal log: \(logURL.path)"
        case let .timeout(timeout, output, logURL, tail):
            return "timed out after \(Int(timeout)) seconds waiting for imported environment proof output; terminal log: \(logURL.path); tail: \(tail); output: \(output)"
        case let .fatalMarker(marker):
            return "fatal imported environment runtime marker found: \(marker)"
        case let .missingMarker(marker, logURL):
            return "missing imported environment marker \(marker); terminal log: \(logURL.path)"
        case let .staleFixture(message):
            return "stale imported environment runtime fixture: \(message)"
        }
    }
}

private final class EnvironmentRootTerminalLog: @unchecked Sendable {
    let url: URL

    private let lock = NSLock()

    init() {
        let fileName = "orlix-environment-root-\(UUID().uuidString).log"
        url = FileManager.default.temporaryDirectory.appendingPathComponent(fileName)
        FileManager.default.createFile(atPath: url.path, contents: nil)
    }

    func writeLine(_ line: String) {
        append(Data("[orlix-environment-root] \(line)\n".utf8))
    }

    func append(_ data: Data) {
        lock.lock()
        defer { lock.unlock() }

        guard let handle = FileHandle(forWritingAtPath: url.path) else {
            return
        }
        handle.seekToEndOfFile()
        handle.write(data)
        handle.closeFile()
    }

    func tail(limit: Int = 4096) -> String {
        lock.lock()
        defer { lock.unlock() }

        guard let data = try? Data(contentsOf: url) else {
            return ""
        }
        return String(decoding: data.suffix(limit), as: UTF8.self)
    }
}

private final class EnvironmentRootBootStatusRecorder: @unchecked Sendable {
    private let lock = NSLock()
    private var storage: OrlixBootStatus?

    var value: OrlixBootStatus? {
        lock.lock()
        defer { lock.unlock() }
        return storage
    }

    func set(_ status: OrlixBootStatus) {
        lock.lock()
        storage = status
        lock.unlock()
    }
}

private final class EnvironmentRootOutputRecorder: @unchecked Sendable {
    private let lock = NSLock()
    private let terminalLog: EnvironmentRootTerminalLog
    private var storage = Data()
    private var sentProofCommands = false
    private var sentDelayedInput = false
    private var sentPostDelayedInputScript = false

    init(terminalLog: EnvironmentRootTerminalLog) {
        self.terminalLog = terminalLog
    }

    var text: String {
        lock.lock()
        defer { lock.unlock() }
        return String(decoding: storage, as: UTF8.self)
    }

    var hasSentProofCommands: Bool {
        lock.lock()
        defer { lock.unlock() }
        return sentProofCommands
    }

    var hasSentDelayedInput: Bool {
        lock.lock()
        defer { lock.unlock() }
        return sentDelayedInput
    }

    var hasSentPostDelayedInputScript: Bool {
        lock.lock()
        defer { lock.unlock() }
        return sentPostDelayedInputScript
    }

    var byteCount: Int {
        lock.lock()
        defer { lock.unlock() }
        return storage.count
    }

    func append(_ data: Data) {
        lock.lock()
        storage.append(data)
        lock.unlock()
        terminalLog.append(data)
    }

    func markProofCommandsSent() {
        lock.lock()
        sentProofCommands = true
        lock.unlock()
    }

    func markDelayedInputSent() {
        lock.lock()
        sentDelayedInput = true
        lock.unlock()
    }

    func markPostDelayedInputScriptSent() {
        lock.lock()
        sentPostDelayedInputScript = true
        lock.unlock()
    }
}
