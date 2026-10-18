import Foundation

nonisolated public enum EternalTerminalSessionFailure: Error, Hashable, Sendable {
    public case bootstrapSSH
    public case bootstrapResponse(String)
    public case malformedBootstrapCredentials
    public case resumeState(message: String, discardStoredState: Bool)
    public case transport
    public case invalidKey
    public case protocolMismatch
    public case disconnectedBufferFull
    public case connectionInProgress
    public case connectionClosed
    public case applicationSuspended
    public case sessionUnrecoverable
    public case client
    public case unknown
}
