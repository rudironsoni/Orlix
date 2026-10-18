import Foundation

nonisolated package struct TerminalFontPreference: Codable, Equatable, Sendable {
    static package var defaultValue: Self {
        Self(
            primaryFamily: TerminalDefaults.defaultFontName,
            cjkFamily: nil,
            updatedAt: .distantPast
        )
    }

    package let primaryFamily: String
    package let cjkFamily: String?
    package let updatedAt: Date

    package init(
        primaryFamily: String,
        cjkFamily: String?,
        updatedAt: Date
    ) {
        let selection = TerminalFontRuntimeSelection(
            primaryFamily: primaryFamily,
            cjkFamily: cjkFamily
        )
        self.primaryFamily = selection.primaryFamily
        self.cjkFamily = selection.cjkFamily
        self.updatedAt = updatedAt
    }
}
