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

nonisolated enum TSSHAgentApprovalMode: String, Codable, CaseIterable, Identifiable, Sendable {
    case automatic
    case perSession
    case perRequest

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
    var sshAgentApprovalMode: TSSHAgentApprovalMode
    var keepTunnelsInBackground: Bool
    var vpnEnabled: Bool
    var blockQUICInVPN: Bool
    var vpnDNSServers: [String]
    var vpnExcludedRoutes: [String]
    var connectTimeoutSeconds: Int
    var aliveTimeoutSeconds: Int
    var heartbeatTimeoutSeconds: Int
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
        sshAgentApprovalMode: TSSHAgentApprovalMode = .perRequest,
        keepTunnelsInBackground: Bool = false,
        vpnEnabled: Bool = false,
        blockQUICInVPN: Bool = false,
        vpnDNSServers: [String] = ["1.1.1.1", "1.0.0.1"],
        vpnExcludedRoutes: [String] = [],
        connectTimeoutSeconds: Int = 30,
        aliveTimeoutSeconds: Int = 0,
        heartbeatTimeoutSeconds: Int = 0,
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
        self.sshAgentApprovalMode = sshAgentApprovalMode
        self.keepTunnelsInBackground = keepTunnelsInBackground
        self.vpnEnabled = vpnEnabled
        self.blockQUICInVPN = blockQUICInVPN
        self.vpnDNSServers = vpnDNSServers
        self.vpnExcludedRoutes = vpnExcludedRoutes
        self.connectTimeoutSeconds = connectTimeoutSeconds
        self.aliveTimeoutSeconds = aliveTimeoutSeconds
        self.heartbeatTimeoutSeconds = heartbeatTimeoutSeconds
        self.forwards = forwards
    }

    private enum CodingKeys: String, CodingKey {
        case transportMode, udpPortMinimum, udpPortMaximum, serverPath, mtu
        case keepPendingInput, keepPendingOutput, sshAgentForwarding
        case sshAgentApprovalMode
        case keepTunnelsInBackground, vpnEnabled, blockQUICInVPN
        case vpnDNSServers, vpnExcludedRoutes
        case connectTimeoutSeconds, aliveTimeoutSeconds, heartbeatTimeoutSeconds
        case forwards
    }

    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        transportMode = try container.decodeIfPresent(TSSHTransportMode.self, forKey: .transportMode) ?? .kcp
        udpPortMinimum = try container.decodeIfPresent(Int.self, forKey: .udpPortMinimum) ?? 61_000
        udpPortMaximum = try container.decodeIfPresent(Int.self, forKey: .udpPortMaximum) ?? 61_999
        serverPath = try container.decodeIfPresent(String.self, forKey: .serverPath)
        mtu = try container.decodeIfPresent(Int.self, forKey: .mtu) ?? 0
        keepPendingInput = try container.decodeIfPresent(Bool.self, forKey: .keepPendingInput) ?? false
        keepPendingOutput = try container.decodeIfPresent(Bool.self, forKey: .keepPendingOutput) ?? false
        sshAgentForwarding = try container.decodeIfPresent(Bool.self, forKey: .sshAgentForwarding) ?? false
        sshAgentApprovalMode = try container.decodeIfPresent(
            TSSHAgentApprovalMode.self,
            forKey: .sshAgentApprovalMode
        ) ?? .perRequest
        keepTunnelsInBackground = try container.decodeIfPresent(
            Bool.self,
            forKey: .keepTunnelsInBackground
        ) ?? false
        vpnEnabled = try container.decodeIfPresent(Bool.self, forKey: .vpnEnabled) ?? false
        blockQUICInVPN = try container.decodeIfPresent(Bool.self, forKey: .blockQUICInVPN) ?? false
        vpnDNSServers = try container.decodeIfPresent([String].self, forKey: .vpnDNSServers)
            ?? ["1.1.1.1", "1.0.0.1"]
        vpnExcludedRoutes = try container.decodeIfPresent([String].self, forKey: .vpnExcludedRoutes) ?? []
        connectTimeoutSeconds = try container.decodeIfPresent(Int.self, forKey: .connectTimeoutSeconds) ?? 30
        aliveTimeoutSeconds = try container.decodeIfPresent(Int.self, forKey: .aliveTimeoutSeconds) ?? 0
        heartbeatTimeoutSeconds = try container.decodeIfPresent(
            Int.self,
            forKey: .heartbeatTimeoutSeconds
        ) ?? 0
        forwards = try container.decodeIfPresent([TSSHPortForwardRule].self, forKey: .forwards) ?? []
    }

    var isValid: Bool {
        guard (1...65_535).contains(udpPortMinimum),
              (1...65_535).contains(udpPortMaximum),
              udpPortMinimum <= udpPortMaximum,
              mtu == 0 || (576...9_000).contains(mtu),
              (1...300).contains(connectTimeoutSeconds),
              (0...864_000).contains(aliveTimeoutSeconds),
              (0...3_600).contains(heartbeatTimeoutSeconds),
              !vpnDNSServers.isEmpty,
              vpnDNSServers.count <= 8,
              vpnDNSServers.allSatisfy({ Self.isSafeNetworkValue($0) }),
              vpnExcludedRoutes.count <= 64,
              vpnExcludedRoutes.allSatisfy({ Self.isSafeNetworkValue($0) }),
              forwards.allSatisfy(\.isValid) else { return false }
        guard let serverPath else { return true }
        let trimmed = serverPath.trimmingCharacters(in: .whitespacesAndNewlines)
        return !trimmed.isEmpty
            && trimmed.utf8.count <= 1_024
            && !trimmed.contains("\n")
            && !trimmed.contains("\0")
    }

    private static func isSafeNetworkValue(_ value: String) -> Bool {
        let trimmed = value.trimmingCharacters(in: .whitespacesAndNewlines)
        return !trimmed.isEmpty
            && trimmed.utf8.count <= 255
            && !trimmed.contains("\n")
            && !trimmed.contains("\0")
    }
}
