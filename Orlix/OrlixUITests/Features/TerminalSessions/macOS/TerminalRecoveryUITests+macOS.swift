#if os(macOS)
import XCTest

final class TerminalRecoveryUITests: XCTestCase {
    @MainActor
    func testEightHourWakeReachesOneConnectedReplacement() {
        let app = launchHarness()

        XCTAssertTrue(
            app.otherElements["orlix.macRecovery.connected"].waitForExistence(timeout: 2)
        )
        XCTAssertTrue(app.staticTexts["simulatedSleepHours=8"].exists)
    }

    @MainActor
    func testBlockedReplacementShowsActionableFailure() {
        let app = launchHarness(arguments: [
            "--orlix-ui-test-mac-terminal-recovery-failure",
        ])

        XCTAssertTrue(
            app.otherElements["orlix.macRecovery.failed"].waitForExistence(timeout: 2)
        )
        XCTAssertTrue(app.buttons["orlix.macRecovery.retry"].exists)
    }

    @MainActor
    private func launchHarness(arguments: [String] = []) -> XCUIApplication {
        let app = XCUIApplication()
        app.launchArguments = [
            "--orlix-ui-testing",
            "--orlix-ui-test-mac-terminal-recovery-harness",
        ] + arguments
        app.launch()
        return app
    }
}
#endif
