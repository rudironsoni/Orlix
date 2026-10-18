import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package struct TranscriptionSettingsKeys {
    static package let provider = "transcriptionProvider"
    static package let mlxWhisperModelId = "mlxWhisperModelId"
    static package let mlxParakeetModelId = "mlxParakeetModelId"
    static package let language = "transcriptionLanguage"
    static package let terminalVoiceButtonEnabled = "terminalVoiceButtonEnabled"

    static package let legacyWhisperModelID = "whisperModelId"
    static package let legacyParakeetModelID = "parakeetModelId"
}

nonisolated package struct TranscriptionSettingsDefaults {
    static package let provider: TranscriptionProvider = .system
    static package let mlxWhisperModelId = MLXModelCatalog.defaultModelID(for: .whisper)
    static package let mlxParakeetModelId = MLXModelCatalog.defaultModelID(for: .parakeetTDT)
    static package let language = "en"
    static package let autoLanguageCode = "auto"
    static package let terminalVoiceButtonEnabled = true

    static package var settings: VoiceSettings {
        VoiceSettings(
            provider: provider,
            whisperModelID: mlxWhisperModelId,
            parakeetModelID: mlxParakeetModelId,
            languageCode: language,
            terminalVoiceButtonEnabled: terminalVoiceButtonEnabled
        )
    }
}

@MainActor
final package class UserDefaultsVoiceSettingsPersistence: VoiceSettingsPersisting {
    private let defaults: UserDefaults

    package init(defaults: UserDefaults) {
        self.defaults = defaults
    }

    package func loadSettings() -> VoiceSettings {
        let settings = VoiceSettings(
            provider: loadProvider(),
            whisperModelID: loadModelID(
                currentKey: TranscriptionSettingsKeys.mlxWhisperModelId,
                legacyKey: TranscriptionSettingsKeys.legacyWhisperModelID,
                kind: .whisper
            ),
            parakeetModelID: loadModelID(
                currentKey: TranscriptionSettingsKeys.mlxParakeetModelId,
                legacyKey: TranscriptionSettingsKeys.legacyParakeetModelID,
                kind: .parakeetTDT
            ),
            languageCode: loadLanguageCode(),
            terminalVoiceButtonEnabled: loadTerminalVoiceButtonEnabled()
        )
        saveSettings(settings)
        defaults.removeObject(forKey: TranscriptionSettingsKeys.legacyWhisperModelID)
        defaults.removeObject(forKey: TranscriptionSettingsKeys.legacyParakeetModelID)
        return settings
    }

    package func saveSettings(_ settings: VoiceSettings) {
        defaults.set(settings.provider.rawValue, forKey: TranscriptionSettingsKeys.provider)
        defaults.set(settings.whisperModelID, forKey: TranscriptionSettingsKeys.mlxWhisperModelId)
        defaults.set(settings.parakeetModelID, forKey: TranscriptionSettingsKeys.mlxParakeetModelId)
        defaults.set(settings.languageCode, forKey: TranscriptionSettingsKeys.language)
        defaults.set(
            settings.terminalVoiceButtonEnabled,
            forKey: TranscriptionSettingsKeys.terminalVoiceButtonEnabled
        )
    }

    private func loadProvider() -> TranscriptionProvider {
        guard let rawValue = defaults.string(forKey: TranscriptionSettingsKeys.provider) else {
            return TranscriptionSettingsDefaults.provider
        }
        switch rawValue {
        case "whisper":
            return .mlxWhisper
        case "parakeet":
            return .mlxParakeet
        default:
            return TranscriptionProvider(rawValue: rawValue) ?? TranscriptionSettingsDefaults.provider
        }
    }

    private func loadModelID(
        currentKey: String,
        legacyKey: String,
        kind: MLXModelKind
    ) -> String {
        let savedModelID = defaults.string(forKey: currentKey)
            ?? defaults.string(forKey: legacyKey)
            ?? MLXModelCatalog.defaultModelID(for: kind)
        return MLXModelLegacyMigration.resolveModelID(savedModelID, kind: kind).modelID
    }

    private func loadLanguageCode() -> String {
        let savedLanguage = defaults.string(forKey: TranscriptionSettingsKeys.language)?
            .trimmingCharacters(in: .whitespacesAndNewlines)
            .lowercased()
        guard let savedLanguage, !savedLanguage.isEmpty else {
            return TranscriptionSettingsDefaults.language
        }
        return savedLanguage
    }

    private func loadTerminalVoiceButtonEnabled() -> Bool {
        guard defaults.object(forKey: TranscriptionSettingsKeys.terminalVoiceButtonEnabled) != nil else {
            return TranscriptionSettingsDefaults.terminalVoiceButtonEnabled
        }
        return defaults.bool(forKey: TranscriptionSettingsKeys.terminalVoiceButtonEnabled)
    }
}
