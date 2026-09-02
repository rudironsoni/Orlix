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
        let terminal = app.descendants(matching: .any)
            .matching(identifier: "orlix.local-instance.terminal")
            .firstMatch
        XCTAssertTrue(terminal.waitForExistence(timeout: 5))

        expectation(
            for: NSPredicate(format: "value == %@", "output"),
            evaluatedWith: terminal
        )
        waitForExpectations(timeout: 30)

        terminal.tap()
        terminal.typeText("echo ORLIX-LOCAL-READY\n")

        let appExited = expectation(
            for: NSPredicate(format: "state != %d", XCUIApplication.State.runningForeground.rawValue),
            evaluatedWith: app
        )
        appExited.isInverted = true
        waitForExpectations(timeout: 15)
        XCTAssertNotEqual(terminal.value as? String, "failed")

        let screenshot = XCTAttachment(screenshot: app.screenshot())
        screenshot.name = "Orlix local terminal after boot"
        screenshot.lifetime = .keepAlways
        add(screenshot)
    }
}
