#if os(iOS)
import XCTest

final class TerminalSettingsNavigationUITests: TerminalReconnectUITestCase {
    @MainActor
    func testServerListCanOpenSettingsFromItsToolbar() throws {
        let app = XCUIApplication()
        app.terminate()
        app.launchArguments = [
            "-AppleLanguages", "(en)",
            "-AppleLocale", "en_US",
            "-hasSeenWelcome", "YES",
            "-iCloudSyncEnabled", "NO",
            "-security.privacyModeEnabled", "NO",
            "-security.fullAppLockEnabled", "NO",
            "-security.lockOnBackground", "NO",
        ]
        app.launch()
        defer { app.terminate() }

        let settings = app.buttons["orlix.serverList.settings"]
        XCTAssertTrue(settings.waitForExistence(timeout: 10))
        settings.tap()

        XCTAssertTrue(
            app.descendants(matching: .any)["orlix.settings.root"]
                .waitForExistence(timeout: 8),
            "Settings did not open from the server list toolbar."
        )
    }

    @MainActor
    func testGroupedSettingsOpenGeneralAndTerminalPages() throws {
        let app = XCUIApplication()
        app.terminate()
        app.launchArguments = [
            "-AppleLanguages", "(en)",
            "-AppleLocale", "en_US",
            "-hasSeenWelcome", "YES",
            "-iCloudSyncEnabled", "NO",
            "-security.privacyModeEnabled", "NO",
            "-security.fullAppLockEnabled", "NO",
            "-security.lockOnBackground", "NO",
        ]
        app.launch()
        defer { app.terminate() }

        let settings = app.buttons["orlix.serverList.settings"]
        XCTAssertTrue(settings.waitForExistence(timeout: 10))
        settings.tap()

        let navigationRoute = app.buttons["orlix.settings.route.navigationAndStats"]
        XCTAssertTrue(navigationRoute.waitForExistence(timeout: 8))
        navigationRoute.tap()

        XCTAssertTrue(
            app.descendants(matching: .any)["orlix.settings.page.navigationAndStats"]
                .waitForExistence(timeout: 8),
            "The Server Views page did not open."
        )
        XCTAssertTrue(
            app.buttons["orlix.settings.navigationAndStats.statsAppearance"].exists
        )

        let settingsBackButton = app.navigationBars["Server Views"].buttons["Settings"]
        XCTAssertTrue(settingsBackButton.waitForExistence(timeout: 5))
        settingsBackButton.tap()

        let searchField = app.searchFields["Search Settings"]
        if !searchField.waitForExistence(timeout: 2) {
            app.swipeDown()
        }
        XCTAssertTrue(searchField.waitForExistence(timeout: 5))
        searchField.tap()
        searchField.typeText("tmux")

        let sessionsRoute = app.buttons["orlix.settings.route.sessionsAndConnections"]
        XCTAssertTrue(sessionsRoute.waitForExistence(timeout: 5))
        sessionsRoute.tap()

        XCTAssertTrue(
            app.descendants(matching: .any)["orlix.settings.page.sessionsAndConnections"]
                .waitForExistence(timeout: 8),
            "The Sessions & SSH page did not open."
        )
        XCTAssertTrue(app.switches["Use persistent sessions by default"].exists)
        XCTAssertTrue(app.switches["Keep screen awake"].exists)
    }

    @MainActor
    func testVoiceInputSettingsUseGroupedLayoutBelowNavigationBar() throws {
        let app = XCUIApplication()
        app.terminate()
        app.launchArguments = [
            "-AppleLanguages", "(en)",
            "-AppleLocale", "en_US",
            "-hasSeenWelcome", "YES",
            "-iCloudSyncEnabled", "NO",
            "-security.privacyModeEnabled", "NO",
            "-security.fullAppLockEnabled", "NO",
            "-security.lockOnBackground", "NO",
            "-transcriptionProvider", "mlxWhisper",
        ]
        app.launch()
        defer { app.terminate() }

        let settings = app.buttons["orlix.serverList.settings"]
        XCTAssertTrue(settings.waitForExistence(timeout: 10))
        settings.tap()

        let voiceInputRoute = app.buttons["orlix.settings.route.transcription"]
        XCTAssertTrue(voiceInputRoute.waitForExistence(timeout: 8))
        voiceInputRoute.tap()

        XCTAssertTrue(
            app.descendants(matching: .any)["orlix.settings.page.transcription"]
                .waitForExistence(timeout: 8)
        )

        let navigationBar = app.navigationBars["Voice Input"]
        let accessHeader = app.staticTexts["Access"]
        XCTAssertTrue(navigationBar.exists)
        XCTAssertTrue(accessHeader.exists)
        XCTAssertGreaterThanOrEqual(
            accessHeader.frame.minY,
            navigationBar.frame.maxY,
            "The first section heading must not be clipped by the navigation bar."
        )
        XCTAssertTrue(app.switches["Show in Terminal"].exists)
        XCTAssertTrue(app.staticTexts["Transcription"].exists)
    }

    @MainActor
    func testAppearanceAndCursorChoicesAreSelectedButtons() throws {
        let app = XCUIApplication()
        app.terminate()
        app.launchArguments = [
            "-AppleLanguages", "(en)",
            "-AppleLocale", "en_US",
            "-hasSeenWelcome", "YES",
            "-iCloudSyncEnabled", "NO",
            "-security.privacyModeEnabled", "NO",
            "-security.fullAppLockEnabled", "NO",
            "-security.lockOnBackground", "NO",
        ]
        app.launch()
        defer { app.terminate() }

        let settings = app.buttons["orlix.serverList.settings"]
        XCTAssertTrue(settings.waitForExistence(timeout: 10))
        settings.tap()

        let appearanceRoute = app.buttons["orlix.settings.route.appearanceAndLanguage"]
        XCTAssertTrue(appearanceRoute.waitForExistence(timeout: 8))
        appearanceRoute.tap()

        let systemAppearance = app.buttons["orlix.settings.appearance.system"]
        let lightAppearance = app.buttons["orlix.settings.appearance.light"]
        let darkAppearance = app.buttons["orlix.settings.appearance.dark"]
        XCTAssertTrue(systemAppearance.waitForExistence(timeout: 5))
        XCTAssertEqual([systemAppearance, lightAppearance, darkAppearance].filter(\.isSelected).count, 1)
        let appearanceTarget = darkAppearance.isSelected ? lightAppearance : darkAppearance
        appearanceTarget.tap()
        XCTAssertTrue(appearanceTarget.isSelected)

        app.navigationBars["Appearance & Language"].buttons["Settings"].tap()

        let terminalAppearanceRoute = app.buttons["orlix.settings.route.terminalAppearance"]
        XCTAssertTrue(terminalAppearanceRoute.waitForExistence(timeout: 5))
        terminalAppearanceRoute.tap()

        let blockCursor = app.buttons["orlix.settings.cursor.block"]
        let barCursor = app.buttons["orlix.settings.cursor.bar"]
        XCTAssertTrue(blockCursor.waitForExistence(timeout: 5))
        let underlineCursor = app.buttons["orlix.settings.cursor.underline"]
        let hollowCursor = app.buttons["orlix.settings.cursor.block_hollow"]
        let cursorButtons = [blockCursor, barCursor, underlineCursor, hollowCursor]
        XCTAssertTrue(hollowCursor.waitForExistence(timeout: 5))
        XCTAssertEqual(cursorButtons.filter(\.isSelected).count, 1)
        XCTAssertTrue(
            cursorButtons.allSatisfy { abs($0.frame.midY - blockCursor.frame.midY) < 2 },
            "All four cursor types must remain in one row."
        )
        let cursorTarget = barCursor.isSelected ? blockCursor : barCursor
        cursorTarget.tap()
        XCTAssertTrue(cursorTarget.isSelected)

        let customThemes = app.buttons["orlix.settings.appearance.customThemes"]
        if !customThemes.waitForExistence(timeout: 2) {
            app.swipeUp()
        }
        XCTAssertTrue(customThemes.waitForExistence(timeout: 5))
        customThemes.tap()

        XCTAssertTrue(
            app.descendants(matching: .any)["orlix.settings.customThemes.page"]
                .waitForExistence(timeout: 5)
        )
        let customThemesBack = app.navigationBars["Custom Themes"].buttons.firstMatch
        XCTAssertTrue(customThemesBack.exists)
        customThemesBack.tap()

        XCTAssertTrue(
            app.descendants(matching: .any)["orlix.settings.page.terminalAppearance"]
                .waitForExistence(timeout: 5),
            "Closing Custom Themes must return to Terminal Appearance."
        )
        XCTAssertTrue(
            app.descendants(matching: .any)["orlix.settings.root"].exists,
            "Closing Custom Themes must not dismiss Settings."
        )
    }

    @MainActor
    func testRemoteClipboardShowsOneWarning() throws {
        let app = XCUIApplication()
        app.terminate()
        app.launchArguments = [
            "-AppleLanguages", "(en)",
            "-AppleLocale", "en_US",
            "-hasSeenWelcome", "YES",
            "-terminalRemoteClipboardReadPolicy", "allow",
            "-iCloudSyncEnabled", "NO",
            "-security.privacyModeEnabled", "NO",
            "-security.fullAppLockEnabled", "NO",
            "-security.lockOnBackground", "NO",
        ]
        app.launch()
        defer { app.terminate() }

        let settings = app.buttons["orlix.serverList.settings"]
        XCTAssertTrue(settings.waitForExistence(timeout: 10))
        settings.tap()

        let clipboardRoute = app.buttons["orlix.settings.route.clipboardAndPaste"]
        XCTAssertTrue(clipboardRoute.waitForExistence(timeout: 8))
        clipboardRoute.tap()

        XCTAssertTrue(
            app.staticTexts["Warning: Remote programs can read your clipboard without asking."]
                .waitForExistence(timeout: 5)
        )
        XCTAssertFalse(
            app.staticTexts["Remote programs can read clipboard data without asking."].exists
        )
    }

    @MainActor
    func testKeyboardInputUsesFocusedSections() throws {
        let app = XCUIApplication()
        app.terminate()
        app.launchArguments = [
            "-AppleLanguages", "(en)",
            "-AppleLocale", "en_US",
            "-hasSeenWelcome", "YES",
            "-iCloudSyncEnabled", "NO",
            "-security.privacyModeEnabled", "NO",
            "-security.fullAppLockEnabled", "NO",
            "-security.lockOnBackground", "NO",
        ]
        app.launch()
        defer { app.terminate() }

        let settings = app.buttons["orlix.serverList.settings"]
        XCTAssertTrue(settings.waitForExistence(timeout: 10))
        settings.tap()

        let keyboardRoute = app.buttons["orlix.settings.route.keyboardAndInput"]
        if !keyboardRoute.waitForExistence(timeout: 3) {
            app.swipeUp()
        }
        XCTAssertTrue(keyboardRoute.waitForExistence(timeout: 5))
        keyboardRoute.tap()

        XCTAssertTrue(app.staticTexts["Hardware Keyboard"].waitForExistence(timeout: 5))
        XCTAssertTrue(app.staticTexts["Software Keyboard"].exists)
        XCTAssertTrue(app.staticTexts["Accessory Bar"].exists)
        XCTAssertTrue(app.switches["Keep terminal size"].exists)
        XCTAssertTrue(app.switches["Show dismiss button"].exists)
        XCTAssertTrue(app.buttons["Customize Accessory Bar"].exists)
        XCTAssertTrue(app.buttons["Custom Actions"].exists)
        XCTAssertFalse(app.switches["Keep terminal size when keyboard opens"].exists)
        XCTAssertFalse(app.switches["Show keyboard dismiss button"].exists)
    }

    @MainActor
    func testProSettingsUsesOneStatusHero() throws {
        let app = XCUIApplication()
        app.terminate()
        app.launchArguments = [
            "-AppleLanguages", "(en)",
            "-AppleLocale", "en_US",
            "-hasSeenWelcome", "YES",
            "-iCloudSyncEnabled", "NO",
            "-security.privacyModeEnabled", "NO",
            "-security.fullAppLockEnabled", "NO",
            "-security.lockOnBackground", "NO",
        ]
        app.launch()
        defer { app.terminate() }

        let settings = app.buttons["orlix.serverList.settings"]
        XCTAssertTrue(settings.waitForExistence(timeout: 10))
        settings.tap()

        let proRoute = app.buttons["orlix.settings.route.pro"]
        XCTAssertTrue(proRoute.waitForExistence(timeout: 5))
        proRoute.tap()

        XCTAssertTrue(
            app.descendants(matching: .any)["orlix.settings.page.pro"]
                .waitForExistence(timeout: 5)
        )
        XCTAssertTrue(
            app.descendants(matching: .any)["orlix.settings.pro.statusHero"]
                .waitForExistence(timeout: 5)
        )
        XCTAssertTrue(app.buttons["Restore Purchases"].exists)
        XCTAssertFalse(app.staticTexts["Subscription"].exists)
        XCTAssertFalse(app.staticTexts["Purchased"].exists)
    }

    @MainActor
    func testConnectedTerminalCanOpenSettingsFromItsToolbar() throws {
        let app = XCUIApplication()
        app.terminate()
        app.launchArguments = [
            "--orlix-ui-test-terminal-reconnect-harness",
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
        app.launch()
        defer { app.terminate() }

        let diagnostics = app.staticTexts["orlix.reconnectTest.diagnostics"]
        XCTAssertTrue(diagnostics.waitForExistence(timeout: 45))
        try requireConfiguredLoopbackSSHFixture(diagnostics: diagnostics, app: app)
        wait(
            for: diagnostics,
            containing: "setup=ready state=connected",
            timeout: 45,
            app: app
        )

        openProductionTerminalMenu(in: app)
        let settings = app.buttons["orlix.terminal.settings"]
        XCTAssertTrue(settings.waitForExistence(timeout: 5), diagnosticText(in: app))
        settings.tap()

        XCTAssertTrue(
            app.descendants(matching: .any)["orlix.settings.root"]
                .waitForExistence(timeout: 8),
            "Settings did not open from the connected terminal toolbar. \(diagnosticText(in: app))"
        )
    }

}
#endif
