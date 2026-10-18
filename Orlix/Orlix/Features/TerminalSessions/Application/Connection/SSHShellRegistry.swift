import Foundation

#if ORLIX_MODULAR_APP
import OrlixSSH
#endif
nonisolated package struct SSHShellRegistry {
    nonisolated package struct StartToken: Hashable, Sendable {
        package let id: UUID

        package init(id: UUID = UUID()) {
            self.id = id
        }
    }

    nonisolated package struct Registration: Sendable {
        package let serverId: UUID
        package let client: SSHClient
        package let shellId: UUID
        package let startToken: StartToken
    }

    nonisolated package struct StartContext: Sendable {
        package let token: StartToken
        package let startedAt: Date
        package let client: SSHClient
        package let serverId: UUID
    }

    nonisolated package enum RegisterResult: Sendable, Equatable {
        case accepted
        case stale
    }

    nonisolated package struct StartResult: Sendable {
        package let token: StartToken?
        package let staleContext: StartContext?

        package var started: Bool { token != nil }
    }

    nonisolated package struct InFlightResult: Sendable {
        package let inFlight: Bool
        package let staleContext: StartContext?
    }

    nonisolated package struct DrainResult: Sendable {
        package let registrations: [Registration]
        package let pendingStarts: [StartContext]
    }

    private(set) var registrations: [UUID: Registration] = [:]
    private(set) var startsInFlight: [UUID: StartContext] = [:]
    private let staleThreshold: TimeInterval

    package init(staleThreshold: TimeInterval) {
        self.staleThreshold = staleThreshold
    }

    mutating package func register(
        client: SSHClient,
        shellId: UUID,
        startToken: StartToken,
        for entityId: UUID,
        serverId: UUID
    ) -> RegisterResult {
        guard let context = startsInFlight[entityId],
              ObjectIdentifier(context.client) == ObjectIdentifier(client),
              context.token == startToken,
              context.serverId == serverId,
              registrations[entityId] == nil else {
            return .stale
        }

        startsInFlight.removeValue(forKey: entityId)
        let newRegistration = Registration(
            serverId: serverId,
            client: client,
            shellId: shellId,
            startToken: startToken
        )
        registrations[entityId] = newRegistration
        return .accepted
    }

    mutating package func unregister(for entityId: UUID) -> (registration: Registration?, pendingStart: StartContext?) {
        let pendingStart = startsInFlight.removeValue(forKey: entityId)
        let registration = registrations.removeValue(forKey: entityId)
        return (registration, pendingStart)
    }

    mutating package func tryBeginStart(
        for entityId: UUID,
        serverId: UUID,
        client: SSHClient,
        now: Date = Date()
    ) -> StartResult {
        if registrations[entityId] != nil {
            return StartResult(token: nil, staleContext: nil)
        }

        if let context = startsInFlight[entityId] {
            if now.timeIntervalSince(context.startedAt) < staleThreshold {
                return StartResult(token: nil, staleContext: nil)
            }
            startsInFlight.removeValue(forKey: entityId)
            let replacement = StartContext(
                token: StartToken(),
                startedAt: now,
                client: client,
                serverId: serverId
            )
            startsInFlight[entityId] = replacement
            return StartResult(token: replacement.token, staleContext: context)
        }

        let context = StartContext(
            token: StartToken(),
            startedAt: now,
            client: client,
            serverId: serverId
        )
        startsInFlight[entityId] = context
        return StartResult(token: context.token, staleContext: nil)
    }

    mutating package func finishStart(
        for entityId: UUID,
        client: SSHClient,
        startToken: StartToken
    ) {
        guard let context = startsInFlight[entityId] else { return }
        guard ObjectIdentifier(context.client) == ObjectIdentifier(client) else { return }
        guard context.token == startToken else { return }
        startsInFlight.removeValue(forKey: entityId)
    }

    mutating package func isStartInFlight(for entityId: UUID, now: Date = Date()) -> InFlightResult {
        guard let context = startsInFlight[entityId] else {
            return InFlightResult(inFlight: false, staleContext: nil)
        }

        if now.timeIntervalSince(context.startedAt) >= staleThreshold {
            startsInFlight.removeValue(forKey: entityId)
            return InFlightResult(inFlight: false, staleContext: context)
        }

        return InFlightResult(inFlight: true, staleContext: nil)
    }

    package func registration(for entityId: UUID) -> Registration? {
        registrations[entityId]
    }

    package func shellId(for entityId: UUID) -> UUID? {
        registrations[entityId]?.shellId
    }

    package func owns(client: SSHClient, shellId: UUID, for entityId: UUID) -> Bool {
        guard let registration = registrations[entityId] else { return false }
        return ObjectIdentifier(registration.client) == ObjectIdentifier(client)
            && registration.shellId == shellId
    }

    package func ownsConnection(
        client: SSHClient,
        startToken: StartToken,
        for entityId: UUID
    ) -> Bool {
        let identifier = ObjectIdentifier(client)
        if let registration = registrations[entityId] {
            return ObjectIdentifier(registration.client) == identifier
                && registration.startToken == startToken
        }
        if let context = startsInFlight[entityId] {
            return ObjectIdentifier(context.client) == identifier
                && context.token == startToken
        }
        return false
    }

    package func client(for entityId: UUID) -> SSHClient? {
        registrations[entityId]?.client
    }

    package func connectionClient(for entityId: UUID) -> SSHClient? {
        registrations[entityId]?.client ?? startsInFlight[entityId]?.client
    }

    package func connectionStartToken(for entityId: UUID) -> StartToken? {
        registrations[entityId]?.startToken ?? startsInFlight[entityId]?.token
    }

    package func owns(startToken: StartToken, for entityId: UUID) -> Bool {
        connectionStartToken(for: entityId) == startToken
    }

    package func hasOtherClientReferences(using client: SSHClient, excluding entityId: UUID) -> Bool {
        let identifier = ObjectIdentifier(client)
        let hasRegistration = registrations.contains { registration in
            registration.key != entityId && ObjectIdentifier(registration.value.client) == identifier
        }
        let hasPendingStart = startsInFlight.contains { start in
            start.key != entityId && ObjectIdentifier(start.value.client) == identifier
        }
        return hasRegistration || hasPendingStart
    }

    package func hasClientReferences(_ client: SSHClient) -> Bool {
        hasActiveRegistration(using: client) || hasPendingStart(using: client)
    }

    package func hasActiveRegistration(using client: SSHClient) -> Bool {
        let identifier = ObjectIdentifier(client)
        return registrations.values.contains { ObjectIdentifier($0.client) == identifier }
    }

    package func hasPendingStart(using client: SSHClient) -> Bool {
        let identifier = ObjectIdentifier(client)
        return startsInFlight.values.contains { ObjectIdentifier($0.client) == identifier }
    }

    package func firstRegisteredClient(for serverId: UUID) -> SSHClient? {
        registrations.values.first(where: { $0.serverId == serverId })?.client
    }

    package func firstPendingClient(for serverId: UUID) -> SSHClient? {
        startsInFlight.values.first(where: { $0.serverId == serverId })?.client
    }

    mutating package func drain() -> DrainResult {
        let result = DrainResult(
            registrations: Array(registrations.values),
            pendingStarts: Array(startsInFlight.values)
        )
        registrations.removeAll()
        startsInFlight.removeAll()
        return result
    }

    mutating package func removeAll() {
        _ = drain()
    }
}
