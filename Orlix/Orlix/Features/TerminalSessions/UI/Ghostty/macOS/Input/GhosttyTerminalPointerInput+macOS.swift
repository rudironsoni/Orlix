//
//  GhosttyTerminalPointerInput+macOS.swift
//  Orlix
//
//  macOS terminal pointer and scroll input routing.
//

#if os(macOS)
import AppKit

package extension GhosttyTerminalView {
    override package func mouseDown(with event: NSEvent) {
        inputHandler.handleMouseDown(with: event)
    }

    override package func mouseUp(with event: NSEvent) {
        inputHandler.handleMouseUp(with: event)
    }

    override package func rightMouseDown(with event: NSEvent) {
        if inputHandler.handleRightMouseDown(with: event) {
            return
        }
        super.rightMouseDown(with: event)
    }

    override package func rightMouseUp(with event: NSEvent) {
        if inputHandler.handleRightMouseUp(with: event) {
            return
        }
        super.rightMouseUp(with: event)
    }

    override package func otherMouseDown(with event: NSEvent) {
        inputHandler.handleOtherMouseDown(with: event)
    }

    override package func otherMouseUp(with event: NSEvent) {
        inputHandler.handleOtherMouseUp(with: event)
    }

    override package func mouseMoved(with event: NSEvent) {
        inputHandler.handleMouseMoved(with: event, viewFrame: frame) { [weak self] point, view in
            self?.convert(point, from: view) ?? .zero
        }
    }

    override package func mouseDragged(with event: NSEvent) {
        mouseMoved(with: event)
    }

    override package func rightMouseDragged(with event: NSEvent) {
        mouseMoved(with: event)
    }

    override package func otherMouseDragged(with event: NSEvent) {
        mouseMoved(with: event)
    }

    override package func mouseEntered(with event: NSEvent) {
        super.mouseEntered(with: event)
        inputHandler.handleMouseEntered(with: event, viewFrame: frame) { [weak self] point, view in
            self?.convert(point, from: view) ?? .zero
        }
    }

    override package func mouseExited(with event: NSEvent) {
        inputHandler.handleMouseExited(with: event)
    }

    override package func scrollWheel(with event: NSEvent) {
        inputHandler.handleScrollWheel(with: event)
    }
}

#endif
