#if os(macOS)
import AppKit
import SwiftUI

package extension SupportSheet {
    package func openURL(_ urlString: String) {
        guard let url = URL(string: urlString) else { return }
        NSWorkspace.shared.open(url)
    }
}
#endif
