import Foundation
import OSLog

package enum Ghostty {
    static package let logger = Logger(
        subsystem: Bundle.main.bundleIdentifier ?? "com.rudironsoni.orlix",
        category: "Ghostty"
    )

    static package let configDidReloadNotification = Notification.Name("GhosttyConfigDidReload")
}
