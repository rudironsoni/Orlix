import Combine
import Foundation
#if ORLIX_MODULAR_APP
import OrlixModels
#endif

/// Owns the selected connection surface for each server.
@MainActor
final package class ConnectionViewSelectionStore: ObservableObject {
    @Published private(set) var selectionsByServer: [UUID: ConnectionViewTabID] = [:]

    package func selection(for serverId: UUID) -> ConnectionViewTabID? {
        selectionsByServer[serverId]
    }

    package func setSelection(_ selection: ConnectionViewTabID?, for serverId: UUID) {
        guard selectionsByServer[serverId] != selection else { return }
        if let selection {
            selectionsByServer[serverId] = selection
        } else {
            selectionsByServer.removeValue(forKey: serverId)
        }
    }

    package func restore(_ selections: [UUID: ConnectionViewTabID]) {
        selectionsByServer = selections
    }

    #if DEBUG
    package func resetForTesting() {
        selectionsByServer.removeAll()
    }
    #endif
}
