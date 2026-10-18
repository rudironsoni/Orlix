import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
@MainActor
final package class TerminalAccessoryResolutionChannel:
    TerminalAccessoryResolutionSource,
    TerminalAccessoryResolutionPublishing {
    package typealias Observer = (TerminalAccessoryProfile) -> Void

    private var observers: [UUID: Observer] = [:]

    @discardableResult
    package func observeTerminalAccessoryProfile(
        _ observer: @escaping Observer
    ) -> UUID {
        let id = UUID()
        observers[id] = observer
        return id
    }

    package func removeTerminalAccessoryProfileObserver(_ id: UUID) {
        observers.removeValue(forKey: id)
    }

    package func publishTerminalAccessoryProfile(_ profile: TerminalAccessoryProfile) {
        for observer in Array(observers.values) {
            observer(profile)
        }
    }

    package init() {}
}
