#if os(macOS)
import SwiftUI

package extension TrustedHostsSettingsView {
    package func platformHostRow(for knownHost: KnownHostSettingsItem) -> some View {
        TrustedHostSettingsRow(knownHost: knownHost)
            .contextMenu {
                resetAction(for: knownHost)
            }
    }
}
#endif
