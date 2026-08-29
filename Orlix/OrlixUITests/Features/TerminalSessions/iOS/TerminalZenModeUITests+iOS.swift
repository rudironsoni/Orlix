#if os(iOS)
import XCTest

final class TerminalZenModeUITests: XCTestCase {
    @MainActor
    func testRealTerminalLauncherOpensZenPanel() {
        let app = XCUIApplication()
        app.launchArguments = [
            "--orlix-ui-test-terminal-zen-mode-harness",
            "-AppleLanguages",
            "(en)",
            "-AppleLocale",
            "en_US"
        ]
        app.launch()

        XCTAssertTrue(
            app.descendants(matching: .any)["orlix.zenTest.terminalSurface"]
                .waitForExistence(timeout: 15)
        )
        app.buttons["orlix.terminal.moreMenu"].tap()
        let enterZenMode = app.buttons["orlix.terminal.enterZenMode"]
        XCTAssertTrue(enterZenMode.waitForExistence(timeout: 5))
        enterZenMode.tap()

        let launcher = app.buttons["orlix.zen.controls"]
        XCTAssertTrue(launcher.waitForExistence(timeout: 5))
        XCTAssertTrue(launcher.isHittable)
        XCTAssertEqual(launcher.frame.width, launcher.frame.height, accuracy: 1)
        XCTAssertLessThanOrEqual(launcher.frame.width, 48)
        launcher.tap()

        XCTAssertTrue(
            app.buttons["orlix.terminal.zen.view.terminal"].waitForExistence(timeout: 5),
            "Zen launcher remained visible but did not open the control panel"
        )
    }

    @MainActor
    func testMenuEntryHidesChromeAndFloatingControlRestoresIt() {
        let app = XCUIApplication()
        app.launchArguments = [
            "--orlix-ui-test-terminal-zen-mode-harness",
            "-AppleLanguages",
            "(en)",
            "-AppleLocale",
            "en_US"
        ]
        app.launch()

        let chrome = app.buttons["orlix.zenTest.chrome"]
        XCTAssertTrue(chrome.waitForExistence(timeout: 5))

        app.buttons["orlix.terminal.moreMenu"].tap()
        let enterZenMode = app.buttons["orlix.terminal.enterZenMode"]
        XCTAssertTrue(enterZenMode.waitForExistence(timeout: 5))
        enterZenMode.tap()

        XCTAssertTrue(
            app.buttons["orlix.zen.controls"].waitForExistence(timeout: 5)
        )
        XCTAssertTrue(chrome.waitForNonExistence(timeout: 5))

        app.buttons["orlix.zen.controls"].tap()
        XCTAssertTrue(app.buttons["orlix.terminal.zen.view.terminal"].waitForExistence(timeout: 5))
        XCTAssertTrue(app.buttons["orlix.terminal.zen.view.files"].exists)
        XCTAssertTrue(app.buttons["orlix.terminal.zen.newTab"].exists)
        XCTAssertTrue(app.buttons["orlix.terminal.zen.settings"].exists)
        XCTAssertTrue(app.buttons["orlix.terminal.zen.editServer"].exists)
        XCTAssertTrue(app.buttons["orlix.terminal.zen.back"].exists)
        XCTAssertTrue(app.buttons["orlix.terminal.zen.disconnect"].exists)

        app.buttons["orlix.terminal.zen.view.files"].tap()
        XCTAssertTrue(app.buttons["orlix.zen.controls"].exists)

        let exitZenMode = app.buttons["orlix.terminal.exitZenMode"]
        XCTAssertTrue(exitZenMode.waitForExistence(timeout: 5))
        if !exitZenMode.isHittable {
            app.scrollViews["orlix.terminal.zenPanel"].swipeUp()
        }
        XCTAssertTrue(exitZenMode.isHittable)
        exitZenMode.tap()

        XCTAssertTrue(chrome.waitForExistence(timeout: 5))
        XCTAssertTrue(
            app.buttons["orlix.zen.controls"].waitForNonExistence(timeout: 5)
        )
    }
}
#endif
