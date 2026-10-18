import Foundation

nonisolated package struct ShellHandle: Sendable {
    package let id: UUID
    package let stream: TerminalOutputStream
    package let transportState: ShellTransportState
    package let origin: ShellStartOrigin

    package var transport: ShellTransport { transportState.transport }
    package var fallbackReason: MoshFallbackReason? { transportState.fallbackReason }
    package var fallbackDiagnostics: MoshFallbackDiagnostics? { transportState.fallbackDiagnostics }

    package init(
        id: UUID,
        stream: TerminalOutputStream,
        transportState: ShellTransportState = .ssh,
        origin: ShellStartOrigin = .fresh
    ) {
        self.id = id
        self.stream = stream
        self.transportState = transportState
        self.origin = origin
    }
}

nonisolated package enum ShellStartOrigin: Equatable, Sendable {
    case fresh
    case restored
}
