import Foundation

@MainActor
final package class VoiceSettingsModelManagerOwner {
    package typealias ManagerFactory = @MainActor (
        _ kind: MLXModelKind,
        _ selectedModelID: @escaping @MainActor () -> String
    ) -> MLXModelManager

    package let settingsStore: VoiceSettingsStore
    package let whisperManager: MLXModelManager
    package let parakeetManager: MLXModelManager

    package init(
        settingsStore: VoiceSettingsStore,
        makeManager: ManagerFactory
    ) {
        self.settingsStore = settingsStore
        whisperManager = makeManager(.whisper) {
            settingsStore.settings.whisperModelID
        }
        parakeetManager = makeManager(.parakeetTDT) {
            settingsStore.settings.parakeetModelID
        }
    }

    isolated deinit {
        whisperManager.shutdown()
        parakeetManager.shutdown()
    }

    package func selectWhisperModel(_ modelID: String) {
        let previousModelID = settingsStore.settings.whisperModelID
        settingsStore.setWhisperModelID(modelID)
        guard settingsStore.settings.whisperModelID != previousModelID else { return }
        whisperManager.modelSelectionDidChange(from: previousModelID)
    }

    package func selectParakeetModel(_ modelID: String) {
        let previousModelID = settingsStore.settings.parakeetModelID
        settingsStore.setParakeetModelID(modelID)
        guard settingsStore.settings.parakeetModelID != previousModelID else { return }
        parakeetManager.modelSelectionDidChange(from: previousModelID)
    }

    package func refreshStatus() {
        whisperManager.refreshStatus()
        parakeetManager.refreshStatus()
    }

    package func clearAllStorage() {
        whisperManager.clearAllStorage()
        parakeetManager.clearAllStorage()
        refreshStatus()
    }
}
