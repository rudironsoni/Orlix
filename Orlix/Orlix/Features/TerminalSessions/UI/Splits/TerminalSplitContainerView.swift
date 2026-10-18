//
//  TerminalSplitContainerView.swift
//  Orlix
//
//  Split menu commands and focused values for terminal splits
//

import Foundation
import SwiftUI

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package struct ServerViewTabActions {
    package let openNew: () -> Void
    package let closeSelected: () -> Void
    package let selectPrevious: () -> Void
    package let selectNext: () -> Void
    /// Select the tab at a zero-based index (Cmd+1…9). No-op if out of range.
    package let selectIndex: (Int) -> Void
}

// MARK: - Split Actions

/// Actions that can be performed on a terminal split layout
package struct TerminalSplitActions {
    package let perform: (TerminalSplitCommand) -> Void
    package let isEnabled: (TerminalSplitCommand) -> Bool
    package let isZoomed: () -> Bool
}

// MARK: - Focused Values

package struct ActiveServerIdKey: FocusedValueKey {
    package typealias Value = UUID
}

package struct ActivePaneIdKey: FocusedValueKey {
    package typealias Value = UUID
}

package struct TerminalSplitActionsKey: FocusedValueKey {
    package typealias Value = TerminalSplitActions
}

package struct OpenTerminalTabActionKey: FocusedValueKey {
    package typealias Value = () -> Void
}

package struct ServerViewTabActionsKey: FocusedValueKey {
    package typealias Value = ServerViewTabActions
}

package struct OpenLocalSSHDiscoveryActionKey: FocusedValueKey {
    package typealias Value = () -> Void
}

package struct ToggleZenModeActionKey: FocusedValueKey {
    package typealias Value = () -> Void
}

package struct ZenModeEnabledKey: FocusedValueKey {
    package typealias Value = Bool
}

package extension FocusedValues {
    package var activeServerId: UUID? {
        get { self[ActiveServerIdKey.self] }
        set { self[ActiveServerIdKey.self] = newValue }
    }

    package var activePaneId: UUID? {
        get { self[ActivePaneIdKey.self] }
        set { self[ActivePaneIdKey.self] = newValue }
    }

    package var terminalSplitActions: TerminalSplitActions? {
        get { self[TerminalSplitActionsKey.self] }
        set { self[TerminalSplitActionsKey.self] = newValue }
    }

    package var openTerminalTab: (() -> Void)? {
        get { self[OpenTerminalTabActionKey.self] }
        set { self[OpenTerminalTabActionKey.self] = newValue }
    }

    package var serverViewTabActions: ServerViewTabActions? {
        get { self[ServerViewTabActionsKey.self] }
        set { self[ServerViewTabActionsKey.self] = newValue }
    }

    package var openLocalSSHDiscovery: (() -> Void)? {
        get { self[OpenLocalSSHDiscoveryActionKey.self] }
        set { self[OpenLocalSSHDiscoveryActionKey.self] = newValue }
    }

    package var toggleZenMode: (() -> Void)? {
        get { self[ToggleZenModeActionKey.self] }
        set { self[ToggleZenModeActionKey.self] = newValue }
    }

    package var isZenModeEnabled: Bool? {
        get { self[ZenModeEnabledKey.self] }
        set { self[ZenModeEnabledKey.self] = newValue }
    }
}

#if os(macOS)
import AppKit

// MARK: - Split Menu Commands

package struct SplitCommands: Commands {
    @FocusedValue(\.terminalSplitActions) package var splitActions
    @FocusedValue(\.toggleZenMode) package var toggleZenMode
    @FocusedValue(\.isZenModeEnabled) package var isZenModeEnabled

    package var body: some Commands {
        CommandMenu("Terminal") {
            Button(isZenModeEnabled == true ? String(localized: "Exit Zen Mode") : String(localized: "Enter Zen Mode")) {
                toggleZenMode?()
            }
            .keyboardShortcut("z", modifiers: [.command, .control])
            .disabled(toggleZenMode == nil)

            Divider()

            Group {
                Button("Split Right") {
                    perform(.splitRight)
                }
                .keyboardShortcut("d", modifiers: [.command])
                .disabled(!isEnabled(.splitRight))

                Button("Split Down") {
                    perform(.splitDown)
                }
                .keyboardShortcut("d", modifiers: [.command, .shift])
                .disabled(!isEnabled(.splitDown))

                Divider()

                Button("Close Pane") {
                    perform(.closeFocusedPane)
                }
                .keyboardShortcut("w", modifiers: [.command, .shift])
                .disabled(!isEnabled(.closeFocusedPane))
            }
        }

        CommandGroup(after: .windowArrangement) {
            Divider()

            Button(splitActions?.isZoomed() == true ? "Unzoom Split" : "Zoom Split") {
                perform(.toggleZoom)
            }
            .keyboardShortcut(.return, modifiers: [.command, .shift])
            .disabled(!isEnabled(.toggleZoom))

            Button("Select Previous Split") {
                perform(.selectPrevious)
            }
            .keyboardShortcut("[", modifiers: [.command])
            .disabled(!isEnabled(.selectPrevious))

            Button("Select Next Split") {
                perform(.selectNext)
            }
            .keyboardShortcut("]", modifiers: [.command])
            .disabled(!isEnabled(.selectNext))

            Menu("Select Split") {
                Button("Select Split Above") { perform(.selectAbove) }
                    .keyboardShortcut(.upArrow, modifiers: [.command, .option])
                    .disabled(!isEnabled(.selectAbove))
                Button("Select Split Below") { perform(.selectBelow) }
                    .keyboardShortcut(.downArrow, modifiers: [.command, .option])
                    .disabled(!isEnabled(.selectBelow))
                Button("Select Split Left") { perform(.selectLeft) }
                    .keyboardShortcut(.leftArrow, modifiers: [.command, .option])
                    .disabled(!isEnabled(.selectLeft))
                Button("Select Split Right") { perform(.selectRight) }
                    .keyboardShortcut(.rightArrow, modifiers: [.command, .option])
                    .disabled(!isEnabled(.selectRight))
            }
            .disabled(splitActions == nil)

            Menu("Resize Split") {
                Button("Equalize Splits") { perform(.equalize) }
                    .keyboardShortcut("=", modifiers: [.command, .control])
                    .disabled(!isEnabled(.equalize))

                Divider()

                Button("Move Divider Up") { perform(.moveDividerUp) }
                    .keyboardShortcut(.upArrow, modifiers: [.command, .control])
                    .disabled(!isEnabled(.moveDividerUp))
                Button("Move Divider Down") { perform(.moveDividerDown) }
                    .keyboardShortcut(.downArrow, modifiers: [.command, .control])
                    .disabled(!isEnabled(.moveDividerDown))
                Button("Move Divider Left") { perform(.moveDividerLeft) }
                    .keyboardShortcut(.leftArrow, modifiers: [.command, .control])
                    .disabled(!isEnabled(.moveDividerLeft))
                Button("Move Divider Right") { perform(.moveDividerRight) }
                    .keyboardShortcut(.rightArrow, modifiers: [.command, .control])
                    .disabled(!isEnabled(.moveDividerRight))
            }
            .disabled(splitActions == nil)
        }
    }

    private func perform(_ command: TerminalSplitCommand) {
        splitActions?.perform(command)
    }

    private func isEnabled(_ command: TerminalSplitCommand) -> Bool {
        splitActions?.isEnabled(command) == true
    }


    package init() {}
}

#endif
