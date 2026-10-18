#if os(iOS)
import SwiftUI

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package struct TerminalSessionPlatformSettingsSection: View {
    package var body: some View {
        Section("Terminal Session") {
            TerminalScreenAwakeSettingRow()
        }
    }
}

package struct TerminalScreenAwakeSettingRow: View {
    @AppStorage(TerminalDefaults.keepScreenAwakeKey)
    private var keepScreenAwake = TerminalDefaults.defaultKeepScreenAwake

    package var body: some View {
        Toggle("Keep screen awake", isOn: $keepScreenAwake)
            .accessibilityIdentifier("orlix.settings.terminal.keepScreenAwake")
    }
}
#endif
