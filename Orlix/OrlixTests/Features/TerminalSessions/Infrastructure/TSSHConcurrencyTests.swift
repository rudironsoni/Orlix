import Foundation
import MoshBootstrap
import NetworkExtension
import Security
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

private nonisolated enum TSSHResumeStoreSpyError: Error {
    case cleanupStorage
}

private nonisolated final class TSSHResumeStoreSpy: TSSHResumeStoring, @unchecked Sendable {
    private(set) var deletedPaneIDs: [UUID] = []
    private(set) var cleanupSavePaneIDs: [UUID] = []
    var loadedState: TSSHResumeState?
    var saveCleanupError: Error?

    init(
        loadedState: TSSHResumeState? = nil,
        saveCleanupError: Error? = nil
    ) {
        self.loadedState = loadedState
        self.saveCleanupError = saveCleanupError
    }

    func load(for paneID: UUID) throws -> TSSHResumeState? { loadedState }
    func recoverCleanupState(for paneID: UUID) -> TSSHResumeCleanupState? { nil }
    func hasCheckpoint(for paneID: UUID) -> Bool { true }
    func save(_ state: TSSHResumeState, for paneID: UUID) throws {}
    func delete(for paneID: UUID) throws { deletedPaneIDs.append(paneID) }
    func loadCleanup(for paneID: UUID) throws -> TSSHResumeCleanupState? { nil }
    func saveCleanup(_ state: TSSHResumeCleanupState, for paneID: UUID) throws {
        cleanupSavePaneIDs.append(paneID)
        if let saveCleanupError { throw saveCleanupError }
    }
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

    @Test @MainActor
    func cachedRuntimeRequiresTheCurrentTrustedHostFingerprint() {
        let paneID = UUID()
        let server = Server(
            workspaceId: UUID(),
            name: "TSSH",
            host: "example.com",
            port: 22,
            username: "root",
            connectionMode: .tssh
        )
        let credentials = ServerCredentials(serverId: server.id)
        var fingerprint: String? = "SHA256:first"
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
            trustedHostFingerprint: { _, _ in fingerprint },
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
        fingerprint = nil
        #expect(!runtime.isBound(to: server, credentials: credentials))
    }

    @Test
    func cleanupIdentityMatchesEndpointAfterMetadataOnlyEdit() {
        var server = Server(
            workspaceId: UUID(),
            name: "TSSH",
            host: "example.com",
            port: 22,
            username: "root",
            connectionMode: .tssh
        )
        let savedIdentity = TSSHResumeServerIdentity(server: server)

        server.name = "Renamed"
        server.updatedAt = server.updatedAt.addingTimeInterval(1)

        #expect(savedIdentity.matchesEndpoint(of: server))
        server.username = "admin"
        #expect(!savedIdentity.matchesEndpoint(of: server))
    }

    @Test @MainActor
    func securityBindingReplacementPreservesCheckpointWhenCleanupCannotStage() async throws {
        let paneID = UUID()
        let server = Server(
            workspaceId: UUID(),
            name: "TSSH",
            host: "example.com",
            port: 22,
            username: "root",
            connectionMode: .tssh
        )
        let info = try TSSHServerInfo.parse(
            output: #"{"ServerVer":"0.2.2","Port":61000,"Mode":"KCP","Pass":"aa","Salt":"bb","ProxyKey":"cc","ClientID":1,"ServerID":2}"#
        )
        let state = TSSHResumeState(
            serverIdentity: TSSHResumeServerIdentity(server: server),
            sshHostKeyFingerprint: "SHA256:trusted",
            serverProcess: TSSHServerProcessIdentity(
                pid: 123,
                supervisorPath: "/tmp/orlix-tsshd-test.sh"
            ),
            host: server.host,
            info: info,
            sessionID: 42,
            profile: server.tsshProfile,
            savedAt: Date()
        )
        let resumeStore = TSSHResumeStoreSpy(
            loadedState: state,
            saveCleanupError: TSSHResumeStoreSpyError.cleanupStorage
        )
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
            resumeStore: resumeStore,
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

        await runtime.closeForSecurityBindingReplacement()

        #expect(resumeStore.cleanupSavePaneIDs == [paneID])
        #expect(resumeStore.deletedPaneIDs.isEmpty)
    }

    @Test
    func corruptCheckpointDeletionPurgesVersionedSecrets() throws {
        let paneID = UUID()
        let fileManager = FileManager.default
        let root = fileManager.temporaryDirectory.appendingPathComponent(
            "TSSHResumeStoreTests.\(UUID().uuidString)",
            isDirectory: true
        )
        let service = "com.rudironsoni.orlix.tssh-resume.tests.\(UUID().uuidString)"
        let serviceQuery: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrSynchronizable as String: kCFBooleanFalse as Any,
        ]
        defer {
            SecItemDelete(serviceQuery as CFDictionary)
            try? fileManager.removeItem(at: root)
        }
        let store = TSSHResumeStore(
            fileManager: fileManager,
            root: root,
            service: service
        )
        let server = Server(
            workspaceId: UUID(),
            name: "TSSH",
            host: "example.com",
            port: 22,
            username: "root",
            connectionMode: .tssh
        )
        let info = try TSSHServerInfo.parse(
            output: #"{"ServerVer":"0.2.2","Port":61000,"Mode":"KCP","Pass":"aa","Salt":"bb","ProxyKey":"cc","ClientID":1,"ServerID":2}"#
        )
        try store.save(
            TSSHResumeState(
                serverIdentity: TSSHResumeServerIdentity(server: server),
                sshHostKeyFingerprint: "SHA256:trusted",
                serverProcess: TSSHServerProcessIdentity(
                    pid: 123,
                    supervisorPath: "/tmp/orlix-tsshd-test.sh"
                ),
                host: server.host,
                info: info,
                sessionID: 42,
                profile: server.tsshProfile,
                savedAt: Date()
            ),
            for: paneID
        )
        #expect(try keychainAccounts(service: service).contains { account in
            account.hasPrefix("\(paneID.uuidString).")
        })

        let checkpoint = root
            .appendingPathComponent(paneID.uuidString)
            .appendingPathExtension("json")
        try Data("{".utf8).write(to: checkpoint, options: .atomic)
        try store.delete(for: paneID)

        #expect(try keychainAccounts(service: service).allSatisfy { account in
            !account.hasPrefix("\(paneID.uuidString).")
        })
        #expect(!fileManager.fileExists(atPath: checkpoint.path))
    }

    @Test
    func checkpointRecoveryRetainsEndpointBoundCleanupCredentials() throws {
        let paneID = UUID()
        let fileManager = FileManager.default
        let root = fileManager.temporaryDirectory.appendingPathComponent(
            "TSSHResumeStoreTests.\(UUID().uuidString)",
            isDirectory: true
        )
        let service = "com.rudironsoni.orlix.tssh-resume.tests.\(UUID().uuidString)"
        let serviceQuery: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrSynchronizable as String: kCFBooleanFalse as Any,
        ]
        defer {
            SecItemDelete(serviceQuery as CFDictionary)
            try? fileManager.removeItem(at: root)
        }
        let store = TSSHResumeStore(fileManager: fileManager, root: root, service: service)
        let server = Server(
            workspaceId: UUID(),
            name: "TSSH",
            host: "old.example.com",
            port: 22,
            username: "root",
            connectionMode: .tssh
        )
        let credentials = ServerCredentials(
            serverId: server.id,
            credentialBinding: ServerCredentialBinding(server: server),
            password: "old-password"
        )
        let info = try TSSHServerInfo.parse(
            output: #"{"ServerVer":"0.2.2","Port":61000,"Mode":"KCP","Pass":"aa","Salt":"bb","ProxyKey":"cc","ClientID":1,"ServerID":2}"#
        )
        try store.save(
            TSSHResumeState(
                serverIdentity: TSSHResumeServerIdentity(server: server),
                sshHostKeyFingerprint: "SHA256:trusted",
                serverProcess: TSSHServerProcessIdentity(
                    pid: 123,
                    supervisorPath: "/tmp/orlix-tsshd-test.sh"
                ),
                host: server.host,
                info: info,
                sessionID: 42,
                profile: server.tsshProfile,
                cleanupCredentials: credentials,
                savedAt: Date()
            ),
            for: paneID
        )

        #expect(store.recoverCleanupState(for: paneID)?.credentials == credentials)
    }

    @Test
    func corruptCleanupReferenceRecoversAndPurgesItsVersionedSecret() throws {
        let paneID = UUID()
        let fileManager = FileManager.default
        let root = fileManager.temporaryDirectory.appendingPathComponent(
            "TSSHResumeStoreTests.\(UUID().uuidString)",
            isDirectory: true
        )
        let service = "com.rudironsoni.orlix.tssh-resume.tests.\(UUID().uuidString)"
        let serviceQuery: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrSynchronizable as String: kCFBooleanFalse as Any,
        ]
        defer {
            SecItemDelete(serviceQuery as CFDictionary)
            try? fileManager.removeItem(at: root)
        }
        let store = TSSHResumeStore(fileManager: fileManager, root: root, service: service)
        let server = Server(
            workspaceId: UUID(),
            name: "TSSH",
            host: "example.com",
            port: 22,
            username: "root",
            connectionMode: .tssh
        )
        let state = TSSHResumeCleanupState(
            serverIdentity: TSSHResumeServerIdentity(server: server),
            serverProcess: TSSHServerProcessIdentity(
                pid: 123,
                supervisorPath: "/tmp/orlix-tsshd-test.sh"
            ),
            credentials: ServerCredentials(
                serverId: server.id,
                credentialBinding: ServerCredentialBinding(server: server),
                password: "password"
            ),
            createdAt: Date()
        )
        try store.saveCleanup(state, for: paneID)
        let cleanupReference = root
            .appendingPathComponent(paneID.uuidString)
            .appendingPathExtension("cleanup.json")
        try Data("{".utf8).write(to: cleanupReference, options: .atomic)

        #expect(try store.loadCleanup(for: paneID) == state)
        try store.deleteCleanup(for: paneID)

        #expect(try keychainAccounts(service: service).allSatisfy { account in
            !account.hasPrefix("\(paneID.uuidString).cleanup.")
        })
        #expect(!fileManager.fileExists(atPath: cleanupReference.path))
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

    private func keychainAccounts(service: String) throws -> [String] {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrSynchronizable as String: kCFBooleanFalse as Any,
            kSecReturnAttributes as String: true,
            kSecMatchLimit as String: kSecMatchLimitAll,
        ]
        var result: CFTypeRef?
        let status = SecItemCopyMatching(query as CFDictionary, &result)
        if status == errSecItemNotFound { return [] }
        guard status == errSecSuccess else {
            throw TSSHResumeStoreError.secureStorage(status)
        }
        let attributes = result as? [[String: Any]] ?? []
        return attributes.compactMap { $0[kSecAttrAccount as String] as? String }
    }
}
