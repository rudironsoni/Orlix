import Foundation
import NetworkExtension
import VPNTunnel

final class PacketTunnelProvider: NEPacketTunnelProvider, @unchecked Sendable {
    private struct State {
        var callback: PacketTunnelCallback?
        var isRunning = false
    }

    private let packetQueue = DispatchQueue(
        label: "com.rudironsoni.orlix.packet-tunnel",
        qos: .userInitiated
    )
    private let stateLock = NSLock()
    private var state = State()

    override func startTunnel(
        options: [String: NSObject]?,
        completionHandler: @escaping (Error?) -> Void
    ) {
        guard let tunnelProtocol = protocolConfiguration as? NETunnelProviderProtocol,
              let configuration = tunnelProtocol.providerConfiguration,
              let configKey = configuration["tsshConfigKey"] as? String,
              let host = configuration["tsshHost"] as? String else {
            completionHandler(PacketTunnelError.invalidConfiguration)
            return
        }
        let configJSON: String
        do {
            configJSON = try TSSHVPNSecretStore.load(key: configKey)
        } catch {
            completionHandler(error)
            return
        }

        let settings = NEPacketTunnelNetworkSettings(tunnelRemoteAddress: host)
        let routeHost = host.trimmingCharacters(in: CharacterSet(charactersIn: "[]"))
        let ipv4 = NEIPv4Settings(
            addresses: ["198.18.0.2"],
            subnetMasks: ["255.255.255.252"]
        )
        ipv4.includedRoutes = [.default()]
        if routeHost.range(of: #"^\d{1,3}(\.\d{1,3}){3}$"#, options: .regularExpression) != nil {
            ipv4.excludedRoutes = [NEIPv4Route(destinationAddress: routeHost, subnetMask: "255.255.255.255")]
        }
        settings.ipv4Settings = ipv4

        let ipv6 = NEIPv6Settings(
            addresses: ["fd00:6f72:6c69:7800::2"],
            networkPrefixLengths: [64]
        )
        ipv6.includedRoutes = [.default()]
        if routeHost.contains(":") {
            ipv6.excludedRoutes = [
                NEIPv6Route(destinationAddress: routeHost, networkPrefixLength: 128),
            ]
        }
        settings.ipv6Settings = ipv6

        let dns = NEDNSSettings(servers: ["1.1.1.1", "1.0.0.1"])
        dns.matchDomains = [""]
        settings.dnsSettings = dns
        settings.mtu = 1_400

        setTunnelNetworkSettings(settings) { [weak self] error in
            guard let self else {
                completionHandler(PacketTunnelError.stopped)
                return
            }
            if let error {
                completionHandler(error)
                return
            }
            self.startNativeTunnel(configJSON: configJSON, completionHandler: completionHandler)
        }
    }

    override func stopTunnel(
        with reason: NEProviderStopReason,
        completionHandler: @escaping () -> Void
    ) {
        updateState(isRunning: false, callback: nil)
        var nativeError: NSError?
        _ = VpntunnelStopTunnel(&nativeError)
        completionHandler()
    }

    private func readPacketsFromSystem() {
        guard isRunning else { return }
        packetFlow.readPackets { [weak self] packets, protocols in
            guard let self, self.isRunning else { return }
            for (packet, family) in zip(packets, protocols) {
                VpntunnelInjectPacket(packet, family.intValue)
            }
            self.readPacketsFromSystem()
        }
    }

    private func writePacketsToSystem() {
        while isRunning {
            guard let packet = VpntunnelReadPacket(), let data = packet.data else { break }
            packetFlow.writePackets([data], withProtocols: [NSNumber(value: packet.family)])
        }
    }

    private func startNativeTunnel(
        configJSON: String,
        completionHandler: @escaping (Error?) -> Void
    ) {
        let callback = PacketTunnelCallback { [weak self] reason in
            self?.cancelTunnelWithError(PacketTunnelError.nativeFailure(reason))
        }
        var nativeError: NSError?
        guard VpntunnelStartTunnel(configJSON, callback, &nativeError) else {
            completionHandler(
                nativeError ?? PacketTunnelError.nativeFailure("Native VPN start failed")
            )
            return
        }
        updateState(isRunning: true, callback: callback)
        readPacketsFromSystem()
        packetQueue.async { [weak self] in self?.writePacketsToSystem() }
        completionHandler(nil)
    }

    private var isRunning: Bool {
        stateLock.lock()
        defer { stateLock.unlock() }
        return state.isRunning
    }

    private func updateState(isRunning: Bool, callback: PacketTunnelCallback?) {
        stateLock.lock()
        state.isRunning = isRunning
        state.callback = callback
        stateLock.unlock()
    }
}

private final class PacketTunnelCallback: NSObject, VpntunnelTunnelCallbackProtocol {
    private let failure: (String) -> Void
    init(failure: @escaping (String) -> Void) { self.failure = failure }
    func onTunnelReady() {}
    func onStatsUpdate(_ bytesIn: Int64, bytesOut: Int64, activeConns: Int) {}
    func onTunnelDisconnected(_ reason: String?) { failure(reason ?? "Tunnel disconnected") }
    func onTunnelError(_ message: String?) { failure(message ?? "Tunnel failed") }
}

private enum PacketTunnelError: LocalizedError {
    case invalidConfiguration
    case stopped
    case nativeFailure(String)

    var errorDescription: String? {
        switch self {
        case .invalidConfiguration: return "The Orlix VPN configuration is invalid."
        case .stopped: return "The Orlix VPN provider stopped."
        case .nativeFailure(let message): return message
        }
    }
}
