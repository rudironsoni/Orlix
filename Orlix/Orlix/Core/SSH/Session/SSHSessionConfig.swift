import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package struct SSHSessionConfig: Sendable {
    package let host: String
    package let port: Int
    package let dialHost: String
    package let dialPort: Int
    package let hostKeyHost: String
    package let hostKeyPort: Int
    package let username: String
    package let connectionMode: SSHConnectionMode
    package let authMethod: AuthMethod
    package let credentials: ServerCredentials

    package var connectionTimeout: TimeInterval = 30
    package let keepAlive: SSHKeepAlivePolicy

    package init(
        host: String,
        port: Int,
        dialHost: String? = nil,
        dialPort: Int? = nil,
        hostKeyHost: String? = nil,
        hostKeyPort: Int? = nil,
        username: String,
        connectionMode: SSHConnectionMode,
        authMethod: AuthMethod,
        credentials: ServerCredentials,
        connectionTimeout: TimeInterval = 30,
        keepAlive: SSHKeepAlivePolicy = .enabled(
            intervalSeconds: SSHRuntimeSettings.defaultKeepAliveIntervalSeconds
        )
    ) {
        self.host = host
        self.port = port
        self.dialHost = dialHost ?? host
        self.dialPort = dialPort ?? port
        self.hostKeyHost = hostKeyHost ?? host
        self.hostKeyPort = hostKeyPort ?? port
        self.username = username
        self.connectionMode = connectionMode
        self.authMethod = authMethod
        self.credentials = credentials
        self.connectionTimeout = connectionTimeout
        self.keepAlive = keepAlive
    }
}
