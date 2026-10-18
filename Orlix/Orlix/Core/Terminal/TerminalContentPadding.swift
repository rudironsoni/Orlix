nonisolated package struct TerminalContentPadding: Equatable, Sendable {
    static package let defaultValue = TerminalContentPadding(
        horizontal: TerminalDefaults.defaultContentPadding,
        vertical: TerminalDefaults.defaultContentPadding
    )

    package let horizontal: Int
    package let vertical: Int

    package init(horizontal: Double, vertical: Double) {
        self.horizontal = Int(TerminalDefaults.clampedContentPadding(horizontal))
        self.vertical = Int(TerminalDefaults.clampedContentPadding(vertical))
    }
}
