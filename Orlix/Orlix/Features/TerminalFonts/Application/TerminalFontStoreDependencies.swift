import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
@MainActor
package protocol TerminalFontCloudClient: AnyObject {
    func fetchTerminalFonts() async throws -> [TerminalFont]
    func fetchTerminalFontPreference() async throws -> TerminalFontPreference?
}

@MainActor
package protocol TerminalFontCloudMutationClient: AnyObject {
    func saveTerminalFont(_ font: TerminalFont) async throws
    func saveTerminalFontPreference(_ preference: TerminalFontPreference) async throws
}

@MainActor
package protocol TerminalFontMutationQueue: AnyObject {
    func enqueueTerminalFontUpsert(_ font: TerminalFont) throws
    func enqueueTerminalFontPreferenceUpsert(_ preference: TerminalFontPreference) throws
    func drainPendingMutations() async
}

@MainActor
package protocol TerminalFontSyncLifecycle: AnyObject {
    func observe(_ observer: @escaping (CloudKitSyncLifecycleEvent) -> Void) -> UUID
    func removeObserver(_ id: UUID)
}

@MainActor
package struct TerminalFontStoreDependencies {
    package let repository: any TerminalFontRepository
    package let cloud: any TerminalFontCloudClient
    package let mutationQueue: any TerminalFontMutationQueue
    package let syncLifecycle: any TerminalFontSyncLifecycle
    package let loadCatalog: ([TerminalFontFamily]) -> TerminalFontCatalog
    package let isSyncEnabled: () -> Bool
    package let now: () -> Date
    package let startsSynchronization: Bool
}
