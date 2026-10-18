import Combine
import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
@MainActor
package protocol ServerVolumeVisibilityPreferencesPersisting: AnyObject {
    func loadPreferences() -> ServerVolumeVisibilityPreferences
    func savePreferences(_ preferences: ServerVolumeVisibilityPreferences)
}

@MainActor
final package class ServerVolumeVisibilityStore: ObservableObject {
    @Published private(set) var preferences: ServerVolumeVisibilityPreferences

    private let persistence: any ServerVolumeVisibilityPreferencesPersisting

    package init(persistence: any ServerVolumeVisibilityPreferencesPersisting) {
        self.persistence = persistence
        preferences = persistence.loadPreferences()
        if preferences.requiresSchemaMigration {
            preferences.markSchemaMigrationComplete()
            persistence.savePreferences(preferences)
        }
    }

    package func hiddenVolumeIDs(
        for serverID: UUID,
        volumes: [VolumeInfo]
    ) -> Set<VolumeIdentity> {
        VolumeVisibilityPolicy.hiddenVolumeIDs(
            in: volumes,
            visibilityOverrides: preferences.visibilityOverrides(for: serverID)
        )
    }

    package func setVolume(_ volume: VolumeInfo, isVisible: Bool, for serverID: UUID) {
        updateVisibilityOverrides(for: [volume], serverID: serverID) { _ in
            isVisible
        }
    }

    package func setVolumes(_ volumes: [VolumeInfo], areVisible: Bool, for serverID: UUID) {
        updateVisibilityOverrides(for: volumes, serverID: serverID) { _ in
            areVisible
        }
    }

    private func updateVisibilityOverrides(
        for volumes: [VolumeInfo],
        serverID: UUID,
        isVisible: (VolumeInfo) -> Bool
    ) {
        var overrides = preferences.visibilityOverrides(for: serverID)
        for volume in VolumeVisibilityPolicy.normalized(volumes) {
            let desiredVisibility = isVisible(volume)
            if desiredVisibility == VolumeVisibilityPolicy.isVisibleByDefault(volume) {
                overrides.removeValue(forKey: volume.identity)
            } else {
                overrides[volume.identity] = desiredVisibility
            }
        }

        var next = preferences
        next.setVisibilityOverrides(overrides, for: serverID)
        guard next != preferences else { return }
        preferences = next
        persistence.savePreferences(preferences)
    }
}
