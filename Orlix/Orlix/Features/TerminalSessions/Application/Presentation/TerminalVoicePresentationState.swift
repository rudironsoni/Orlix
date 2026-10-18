nonisolated public enum TerminalVoicePresentationState: Equatable, Sendable {
    nonisolated package enum Event: Equatable, Sendable {
        case recordingStarted
        case recordingStopped
        case transcriptionSent
        case pendingReturnDismissed
    }

    public case idle
    public case recording
    public case pendingReturn

    public var isRecording: Bool { self == .recording }
    public var isPendingReturn: Bool { self == .pendingReturn }

    public func applying(_ event: Event) -> Self {
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
