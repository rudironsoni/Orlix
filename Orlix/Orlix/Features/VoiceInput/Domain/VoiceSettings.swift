import Foundation

nonisolated public enum TranscriptionProvider: String, CaseIterable, Identifiable, Sendable {
    public case system
    public case mlxWhisper
    public case mlxParakeet

    nonisolated public var id: String { rawValue }
}

nonisolated package struct VoiceSettings: Equatable, Sendable {
    package var provider: TranscriptionProvider
    package var whisperModelID: String
    package var parakeetModelID: String
    package var languageCode: String
    package var terminalVoiceButtonEnabled: Bool
}
