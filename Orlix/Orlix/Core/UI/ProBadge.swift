import SwiftUI

package struct ProBadge: View {
    package var compact: Bool = false

    package init(compact: Bool = false) {
        self.compact = compact
    }

    package var body: some View {
        HStack(spacing: 4) {
            Image(systemName: "star.fill")
                .font(compact ? .caption2 : .caption)
            if !compact {
                Text("PRO")
                    .font(.caption2)
                    .fontWeight(.bold)
            }
        }
        .foregroundStyle(.white)
        .padding(.horizontal, compact ? 4 : 6)
        .padding(.vertical, 2)
        .background(.orange, in: Capsule())
    }
}

package struct LockedBadge: View {
    package init() {}

    package var body: some View {
        HStack(spacing: 4) {
            Image(systemName: "lock.fill")
                .font(.caption2)
        }
        .foregroundStyle(.white)
        .padding(.horizontal, 6)
        .padding(.vertical, 3)
        .background(.secondary.opacity(0.8), in: Capsule())
    }
}
