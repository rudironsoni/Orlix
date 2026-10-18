import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package enum MoshSSHFallbackDecision: Equatable, Sendable {
    case allow
    case rejectToPreventStartupCommandReplay
}

nonisolated package enum MoshStartupDispatchStage: Equatable, Sendable {
    case beforeUDPClient
    case udpClientStartingOrStarted
}

nonisolated package struct MoshStartupFailure: LocalizedError, Sendable {
    package let stage: MoshStartupDispatchStage
    package let underlying: SSHError

    package var errorDescription: String? {
        underlying.errorDescription
    }
}

nonisolated package enum MoshSSHFallbackPolicy {
    static package func decision(
        after error: Error,
        startupCommand: String?,
        mayExecuteUserStartupAction: Bool
    ) -> MoshSSHFallbackDecision {
        let command = startupCommand?.trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
        guard !command.isEmpty else { return .allow }
        guard mayExecuteUserStartupAction else { return .allow }
        guard let failure = error as? MoshStartupFailure else {
            return .rejectToPreventStartupCommandReplay
        }
        return failure.stage == .beforeUDPClient
            ? .allow
            : .rejectToPreventStartupCommandReplay
    }

    static package func fallbackFailure(
        moshError _: Error,
        fallbackError: Error
    ) -> SSHError {
        if let sshError = fallbackError as? SSHError {
            return sshError
        }
        return .moshSessionFailed(
            "Mosh and SSH startup both failed"
        )
    }
}
