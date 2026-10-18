//
//  GhosttyTerminalIME+macOS.swift
//  Orlix
//
//  macOS input method editor integration.
//

#if os(macOS)
import AppKit

/// NSTextInputClient protocol conformance for IME (Input Method Editor) support
extension GhosttyTerminalView: NSTextInputClient {
    package func insertText(_ string: Any, replacementRange: NSRange) {
        imeHandler.insertText(string, replacementRange: replacementRange)
    }

    package func setMarkedText(_ string: Any, selectedRange: NSRange, replacementRange: NSRange) {
        imeHandler.setMarkedText(string, selectedRange: selectedRange, replacementRange: replacementRange)
    }

    package func unmarkText() {
        imeHandler.unmarkText()
    }

    package func selectedRange() -> NSRange {
        return imeHandler.selectedRange()
    }

    package func markedRange() -> NSRange {
        return imeHandler.markedRange()
    }

    package func hasMarkedText() -> Bool {
        return imeHandler.hasMarkedText
    }

    package func attributedSubstring(forProposedRange range: NSRange, actualRange: NSRangePointer?) -> NSAttributedString? {
        return imeHandler.attributedSubstring(forProposedRange: range, actualRange: actualRange)
    }

    package func validAttributesForMarkedText() -> [NSAttributedString.Key] {
        return imeHandler.validAttributesForMarkedText()
    }

    package func firstRect(forCharacterRange range: NSRange, actualRange: NSRangePointer?) -> NSRect {
        return imeHandler.firstRect(
            forCharacterRange: range,
            actualRange: actualRange,
            viewFrame: frame,
            window: window,
            surface: surface?.unsafeCValue
        )
    }

    package func characterIndex(for point: NSPoint) -> Int {
        return imeHandler.characterIndex(for: point)
    }
}

#endif
