#if os(macOS)
import AppKit
import Testing
@testable import Orlix

@MainActor
struct AppTerminalManagerInjectionTests {
    @Test
    func macToolbarDefersTerminalItemsUntilManagerIsInjected() {
        let controller = MacConnectionToolbarController()

        let identifiers = controller.toolbarDefaultItemIdentifiers(controller.toolbar)

        #expect(identifiers == [
            .flexibleSpace,
            .toggleSidebar,
            .sidebarTrackingSeparator,
        ])
    }
}
#endif
