import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package extension CloudKitSyncCoordinator: TerminalFontMutationQueue {
    package func enqueueTerminalFontUpsert(_ font: TerminalFont) throws {
        try enqueue(.terminalFontUpsert(font))
    }

    package func enqueueTerminalFontPreferenceUpsert(_ preference: TerminalFontPreference) throws {
        try enqueue(.terminalFontPreferenceUpsert(preference))
    }
}

package extension CloudKitSyncLifecycleDriver: TerminalFontSyncLifecycle {}

extension TerminalFontStoreDependencies {
    static func live(
        repository: any TerminalFontRepository,
        cloud: any TerminalFontCloudClient,
        mutationQueue: any TerminalFontMutationQueue,
        syncLifecycle: any TerminalFontSyncLifecycle,
        isSyncEnabled: @escaping () -> Bool,
        now: @escaping () -> Date
    ) -> Self {
        TerminalFontStoreDependencies(
            repository: repository,
            cloud: cloud,
            mutationQueue: mutationQueue,
            syncLifecycle: syncLifecycle,
            loadCatalog: { customFamilies in
                TerminalFontCatalog.live(appOwnedFamilies: customFamilies)
            },
            isSyncEnabled: isSyncEnabled,
            now: now,
            startsSynchronization: true
        )
    }
}
