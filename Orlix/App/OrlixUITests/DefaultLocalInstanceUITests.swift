import XCTest

final class DefaultLocalInstanceUITests: XCTestCase {
    func testOpensDefaultLocalInstanceTerminal() {
        let app = XCUIApplication()
        app.launchArguments += ["-hasSeenWelcome", "YES"]
        app.launch()

        let openButton = app.buttons["orlix.local-instance.open"]
        XCTAssertTrue(openButton.waitForExistence(timeout: 5))
        openButton.tap()

        XCTAssertTrue(app.navigationBars["Orlix"].waitForExistence(timeout: 5))
        let terminal = app.otherElements["orlix.local-instance.terminal"]
        XCTAssertTrue(terminal.waitForExistence(timeout: 5))

        expectation(
            for: NSPredicate(format: "value == %@", "output"),
            evaluatedWith: terminal
        )
        waitForExpectations(timeout: 30)
    }
}
