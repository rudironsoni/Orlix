import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
@MainActor
final package class StatsPreferencesResolutionChannel:
    StatsPreferencesResolutionSource,
    StatsPreferencesResolutionPublishing {
    typealias Observer = (StatsPreferences) -> Void

    private var observers: [UUID: Observer] = [:]

    @discardableResult
    package func observeStatsPreferences(
        _ observer: @escaping (StatsPreferences) -> Void
    ) -> UUID {
        let id = UUID()
        observers[id] = observer
        return id
    }

    package func removeStatsPreferencesObserver(_ id: UUID) {
        observers.removeValue(forKey: id)
    }

    package func publishStatsPreferences(_ preferences: StatsPreferences) {
        for observer in Array(observers.values) {
            observer(preferences)
        }
    }
}
