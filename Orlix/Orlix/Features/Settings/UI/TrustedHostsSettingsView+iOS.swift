#if os(iOS)
import SwiftUI

package extension TrustedHostsSettingsView {
    package func platformHostRow(for knownHost: KnownHostSettingsItem) -> some View {
        TrustedHostSettingsRow(knownHost: knownHost)
            .orlixListRowSeparatorLeadingZero()
            .swipeActions(edge: .trailing, allowsFullSwipe: false) {
                resetAction(for: knownHost)
            }
    }
}
#endif
