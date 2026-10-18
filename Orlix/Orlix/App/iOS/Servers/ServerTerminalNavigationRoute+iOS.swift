import Foundation

#if os(iOS)
#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package enum ServerTerminalNavigationRoute: Equatable, Sendable {
    nonisolated package enum ConnectionResolution: Sendable {
        case succeeded
        case failed
    }

    case connecting(server: Server, attemptID: UUID)
    case active(serverId: UUID)

    package var serverId: UUID {
        switch self {
        case .connecting(let server, _):
            server.id
        case .active(let serverId):
            serverId
        }
    }

    package var connectingServer: Server? {
        guard case .connecting(let server, _) = self else { return nil }
        return server
    }

    package var connectionAttemptID: UUID? {
        guard case .connecting(_, let attemptID) = self else { return nil }
        return attemptID
    }

    package var isConnecting: Bool {
        connectingServer != nil
    }

    package func resolvingConnection(
        for attemptID: UUID,
        as resolution: ConnectionResolution
    ) -> Self? {
        guard case .connecting(let server, let currentAttemptID) = self,
              currentAttemptID == attemptID else {
            return self
        }

        switch resolution {
        case .succeeded:
            return .active(serverId: server.id)
        case .failed:
            return nil
        }
    }
}
#endif
