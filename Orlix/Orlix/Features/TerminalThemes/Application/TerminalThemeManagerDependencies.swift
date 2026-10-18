import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
@MainActor
package protocol TerminalThemeCloudClient: AnyObject {
    func fetchTerminalThemes() async throws -> [TerminalTheme]
    func fetchTerminalThemePreference() async throws -> TerminalThemePreference?
}

@MainActor
package protocol TerminalThemeCloudMutationClient: AnyObject {
    func saveTerminalTheme(_ theme: TerminalTheme) async throws
    func saveTerminalThemePreference(_ preference: TerminalThemePreference) async throws
}

@MainActor
package protocol TerminalThemeMutationQueue: AnyObject {
    func enqueueTerminalThemeUpsert(_ theme: TerminalTheme) throws
    func enqueueTerminalThemePreferenceUpsert(_ preference: TerminalThemePreference) throws
    func drainPendingMutations() async
}

@MainActor
package protocol TerminalThemeSyncLifecycle: AnyObject {
    func observe(
        _ observer: @escaping (CloudKitSyncLifecycleEvent) -> Void
    ) -> UUID
    func removeObserver(_ id: UUID)
}

@MainActor
package protocol TerminalThemePreferenceChangeSource: AnyObject {
    func observeChanges(
        _ observer: @escaping @MainActor @Sendable () -> Void
    ) -> NSObjectProtocol
    func removeObserver(_ observer: NSObjectProtocol)
}

@MainActor
package protocol TerminalThemePersistence: AnyObject {
    func loadCustomThemes() throws -> [TerminalTheme]
    func saveCustomThemes(_ themes: [TerminalTheme]) throws
    func loadSelection() -> TerminalThemeSelection
    func saveSelection(_ selection: TerminalThemeSelection)
    func loadPreferenceUpdatedAt() -> Date
    func savePreferenceUpdatedAt(_ date: Date)
    func cacheActiveBackgroundHex(_ hex: String)
}

@MainActor
package protocol TerminalThemeFileSynchronizing {
    func synchronize(_ themes: [TerminalTheme]) throws
}

@MainActor
package protocol BuiltInTerminalThemeCatalog {
    func themeNames() -> [String]
}

@MainActor
package protocol TerminalThemePaletteResolving {
    func palette(forThemeNamed name: String) -> TerminalThemePalette
    func palette(forThemeContent content: String) -> TerminalThemePalette
    func invalidateCache()
}

@MainActor
package struct TerminalThemeManagerDependencies {
    package let persistence: any TerminalThemePersistence
    package let cloud: any TerminalThemeCloudClient
    package let mutationQueue: any TerminalThemeMutationQueue
    package let syncLifecycle: any TerminalThemeSyncLifecycle
    package let preferenceChanges: any TerminalThemePreferenceChangeSource
    package let themeFiles: any TerminalThemeFileSynchronizing
    package let builtInThemeCatalog: any BuiltInTerminalThemeCatalog
    package let paletteResolver: any TerminalThemePaletteResolving
    package let isSyncEnabled: () -> Bool
    package let now: () -> Date
    package let waitForPreferenceSyncDebounce: () async throws -> Void
    package let startsSynchronization: Bool
}
