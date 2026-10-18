import Foundation
import os

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package enum SSHStartupStage: String, Sendable {
    case transportPreparation
    case dnsResolution
    case tcpAddressAttempt
    case sshHandshake
    case hostKeyVerification
    case authentication
    case remoteEnvironment
    case terminalType
    case shellChannel
    case ptyRequest
    case shellRequest
    case firstTerminalByte
    case moshBootstrap
    case moshEndpoint
    case moshUDPSession
    case sshFallback
    case connectionDeadline
}

nonisolated final package class SSHStartupTrace: @unchecked Sendable {
    package struct Event: Equatable, Sendable {
        package let stage: SSHStartupStage
        package let stageMilliseconds: Int
        package let totalMilliseconds: Int
        package let outcome: String
        package let detail: String
    }

    package struct Token: Sendable {
        package let stage: SSHStartupStage
        package let startedAt: ContinuousClock.Instant
    }

    private let logger: Logger
    private let eventHandler: (@Sendable (Event) -> Void)?
    private let startedAt = ContinuousClock.now
    private let completedStagesLock = NSLock()
    private var completedStages = Set<SSHStartupStage>()
    private let eventsLock = NSLock()
    private var events = [Event]()

    package init(
        logger: Logger,
        eventHandler: (@Sendable (Event) -> Void)? = nil
    ) {
        self.logger = logger
        self.eventHandler = eventHandler
    }

    package func begin(_ stage: SSHStartupStage) -> Token {
        Token(stage: stage, startedAt: .now)
    }

    package func end(
        _ token: Token,
        outcome: String = "ok",
        detail: String = "none"
    ) {
        record(
            token.stage,
            stageMilliseconds: Self.milliseconds(token.startedAt.duration(to: .now)),
            outcome: outcome,
            detail: detail
        )
    }

    package func recordOnce(
        _ stage: SSHStartupStage,
        outcome: String = "ok",
        detail: String = "none"
    ) {
        let inserted = completedStagesLock.withLock {
            completedStages.insert(stage).inserted
        }
        guard inserted else { return }
        record(stage, stageMilliseconds: 0, outcome: outcome, detail: detail)
    }

    package func record(
        _ stage: SSHStartupStage,
        stageMilliseconds: Int,
        outcome: String,
        detail: String
    ) {
        let totalMilliseconds = Self.milliseconds(startedAt.duration(to: .now))
        let event = Event(
            stage: stage,
            stageMilliseconds: stageMilliseconds,
            totalMilliseconds: totalMilliseconds,
            outcome: outcome,
            detail: detail
        )
        eventsLock.withLock { events.append(event) }
        logger.info(
            "startup stage=\(stage.rawValue, privacy: .public) stageMs=\(stageMilliseconds) totalMs=\(totalMilliseconds) outcome=\(outcome, privacy: .public) detail=\(detail, privacy: .public)"
        )
        eventHandler?(event)
    }

    package func snapshot() -> [Event] {
        eventsLock.withLock { events }
    }

    private static func milliseconds(_ duration: Duration) -> Int {
        let components = duration.components
        let value = Double(components.seconds) * 1_000
            + Double(components.attoseconds) / 1_000_000_000_000_000
        guard value.isFinite, value > 0 else { return 0 }
        let rounded = value.rounded()
        guard rounded < Double(Int.max) else { return Int.max }
        return Int(rounded)
    }
}
