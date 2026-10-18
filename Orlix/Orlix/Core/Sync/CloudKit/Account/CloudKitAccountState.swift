nonisolated package enum CloudKitAccountState: Equatable, Sendable {
    package case checking
    package case available
    package case noAccount
    package case restricted
    package case couldNotDetermine
    package case temporarilyUnavailable
    package case unknown(rawValue: Int)
    package case failed(detail: String)
    package case disabled
}
