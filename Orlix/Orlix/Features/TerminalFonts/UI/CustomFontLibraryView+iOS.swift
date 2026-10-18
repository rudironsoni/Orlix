#if os(iOS)
import SwiftUI

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package extension CustomFontLibraryView {
    package var platformBody: some View {
        Group {
            if fontStore.customFonts.isEmpty {
                emptyState
            } else {
                List {
                    Section {
                        ForEach(fontStore.customFonts) { font in
                            fontRow(font)
                        }
                    } footer: {
                        libraryNote
                    }
                }
            }
        }
        .navigationTitle("Custom Fonts")
        .navigationBarTitleDisplayMode(.inline)
        .toolbar {
            ToolbarItem(placement: .primaryAction) {
                Button(action: requestImport) {
                    Image(systemName: "plus")
                }
                .accessibilityLabel("Import Font")
                .accessibilityIdentifier("orlix.settings.customFonts.import")
            }
        }
        .accessibilityIdentifier("orlix.settings.customFonts.page")
    }

    private func fontRow(_ font: TerminalFont) -> some View {
        FontRow(font: font, isAvailable: fontStore.isAvailable(font))
            .swipeActions(edge: .trailing, allowsFullSwipe: false) {
                Button("Delete", role: .destructive) {
                    presentation = .deleteConfirmation(font.id)
                }
            }
            .contextMenu {
                deleteButton(for: font)
            }
    }
}
#endif
