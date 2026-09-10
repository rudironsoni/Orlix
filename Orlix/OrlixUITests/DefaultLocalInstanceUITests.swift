import XCTest

final class DefaultLocalInstanceUITests: XCTestCase {
    func testOpensDefaultLocalInstanceTerminal() {
        let app = XCUIApplication()
        app.launchArguments += [
            "--orlix-ui-test-local-terminal",
            "-hasSeenWelcome", "YES",
            "-iCloudSyncEnabled", "NO",
            "-terminalUsePerAppearanceTheme", "NO",
            "-terminalThemeName", "Orlix Dark",
            "-terminalKeyboardDismissButtonEnabled", "YES",
            "--orlix-debug-log", "keyboard",
        ]
        app.launch()

        let openButton = app.buttons["orlix.local-instance.open"]
        XCTAssertTrue(openButton.waitForExistence(timeout: 5))
        openButton.tap()

        XCTAssertTrue(app.navigationBars["Orlix"].waitForExistence(timeout: 5))
        let terminal = app.descendants(matching: .any)["orlix.local-instance.terminal"]
        XCTAssertTrue(terminal.waitForExistence(timeout: 15))

        expectation(
            for: NSPredicate(format: "value == %@", "output"),
            evaluatedWith: terminal
        )
        waitForExpectations(timeout: 30)
        XCTAssertNotEqual(terminal.value as? String, "failed")

        terminal.tap()
        XCTAssertTrue(app.keyboards.firstMatch.waitForExistence(timeout: 8))
        let hideKeyboard = app.descendants(matching: .any)["orlix.keyboard.accessory.hide"]
        XCTAssertTrue(hideKeyboard.waitForExistence(timeout: 5))

        let screenshot = XCTAttachment(screenshot: app.screenshot())
        screenshot.name = "Orlix local terminal after boot"
        screenshot.lifetime = .keepAlways
        add(screenshot)

        let ready = XCTNSPredicateExpectation(
            predicate: NSPredicate(format: "hittable == true"), object: hideKeyboard
        )
        guard XCTWaiter.wait(for: [ready], timeout: 8) == .completed else {
            XCTFail(hideKeyboard.debugDescription)
            return
        }
        hideKeyboard.tap()
        XCTAssertTrue(app.keyboards.firstMatch.waitForNonExistence(timeout: 8))
    }
}
