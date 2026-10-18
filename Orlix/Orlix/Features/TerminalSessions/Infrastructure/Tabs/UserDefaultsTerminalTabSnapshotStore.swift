import Foundation

@MainActor
final package class UserDefaultsTerminalTabSnapshotStore: TerminalTabSnapshotStoring {
    private let defaults: UserDefaults
    private let key: String

    package init(defaults: UserDefaults, key: String) {
        self.defaults = defaults
        self.key = key
    }

    package func loadSnapshotData() -> Data? {
        defaults.data(forKey: key)
    }

    package func saveSnapshotData(_ data: Data) {
        defaults.set(data, forKey: key)
    }

    package func removeSnapshotData() {
        defaults.removeObject(forKey: key)
    }
}
