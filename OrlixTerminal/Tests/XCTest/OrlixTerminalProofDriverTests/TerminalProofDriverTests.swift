import XCTest
@testable import OrlixTerminal

final class TerminalProofDriverTests: XCTestCase {
    func testCreatesDriverOnlyForShellPackageProofArgument() {
        XCTAssertNil(TerminalProofDriver.fromArguments(["OrlixTerminal"]) { _ in })
        XCTAssertNotNil(
            TerminalProofDriver.fromArguments(
                ["OrlixTerminal", "--orlix-terminal-proof=shell-package"]
            ) { _ in }
        )
    }

    func testStartsProofWhenShellPromptArrivesAcrossConsoleChunks() {
        var sentInputs: [String] = []
        var logLines: [String] = []
        let driver = TerminalProofDriver(
            sendInput: { data in
                sentInputs.append(String(decoding: data, as: UTF8.self))
            },
            log: { line in
                logLines.append(line)
            }
        )

        driver.receive("Linux boot output\nsh-")
        driver.receive("5.3# ")

        XCTAssertEqual(logLines.first, "ORLIX-TERMINAL-PROOF-BEGIN shell-package")
        XCTAssertEqual(sentInputs.count, 1)
        XCTAssertTrue(sentInputs.first?.hasSuffix("\r") == true)
    }

    func testLogsEachShellPackageStepAndNamedEndMarker() {
        var sentInputs: [String] = []
        var logLines: [String] = []
        let driver = TerminalProofDriver(
            sendInput: { data in
                sentInputs.append(String(decoding: data, as: UTF8.self))
            },
            log: { line in
                logLines.append(line)
            }
        )
        let expectedSteps = [
            ("echo", "ORLIX_ECHO"),
            ("pwd", "PWD_OK"),
            ("cd", "CD_OK"),
            ("redirection", "REDIR_OK"),
            ("pipe", "PIPE_OK"),
            ("fork-exec-wait-status", "status:7"),
            ("signal-trap", "SIG_OK"),
            ("ls", "LS_OK"),
            ("ls-dot", "LS_DOT_OK"),
            ("cp-mv-ln", "COREUTILS_FILE"),
            ("sort-tail", "beta"),
            ("sleep-date", "DATE_OK"),
        ]

        driver.receive("sh-5.3# ")

        XCTAssertEqual(
            logLines,
            ["ORLIX-TERMINAL-PROOF-BEGIN shell-package"]
        )
        for (index, step) in expectedSteps.enumerated() {
            XCTAssertEqual(sentInputs.count, index + 1)
            driver.receive("\(step.1)\nsh-5.3# ")
            XCTAssertTrue(
                logLines.contains("ORLIX-TERMINAL-PROOF-OK \(step.0)")
            )
        }

        XCTAssertEqual(
            logLines.last,
            "ORLIX-TERMINAL-PROOF-END shell-package status=0"
        )
        XCTAssertEqual(sentInputs.count, expectedSteps.count)
    }

    func testMatchesCoreutilsOutputWithAnsiSequences() {
        var sentInputs: [String] = []
        var logLines: [String] = []
        let driver = TerminalProofDriver(
            sendInput: { data in
                sentInputs.append(String(decoding: data, as: UTF8.self))
            },
            log: { line in
                logLines.append(line)
            }
        )
        let preLsOutputs = [
            "ORLIX_ECHO",
            "PWD_OK",
            "CD_OK",
            "REDIR_OK",
            "PIPE_OK",
            "status:7",
            "SIG_OK",
        ]

        driver.receive("sh-5.3# ")
        for output in preLsOutputs {
            driver.receive("\(output)\nsh-5.3# ")
        }

        XCTAssertTrue(sentInputs.last?.contains("/bin/ls /bin") == true)
        driver.receive(
            "\u{1b}[1;39mLS_OK\u{1b}[0m\r\nsh-5.3# "
        )

        XCTAssertTrue(logLines.contains("ORLIX-TERMINAL-PROOF-OK ls"))
        XCTAssertTrue(sentInputs.last?.contains("/bin/ls .") == true)
    }

    func testProofIncludesExternalCoreutilsFileCommands() {
        var sentInputs: [String] = []
        let driver = TerminalProofDriver(
            sendInput: { data in
                sentInputs.append(String(decoding: data, as: UTF8.self))
            },
            log: { _ in }
        )
        let outputsBeforeZshLs = [
            "ORLIX_ECHO",
            "PWD_OK",
            "CD_OK",
            "REDIR_OK",
            "PIPE_OK",
            "status:7",
            "SIG_OK",
            "LS_OK",
            "LS_DOT_OK",
        ]

        driver.receive("sh-5.3# ")
        for output in outputsBeforeZshLs {
            driver.receive("\(output)\nsh-5.3# ")
        }

        XCTAssertTrue(sentInputs.last?.contains("/bin/cp") == true)
        XCTAssertTrue(sentInputs.last?.contains("/bin/mv") == true)
        XCTAssertTrue(sentInputs.last?.contains("/bin/ln") == true)
        XCTAssertFalse(sentInputs.last?.contains("COREUTILS_FILE") == true)
    }

    func testFailsWhenPromptReturnsWithoutExpectedOutput() {
        var logLines: [String] = []
        let driver = TerminalProofDriver(
            sendInput: { _ in },
            log: { line in
                logLines.append(line)
            }
        )

        driver.receive("sh-5.3# ")
        driver.receive("printf '\\117\\122\\114\\111\\130\\137\\105\\103\\110\\117\\012'\r\nsh-5.3# ")

        XCTAssertEqual(
            logLines.last,
            "ORLIX-TERMINAL-PROOF-FAIL echo reason=missing-output"
        )
    }
}
