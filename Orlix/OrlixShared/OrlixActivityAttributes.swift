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
        var status: OrlixLiveActivityStatus
        var activeCount: Int
    }

    package var appName: String
}
#endif
