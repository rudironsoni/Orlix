import Foundation

nonisolated public enum TerminalSplitCommand: Equatable, Sendable {
    public case splitRight
    public case splitDown
    public case closeFocusedPane
    public case toggleZoom
    public case selectPrevious
    public case selectNext
    public case selectAbove
    public case selectBelow
    public case selectLeft
    public case selectRight
    public case equalize
    public case moveDividerUp
    public case moveDividerDown
    public case moveDividerLeft
    public case moveDividerRight

    public var createsPane: Bool {
        self == .splitRight || self == .splitDown
    }
}

public enum TerminalSplitCommandOutcome: Equatable {
    public case performed
    public case unavailable
    public case requiresUpgrade
    public case requiresCloseConfirmation
}

nonisolated package struct TerminalSplitShortcutModifiers: OptionSet, Sendable {
    package let rawValue: UInt8

    static package let command = Self(rawValue: 1 << 0)
    static package let shift = Self(rawValue: 1 << 1)
    static package let control = Self(rawValue: 1 << 2)
    static package let alternate = Self(rawValue: 1 << 3)
}

nonisolated public enum TerminalSplitShortcutKey: Equatable, Sendable {
    public case character(String)
    public case upArrow
    public case downArrow
    public case leftArrow
    public case rightArrow
}

nonisolated package enum TerminalSplitShortcutRouting {
    static package let upArrow = "\u{F700}"
    static package let downArrow = "\u{F701}"
    static package let leftArrow = "\u{F702}"
    static package let rightArrow = "\u{F703}"

    nonisolated static package func command(
        for input: String,
        modifiers: TerminalSplitShortcutModifiers
    ) -> TerminalSplitCommand? {
        command(for: key(for: input), modifiers: modifiers)
    }

    nonisolated static package func command(
        for key: TerminalSplitShortcutKey,
        modifiers: TerminalSplitShortcutModifiers
    ) -> TerminalSplitCommand? {
        switch (key, modifiers) {
        case (.character("d"), [.command]):
            return .splitRight
        case (.character("d"), [.command, .shift]):
            return .splitDown
        case (.character("w"), [.command]):
            return .closeFocusedPane
        case (.character("\r"), [.command, .shift]),
             (.character("\n"), [.command, .shift]):
            return .toggleZoom
        case (.character("["), [.command]):
            return .selectPrevious
        case (.character("]"), [.command]):
            return .selectNext
        case (.upArrow, [.command, .alternate]):
            return .selectAbove
        case (.downArrow, [.command, .alternate]):
            return .selectBelow
        case (.leftArrow, [.command, .alternate]):
            return .selectLeft
        case (.rightArrow, [.command, .alternate]):
            return .selectRight
        case (.character("="), [.command, .control]):
            return .equalize
        case (.upArrow, [.command, .control]):
            return .moveDividerUp
        case (.downArrow, [.command, .control]):
            return .moveDividerDown
        case (.leftArrow, [.command, .control]):
            return .moveDividerLeft
        case (.rightArrow, [.command, .control]):
            return .moveDividerRight
        default:
            return nil
        }
    }

    private nonisolated static func key(for input: String) -> TerminalSplitShortcutKey {
        switch input {
        case upArrow, "UIKeyInputUpArrow":
            return .upArrow
        case downArrow, "UIKeyInputDownArrow":
            return .downArrow
        case leftArrow, "UIKeyInputLeftArrow":
            return .leftArrow
        case rightArrow, "UIKeyInputRightArrow":
            return .rightArrow
        default:
            return .character(input.lowercased())
        }
    }
}
