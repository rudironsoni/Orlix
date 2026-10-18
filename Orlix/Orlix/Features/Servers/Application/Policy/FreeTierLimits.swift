import Foundation

nonisolated package enum FreeTierLimits {
    static package let maxWorkspaces = 1
    static package let currentMaxServers = 1
    static package let legacyMaxServers = 3
    static package let maxTabs = 1
    static package let maxCustomActions = 3
    static package let planGenerationStorageKey = "freePlanGeneration"
    static package let currentOneServerPlanCutoff: Date = {
        var components = DateComponents()
        components.calendar = Calendar(identifier: .gregorian)
        components.timeZone = TimeZone(secondsFromGMT: 0)
        components.year = 2026
        components.month = 6
        components.day = 25
        return components.date ?? Date(timeIntervalSince1970: 1_782_345_600)
    }()
}
