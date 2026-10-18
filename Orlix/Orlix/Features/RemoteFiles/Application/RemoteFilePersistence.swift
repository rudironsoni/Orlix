import Combine
import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package extension RemoteFileBrowserStore {
    package func loadPersistedStates() {
        if defaults.object(forKey: legacyPersistenceKey) != nil {
            defaults.removeObject(forKey: legacyPersistenceKey)
        }

        guard let data = defaults.data(forKey: persistenceKey),
              let decoded = try? JSONDecoder().decode([String: RemoteFileBrowserPersistedState].self, from: data) else {
            persistedStates = [:]
            return
        }
        persistedStates = decoded
    }

    package func persistedState(for tabId: UUID) -> RemoteFileBrowserPersistedState {
        persistedStates[tabId.uuidString] ?? .init()
    }

    package func persistState(for tabId: UUID) {
        let fallback = persistedState(for: tabId)
        let state = states[tabId]
        persistedStates[tabId.uuidString] = RemoteFileBrowserPersistedState(
            lastVisitedPath: state?.currentPath ?? fallback.lastVisitedPath,
            sort: state?.sort ?? fallback.sort,
            sortDirection: state?.sortDirection ?? fallback.sortDirection,
            showHiddenFiles: state?.showHiddenFiles ?? fallback.showHiddenFiles,
            hasCustomizedHiddenFiles: state?.hasCustomizedHiddenFiles ?? fallback.hasCustomizedHiddenFiles
        )
        persistStates()
    }

    package func persistStates() {
        guard let data = try? JSONEncoder().encode(persistedStates) else { return }
        defaults.set(data, forKey: persistenceKey)
    }
}
