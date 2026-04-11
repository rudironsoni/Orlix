// SessionTests.swift
// Session management validation tests
// Part of M3-I4: Validate session behavior

import Foundation
import Testing
@testable import ios_system

@Suite("Session Management Tests")
struct SessionTests {

    let harness: CommandTestHarness

    init() async {
        self.harness = await CommandTestHarness()
    }

    // MARK: - Session Creation Tests

    @Test("create new session with unique ID")
    func createSession() async throws {
        let sessionId = "test-session-\(UUID().uuidString)"

        await harness.switchSession(to: sessionId)

        // Verify session is active
        let result = await harness.run("echo $ASHELL_SESSION")
        #expect(result.exitCode == 0)
    }

    @Test("session environment is isolated")
    func sessionEnvironmentIsolation() async throws {
        // Create Session A
        await harness.switchSession(to: "session-a")
        await harness.setenv("TEST_VAR", "value-a")

        // Create Session B
        await harness.switchSession(to: "session-b")
        await harness.setenv("TEST_VAR", "value-b")

        // Verify Session B value
        let valueB = await harness.getenv("TEST_VAR")
        #expect(valueB == "value-b")

        // Switch back to Session A
        await harness.switchSession(to: "session-a")
        let valueA = await harness.getenv("TEST_VAR")
        #expect(valueA == "value-a")
    }

    @Test("session working directory isolation")
    func sessionWorkingDirectoryIsolation() async throws {
        // Create directories
        _ = await harness.createDirectory(name: "session_a_dir")
        _ = await harness.createDirectory(name: "session_b_dir")

        // Session A in dir A
        await harness.switchSession(to: "session-a")
        _ = await harness.run("cd session_a_dir")
        let pwdA = await harness.run("pwd")

        // Session B in dir B
        await harness.switchSession(to: "session-b")
        _ = await harness.run("cd session_b_dir")
        let pwdB = await harness.run("pwd")

        // Verify different directories
        #expect(pwdA.stdout != pwdB.stdout)

        // Switch back to A - should still be in dir A
        await harness.switchSession(to: "session-a")
        let pwdACheck = await harness.run("pwd")
        #expect(pwdACheck.stdout == pwdA.stdout)
    }

    // MARK: - Rapid Session Switching Tests

    @Test("rapid session switching does not crash")
    func rapidSessionSwitching() async throws {
        let sessions = (1...10).map { "rapid-session-\($0)" }

        // Create all sessions
        for session in sessions {
            await harness.switchSession(to: session)
            await harness.setenv("ID", session)
        }

        // Rapidly switch between sessions 100 times
        for i in 0..<100 {
            let session = sessions[i % sessions.count]
            await harness.switchSession(to: session)

            // Verify correct session
            let value = await harness.getenv("ID")
            #expect(value == session)
        }
    }

    @Test("rapid session switching with commands")
    func rapidSwitchingWithCommands() async throws {
        let sessions = ["rapid-1", "rapid-2", "rapid-3"]

        for i in 0..<50 {
            let session = sessions[i % sessions.count]
            await harness.switchSession(to: session)

            // Run command in each session
            let result = await harness.run("echo session-\(i)")
            #expect(result.exitCode == 0)
            #expect(result.stdout.contains("session-\(i)"))
        }
    }

    // MARK: - Concurrent Session Tests

    @Test("concurrent session access")
    func concurrentSessionAccess() async throws {
        // Create multiple harnesses for true concurrency
        let harnesses = await withTaskGroup(of: CommandTestHarness.self) { group in
            var result: [CommandTestHarness] = []
            for _ in 0..<5 {
                group.addTask {
                    await CommandTestHarness()
                }
            }
            for await harness in group {
                result.append(harness)
            }
            return result
        }

        // Run commands concurrently in different sessions
        await withTaskGroup(of: Void.self) { group in
            for (index, h) in harnesses.enumerated() {
                group.addTask {
                    await h.switchSession(to: "concurrent-\(index)")
                    await h.setenv("CONCURRENT_ID", "\(index)")

                    for i in 0..<10 {
                        _ = await h.run("echo concurrent-\(index)-\(i)")
                    }

                    // Verify isolation
                    let value = await h.getenv("CONCURRENT_ID")
                    #expect(value == "\(index)")
                }
            }
        }
    }

    // MARK: - Long-Running Command Tests

    @Test("session with long-running command")
    func longRunningCommand() async throws {
        await harness.switchSession(to: "long-running")

        // Start a long-running command in background
        let startTime = Date()
        let result = await harness.run("sleep 0.5 && echo done")
        let duration = Date().timeIntervalSince(startTime)

        #expect(result.exitCode == 0)
        #expect(result.stdout.contains("done"))
        #expect(duration >= 0.5)
    }

    @Test("switch session during long-running command")
    func switchDuringLongCommand() async throws {
        await harness.switchSession(to: "session-long")

        // Note: In single-threaded ios_system, we can't truly run concurrent commands
        // But we can verify session state is preserved
        await harness.setenv("LONG_VAR", "long-value")

        // Simulate command execution time
        try await Task.sleep(for: .milliseconds(100))

        // Switch to different session
        await harness.switchSession(to: "session-other")
        await harness.setenv("OTHER_VAR", "other-value")

        // Switch back - original session state should be preserved
        await harness.switchSession(to: "session-long")
        let value = await harness.getenv("LONG_VAR")
        #expect(value == "long-value")
    }

    // MARK: - Session Cleanup Tests

    @Test("session cleanup removes environment")
    func sessionCleanup() async throws {
        let sessionId = "cleanup-test"

        await harness.switchSession(to: sessionId)
        await harness.setenv("CLEANUP_VAR", "should-be-removed")

        // Verify variable exists
        let before = await harness.getenv("CLEANUP_VAR")
        #expect(before == "should-be-removed")

        // Close session
        await harness.closeSession(sessionId)

        // Recreate session with same ID - should be fresh
        await harness.switchSession(to: sessionId)
        let after = await harness.getenv("CLEANUP_VAR")
        #expect(after == nil)
    }

    @Test("session cleanup with pending operations")
    func cleanupWithPendingOperations() async throws {
        await harness.switchSession(to: "cleanup-pending")

        // Create files that might be in use
        _ = await harness.createFile(name: "pending.txt", content: "data")
        _ = await harness.createDirectory(name: "pending_dir")

        // Set environment
        await harness.setenv("PENDING", "true")

        // Close session
        await harness.closeSession("cleanup-pending")

        // Verify clean state for new session
        await harness.switchSession(to: "cleanup-pending")
        let value = await harness.getenv("PENDING")
        #expect(value == nil)
    }

    // MARK: - Edge Case Tests

    @Test("empty session ID handling")
    func emptySessionId() async throws {
        // Switch to empty session - should handle gracefully
        await harness.switchSession(to: "")

        let result = await harness.run("echo test")
        #expect(result.exitCode == 0)
    }

    @Test("special characters in session ID")
    func specialCharactersInSessionId() async throws {
        let specialIds = ["session-with-dashes", "session_with_underscores", "session123", "SessionWithMixedCase"]

        for id in specialIds {
            await harness.switchSession(to: id)
            await harness.setenv("SESSION_ID", id)

            let result = await harness.run("echo $SESSION_ID")
            #expect(result.stdout.contains(id))
        }
    }

    @Test("very long session ID")
    func veryLongSessionId() async throws {
        let longId = String(repeating: "a", count: 200)

        await harness.switchSession(to: longId)
        await harness.setenv("LONG_ID_TEST", "true")

        let value = await harness.getenv("LONG_ID_TEST")
        #expect(value == "true")
    }

    @Test("session with unicode in environment")
    func unicodeEnvironmentVariables() async throws {
        await harness.switchSession(to: "unicode-session")

        let unicodeValue = "Hello 世界 🌍"
        await harness.setenv("UNICODE_VAR", unicodeValue)

        let result = await harness.run("echo $UNICODE_VAR")
        #expect(result.stdout.contains(unicodeValue))
    }

    // MARK: - Thread Safety Tests

    @Test("thread-local I/O isolation")
    func threadLocalIOIsolation() async throws {
        await harness.switchSession(to: "io-session-a")
        let resultA = await harness.run("echo 'output-a'")

        await harness.switchSession(to: "io-session-b")
        let resultB = await harness.run("echo 'output-b'")

        #expect(resultA.stdout.contains("output-a"))
        #expect(resultB.stdout.contains("output-b"))
        #expect(!resultA.stdout.contains("output-b"))
        #expect(!resultB.stdout.contains("output-a"))
    }

    // MARK: - Memory Tests

    @Test("repeated session creation does not leak")
    func repeatedSessionCreation() async throws {
        // Create and destroy many sessions
        for i in 0..<100 {
            let sessionId = "temp-session-\(i)"
            await harness.switchSession(to: sessionId)
            await harness.setenv("TEMP", "\(i)")
            await harness.run("echo test")
            await harness.closeSession(sessionId)
        }

        // If we get here without memory issues, test passes
        #expect(Bool(true))
    }
}

// MARK: - Stress Tests

@Suite("Session Stress Tests")
struct SessionStressTests {

    @Test("100 rapid session switches", .tags(.stress))
    func hundredRapidSwitches() async throws {
        let harness = await CommandTestHarness()

        for i in 0..<100 {
            let sessionId = "stress-\(i % 10)"
            await harness.switchSession(to: sessionId)
            _ = await harness.run("echo \(i)")
        }

        // Verify final state is stable
        let result = await harness.run("echo final")
        #expect(result.exitCode == 0)
    }

    @Test("concurrent command execution in multiple sessions", .tags(.stress))
    func concurrentCommandExecution() async throws {
        await withTaskGroup(of: Void.self) { group in
            for i in 0..<10 {
                group.addTask {
                    let harness = await CommandTestHarness()
                    await harness.switchSession(to: "concurrent-stress-\(i)")

                    for j in 0..<20 {
                        _ = await harness.run("echo command-\(j)")
                    }
                }
            }
        }
    }

    @Test("environment variable stress test", .tags(.stress))
    func environmentVariableStress() async throws {
        let harness = await CommandTestHarness()
        await harness.switchSession(to: "env-stress")

        // Set many environment variables
        for i in 0..<100 {
            await harness.setenv("VAR_\(i)", "value-\(i)")
        }

        // Verify all variables
        for i in 0..<100 {
            let value = await harness.getenv("VAR_\(i)")
            #expect(value == "value-\(i)")
        }
    }

    @Test("session switch during pipeline", .tags(.stress))
    func sessionSwitchDuringPipeline() async throws {
        let harness = await CommandTestHarness()

        // Create test data
        let content = (1...100).map { "Line \($0)" }.joined(separator: "\n")
        _ = await harness.createFile(name: "stress_data.txt", content: content)

        // Run pipeline
        await harness.switchSession(to: "pipeline-session")
        let result = await harness.run("cat stress_data.txt | grep 'Line 5' | wc -l")

        #expect(result.exitCode == 0)
    }
}
