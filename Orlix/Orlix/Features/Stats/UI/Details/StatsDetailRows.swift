import SwiftUI

package struct ProcessSheetMetric: View {
    package let title: String
    package let value: Double
    package let color: Color

    package var body: some View {
        VStack(alignment: .trailing, spacing: 5) {
            HStack(alignment: .firstTextBaseline, spacing: 3) {
                Text(formatPercent(value))
                    .font(.caption.weight(.bold))
                    .foregroundStyle(.primary)
                    .monospacedDigit()
                    .lineLimit(1)
                    .minimumScaleFactor(0.68)

                Text(title)
                    .font(.caption2.weight(.bold))
                    .foregroundStyle(.secondary)
            }

            ProgressView(value: min(max(value / 100, 0), 1))
                .tint(color)
                .frame(width: 58)
        }
        .frame(width: 66, alignment: .trailing)
    }
}

package struct InfoRow: View {
    package let title: String
    package let value: String

    package var body: some View {
        if !value.isEmpty {
            HStack(alignment: .firstTextBaseline) {
                Text(title)
                    .foregroundStyle(.secondary)
                Spacer(minLength: 16)
                Text(value)
                    .multilineTextAlignment(.trailing)
                    .monospacedDigit()
            }
        }
    }
}
