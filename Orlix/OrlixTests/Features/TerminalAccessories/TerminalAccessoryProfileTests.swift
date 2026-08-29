import XCTest
@testable import Orlix

final class TerminalAccessoryProfileTests: XCTestCase {
    func testNormalizedRemovesDuplicateActiveItems() {
        let profile = TerminalAccessoryProfile(
            schemaVersion: TerminalAccessoryProfile.schemaVersion,
            layout: TerminalAccessoryLayout(
                version: 1,
                activeItems: [
                    .system(.escape),
                    .system(.escape),
                    .system(.tab),
                    .system(.arrowUp),
                    .system(.arrowDown)
                ],
                updatedAt: Date()
            ),
            customActions: [],
            updatedAt: Date(),
            lastWriterDeviceId: "test-device"
        )

        let normalized = profile.normalized()

        XCTAssertEqual(
            normalized.layout.activeItems,
            [
                TerminalAccessoryItemRef.system(.escape),
                TerminalAccessoryItemRef.system(.tab),
                TerminalAccessoryItemRef.system(.arrowUp),
                TerminalAccessoryItemRef.system(.arrowDown),
            ]
        )
    }

    func testNormalizedDropsDeletedCustomActionReferences() {
        let deletedAction = TerminalAccessoryCustomAction(
            id: UUID(),
            title: "Deleted",
            kind: .command,
            commandContent: "ls",
            commandSendMode: .insert,
            shortcutKey: .a,
            shortcutModifiers: .none,
            updatedAt: Date(),
            deletedAt: Date()
        )
        let profile = TerminalAccessoryProfile(
            schemaVersion: TerminalAccessoryProfile.schemaVersion,
            layout: TerminalAccessoryLayout(
                version: 1,
                activeItems: [
                    .system(.escape),
                    .custom(deletedAction.id),
                    .system(.tab),
                    .system(.arrowUp),
                    .system(.arrowDown),
                ],
                updatedAt: Date()
            ),
            customActions: [deletedAction],
            updatedAt: Date(),
            lastWriterDeviceId: "test-device"
        )

        let normalized = profile.normalized()

        XCTAssertEqual(
            normalized.layout.activeItems,
            [
                TerminalAccessoryItemRef.system(.escape),
                TerminalAccessoryItemRef.system(.tab),
                TerminalAccessoryItemRef.system(.arrowUp),
                TerminalAccessoryItemRef.system(.arrowDown),
            ]
        )
    }
}
