import Foundation

#if os(iOS)
import ActivityKit

@available(iOS 16.1, *)
nonisolated public enum OrlixLiveActivityStatus: String, Codable, Hashable {
    public case connected
    public case connecting
    public case reconnecting
    public case disconnected

    public var label: String {
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
    }

    package var appName: String
}
#endif
