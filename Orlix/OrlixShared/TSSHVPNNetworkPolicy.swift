import Darwin
import Foundation

nonisolated enum TSSHVPNExcludedRoute: Equatable, Sendable {
    case ipv4(address: String, subnetMask: String)
    case ipv6(address: String, prefixLength: Int)
}

nonisolated struct TSSHVPNNetworkPolicy: Decodable, Equatable, Sendable {
    let dnsServers: [String]
    let excludedRoutes: [TSSHVPNExcludedRoute]
    let mtu: Int

    private enum CodingKeys: String, CodingKey {
        case dnsServers, excludedRoutes, mtu
    }

    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        let dnsServers = try container.decode([String].self, forKey: .dnsServers)
        let routeValues = try container.decode([String].self, forKey: .excludedRoutes)
        let mtu = try container.decode(Int.self, forKey: .mtu)
        guard !dnsServers.isEmpty,
              dnsServers.count <= 8,
              dnsServers.allSatisfy(Self.isIPAddress),
              routeValues.count <= 64,
              mtu >= 576,
              mtu <= 9_000 else {
            throw TSSHVPNNetworkPolicyError.invalidConfiguration
        }
        let routes = routeValues.compactMap(Self.parseExcludedRoute)
        guard routes.count == routeValues.count else {
            throw TSSHVPNNetworkPolicyError.invalidConfiguration
        }
        self.dnsServers = dnsServers
        excludedRoutes = routes
        self.mtu = mtu
    }

    static func isIPAddress(_ value: String) -> Bool {
        isIPv4Address(value) || isIPv6Address(value)
    }

    static func parseExcludedRoute(_ value: String) -> TSSHVPNExcludedRoute? {
        let components = value.split(separator: "/", omittingEmptySubsequences: false)
        guard components.count <= 2 else { return nil }
        let address = String(components[0])
        if isIPv4Address(address) {
            let prefix = components.count == 2 ? Int(components[1]) : 32
            guard let prefix, (0...32).contains(prefix) else { return nil }
            return .ipv4(address: address, subnetMask: ipv4Mask(prefixLength: prefix))
        }
        if isIPv6Address(address) {
            let prefix = components.count == 2 ? Int(components[1]) : 128
            guard let prefix, (0...128).contains(prefix) else { return nil }
            return .ipv6(address: address, prefixLength: prefix)
        }
        return nil
    }

    private static func isIPv4Address(_ value: String) -> Bool {
        var address = in_addr()
        return value.withCString { inet_pton(AF_INET, $0, &address) == 1 }
    }

    private static func isIPv6Address(_ value: String) -> Bool {
        var address = in6_addr()
        return value.withCString { inet_pton(AF_INET6, $0, &address) == 1 }
    }

    private static func ipv4Mask(prefixLength: Int) -> String {
        let bits: UInt32 = switch prefixLength {
        case 0: 0
        case 32: .max
        default: UInt32.max << (32 - prefixLength)
        }
        return "\((bits >> 24) & 0xff).\((bits >> 16) & 0xff).\((bits >> 8) & 0xff).\(bits & 0xff)"
    }
}

private nonisolated enum TSSHVPNNetworkPolicyError: Error {
    case invalidConfiguration
}
