import Foundation
import Testing
@testable import Orlix

struct TSSHProfileTests {
    @Test
    func defaultProfileMatchesTSSHDDefaults() {
        let profile = TSSHProfile()

        #expect(profile.transportMode == .kcp)
        #expect(profile.udpPortMinimum == 61_000)
        #expect(profile.udpPortMaximum == 61_999)
        #expect(profile.mtu == 0)
        #expect(profile.connectTimeoutSeconds == 30)
        #expect(profile.vpnDNSServers == ["1.1.1.1", "1.0.0.1"])
        #expect(profile.isValid)
    }

    @Test
    func profileRoundTripPreservesNativeTransportFeatures() throws {
        let forward = TSSHPortForwardRule(
            direction: .local,
            bindAddress: "127.0.0.1",
            bindPort: 8_080,
            targetHost: "127.0.0.1",
            targetPort: 80
        )
        let profile = TSSHProfile(
            transportMode: .quic,
            udpPortMinimum: 62_000,
            udpPortMaximum: 62_100,
            serverPath: "/opt/tssh/tsshd",
            mtu: 1_280,
            keepPendingInput: true,
            keepPendingOutput: true,
            sshAgentForwarding: true,
            sshAgentApprovalMode: .perSession,
            keepTunnelsInBackground: true,
            vpnEnabled: true,
            blockQUICInVPN: true,
            vpnDNSServers: ["9.9.9.9"],
            vpnExcludedRoutes: ["10.0.0.0/8"],
            connectTimeoutSeconds: 45,
            aliveTimeoutSeconds: 90,
            heartbeatTimeoutSeconds: 15,
            forwards: [forward]
        )

        let data = try JSONEncoder().encode(profile)
        let decoded = try JSONDecoder().decode(TSSHProfile.self, from: data)

        #expect(decoded == profile)
        #expect(decoded.isValid)
    }

    @Test(arguments: [
        TSSHProfile(udpPortMinimum: 0),
        TSSHProfile(udpPortMinimum: 62_000, udpPortMaximum: 61_000),
        TSSHProfile(mtu: 575),
        TSSHProfile(connectTimeoutSeconds: 0),
        TSSHProfile(heartbeatTimeoutSeconds: 3_601),
        TSSHProfile(vpnDNSServers: []),
        TSSHProfile(serverPath: "bad\npath"),
        TSSHProfile(forwards: [
            TSSHPortForwardRule(
                direction: .local,
                bindPort: 8_080,
                targetHost: "",
                targetPort: 80
            ),
        ]),
    ])
    func rejectsInvalidProfiles(_ profile: TSSHProfile) {
        #expect(!profile.isValid)
    }

    @Test
    func serverRoundTripPreservesTSSHModeAndProfile() throws {
        let profile = TSSHProfile(
            transportMode: .quic,
            keepPendingInput: true,
            forwards: [
                TSSHPortForwardRule(
                    direction: .dynamic,
                    bindPort: 10_800
                ),
            ]
        )
        let server = Server(
            workspaceId: UUID(),
            name: "TSSH",
            host: "example.com",
            port: 22,
            tsshProfile: profile,
            username: "rudi",
            connectionMode: .tssh
        )

        let data = try JSONEncoder().encode(server)
        let decoded = try JSONDecoder().decode(Server.self, from: data)

        #expect(decoded.connectionMode == .tssh)
        #expect(decoded.tsshProfile == profile)
        #expect(ServerTransportSelection(server: decoded) == .tssh)
    }

    @Test
    func oldProfileDataUsesSafeFeatureDefaults() throws {
        let data = Data(#"{"transportMode":"kcp","udpPortMinimum":61000,"udpPortMaximum":61999,"mtu":0}"#.utf8)

        let profile = try JSONDecoder().decode(TSSHProfile.self, from: data)

        #expect(profile.sshAgentApprovalMode == .perRequest)
        #expect(profile.connectTimeoutSeconds == 30)
        #expect(profile.vpnDNSServers == ["1.1.1.1", "1.0.0.1"])
        #expect(profile.isValid)
    }
}
