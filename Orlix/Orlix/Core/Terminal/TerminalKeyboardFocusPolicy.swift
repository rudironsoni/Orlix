nonisolated public enum TerminalKeyboardFocusReason: Sendable {
    public case explicitUserRequest
    public case initialActivation
    public case reconnectRestore
    public case directTouch
    public case selectionGesture
    public case hardwareKeyboard
}

nonisolated package struct TerminalKeyboardFocusPolicy: Sendable {
    nonisolated private enum Mode: Sendable {
        case automaticTyping(restoreOnReconnect: Bool)
        case forcedSoftwareTyping
        case browse
    }

    private var mode: Mode = .automaticTyping(restoreOnReconnect: false)

    package var allowsAutomaticFocus: Bool {
        if case .browse = mode {
            return false
        }
        return true
    }

    package var isBrowsing: Bool {
        if case .browse = mode {
            return true
        }
        return false
    }

    package var shouldRestoreOnReconnect: Bool {
        switch mode {
        case .automaticTyping(let restoreOnReconnect):
            return restoreOnReconnect
        case .forcedSoftwareTyping:
            return true
        case .browse:
            return false
        }
    }

    package var forcesSoftwareKeyboardPresentation: Bool {
        if case .forcedSoftwareTyping = mode {
            return true
        }
        return false
    }

    package func shouldSuppressSoftwareKeyboard(hasHardwareKeyboardAttached: Bool) -> Bool {
        isBrowsing || (hasHardwareKeyboardAttached && !forcesSoftwareKeyboardPresentation)
    }

    mutating package func requestFocus(for reason: TerminalKeyboardFocusReason) -> Bool {
        switch reason {
        case .explicitUserRequest:
            mode = .forcedSoftwareTyping
            return true
        case .initialActivation, .directTouch, .selectionGesture, .hardwareKeyboard:
            switch mode {
            case .automaticTyping:
                mode = .automaticTyping(restoreOnReconnect: true)
                return true
            case .forcedSoftwareTyping:
                return true
            case .browse:
                return false
            }
        case .reconnectRestore:
            return shouldRestoreOnReconnect
        }
    }

    mutating package func dismissForUser() {
        mode = .browse
    }
}
