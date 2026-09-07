import XCTest
@testable import OrlixOSTestApp

final class OrlixUpstreamTestOutputParserTests: XCTestCase {
    private let parser = OrlixUpstreamTestOutputParser()

    func testCoreutilsRunnerTimeoutMatchesUpstreamHarnessBudget() {
        XCTAssertEqual(OrlixUpstreamTestRunSpec.coreutils.timeout, 14_400)
    }

    func testFocusedOrlixTCTIAtomicMemoryDiagnosticUsesUpstreamKUnitParameter() {
        let spec = OrlixUpstreamTestRunSpec.kernelTCTIAtomicMemoryDiagnostic

        XCTAssertEqual(
            spec.kernelCommandLineSuffix,
            "kunit.filter_glob=orlix-tcti-atomic-memory"
        )
        XCTAssertEqual(spec.expectedKUnitSuite, "orlix-tcti-atomic-memory")
    }

    func testFocusedOrlixTCTIKthreadHandoffDiagnosticSelectsExactOuterSuite() {
        let spec = OrlixUpstreamTestRunSpec.kernelTCTIKthreadHandoffDiagnostic

        XCTAssertEqual(
            spec.kernelCommandLineSuffix,
            "kunit.filter_glob=orlix-tcti-kthread-handoff"
        )
        XCTAssertEqual(spec.expectedKUnitSuite, "orlix-tcti-kthread-handoff")
    }

    func testFocusedOrlixTCTIBaseLoadStoreDiagnosticSelectsExactOuterSuite() {
        let spec = OrlixUpstreamTestRunSpec.kernelTCTIBaseLoadStoreDiagnostic

        XCTAssertEqual(
            spec.kernelCommandLineSuffix,
            "kunit.filter_glob=orlix-tcti-base-load-store-source-bound " +
            "orlix.kselftest=boot_profile_contract"
        )
        XCTAssertEqual(spec.expectedKUnitSuite, "orlix-tcti-base-load-store-source-bound")
    }

    func testFocusedOrlixTCTIAdvsimdLoadStoreDiagnosticSelectsExactOuterSuite() {
        let spec = OrlixUpstreamTestRunSpec.kernelTCTIAdvsimdLoadStoreDiagnostic

        XCTAssertEqual(
            spec.kernelCommandLineSuffix,
            "kunit.filter_glob=orlix-tcti-advsimd-structure-source-bound " +
            "orlix.kselftest=boot_profile_contract"
        )
        XCTAssertEqual(spec.expectedKUnitSuite, "orlix-tcti-advsimd-structure-source-bound")
    }

    func testFocusedOrlixTCTIBaseAtomicDiagnosticSelectsExactOuterSuite() {
        let spec = OrlixUpstreamTestRunSpec.kernelTCTIBaseAtomicDiagnostic

        XCTAssertEqual(
            spec.kernelCommandLineSuffix,
            "kunit.filter_glob=orlix-tcti-base-atomic-source-bound " +
            "orlix.kselftest=boot_profile_contract"
        )
        XCTAssertEqual(spec.expectedKUnitSuite, "orlix-tcti-base-atomic-source-bound")
    }

    func testFocusedOrlixTCTIBaseAddSubDiagnosticSelectsExactOuterSuite() {
        let spec = OrlixUpstreamTestRunSpec.kernelTCTIBaseAddSubDiagnostic

        XCTAssertEqual(
            spec.kernelCommandLineSuffix,
            "kunit.filter_glob=orlix-tcti-base-add-sub-source-bound " +
            "orlix.kselftest=boot_profile_contract"
        )
        XCTAssertEqual(spec.expectedKUnitSuite, "orlix-tcti-base-add-sub-source-bound")
    }

    func testFocusedOrlixTCTIBaseControlFlowDiagnosticSelectsExactOuterSuite() {
        let spec = OrlixUpstreamTestRunSpec.kernelTCTIBaseControlFlowDiagnostic

        XCTAssertEqual(
            spec.kernelCommandLineSuffix,
            "kunit.filter_glob=orlix-tcti-base-control-flow-source-bound " +
            "orlix.kselftest=boot_profile_contract"
        )
        XCTAssertEqual(spec.expectedKUnitSuite, "orlix-tcti-base-control-flow-source-bound")
    }

    func testFocusedOrlixTCTIBaseExceptionsDiagnosticSelectsExactOuterSuite() {
        let spec = OrlixUpstreamTestRunSpec.kernelTCTIBaseExceptionsDiagnostic

        XCTAssertEqual(
            spec.kernelCommandLineSuffix,
            "kunit.filter_glob=orlix-tcti-base-exceptions-source-bound " +
            "orlix.kselftest=boot_profile_contract"
        )
        XCTAssertEqual(spec.expectedKUnitSuite, "orlix-tcti-base-exceptions-source-bound")
    }

    func testFocusedOrlixTCTIBaseConditionalDiagnosticSelectsExactOuterSuite() {
        let spec = OrlixUpstreamTestRunSpec.kernelTCTIBaseConditionalDiagnostic

        XCTAssertEqual(
            spec.kernelCommandLineSuffix,
            "kunit.filter_glob=orlix-tcti-base-conditional-source-bound " +
            "orlix.kselftest=boot_profile_contract"
        )
        XCTAssertEqual(spec.expectedKUnitSuite, "orlix-tcti-base-conditional-source-bound")
    }

    func testFocusedOrlixTCTIBaseBitfieldUnaryDiagnosticSelectsExactOuterSuite() {
        let spec = OrlixUpstreamTestRunSpec.kernelTCTIBaseBitfieldUnaryDiagnostic

        XCTAssertEqual(
            spec.kernelCommandLineSuffix,
            "kunit.filter_glob=orlix-tcti-base-bitfield-unary-source-bound " +
            "orlix.kselftest=boot_profile_contract"
        )
        XCTAssertEqual(spec.expectedKUnitSuite, "orlix-tcti-base-bitfield-unary-source-bound")
    }

    func testFocusedOrlixTCTIBaseMultiplyDivideDiagnosticSelectsExactOuterSuite() {
        let spec = OrlixUpstreamTestRunSpec.kernelTCTIBaseMultiplyDivideDiagnostic

        XCTAssertEqual(
            spec.kernelCommandLineSuffix,
            "kunit.filter_glob=orlix-tcti-base-multiply-divide-source-bound " +
            "orlix.kselftest=boot_profile_contract"
        )
        XCTAssertEqual(spec.expectedKUnitSuite, "orlix-tcti-base-multiply-divide-source-bound")
    }

    func testFocusedOrlixTCTIAdvSIMDCryptoDiagnosticSelectsExactOuterSuite() {
        let spec = OrlixUpstreamTestRunSpec.kernelTCTIAdvSIMDCryptoDiagnostic

        XCTAssertEqual(
            spec.kernelCommandLineSuffix,
            "kunit.filter_glob=orlix-tcti-advsimd-crypto-source-bound " +
            "orlix.kselftest=boot_profile_contract"
        )
        XCTAssertEqual(spec.expectedKUnitSuite, "orlix-tcti-advsimd-crypto-source-bound")
    }

    func testFocusedOrlixTCTIAdvSIMDPermuteMoveDiagnosticSelectsExactOuterSuite() {
        let spec = OrlixUpstreamTestRunSpec.kernelTCTIAdvSIMDPermuteMoveDiagnostic

        XCTAssertEqual(
            spec.kernelCommandLineSuffix,
            "kunit.filter_glob=orlix-tcti-advsimd-permute-move-source-bound " +
            "orlix.kselftest=boot_profile_contract"
        )
        XCTAssertEqual(spec.expectedKUnitSuite, "orlix-tcti-advsimd-permute-move-source-bound")
    }

    func testFocusedOrlixTCTIAdvSIMDIntegerDiagnosticSelectsExactOuterSuite() {
        let spec = OrlixUpstreamTestRunSpec.kernelTCTIAdvSIMDIntegerDiagnostic

        XCTAssertEqual(
            spec.kernelCommandLineSuffix,
            "kunit.filter_glob=orlix-tcti-advsimd-integer-source-bound " +
            "orlix.kselftest=boot_profile_contract"
        )
        XCTAssertEqual(spec.expectedKUnitSuite, "orlix-tcti-advsimd-integer-source-bound")
    }

    func testFocusedOrlixTCTIAdvSIMDFPDiagnosticSelectsExactOuterSuite() {
        let spec = OrlixUpstreamTestRunSpec.kernelTCTIAdvSIMDFPDiagnostic

        XCTAssertEqual(
            spec.kernelCommandLineSuffix,
            "kunit.filter_glob=orlix-tcti-advsimd-fp-source-bound " +
            "orlix.kselftest=boot_profile_contract"
        )
        XCTAssertEqual(spec.expectedKUnitSuite, "orlix-tcti-advsimd-fp-source-bound")
    }

    func testFocusedOrlixTCTIScalarFPDiagnosticSelectsExactOuterSuite() {
        let spec = OrlixUpstreamTestRunSpec.kernelTCTIScalarFPDiagnostic

        XCTAssertEqual(
            spec.kernelCommandLineSuffix,
            "kunit.filter_glob=orlix-tcti-scalar-fp-source-bound " +
            "orlix.kselftest=boot_profile_contract"
        )
        XCTAssertEqual(spec.expectedKUnitSuite, "orlix-tcti-scalar-fp-source-bound")
    }

    func testLiveTerminalOutputIsNotDuplicatedByRecentConsoleFallback() {
        XCTAssertEqual(
            OrlixUpstreamTestSessionRunner.combinedUpstreamOutput(
                terminal: "live terminal",
                console: "live terminal"
            ),
            "live terminal"
        )
        XCTAssertEqual(
            OrlixUpstreamTestSessionRunner.combinedUpstreamOutput(
                terminal: "",
                console: "early console"
            ),
            "early console"
        )
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

    func testKselftestIgnoresBootTimeKUnitFailuresUntilTAP() throws {
        let output = """
        KTAP version 1
        1..1
            KTAP version 1
            # Subtest: orlix-tcti-decode
            1..1
            not ok 10 orlix_tcti_configured_profile_leaf_conditions_are_exact
        not ok 1 orlix-tcti-decode
        ORLIX-KSELFTEST-INIT
        TAP version 13
        1..2
        ok 1 procfs mounted for kselftest
        ok 2 installed Orlix kselftest list is readable
        ORLIX-KSELFTEST-END
        """

        XCTAssertFalse(
            parser.containsTerminalCondition(
                """
                KTAP version 1
                    not ok 10 orlix_tcti_configured_profile_leaf_conditions_are_exact
                not ok 1 orlix-tcti-decode
                """,
                for: .kernel
            )
        )
        XCTAssertNoThrow(try parser.validate(output, for: .kernel))
    }

    func testAcceptsSelectedKernelTestOnlyWhenItsPassingTAPResultIsPresent() throws {
        let output = """
        ORLIX-KSELFTEST-INIT
        TAP version 13
        1..1
        ok 1 - time_surface_probe
        ORLIX-KSELFTEST-END
        """

        XCTAssertNoThrow(try parser.validate(output, for: .kernelTimeSurface))
    }

    func testRejectsSelectedKernelTestWhenAnotherProbePassed() {
        let output = """
        ORLIX-KSELFTEST-INIT
        TAP version 13
        1..1
        ok 1 - mount_namespace_probe
        ORLIX-KSELFTEST-END
        """

        XCTAssertThrowsError(try parser.validate(output, for: .kernelTimeSurface)) { error in
            XCTAssertEqual(
                error as? OrlixUpstreamTestRunError,
                .malformedUpstreamOutput(
                    "missing passing TAP result for selected kernel test time_surface_probe"
                )
            )
        }
    }

    func testAcceptsFocusedKUnitSuiteWithPassingKTAPResult() throws {
        let output = """
        KTAP version 1
        1..1
            KTAP version 1
            # Subtest: orlix-tcti-atomic-memory
            1..1
            ok 1 orlix_tcti_atomic_memory_casp_no_tear_under_coordinated_threads
        ok 1 orlix-tcti-atomic-memory
        TAP version 13
        1..1
        ok 1 installed Orlix kselftest list is readable
        ORLIX-KSELFTEST-END
        """

        XCTAssertNoThrow(
            try parser.validate(output, for: .kernelTCTIAtomicMemoryDiagnostic)
        )
    }

    func testFocusedKUnitSuiteCompletesWithoutKselftestEnd() throws {
        let output = """
        KTAP version 1
        1..1
            KTAP version 1
            # Subtest: orlix-tcti-atomic-memory
            1..1
            ok 1 orlix_tcti_atomic_memory_casp_no_tear_under_coordinated_threads
        ok 1 orlix-tcti-atomic-memory
        """

        XCTAssertTrue(
            parser.containsTerminalCondition(
                output,
                for: .kernelTCTIAtomicMemoryDiagnostic
            )
        )
        XCTAssertFalse(parser.containsTerminalCondition(output, for: .kernel))
        XCTAssertNoThrow(
            try parser.validate(output, for: .kernelTCTIAtomicMemoryDiagnostic)
        )
    }

    func testRejectsFocusedKUnitSuiteWhenKTAPSubtestIsMissing() {
        let output = """
        KTAP version 1
        1..0
        TAP version 13
        1..1
        ok 1 installed Orlix kselftest list is readable
        ORLIX-KSELFTEST-END
        """

        XCTAssertThrowsError(
            try parser.validate(output, for: .kernelTCTIAtomicMemoryDiagnostic)
        ) { error in
            XCTAssertEqual(
                error as? OrlixUpstreamTestRunError,
                .malformedUpstreamOutput(
                    "missing passing KTAP result for selected KUnit suite orlix-tcti-atomic-memory"
                )
            )
        }
    }

    func testRejectsFocusedKUnitSuiteWithFailingKTAPResult() {
        let output = """
        KTAP version 1
        1..1
            KTAP version 1
            # Subtest: orlix-tcti-atomic-memory
            1..1
            not ok 1 orlix_tcti_atomic_memory_casp_no_tear_under_coordinated_threads
        not ok 1 orlix-tcti-atomic-memory
        TAP version 13
        1..1
        ok 1 installed Orlix kselftest list is readable
        ORLIX-KSELFTEST-END
        """

        XCTAssertThrowsError(
            try parser.validate(output, for: .kernelTCTIAtomicMemoryDiagnostic)
        ) { error in
            XCTAssertEqual(
                error as? OrlixUpstreamTestRunError,
                .malformedUpstreamOutput(
                    "missing passing KTAP result for selected KUnit suite orlix-tcti-atomic-memory"
                )
            )
        }
    }

    func testRejectsFocusedKUnitSuiteWithSkippedKTAPResult() {
        let output = """
        KTAP version 1
        1..1
            KTAP version 1
            # Subtest: orlix-tcti-atomic-memory
            1..1
            ok 1 orlix_tcti_atomic_memory_casp_no_tear_under_coordinated_threads
        ok 1 orlix-tcti-atomic-memory # SKIP filtered
        TAP version 13
        1..1
        ok 1 installed Orlix kselftest list is readable
        ORLIX-KSELFTEST-END
        """

        XCTAssertThrowsError(
            try parser.validate(output, for: .kernelTCTIAtomicMemoryDiagnostic)
        ) { error in
            XCTAssertEqual(
                error as? OrlixUpstreamTestRunError,
                .malformedUpstreamOutput(
                    "missing passing KTAP result for selected KUnit suite orlix-tcti-atomic-memory"
                )
            )
        }
    }

    func testRejectsFocusedKUnitSuiteWhenAnotherKTAPSuitePasses() {
        let output = """
        KTAP version 1
        1..1
            KTAP version 1
            # Subtest: another-kunit-suite
            1..1
            ok 1 another_case
        ok 1 another-kunit-suite
        TAP version 13
        1..1
        ok 1 installed Orlix kselftest list is readable
        ORLIX-KSELFTEST-END
        """

        XCTAssertThrowsError(
            try parser.validate(output, for: .kernelTCTIAtomicMemoryDiagnostic)
        ) { error in
            XCTAssertEqual(
                error as? OrlixUpstreamTestRunError,
                .malformedUpstreamOutput(
                    "missing passing KTAP result for selected KUnit suite orlix-tcti-atomic-memory"
                )
            )
        }
    }

    func testRejectsFocusedKUnitSuiteWhenOnlyAnIndentedCaseMatchesSuiteName() {
        let output = """
        KTAP version 1
        1..1
            KTAP version 1
            # Subtest: orlix-tcti-atomic-memory
            1..1
            ok 1 orlix-tcti-atomic-memory
        ok 1 another-kunit-suite
        TAP version 13
        1..1
        ok 1 installed Orlix kselftest list is readable
        ORLIX-KSELFTEST-END
        """

        XCTAssertThrowsError(
            try parser.validate(output, for: .kernelTCTIAtomicMemoryDiagnostic)
        ) { error in
            XCTAssertEqual(
                error as? OrlixUpstreamTestRunError,
                .malformedUpstreamOutput(
                    "missing passing KTAP result for selected KUnit suite orlix-tcti-atomic-memory"
                )
            )
        }
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

    func testFocusedMLibCFopenSelectsAnsiFopen() {
        XCTAssertEqual(
            OrlixUpstreamTestRunSpec.mlibcFopen.kernelCommandLineSuffix,
            "orlix.mlibc=ansi/fopen"
        )
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
        XCTAssertFalse(
            parser.containsTerminalCondition(
                "not ok 7 upstream behavior",
                for: .mlibc
            )
        )
        XCTAssertTrue(
            parser.containsTerminalCondition(
                "ORLIX-MLIBC-TEST-END",
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
