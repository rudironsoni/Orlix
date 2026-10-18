@MainActor
package struct AppLifecycleDependencies {
    package let subscribeToRemoteChanges: @MainActor () async -> Void
    package let refreshNetwork: @MainActor () -> Void

    #if os(iOS)
    package let endLiveActivitiesForApplicationTermination: @MainActor () -> Bool
    #endif
}
