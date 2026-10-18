import Combine

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
@MainActor
package protocol VoiceSettingsReading: AnyObject {
    var currentSettings: VoiceSettings { get }
}

@MainActor
package protocol VoiceSettingsPersisting: AnyObject {
    func loadSettings() -> VoiceSettings
    func saveSettings(_ settings: VoiceSettings)
}

@MainActor
final package class VoiceSettingsStore: ObservableObject, VoiceSettingsReading {
    @Published private(set) var settings: VoiceSettings

    private let persistence: any VoiceSettingsPersisting

    package init(persistence: any VoiceSettingsPersisting) {
        self.persistence = persistence
        settings = persistence.loadSettings()
    }

    package var currentSettings: VoiceSettings { settings }

    package func setProvider(_ provider: TranscriptionProvider) {
        update { $0.provider = provider }
    }

    package func setWhisperModelID(_ modelID: String) {
        update { $0.whisperModelID = modelID }
    }

    package func setParakeetModelID(_ modelID: String) {
        update { $0.parakeetModelID = modelID }
    }

    package func setLanguageCode(_ languageCode: String) {
        update { $0.languageCode = languageCode }
    }

    package func setTerminalVoiceButtonEnabled(_ isEnabled: Bool) {
        update { $0.terminalVoiceButtonEnabled = isEnabled }
    }

    package func useSystemProviderWhenMLXIsUnavailable() {
        guard settings.provider != .system else { return }
        setProvider(.system)
    }

    private func update(_ mutate: (inout VoiceSettings) -> Void) {
        var updated = settings
        mutate(&updated)
        guard updated != settings else { return }
        settings = updated
        persistence.saveSettings(updated)
    }
}
