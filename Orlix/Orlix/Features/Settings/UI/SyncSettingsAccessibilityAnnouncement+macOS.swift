#if os(macOS)
import AppKit

package enum SyncSettingsAccessibilityAnnouncement {
    static package func post(_ message: String) {
        guard NSWorkspace.shared.isVoiceOverEnabled else { return }
        NSAccessibility.post(
            element: NSApp,
            notification: .announcementRequested,
            userInfo: [.announcement: message]
        )
    }
}
#endif
