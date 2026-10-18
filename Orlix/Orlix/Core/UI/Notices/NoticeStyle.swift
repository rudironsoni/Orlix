import SwiftUI

package enum NoticeSurfaceStyle {
    case standard
    case terminal(backgroundColor: Color, foregroundColor: Color)

    package var primaryForegroundColor: Color {
        switch self {
        case .standard:
            return .primary
        case .terminal(_, let foregroundColor):
            return foregroundColor
        }
    }

    package var secondaryForegroundColor: Color {
        switch self {
        case .standard:
            return .secondary
        case .terminal:
            return primaryForegroundColor.opacity(0.68)
        }
    }
}

package struct NoticeMetrics {
    static package let cornerRadius: CGFloat = 16
    static package let bannerMaxWidth: CGFloat = 620
    static package let operationMaxWidth: CGFloat = 560
    static package let blockingMaxWidth: CGFloat = 340
    static package let notificationCornerRadius: CGFloat = 30
}

package extension NoticeLevel {
    package var tintColor: Color {
        switch self {
        case .info:
            return .accentColor
        case .success:
            return .green
        case .warning:
            return .orange
        case .error:
            return .red
        }
    }

    package var defaultIconSystemName: String {
        switch self {
        case .info:
            return "info.circle.fill"
        case .success:
            return "checkmark.circle.fill"
        case .warning:
            return "exclamationmark.triangle.fill"
        case .error:
            return "xmark.octagon.fill"
        }
    }
}
