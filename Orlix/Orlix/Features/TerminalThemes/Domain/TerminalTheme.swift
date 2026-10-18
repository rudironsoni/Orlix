//
//  TerminalTheme.swift
//  Orlix
//

import Foundation

nonisolated package struct TerminalTheme: Identifiable, Codable, Equatable, Sendable {
    package let id: UUID
    package var name: String
    package var content: String
    package var updatedAt: Date
    package var deletedAt: Date?

    package init(
        id: UUID = UUID(),
        name: String,
        content: String,
        updatedAt: Date = Date(),
        deletedAt: Date? = nil
    ) {
        self.id = id
        self.name = name
        self.content = content
        self.updatedAt = updatedAt
        self.deletedAt = deletedAt
    }

    package var isDeleted: Bool {
        deletedAt != nil
    }

    package var validationState: TerminalThemeValidationState {
        do {
            let content = try TerminalThemeValidator.validateAndNormalizeThemeContent(content)
            _ = try TerminalThemeValidator.validateAndNormalizeThemeName(name)
            return .ready(normalizedContent: content)
        } catch {
            return .needsRepair
        }
    }

    package var canApply: Bool {
        if case .ready = validationState { return true }
        return false
    }
}

nonisolated enum TerminalThemeValidationState: Equatable, Sendable {
    case ready(normalizedContent: String)
    case needsRepair
}

nonisolated package enum TerminalThemeMergePolicy {
    static package func merge(local: [TerminalTheme], remote: [TerminalTheme]) -> [TerminalTheme] {
        var themesByID: [UUID: TerminalTheme] = [:]
        for theme in local {
            if let existing = themesByID[theme.id], existing.updatedAt >= theme.updatedAt {
                continue
            }
            themesByID[theme.id] = theme
        }

        for untrustedTheme in remote {
            guard let theme = try? TerminalThemeValidator.validateStoredTheme(untrustedTheme) else {
                continue
            }
            if let existing = themesByID[theme.id], existing.updatedAt >= theme.updatedAt {
                continue
            }
            themesByID[theme.id] = theme
        }
        return Array(themesByID.values)
    }
}

nonisolated package struct TerminalThemePreference: Codable, Equatable, Sendable {
    package var darkThemeName: String
    package var lightThemeName: String
    package var usePerAppearanceTheme: Bool
    package var updatedAt: Date
}
