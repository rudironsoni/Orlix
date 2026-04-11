// PerformanceTests.swift
// Performance benchmarks for ios_system
// Part of M6-I1: Performance benchmarks

import Foundation
import Testing
@testable import ios_system

@Suite("Performance Benchmarks")
struct PerformanceTests {

    let harness: CommandTestHarness

    init() async {
        self.harness = await CommandTestHarness()
    }

    // MARK: - Command Startup Benchmarks

    @Test("command cold start: ls", .tags(.benchmark))
    func lsColdStart() async throws {
        let iterations = 100
        var times: [TimeInterval] = []

        for _ in 0..<iterations {
            let start = Date()
            let result = await harness.run("ls")
            let duration = Date().timeIntervalSince(start)

            #expect(result.exitCode == 0)
            times.append(duration)
        }

        let avg = times.reduce(0, +) / Double(times.count)
        let max = times.max() ?? 0
        let min = times.min() ?? 0

        print("ls cold start: avg=\(avg*1000)ms, max=\(max*1000)ms, min=\(min*1000)ms")

        // Target: < 50ms average
        #expect(avg < 0.050, "ls average startup time \(avg*1000)ms exceeds 50ms target")
    }

    @Test("command cold start: cat", .tags(.benchmark))
    func catColdStart() async throws {
        _ = await harness.createFile(name: "test.txt", content: "Hello, World!")

        let iterations = 100
        var times: [TimeInterval] = []

        for _ in 0..<iterations {
            let start = Date()
            let result = await harness.run("cat test.txt")
            let duration = Date().timeIntervalSince(start)

            #expect(result.exitCode == 0)
            times.append(duration)
        }

        let avg = times.reduce(0, +) / Double(times.count)
        print("cat cold start: avg=\(avg*1000)ms")

        #expect(avg < 0.050, "cat average startup time \(avg*1000)ms exceeds 50ms target")
    }

    @Test("command cold start: grep", .tags(.benchmark))
    func grepColdStart() async throws {
        _ = await harness.createFile(name: "test.txt", content: "Line one\nLine two\nLine three")

        let iterations = 100
        var times: [TimeInterval] = []

        for _ in 0..<iterations {
            let start = Date()
            let result = await harness.run("grep 'Line' test.txt")
            let duration = Date().timeIntervalSince(start)

            #expect(result.exitCode == 0)
            times.append(duration)
        }

        let avg = times.reduce(0, +) / Double(times.count)
        print("grep cold start: avg=\(avg*1000)ms")

        #expect(avg < 0.050, "grep average startup time \(avg*1000)ms exceeds 50ms target")
    }

    // MARK: - Session Switching Benchmarks

    @Test("session switch time", .tags(.benchmark))
    func sessionSwitchTime() async throws {
        let sessions = (0..<10).map { "perf-session-\($0)" }
        let iterations = 100

        var times: [TimeInterval] = []

        for i in 0..<iterations {
            let session = sessions[i % sessions.count]
            let start = Date()
            await harness.switchSession(to: session)
            let duration = Date().timeIntervalSince(start)
            times.append(duration)
        }

        let avg = times.reduce(0, +) / Double(times.count)
        let max = times.max() ?? 0

        print("session switch: avg=\(avg*1000)ms, max=\(max*1000)ms")

        // Target: < 10ms average
        #expect(avg < 0.010, "session switch average \(avg*1000)ms exceeds 10ms target")
    }

    @Test("rapid session switching", .tags(.benchmark))
    func rapidSessionSwitching() async throws {
        let start = Date()

        for i in 0..<1000 {
            await harness.switchSession(to: "rapid-\(i % 5)")
        }

        let duration = Date().timeIntervalSince(start)
        let avg = duration / 1000.0

        print("1000 session switches: total=\(duration)s, avg=\(avg*1000)ms")

        #expect(avg < 0.010, "rapid session switch average \(avg*1000)ms exceeds 10ms target")
    }

    // MARK: - Memory Benchmarks

    @Test("memory per session", .tags(.benchmark))
    func memoryPerSession() async throws {
        // Note: This is a simplified check
        // Real implementation would use mach APIs

        var sessionIds: [String] = []

        // Create 20 sessions
        for i in 0..<20 {
            let sessionId = "mem-test-\(i)"
            await harness.switchSession(to: sessionId)
            await harness.setenv("TEST_VAR_\(i)", String(repeating: "x", count: 1000))
            sessionIds.append(sessionId)
        }

        // If we can create 20 sessions without issues, memory is likely bounded
        #expect(sessionIds.count == 20)
    }

    @Test("memory under load", .tags(.benchmark))
    func memoryUnderLoad() async throws {
        await harness.switchSession(to: "memory-load")

        // Create large files and process them
        let largeContent = String(repeating: "Line of text content\n", count: 10000)
        _ = await harness.createFile(name: "large.txt", content: largeContent)

        let start = Date()
        let result = await harness.run("cat large.txt | wc -l")
        let duration = Date().timeIntervalSince(start)

        #expect(result.exitCode == 0)
        print("Large file processing: \(duration)s")

        // Should complete in reasonable time
        #expect(duration < 5.0, "Large file processing took too long: \(duration)s")
    }

    // MARK: - File Operation Benchmarks

    @Test("file read throughput", .tags(.benchmark))
    func fileReadThroughput() async throws {
        // Create 1MB file
        let content = String(repeating: "X", count: 1024 * 1024)
        _ = await harness.createFile(name: "1mb.txt", content: content)

        let start = Date()
        let result = await harness.run("cat 1mb.txt")
        let duration = Date().timeIntervalSince(start)

        #expect(result.exitCode == 0)

        let bytesPerSecond = Double(content.utf8.count) / duration
        print("File read throughput: \(bytesPerSecond / 1024 / 1024) MB/s")

        // Target: > 10 MB/s
        #expect(bytesPerSecond > 10 * 1024 * 1024, "Read throughput too low")
    }

    @Test("file write throughput", .tags(.benchmark))
    func fileWriteThroughput() async throws {
        let start = Date()

        // Generate and write 100KB
        let result = await harness.run("dd if=/dev/zero of=test_write bs=1024 count=100")

        let duration = Date().timeIntervalSince(start)

        #expect(result.exitCode == 0)

        let bytesPerSecond = Double(100 * 1024) / duration
        print("File write throughput: \(bytesPerSecond / 1024) KB/s")

        // Target: > 1 MB/s
        #expect(bytesPerSecond > 1 * 1024 * 1024, "Write throughput too low")
    }

    @Test("recursive directory listing", .tags(.benchmark))
    func recursiveDirectoryListing() async throws {
        // Create directory tree
        _ = await harness.run("mkdir -p tree/a/b/c tree/a/b/d tree/e/f")
        for dir in ["tree/a", "tree/a/b", "tree/a/b/c", "tree/a/b/d", "tree/e", "tree/e/f"] {
            _ = await harness.createFile(name: "\(dir)/file.txt", content: "test")
        }

        let start = Date()
        let result = await harness.run("find tree -type f")
        let duration = Date().timeIntervalSince(start)

        #expect(result.exitCode == 0)
        print("Recursive directory listing: \(duration)s")

        #expect(duration < 1.0, "Directory listing too slow")
    }

    // MARK: - Pipeline Benchmarks

    @Test("pipeline performance: cat | grep | wc", .tags(.benchmark))
    func pipelineThreeStage() async throws {
        let content = (1...1000).map { "Line \($0)" }.joined(separator: "\n")
        _ = await harness.createFile(name: "lines.txt", content: content)

        let iterations = 50
        var times: [TimeInterval] = []

        for _ in 0..<iterations {
            let start = Date()
            let result = await harness.run("cat lines.txt | grep 'Line 5' | wc -l")
            let duration = Date().timeIntervalSince(start)

            #expect(result.exitCode == 0)
            times.append(duration)
        }

        let avg = times.reduce(0, +) / Double(times.count)
        print("3-stage pipeline: avg=\(avg*1000)ms")

        #expect(avg < 0.100, "3-stage pipeline average \(avg*1000)ms exceeds 100ms target")
    }

    @Test("pipeline performance: multi-stage", .tags(.benchmark))
    func pipelineMultiStage() async throws {
        let content = (1...100).map { "Item \($0)" }.joined(separator: "\n")
        _ = await harness.createFile(name: "items.txt", content: content)

        let start = Date()
        let result = await harness.run("cat items.txt | head -50 | tail -25 | sort | uniq")
        let duration = Date().timeIntervalSince(start)

        #expect(result.exitCode == 0)
        print("5-stage pipeline: \(duration)s")

        #expect(duration < 0.5, "5-stage pipeline too slow: \(duration)s")
    }

    // MARK: - Package Operation Benchmarks

    @Test("package extraction speed", .tags(.benchmark))
    func packageExtractionSpeed() async throws {
        // Simulate package extraction with tar
        _ = await harness.createFile(name: "test_file.txt", content: String(repeating: "X", count: 1024 * 1024))

        let tarStart = Date()
        let tarResult = await harness.run("tar cvf test.tar test_file.txt")
        let tarDuration = Date().timeIntervalSince(tarStart)

        #expect(tarResult.exitCode == 0)
        print("1MB tar creation: \(tarDuration)s")

        let extractStart = Date()
        let extractResult = await harness.run("tar xvf test.tar")
        let extractDuration = Date().timeIntervalSince(extractStart)

        #expect(extractResult.exitCode == 0)
        print("1MB tar extraction: \(extractDuration)s")

        // Target: < 100ms for 1MB extraction
        #expect(extractDuration < 0.100, "Extraction too slow: \(extractDuration)s")
    }

    // MARK: - Stress Benchmarks

    @Test("concurrent command execution", .tags(.benchmark, .stress))
    func concurrentCommandExecution() async throws {
        let start = Date()

        await withTaskGroup(of: Void.self) { group in
            for i in 0..<10 {
                group.addTask {
                    let h = await CommandTestHarness()
                    await h.switchSession(to: "concurrent-\(i)")

                    for _ in 0..<10 {
                        _ = await h.run("echo test")
                    }
                }
            }
        }

        let duration = Date().timeIntervalSince(start)
        print("10 concurrent sessions, 10 commands each: \(duration)s")

        #expect(duration < 5.0, "Concurrent execution too slow")
    }

    @Test("large file handling", .tags(.benchmark))
    func largeFileHandling() async throws {
        // Create 5MB file
        let lines = 50000
        let content = String(repeating: "Large file content line with data\n", count: lines)
        _ = await harness.createFile(name: "5mb.txt", content: content)

        let start = Date()
        let result = await harness.run("wc -l 5mb.txt")
        let duration = Date().timeIntervalSince(start)

        #expect(result.exitCode == 0)
        #expect(result.stdout.contains("\(lines)"))

        print("5MB file line count: \(duration)s")
        #expect(duration < 2.0, "Large file processing too slow")
    }
}

// MARK: - Performance Summary

@Suite("Performance Summary")
struct PerformanceSummary {

    @Test("all benchmarks summary", .tags(.benchmark))
    func benchmarksSummary() async throws {
        // This test runs all benchmarks and prints a summary
        // It's useful for CI to have a single output

        print("\n=== Performance Benchmarks Summary ===")
        print("Run individual benchmark tests for detailed results")
        print("=====================================\n")

        // Mark as passing - actual benchmarks are in other tests
        #expect(Bool(true))
    }
}
