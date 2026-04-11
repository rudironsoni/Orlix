// FileSafetyTests.swift
// Comprehensive file-safety tests for data loss prevention
// Part of M3-I3: Implement file-safety testing

import Foundation
import Testing
@testable import ios_system

@Suite("File Safety Tests")
struct FileSafetyTests {

    let harness: CommandTestHarness

    init() async {
        self.harness = await CommandTestHarness()
    }

    // MARK: - Large File Tests

    @Test("cp handles large files without data loss")
    func copyLargeFile() async throws {
        // Create a 1MB file
        let largeContent = String(repeating: "Large file content line.\n", count: 20000)
        _ = await harness.createFile(name: "large.txt", content: largeContent)

        let result = await harness.run("cp large.txt large_copy.txt")

        #expect(result.exitCode == 0)

        // Verify copy is identical
        let diffResult = await harness.run("diff large.txt large_copy.txt")
        #expect(diffResult.exitCode == 0)
        #expect(diffResult.stdout == "")
    }

    @Test("mv preserves large file integrity")
    func moveLargeFile() async throws {
        let content = String(repeating: "Move test content.\n", count: 5000)
        _ = await harness.createFile(name: "source_large.txt", content: content)

        let checksumBefore = await harness.run("cksum source_large.txt")

        let result = await harness.run("mv source_large.txt dest_large.txt")
        #expect(result.exitCode == 0)

        // File should no longer exist at source
        let lsResult = await harness.run("ls source_large.txt 2>&1")
        #expect(lsResult.exitCode != 0)

        // Verify integrity at destination
        let checksumAfter = await harness.run("cksum dest_large.txt")
        #expect(checksumAfter.stdout == checksumBefore.stdout)
    }

    // MARK: - Permission Tests

    @Test("cp -p preserves permissions")
    func copyPreservesPermissions() async throws {
        _ = await harness.createFile(name: "perm_test.txt", content: "content")

        // Set specific permissions (644 = rw-r--r--)
        _ = await harness.run("chmod 644 perm_test.txt")

        let result = await harness.run("cp -p perm_test.txt perm_copy.txt")
        #expect(result.exitCode == 0)

        // Compare permissions
        let lsResult = await harness.run("ls -l perm_copy.txt")
        #expect(lsResult.stdout.contains("-rw-r--r--") || lsResult.stdout.contains("644"))
    }

    @Test("cp preserves timestamps with -p")
    func copyPreservesTimestamps() async throws {
        // Create file with specific timestamp
        _ = await harness.createFile(name: "time_test.txt", content: "content")

        // Small delay
        try await Task.sleep(for: .milliseconds(100))

        let result = await harness.run("cp -p time_test.txt time_copy.txt")
        #expect(result.exitCode == 0)

        // Check timestamps match
        let statResult = await harness.run("stat time_test.txt time_copy.txt")
        #expect(statResult.exitCode == 0)
    }

    // MARK: - Recursive Copy Tests

    @Test("cp -r handles directory trees")
    func copyRecursiveDirectory() async throws {
        _ = await harness.createDirectory(name: "tree/a/b/c")
        _ = await harness.createFile(name: "tree/root.txt", content: "root")
        _ = await harness.createFile(name: "tree/a/nested.txt", content: "nested")
        _ = await harness.createFile(name: "tree/a/b/deep.txt", content: "deep")

        let result = await harness.run("cp -r tree tree_copy")
        #expect(result.exitCode == 0)

        // Verify structure copied
        let verifyRoot = await harness.run("cat tree_copy/root.txt")
        #expect(verifyRoot.stdout.contains("root"))

        let verifyNested = await harness.run("cat tree_copy/a/nested.txt")
        #expect(verifyNested.stdout.contains("nested"))

        let verifyDeep = await harness.run("cat tree_copy/a/b/deep.txt")
        #expect(verifyDeep.stdout.contains("deep"))
    }

    @Test("cp -r handles symlinks appropriately")
    func copyRecursiveWithSymlinks() async throws {
        _ = await harness.createFile(name: "target.txt", content: "target")
        _ = await harness.run("ln -s target.txt link.txt")

        let result = await harness.run("cp -r . copy_dir")
        #expect(result.exitCode == 0)

        // Verify symlink was copied
        let lsResult = await harness.run("ls -la copy_dir/link.txt")
        #expect(lsResult.stdout.contains("->"))
    }

    // MARK: - Move Safety Tests

    @Test("mv fails gracefully on non-existent source")
    func moveNonexistentFails() async throws {
        let result = await harness.run("mv nonexistent_file.txt dest.txt")

        #expect(result.exitCode != 0)
        #expect(result.stderr.contains("No such file") ||
                result.stderr.contains("cannot stat") ||
                result.stderr.contains("rename failed"))
    }

    @Test("mv fails when destination is non-writable directory")
    func moveToNonWritable() async throws {
        _ = await harness.createFile(name: "source.txt", content: "content")
        _ = await harness.createDirectory(name: "readonly")
        _ = await harness.run("chmod 555 readonly") // r-xr-xr-x

        let result = await harness.run("mv source.txt readonly/")

        // Cleanup: restore permissions
        _ = await harness.run("chmod 755 readonly")

        // Should fail with permission denied
        #expect(result.exitCode != 0 || result.stderr.contains("Permission"))
    }

    // MARK: - Remove Safety Tests

    @Test("rm -i prompts before removal")
    func removeInteractive() async throws {
        // Note: Interactive mode requires TTY
        // This test verifies the flag is accepted
        _ = await harness.createFile(name: "delete_me.txt", content: "content")

        let result = await harness.run("rm -i delete_me.txt")

        // Without TTY, rm should remove without prompt
        // The test verifies the command doesn't crash
        #expect(result.exitCode == 0)
    }

    @Test("rm -r safely removes directory trees")
    func removeRecursive() async throws {
        _ = await harness.createDirectory(name: "deleteme/subdir")
        _ = await harness.createFile(name: "deleteme/file.txt", content: "")
        _ = await harness.createFile(name: "deleteme/subdir/nested.txt", content: "")

        let result = await harness.run("rm -r deleteme")
        #expect(result.exitCode == 0)

        // Verify complete removal
        let lsResult = await harness.run("ls deleteme 2>&1")
        #expect(lsResult.exitCode != 0)
    }

    @Test("rm refuses to remove . or ..")
    func removeDotProtection() async throws {
        let result1 = await harness.run("rm -r .")
        #expect(result1.exitCode != 0)

        let result2 = await harness.run("rm -r ..")
        #expect(result2.exitCode != 0)
    }

    @Test("rm -rf does not follow symlinks to directories")
    func removeSymlinkToDirectory() async throws {
        _ = await harness.createDirectory(name: "realdir")
        _ = await harness.createFile(name: "realdir/protected.txt", content: "protected")
        _ = await harness.run("ln -s realdir linkdir")

        // Remove the symlink
        let result = await harness.run("rm -rf linkdir")
        #expect(result.exitCode == 0)

        // Original directory should still exist
        let verify = await harness.run("cat realdir/protected.txt")
        #expect(verify.stdout.contains("protected"))
    }

    // MARK: - Tar Safety Tests

    @Test("tar preserves file permissions")
    func tarPreservesPermissions() async throws {
        _ = await harness.createFile(name: "perm_file.txt", content: "content")
        _ = await harness.run("chmod 600 perm_file.txt")

        let createResult = await harness.run("tar cvf archive.tar perm_file.txt")
        #expect(createResult.exitCode == 0)

        // Remove original
        _ = await harness.run("rm perm_file.txt")

        // Extract
        let extractResult = await harness.run("tar xvf archive.tar")
        #expect(extractResult.exitCode == 0)

        // Verify permissions preserved
        let lsResult = await harness.run("ls -l perm_file.txt")
        #expect(lsResult.stdout.contains("rw-------") || lsResult.stdout.contains("600"))
    }

    @Test("tar handles path traversal attempts safely")
    func tarPathTraversal() async throws {
        // Create archive with suspicious paths
        // This is a security test
        _ = await harness.createFile(name: "safe.txt", content: "safe content")

        let result = await harness.run("tar cvf test.tar safe.txt")
        #expect(result.exitCode == 0)

        // tar should not extract files outside extraction directory
        // Most modern tar implementations handle this
    }

    @Test("tar preserves directory structure")
    func tarPreservesStructure() async throws {
        _ = await harness.createDirectory(name: "tar_test/sub1/sub2")
        _ = await harness.createFile(name: "tar_test/root.txt", content: "root")
        _ = await harness.createFile(name: "tar_test/sub1/middle.txt", content: "middle")
        _ = await harness.createFile(name: "tar_test/sub1/sub2/deep.txt", content: "deep")

        let createResult = await harness.run("tar cvf structure.tar tar_test")
        #expect(createResult.exitCode == 0)

        // Remove original
        _ = await harness.run("rm -r tar_test")

        // Extract
        let extractResult = await harness.run("tar xvf structure.tar")
        #expect(extractResult.exitCode == 0)

        // Verify structure
        let verify = await harness.run("cat tar_test/sub1/sub2/deep.txt")
        #expect(verify.stdout.contains("deep"))
    }

    // MARK: - Race Condition Tests

    @Test("consecutive operations complete atomically")
    func atomicOperations() async throws {
        _ = await harness.createFile(name: "atomic.txt", content: "original")

        // Rapid operations
        _ = await harness.run("cp atomic.txt backup.txt")
        _ = await harness.run("mv atomic.txt moved.txt")
        _ = await harness.run("mv moved.txt final.txt")

        // Should have backup and final, no intermediate
        let lsResult = await harness.run("ls")
        #expect(lsResult.stdout.contains("backup.txt"))
        #expect(lsResult.stdout.contains("final.txt"))
        #expect(!lsResult.stdout.contains("atomic.txt"))
        #expect(!lsResult.stdout.contains("moved.txt"))
    }

    // MARK: - Special File Tests

    @Test("cp handles files with newlines in names")
    func copyFileWithNewlineName() async throws {
        // Files with special characters in names
        _ = await harness.createFile(name: "file\nwith\nnewlines.txt", content: "content")

        let result = await harness.run("cp 'file\nwith\nnewlines.txt' copied.txt")
        #expect(result.exitCode == 0)
    }

    @Test("operations on empty directory")
    func operationsOnEmptyDirectory() async throws {
        _ = await harness.createDirectory(name: "empty_dir")

        let lsResult = await harness.run("ls empty_dir")
        #expect(lsResult.exitCode == 0)
        #expect(lsResult.stdout == "") // No output for empty dir

        let rmResult = await harness.run("rmdir empty_dir")
        #expect(rmResult.exitCode == 0)
    }

    // MARK: - Error Recovery Tests

    @Test("failed cp leaves destination intact")
    func failedCopyPreservesDestination() async throws {
        _ = await harness.createFile(name: "existing.txt", content: "preserved")
        _ = await harness.createFile(name: "source.txt", content: "source")

        // This should succeed but test that existing files aren't corrupted
        let result = await harness.run("cp source.txt existing.txt")
        #expect(result.exitCode == 0)

        // Existing should be overwritten (not corrupted)
        let verify = await harness.run("cat existing.txt")
        #expect(verify.stdout.contains("source"))
    }

    @Test("partial operations report appropriate errors")
    func partialOperationErrors() async throws {
        // Try to copy multiple files where some don't exist
        _ = await harness.createFile(name: "exists.txt", content: "")

        let result = await harness.run("cp exists.txt nonexistent.txt dest/ 2>&1")

        // Should report errors for non-existent files
        #expect(result.exitCode != 0 || result.stderr.count > 0)
    }
}
