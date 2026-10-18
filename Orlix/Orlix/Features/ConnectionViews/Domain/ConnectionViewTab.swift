import Foundation

nonisolated package enum ConnectionViewTabID: String, CaseIterable, Codable, Identifiable, Sendable {
    package case stats
    package case terminal
    package case files

    package var id: Self { self }
}

nonisolated package struct ConnectionViewTabConfiguration: Codable, Equatable, Sendable {
    package var order: [ConnectionViewTabID]
    package var visibleTabs: Set<ConnectionViewTabID>
    package var defaultTab: ConnectionViewTabID

    static let `default` = ConnectionViewTabConfiguration(
        order: ConnectionViewTabID.allCases,
        visibleTabs: Set(ConnectionViewTabID.allCases),
        defaultTab: .stats
    )

    package init(
        order: [ConnectionViewTabID],
        visibleTabs: Set<ConnectionViewTabID>,
        defaultTab: ConnectionViewTabID
    ) {
        var normalizedOrder: [ConnectionViewTabID] = []
        normalizedOrder.reserveCapacity(ConnectionViewTabID.allCases.count)

        for tab in order where !normalizedOrder.contains(tab) {
            normalizedOrder.append(tab)
        }
        for tab in ConnectionViewTabID.allCases where !normalizedOrder.contains(tab) {
            normalizedOrder.append(tab)
        }

        self.order = normalizedOrder
        self.visibleTabs = visibleTabs.isEmpty
            ? Set(ConnectionViewTabID.allCases)
            : visibleTabs
        self.defaultTab = defaultTab
    }

    package var orderedVisibleTabs: [ConnectionViewTabID] {
        order.filter(visibleTabs.contains)
    }

    package var effectiveDefaultTab: ConnectionViewTabID {
        if visibleTabs.contains(defaultTab) {
            return defaultTab
        }
        return orderedVisibleTabs.first ?? .stats
    }

    package func effectiveView(for storedView: ConnectionViewTabID?) -> ConnectionViewTabID {
        guard let storedView, visibleTabs.contains(storedView) else {
            return effectiveDefaultTab
        }
        return storedView
    }

    private enum CodingKeys: String, CodingKey {
        case order
        case visibleTabs
        case defaultTab
    }

    package init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        self.init(
            order: try container.decode([ConnectionViewTabID].self, forKey: .order),
            visibleTabs: Set(try container.decode([ConnectionViewTabID].self, forKey: .visibleTabs)),
            defaultTab: try container.decode(ConnectionViewTabID.self, forKey: .defaultTab)
        )
    }

    package func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(order, forKey: .order)
        try container.encode(
            ConnectionViewTabID.allCases.filter(visibleTabs.contains),
            forKey: .visibleTabs
        )
        try container.encode(defaultTab, forKey: .defaultTab)
    }
}
