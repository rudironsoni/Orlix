nonisolated package enum TerminalVoicePresentationState: Equatable, Sendable {
    nonisolated package enum Event: Equatable, Sendable {
        case recordingStarted
        case recordingStopped
        case transcriptionSent
        case pendingReturnDismissed
    }

    case idle
    case recording
    case pendingReturn

    package var isRecording: Bool { self == .recording }
    package var isPendingReturn: Bool { self == .pendingReturn }

    package func applying(_ event: Event) -> Self {
        switch event {
        case .recordingStarted:
            return .recording
        case .recordingStopped:
            return self == .recording ? .idle : self
        case .transcriptionSent:
            return .pendingReturn
        case .pendingReturnDismissed:
            return self == .pendingReturn ? .idle : self
        }
    }
}
