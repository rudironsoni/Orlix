import Foundation

package typealias VoiceInputRuntimeFactory = @MainActor () -> VoiceInputRuntimeOwner

@MainActor
final package class VoiceInputRuntimeOwner {
    package let audioService: AudioService
    package let recordingOperation: VoiceRecordingOperationCoordinator

    package init(
        audioService: AudioService,
        recordingOperation: VoiceRecordingOperationCoordinator
    ) {
        self.audioService = audioService
        self.recordingOperation = recordingOperation
    }

    package func cancel() {
        recordingOperation.cancel()
        audioService.cancelRecording()
    }

    isolated deinit {
        cancel()
    }
}

@MainActor
final package class VoiceInputRuntimeStore {
    package let settingsStore: VoiceSettingsStore
    private let makeRuntime: VoiceInputRuntimeFactory
    private var runtimesByTabID: [UUID: VoiceInputRuntimeOwner] = [:]
    private var tabIDsByServerID: [UUID: Set<UUID>] = [:]

    package init(
        settingsStore: VoiceSettingsStore,
        makeRuntime: @escaping VoiceInputRuntimeFactory
    ) {
        self.settingsStore = settingsStore
        self.makeRuntime = makeRuntime
    }

    package func runtime(for tabID: UUID) -> VoiceInputRuntimeOwner {
        if let runtime = runtimesByTabID[tabID] {
            return runtime
        }
        let runtime = makeRuntime()
        runtimesByTabID[tabID] = runtime
        return runtime
    }

    package func synchronize(tabIDs: Set<UUID>, for serverID: UUID) {
        let removedTabIDs = (tabIDsByServerID[serverID] ?? []).subtracting(tabIDs)
        for tabID in removedTabIDs {
            runtimesByTabID.removeValue(forKey: tabID)?.cancel()
        }
        if tabIDs.isEmpty {
            tabIDsByServerID.removeValue(forKey: serverID)
        } else {
            tabIDsByServerID[serverID] = tabIDs
        }
    }

    package func removeAll() {
        let runtimes = Array(runtimesByTabID.values)
        runtimesByTabID.removeAll(keepingCapacity: false)
        tabIDsByServerID.removeAll(keepingCapacity: false)
        runtimes.forEach { $0.cancel() }
    }

    package var runtimeCount: Int {
        runtimesByTabID.count
    }

    isolated deinit {
        runtimesByTabID.values.forEach { $0.cancel() }
    }
}

@MainActor
package enum VoiceInputRuntimeLiveComposition {
    static package func makeFactory(
        settingsStore: VoiceSettingsStore
    ) -> VoiceInputRuntimeFactory {
        {
            let speechRecognition = SpeechRecognitionService(
                selectedLanguageCode: {
                    settingsStore.currentSettings.languageCode
                }
            )
            let audioService = AudioService(
                permissionManager: AudioPermissionManager(),
                speechRecognitionService: speechRecognition,
                audioCaptureService: AudioCaptureService(),
                mlxWhisperProvider: MLXWhisperProvider(),
                mlxParakeetProvider: MLXParakeetProvider(),
                settingsReader: settingsStore,
                startupOperation: nil
            )
            return VoiceInputRuntimeOwner(
                audioService: audioService,
                recordingOperation: VoiceRecordingOperationCoordinator()
            )
        }
    }
}
