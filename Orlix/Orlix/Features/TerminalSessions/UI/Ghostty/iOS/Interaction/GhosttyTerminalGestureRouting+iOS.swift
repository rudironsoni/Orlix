//
//  GhosttyTerminalGestureRouting+iOS.swift
//  Orlix
//
//  iOS terminal gesture-recognizer routing.
//

#if os(iOS)
import UIKit
#if ORLIX_MODULAR_APP
import OrlixModels
#endif

// MARK: - Gesture Recognizer Delegate

package extension GhosttyTerminalView: UIGestureRecognizerDelegate {
    package func gestureRecognizer(_ gestureRecognizer: UIGestureRecognizer, shouldReceive touch: UITouch) -> Bool {
        if gestureRecognizer == nativeSelectionLongPressRecognizer {
            guard canRouteTerminalInput, !isPaused, !isShuttingDown else { return false }
            return !isPointOnNativeSelectionHandleHitArea(touch.location(in: self))
        }
        if gestureRecognizer == directTouchTapRecognizer {
            guard canRouteTerminalInput, !isPaused, !isShuttingDown else { return false }
            let location = touch.location(in: self)
            return !isPointOnNativeSelectionHandleHitArea(location)
        }
        if gestureRecognizer == pinchRecognizer {
            return canHandlePinchZoom
        }
        if gestureRecognizer == scrollRecognizer {
            guard shouldAllowScrollGesture(gestureRecognizer, touchType: touch.type) else {
                return false
            }
            if nativeSelectionInteractionActive || nativeSelectedRange != nil {
                return false
            }
        }
        return true
    }

    override package func gestureRecognizerShouldBegin(_ gestureRecognizer: UIGestureRecognizer) -> Bool {
        if gestureRecognizer == scrollRecognizer {
            return shouldAllowScrollGesture(gestureRecognizer, touchType: .indirectPointer)
        }

        return true
    }

    package func gestureRecognizer(
        _ gestureRecognizer: UIGestureRecognizer,
        shouldRecognizeSimultaneouslyWith otherGestureRecognizer: UIGestureRecognizer
    ) -> Bool {
        if gestureRecognizer == directTouchTapRecognizer || otherGestureRecognizer == directTouchTapRecognizer {
            return false
        }
        if nativeSelectionInteractionActive || nativeSelectedRange != nil,
           gestureRecognizer == scrollRecognizer || otherGestureRecognizer == scrollRecognizer {
            return false
        }
        if gestureRecognizer == pinchRecognizer || otherGestureRecognizer == pinchRecognizer {
            return false
        }
        // Allow the remaining compatible gestures to recognize simultaneously.
        return true
    }
}

package extension GhosttyTerminalView {
    package var allowsHostTextSelection: Bool {
        TerminalSelectionRoutingPolicy.shouldAllowHostSelection(
            terminalMouseCaptured: isMouseCapturedForHostInteraction,
            selectionInteractionActive: hasActiveSelectionInteraction
        )
    }

    package var hasActiveSelectionInteraction: Bool {
        nativeSelectionInteractionActive
            || nativeSelectedRange != nil
            || prefersNativeSelectionFirstResponder
    }

    package func shouldAllowScrollGesture(
        _ gestureRecognizer: UIGestureRecognizer,
        touchType: UITouch.TouchType
    ) -> Bool {
        TerminalPointerInputRoutingPolicy.shouldAllowScrollGesture(
            isIndirectPointer: touchType == .indirectPointer,
            isPointerButtonPressed: !gestureRecognizer.buttonMask.isEmpty,
            hasActiveTerminalPointerButton: activePointerButton != nil
        )
    }

    package func shouldAllowActiveScrollGesture(_ gestureRecognizer: UIGestureRecognizer) -> Bool {
        TerminalPointerInputRoutingPolicy.shouldAllowScrollGesture(
            isIndirectPointer: !gestureRecognizer.buttonMask.isEmpty || activePointerButton != nil,
            isPointerButtonPressed: !gestureRecognizer.buttonMask.isEmpty,
            hasActiveTerminalPointerButton: activePointerButton != nil
        )
    }
}

package extension GhosttyTerminalView {
    // MARK: - Touch Input

    override package func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        super.touchesBegan(touches, with: event)
        if handleIndirectPointerTouchesBegan(touches, event: event) {
            return
        }
        let location = touches.first?.location(in: self)
        if nativeSelectionLifecycle.interactionIsActive {
            return
        }
        if nativeSelectionLifecycle.keepsFirstResponder {
            if let location, isPointOnNativeSelectionHandleHitArea(location) {
                return
            }
            guard shouldAutoFocusKeyboard(for: touches) else { return }
            notifyDirectTouchOnTerminal()
            return
        }
        // Tap just focuses keyboard - no mouse events (avoids accidental selection).
        notifyDirectTouchOnTerminal()
        requestKeyboardFocus(for: .directTouch)
    }

    override package func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        super.touchesMoved(touches, with: event)
        if handleIndirectPointerTouchesMoved(touches, event: event) {
            return
        }
        // Pan gesture handles scrolling, long press handles selection
    }

    override package func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        super.touchesEnded(touches, with: event)
        _ = handleIndirectPointerTouchesEnded(touches, event: event)
    }

    override package func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
        super.touchesCancelled(touches, with: event)
        _ = handleIndirectPointerTouchesEnded(touches, event: event)
    }

    @objc package func handleDirectTouchTap(_ recognizer: UITapGestureRecognizer) {
        guard recognizer.state == .ended,
              canRouteTerminalInput,
              !isPaused,
              !isShuttingDown else {
            return
        }

        let selectionWasActive = hasActiveSelectionInteraction
        clearNativeSelectionStateForTerminalInput()
        notifyDirectTouchOnTerminal(isFocusTap: true)
        if selectionWasActive {
            _ = requestKeyboardFocus(for: .directTouch)
        }

        guard let surface,
              TerminalPointerInputRoutingPolicy.shouldSendDirectTouchClick(
                  terminalMouseCaptured: isMouseCapturedForHostInteraction,
                  terminalInputAvailable: true,
                  selectionInteractionActive: selectionWasActive
              ) else {
            return
        }

        let position = ghosttyPoint(recognizer.location(in: self))
        stopMomentumScrolling()
        surface.sendMousePos(.init(x: position.x, y: position.y, mods: []))
        surface.sendMouseButton(.init(action: .press, button: .left, mods: []))
        surface.sendMouseButton(.init(action: .release, button: .left, mods: []))
        requestRender()
    }

    package func ghosttyPoint(_ location: CGPoint) -> CGPoint {
        // UIKit coordinates are top-left origin; Ghostty iOS expects the same.
        location
    }

    package func firstIndirectPointerTouch(in touches: Set<UITouch>) -> UITouch? {
        touches.first { $0.type == .indirectPointer }
    }

    package func pointerModifiers(from event: UIEvent?) -> Ghostty.Input.Mods {
        TerminalPointerInputRoutingPolicy.ghosttyModifiers(from: event?.modifierFlags ?? [])
    }

    package func pointerButton(
        from event: UIEvent?
    ) -> TerminalPointerButton? {
        TerminalPointerInputRoutingPolicy.pointerButton(
            for: event?.buttonMask ?? [],
            modifiers: event?.modifierFlags ?? []
        )
    }

    package func sendPointerPosition(_ location: CGPoint, mods: Ghostty.Input.Mods) {
        guard let surface else { return }
        let pos = ghosttyPoint(location)
        surface.sendMousePos(.init(x: pos.x, y: pos.y, mods: mods))
    }

    @discardableResult
    package func handleIndirectPointerTouchesBegan(_ touches: Set<UITouch>, event: UIEvent?) -> Bool {
        guard let touch = firstIndirectPointerTouch(in: touches) else { return false }
        guard canRouteTerminalInput, let surface else { return true }

        focusForHardwareKeyboardIfNeeded()

        let location = touch.location(in: self)
        let mods = pointerModifiers(from: event)
        sendPointerPosition(location, mods: mods)

        guard let button = pointerButton(from: event) else {
            requestRender()
            return true
        }

        activePointerButton = button
        if button == .left {
            stopMomentumScrolling()
            updateSelectionAutoscroll(location: location, mods: mods)
        } else {
            stopSelectionAutoscroll()
        }
        let handled = surface.sendMouseButton(.init(action: .press, button: button.ghosttyMouseButton, mods: mods))
        requestRender()

        if TerminalPointerInputRoutingPolicy.shouldShowHostContextMenu(
            button: button,
            terminalHandledButtonPress: handled,
            terminalMouseCaptured: isMouseCapturedForHostInteraction
        ) {
            showPointerContextMenu(at: location)
        }

        return true
    }

    @discardableResult
    package func handleIndirectPointerTouchesMoved(_ touches: Set<UITouch>, event: UIEvent?) -> Bool {
        guard let touch = firstIndirectPointerTouch(in: touches) else { return false }
        guard canRouteTerminalInput else { return true }

        let location = touch.location(in: self)
        let mods = pointerModifiers(from: event)
        sendPointerPosition(location, mods: mods)
        if activePointerButton == .left {
            updateSelectionAutoscroll(location: location, mods: mods)
        }
        requestRender()
        return true
    }

    @discardableResult
    package func handleIndirectPointerTouchesEnded(_ touches: Set<UITouch>, event: UIEvent?) -> Bool {
        guard let touch = firstIndirectPointerTouch(in: touches) else { return false }
        guard canRouteTerminalInput, let surface else {
            activePointerButton = nil
            stopSelectionAutoscroll()
            return true
        }

        let mods = pointerModifiers(from: event)
        sendPointerPosition(touch.location(in: self), mods: mods)
        if let button = activePointerButton {
            _ = surface.sendMouseButton(.init(action: .release, button: button.ghosttyMouseButton, mods: mods))
        }
        activePointerButton = nil
        stopSelectionAutoscroll()
        requestRender()
        return true
    }

    @objc package func handlePointerHover(_ recognizer: UIHoverGestureRecognizer) {
        guard canRouteTerminalInput, let surface else { return }
        let mods = TerminalPointerInputRoutingPolicy.ghosttyModifiers(from: recognizer.modifierFlags)

        switch recognizer.state {
        case .began, .changed:
            sendPointerPosition(recognizer.location(in: self), mods: mods)
        case .ended, .cancelled, .failed:
            surface.sendMousePos(.init(x: -1, y: -1, mods: mods))
        default:
            return
        }

        requestRender()
    }

    @objc package func handlePanGesture(_ recognizer: UIPanGestureRecognizer) {
        guard let surface = surface else { return }
        guard shouldAllowActiveScrollGesture(recognizer) else { return }
        if isPinchingTerminalZoom { return }

        let translation = recognizer.translation(in: self)
        let location = recognizer.location(in: self)

        switch recognizer.state {
        case .began:
            isScrolling = true
            stopMomentumScrolling()
        case .changed:
            // Update mouse position so TUI apps receive wheel events with coordinates.
            let pos = ghosttyPoint(location)
            surface.sendMousePos(.init(x: pos.x, y: pos.y, mods: []))
            // Send scroll delta directly with increased multiplier for snappy feel
            let scrollEvent = Ghostty.Input.MouseScrollEvent(
                x: Double(translation.x) * Self.scrollMultiplier,
                y: Double(translation.y) * Self.scrollMultiplier,
                mods: Ghostty.Input.ScrollMods(precision: true, momentum: .none)
            )
            surface.sendMouseScroll(scrollEvent)
            requestRender()

            // Reset translation so we get delta on next call
            recognizer.setTranslation(.zero, in: self)
        case .ended:
            isScrolling = false
            // Get velocity for momentum scrolling
            let velocity = recognizer.velocity(in: self)
            startMomentumScrolling(velocity: velocity)
        case .cancelled, .failed:
            isScrolling = false
            stopMomentumScrolling()
        default:
            break
        }
    }

    package func updateSelectionAutoscroll(location: CGPoint, mods: Ghostty.Input.Mods) {
        guard allowsHostTextSelection,
              TerminalSelectionAutoscrollPolicy.decision(
                locationY: Double(location.y),
                viewportHeight: Double(bounds.height),
                edgeInset: Self.selectionAutoscrollEdgeInset,
                maximumScrollDelta: Self.selectionAutoscrollMaximumDelta
              ) != nil else {
            stopSelectionAutoscroll()
            return
        }

        selectionAutoscrollLocation = location
        selectionAutoscrollMods = mods
        startSelectionAutoscrollIfNeeded()
    }

    package func startSelectionAutoscrollIfNeeded() {
        guard selectionAutoscrollDisplayLink == nil else { return }
        let displayLink = CADisplayLink(target: self, selector: #selector(selectionAutoscrollTick))
        displayLink.add(to: .main, forMode: .common)
        selectionAutoscrollDisplayLink = displayLink
    }

    package func stopSelectionAutoscroll() {
        selectionAutoscrollDisplayLink?.invalidate()
        selectionAutoscrollDisplayLink = nil
        selectionAutoscrollLocation = nil
        selectionAutoscrollMods = []
    }

    package func clampedSelectionAutoscrollLocation(_ location: CGPoint) -> CGPoint {
        CGPoint(
            x: min(max(location.x, 0), bounds.width),
            y: min(max(location.y, 0), bounds.height)
        )
    }

    @objc package func selectionAutoscrollTick() {
        guard let surface,
              let location = selectionAutoscrollLocation,
              allowsHostTextSelection else {
            stopSelectionAutoscroll()
            return
        }

        guard let decision = TerminalSelectionAutoscrollPolicy.decision(
            locationY: Double(location.y),
            viewportHeight: Double(bounds.height),
            edgeInset: Self.selectionAutoscrollEdgeInset,
            maximumScrollDelta: Self.selectionAutoscrollMaximumDelta
        ) else {
            stopSelectionAutoscroll()
            return
        }

        let clampedLocation = clampedSelectionAutoscrollLocation(location)
        let pos = ghosttyPoint(clampedLocation)
        let mousePos = Ghostty.Input.MousePosEvent(
            x: pos.x,
            y: pos.y,
            mods: selectionAutoscrollMods
        )
        surface.sendMousePos(mousePos)
        surface.sendMouseScroll(
            Ghostty.Input.MouseScrollEvent(
                x: 0,
                y: decision.scrollDelta,
                mods: Ghostty.Input.ScrollMods(precision: true, momentum: .none)
            )
        )
        surface.sendMousePos(mousePos)
        requestRender()
    }

    package func startMomentumScrolling(velocity: CGPoint) {
        // Only start momentum if velocity is significant
        guard abs(velocity.y) > Self.minimumMomentumVelocity || abs(velocity.x) > Self.minimumMomentumVelocity else {
            sendMomentumEnd()
            return
        }

        // Scale velocity for momentum (divide by 60 for per-frame amount at 60fps)
        momentumVelocity = CGPoint(
            x: velocity.x / 60.0 * Self.scrollMultiplier * 0.5,
            y: velocity.y / 60.0 * Self.scrollMultiplier * 0.5
        )

        // Create display link for smooth animation
        momentumPhase = .began
        momentumDisplayLink = CADisplayLink(target: self, selector: #selector(momentumScrollTick))
        momentumDisplayLink?.add(to: .main, forMode: .common)
    }

    @objc package func momentumScrollTick() {
        guard let surface = surface else {
            stopMomentumScrolling()
            return
        }

        // Apply deceleration
        momentumVelocity.x *= Self.momentumDeceleration
        momentumVelocity.y *= Self.momentumDeceleration

        // Stop if velocity is very low
        if abs(momentumVelocity.x) < 0.5 && abs(momentumVelocity.y) < 0.5 {
            stopMomentumScrolling()
            sendMomentumEnd()
            return
        }

        // Send momentum scroll event (began -> changed)
        let scrollEvent = Ghostty.Input.MouseScrollEvent(
            x: Double(momentumVelocity.x),
            y: Double(momentumVelocity.y),
            mods: Ghostty.Input.ScrollMods(
                precision: true,
                momentum: momentumPhase == .began ? .began : .changed
            )
        )
        surface.sendMouseScroll(scrollEvent)
        momentumPhase = .changed
        requestRender()
    }

    package func stopMomentumScrolling() {
        momentumDisplayLink?.invalidate()
        momentumDisplayLink = nil
        momentumVelocity = .zero
        momentumPhase = .none
    }

    package func sendMomentumEnd() {
        guard let surface = surface else { return }
        let endEvent = Ghostty.Input.MouseScrollEvent(
            x: 0,
            y: 0,
            mods: Ghostty.Input.ScrollMods(precision: true, momentum: .ended)
        )
        surface.sendMouseScroll(endEvent)
        momentumPhase = .none
    }

    @objc package func handlePinchGesture(_ recognizer: UIPinchGestureRecognizer) {
        guard canHandlePinchZoom else {
            isPinchingTerminalZoom = false
            return
        }

        switch recognizer.state {
        case .began:
            isPinchingTerminalZoom = true
            pinchReferenceScale = recognizer.scale
            stopMomentumScrolling()
            showZoomIndicator()
        case .changed:
            guard isPinchingTerminalZoom else { return }
            let relativeScale = recognizer.scale / pinchReferenceScale
            if relativeScale >= CGFloat(TerminalZoomPresentation.pinchZoomInThreshold) {
                if let result = onZoomAction?(.zoomIn) {
                    showZoomIndicator(fontSize: result.effectiveFontSize)
                }
                pinchReferenceScale = recognizer.scale
            } else if relativeScale <= CGFloat(TerminalZoomPresentation.pinchZoomOutThreshold) {
                if let result = onZoomAction?(.zoomOut) {
                    showZoomIndicator(fontSize: result.effectiveFontSize)
                }
                pinchReferenceScale = recognizer.scale
            }
        case .ended, .cancelled, .failed:
            isPinchingTerminalZoom = false
            pinchReferenceScale = 1
            scheduleZoomIndicatorHide(after: TerminalZoomPresentation.indicatorGestureEndHideDelay)
        default:
            break
        }
    }

    package var canHandlePinchZoom: Bool {
        if nativeSelectionLifecycle.shouldRefreshSnapshot {
            return false
        }
        return true
    }
}

#endif
