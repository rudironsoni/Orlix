//
//  GhosttyTerminalCustomIO+macOS.swift
//  Orlix
//
//  macOS custom terminal data and input forwarding.
//

#if os(macOS)
import Foundation

extension GhosttyTerminalView {
    /// Feed data from SSH channel to the terminal for rendering
    func feedData(_ data: Data) {
        guard let surface = surface?.unsafeCValue else { return }

        // Feed data immediately - SSH read loop already batches appropriately
        GhosttyHostIO.feed(surface, data)

        // Request render via display link (event-driven, will auto-stop when idle)
        requestRender()
    }

    /// Setup the write callback to capture keyboard input
    /// Call this after the surface is created to start receiving input
    func setupWriteCallback() {
        guard let surface = surface?.unsafeCValue else { return }
        guard let userdata = ghostty_surface_userdata(surface) else { return }

        GhosttyHostIO.setWriteCallback(
            surface,
            ghosttyTerminalWriteCallback,
            userdata: userdata
        )
    }

    /// Send text to the terminal (used by voice input)
    func sendText(_ text: String) {
        surface?.sendText(text)
        requestRender()
    }

    func pasteTextFromClipboard() {
        _ = surface?.perform(action: "paste_from_clipboard")
        requestRender()
    }

    /// Send a special key to the terminal
    func sendSpecialKey(_ key: TerminalSpecialKey) {
        guard let surface = surface else { return }
        let escapeSequence = TerminalSpecialKeySequence.escapeSequence(for: key)
        surface.sendText(escapeSequence)
        requestRender()
    }

    /// Send a control key combination (Ctrl+C, Ctrl+D, etc.)
    func sendControlKey(_ char: Character) {
        guard let surface = surface else { return }
        if let controlChar = TerminalControlKey.controlCharacter(for: char) {
            surface.sendText(String(controlChar))
            requestRender()
        }
    }
}

#endif
