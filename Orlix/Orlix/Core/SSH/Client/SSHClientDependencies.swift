import Foundation
import MoshBootstrap

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package struct SSHHostKeyCandidate: Equatable, Sendable {
    package let host: String
    package let port: Int
    package let fingerprint: String
    package let keyType: Int
    package let keyTypeName: String
}

nonisolated package enum SSHHostKeyVerificationDecision: Equatable, Sendable {
    case trusted
    case approvalRequired
}

nonisolated package protocol SSHHostKeyVerifying: Sendable {
    func verify(_ candidate: SSHHostKeyCandidate) -> SSHHostKeyVerificationDecision
}

package typealias SSHMoshCommandExecutor = @Sendable (
    _ command: String,
    _ timeout: Duration
) async throws -> String

nonisolated package protocol SSHMoshBootstrapping: Sendable {
    func bootstrapConnectInfo(
        terminalType: RemoteTerminalType,
        startCommand: String?,
        portRange: ClosedRange<Int>,
        execute: @escaping SSHMoshCommandExecutor
    ) async throws -> MoshServerConnectInfo

    func terminateMoshServer(
        pid: Int32,
        execute: @escaping SSHMoshCommandExecutor
    ) async
}

nonisolated package struct SSHClientFactory: Sendable {
    private let runtimeSettings: @Sendable () -> SSHRuntimeSettings
    private let hostKeyVerifier: any SSHHostKeyVerifying
    private let moshBootstrap: any SSHMoshBootstrapping

    package init(
        runtimeSettings: @escaping @Sendable () -> SSHRuntimeSettings,
        hostKeyVerifier: any SSHHostKeyVerifying,
        moshBootstrap: any SSHMoshBootstrapping
    ) {
        self.runtimeSettings = runtimeSettings
        self.hostKeyVerifier = hostKeyVerifier
        self.moshBootstrap = moshBootstrap
    }

    package func makeClient(connectTimeout: Duration = .seconds(30)) -> SSHClient {
        SSHClient(
            connectTimeout: connectTimeout,
            runtimeSettings: runtimeSettings(),
            hostKeyVerifier: hostKeyVerifier,
            moshBootstrap: moshBootstrap
        )
    }
}

/// `UserDefaults` supports concurrent reads, and this source never mutates its
/// reference after initialization. Each call creates an immutable settings value.
private nonisolated final class SSHRuntimeSettingsSource: @unchecked Sendable {
    private let defaults: UserDefaults

    package init(defaults: UserDefaults) {
        self.defaults = defaults
    }

    package func current() -> SSHRuntimeSettings {
        SSHRuntimeSettings(defaults: defaults)
    }
}

nonisolated package enum SSHClientLiveComposition {
    static package func makeFactory(
        defaults: UserDefaults,
        knownHostsManager: KnownHostsManager,
        remoteMoshManager: RemoteMoshManager
    ) -> SSHClientFactory {
        let runtimeSettings = SSHRuntimeSettingsSource(defaults: defaults)
        return SSHClientFactory(
            runtimeSettings: {
                runtimeSettings.current()
            },
            hostKeyVerifier: knownHostsManager,
            moshBootstrap: remoteMoshManager
        )
    }
}

extension KnownHostsManager: SSHHostKeyVerifying {
    nonisolated package func verify(
        _ candidate: SSHHostKeyCandidate
    ) -> SSHHostKeyVerificationDecision {
        switch evaluate(
            host: candidate.host,
            port: candidate.port,
            fingerprint: candidate.fingerprint,
            keyType: candidate.keyType,
            keyTypeName: candidate.keyTypeName
        ) {
        case .trusted:
            return .trusted
        case .approvalRequired:
            return .approvalRequired
        }
    }
}

extension RemoteMoshManager: SSHMoshBootstrapping {}

#if DEBUG
package extension SSHClientFactory {
    static package func testing(
        runtimeSettings: SSHRuntimeSettings = SSHRuntimeSettings(
            keepAliveEnabled: true,
            keepAliveIntervalSeconds: SSHRuntimeSettings.defaultKeepAliveIntervalSeconds
        ),
        hostKeyVerifier: any SSHHostKeyVerifying = TestingSSHHostKeyVerifier(),
        moshBootstrap: any SSHMoshBootstrapping = TestingSSHMoshBootstrap()
    ) -> SSHClientFactory {
        SSHClientFactory(
            runtimeSettings: { runtimeSettings },
            hostKeyVerifier: hostKeyVerifier,
            moshBootstrap: moshBootstrap
        )
    }
}

package extension SSHClient {
    static package func testing(
        connectTimeout: Duration = .seconds(30),
        runtimeSettings: SSHRuntimeSettings = SSHRuntimeSettings(
            keepAliveEnabled: true,
            keepAliveIntervalSeconds: SSHRuntimeSettings.defaultKeepAliveIntervalSeconds
        ),
        hostKeyVerifier: any SSHHostKeyVerifying = TestingSSHHostKeyVerifier(),
        moshBootstrap: any SSHMoshBootstrapping = TestingSSHMoshBootstrap()
    ) -> SSHClient {
        SSHClient(
            connectTimeout: connectTimeout,
            runtimeSettings: runtimeSettings,
            hostKeyVerifier: hostKeyVerifier,
            moshBootstrap: moshBootstrap
        )
    }
}

nonisolated package struct TestingSSHHostKeyVerifier: SSHHostKeyVerifying {
    package func verify(_ candidate: SSHHostKeyCandidate) -> SSHHostKeyVerificationDecision {
        .trusted
    }
}

package actor TestingSSHMoshBootstrap: SSHMoshBootstrapping {
    package func bootstrapConnectInfo(
        terminalType: RemoteTerminalType,
        startCommand: String?,
        portRange: ClosedRange<Int>,
        execute: @escaping SSHMoshCommandExecutor
    ) async throws -> MoshServerConnectInfo {
        throw SSHError.moshBootstrapFailed("Unavailable in this test")
    }

    package func terminateMoshServer(
        pid: Int32,
        execute: @escaping SSHMoshCommandExecutor
    ) async {}
}
#endif
