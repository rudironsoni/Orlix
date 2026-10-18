import SwiftUI

enum NoticeLane {
    case topBanner
    case bottomOperation
}

package enum NoticeLevel {
    package case info
    package case success
    package case warning
    package case error
}

package enum NoticeLeading {
    package case none
    package case icon(String)
    package case activity
}

enum NoticeLifetime {
    case persistent
    case autoDismiss(Duration)
}

package struct NoticeProgress: Equatable {
    package var completedUnitCount: Int?
    package var totalUnitCount: Int?

    package var isDeterminate: Bool {
        guard let totalUnitCount, totalUnitCount > 0 else { return false }
        return completedUnitCount != nil
    }
}

package struct NoticeAction: Identifiable {
    package let id: String
    package let title: String
    package let role: ButtonRole?
    package let handler: () -> Void

    package init(
        id: String,
        title: String,
        role: ButtonRole? = nil,
        handler: @escaping () -> Void
    ) {
        self.id = id
        self.title = title
        self.role = role
        self.handler = handler
    }
}

package struct NoticeItem: Identifiable {
    package let id: String
    package let lane: NoticeLane
    package let level: NoticeLevel
    package let leading: NoticeLeading
    package let title: String?
    package let message: String
    package let detail: String?
    package let progress: NoticeProgress?
    package let lifetime: NoticeLifetime
    package let action: NoticeAction?
    package let dismissAction: (() -> Void)?

    package init(
        id: String,
        lane: NoticeLane,
        level: NoticeLevel,
        leading: NoticeLeading = .none,
        title: String? = nil,
        message: String,
        detail: String? = nil,
        progress: NoticeProgress? = nil,
        lifetime: NoticeLifetime = .persistent,
        action: NoticeAction? = nil,
        dismissAction: (() -> Void)? = nil
    ) {
        self.id = id
        self.lane = lane
        self.level = level
        self.leading = leading
        self.title = title
        self.message = message
        self.detail = detail
        self.progress = progress
        self.lifetime = lifetime
        self.action = action
        self.dismissAction = dismissAction
    }
}
