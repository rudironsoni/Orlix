//
//  GhosttyTerminalAccessoryPresentation+iOS.swift
//  Orlix
//
//  iOS terminal keyboard accessory presentation.
//

#if os(iOS)
import UIKit
#if ORLIX_MODULAR_APP
import OrlixModels
#endif

// MARK: - Keyboard Accessory View

package extension GhosttyTerminalView {
    package var shouldHideKeyboardAccessoryBar: Bool {
        terminalInputConfiguration != .systemWithAccessory
    }

    package func resolvedInputAccessoryView() -> UIView? {
        guard !shouldHideKeyboardAccessoryBar else {
            return nil
        }
        if keyboardToolbar == nil {
            let toolbar = TerminalInputAccessoryView(
                terminalOwner: self,
                inputSnapshot: terminalAccessoryInputSnapshot,
                onKey: { [weak self] key in
                    self?.handleToolbarKey(key)
                },
                onCustomAction: { [weak self] action in
                    self?.handleToolbarCustomAction(action)
                },
                onVoice: onVoiceButtonTapped,
                onDismissKeyboard: { [weak self] in
                    self?.dismissKeyboardFromToolbar()
                }
            )
            keyboardToolbar = toolbar
            toolbar.setMouseReportingSuppressed(isMouseReportingSuppressed)
        } else {
            keyboardToolbar?.onVoice = onVoiceButtonTapped
        }
        return keyboardToolbar
    }

    package func applyTerminalAccessoryInputSnapshot(_ snapshot: TerminalAccessoryInputSnapshot) {
        guard snapshot != terminalAccessoryInputSnapshot else { return }
        terminalAccessoryInputSnapshot = snapshot
        keyboardToolbar?.apply(snapshot)
    }

    package func refreshTerminalInputAccessoryAppearance() {
        keyboardToolbar?.refreshAppearance()
    }

    private func handleToolbarKey(_ key: TerminalKey) {
        sendToolbarKey(key)
    }
}

#endif
