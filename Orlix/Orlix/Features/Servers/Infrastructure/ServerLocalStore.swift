import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
@MainActor
package struct ServerLocalStore {
    static package let serversStorageKey = "com.rudironsoni.orlix.servers"
    static package let workspacesStorageKey = "com.rudironsoni.orlix.workspaces"
    private static let serverDataMutationJournalKey =
        "com.rudironsoni.orlix.serverDataMutationJournal.v2"
    private static let ambiguousCloudRecoveryBackupKey =
        "com.rudironsoni.orlix.ambiguousCloudRecoveryBackup.v1"

    private let defaults: UserDefaults
    private let serversKey: String
    private let workspacesKey: String

    package init(
        defaults: UserDefaults,
        serversKey: String = Self.serversStorageKey,
        workspacesKey: String = Self.workspacesStorageKey
    ) {
        self.defaults = defaults
        self.serversKey = serversKey
        self.workspacesKey = workspacesKey
    }

    package func loadServers() -> ServerLocalLoadResult<[Server]> {
        if let journal = try? loadServerDataMutationJournal() {
            return .loaded(
                journal.presentsResultingState
                    ? journal.plan.resultingServers
                    : journal.plan.previousServers
            )
        }
        return load([Server].self, forKey: serversKey, collection: .servers)
    }

    package func loadWorkspaces() -> ServerLocalLoadResult<[Workspace]> {
        if let journal = try? loadServerDataMutationJournal() {
            return .loaded(
                journal.presentsResultingState
                    ? journal.plan.resultingWorkspaces
                    : journal.plan.previousWorkspaces
            )
        }
        return load([Workspace].self, forKey: workspacesKey, collection: .workspaces)
    }

    package func storeServers(_ servers: [Server]) throws {
        try requireNoPendingServerDataMutation()
        try store(servers, forKey: serversKey)
    }

    package func storeWorkspaces(_ workspaces: [Workspace]) throws {
        try requireNoPendingServerDataMutation()
        try store(workspaces, forKey: workspacesKey)
    }

    private func load<Value: Decodable>(
        _ type: Value.Type,
        forKey key: String,
        collection: ServerLocalStorageIssue.Collection
    ) -> ServerLocalLoadResult<Value> {
        guard let data = defaults.data(forKey: key) else {
            return .missing
        }

        do {
            return .loaded(try JSONDecoder().decode(type, from: data))
        } catch {
            let quarantineKey = quarantineKey(for: key)
            if defaults.data(forKey: quarantineKey) == nil {
                defaults.set(data, forKey: quarantineKey)
            }
            return .unreadable(
                ServerLocalStorageIssue(
                    collection: collection,
                    quarantineKey: quarantineKey
                )
            )
        }
    }

    private func store<Value: Encodable>(_ value: Value, forKey key: String) throws {
        defaults.set(try JSONEncoder().encode(value), forKey: key)
    }

    private func quarantineKey(for storageKey: String) -> String {
        "\(storageKey).unreadable-backup.v1"
    }
}

extension ServerLocalStore: ServerDataMutationJournalStoring {
    package func loadServerDataMutationJournal() throws -> ServerDataMutationJournal? {
        guard let data = defaults.data(forKey: Self.serverDataMutationJournalKey) else {
            return nil
        }
        return try JSONDecoder().decode(ServerDataMutationJournal.self, from: data)
    }

    package func storeServerDataMutationJournal(
        _ journal: ServerDataMutationJournal
    ) throws {
        let data = try JSONEncoder().encode(journal)
        defaults.set(data, forKey: Self.serverDataMutationJournalKey)
        guard defaults.data(forKey: Self.serverDataMutationJournalKey) == data,
              try loadServerDataMutationJournal() == journal else {
            throw ServerLocalStoreError.persistenceFailed
        }
    }

    package func materializeServerDataMutation(_ plan: ServerDataMutationPlan) throws {
        try persistCollections(
            servers: plan.resultingServers,
            workspaces: plan.resultingWorkspaces
        )
    }

    package func clearServerDataMutationJournal() throws {
        defaults.removeObject(forKey: Self.serverDataMutationJournalKey)
        guard defaults.object(forKey: Self.serverDataMutationJournalKey) == nil else {
            throw ServerLocalStoreError.persistenceFailed
        }
    }
}

extension ServerLocalStore: ServerLocalRepository {
    package func loadSnapshot() -> ServerLocalRepositorySnapshot {
        ServerLocalRepositorySnapshot(
            servers: loadServers(),
            workspaces: loadWorkspaces()
        )
    }

    package func persist(servers: [Server], workspaces: [Workspace]) throws {
        try requireNoPendingServerDataMutation()
        try persistCollections(servers: servers, workspaces: workspaces)
    }

    package func clearServerData() throws {
        try requireNoPendingServerDataMutation()
        defaults.removeObject(forKey: serversKey)
        defaults.removeObject(forKey: workspacesKey)
    }

    package func loadAmbiguousCloudRecoveryBackup() throws -> AmbiguousCloudRecoveryBackup? {
        guard let data = defaults.data(forKey: Self.ambiguousCloudRecoveryBackupKey) else {
            return nil
        }
        return try JSONDecoder().decode(AmbiguousCloudRecoveryBackup.self, from: data)
    }

    package func storeAmbiguousCloudRecoveryBackup(_ backup: AmbiguousCloudRecoveryBackup) throws {
        if try loadAmbiguousCloudRecoveryBackup() != nil {
            return
        }
        let data = try JSONEncoder().encode(backup)
        defaults.set(data, forKey: Self.ambiguousCloudRecoveryBackupKey)
        guard defaults.data(forKey: Self.ambiguousCloudRecoveryBackupKey) == data,
              try loadAmbiguousCloudRecoveryBackup() == backup else {
            throw ServerLocalStoreError.persistenceFailed
        }
    }

    package func clearAmbiguousCloudRecoveryBackup() throws {
        defaults.removeObject(forKey: Self.ambiguousCloudRecoveryBackupKey)
        guard defaults.object(forKey: Self.ambiguousCloudRecoveryBackupKey) == nil else {
            throw ServerLocalStoreError.persistenceFailed
        }
    }

    private func requireNoPendingServerDataMutation() throws {
        guard try loadServerDataMutationJournal() == nil else {
            throw ServerLocalStoreError.serverDataMutationPending
        }
    }

    private func persistCollections(servers: [Server], workspaces: [Workspace]) throws {
        let encoder = JSONEncoder()
        let serverData = try encoder.encode(servers)
        let workspaceData = try encoder.encode(workspaces)
        let previousServerData = defaults.data(forKey: serversKey)
        let previousWorkspaceData = defaults.data(forKey: workspacesKey)

        defaults.set(serverData, forKey: serversKey)
        defaults.set(workspaceData, forKey: workspacesKey)
        guard defaults.data(forKey: serversKey) == serverData,
              defaults.data(forKey: workspacesKey) == workspaceData else {
            restore(previousServerData, forKey: serversKey)
            restore(previousWorkspaceData, forKey: workspacesKey)
            throw ServerLocalStoreError.persistenceFailed
        }
    }

    private func restore(_ data: Data?, forKey key: String) {
        if let data {
            defaults.set(data, forKey: key)
        } else {
            defaults.removeObject(forKey: key)
        }
    }
}

nonisolated package enum ServerLocalStoreError: Error, Equatable, Sendable {
    case persistenceFailed
    case serverDataMutationPending
}
