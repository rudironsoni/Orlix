//
//  TerminalTab.swift
//  Orlix
//
//  A tab containing one or more terminal panes (via splits).
//  Each tab is independent - splits happen within a tab, not across tabs.
//

import Foundation
#if ORLIX_MODULAR_APP
import OrlixModels
import OrlixSSH
#endif

// MARK: - Terminal Tab

/// Represents a single tab in the terminal toolbar.
/// Each tab can contain multiple panes via splits.
package struct TerminalTab: Identifiable, Equatable, Codable {
    package let id: UUID
    package let serverId: UUID
    package var title: String
    package var createdAt: Date

    /// The split layout for this tab. Nil means single pane (the root pane).
    package var layout: TerminalSplitNode?

    /// The currently focused pane ID within this tab
    package var focusedPaneId: UUID

    /// Root pane ID - the original pane created with this tab
    package let rootPaneId: UUID

    package init(
        id: UUID = UUID(),
        serverId: UUID,
        title: String,
        createdAt: Date = Date(),
        rootPaneId: UUID = UUID(),
        focusedPaneId: UUID? = nil,
        layout: TerminalSplitNode? = nil
    ) {
        self.id = id
        self.serverId = serverId
        self.title = title
        self.createdAt = createdAt
        self.rootPaneId = rootPaneId
        self.focusedPaneId = focusedPaneId ?? rootPaneId
        self.layout = layout
    }

    /// All pane IDs in this tab (from layout or just root)
    package var allPaneIds: [UUID] {
        layout?.allPaneIds() ?? [rootPaneId]
    }

    /// Number of panes in this tab
    package var paneCount: Int {
        layout?.leafCount ?? 1
    }

    /// Whether this tab has splits
    package var hasSplits: Bool {
        layout != nil && (layout?.leafCount ?? 1) > 1
    }
}

// MARK: - Terminal Pane State

/// State for a single terminal pane (leaf in split tree)
package struct TerminalPaneState: Equatable {
    package let paneId: UUID
    package let tabId: UUID
    package let serverId: UUID
    package var connectionState: ConnectionState
    package var disconnectReason: TerminalDisconnectReason?
    private(set) var hasEstablishedConnection: Bool
    package var lastActivity: Date
    package var remoteSessionStatus: RemoteSessionStatus
    package var workingDirectory: String?
    package var presentationOverrides: TerminalPresentationOverrides
    package var seedPaneId: UUID?
    /// Runtime transport and any valid Mosh fallback data for this pane (never persisted).
    package var transportState: ShellTransportState
    /// Minimal non-secret context needed to recognize remote-session lifecycle markers
    /// when an existing Mosh or ET session resumes after process relaunch.
    package var remoteSessionResumeContext: RemoteSessionLifecycleContext?
    /// True after a startup action may have been dispatched. Automatic recovery
    /// must not dispatch it again until normal completion clears this guard.
    package var startupActionReplayPending: Bool

    package init(paneId: UUID, tabId: UUID, serverId: UUID) {
        self.paneId = paneId
        self.tabId = tabId
        self.serverId = serverId
        self.connectionState = .connecting
        self.disconnectReason = nil
        self.hasEstablishedConnection = false
        self.lastActivity = Date()
        self.remoteSessionStatus = .unknown
        self.workingDirectory = nil
        self.presentationOverrides = .empty
        self.seedPaneId = nil
        self.transportState = .ssh
        self.remoteSessionResumeContext = nil
        self.startupActionReplayPending = false
    }

    mutating package func markConnectionEstablished() {
        hasEstablishedConnection = true
    }

    package var activeTransport: ShellTransport { transportState.transport }
    package var moshFallbackReason: MoshFallbackReason? { transportState.fallbackReason }
    package var moshFallbackDiagnostics: MoshFallbackDiagnostics? { transportState.fallbackDiagnostics }
}
