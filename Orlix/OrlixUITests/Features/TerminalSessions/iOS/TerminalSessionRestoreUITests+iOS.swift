#if os(iOS)
import XCTest

final class TerminalSessionRestoreUITests: TerminalReconnectUITestCase {
    @MainActor
    func testColdRelaunchRestoresTabsSplitsSelectionAndReconnects() throws {
        let app = XCUIApplication()
        app.terminate()
        let commonArguments = [
            "--orlix-ui-test-terminal-reconnect-harness",
            "--orlix-ui-test-cold-relaunch",
            "-AppleLanguages", "(en)",
            "-AppleLocale", "en_US",
            "-hasSeenWelcome", "YES",
            "-iCloudSyncEnabled", "NO",
            "-sshAutoReconnect", "YES",
            "-terminalTmuxEnabledDefault", "NO",
            "-terminalUsePerAppearanceTheme", "NO",
            "-terminalThemeName", "Orlix Dark",
            "-security.privacyModeEnabled", "NO",
            "-security.fullAppLockEnabled", "NO",
            "-security.lockOnBackground", "NO",
        ]
        app.launchArguments = commonArguments + ["--orlix-ui-test-seed-cold-relaunch"]
        app.launch()
        defer { app.terminate() }

        let connectionDiagnostics = app.staticTexts["orlix.reconnectTest.diagnostics"]
        XCTAssertTrue(connectionDiagnostics.waitForExistence(timeout: 45))
        try requireConfiguredLoopbackSSHFixture(
            diagnostics: connectionDiagnostics,
            app: app
        )
        let relaunchDiagnostics = app.staticTexts["orlix.coldRelaunchTest.diagnostics"]
        XCTAssertTrue(relaunchDiagnostics.waitForExistence(timeout: 45))
        wait(for: relaunchDiagnostics, containing: "tabs=2 panes=3", timeout: 15, app: app)
        let selectedTab = try XCTUnwrap(
            diagnosticValue("selected", in: relaunchDiagnostics),
            "Missing selected tab before relaunch"
        )
        let focusedPane = try XCTUnwrap(
            diagnosticValue("focused", in: relaunchDiagnostics),
            "Missing focused pane before relaunch"
        )
        RunLoop.current.run(until: Date().addingTimeInterval(1))

        app.terminate()
        app.launchArguments = commonArguments
        app.launch()

        XCTAssertTrue(relaunchDiagnostics.waitForExistence(timeout: 45))
        wait(for: relaunchDiagnostics, containing: "tabs=2 panes=3", timeout: 15, app: app)
        XCTAssertEqual(diagnosticValue("selected", in: relaunchDiagnostics), selectedTab)
        XCTAssertEqual(diagnosticValue("focused", in: relaunchDiagnostics), focusedPane)

        XCTAssertTrue(connectionDiagnostics.waitForExistence(timeout: 10))
        wait(
            for: connectionDiagnostics,
            containing: "setup=ready state=connected",
            timeout: 45,
            app: app
        )
    }

}
#endif
