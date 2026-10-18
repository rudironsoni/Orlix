#if os(macOS)
import SwiftUI

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package extension CustomFontLibraryView {
    package var platformBody: some View {
        VStack(spacing: 0) {
            DialogSheetHeader(title: "Custom Fonts", onClose: onClose)
            Divider()

            if fontStore.customFonts.isEmpty {
                emptyState
            } else {
                List(fontStore.customFonts) { font in
                    HStack(spacing: 10) {
                        FontRow(font: font, isAvailable: fontStore.isAvailable(font))

                        Button(role: .destructive) {
                            presentation = .deleteConfirmation(font.id)
                        } label: {
                            Image(systemName: "trash")
                        }
                        .buttonStyle(.borderless)
                        .help("Delete")
                    }
                    .contextMenu {
                        deleteButton(for: font)
                    }
                }
                libraryNote
                    .padding(.horizontal, 20)
                    .padding(.bottom, 10)
            }

            Divider()
            Button(action: requestImport) {
                Label("Import Font", systemImage: "plus.circle.fill")
                    .frame(maxWidth: .infinity, alignment: .leading)
            }
            .buttonStyle(.plain)
            .accessibilityIdentifier("orlix.settings.customFonts.import")
            .padding(.horizontal, 20)
            .padding(.vertical, 12)
        }
        .frame(width: 440, height: 500)
    }
}
#endif
