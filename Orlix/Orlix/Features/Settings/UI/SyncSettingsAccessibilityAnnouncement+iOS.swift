#if os(iOS)
import UIKit

package enum SyncSettingsAccessibilityAnnouncement {
    static package func post(_ message: String) {
        guard UIAccessibility.isVoiceOverRunning else { return }
        UIAccessibility.post(notification: .announcement, argument: message)
    }
}
#endif
