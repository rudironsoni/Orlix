---
type: source
tags:
  - application
  - terminal
  - mouse
updated: 2026-08-29
status: current
summary: "Pinned RootShell mouse mode implementation reference."
sources:
  - "https://github.com/kitknox/rootshell/tree/908d725ee5057a6c49668528f1d1e7285a8fcc44"
  - "https://ghostty.org/docs/config/reference#toggle_mouse_reporting"
---

# RootShell mouse mode reference

## Source identity

- Repository: `https://github.com/kitknox/rootshell`
- Commit: `908d725ee5057a6c49668528f1d1e7285a8fcc44`
- Inspected on: 2026-08-29

## Behavior

RootShell calls `ghostty_surface_binding_action` with
`toggle_mouse_reporting`. Ghostty then suppresses terminal-program mouse
reporting for that surface. Host selection, scrolling, and other UI interaction
can use the pointer while the override is active.

RootShell keeps the override in terminal-view state. It exposes the action in
its customizable keyboard toolbar, gives the active button a selected style,
shows `Mouse Capture Off` or `Mouse Capture On`, and routes a keyboard shortcut
to the same toggle.

The inspected implementation is in:

- `rootshell/UI/Terminal/TerminalView.swift`
- `rootshell/UI/Keyboard/KeyboardToolbarView.swift`
- `rootshell/Core/Keybinds/KeybindManager.swift`

RootShell uses `Cmd+Shift+M`. Orlix uses `Cmd+Option+M` because
`Cmd+Shift+M` already owns voice input.

## Orlix adoption boundary

Orlix uses the same Ghostty surface action. Orlix keeps the desired override in
`TerminalPresentationStateStore`, keyed by pane identifier, so surface view
reconstruction does not lose it and split panes do not share it. The state is
process-local and is removed when the pane closes.

The Orlix accessory action is customizable. Untouched version 2 default
layouts gain the action during normalization. Customized layouts are not
changed.

RootShell does not provide focused terminal mouse-mode unit, integration, or UI
tests at the pinned commit. Orlix therefore owns tests for state isolation,
default migration, the actual Ghostty custom-I/O surface action, toolbar and
shortcut routing, capture disable and restore, native selection, and mouse
report resumption.
