//
//  GhosttyTerminalFocusPresentation+iOS.swift
//  Orlix
//
//  iOS terminal focus and keyboard presentation.
//

#if os(iOS)
import UIKit

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package extension GhosttyTerminalView {
    package enum TerminalInputConfiguration: Equatable {
        case systemWithAccessory
        case systemWithoutAccessory
        case suppressed
    }

    override package var canBecomeFirstResponder: Bool {
        return true
    }

    package var isTextInputSessionEligible: Bool {
        guard !isShuttingDown else { return false }
        guard window != nil, !isHidden, alpha > 0.01 else { return false }
        if let activationState = window?.windowScene?.activationState {
            return activationState == .foregroundActive
        }
        return UIApplication.shared.applicationState == .active
    }

    package var isFindNavigatorVisible: Bool {
        isFindNavigatorActive
    }

    package var isHardwareKeyboardAttached: Bool {
        hasHardwareKeyboardAttached
    }

    package var shouldRestoreKeyboardFocusOnReconnect: Bool {
        keyboardFocusPolicy.shouldRestoreOnReconnect
    }

    package var allowsAutomaticKeyboardFocus: Bool {
        keyboardFocusPolicy.allowsAutomaticFocus && !isFindNavigatorActive
    }

    package var isKeyboardInBrowseMode: Bool {
        keyboardFocusPolicy.isBrowsing
    }

    package var isFindNavigatorActive: Bool {
        guard #available(iOS 16.0, *) else { return false }
        return findNavigatorLifecycle.isActive
            || nativeFindInteraction?.isFindNavigatorVisible == true
    }

    package var canRouteTerminalInput: Bool {
        acceptsTerminalInput && !isFindNavigatorActive
    }

    package var canRouteProxyDeleteBackward: Bool {
        canRouteTerminalInput
    }

    package var isTerminalTextInputActive: Bool {
        isKeyboardTextInputActive || super.isFirstResponder
    }

    package var isKeyboardTextInputActive: Bool {
        imeProxyTextView.isFirstResponder
    }

    package var shouldSuppressSoftwareKeyboard: Bool {
        keyboardFocusPolicy.shouldSuppressSoftwareKeyboard(
            hasHardwareKeyboardAttached: hasHardwareKeyboardAttached
        )
    }

    package var terminalInputConfiguration: TerminalInputConfiguration {
        if shouldSuppressSoftwareKeyboard {
            return .suppressed
        }
        if isFindNavigatorActive || suppressAccessoryForMissingSoftwareKeyboard {
            return .systemWithoutAccessory
        }
        return .systemWithAccessory
    }

    package func resolvedInputView() -> UIView? {
        #if DEBUG
        if keyboardUITestSoftwareKeyboardFailure == .untilSessionRebuild {
            return hiddenKeyboardInputView
        }
        #endif
        return shouldSuppressSoftwareKeyboard ? hiddenKeyboardInputView : nil
    }

    package func reloadTerminalInputViewsIfActive() {
        guard imeProxyTextView.isFirstResponder else { return }
        #if DEBUG
        keyboardInputViewReloadCount += 1
        #endif
        imeProxyTextView.reloadInputViews()
    }

    package func reloadTerminalInputViews(
        ifChangedFrom previousInputConfiguration: TerminalInputConfiguration
    ) {
        guard previousInputConfiguration != terminalInputConfiguration else { return }
        reloadTerminalInputViewsIfActive()
    }

    @discardableResult
    package func acquireTerminalInput() -> Bool {
        requestKeyboardFocus(for: .initialActivation)
    }

    @discardableResult
    package func forceSoftwareKeyboardInput() -> Bool {
        requestKeyboardFocus(for: .explicitUserRequest)
    }

    @discardableResult
    package func requestKeyboardFocus(for reason: TerminalKeyboardFocusReason) -> Bool {
        let reasonDescription = String(describing: reason)
        logKeyboardLifecycle("focus.request.begin", detail: "reason=\(reasonDescription)")
        guard prepareKeyboardFocus(for: reason) else {
            logKeyboardLifecycle("focus.request.rejected", result: false, detail: "reason=\(reasonDescription)")
            return false
        }
        let result = becomeFirstResponder()
        logKeyboardLifecycle("focus.request.end", result: result, detail: "reason=\(reasonDescription)")
        return result
    }

    private func prepareKeyboardFocus(for reason: TerminalKeyboardFocusReason) -> Bool {
        guard !isFindNavigatorActive else { return false }
        if reason != .hardwareKeyboard {
            refreshHardwareKeyboardAttachmentFromSystem()
        }
        let previousInputConfiguration = terminalInputConfiguration
        guard keyboardFocusPolicy.requestFocus(for: reason) else { return false }
        clearNativeSelectionStateForTerminalInput()
        notifyKeyboardBrowseModeChange(
            previousInputConfiguration: previousInputConfiguration
        )
        return true
    }

    package func dismissKeyboardForUser(suppressDirectTouchRefocus: Bool = false) {
        let previousInputConfiguration = terminalInputConfiguration
        keyboardFocusPolicy.dismissForUser()
        notifyKeyboardBrowseModeChange(
            previousInputConfiguration: previousInputConfiguration
        )
        if suppressDirectTouchRefocus {
            suppressDirectTouchKeyboardFocusUntil = Date().addingTimeInterval(0.35)
        }
        if !isTerminalTextInputActive {
            _ = focusTerminalInputWithoutShowingSoftwareKeyboard()
        }
    }

    @discardableResult
    package func focusTerminalInputWithoutShowingSoftwareKeyboard() -> Bool {
        guard !isFindNavigatorActive else { return false }
        let previousInputConfiguration = terminalInputConfiguration
        keyboardFocusPolicy.dismissForUser()
        refreshHardwareKeyboardAttachmentFromSystem()
        clearNativeSelectionStateForTerminalInput()
        notifyKeyboardBrowseModeChange(
            previousInputConfiguration: previousInputConfiguration
        )
        return becomeFirstResponder()
    }

    package func focusForHardwareKeyboardIfNeeded() {
        guard hasHardwareKeyboardAttached,
              canRouteTerminalInput,
              isTextInputSessionEligible,
              !isFindNavigatorActive else {
            return
        }
        guard keyboardFocusPolicy.isBrowsing || !isTerminalTextInputActive else {
            return
        }
        _ = requestKeyboardFocus(for: .hardwareKeyboard)
    }

    @discardableResult
    package func exitNativeSelectionTextInputContextForTerminalInput() -> Bool {
        guard isNativeSelectionTextInputContext else { return true }
        guard !isFindNavigatorActive else { return false }

        clearNativeSelectionStateForTerminalInput()
        notifyDirectTouchOnTerminal()
        return true
    }

    package func clearNativeSelectionStateForTerminalInput() {
        guard usesNativeTouchSelection else { return }
        let hadSelection = nativeSelectionLifecycle.selection != nil
        if hadSelection {
            imeProxyTextView.inputDelegate?.selectionWillChange(imeProxyTextView)
        }
        nativeSelectionLifecycle.cancel()
        if hadSelection {
            imeProxyTextView.inputDelegate?.selectionDidChange(imeProxyTextView)
        }
    }

    package func releaseTerminalInput() {
        _ = resignFirstResponder()
    }

    package func dismissKeyboardFromToolbar() {
        #if DEBUG
        keyboardHideRequestCount += 1
        #endif
        // Publish the user's intent while the accessory is still mounted. The
        // route can then install its Keyboard/Voice recovery controls before
        // UIKit tears down the input view hierarchy.
        onKeyboardAccessoryHideRequested?()
        dismissKeyboardForUser(suppressDirectTouchRefocus: true)
    }

    package func shouldAutoFocusKeyboard(for touches: Set<UITouch>) -> Bool {
        guard !isFindNavigatorActive else { return false }
        guard keyboardFocusPolicy.allowsAutomaticFocus else { return false }
        guard touches.contains(where: { $0.type == .direct }) else { return true }
        return Date() >= suppressDirectTouchKeyboardFocusUntil
    }

    package func setTerminalInputAccessorySuppressed(_ suppressed: Bool) {
        guard suppressAccessoryForMissingSoftwareKeyboard != suppressed else { return }
        let previousInputConfiguration = terminalInputConfiguration
        suppressAccessoryForMissingSoftwareKeyboard = suppressed
        if previousInputConfiguration != terminalInputConfiguration {
            reloadTerminalInputViewsIfActive()
        }
        logKeyboardLifecycle("accessory.suppression.changed", detail: "suppressed=\(suppressed)")
    }

    package func notifyKeyboardBrowseModeChange(
        previousInputConfiguration: TerminalInputConfiguration
    ) {
        onKeyboardBrowseModeChange?(keyboardFocusPolicy.isBrowsing)
        if previousInputConfiguration != terminalInputConfiguration {
            reloadTerminalInputViewsIfActive()
        }
    }

    /// Tears the input session down and rebuilds it across runloop turns.
    /// A same-tick resign/become pair is coalesced by UIKit into "nothing
    /// changed" and cannot revive a dead keyboard scene (iOS 26 "No scene
    /// exists for this identity"); the responder-free turn gives InputUI a
    /// real session boundary before the coordinator requests reacquisition.
    package func releaseTerminalInputForReacquisition(completion: @escaping () -> Void) {
        #if DEBUG
        keyboardInputSessionRebuildCount += 1
        #endif
        logKeyboardLifecycle("session.rebuild.begin")
        releaseTerminalInput()
        #if DEBUG
        if keyboardUITestSoftwareKeyboardFailure == .untilSessionRebuild {
            keyboardUITestSoftwareKeyboardFailure = .none
        }
        #endif
        logKeyboardLifecycle("session.rebuild.released")
        DispatchQueue.main.async { [weak self] in
            self?.logKeyboardLifecycle("session.rebuild.readyForReacquisition")
            completion()
        }
    }

    package func notifyFindNavigatorVisibilityChange(_ visibilityOverride: Bool? = nil) {
        onFindNavigatorVisibilityChange?(visibilityOverride ?? isFindNavigatorVisible)
    }

    package func notifyDirectTouchOnTerminal(isFocusTap: Bool = false) {
        guard !isFindNavigatorActive else { return }
        terminalContextMenuActions?.focus()
        onTerminalDirectTouch?(isFocusTap)
    }

    override package var isFirstResponder: Bool {
        isTerminalTextInputActive
    }

    override package func becomeFirstResponder() -> Bool {
        guard isTextInputSessionEligible else { return false }
        return imeProxyTextView.becomeFirstResponder()
    }

    override package func resignFirstResponder() -> Bool {
        guard imeProxyTextView.isFirstResponder || super.isFirstResponder else { return true }
        let proxyResult: Bool
        if imeProxyTextView.isFirstResponder {
            proxyResult = imeProxyTextView.resignFirstResponder()
        } else {
            proxyResult = true
        }
        let ownResult = super.isFirstResponder ? super.resignFirstResponder() : true
        if (proxyResult && ownResult) || !isTextInputSessionEligible {
            imeProxyFocusDidChange(isFocused: false)
            pendingSystemTextInputHardwareKeys.removeAll()
        }
        return (proxyResult && ownResult) || !isTextInputSessionEligible
    }
}

#endif
