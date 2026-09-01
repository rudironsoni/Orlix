#if os(iOS)
import SwiftUI

extension TrustedHostsSettingsView {
    func platformHostRow(for knownHost: KnownHostSettingsItem) -> some View {
        TrustedHostSettingsRow(knownHost: knownHost)
            .orlixListRowSeparatorLeadingZero()
            .swipeActions(edge: .trailing, allowsFullSwipe: false) {
                resetAction(for: knownHost)
            }
    }
}
#endif
