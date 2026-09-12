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
            [.system(.escape), .system(.tab), .system(.arrowUp), .system(.arrowDown)]
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
                activeItems: [.custom(deletedAction.id)],
                updatedAt: Date()
            ),
            customActions: [deletedAction],
            updatedAt: Date(),
            lastWriterDeviceId: "test-device"
        )

        let normalized = profile.normalized()

        XCTAssertFalse(normalized.layout.activeItems.contains(.custom(deletedAction.id)))
    }
}
