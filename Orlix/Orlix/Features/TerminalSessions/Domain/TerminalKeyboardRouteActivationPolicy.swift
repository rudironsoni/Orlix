#if os(iOS)
nonisolated package enum TerminalKeyboardRouteActivationPolicy {
    nonisolated package enum SceneActivation: Sendable {
        case foregroundActive
        case foregroundInactive
        case background
    }

    nonisolated package enum Effect: Equatable, Sendable {
        case activate
        /// Preserve the user's typing intent while relinquishing UIKit's
        /// first-responder ownership until this scene becomes locally active.
        case suspend
        case deactivate
    }

    nonisolated package enum WindowOwnership: Equatable, Sendable {
        case unknown
        case key
        case notKey
    }

    nonisolated package enum PresentationOwnership: Equatable, Sendable {
        case terminal
        case routeModal
    }

    static package func effect(
        routeVisible: Bool,
        terminalSelected: Bool,
        sceneActivation: SceneActivation,
        windowOwnership: WindowOwnership = .unknown,
        presentationOwnership: PresentationOwnership = .terminal,
        contentObscured: Bool = false
    ) -> Effect {
        guard routeVisible,
              terminalSelected,
              presentationOwnership == .terminal,
              !contentObscured else {
            return .deactivate
        }
        switch sceneActivation {
        case .foregroundActive:
            return windowOwnership == .notKey ? .deactivate : .activate
        case .foregroundInactive, .background:
            return .suspend
        }
    }
}
#endif
