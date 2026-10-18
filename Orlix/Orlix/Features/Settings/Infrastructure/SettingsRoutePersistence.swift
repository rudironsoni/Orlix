import Foundation

nonisolated package enum SettingsRoutePersistence {
    static package let selectedRouteKey = "settings.selectedRoute"

    static package func load(from defaults: UserDefaults) -> SettingsRoute {
        guard let rawValue = defaults.string(forKey: selectedRouteKey),
              let route = SettingsRoute(rawValue: rawValue) else {
            return .defaultRoute
        }
        return route
    }

    static package func save(_ route: SettingsRoute, to defaults: UserDefaults) {
        defaults.set(route.rawValue, forKey: selectedRouteKey)
    }

    static package func route(for rawValue: String) -> SettingsRoute {
        SettingsRoute(rawValue: rawValue) ?? .defaultRoute
    }
}
