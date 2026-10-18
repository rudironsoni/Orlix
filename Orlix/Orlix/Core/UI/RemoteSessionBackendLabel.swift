import SwiftUI

package struct RemoteSessionBackendLabel: View {
    package let backend: RemoteSessionBackendMetadata

    package var body: some View {
        Label {
            Text(backend.displayName)
        } icon: {
            icon
                .frame(width: 20, height: 20)
                .clipped()
                .padding(.trailing, 6)
        }
    }

    @ViewBuilder
    private var icon: some View {
        if let assetName {
            Image(decorative: assetName)
                .resizable()
                .scaledToFit()
        } else {
            Image(systemName: "terminal")
                .accessibilityHidden(true)
        }
    }

    private var assetName: String? {
        switch backend.identifier.rawValue {
        case "herdr", "tmux", "zellij", "zmx":
            "RemoteSessionBackend-\(backend.identifier.rawValue)"
        default:
            nil
        }
    }


    package init(backend: RemoteSessionBackendMetadata) {

        self.backend = backend

    }
}
