import Foundation

package enum TerminalPointerButton: Equatable {
    case left
    case right
    case middle
}

package enum TerminalPointerInputRoutingPolicy {
    static package func shouldSendDirectTouchClick(
        terminalMouseCaptured: Bool,
        terminalInputAvailable: Bool,
        selectionInteractionActive: Bool
    ) -> Bool {
        terminalMouseCaptured && terminalInputAvailable && !selectionInteractionActive
    }

    static package func pointerButton(
        isPrimaryPressed: Bool,
        isSecondaryPressed: Bool,
        isMiddlePressed: Bool,
        hasControlModifier: Bool
    ) -> TerminalPointerButton? {
        if isSecondaryPressed {
            return .right
        }

        if isMiddlePressed {
            return .middle
        }

        if isPrimaryPressed {
            return hasControlModifier ? .right : .left
        }

        return nil
    }

    static package func shouldShowHostContextMenu(
        button: TerminalPointerButton,
        terminalHandledButtonPress: Bool,
        terminalMouseCaptured: Bool
    ) -> Bool {
        button == .right
            && !terminalHandledButtonPress
            && !terminalMouseCaptured
    }

    static package func shouldAllowScrollGesture(
        isIndirectPointer: Bool,
        isPointerButtonPressed: Bool,
        hasActiveTerminalPointerButton: Bool
    ) -> Bool {
        guard isIndirectPointer else { return true }
        return !isPointerButtonPressed && !hasActiveTerminalPointerButton
    }
}

package enum TerminalSelectionRoutingPolicy {
    static package func shouldAllowHostSelection(
        terminalMouseCaptured: Bool,
        selectionInteractionActive: Bool
    ) -> Bool {
        !terminalMouseCaptured || selectionInteractionActive
    }
}

package struct TerminalSelectionAutoscrollDecision: Equatable {
    package enum Edge: Equatable {
        case top
        case bottom
    }

    package let edge: Edge
    package let scrollDelta: Double
}

package enum TerminalSelectionAutoscrollPolicy {
    static package func decision(
        locationY: Double,
        viewportHeight: Double,
        edgeInset: Double,
        maximumScrollDelta: Double
    ) -> TerminalSelectionAutoscrollDecision? {
        guard viewportHeight > 0, edgeInset > 0, maximumScrollDelta > 0 else {
            return nil
        }

        if locationY < edgeInset {
            let distance = min(max(edgeInset - locationY, 0), edgeInset)
            let intensity = max(distance / edgeInset, 0.1)
            return TerminalSelectionAutoscrollDecision(
                edge: .top,
                scrollDelta: maximumScrollDelta * intensity
            )
        }

        let bottomEdge = viewportHeight - edgeInset
        if locationY > bottomEdge {
            let distance = min(max(locationY - bottomEdge, 0), edgeInset)
            let intensity = max(distance / edgeInset, 0.1)
            return TerminalSelectionAutoscrollDecision(
                edge: .bottom,
                scrollDelta: -maximumScrollDelta * intensity
            )
        }

        return nil
    }
}

#if os(iOS)
import UIKit

package extension TerminalPointerButton {
    package var ghosttyMouseButton: Ghostty.Input.MouseButton {
        switch self {
        case .left:
            .left
        case .right:
            .right
        case .middle:
            .middle
        }
    }
}

package extension TerminalPointerInputRoutingPolicy {
    static package func ghosttyModifiers(from flags: UIKeyModifierFlags) -> Ghostty.Input.Mods {
        Ghostty.Input.Mods(uiKeyModifiers: flags)
    }

    static package func pointerButton(
        for buttonMask: UIEvent.ButtonMask,
        modifiers: UIKeyModifierFlags
    ) -> TerminalPointerButton? {
        pointerButton(
            isPrimaryPressed: buttonMask.contains(.primary),
            isSecondaryPressed: buttonMask.contains(.secondary),
            isMiddlePressed: buttonMask.contains(.button(3)),
            hasControlModifier: modifiers.contains(.control)
        )
    }
}
#endif
