import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package extension Ghostty {
    package struct RuntimeConfiguration: Equatable {
        package let fontSelection: TerminalFontRuntimeSelection
        package let fontSize: Double
        package let contentPadding: TerminalContentPadding
        package let cursorStyle: TerminalCursorStyle
        package let cursorBlink: Bool
        package let optionAsAltMode: TerminalOptionAsAltMode
        package let remoteClipboardReadPolicy: TerminalRemoteClipboardReadPolicy

        package init(
            fontSelection: TerminalFontRuntimeSelection,
            fontSize: Double,
            contentPadding: TerminalContentPadding,
            cursorStyleRawValue: String,
            cursorBlink: Bool,
            optionAsAltModeRawValue: String,
            remoteClipboardReadPolicyRawValue: String
        ) {
            self.fontSelection = fontSelection
            self.fontSize = TerminalDefaults.clampedFontSize(fontSize)
            self.contentPadding = contentPadding
            self.cursorStyle = TerminalCursorStyle(rawValue: cursorStyleRawValue)
                ?? TerminalDefaults.defaultCursorStyle
            self.cursorBlink = cursorBlink
            self.optionAsAltMode = TerminalOptionAsAltMode(rawValue: optionAsAltModeRawValue)
                ?? .none
            self.remoteClipboardReadPolicy = TerminalRemoteClipboardReadPolicy(
                rawValue: remoteClipboardReadPolicyRawValue
            ) ?? .defaultValue
        }

        @MainActor static package var defaultValue: Self {
            Self(
                fontSelection: .defaultValue,
                fontSize: TerminalDefaults.defaultFontSize,
                contentPadding: .defaultValue,
                cursorStyleRawValue: TerminalDefaults.defaultCursorStyle.rawValue,
                cursorBlink: TerminalDefaults.defaultCursorBlink,
                optionAsAltModeRawValue: TerminalOptionAsAltMode.none.rawValue,
                remoteClipboardReadPolicyRawValue: TerminalRemoteClipboardReadPolicy.defaultValue.rawValue
            )
        }
    }
}
