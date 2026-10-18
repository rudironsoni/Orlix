import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package struct ServerStatsConnectionIdentity: Hashable, Sendable {
    private let value: ObjectIdentifier

    package init(_ value: AnyObject) {
        self.value = ObjectIdentifier(value)
    }
}

nonisolated package enum ServerStatsClientOwnership: Equatable, Sendable {
    case owned
    case shared

    package var disconnectWhenDone: Bool { self == .owned }
}

nonisolated package struct ServerStatsApprovalRequest: Identifiable, Equatable, Sendable {
    package let id: String
    package let serverID: UUID
}

@MainActor
package protocol ServerStatsCollectionTarget {
    var serverID: UUID { get }
}

@MainActor
package protocol ServerStatsConnectionReference {
    var identity: ServerStatsConnectionIdentity { get }
}

@MainActor
package protocol ServerStatsApprovalReference: AnyObject, Sendable {
    var request: ServerStatsApprovalRequest { get }
}

package struct ServerStatsApprovalRequired: Error {
    package let reference: any ServerStatsApprovalReference
}

nonisolated package struct ServerStatsCollectionPreparation: Sendable {
    package let platformName: String
    package let profile: HardwareProfile?
}

@MainActor
package protocol ServerStatsCollectionSession: AnyObject {
    var connectionIdentity: ServerStatsConnectionIdentity { get }
    var ownership: ServerStatsClientOwnership { get }

    func runCollection(
        _ operation: @MainActor @Sendable @escaping () async throws -> Void
    ) async throws
    func disconnect() async
    func prepareIfNeeded() async -> ServerStatsCollectionPreparation?
    func collectStats(collectDocker: Bool) async throws -> ServerStats
    func terminateProcess(_ process: ProcessInfo) async throws
    func loadProcesses(fallback: [ProcessInfo]) async throws -> [ProcessInfo]
    func loadDockerStats(fallback: DockerStats) async -> DockerStats
    func loadStorageHealth(for volume: VolumeInfo) async throws -> StorageHealthResult
    func performDockerAction(
        _ action: DockerContainerAction,
        on container: DockerContainer,
        fallback: DockerStats
    ) async throws -> DockerStats
}

@MainActor
package struct ServerStatsCollectorDependencies {
    package typealias WaitForNextPoll = @MainActor @Sendable () async throws -> Void

    package let makeOwnedConnection: () -> any ServerStatsConnectionReference
    package let makeSession: (
        _ target: any ServerStatsCollectionTarget,
        _ connection: any ServerStatsConnectionReference,
        _ ownership: ServerStatsClientOwnership
    ) throws -> any ServerStatsCollectionSession
    package let makeAttemptID: () -> UUID
    package let waitForNextPoll: WaitForNextPoll

    package init(
        makeOwnedConnection: @escaping () -> any ServerStatsConnectionReference,
        makeSession: @escaping (
            _ target: any ServerStatsCollectionTarget,
            _ connection: any ServerStatsConnectionReference,
            _ ownership: ServerStatsClientOwnership
        ) throws -> any ServerStatsCollectionSession,
        makeAttemptID: @escaping () -> UUID,
        waitForNextPoll: @escaping WaitForNextPoll = {
            try await Task.sleep(for: .seconds(2))
        }
    ) {
        self.makeOwnedConnection = makeOwnedConnection
        self.makeSession = makeSession
        self.makeAttemptID = makeAttemptID
        self.waitForNextPoll = waitForNextPoll
    }
}
