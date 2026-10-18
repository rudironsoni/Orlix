#if os(iOS)
import CoreGraphics
import Foundation

nonisolated package struct TerminalKeyboardCoordinatorDiagnosticSnapshot: Equatable, Sendable {
    package var windowAttached: Bool
    package var windowIsKey: Bool
    package var sceneActivationState: String
    package var isFirstResponder: Bool
    package var isSoftwareInputActive: Bool
    package var keyboardLayoutFrame: CGRect? = nil
    package var screenFrame: CGRect? = nil
    package var screenIdentifier: ObjectIdentifier? = nil
    package var isSoftwareKeyboardSuppressed = false
    package var isKeyboardInBrowseMode = false

    package var lifecycleDescription: String {
        [
            "windowAttached=\(windowAttached)",
            "keyWindow=\(windowIsKey)",
            "scene=\(sceneActivationState)",
            "firstResponder=\(isFirstResponder)",
            "softwareInput=\(isSoftwareInputActive)",
            "softwareSuppressed=\(isSoftwareKeyboardSuppressed)",
            "browse=\(isKeyboardInBrowseMode)",
            "keyboardLayoutFrame=\(keyboardLayoutFrame?.debugDescription ?? "nil")",
        ].joined(separator: " ")
    }
}

nonisolated package enum TerminalKeyboardAnimationCurve: Int, Equatable, Sendable {
    case easeInOut
    case easeIn
    case easeOut
    case linear
}

nonisolated package struct TerminalKeyboardEvent: Equatable, Sendable {
    nonisolated package enum Kind: Equatable, Sendable {
        case frameChanged(CGRect?)
        case hidden
    }

    nonisolated package struct Diagnostics: Equatable, Sendable {
        package let name: String
        package let object: String
        package let beginFrame: CGRect?
        package let endFrame: CGRect?
    }

    package let kind: Kind
    package let isLocal: Bool?
    package let sourceScreenIdentifier: ObjectIdentifier?
    package let animationDuration: TimeInterval?
    package let animationCurve: TerminalKeyboardAnimationCurve?
    package let diagnostics: Diagnostics?
}

@MainActor
package protocol TerminalKeyboardEventSource: AnyObject {
    func start(
        handler: @escaping @MainActor @Sendable (TerminalKeyboardEvent) -> Void
    )
    func stop()
}
#endif
