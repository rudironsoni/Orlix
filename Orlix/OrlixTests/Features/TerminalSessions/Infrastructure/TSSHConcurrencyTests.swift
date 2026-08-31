import Foundation
import MoshBootstrap
import NetworkExtension
import Testing
@testable import Orlix

private nonisolated struct TSSHHostKeyVerifierStub: SSHHostKeyVerifying {
    func verify(_ candidate: SSHHostKeyCandidate) -> SSHHostKeyVerificationDecision {
        .trusted
    }
}

private actor TSSHMoshBootstrapStub: SSHMoshBootstrapping {
    func bootstrapConnectInfo(
        terminalType: RemoteTerminalType,
        startCommand: String?,
        portRange: ClosedRange<Int>,
        execute: @escaping SSHMoshCommandExecutor
    ) async throws -> MoshServerConnectInfo {
        throw SSHError.notConnected
    }

    func terminateMoshServer(
        pid: Int32,
        execute: @escaping SSHMoshCommandExecutor
    ) async {}
}

private nonisolated final class TSSHResumeStoreSpy: TSSHResumeStoring, @unchecked Sendable {
    private(set) var deletedPaneIDs: [UUID] = []

    func load(for paneID: UUID) throws -> TSSHResumeState? { nil }
    func hasCheckpoint(for paneID: UUID) -> Bool { true }
    func save(_ state: TSSHResumeState, for paneID: UUID) throws {}
    func delete(for paneID: UUID) throws { deletedPaneIDs.append(paneID) }
    func loadCleanup(for paneID: UUID) throws -> TSSHResumeCleanupState? { nil }
    func saveCleanup(_ state: TSSHResumeCleanupState, for paneID: UUID) throws {}
    func deleteCleanup(for paneID: UUID) throws {}
    func pendingCleanupPaneIDs() -> [UUID] { [] }
}

struct TSSHConcurrencyTests {
    @Test @MainActor
    func cachedRuntimeRequiresExactEndpointProfileAndCredentials() {
        let paneID = UUID()
        let server = Server(
            workspaceId: UUID(),
            name: "TSSH",
            host: "example.com",
            port: 22,
            tsshProfile: TSSHProfile(sshAgentForwarding: true),
            username: "root",
            connectionMode: .tssh
        )
        let credentials = ServerCredentials(
            serverId: server.id,
            credentialBinding: ServerCredentialBinding(server: server),
            password: "first"
        )
        let runtime = TSSHRuntime(
            paneID: paneID,
            server: server,
            credentials: credentials,
            sshClientFactory: SSHClientFactory(
                runtimeSettings: {
                    SSHRuntimeSettings(keepAliveEnabled: false, keepAliveIntervalSeconds: 10)
                },
                hostKeyVerifier: TSSHHostKeyVerifierStub(),
                moshBootstrap: TSSHMoshBootstrapStub()
            ),
            resumeStore: TSSHResumeStoreSpy(),
            ownerAccess: TSSHRuntimeOwnerAccess(
                isCurrent: { _, _ in true },
                startupPlan: { _, _, _, _ in throw SSHError.notConnected },
                resumeContext: { _ in nil },
                startupActionReplayPending: { _ in false },
                setResumeContext: { _, _ in },
                setStartupActionReplayPending: { _, _ in },
                remoteSessionAttached: { _ in },
                updateConnectionState: { _, _ in },
                markTransport: { _ in },
                handleShellEnd: { _, _, _ in }
            )
        )

        #expect(runtime.isBound(to: server, credentials: credentials))

        var changedEndpoint = server
        changedEndpoint.host = "replacement.example.com"
        #expect(!runtime.isBound(to: changedEndpoint, credentials: credentials))

        var changedProfile = server
        changedProfile.tsshProfile = TSSHProfile(sshAgentForwarding: false)
        #expect(!runtime.isBound(to: changedProfile, credentials: credentials))

        var changedCredentials = credentials
        changedCredentials.password = "second"
        #expect(!runtime.isBound(to: server, credentials: changedCredentials))
    }

    @Test(arguments: [
        (preservationRequested: true, hasCheckpoint: true, expected: true),
        (preservationRequested: true, hasCheckpoint: false, expected: false),
        (preservationRequested: false, hasCheckpoint: true, expected: false),
    ])
    func cancelledStartOnlyPreservesACheckpointedServer(
        preservationRequested: Bool,
        hasCheckpoint: Bool,
        expected: Bool
    ) {
        #expect(tsshShouldPreserveCancelledStartServer(
            preservationRequested: preservationRequested,
            hasCheckpoint: hasCheckpoint
        ) == expected)
    }

    @Test @MainActor
    func reconnectPreservesTheAttachableSessionCheckpoint() async {
        let paneID = UUID()
        let server = Server(
            workspaceId: UUID(),
            name: "TSSH",
            host: "example.com",
            port: 22,
            username: "root",
            connectionMode: .tssh
        )
        let store = TSSHResumeStoreSpy()
        let runtime = TSSHRuntime(
            paneID: paneID,
            server: server,
            credentials: ServerCredentials(serverId: server.id),
            sshClientFactory: SSHClientFactory(
                runtimeSettings: {
                    SSHRuntimeSettings(keepAliveEnabled: false, keepAliveIntervalSeconds: 10)
                },
                hostKeyVerifier: TSSHHostKeyVerifierStub(),
                moshBootstrap: TSSHMoshBootstrapStub()
            ),
            resumeStore: store,
            ownerAccess: TSSHRuntimeOwnerAccess(
                isCurrent: { _, _ in true },
                startupPlan: { _, _, _, _ in throw SSHError.notConnected },
                resumeContext: { _ in nil },
                startupActionReplayPending: { _ in false },
                setResumeContext: { _, _ in },
                setStartupActionReplayPending: { _, _ in },
                remoteSessionAttached: { _ in },
                updateConnectionState: { _, _ in },
                markTransport: { _ in },
                handleShellEnd: { _, _, _ in }
            )
        )

        await runtime.prepareForReconnect()

        #expect(store.deletedPaneIDs.isEmpty)
    }

    @Test
    func cancellingNativeOperationReturnsBeforeBlockingWorkFinishes() async throws {
        let operationStarted = DispatchSemaphore(value: 0)
        let releaseOperation = DispatchSemaphore(value: 0)
        let lateValueDiscarded = DispatchSemaphore(value: 0)
        let task = Task {
            try await tsshPerformCancellable(
                on: DispatchQueue(label: "TSSHConcurrencyTests.native"),
                operation: {
                    operationStarted.signal()
                    releaseOperation.wait()
                    return 42
                },
                discardLateValue: { value in
                    if value == 42 { lateValueDiscarded.signal() }
                }
            )
        }

        #expect(operationStarted.wait(timeout: .now() + 1) == .success)
        task.cancel()
        do {
            _ = try await task.value
            Issue.record("Cancelled native operation returned a value")
        } catch is CancellationError {
        } catch {
            Issue.record("Cancelled native operation returned \(error)")
        }

        releaseOperation.signal()
        #expect(lateValueDiscarded.wait(timeout: .now() + 1) == .success)
    }

    @Test @MainActor
    func secondVPNOwnerCannotReplaceOrReleaseFirstOwner() throws {
        let coordinator = TSSHVPNOwnershipCoordinator()
        let firstOwner = UUID()
        let secondOwner = UUID()

        #expect(try coordinator.acquire(firstOwner) == .vacant)
        #expect(try coordinator.acquire(firstOwner) == .existingOwner)
        #expect(throws: TSSHRuntimeError.self) {
            try coordinator.acquire(secondOwner)
        }
        coordinator.release(secondOwner)
        #expect(coordinator.isOwned(by: firstOwner))
        coordinator.release(firstOwner)
        #expect(coordinator.ownerID == nil)
    }

    @Test
    func persistedVPNRevocationRequiresTheMutatedServerIdentity() {
        let serverID = UUID()

        #expect(tsshVPNPersistedServerMatches(
            existingServerID: serverID.uuidString,
            requestedServerID: serverID
        ))
        #expect(!tsshVPNPersistedServerMatches(
            existingServerID: UUID().uuidString,
            requestedServerID: serverID
        ))
        #expect(!tsshVPNPersistedServerMatches(
            existingServerID: nil,
            requestedServerID: serverID
        ))
    }

    @Test
    func VPNStartupRequiresConnectedAndRejectsFailedStates() {
        var monitor = TSSHVPNStartupMonitor()
        #expect(monitor.decision(for: .disconnected, elapsedSeconds: 0) == .waiting)
        #expect(monitor.decision(for: .connecting, elapsedSeconds: 0.1) == .waiting)
        #expect(monitor.decision(for: .disconnected, elapsedSeconds: 0.2) == .failed(
            "The VPN disconnected during startup."
        ))

        monitor = TSSHVPNStartupMonitor()
        #expect(monitor.decision(for: .invalid, elapsedSeconds: 0) == .failed(
            "The VPN configuration is invalid."
        ))
        #expect(monitor.decision(for: .connected, elapsedSeconds: 0) == .connected)
    }

    @Test
    func nativeTransportDoesNotPublishConnectedBeforeRequestedSetupIsReady() {
        #expect(tsshPublishedTransportState(
            isHealthy: true,
            startupReady: false
        ) == .connecting)
        #expect(tsshPublishedTransportState(
            isHealthy: true,
            startupReady: true
        ) == .connected)
        #expect(tsshPublishedTransportState(
            isHealthy: false,
            startupReady: true
        ) == .reconnecting(attempt: 1))
    }

    @Test
    func terminalExitAndCloseDeliverOnlyOncePerGeneration() {
        var gate = TSSHTerminalEventGate()
        let firstGeneration = UUID()
        let secondGeneration = UUID()

        let firstExit = gate.claim(firstGeneration)
        let duplicateClose = gate.claim(firstGeneration)
        let nextExit = gate.claim(secondGeneration)
        let nextDuplicateClose = gate.claim(secondGeneration)

        #expect(firstExit)
        #expect(!duplicateClose)
        #expect(nextExit)
        #expect(!nextDuplicateClose)
    }

    @Test
    func asynchronousForwardFailureProducesVisibleTerminalNotice() throws {
        let notice = tsshForwardFailureNotice(
            id: "8B40B930-7F97-4A71-AF08-ECE01F2D0D92",
            message: "bind: address already in use"
        )
        let rendered = try #require(String(data: notice, encoding: .utf8))

        #expect(rendered.contains("TSSH port forward 8B40B930-7F97-4A71-AF08-ECE01F2D0D92 failed"))
        #expect(rendered.contains("bind: address already in use"))
    }

    @Test
    func postConnectVPNDropFailsClosedAtTheOwningPane() {
        #expect(tsshVPNPostConnectDecision(for: .connected) == .healthy)
        #expect(tsshVPNPostConnectDecision(for: .reasserting) == .healthy)
        #expect(tsshVPNPostConnectDecision(for: .disconnected) == .failed(
            "The TSSH VPN disconnected after startup."
        ))
        #expect(tsshVPNPostConnectDecision(for: .invalid) == .failed(
            "The TSSH VPN became invalid after startup."
        ))
    }

    @Test
    func reclaimedVPNRequiresTheExactPersistedConfiguration() throws {
        let info = try TSSHServerInfo.parse(output: #"{"ServerVer":"0.2.2","ProtoVer":1,"Port":61000,"Mode":"KCP","Pass":"aa","Salt":"bb","ProxyKey":"cc","ProxyMode":"TCP","MTU":1400,"ClientID":1,"ServerID":2}"#)
        let original = TSSHVPNConfiguration(
            host: "vpn.example.com",
            info: info,
            profile: TSSHProfile(
                mtu: 1_400,
                vpnEnabled: true,
                blockQUICInVPN: true,
                vpnDNSServers: ["1.1.1.1"],
                vpnExcludedRoutes: ["10.0.0.0/8"]
            )
        )
        let changedPolicy = TSSHVPNConfiguration(
            host: "vpn.example.com",
            info: info,
            profile: TSSHProfile(
                mtu: 1_280,
                vpnEnabled: true,
                blockQUICInVPN: false,
                vpnDNSServers: ["9.9.9.9"],
                vpnExcludedRoutes: ["192.168.0.0/16"]
            )
        )
        let fingerprint = try #require(original.fingerprint)

        #expect(tsshVPNConfigurationMatches(
            existingHost: "vpn.example.com",
            existingFingerprint: fingerprint,
            requestedConfiguration: original
        ))
        #expect(!tsshVPNConfigurationMatches(
            existingHost: "vpn.example.com",
            existingFingerprint: fingerprint,
            requestedConfiguration: changedPolicy
        ))
        #expect(!tsshVPNConfigurationMatches(
            existingHost: "other.example.com",
            existingFingerprint: fingerprint,
            requestedConfiguration: original
        ))
        #expect(!tsshVPNConfigurationMatches(
            existingHost: "vpn.example.com",
            existingFingerprint: nil,
            requestedConfiguration: original
        ))
    }

    @Test
    func VPNReusesTheTerminalEndpointWithADistinctClientIdentity() throws {
        let info = try TSSHServerInfo.parse(output: #"{"ServerVer":"0.2.2","ProtoVer":1,"Port":61000,"Mode":"KCP","Pass":"aa","Salt":"bb","ProxyKey":"cc","ProxyMode":"TCP","MTU":1400,"ClientID":7,"ServerID":9}"#)
        let configuration = TSSHVPNConfiguration(
            reusing: "vpn.example.com",
            info: info,
            profile: TSSHProfile(vpnEnabled: true)
        )

        #expect(configuration.tsshHost == "vpn.example.com")
        #expect(configuration.tsshPort == info.port)
        #expect(configuration.tsshServerID == info.serverID)
        #expect(configuration.tsshClientID == 8)
    }
}
