import Foundation

nonisolated package enum TranscriptionProvider: String, CaseIterable, Identifiable, Sendable {
    case system
    case mlxWhisper
    case mlxParakeet

    nonisolated package var id: String { rawValue }
}

nonisolated package struct VoiceSettings: Equatable, Sendable {
    package var provider: TranscriptionProvider
    package var whisperModelID: String
    package var parakeetModelID: String
    package var languageCode: String
    package var terminalVoiceButtonEnabled: Bool


    package init(

        provider: TranscriptionProvider,

        whisperModelID: String,

        parakeetModelID: String,

        languageCode: String,

        terminalVoiceButtonEnabled: Bool

    ) {

        self.provider = provider

        self.whisperModelID = whisperModelID

        self.parakeetModelID = parakeetModelID

        self.languageCode = languageCode

        self.terminalVoiceButtonEnabled = terminalVoiceButtonEnabled

    }
}
