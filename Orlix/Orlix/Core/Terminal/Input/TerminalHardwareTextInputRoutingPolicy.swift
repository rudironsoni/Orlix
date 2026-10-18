import Foundation

nonisolated package enum TerminalHardwareTextInputRoutingPolicy {
    nonisolated static package func inputModeAllowsOneToOneHardwareText(
        _ primaryLanguage: String?
    ) -> Bool {
        guard let language = primaryLanguage?
            .split(whereSeparator: { $0 == "-" || $0 == "_" })
            .first?
            .lowercased() else {
            return false
        }

        switch language {
        case "ja", "ko", "zh":
            return false
        default:
            return true
        }
    }

    /// Returns text that is safe to consume at the `UIPress` boundary. UIKit
    /// already resolves `UIKey.characters` through the active hardware layout,
    /// so handling ordinary letters and numbers here preserves layout-aware
    /// input while preventing the text system from retaining the hold for its
    /// alternate-character UI. Inputs that may participate in dead keys,
    /// shortcuts, or candidate-based IMEs remain on the system text path.
    nonisolated static package func directlyRoutableText(
        _ characters: String,
        primaryLanguage: String?,
        hasControlModifier: Bool,
        hasAlternateModifier: Bool,
        hasCommandModifier: Bool,
        hasActiveIMEComposition: Bool
    ) -> String? {
        guard !hasControlModifier,
              !hasAlternateModifier,
              !hasCommandModifier,
              !hasActiveIMEComposition else {
            return nil
        }

        let normalized = characters.precomposedStringWithCanonicalMapping
        guard normalized.count == 1,
              !normalized.isEmpty,
              normalized.unicodeScalars.allSatisfy(CharacterSet.alphanumerics.contains) else {
            return nil
        }

        guard inputModeAllowsOneToOneHardwareText(primaryLanguage) else {
            return nil
        }
        return normalized
    }

    static package func shouldRoutePressToSystemTextInput(
        hasControlModifier: Bool,
        hasAlternateModifier: Bool,
        usesAlternateModifierAsTerminalAlt: Bool = false,
        hasCommandModifier: Bool,
        hasActiveIMEComposition: Bool,
        isSystemTextInputToggleKey: Bool,
        isTextInputModifierOnlyKey: Bool,
        hasTerminalFallbackKey: Bool,
        keyProducesText: Bool
    ) -> Bool {
        if hasCommandModifier {
            return false
        }
        if usesAlternateModifierAsTerminalAlt {
            return false
        }
        if isTextInputModifierOnlyKey {
            return true
        }
        if hasActiveIMEComposition {
            return true
        }
        if hasControlModifier {
            return false
        }
        if isSystemTextInputToggleKey {
            return true
        }
        if hasTerminalFallbackKey {
            return false
        }
        if hasAlternateModifier {
            return keyProducesText
        }
        if keyProducesText {
            return true
        }
        return false
    }

    static package func shouldRouteBackwardDeleteToSystemTextInput(
        inputModeAllowsOneToOneText: Bool,
        hasLocalTextInputSession: Bool,
        hasControlModifier: Bool,
        hasAlternateModifier: Bool,
        hasCommandModifier: Bool
    ) -> Bool {
        !inputModeAllowsOneToOneText
            && hasLocalTextInputSession
            && !hasControlModifier
            && !hasAlternateModifier
            && !hasCommandModifier
    }

    static package func shouldRecordPendingInterpretedHardwareKey(
        keyProducesText: Bool,
        hasControlModifier: Bool,
        hasAlternateModifier: Bool,
        hasCommandModifier: Bool,
        hasActiveIMEComposition: Bool,
        isSystemTextInputToggleKey: Bool,
        inputModeAllowsOneToOneText: Bool
    ) -> Bool {
        keyProducesText
            && inputModeAllowsOneToOneText
            && !hasActiveIMEComposition
            && !hasControlModifier
            && !hasAlternateModifier
            && !hasCommandModifier
            && !isSystemTextInputToggleKey
    }

    static package func shouldMirrorSystemTextInputModifierPressToTerminal(
        isTextInputModifierOnlyKey: Bool
    ) -> Bool {
        isTextInputModifierOnlyKey
    }
}

nonisolated package enum TerminalHardwareKeyRepeatSource: Sendable {
    case directTerminal
    case layoutResolvedText
    case systemInterpretedText

    package var lifecycleDescription: String {
        switch self {
        case .directTerminal: "directTerminal"
        case .layoutResolvedText: "layoutResolvedText"
        case .systemInterpretedText: "systemInterpretedText"
        }
    }
}

nonisolated package enum TerminalHardwareKeyRepeatPolicy {
    static package func shouldRepeat(
        source: TerminalHardwareKeyRepeatSource,
        isPrintableKey: Bool,
        isRepeatableSpecialKey: Bool,
        hasControlModifier: Bool,
        hasAlternateModifier: Bool,
        hasCommandModifier: Bool,
        hasActiveIMEComposition: Bool
    ) -> Bool {
        guard !hasCommandModifier,
              !hasActiveIMEComposition else {
            return false
        }

        switch source {
        case .directTerminal:
            return isPrintableKey || isRepeatableSpecialKey
        case .layoutResolvedText, .systemInterpretedText:
            return isPrintableKey && !hasControlModifier && !hasAlternateModifier
        }
    }
}

nonisolated package enum TerminalKeyInputModifierPolicy {
    static package func consumedModifiers(for mods: Ghostty.Input.Mods) -> Ghostty.Input.Mods {
        mods.subtracting([.ctrl, .super])
    }
}
