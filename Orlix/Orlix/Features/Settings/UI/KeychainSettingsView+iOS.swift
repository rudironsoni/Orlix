#if os(iOS)
import SwiftUI

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package extension KeychainSettingsView {
    @ViewBuilder
    package func platformKeyRow(for key: SSHKeyEntry) -> some View {
        HStack(spacing: 8) {
            Button {
                keyToShowDetails = key
            } label: {
                SSHKeyRow(key: key)
            }
            .buttonStyle(.plain)
        }
        .swipeActions(edge: .trailing, allowsFullSwipe: false) {
            keyActions(for: key)
        }
    }
}
#endif
