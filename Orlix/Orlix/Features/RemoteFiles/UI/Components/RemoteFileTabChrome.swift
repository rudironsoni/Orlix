import SwiftUI

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package struct RemoteFileTabsEmptyState: View {
    package let server: Server?
    package let onNewTab: () -> Void

    package init(server: Server? = nil, onNewTab: @escaping () -> Void) {
        self.server = server
        self.onNewTab = onNewTab
    }

    package var body: some View {
        VStack(spacing: 24) {
            Spacer()

            VStack(spacing: 16) {
                Image(systemName: "folder")
                    .font(.system(size: 56))
                    .foregroundStyle(.secondary)

                VStack(spacing: 8) {
                    Text(server?.name ?? String(localized: "Files"))
                        .font(.title2)
                        .fontWeight(.semibold)

                    Text("No file tabs open")
                        .font(.body)
                        .foregroundStyle(.secondary)
                }
            }

            Button(action: onNewTab) {
                HStack(spacing: 8) {
                    Image(systemName: "plus")
                    Text("New File Tab")
                        .fontWeight(.medium)
                }
                .padding(.horizontal, 24)
                .padding(.vertical, 12)
                .background(.tint, in: RoundedRectangle(cornerRadius: 10))
                .foregroundStyle(.white)
            }
            .buttonStyle(.plain)

            Spacer()
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}
