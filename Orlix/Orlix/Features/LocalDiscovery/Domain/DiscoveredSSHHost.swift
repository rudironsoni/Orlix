import Foundation

nonisolated package enum DiscoverySource: String, CaseIterable, Codable, Hashable, Sendable {
    case bonjour
    case portScan
}

nonisolated package struct DiscoveredSSHHost: Identifiable, Hashable, Sendable {
    package var displayName: String
    package var host: String
    package var port: Int
    package var sources: Set<DiscoverySource>
    package var lastSeenAt: Date
    package var latencyMs: Int?

    package var id: String {
        "\(host):\(port)"
    }

    package init(
        displayName: String,
        host: String,
        port: Int = 22,
        sources: Set<DiscoverySource>,
        lastSeenAt: Date = Date(),
        latencyMs: Int? = nil
    ) {
        self.displayName = displayName.isEmpty ? host : displayName
        self.host = host
        self.port = port
        self.sources = sources
        self.lastSeenAt = lastSeenAt
        self.latencyMs = latencyMs
    }

    mutating package func merge(with newer: DiscoveredSSHHost) {
        if !newer.displayName.isEmpty && newer.displayName != newer.host {
            displayName = newer.displayName
        }
        sources.formUnion(newer.sources)
        lastSeenAt = max(lastSeenAt, newer.lastSeenAt)
        if let newerLatency = newer.latencyMs {
            latencyMs = newerLatency
        }
    }
}
