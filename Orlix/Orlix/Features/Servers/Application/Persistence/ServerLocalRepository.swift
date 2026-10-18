import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package struct ServerLocalStorageIssue: Identifiable, Equatable, Sendable {
    nonisolated package enum Collection: String, Equatable, Sendable {
        case servers
        case workspaces
    }

    package let collection: Collection
    package let quarantineKey: String

    package var id: Collection { collection }
}

nonisolated package enum ServerLocalLoadResult<Value> {
    case missing
    case loaded(Value)
    case unreadable(ServerLocalStorageIssue)
}

nonisolated package struct ServerLocalRepositorySnapshot {
    package let servers: ServerLocalLoadResult<[Server]>
    package let workspaces: ServerLocalLoadResult<[Workspace]>
}

@MainActor
package protocol ServerLocalRepository: ServerDataMutationJournalStoring {
    func loadSnapshot() -> ServerLocalRepositorySnapshot
    func persist(servers: [Server], workspaces: [Workspace]) throws
    func clearServerData() throws
    func loadAmbiguousCloudRecoveryBackup() throws -> AmbiguousCloudRecoveryBackup?
    func storeAmbiguousCloudRecoveryBackup(_ backup: AmbiguousCloudRecoveryBackup) throws
    func clearAmbiguousCloudRecoveryBackup() throws
}
