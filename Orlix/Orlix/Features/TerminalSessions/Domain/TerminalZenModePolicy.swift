nonisolated package enum TerminalZenModePolicy {
    nonisolated static package func canEnter(
        isTerminalSelected: Bool,
        hasActiveTerminal: Bool
    ) -> Bool {
        isTerminalSelected && hasActiveTerminal
    }

    nonisolated static package func resolvedEnabled(
        requested: Bool,
        hasRouteContext: Bool
    ) -> Bool {
        requested && hasRouteContext
    }
}
