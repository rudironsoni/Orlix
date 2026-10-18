import Foundation

nonisolated package struct SSHExecOutputBudget: Sendable {
    static package let defaultMaximumBytes = 1 * 1_024 * 1_024

    package let maximumBytes: Int
    private(set) var retainedBytes = 0

    package init(maximumBytes: Int = Self.defaultMaximumBytes) {
        self.maximumBytes = max(0, maximumBytes)
    }

    mutating package func reserve(_ byteCount: Int) -> Bool {
        guard byteCount >= 0,
              byteCount <= maximumBytes - retainedBytes else {
            return false
        }
        retainedBytes += byteCount
        return true
    }
}
