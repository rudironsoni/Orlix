import Foundation

nonisolated package struct RemoteSessionLifecycleContext: Codable, Hashable, Sendable {
    package let attachment: RemoteSessionAttachment
    package let observation: RemoteSessionLifecycleObservation

    private enum CodingKeys: String, CodingKey {
        case attachment
        case envelope
        case presenceProbe
        case legacyTmuxMarkerToken
    }

    package init(
        attachment: RemoteSessionAttachment,
        envelope: RemoteSessionLifecycleEnvelope,
        presenceProbe: RemoteSessionPresenceProbe
    ) {
        self.attachment = attachment
        observation = .current(envelope: envelope, presenceProbe: presenceProbe)
    }

    package init(
        attachment: RemoteSessionAttachment,
        legacyTmuxMarkerToken: String
    ) throws {
        self.attachment = attachment
        observation = try RemoteSessionLifecycleObservation(
            legacyTmuxMarkerToken: legacyTmuxMarkerToken
        )
    }

    package init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        attachment = try container.decode(RemoteSessionAttachment.self, forKey: .attachment)
        if let envelope = try container.decodeIfPresent(
            RemoteSessionLifecycleEnvelope.self,
            forKey: .envelope
        ) {
            observation = .current(
                envelope: envelope,
                presenceProbe: try container.decode(
                    RemoteSessionPresenceProbe.self,
                    forKey: .presenceProbe
                )
            )
        } else {
            observation = try RemoteSessionLifecycleObservation(
                legacyTmuxMarkerToken: container.decode(
                    String.self,
                    forKey: .legacyTmuxMarkerToken
                )
            )
        }
    }

    package func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(attachment, forKey: .attachment)
        switch observation {
        case .current(let envelope, let presenceProbe):
            try container.encode(envelope, forKey: .envelope)
            try container.encode(presenceProbe, forKey: .presenceProbe)
        case .legacyTmux(let markerToken):
            try container.encode(markerToken, forKey: .legacyTmuxMarkerToken)
        }
    }
}

nonisolated package struct TerminalShellStartupPlan: Sendable {
    package let command: String?
    package let remoteSessionLifecycle: RemoteSessionLifecycleContext?
    package let mayExecuteUserStartupAction: Bool

    package var mayExecuteStandaloneUserStartupAction: Bool {
        remoteSessionLifecycle == nil && mayExecuteUserStartupAction
    }

    package var allowsPostLaunchWorkingDirectoryRestore: Bool {
        !mayExecuteStandaloneUserStartupAction
    }

    nonisolated static package let plainShell = TerminalShellStartupPlan(
        command: nil,
        remoteSessionLifecycle: nil,
        mayExecuteUserStartupAction: false
    )
}

nonisolated public enum TerminalShellEndReason: Hashable, Sendable {
    public case transportInterrupted
    public case standaloneStartupActionCompleted
    public case remoteSessionDetached(RemoteSessionOwnership)
    public case remoteSessionTerminated(RemoteSessionOwnership)
    public case remoteSessionCreationFailed
    public case remoteSessionAttachFailed
    public case observationAmbiguous

    nonisolated static public func resolve(
        lifecycle: RemoteSessionLifecycleContext?,
        event: RemoteSessionEvent?,
        sessionExists: Bool?
    ) -> Self {
        guard let lifecycle else {
            return .transportInterrupted
        }

        switch event {
        case .detached:
            return .remoteSessionDetached(lifecycle.attachment.ownership)
        case .terminated:
            return .remoteSessionTerminated(lifecycle.attachment.ownership)
        case .creationFailed:
            return .remoteSessionCreationFailed
        case .attachFailed:
            return .remoteSessionAttachFailed
        case .transportInterrupted:
            return .transportInterrupted
        case .observationAmbiguous:
            return .observationAmbiguous
        case .attached, nil:
            switch sessionExists {
            case true:
                return .remoteSessionDetached(lifecycle.attachment.ownership)
            case false:
                return .remoteSessionTerminated(lifecycle.attachment.ownership)
            case nil:
                return .observationAmbiguous
            }
        }
    }
}

nonisolated public enum TerminalDisconnectReason: String, Codable, Hashable, Sendable {
    // Keep raw values stable for local snapshot migration.
    public case transportInterrupted = "transportEnded"
    public case startupActionCompleted
    public case remoteSessionDetached = "tmuxDetached"
    public case externalRemoteSessionTerminated = "externalTmuxEnded"

    public var allowsAutomaticReconnect: Bool {
        self == .transportInterrupted
    }
}

nonisolated public enum TerminalTeardownIntent: CaseIterable, Sendable {
    public case explicitClose
    public case explicitServerDisconnect
    public case remoteSessionEnded
    public case applicationTermination

    public var removesPersistedDescriptor: Bool {
        self != .applicationTermination
    }

    public var terminatesManagedRemoteSession: Bool {
        switch self {
        case .explicitClose, .explicitServerDisconnect:
            true
        case .remoteSessionEnded, .applicationTermination:
            false
        }
    }

    public var deletesResumableSessionState: Bool {
        self != .applicationTermination
    }
}
