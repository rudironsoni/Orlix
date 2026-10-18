import Foundation

nonisolated package struct TerminalFontFamily: Identifiable, Hashable, Sendable {
    nonisolated package enum Source: String, CaseIterable, Hashable, Sendable {
        case builtIn
        case custom
        case system
    }

    package let name: String
    package let source: Source

    package var id: String { name }
}
