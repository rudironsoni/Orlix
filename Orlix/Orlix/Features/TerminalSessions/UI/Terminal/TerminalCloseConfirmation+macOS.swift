#if os(macOS)
import SwiftUI

package extension View {
    package func terminalCloseConfirmationAlert(
        isPresented: Binding<Bool>,
        message: String,
        onCancel: @escaping () -> Void = {},
        onClose: @escaping () -> Void
    ) -> some View {
        alert("Close this terminal?", isPresented: isPresented) {
            Button("Cancel", role: .cancel, action: onCancel)
                .keyboardShortcut(.cancelAction)
            Button("Close", role: .destructive, action: onClose)
                .keyboardShortcut(.defaultAction)
        } message: {
            Text(message)
        }
    }
}
#endif
