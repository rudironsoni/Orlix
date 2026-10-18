import SwiftUI

package struct TerminalKeyboardInputSettingsView: View {
    package var body: some View {
        TerminalKeyboardInputPlatformSettingsView()
            .accessibilityIdentifier("orlix.settings.page.keyboardAndInput")
    }
}
