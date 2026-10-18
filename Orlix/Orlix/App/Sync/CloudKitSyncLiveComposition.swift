import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
import OrlixSettings
import OrlixStats
import OrlixTerminal
#endif
@MainActor
package struct CloudKitSyncClients {
    package let serverCloud: any ServerRemoteMutationClient
    package let terminalThemeCloud: any TerminalThemeCloudMutationClient
    package let terminalFontCloud: any TerminalFontCloudMutationClient
    package let terminalAccessoryCloud: any TerminalAccessoryCloudClient
    package let statsPreferencesCloud: any StatsPreferencesCloudClient
}

@MainActor
package struct CloudKitSyncComposition {
    package let coordinator: CloudKitSyncCoordinator
    package let terminalAccessoryResolutions: TerminalAccessoryResolutionChannel
    package let statsPreferencesResolutions: StatsPreferencesResolutionChannel
}

@MainActor
package struct CloudKitLiveSyncComposition {
    package let coordinator: CloudKitSyncCoordinator
    package let terminalAccessoryResolutions: TerminalAccessoryResolutionChannel
    package let statsPreferencesResolutions: StatsPreferencesResolutionChannel
    package let serverCloud: ServerCloudKitClient
    package let terminalThemeCloud: TerminalThemeCloudKitClient
    package let terminalFontCloud: TerminalFontCloudKitClient
    package let terminalAccessoryCloud: TerminalAccessoryCloudKitClient
    package let statsPreferencesCloud: StatsPreferencesCloudKitClient
}

@MainActor
package enum CloudKitSyncLiveComposition {
    static package func make(
        clients: CloudKitSyncClients,
        queue: PendingCloudKitSyncQueue,
        isSyncEnabled: @escaping () -> Bool,
        currentGeneration: @escaping () -> UUID,
        now: @escaping () -> Date,
        makeID: @escaping () -> UUID
    ) -> CloudKitSyncComposition {
        let terminalAccessoryResolutions = TerminalAccessoryResolutionChannel()
        let statsPreferencesResolutions = StatsPreferencesResolutionChannel()
        let mutationHandler = CloudKitPendingMutationRouter(
            serverCloud: clients.serverCloud,
            terminalThemeCloud: clients.terminalThemeCloud,
            terminalFontCloud: clients.terminalFontCloud,
            terminalAccessoryHandler: TerminalAccessoryPendingMutationHandler(
                cloud: clients.terminalAccessoryCloud,
                resolutionPublisher: terminalAccessoryResolutions
            ),
            statsPreferencesHandler: StatsPreferencesPendingMutationHandler(
                cloud: clients.statsPreferencesCloud,
                resolutionPublisher: statsPreferencesResolutions
            )
        )
        return CloudKitSyncComposition(
            coordinator: CloudKitSyncCoordinator(
                mutationHandler: mutationHandler,
                queue: queue,
                isSyncEnabled: isSyncEnabled,
                currentGeneration: currentGeneration,
                now: now,
                makeID: makeID
            ),
            terminalAccessoryResolutions: terminalAccessoryResolutions,
            statsPreferencesResolutions: statsPreferencesResolutions
        )
    }

    static package func makeLive(
        transport: any CloudKitRecordChangeTransport,
        terminalFontRepository: any TerminalFontAssetRepository,
        defaults: UserDefaults,
        now: @escaping () -> Date,
        makeID: @escaping () -> UUID
    ) -> CloudKitLiveSyncComposition {
        let serverCloud = ServerCloudKitClient(transport: transport, now: now)
        let terminalThemeCloud = TerminalThemeCloudKitClient(transport: transport)
        let terminalFontCloud = TerminalFontCloudKitClient(
            transport: transport,
            repository: terminalFontRepository
        )
        let terminalAccessoryCloud = TerminalAccessoryCloudKitClient(transport: transport)
        let statsPreferencesCloud = StatsPreferencesCloudKitClient(transport: transport)
        let composition = make(
            clients: CloudKitSyncClients(
                serverCloud: serverCloud,
                terminalThemeCloud: terminalThemeCloud,
                terminalFontCloud: terminalFontCloud,
                terminalAccessoryCloud: terminalAccessoryCloud,
                statsPreferencesCloud: statsPreferencesCloud
            ),
            queue: PendingCloudKitSyncQueue(
                defaults: defaults,
                legacyMigrator: CloudKitPendingMutationLegacyMigrator()
            ),
            isSyncEnabled: { SyncSettings.isEnabled(in: defaults) },
            currentGeneration: { transport.cloudKitSyncGeneration },
            now: now,
            makeID: makeID
        )
        return CloudKitLiveSyncComposition(
            coordinator: composition.coordinator,
            terminalAccessoryResolutions: composition.terminalAccessoryResolutions,
            statsPreferencesResolutions: composition.statsPreferencesResolutions,
            serverCloud: serverCloud,
            terminalThemeCloud: terminalThemeCloud,
            terminalFontCloud: terminalFontCloud,
            terminalAccessoryCloud: terminalAccessoryCloud,
            statsPreferencesCloud: statsPreferencesCloud
        )
    }
}
