// CommandTestHarness.swift
// Test infrastructure for Tier A command conformance testing
// Part of M3-I2: Build conformance test suite

import Foundation
import Testing
@testable import ios_system

/// Test harness for capturing and verifying command output
public actor CommandTestHarness {

    // MARK: - Types

    /// Result of command execution
    public struct CommandResult {
        public let stdout: String
        public let stderr: String
        public let exitCode: Int32
        public let duration: TimeInterval

        public init(stdout: String, stderr: String, exitCode: Int32, duration: TimeInterval) {
            self.stdout = stdout
            self.stderr = stderr
            self.exitCode = exitCode
            self.duration = duration
        }
    }

    /// Test expectation for command output
    public struct OutputExpectation {
        public let contains: [String]?
        public let matches: NSRegularExpression?
        public let exact: String?
        public let exitCode: Int32?

        public init(
            contains: [String]? = nil,
            matches: NSRegularExpression? = nil,
            exact: String? = nil,
            exitCode: Int32? = nil
        ) {
            self.contains = contains
            self.matches = matches
            self.exact = exact
            self.exitCode = exitCode
        }
    }

    // MARK: - Properties

    private var sessionId: String
    private let tempDirectory: URL
    private let fixturesDirectory: URL

    // MARK: - Initialization

    public init(sessionId: String = "test-session") {
        self.sessionId = sessionId

        // Create temp directory for test isolation
        let tempBase = FileManager.default.temporaryDirectory
        self.tempDirectory = tempBase.appendingPathComponent("ashell-tests-\(sessionId)")

        // Locate fixtures
        let bundle = Bundle.module
        self.fixturesDirectory = bundle.url(forResource: "fixtures", withExtension: nil)
            ?? tempBase.appendingPathComponent("fixtures")

        // Create directories
        try? FileManager.default.createDirectory(at: tempDirectory, withIntermediateDirectories: true)
    }

    deinit {
        // Cleanup temp directory
        try? FileManager.default.removeItem(at: tempDirectory)
    }

    // MARK: - Command Execution

    /// Execute a command and capture output
    public func run(_ command: String, timeout: TimeInterval = 30.0) async -> CommandResult {
        let startTime = Date()

        // Create pipes for capturing output
        var stdoutPipe: [Int32] = [0, 0]
        var stderrPipe: [Int32] = [0, 0]

        pipe(&stdoutPipe)
        pipe(&stderrPipe)

        // Convert to FILE* for ios_system
        let stdoutFile = fdopen(stdoutPipe[1], "w")
        let stderrFile = fdopen(stderrPipe[1], "w")

        // Switch to test session
        ios_switchSession(UnsafeRawPointer(sessionId as CFString))
        ios_setStreams(nil, stdoutFile, stderrFile)

        // Execute command
        let exitCode = ios_system(command)

        // Close write ends
        fclose(stdoutFile)
        fclose(stderrFile)

        // Read output
        var stdoutData = Data()
        var stderrData = Data()

        let stdoutRead = FileHandle(fileDescriptor: stdoutPipe[0])
        let stderrRead = FileHandle(fileDescriptor: stderrPipe[0])

        if let data = try? stdoutRead.readToEnd() {
            stdoutData = data
        }
        if let data = try? stderrRead.readToEnd() {
            stderrData = data
        }

        // Close read ends
        close(stdoutPipe[0])
        close(stderrPipe[0])

        let duration = Date().timeIntervalSince(startTime)

        return CommandResult(
            stdout: String(data: stdoutData, encoding: .utf8) ?? "",
            stderr: String(data: stderrData, encoding: .utf8) ?? "",
            exitCode: exitCode,
            duration: duration
        )
    }

    // MARK: - Verification

    /// Verify command result matches expectation
    public func verify(_ result: CommandResult, against expectation: OutputExpectation) -> [String] {
        var failures: [String] = []

        // Check exit code
        if let expectedExit = expectation.exitCode {
            if result.exitCode != expectedExit {
                failures.append("Exit code mismatch: expected \(expectedExit), got \(result.exitCode)")
            }
        }

        // Check contains
        if let contains = expectation.contains {
            for substring in contains {
                if !result.stdout.contains(substring) {
                    failures.append("Expected stdout to contain '\(substring)'")
                }
            }
        }

        // Check exact match
        if let exact = expectation.exact {
            if result.stdout.trimmingCharacters(in: .whitespacesAndNewlines) != exact {
                failures.append("Exact output mismatch")
            }
        }

        // Check regex match
        if let regex = expectation.matches {
            let range = NSRange(result.stdout.startIndex..., in: result.stdout)
            if regex.firstMatch(in: result.stdout, options: [], range: range) == nil {
                failures.append("Output does not match expected pattern")
            }
        }

        return failures
    }

    // MARK: - Fixtures

    /// Create a test file in the temp directory
    public func createFile(name: String, content: String) -> URL {
        let fileURL = tempDirectory.appendingPathComponent(name)
        try? content.write(to: fileURL, atomically: true, encoding: .utf8)
        return fileURL
    }

    /// Create a test directory structure
    public func createDirectory(name: String) -> URL {
        let dirURL = tempDirectory.appendingPathComponent(name)
        try? FileManager.default.createDirectory(at: dirURL, withIntermediateDirectories: true)
        return dirURL
    }

    /// Copy fixture to temp directory
    public func copyFixture(_ name: String) -> URL? {
        let source = fixturesDirectory.appendingPathComponent(name)
        let dest = tempDirectory.appendingPathComponent(name)

        do {
            try FileManager.default.copyItem(at: source, to: dest)
            return dest
        } catch {
            return nil
        }
    }

    // MARK: - Session Management

    /// Switch to a different test session
    public func switchSession(to newSessionId: String) {
        self.sessionId = newSessionId
        ios_switchSession(UnsafeRawPointer(newSessionId as CFString))
    }

    /// Set environment variable for current session
    public func setenv(_ name: String, _ value: String) {
        ios_setenv(name, value, 1)
    }

    /// Get environment variable
    public func getenv(_ name: String) -> String? {
        guard let value = ios_getenv(name) else { return nil }
        return String(cString: value)
    }

    /// Close a session and clean up its resources
    public func closeSession(_ sessionId: String) {
        // Close the session - ios_system will clean up resources
        ios_closeSession(UnsafeRawPointer(sessionId as CFString))

        // If we closed the current session, switch to a default session
        if self.sessionId == sessionId {
            self.sessionId = "default"
            ios_switchSession(UnsafeRawPointer("default" as CFString))
        }
    }
}

// MARK: - Test Fixtures

/// Pre-defined test fixtures for common scenarios
public enum TestFixtures {

    /// Create a simple text file
    public static func simpleTextFile() -> String {
        return "Hello, World!\nThis is a test file.\n"
    }

    /// Create a multi-line file for sorting
    public static func unsortedLines() -> String {
        return """zebra
apple
banana
cherry
apple
"""
    }

    /// Create a file with special characters
    public static func specialCharacters() -> String {
        return "File with \t tabs and \n newlines and spaces   \n"
    }

    /// Create a large file (1MB)
    public static func largeFile() -> String {
        return String(repeating: "Line of text content\n", count: 50000)
    }

    /// Expected sorted output
    public static func sortedLines() -> String {
        return """apple
apple
banana
cherry
zebra
"""
    }
}
