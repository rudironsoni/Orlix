#if os(macOS)
import SwiftUI

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package struct TerminalConnectionStatusView: View {
    package let presentation: TerminalConnectionStatusPresentation
    package let connectionAttemptID: UUID
    package let surfaceStyle: NoticeSurfaceStyle
    package let isActive: Bool
    package let onRetry: () -> Void
    package let onTrustNewHostKey: () -> Void

    @ViewBuilder
    package var body: some View {
        switch presentation {
        case .hidden:
            EmptyView()
        case .connecting(let serverName):
            progressCard(
                message: String(
                    format: String(localized: "Connecting to %@..."),
                    serverName
                ),
                tint: .secondary
            )
        case .disconnected(let message):
            BlockingStatusView(surfaceStyle: surfaceStyle) {
                VStack(spacing: 16) {
                    Image(systemName: "bolt.slash")
                        .font(.largeTitle)
                        .foregroundStyle(.secondary)
                    Text("Disconnected")
                        .foregroundStyle(.secondary)
                    if let message {
                        Text(message)
                            .font(.caption)
                            .foregroundStyle(.secondary)
                            .multilineTextAlignment(.center)
                    }
                    Button("Reconnect", action: onRetry)
                        .noticeSecondaryButtonStyle()
                }
                .multilineTextAlignment(.center)
            }
        case .failed(let message, let allowsHostKeyReplacement):
            BlockingStatusView(surfaceStyle: surfaceStyle) {
                VStack(spacing: 16) {
                    Image(systemName: "exclamationmark.triangle")
                        .font(.largeTitle)
                        .foregroundStyle(.red)
                    Text("Connection Failed")
                        .font(.headline)
                    Text(message)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                    if allowsHostKeyReplacement {
                        Button("Trust New Host Key", action: onTrustNewHostKey)
                            .noticePrimaryButtonStyle()
                    }
                    Button("Retry", action: onRetry)
                        .noticeSecondaryButtonStyle()
                }
                .multilineTextAlignment(.center)
            }
        }
    }

    private func progressCard(message: String, tint: Color) -> some View {
        BlockingStatusView(showsScrim: false, surfaceStyle: surfaceStyle) {
            VStack(spacing: 12) {
                ProgressView()
                    .progressViewStyle(.circular)
                Text(message)
                    .foregroundStyle(tint)
            }
            .padding(.vertical, 6)
            .multilineTextAlignment(.center)
        }
    }
}
#endif
