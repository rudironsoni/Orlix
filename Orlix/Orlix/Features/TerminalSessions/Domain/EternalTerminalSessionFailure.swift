import Foundation

nonisolated package enum EternalTerminalSessionFailure: Error, Hashable, Sendable {
    package case bootstrapSSH
    package case bootstrapResponse(String)
    package case malformedBootstrapCredentials
    package case resumeState(message: String, discardStoredState: Bool)
    package case transport
    package case invalidKey
    package case protocolMismatch
    package case disconnectedBufferFull
    package case connectionInProgress
    package case connectionClosed
    package case applicationSuspended
    package case sessionUnrecoverable
    package case client
    package case unknown
}
