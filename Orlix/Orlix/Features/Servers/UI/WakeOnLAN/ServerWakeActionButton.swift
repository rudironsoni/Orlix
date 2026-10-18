import SwiftUI

package struct ServerWakeActionButton: View {
    package let serverID: UUID
    package let onAction: () -> Void

    package var body: some View {
        Button(action: onAction) {
            Label("Wake Server", systemImage: "power")
        }
        .accessibilityIdentifier(
            "orlix.serverList.wake.\(serverID.uuidString)"
        )
    }
}
