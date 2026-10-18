import Foundation

#if os(iOS)
import ActivityKit

@available(iOS 16.1, *)
nonisolated package enum OrlixLiveActivityStatus: String, Codable, Hashable {
    case connected
    case connecting
    case reconnecting
    case disconnected

    package var label: String {
        switch self {
        case .connected:
            return "Connected"
        case .connecting:
            return "Connecting"
        case .reconnecting:
            return "Reconnecting"
        case .disconnected:
            return "Disconnected"
        }
    }
}

@available(iOS 16.1, *)
nonisolated package struct OrlixActivityAttributes: ActivityAttributes {
    package struct ContentState: Codable, Hashable {
        package var status: OrlixLiveActivityStatus
        package var activeCount: Int

        package init(status: OrlixLiveActivityStatus, activeCount: Int) {
            self.status = status
            self.activeCount = activeCount
        }
    }

    package var appName: String

    package init(appName: String) {
        self.appName = appName
    }
}
#endif
