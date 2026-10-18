nonisolated public enum CloudKitAccountState: Equatable, Sendable {
    public case checking
    public case available
    public case noAccount
    public case restricted
    public case couldNotDetermine
    public case temporarilyUnavailable
    public case unknown(rawValue: Int)
    public case failed(detail: String)
    public case disabled
}
