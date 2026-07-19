import XCTest
@testable import OrlixTestRunner

final class OrlixUpstreamTestOutputParserTests: XCTestCase {
    private let parser = OrlixUpstreamTestOutputParser()

    func testCoreutilsRunnerTimeoutMatchesUpstreamHarnessBudget() {
        XCTAssertEqual(OrlixUpstreamTestRunSpec.coreutils.timeout, 14_400)
    }

    func testAcceptsKselftestCompletionWithPassingTAP() throws {
        let output = """
        ORLIX-KSELFTEST-INIT
        TAP version 13
        1..2
        ok 1 procfs mounted for kselftest
        ok 2 installed Orlix kselftest list is readable
        ORLIX-KSELFTEST-END
        """

        XCTAssertNoThrow(try parser.validate(output, for: .kernel))
    }

    func testAcceptsMLibCCompletionWithPassingTAP() throws {
        let output = """
        ORLIX-MLIBC-TEST-INIT
        TAP version 13
        1..1
        ok 1 installed upstream mlibc test list is readable
        ORLIX-MLIBC-TEST-END
        """

        XCTAssertNoThrow(try parser.validate(output, for: .mlibc))
    }

    func testAcceptsCoreutilsSuiteOnlyWhenExecutionMatchesPackagedManifest() throws {
        let output = """
        ORLIX-COREUTILS-TEST-INIT
        ORLIX-COREUTILS-TEST-RUNNING 1 tests/example.sh
        ORLIX-COREUTILS-TEST-END failures=0 skips=0 total=1
        """

        XCTAssertNoThrow(
            try parser.validate(
                output,
                for: .coreutils,
                expectedCoreutilsManifest: ["1 tests/example.sh"]
            )
        )
    }

    func testRejectsCoreutilsSequenceThatDoesNotMatchPackagedManifest() {
        let output = """
        ORLIX-COREUTILS-TEST-INIT
        ORLIX-COREUTILS-TEST-RUNNING 1 tests/second.sh
        ORLIX-COREUTILS-TEST-RUNNING 2 tests/first.sh
        ORLIX-COREUTILS-TEST-END failures=0 skips=0 total=2
        """

        XCTAssertThrowsError(
            try parser.validate(
                output,
                for: .coreutils,
                expectedCoreutilsManifest: [
                    "1 tests/first.sh",
                    "2 tests/second.sh",
                ]
            )
        ) { error in
            XCTAssertEqual(
                error as? OrlixUpstreamTestRunError,
                .malformedUpstreamOutput(
                    "Coreutils execution does not match the packaged upstream manifest"
                )
            )
        }
    }

    func testRejectsUpstreamFailureMarkerBeforeCompletion() {
        let output = """
        TAP version 13
        1..1
        not ok 1 waitpid returns the forked child
        ORLIX-KSELFTEST-END
        """

        XCTAssertThrowsError(try parser.validate(output, for: .kernel)) { error in
            guard case let .upstreamFailure(line, outputTail) = error as? OrlixUpstreamTestRunError else {
                XCTFail("expected upstream failure, got \(error)")
                return
            }

            XCTAssertEqual(line, "not ok 1 waitpid returns the forked child")
            XCTAssertTrue(outputTail.contains("not ok 1 waitpid returns the forked child"))
        }
    }

    func testRejectsMissingCompletionMarker() {
        let output = """
        TAP version 13
        1..1
        ok 1 installed upstream mlibc test list is readable
        """

        XCTAssertThrowsError(try parser.validate(output, for: .mlibc)) { error in
            XCTAssertEqual(
                error as? OrlixUpstreamTestRunError,
                .missingCompletionMarker("ORLIX-MLIBC-TEST-END")
            )
        }
    }

    func testRejectsKernelPanicBeforeMissingMarker() {
        let output = """
        Kernel panic - not syncing: Attempted to kill init
        """

        XCTAssertThrowsError(try parser.validate(output, for: .kernel)) { error in
            XCTAssertEqual(
                error as? OrlixUpstreamTestRunError,
                .kernelPanic("Kernel panic", outputTail: output)
            )
        }
    }

    func testRejectsOutOfMemoryBeforeMissingMarker() {
        let output = """
        Out of memory: Killed process 42
        """

        XCTAssertThrowsError(try parser.validate(output, for: .coreutils)) { error in
            XCTAssertEqual(
                error as? OrlixUpstreamTestRunError,
                .oom("Out of memory")
            )
        }
    }

    func testRejectsHostCrashReportMarkers() {
        let output = """
        Incident Identifier: 00000000-0000-0000-0000-000000000000
        Exception Type: EXC_BAD_ACCESS
        """

        XCTAssertThrowsError(try parser.validate(output, for: .kernel)) { error in
            XCTAssertEqual(
                error as? OrlixUpstreamTestRunError,
                .crashReport("Incident Identifier:")
            )
        }
    }

    func testTerminalConditionWakesOnCompletionFatalMarkerOrUpstreamFailure() {
        XCTAssertTrue(
            parser.containsTerminalCondition(
                "ORLIX-KSELFTEST-END",
                for: .kernel
            )
        )
        XCTAssertTrue(
            parser.containsTerminalCondition(
                "panic: fatal exception",
                for: .kernel
            )
        )
        XCTAssertTrue(
            parser.containsTerminalCondition(
                "not ok 7 upstream behavior",
                for: .mlibc
            )
        )
        XCTAssertFalse(
            parser.containsTerminalCondition(
                "TAP version 13\n1..1\nok 1 still running",
                for: .mlibc
            )
        )
    }

    func testRejectsMalformedCoreutilsCompletion() {
        let output = """
        ORLIX-COREUTILS-TEST-END failures=0 total=1
        """

        XCTAssertThrowsError(try parser.validate(output, for: .coreutils)) { error in
            XCTAssertEqual(
                error as? OrlixUpstreamTestRunError,
                .malformedCoreutilsCompletion(
                    "ORLIX-COREUTILS-TEST-END failures=0 total=1"
                )
            )
        }
    }

    func testRejectsCoreutilsFailuresOrSkips() {
        let output = """
        ORLIX-COREUTILS-TEST-RUNNING 1 tests/example.sh
        ORLIX-COREUTILS-TEST-END failures=0 skips=1 total=1
        """

        XCTAssertThrowsError(
            try parser.validate(
                output,
                for: .coreutils,
                expectedCoreutilsManifest: ["1 tests/example.sh"]
            )
        ) { error in
            XCTAssertEqual(
                error as? OrlixUpstreamTestRunError,
                .coreutilsSummaryFailed(
                    failures: 0,
                    skips: 1,
                    total: 1,
                    expectedTotal: 1
                )
            )
        }
    }
}
