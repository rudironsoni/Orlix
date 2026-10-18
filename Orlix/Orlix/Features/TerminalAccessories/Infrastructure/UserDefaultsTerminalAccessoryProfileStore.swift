import Foundation
import os.log

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
@MainActor
final package class UserDefaultsTerminalAccessoryProfileStore: TerminalAccessoryProfilePersisting {
    static package let storageKey = "terminalAccessoryProfileV1"

    private let defaults: UserDefaults
    private let key: String
    private let logger = Logger(
        subsystem: "com.rudironsoni.orlix",
        category: "TerminalAccessoryProfileStore"
    )

    package init(defaults: UserDefaults, key: String) {
        self.defaults = defaults
        self.key = key
    }

    package func loadProfile(defaultWriterID writerID: String) -> TerminalAccessoryProfile {
        guard let data = defaults.data(forKey: key) else {
            return repairWithDefaultProfile(writerID: writerID)
        }

        do {
            var decoded = try JSONDecoder().decode(TerminalAccessoryProfile.self, from: data)
            if decoded.lastWriterDeviceId.isEmpty {
                decoded.lastWriterDeviceId = writerID
            }
            let normalized = decoded.normalized()
            if normalized != decoded {
                saveProfile(normalized)
            }
            return normalized
        } catch {
            logger.error("Failed to decode terminal accessory profile: \(error.localizedDescription)")
            return repairWithDefaultProfile(writerID: writerID)
        }
    }

    package func saveProfile(_ profile: TerminalAccessoryProfile) {
        do {
            defaults.set(try JSONEncoder().encode(profile), forKey: key)
        } catch {
            logger.error("Failed to encode terminal accessory profile: \(error.localizedDescription)")
        }
    }

    private func repairWithDefaultProfile(writerID: String) -> TerminalAccessoryProfile {
        let profile = TerminalAccessoryProfile
            .defaultValue(lastWriterDeviceId: writerID)
            .normalized()
        saveProfile(profile)
        return profile
    }
}
