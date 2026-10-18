//
//  ViewTabConfigurationManager.swift
//  Orlix
//

import Combine
import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
@MainActor
package protocol ConnectionViewTabConfigurationPersisting {
    func load() -> ConnectionViewTabConfiguration
    func save(_ configuration: ConnectionViewTabConfiguration)
}

@MainActor
final package class ViewTabConfigurationManager: ObservableObject {
    private let persistence: any ConnectionViewTabConfigurationPersisting

    @Published private(set) var configuration: ConnectionViewTabConfiguration

    package init(persistence: any ConnectionViewTabConfigurationPersisting) {
        self.persistence = persistence
        configuration = persistence.load()
    }

    package var tabOrder: [ConnectionViewTabID] {
        configuration.order
    }

    package var currentVisibleTabs: [ConnectionViewTabID] {
        configuration.orderedVisibleTabs
    }

    package func moveTab(from source: IndexSet, to destination: Int) {
        guard source.allSatisfy(configuration.order.indices.contains),
              destination >= 0,
              destination <= configuration.order.count else {
            return
        }

        var order = configuration.order
        let movingTabs = source.map { order[$0] }
        for index in source.sorted(by: >) {
            order.remove(at: index)
        }
        let removedBeforeDestination = source.count(in: ..<destination)
        order.insert(
            contentsOf: movingTabs,
            at: destination - removedBeforeDestination
        )
        updateConfiguration(
            ConnectionViewTabConfiguration(
                order: order,
                visibleTabs: configuration.visibleTabs,
                defaultTab: configuration.defaultTab
            )
        )
    }

    package func setDefaultTab(_ tabID: ConnectionViewTabID) {
        updateConfiguration(
            ConnectionViewTabConfiguration(
                order: configuration.order,
                visibleTabs: configuration.visibleTabs,
                defaultTab: tabID
            )
        )
    }

    package func setVisibility(for tabID: ConnectionViewTabID, isVisible: Bool) {
        var visibleTabs = configuration.visibleTabs

        if isVisible {
            visibleTabs.insert(tabID)
        } else {
            guard visibleTabs.contains(tabID), visibleTabs.count > 1 else { return }
            visibleTabs.remove(tabID)
        }

        updateConfiguration(
            ConnectionViewTabConfiguration(
                order: configuration.order,
                visibleTabs: visibleTabs,
                defaultTab: configuration.defaultTab
            )
        )
    }

    package func resetToDefaults() {
        updateConfiguration(.default)
    }

    package func effectiveDefaultTab() -> ConnectionViewTabID {
        configuration.effectiveDefaultTab
    }

    package func isTabVisible(_ tabID: ConnectionViewTabID) -> Bool {
        configuration.visibleTabs.contains(tabID)
    }

    package func effectiveView(for storedView: ConnectionViewTabID?) -> ConnectionViewTabID {
        configuration.effectiveView(for: storedView)
    }

    private func updateConfiguration(_ newConfiguration: ConnectionViewTabConfiguration) {
        guard newConfiguration != configuration else { return }
        configuration = newConfiguration
        persistence.save(newConfiguration)
    }
}
