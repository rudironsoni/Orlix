//
//  Ghostty+Action.swift
//  Orlix
//
//  Action types for Ghostty terminal events
//

import Foundation

// MARK: - Ghostty.Action

package extension Ghostty {
    package enum Action {}
}

// MARK: - Scrollbar

package extension Ghostty.Action {
    /// Represents the scrollbar state from the terminal core.
    ///
    /// ## Fields
    /// - `total`: Total rows in scrollback + active area
    /// - `offset`: First visible row (0 = top of history)
    /// - `len`: Number of visible rows (viewport height)
    nonisolated package struct Scrollbar: Sendable {
        package let total: UInt64
        package let offset: UInt64
        package let len: UInt64

        package init(c: ghostty_action_scrollbar_s) {
            self.init(total: c.total, offset: c.offset, len: c.len)
        }

        package init(total: UInt64, offset: UInt64, len: UInt64) {
            self.total = total
            self.len = min(len, total)
            self.offset = min(offset, total - self.len)
        }

        package var rowsBelowViewport: UInt64 {
            total - offset - len
        }

        package var offsetAsInt: Int {
            Int(clamping: offset)
        }

        static package func clampedRowIndex(_ value: Double) -> Int? {
            guard value.isFinite else { return nil }
            let row = floor(value)
            guard row > 0 else { return 0 }
            return Int(exactly: row) ?? Int.max
        }
    }
}

// MARK: - Notification Names

nonisolated package extension Notification.Name {
    /// Posted when the terminal scrollbar state changes.
    /// userInfo contains ScrollbarKey with Ghostty.Action.Scrollbar value.
    static package let ghosttyDidUpdateScrollbar = Notification.Name("com.rudironsoni.orlix.ghostty.didUpdateScrollbar")

    /// Key for scrollbar state in notification userInfo
    static package let ScrollbarKey = ghosttyDidUpdateScrollbar.rawValue + ".scrollbar"
}
