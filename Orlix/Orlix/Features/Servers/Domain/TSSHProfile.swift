import Foundation

nonisolated enum TSSHTransportMode: String, Codable, CaseIterable, Identifiable, Sendable {
    case kcp
    case quic

    var id: String { rawValue }
}

nonisolated enum TSSHForwardDirection: String, Codable, CaseIterable, Identifiable, Sendable {
    case local
    case remote
    case dynamic

    var id: String { rawValue }
}

nonisolated struct TSSHPortForwardRule: Identifiable, Codable, Hashable, Sendable {
    var id: UUID
    var direction: TSSHForwardDirection
    var bindAddress: String
    var bindPort: Int
    var targetHost: String
    var targetPort: Int

    init(
        id: UUID = UUID(),
        direction: TSSHForwardDirection,
        bindAddress: String = "127.0.0.1",
        bindPort: Int,
        targetHost: String = "",
        targetPort: Int = 0
    ) {
        self.id = id
        self.direction = direction
        self.bindAddress = bindAddress
        self.bindPort = bindPort
        self.targetHost = targetHost
        self.targetPort = targetPort
    }

    var isValid: Bool {
        guard (0...65_535).contains(bindPort) else { return false }
        if direction == .remote && bindPort == 0 { return false }
        if direction == .dynamic { return true }
        return !targetHost.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
            && (1...65_535).contains(targetPort)
    }
}

nonisolated struct TSSHProfile: Codable, Hashable, Sendable {
    var transportMode: TSSHTransportMode
    var udpPortMinimum: Int
    var udpPortMaximum: Int
    var serverPath: String?
    var mtu: Int
    var keepPendingInput: Bool
    var keepPendingOutput: Bool
    var sshAgentForwarding: Bool
    var keepTunnelsInBackground: Bool
    var vpnEnabled: Bool
    var blockQUICInVPN: Bool
    var forwards: [TSSHPortForwardRule]

    init(
        transportMode: TSSHTransportMode = .kcp,
        udpPortMinimum: Int = 61_000,
        udpPortMaximum: Int = 61_999,
        serverPath: String? = nil,
        mtu: Int = 0,
        keepPendingInput: Bool = false,
        keepPendingOutput: Bool = false,
        sshAgentForwarding: Bool = false,
        keepTunnelsInBackground: Bool = false,
        vpnEnabled: Bool = false,
        blockQUICInVPN: Bool = false,
        forwards: [TSSHPortForwardRule] = []
    ) {
        self.transportMode = transportMode
        self.udpPortMinimum = udpPortMinimum
        self.udpPortMaximum = udpPortMaximum
        self.serverPath = serverPath
        self.mtu = mtu
        self.keepPendingInput = keepPendingInput
        self.keepPendingOutput = keepPendingOutput
        self.sshAgentForwarding = sshAgentForwarding
        self.keepTunnelsInBackground = keepTunnelsInBackground
        self.vpnEnabled = vpnEnabled
        self.blockQUICInVPN = blockQUICInVPN
        self.forwards = forwards
    }

    var isValid: Bool {
        guard (1...65_535).contains(udpPortMinimum),
              (1...65_535).contains(udpPortMaximum),
              udpPortMinimum <= udpPortMaximum,
              mtu == 0 || (576...9_000).contains(mtu),
              forwards.allSatisfy(\.isValid) else { return false }
        guard let serverPath else { return true }
        let trimmed = serverPath.trimmingCharacters(in: .whitespacesAndNewlines)
        return !trimmed.isEmpty && !trimmed.contains("\n") && !trimmed.contains("\0")
    }
}
