import Foundation
import Testing
@testable import Orlix

private actor ServerConnectionOperationRunnerFake: ServerConnectionOperationRunning {
    enum Behavior: Sendable {
        case run
        case cancel
        case hostKeyApprovalRequired
    }

    private let behavior: Behavior
    private var callCount = 0

    init(behavior: Behavior = .run) {
        self.behavior = behavior
    }

    func runServerConnectionTest(
        server: Server,
        credentials: ServerCredentials,
        operation: @escaping @Sendable (SSHClient) async throws -> Void
    ) async throws {
        callCount += 1
        switch behavior {
        case .run:
            try await operation(SSHClient.testing())
        case .cancel:
            throw CancellationError()
        case .hostKeyApprovalRequired:
            throw SSHError.hostKeyApprovalRequired
        }
    }

    func calls() -> Int {
        callCount
    }
}

private actor ServerMoshConnectionTesterFake: ServerMoshConnectionTesting {
    private var testedPortRanges: [ClosedRange<Int>] = []

    func testServerConnection(
        using client: SSHClient,
        portRange: ClosedRange<Int>
    ) async throws {
        testedPortRanges.append(portRange)
    }

    func portRanges() -> [ClosedRange<Int>] {
        testedPortRanges
    }
}

private actor ServerTSSHConnectionTesterFake: ServerTSSHConnectionTesting {
    private var testedPortRanges: [ClosedRange<Int>] = []
    private var testedModes: [TSSHTransportMode] = []

    func testServerConnection(
        server: Server,
        credentials: ServerCredentials,
        using client: SSHClient,
        portRange: ClosedRange<Int>
    ) async throws {
        testedPortRanges.append(portRange)
        testedModes.append(server.tsshProfile.transportMode)
    }

    func portRanges() -> [ClosedRange<Int>] {
        testedPortRanges
    }

    func modes() -> [TSSHTransportMode] {
        testedModes
    }
}

private nonisolated struct ConnectionTestHostKeyRepositoryFake: ServerHostKeyRepository {
    private let challenge: KnownHostsManager.Challenge?

    init(challenge: KnownHostsManager.Challenge? = nil) {
        self.challenge = challenge
    }

    func pendingChallenge(for host: String, port: Int, now: Date) -> KnownHostsManager.Challenge? {
        challenge
    }

    func approve(_ challenge: KnownHostsManager.Challenge, now: Date) -> Bool {
        false
    }

    func reject(_ challenge: KnownHostsManager.Challenge) {}
}

struct AppServerConnectionTestPlanTests {
    @Test
    func standardUsesOnlyTheSSHProbe() {
        #expect(ServerConnectionTestPlan(server: makeServer(mode: .standard)) == .sshOnly)
    }

    @Test
    func tailscaleUsesOnlyTheSSHProbe() {
        #expect(ServerConnectionTestPlan(server: makeServer(mode: .tailscale)) == .sshOnly)
    }

    @Test
    func cloudflareUsesOnlyTheSSHProbe() {
        #expect(ServerConnectionTestPlan(server: makeServer(mode: .cloudflare)) == .sshOnly)
    }

    @Test
    func moshUsesTheBoundedBootstrapPortRange() {
        #expect(
            ServerConnectionTestPlan(server: makeServer(mode: .mosh))
                == .mosh(portRange: 60_001...61_000)
        )
    }

    @Test
    func eternalTerminalUsesItsConfiguredPortAndBoundsInvalidValues() {
        var server = makeServer(mode: .eternalTerminal)
        server.eternalTerminalPort = 22_022
        #expect(ServerConnectionTestPlan(server: server) == .eternalTerminal(port: 22_022))

        server.eternalTerminalPort = Int.max
        #expect(ServerConnectionTestPlan(server: server) == .eternalTerminal(port: 2_022))

        server.eternalTerminalPort = 0
        #expect(ServerConnectionTestPlan(server: server) == .eternalTerminal(port: 2_022))
    }

    @Test
    func tsshUsesItsConfiguredBootstrapRange() {
        var server = makeServer(mode: .tssh)
        server.tsshProfile = TSSHProfile(
            transportMode: .quic,
            udpPortMinimum: 62_000,
            udpPortMaximum: 62_100
        )

        #expect(
            ServerConnectionTestPlan(server: server)
                == .tssh(portRange: 62_000...62_100)
        )
    }

    @Test
    func hostKeyFailureRequiresApprovalForTheCurrentEndpoint() {
        let server = makeServer(mode: .standard)

        #expect(
            ServerConnectionApprovalPolicy.requirement(
                for: SSHError.hostKeyApprovalRequired,
                server: server
            ) == .hostKey(host: server.host, port: server.port)
        )
    }

    @Test
    func unrelatedFailureDoesNotRequestApproval() {
        #expect(
            ServerConnectionApprovalPolicy.requirement(
                for: SSHError.authenticationFailed,
                server: makeServer(mode: .standard)
            ) == nil
        )
    }

    @Test
    func testerUsesOnlyItsInjectedConnectionAndMoshOwners() async {
        let usedConnectionOperations = ServerConnectionOperationRunnerFake()
        let unusedConnectionOperations = ServerConnectionOperationRunnerFake()
        let usedMosh = ServerMoshConnectionTesterFake()
        let unusedMosh = ServerMoshConnectionTesterFake()
        let tester = AppServerConnectionTester(
            connectionOperations: usedConnectionOperations,
            remoteMosh: usedMosh,
            nativeTSSH: ServerTSSHConnectionTesterFake(),
            hostKeys: ConnectionTestHostKeyRepositoryFake(),
            now: { .distantPast }
        )
        let server = makeServer(mode: .mosh)

        let result = await tester.test(
            server: server,
            credentials: ServerCredentials(serverId: server.id)
        )

        #expect(result == .success)
        #expect(await usedConnectionOperations.calls() == 1)
        #expect(await unusedConnectionOperations.calls() == 0)
        #expect(await usedMosh.portRanges() == [60_001...61_000])
        #expect(await unusedMosh.portRanges().isEmpty)
    }

    @Test
    func testerPassesTheConfiguredRangeToTheNativeTSSHProbe() async {
        let nativeTSSH = ServerTSSHConnectionTesterFake()
        var server = makeServer(mode: .tssh)
        server.tsshProfile = TSSHProfile(
            transportMode: .quic,
            udpPortMinimum: 62_000,
            udpPortMaximum: 62_100
        )
        let tester = AppServerConnectionTester(
            connectionOperations: ServerConnectionOperationRunnerFake(),
            remoteMosh: ServerMoshConnectionTesterFake(),
            nativeTSSH: nativeTSSH,
            hostKeys: ConnectionTestHostKeyRepositoryFake(),
            now: { .distantPast }
        )

        let result = await tester.test(
            server: server,
            credentials: ServerCredentials(serverId: server.id)
        )

        #expect(result == .success)
        #expect(await nativeTSSH.portRanges() == [62_000...62_100])
        #expect(await nativeTSSH.modes() == [.quic])
    }

    @Test
    func testerMapsInjectedOperationCancellationToCancelled() async {
        let server = makeServer(mode: .standard)
        let tester = AppServerConnectionTester(
            connectionOperations: ServerConnectionOperationRunnerFake(behavior: .cancel),
            remoteMosh: ServerMoshConnectionTesterFake(),
            nativeTSSH: ServerTSSHConnectionTesterFake(),
            hostKeys: ConnectionTestHostKeyRepositoryFake(),
            now: { .distantPast }
        )

        let result = await tester.test(
            server: server,
            credentials: ServerCredentials(serverId: server.id)
        )

        #expect(result == .cancelled)
    }

    @Test
    func unavailableHostKeyChallengeRemainsUnavailable() async {
        let server = makeServer(mode: .standard)
        let tester = AppServerConnectionTester(
            connectionOperations: ServerConnectionOperationRunnerFake(
                behavior: .hostKeyApprovalRequired
            ),
            remoteMosh: ServerMoshConnectionTesterFake(),
            nativeTSSH: ServerTSSHConnectionTesterFake(),
            hostKeys: ConnectionTestHostKeyRepositoryFake(),
            now: { .distantPast }
        )

        let result = await tester.test(
            server: server,
            credentials: ServerCredentials(serverId: server.id)
        )

        guard case .failure(let failure) = result else {
            Issue.record("The host-key error did not return a failure")
            return
        }
        #expect(failure.hostKeyChallenge == nil)
    }

    private func makeServer(mode: SSHConnectionMode) -> Server {
        Server(
            workspaceId: UUID(),
            name: "Server",
            host: "example.com",
            port: 2222,
            username: "root",
            connectionMode: mode
        )
    }
}
