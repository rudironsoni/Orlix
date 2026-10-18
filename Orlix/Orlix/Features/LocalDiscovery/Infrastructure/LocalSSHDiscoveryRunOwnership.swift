import Foundation

nonisolated package struct LocalSSHDiscoveryRunOwnership: Equatable, Sendable {
    private(set) var activeRunID: UUID?

    mutating package func start(runID: UUID) {
        activeRunID = runID
    }

    package func owns(runID: UUID) -> Bool {
        activeRunID == runID
    }

    @discardableResult
    mutating package func stop(runID: UUID) -> Bool {
        guard owns(runID: runID) else { return false }
        activeRunID = nil
        return true
    }
}
