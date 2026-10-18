import Foundation

nonisolated package enum SSHConnectedSessionAction: Equatable, Sendable {
    case reuse
    case recover
    case reject
}

nonisolated package enum SSHConnectedSessionPolicy {
    static package func action(
        existingConnectionKey: String,
        requestedConnectionKey: String,
        transportIsConnected: Bool
    ) -> SSHConnectedSessionAction {
        guard transportIsConnected else { return .recover }
        return existingConnectionKey == requestedConnectionKey ? .reuse : .reject
    }
}
