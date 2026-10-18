//
//  TerminalAppearance.swift
//  Orlix
//

nonisolated package enum TerminalColorAppearance: Equatable, Sendable {
    package case light
    package case dark
}

nonisolated package struct TerminalThemeSelection: Equatable, Sendable {
    package let darkThemeName: String
    package let lightThemeName: String
    package let usePerAppearanceTheme: Bool
}

nonisolated package struct TerminalThemePalette: Equatable, Sendable {
    package let backgroundHex: String
    package let foregroundHex: String
    package let cursorHex: String
    package let cursorTextHex: String

    static package let fallback = TerminalThemePalette(
        backgroundHex: "#101418",
        foregroundHex: "#D8E0EA",
        cursorHex: "#F8B26A",
        cursorTextHex: "#101418"
    )
}

nonisolated package struct ResolvedTerminalTheme: Equatable, Sendable {
    package let name: String
    package let palette: TerminalThemePalette
}

nonisolated package struct TerminalAppearanceSnapshot: Equatable, Sendable {
    package let activeAppearance: TerminalColorAppearance
    package let lightTheme: ResolvedTerminalTheme
    package let darkTheme: ResolvedTerminalTheme

    package var activeTheme: ResolvedTerminalTheme {
        theme(for: activeAppearance)
    }

    package func theme(for appearance: TerminalColorAppearance) -> ResolvedTerminalTheme {
        switch appearance {
        case .light:
            lightTheme
        case .dark:
            darkTheme
        }
    }

    static package let fallback = TerminalAppearanceSnapshot(
        activeAppearance: .dark,
        lightTheme: ResolvedTerminalTheme(
            name: "Orlix Light",
            palette: .fallback
        ),
        darkTheme: ResolvedTerminalTheme(
            name: "Orlix Dark",
            palette: .fallback
        )
    )
}
