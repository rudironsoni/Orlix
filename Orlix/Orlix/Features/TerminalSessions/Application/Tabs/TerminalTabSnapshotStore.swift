import Foundation

@MainActor
package protocol TerminalTabSnapshotStoring: AnyObject {
    func loadSnapshotData() -> Data?
    func saveSnapshotData(_ data: Data)
    func removeSnapshotData()
}
