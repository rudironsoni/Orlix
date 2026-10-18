import SwiftUI

#if ORLIX_MODULAR_APP
import OrlixModels
#endif

package struct MultiConnectionUpgradeEmptyState: View {
    package let server: Server
    @State private var showingTabLimitAlert = false

    package var body: some View {
        VStack(spacing: 24) {
            Spacer()

            VStack(spacing: 16) {
                Image(systemName: "star.fill")
                    .font(.system(size: 56))
                    .foregroundStyle(.orange)

                VStack(spacing: 8) {
                    Text(server.name)
                        .font(.title2)
                        .fontWeight(.semibold)

                    Text("Multiple connections require Pro")
                        .font(.body)
                        .foregroundStyle(.secondary)

                    Text(String(
                        format: String(localized: "Free plan allows %lld active connection. Disconnect another server or upgrade."),
                        Int64(FreeTierLimits.maxTabs)
                    ))
                        .font(.caption)
                        .foregroundStyle(.tertiary)
                        .multilineTextAlignment(.center)
                        .padding(.horizontal, 32)
                }
            }

            Button {
                showingTabLimitAlert = true
            } label: {
                HStack(spacing: 8) {
                    Image(systemName: "star.fill")
                    Text("Upgrade to Pro")
                        .fontWeight(.medium)
                }
                .padding(.horizontal, 24)
                .padding(.vertical, 12)
                .background(.orange, in: RoundedRectangle(cornerRadius: 10))
                .foregroundStyle(.white)
            }
            .buttonStyle(.plain)

            Spacer()
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .limitReachedAlert(.tabs, isPresented: $showingTabLimitAlert)
    }
}
