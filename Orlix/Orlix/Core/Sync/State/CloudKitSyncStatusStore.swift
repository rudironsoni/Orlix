import Combine
import Foundation

@MainActor
final package class CloudKitSyncStatusStore: ObservableObject {
    @Published package var syncState = CloudKitSyncState()
    @Published package var lastSyncDate: Date?
    @Published package var accountState: CloudKitAccountState = .checking
}
