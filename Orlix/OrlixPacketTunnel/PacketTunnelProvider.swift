import Darwin
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
        guard let policyData = configJSON.data(using: .utf8),
              let policy = try? JSONDecoder().decode(
                  TSSHVPNNetworkPolicy.self,
                  from: policyData
              ) else {
            completionHandler(PacketTunnelError.invalidConfiguration)
            return
        }

        let routeHost = host.trimmingCharacters(in: CharacterSet(charactersIn: "[]"))
        let resolvedHosts = Self.resolveHostAddresses(routeHost)
        guard !resolvedHosts.isEmpty else {
            completionHandler(PacketTunnelError.unresolvedHost(routeHost))
            return
        }
        let settings = NEPacketTunnelNetworkSettings(
            tunnelRemoteAddress: resolvedHosts[0]
        )
        let ipv4 = NEIPv4Settings(
            addresses: ["198.18.0.2"],
            subnetMasks: ["255.255.255.252"]
        )
        ipv4.includedRoutes = [.default()]
        var ipv4Routes: [NEIPv4Route] = []
        var ipv6Routes: [NEIPv6Route] = []
        for route in policy.excludedRoutes + resolvedHosts.compactMap(
            TSSHVPNNetworkPolicy.parseExcludedRoute
        ) {
            switch route {
            case .ipv4(let address, let subnetMask):
                ipv4Routes.append(NEIPv4Route(
                    destinationAddress: address,
                    subnetMask: subnetMask
                ))
            case .ipv6(let address, let prefixLength):
                ipv6Routes.append(NEIPv6Route(
                    destinationAddress: address,
                    networkPrefixLength: NSNumber(value: prefixLength)
                ))
            }
        }
        ipv4.excludedRoutes = ipv4Routes
        settings.ipv4Settings = ipv4

        let ipv6 = NEIPv6Settings(
            addresses: ["fd00:6f72:6c69:7800::2"],
            networkPrefixLengths: [64]
        )
        ipv6.includedRoutes = [.default()]
        ipv6.excludedRoutes = ipv6Routes
        settings.ipv6Settings = ipv6

        let dns = NEDNSSettings(servers: policy.dnsServers)
        dns.matchDomains = [""]
        settings.dnsSettings = dns
        settings.mtu = NSNumber(value: policy.mtu)

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
        let startup = PacketTunnelStartupCompletion(completionHandler)
        let callback = PacketTunnelCallback(
            ready: {
                startup.succeed()
            },
            failure: { [weak self] reason, wasReady in
                let error = PacketTunnelError.nativeFailure(reason)
                if wasReady {
                    self?.cancelTunnelWithError(error)
                } else {
                    self?.updateState(isRunning: false, callback: nil)
                    var nativeError: NSError?
                    _ = VpntunnelStopTunnel(&nativeError)
                    startup.fail(error)
                }
            }
        )
        var nativeError: NSError?
        guard VpntunnelStartTunnel(configJSON, callback, &nativeError) else {
            startup.fail(
                nativeError ?? PacketTunnelError.nativeFailure("Native VPN start failed")
            )
            return
        }
        updateState(isRunning: true, callback: callback)
        readPacketsFromSystem()
        packetQueue.async { [weak self] in self?.writePacketsToSystem() }
        callback.activate()
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

    private static func resolveHostAddresses(_ host: String) -> [String] {
        if TSSHVPNNetworkPolicy.isIPAddress(host) { return [host] }
        var hints = addrinfo()
        hints.ai_family = AF_UNSPEC
        hints.ai_socktype = SOCK_DGRAM
        var result: UnsafeMutablePointer<addrinfo>?
        let status = host.withCString { getaddrinfo($0, nil, &hints, &result) }
        guard status == 0, let result else { return [] }
        defer { freeaddrinfo(result) }

        var addresses: [String] = []
        var cursor: UnsafeMutablePointer<addrinfo>? = result
        while let info = cursor?.pointee {
            var buffer = [CChar](repeating: 0, count: Int(NI_MAXHOST))
            if getnameinfo(
                info.ai_addr,
                info.ai_addrlen,
                &buffer,
                socklen_t(buffer.count),
                nil,
                0,
                NI_NUMERICHOST
            ) == 0 {
                let address = String(cString: buffer)
                if !addresses.contains(address) { addresses.append(address) }
            }
            cursor = info.ai_next
        }
        return addresses
    }
}

private final class PacketTunnelStartupCompletion: @unchecked Sendable {
    private let lock = NSLock()
    private var completion: ((Error?) -> Void)?

    init(_ completion: @escaping (Error?) -> Void) {
        self.completion = completion
    }

    func succeed() { finish(nil) }
    func fail(_ error: Error) { finish(error) }

    private func finish(_ error: Error?) {
        lock.lock()
        let completion = self.completion
        self.completion = nil
        lock.unlock()
        completion?(error)
    }
}

private final class PacketTunnelCallback: NSObject, VpntunnelTunnelCallbackProtocol {
    private let lock = NSLock()
    private let ready: () -> Void
    private let failure: (_ reason: String, _ wasReady: Bool) -> Void
    private var isActivated = false
    private var didBecomeReady = false
    private var terminationReason: String?

    init(
        ready: @escaping () -> Void,
        failure: @escaping (_ reason: String, _ wasReady: Bool) -> Void
    ) {
        self.ready = ready
        self.failure = failure
    }

    func activate() {
        lock.lock()
        isActivated = true
        let reason = terminationReason
        let becameReady = didBecomeReady
        lock.unlock()
        if let reason {
            failure(reason, becameReady)
        } else if becameReady {
            ready()
        }
    }

    func onTunnelReady() {
        lock.lock()
        guard !didBecomeReady, terminationReason == nil else {
            lock.unlock()
            return
        }
        didBecomeReady = true
        let shouldNotify = isActivated
        lock.unlock()
        if shouldNotify { ready() }
    }
    func onStatsUpdate(_ bytesIn: Int64, bytesOut: Int64, activeConns: Int) {}
    func onTunnelDisconnected(_ reason: String?) {
        terminate(reason ?? "Tunnel disconnected")
    }
    func onTunnelError(_ message: String?) {
        terminate(message ?? "Tunnel failed")
    }

    private func terminate(_ reason: String) {
        lock.lock()
        guard terminationReason == nil else {
            lock.unlock()
            return
        }
        terminationReason = reason
        let wasReady = didBecomeReady
        let shouldNotify = isActivated
        lock.unlock()
        if shouldNotify { failure(reason, wasReady) }
    }
}

private enum PacketTunnelError: LocalizedError {
    case invalidConfiguration
    case unresolvedHost(String)
    case stopped
    case nativeFailure(String)

    var errorDescription: String? {
        switch self {
        case .invalidConfiguration: return "The Orlix VPN configuration is invalid."
        case .unresolvedHost(let host): return "The Orlix VPN host could not be resolved: \(host)"
        case .stopped: return "The Orlix VPN provider stopped."
        case .nativeFailure(let message): return message
        }
    }
}
