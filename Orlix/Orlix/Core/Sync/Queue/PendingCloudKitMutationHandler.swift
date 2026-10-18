@MainActor
package protocol PendingCloudKitMutationHandling: AnyObject {
    package func handle(_ mutation: PendingCloudKitMutation) async throws
}
