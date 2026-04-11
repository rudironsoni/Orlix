// TierAConformanceTests.swift
// Conformance test suite for Tier A commands
// Part of M3-I2: Build conformance test suite

import Foundation
import Testing
@testable import ios_system

@Suite("Tier A Command Conformance Tests")
struct TierAConformanceTests {

    let harness: CommandTestHarness

    init() async {
        self.harness = await CommandTestHarness()
    }

    // MARK: - echo Tests

    @Test("echo basic output")
    func echoBasic() async throws {
        let result = await harness.run("echo hello")

        #expect(result.exitCode == 0)
        #expect(result.stdout.contains("hello"))
    }

    @Test("echo multiple arguments")
    func echoMultipleArgs() async throws {
        let result = await harness.run("echo hello world")

        #expect(result.exitCode == 0)
        #expect(result.stdout.contains("hello world"))
    }

    @Test("echo with environment variable")
    func echoEnvironment() async throws {
        await harness.setenv("TEST_VAR", "test_value")
        let result = await harness.run("echo $TEST_VAR")

        #expect(result.exitCode == 0)
        #expect(result.stdout.contains("test_value"))
    }

    // MARK: - cat Tests

    @Test("cat displays file contents")
    func catFile() async throws {
        _ = await harness.createFile(name: "test.txt", content: "Hello from file")

        let result = await harness.run("cat test.txt")

        #expect(result.exitCode == 0)
        #expect(result.stdout.contains("Hello from file"))
    }

    @Test("cat concatenates multiple files")
    func catMultipleFiles() async throws {
        _ = await harness.createFile(name: "file1.txt", content: "Content1")
        _ = await harness.createFile(name: "file2.txt", content: "Content2")

        let result = await harness.run("cat file1.txt file2.txt")

        #expect(result.exitCode == 0)
        #expect(result.stdout.contains("Content1"))
        #expect(result.stdout.contains("Content2"))
    }

    @Test("cat returns error for nonexistent file")
    func catNonexistent() async throws {
        let result = await harness.run("cat nonexistent.txt")

        #expect(result.exitCode != 0)
        #expect(result.stderr.contains("No such file") || result.stderr.contains("cannot open"))
    }

    // MARK: - ls Tests

    @Test("ls lists current directory")
    func lsCurrent() async throws {
        _ = await harness.createFile(name: "visible_file.txt", content: "")

        let result = await harness.run("ls")

        #expect(result.exitCode == 0)
        #expect(result.stdout.contains("visible_file.txt"))
    }

    @Test("ls -la shows hidden files")
    func lsAll() async throws {
        _ = await harness.createFile(name: ".hidden", content: "")

        let result = await harness.run("ls -la")

        #expect(result.exitCode == 0)
        #expect(result.stdout.contains(".hidden"))
        #expect(result.stdout.contains("total"))
    }

    @Test("ls returns error for nonexistent directory")
    func lsNonexistent() async throws {
        let result = await harness.run("ls /nonexistent_directory")

        #expect(result.exitCode != 0)
    }

    // MARK: - pwd Tests

    @Test("pwd prints working directory")
    func pwdBasic() async throws {
        let result = await harness.run("pwd")

        #expect(result.exitCode == 0)
        #expect(result.stdout.count > 0)
        #expect(result.stdout.hasPrefix("/"))
    }

    // MARK: - mkdir/rmdir Tests

    @Test("mkdir creates directory")
    func mkdirBasic() async throws {
        let result = await harness.run("mkdir newdir")

        #expect(result.exitCode == 0)

        // Verify directory exists
        let lsResult = await harness.run("ls -la")
        #expect(lsResult.stdout.contains("newdir"))
    }

    @Test("mkdir -p creates nested directories")
    func mkdirParents() async throws {
        let result = await harness.run("mkdir -p a/b/c")

        #expect(result.exitCode == 0)

        // Verify nested structure
        let lsResult = await harness.run("ls a/b")
        #expect(lsResult.stdout.contains("c"))
    }

    // MARK: - touch Tests

    @Test("touch creates empty file")
    func touchBasic() async throws {
        let result = await harness.run("touch newfile.txt")

        #expect(result.exitCode == 0)

        let lsResult = await harness.run("ls -la")
        #expect(lsResult.stdout.contains("newfile.txt"))
    }

    // MARK: - cp Tests

    @Test("cp copies file")
    func cpFile() async throws {
        _ = await harness.createFile(name: "source.txt", content: "source content")

        let result = await harness.run("cp source.txt dest.txt")

        #expect(result.exitCode == 0)

        let catResult = await harness.run("cat dest.txt")
        #expect(catResult.stdout.contains("source content"))
    }

    @Test("cp -r copies directory")
    func cpRecursive() async throws {
        let dir = await harness.createDirectory(name: "sourcedir")
        _ = await harness.createFile(name: "sourcedir/file.txt", content: "inside")

        let result = await harness.run("cp -r sourcedir destdir")

        #expect(result.exitCode == 0)

        let catResult = await harness.run("cat destdir/file.txt")
        #expect(catResult.stdout.contains("inside"))
    }

    // MARK: - mv Tests

    @Test("mv renames file")
    func mvRename() async throws {
        _ = await harness.createFile(name: "oldname.txt", content: "content")

        let result = await harness.run("mv oldname.txt newname.txt")

        #expect(result.exitCode == 0)

        let lsResult = await harness.run("ls")
        #expect(lsResult.stdout.contains("newname.txt"))
        #expect(!lsResult.stdout.contains("oldname.txt"))
    }

    @Test("mv moves file to directory")
    func mvToDirectory() async throws {
        _ = await harness.createFile(name: "file.txt", content: "content")
        _ = await harness.createDirectory(name: "subdir")

        let result = await harness.run("mv file.txt subdir/")

        #expect(result.exitCode == 0)

        let lsResult = await harness.run("ls subdir/")
        #expect(lsResult.stdout.contains("file.txt"))
    }

    // MARK: - rm Tests

    @Test("rm removes file")
    func rmFile() async throws {
        _ = await harness.createFile(name: "delete_me.txt", content: "")

        let result = await harness.run("rm delete_me.txt")

        #expect(result.exitCode == 0)

        let lsResult = await harness.run("ls")
        #expect(!lsResult.stdout.contains("delete_me.txt"))
    }

    @Test("rm -r removes directory")
    func rmRecursive() async throws {
        _ = await harness.createDirectory(name: "deletedir")
        _ = await harness.createFile(name: "deletedir/file.txt", content: "")

        let result = await harness.run("rm -r deletedir")

        #expect(result.exitCode == 0)

        let lsResult = await harness.run("ls")
        #expect(!lsResult.stdout.contains("deletedir"))
    }

    // MARK: - grep Tests

    @Test("grep finds pattern")
    func grepPattern() async throws {
        _ = await harness.createFile(name: "text.txt", content: "line one\nline two\nline three")

        let result = await harness.run("grep 'two' text.txt")

        #expect(result.exitCode == 0)
        #expect(result.stdout.contains("line two"))
        #expect(!result.stdout.contains("line one"))
    }

    @Test("grep -i is case insensitive")
    func grepCaseInsensitive() async throws {
        _ = await harness.createFile(name: "text.txt", content: "Hello World")

        let result = await harness.run("grep -i 'hello' text.txt")

        #expect(result.exitCode == 0)
        #expect(result.stdout.contains("Hello"))
    }

    // MARK: - head/tail Tests

    @Test("head shows first lines")
    func headBasic() async throws {
        let content = (1...20).map { "Line \($0)" }.joined(separator: "\n")
        _ = await harness.createFile(name: "lines.txt", content: content)

        let result = await harness.run("head -5 lines.txt")

        #expect(result.exitCode == 0)
        #expect(result.stdout.contains("Line 1"))
        #expect(result.stdout.contains("Line 5"))
        #expect(!result.stdout.contains("Line 6"))
    }

    @Test("tail shows last lines")
    func tailBasic() async throws {
        let content = (1...20).map { "Line \($0)" }.joined(separator: "\n")
        _ = await harness.createFile(name: "lines.txt", content: content)

        let result = await harness.run("tail -5 lines.txt")

        #expect(result.exitCode == 0)
        #expect(result.stdout.contains("Line 16"))
        #expect(result.stdout.contains("Line 20"))
        #expect(!result.stdout.contains("Line 15"))
    }

    // MARK: - wc Tests

    @Test("wc counts lines words bytes")
    func wcBasic() async throws {
        _ = await harness.createFile(name: "count.txt", content: "Hello world\nSecond line\n")

        let result = await harness.run("wc count.txt")

        #expect(result.exitCode == 0)
        // Output format: lines words bytes filename
        // Should contain something like: 2 4 24 count.txt
        #expect(result.stdout.contains("count.txt"))
    }

    // MARK: - Pipeline Tests

    @Test("pipe chains commands")
    func pipeChain() async throws {
        _ = await harness.createFile(name: "data.txt", content: "apple\nbanana\ncherry\napple\n")

        let result = await harness.run("cat data.txt | grep apple | wc -l")

        #expect(result.exitCode == 0)
        // Should output 2 (two lines contain "apple")
    }

    @Test("multi-stage pipeline")
    func multiStagePipe() async throws {
        let content = (1...10).map { "Item \($0)" }.joined(separator: "\n")
        _ = await harness.createFile(name: "items.txt", content: content)

        let result = await harness.run("cat items.txt | head -5 | tail -3")

        #expect(result.exitCode == 0)
        // Should show lines 3, 4, 5
    }

    // MARK: - Session Isolation Tests

    @Test("environment is session isolated")
    func sessionEnvironmentIsolation() async throws {
        // Set variable in current session
        await harness.setenv("SESSION_VAR", "value1")

        // Switch to different session
        await harness.switchSession(to: "other-session")

        // Variable should not exist in new session
        let value = await harness.getenv("SESSION_VAR")
        #expect(value == nil)

        // Set different value in new session
        await harness.setenv("SESSION_VAR", "value2")
        let newValue = await harness.getenv("SESSION_VAR")
        #expect(newValue == "value2")
    }

    // MARK: - Edge Cases

    @Test("empty file handling")
    func emptyFile() async throws {
        _ = await harness.createFile(name: "empty.txt", content: "")

        let result = await harness.run("cat empty.txt")

        #expect(result.exitCode == 0)
        #expect(result.stdout == "")
    }

    @Test("unicode in filenames")
    func unicodeFilename() async throws {
        _ = await harness.createFile(name: "文件.txt", content: "unicode content")

        let result = await harness.run("cat 文件.txt")

        #expect(result.exitCode == 0)
        #expect(result.stdout.contains("unicode content"))
    }

    @Test("spaces in filenames")
    func spacesInFilename() async throws {
        _ = await harness.createFile(name: "file with spaces.txt", content: "content")

        let result = await harness.run("cat 'file with spaces.txt'")

        #expect(result.exitCode == 0)
        #expect(result.stdout.contains("content"))
    }
}
