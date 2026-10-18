@MainActor
package struct AnalyticsOptOutAction {
    private let emitAnalyticsDisabled: @MainActor () -> Void

    package init(emitAnalyticsDisabled: @escaping @MainActor () -> Void) {
        self.emitAnalyticsDisabled = emitAnalyticsDisabled
    }

    package func applyTransition(
        from currentValue: Bool,
        to newValue: Bool,
        persist: (Bool) -> Void
    ) {
        if currentValue && !newValue {
            emitAnalyticsDisabled()
        }
        persist(newValue)
    }
}
