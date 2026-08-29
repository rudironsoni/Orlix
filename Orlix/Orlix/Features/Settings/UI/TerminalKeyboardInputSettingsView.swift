import SwiftUI

struct TerminalKeyboardInputSettingsView: View {
    var body: some View {
        TerminalKeyboardInputPlatformSettingsView()
            .accessibilityIdentifier("orlix.settings.page.keyboardAndInput")
    }
}
