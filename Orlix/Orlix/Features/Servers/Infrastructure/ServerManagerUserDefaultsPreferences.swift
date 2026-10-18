import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
@MainActor
final package class ServerManagerUserDefaultsPreferences: ServerManagerPreferences {
    static package let didBootstrapDefaultWorkspaceKey = "com.rudironsoni.orlix.didBootstrapDefaultWorkspace"
    static package let pendingBootstrapWorkspaceIDKey = "com.rudironsoni.orlix.pendingBootstrapWorkspaceID"

    private let defaults: UserDefaults

    package init(defaults: UserDefaults) {
        self.defaults = defaults
    }

    package var didBootstrapDefaultWorkspace: Bool {
        get { defaults.bool(forKey: Self.didBootstrapDefaultWorkspaceKey) }
        set { defaults.set(newValue, forKey: Self.didBootstrapDefaultWorkspaceKey) }
    }

    package var hasSeenWelcome: Bool {
        defaults.bool(forKey: "hasSeenWelcome")
    }

    package var freePlanGeneration: FreePlanGeneration? {
        get {
            guard let rawValue = defaults.string(forKey: FreeTierLimits.planGenerationStorageKey) else {
                return nil
            }
            return FreePlanGeneration(rawValue: rawValue)
        }
        set {
            if let newValue {
                defaults.set(newValue.rawValue, forKey: FreeTierLimits.planGenerationStorageKey)
            } else {
                defaults.removeObject(forKey: FreeTierLimits.planGenerationStorageKey)
            }
        }
    }

    package var pendingBootstrapWorkspaceID: UUID? {
        get {
            guard let rawValue = defaults.string(
                forKey: Self.pendingBootstrapWorkspaceIDKey
            ) else {
                return nil
            }
            return UUID(uuidString: rawValue)
        }
        set {
            if let newValue {
                defaults.set(
                    newValue.uuidString,
                    forKey: Self.pendingBootstrapWorkspaceIDKey
                )
            } else {
                defaults.removeObject(forKey: Self.pendingBootstrapWorkspaceIDKey)
            }
        }
    }
}
