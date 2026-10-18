#if os(iOS)
import SwiftUI

package struct TerminalKeyboardSafeAreaModifier: ViewModifier {
    package let isEnabled: Bool

    @ViewBuilder
    package func body(content: Content) -> some View {
        if isEnabled {
            content.ignoresSafeArea(.keyboard, edges: .bottom)
        } else {
            content
        }
    }
}
#endif
