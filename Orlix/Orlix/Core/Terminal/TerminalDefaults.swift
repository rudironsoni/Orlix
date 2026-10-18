//
//  TerminalDefaults.swift
//  Orlix
//

import Foundation
import CoreFoundation
#if os(iOS)
import UIKit
#elseif os(macOS)
import AppKit
#endif

nonisolated package enum TerminalCursorStyle: String, CaseIterable, Codable, Identifiable, Sendable {
    case block
    case bar
    case underline
    case blockHollow = "block_hollow"

    package var id: String { rawValue }

    package var displayName: String {
        switch self {
        case .block: return String(localized: "Block")
        case .bar: return String(localized: "Bar")
        case .underline: return String(localized: "Underline")
        case .blockHollow: return String(localized: "Block Hollow")
        }
    }
}

nonisolated package enum TerminalZoomAction: Equatable, Sendable {
    case zoomIn
    case zoomOut
    case reset
}

nonisolated package enum TerminalZoomShortcutKey: Sendable {
    case equal
    case literalPlus
    case minus
    case zero
    case keypadPlus
    case keypadMinus
    case keypadZero
}

nonisolated package enum TerminalZoomShortcutRouting {
    nonisolated static package func key(forCommandInput input: String) -> TerminalZoomShortcutKey? {
        switch input {
        case "=": .equal
        case "+": .literalPlus
        case "-": .minus
        case "0": .zero
        default: nil
        }
    }

    nonisolated static package func resolvedKey(
        physicalKey: TerminalZoomShortcutKey?,
        characters: String
    ) -> TerminalZoomShortcutKey? {
        if characters == "+" {
            return .literalPlus
        }
        return physicalKey
    }

    nonisolated static package func action(
        for key: TerminalZoomShortcutKey,
        hasCommandModifier: Bool,
        hasShiftModifier: Bool,
        hasControlModifier: Bool,
        hasAlternateModifier: Bool
    ) -> TerminalZoomAction? {
        guard hasCommandModifier,
              !hasControlModifier,
              !hasAlternateModifier else {
            return nil
        }

        switch key {
        case .equal:
            return .zoomIn
        case .literalPlus, .keypadPlus:
            return .zoomIn
        case .minus, .keypadMinus:
            return hasShiftModifier ? nil : .zoomOut
        case .zero, .keypadZero:
            return hasShiftModifier ? nil : .reset
        }
    }
}

nonisolated package enum TerminalOptionAsAltMode: String, CaseIterable, Identifiable, Sendable {
    case none
    case left
    case right
    case both

    package var id: String { rawValue }

    package var displayName: String {
        switch self {
        case .none: return String(localized: "Neither Option Key")
        case .left: return String(localized: "Left Option Key")
        case .right: return String(localized: "Right Option Key")
        case .both: return String(localized: "Both Option Keys")
        }
    }

    package func usesOptionKeyAsAlt(_ side: TerminalOptionKeySide) -> Bool {
        switch (self, side) {
        case (.none, _): false
        case (.left, .left), (.right, .right), (.both, _): true
        default: false
        }
    }
}

nonisolated package enum TerminalOptionKeySide: Sendable {
    case left
    case right
}

nonisolated package struct TerminalZoomResult: Hashable, Sendable {
    package let presentationOverrides: TerminalPresentationOverrides
    package let effectiveFontSize: Double
}

nonisolated package struct TerminalPresentationOverrides: Codable, Hashable, Sendable {
    nonisolated static package let empty = TerminalPresentationOverrides()

    package var fontSize: Double?

    package init(fontSize: Double? = nil) {
        self.fontSize = fontSize.map(TerminalDefaults.clampedFontSize)
    }

    package var isEmpty: Bool {
        fontSize == nil
    }

    @MainActor package func resolvedFontSize(defaults: UserDefaults = .standard) -> Double {
        fontSize ?? TerminalDefaults.storedFontSize(defaults: defaults)
    }

    @MainActor package func applyingZoom(
        _ action: TerminalZoomAction,
        defaults: UserDefaults = .standard
    ) -> TerminalPresentationOverrides {
        var overrides = self
        let currentFontSize = resolvedFontSize(defaults: defaults)

        switch action {
        case .zoomIn:
            overrides.fontSize = TerminalDefaults.clampedFontSize(currentFontSize + TerminalDefaults.fontSizeStep)
        case .zoomOut:
            overrides.fontSize = TerminalDefaults.clampedFontSize(currentFontSize - TerminalDefaults.fontSizeStep)
        case .reset:
            overrides.fontSize = nil
        }

        return overrides
    }
}

nonisolated package enum TerminalZoomPresentation {
    static package let pinchZoomInThreshold = 1.12
    static package let pinchZoomOutThreshold = 0.89
    static package let magnificationStepThreshold = 0.12
    static package let indicatorFadeInDuration = 0.12
    static package let indicatorFadeOutDuration = 0.18
    static package let indicatorHideDelay = 0.8
    static package let indicatorGestureEndHideDelay = 0.45
    static package let indicatorMinimumWidth = 112.0
    static package let indicatorMinimumHeight = 72.0

    static package var indicatorTitle: String {
        String(localized: "Font Size")
    }

    static package func formattedFontSize(_ fontSize: Double) -> String {
        String(format: "%.0f pt", fontSize)
    }
}

nonisolated package enum TerminalDefaults {
    static package let fontNameKey = "terminalFontName"
    static package let cjkFontNameKey = "terminalCJKFontName"
    static package let fontSizeKey = "terminalFontSize"
    static package let cursorStyleKey = "terminalCursorStyle"
    static package let cursorBlinkKey = "terminalCursorBlink"
    static package let contentPaddingHorizontalKey = "terminalContentPaddingHorizontal"
    static package let contentPaddingVerticalKey = "terminalContentPaddingVertical"
    static package let sshAutoReconnectKey = "sshAutoReconnect"
    static package let keepScreenAwakeKey = "terminalKeepScreenAwake"
    static package let optionAsAltModeKey = "terminalOptionAsAltMode"
    static package let preserveTerminalSizeForKeyboardKey = "terminalPreserveSizeForKeyboard"
    nonisolated static package let legacyDefaultFontName = "JetBrainsMono Nerd Font"
    nonisolated static package let bundledFontFamilyNames = [
        legacyDefaultFontName,
        "Hack Nerd Font",
        "FiraCode Nerd Font",
        "MesloLGS Nerd Font"
    ]
    nonisolated static package let symbolFallbackFontFamily = legacyDefaultFontName
    #if os(macOS)
    nonisolated static package let automaticTextFallbackFontFamilies = ["Apple SD Gothic Neo"]
    #else
    nonisolated static package let automaticTextFallbackFontFamilies: [String] = []
    #endif
    nonisolated static package let minimumFontSize = 4.0
    nonisolated static package let maximumFontSize = 32.0
    nonisolated static package let fontSizeStep = 1.0
    nonisolated static package let minimumContentPadding = 0.0
    nonisolated static package let maximumContentPadding = 32.0
    nonisolated static package let contentPaddingStep = 1.0
    nonisolated static package let defaultContentPadding = 0.0
    nonisolated static package let defaultCursorStyle: TerminalCursorStyle = .block
    nonisolated static package let defaultCursorBlink = true
    nonisolated static package let defaultKeepScreenAwake = true
    #if os(macOS)
    nonisolated static package let defaultPrimaryFontName = "Menlo"
    #endif

    @MainActor static package func applyIfNeeded() {
        applyIfNeeded(defaults: .standard)
    }

    @MainActor static package func applyIfNeeded(defaults: UserDefaults) {
        seedFontDefaultsIfNeeded(defaults: defaults)
        seedCursorDefaultsIfNeeded(defaults: defaults)
        sanitizeStoredContentPadding(defaults: defaults)

        if defaults.object(forKey: ImagePasteBehavior.userDefaultsKey) == nil {
            let imagePasteBehavior = RichClipboardSettings.resolvedImagePasteBehavior(defaults: defaults)
            defaults.set(imagePasteBehavior.rawValue, forKey: ImagePasteBehavior.userDefaultsKey)
        }
    }

    nonisolated static package func clampedFontSize(_ fontSize: Double) -> Double {
        min(max(fontSize.rounded(), minimumFontSize), maximumFontSize)
    }

    nonisolated static package func clampedContentPadding(_ padding: Double) -> Double {
        guard padding.isFinite else { return defaultContentPadding }
        return min(
            max(padding.rounded(), minimumContentPadding),
            maximumContentPadding
        )
    }

    nonisolated static package func storedContentPadding(
        defaults: UserDefaults = .standard
    ) -> TerminalContentPadding {
        TerminalContentPadding(
            horizontal: storedContentPaddingValue(
                forKey: contentPaddingHorizontalKey,
                defaults: defaults
            ),
            vertical: storedContentPaddingValue(
                forKey: contentPaddingVerticalKey,
                defaults: defaults
            )
        )
    }

    @MainActor static package func storedFontSize(defaults: UserDefaults = .standard) -> Double {
        let stored = defaults.object(forKey: fontSizeKey) as? Double ?? defaultFontSize
        return clampedFontSize(stored)
    }

    static package func sshAutoReconnectEnabled(defaults: UserDefaults = .standard) -> Bool {
        (defaults.object(forKey: sshAutoReconnectKey) as? Bool) ?? true
    }

    static package func keepScreenAwakeEnabled(defaults: UserDefaults = .standard) -> Bool {
        (defaults.object(forKey: keepScreenAwakeKey) as? Bool) ?? defaultKeepScreenAwake
    }

    static package func optionAsAltMode(defaults: UserDefaults = .standard) -> TerminalOptionAsAltMode {
        guard let rawValue = defaults.string(forKey: optionAsAltModeKey) else { return .none }
        return TerminalOptionAsAltMode(rawValue: rawValue) ?? .none
    }

    @MainActor static package var defaultFontSize: Double {
        #if os(macOS)
        return 12.0
        #elseif os(iOS)
        switch UIDevice.current.userInterfaceIdiom {
        case .pad:
            return 12.0
        case .phone:
            return 9.0
        default:
            return 10.0
        }
        #else
        return 10.0
        #endif
    }

    #if os(macOS)
    static package var defaultFontName: String {
        defaultPrimaryFontName
    }
    #else
    static package var defaultFontName: String {
        legacyDefaultFontName
    }
    #endif

    @MainActor private static func seedFontDefaultsIfNeeded(defaults: UserDefaults) {
        #if os(macOS)
        seedMacOSFontDefaultsIfNeeded(defaults: defaults)
        #else
        if let fontName = defaults.string(forKey: fontNameKey) {
            if fontName.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
                defaults.set(defaultFontName, forKey: fontNameKey)
            }
        } else {
            defaults.set(defaultFontName, forKey: fontNameKey)
        }

        if defaults.object(forKey: fontSizeKey) == nil {
            defaults.set(defaultFontSize, forKey: fontSizeKey)
        }
        #endif
    }

    private static func seedCursorDefaultsIfNeeded(defaults: UserDefaults) {
        if let rawStyle = defaults.string(forKey: cursorStyleKey),
           TerminalCursorStyle(rawValue: rawStyle) != nil {
            // Existing value is valid.
        } else {
            defaults.set(defaultCursorStyle.rawValue, forKey: cursorStyleKey)
        }

        if defaults.object(forKey: cursorBlinkKey) == nil {
            defaults.set(defaultCursorBlink, forKey: cursorBlinkKey)
        }
    }

    private static func sanitizeStoredContentPadding(defaults: UserDefaults) {
        for key in [contentPaddingHorizontalKey, contentPaddingVerticalKey] {
            guard let storedValue = defaults.object(forKey: key) else { continue }

            let resolvedValue = storedContentPaddingValue(forKey: key, defaults: defaults)
            guard let number = storedValue as? NSNumber,
                  CFGetTypeID(number) != CFBooleanGetTypeID(),
                  number.doubleValue.isFinite,
                  number.doubleValue == resolvedValue else {
                defaults.set(resolvedValue, forKey: key)
                continue
            }
        }
    }

    private static func storedContentPaddingValue(
        forKey key: String,
        defaults: UserDefaults
    ) -> Double {
        guard let number = defaults.object(forKey: key) as? NSNumber,
              CFGetTypeID(number) != CFBooleanGetTypeID() else {
            return defaultContentPadding
        }
        return clampedContentPadding(number.doubleValue)
    }

    #if os(macOS)
    @MainActor private static func seedMacOSFontDefaultsIfNeeded(defaults: UserDefaults) {
        let storedFontName = defaults.string(forKey: fontNameKey)
        let normalizedStoredFontName = storedFontName?.trimmingCharacters(in: .whitespacesAndNewlines)
        let storedFontSize = defaults.object(forKey: fontSizeKey) as? Double

        if normalizedStoredFontName == nil || normalizedStoredFontName?.isEmpty == true {
            defaults.set(defaultPrimaryFontName, forKey: fontNameKey)
        }

        if storedFontSize == nil {
            defaults.set(defaultFontSize, forKey: fontSizeKey)
        }

        guard let resolvedFontName = defaults.string(forKey: fontNameKey)?.trimmingCharacters(in: .whitespacesAndNewlines),
              !resolvedFontName.isEmpty else {
            return
        }

        let normalizedFontName = normalizedMacOSFontName(
            storedFontName: resolvedFontName,
            fontAvailability: { isAvailableMacOSFont(named: $0) }
        )

        if normalizedFontName != resolvedFontName {
            defaults.set(normalizedFontName, forKey: fontNameKey)
        }
    }

    static package func normalizedMacOSFontName(
        storedFontName: String,
        fontAvailability: (String) -> Bool
    ) -> String {
        fontAvailability(storedFontName) ? storedFontName : defaultPrimaryFontName
    }

    @MainActor private static func isAvailableMacOSFont(named fontName: String) -> Bool {
        NSFont(name: fontName, size: 12) != nil
    }
    #endif
}
