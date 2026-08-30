import Foundation
import Testing
@testable import Orlix

struct TSSHServerInfoTests {
    @Test
    func parsesKCPServerInfoFromMixedBootstrapOutput() throws {
        let output = """
        tsshd starting
        {"ServerVer":"0.2.2","ProtoVer":2,"Port":61001,"Mode":"KCP","Pass":"aabb","Salt":"ccdd","ProxyKey":"eeff","ProxyMode":"TCP","MTU":1280,"ClientID":7,"ServerID":9}
        """

        let info = try TSSHServerInfo.parse(output: output)

        #expect(info.serverVersion == "0.2.2")
        #expect(info.protocolVersion == 2)
        #expect(info.port == 61_001)
        #expect(info.mode == .kcp)
        #expect(info.clientID == 7)
        #expect(info.serverID == 9)
        #expect(info.proxyMode == "TCP")
        #expect(info.mtu == 1_280)
        #expect(info.hasRequiredCredentials)
    }

    @Test
    func parsesQUICServerInfo() throws {
        let output = """
        {"ServerVer":"0.2.2","Port":61999,"Mode":"QUIC","ServerCert":"aa","ClientCert":"bb","ClientKey":"cc","ProxyKey":"dd"}
        """

        let info = try TSSHServerInfo.parse(output: output)

        #expect(info.mode == .quic)
        #expect(info.hasRequiredCredentials)
        #expect(info.assigningClientIDIfNeeded().clientID > 0)
    }

    @Test
    func rejectsIncompleteOrOutOfRangeServerInfo() {
        #expect(throws: TSSHRuntimeError.self) {
            try TSSHServerInfo.parse(
                output: #"{"ServerVer":"0.2.2","Port":0,"Mode":"KCP"}"#
            )
        }
        #expect(throws: TSSHRuntimeError.self) {
            try TSSHServerInfo.parse(
                output: #"{"ServerVer":"0.2.2","Port":61000,"Mode":"KCP","Pass":"aa","Salt":"bb","ProxyKey":"cc","ProxyMode":"UDP"}"#
            )
        }
        #expect(throws: TSSHRuntimeError.self) {
            try TSSHServerInfo.parse(
                output: #"{"ServerVer":"0.2.2","Port":61000,"Mode":"KCP","Pass":"aa","Salt":"bb","ProxyKey":"cc","MTU":575}"#
            )
        }
        #expect(throws: TSSHRuntimeError.self) {
            try TSSHServerInfo.parse(
                output: #"{"ServerVer":"0.2.2","Port":61000,"Mode":"QUIC","ProxyKey":"dd"}"#
            )
        }
        #expect(throws: TSSHRuntimeError.self) {
            try TSSHServerInfo.parse(
                output: #"{"ServerVer":"0.2.2","Port":61000,"Mode":"KCP","Pass":"not-hex","Salt":"bb","ProxyKey":"cc"}"#
            )
        }
        #expect(throws: TSSHRuntimeError.self) {
            try TSSHServerInfo.parse(
                output: #"{"ServerVer":"0.2.2","Port":61000,"Mode":"KCP","Pass":"aa","Salt":"bb","ProxyKey":"cc","ClientID":-1}"#
            )
        }
    }

    @Test
    func reattachAdvancesClientIdentityWithoutOverflow() throws {
        let output = #"{"ServerVer":"0.2.2","Port":61000,"Mode":"KCP","Pass":"aa","Salt":"bb","ProxyKey":"cc","ClientID":18446744073709551615,"ServerID":16138835072071328626}"#
        let info = try TSSHServerInfo.parse(output: output)

        #expect(info.serverID == 16_138_835_072_071_328_626)
        #expect(info.advancingClientID().clientID == 1)
    }

    @Test
    func bootstrapCommandUsesAttachableModeAndConfiguredBounds() {
        let profile = TSSHProfile(
            transportMode: .quic,
            udpPortMinimum: 62_000,
            udpPortMaximum: 62_100,
            serverPath: "/opt/tssh/tsshd",
            mtu: 1_280
        )

        let command = TSSHBootstrap.startCommand(
            profile: profile,
            nonce: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!
        )

        #expect(command.contains("--attachable"))
        #expect(command.contains("62000-62100"))
        #expect(command.contains("--quic"))
        #expect(command.contains("--mtu"))
        #expect(command.contains("1280"))
        #expect(command.contains("nohup"))
        #expect(command.contains("umask 077"))
        #expect(command.contains("trap 'rm -f \\\"\\$output\\\"'"))
    }
}
