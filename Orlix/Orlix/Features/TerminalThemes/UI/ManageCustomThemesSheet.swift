import SwiftUI
import UniformTypeIdentifiers

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
private struct PendingCustomThemeSource: Identifiable {
    package let id = UUID()
    package var suggestedName: String
    package var content: String
}

package struct ManageCustomThemesSheet: View {
    package let customThemes: [TerminalTheme]
    package let themeSelection: TerminalThemeSelection
    package let onClose: () -> Void
    package let onSuggestThemeName: (String) -> String
    package let onCreateTheme: (String, String, TerminalThemeSelectionTarget) throws -> Void
    package let onApplyTheme: (String, TerminalThemeSelectionTarget) -> Void
    package let onDelete: (UUID) -> Void
    package let onSaveEdit: (UUID, String, String) throws -> Void

    @State private var showingThemeImporter = false
    @State private var showingThemeBuilder = false
    @State private var pendingCustomThemeSource: PendingCustomThemeSource?
    @State private var customThemeErrorMessage: String?
    @State package var themePendingDeletion: TerminalTheme?
    @State package var themePendingEdit: TerminalTheme?
    @State package var hoveredThemeID: UUID?

    package var sortedThemes: [TerminalTheme] {
        customThemes.sorted { $0.name.localizedCaseInsensitiveCompare($1.name) == .orderedAscending }
    }

    private var deleteThemeAlertBinding: Binding<Bool> {
        Binding(
            get: { themePendingDeletion != nil },
            set: { newValue in
                if !newValue {
                    themePendingDeletion = nil
                }
            }
        )
    }

    private var editThemeSheetBinding: Binding<TerminalTheme?> {
        Binding(
            get: { themePendingEdit },
            set: { themePendingEdit = $0 }
        )
    }

    private var customThemeErrorAlertBinding: Binding<Bool> {
        Binding(
            get: { customThemeErrorMessage != nil },
            set: { newValue in
                if !newValue {
                    customThemeErrorMessage = nil
                }
            }
        )
    }

    package var body: some View {
        platformBody
        .sheet(item: editThemeSheetBinding) { theme in
            ThemeBuilderSheet(
                usePerAppearanceTheme: false,
                showApplyTarget: false,
                title: String(
                    format: String(localized: "Edit \"%@\""),
                    theme.name
                ),
                initialName: theme.name,
                initialContent: theme.content,
                onDeleteRequest: {
                    onDelete(theme.id)
                    themePendingEdit = nil
                }
            ) { name, content, _ in
                try onSaveEdit(theme.id, name, content)
            }
            .adaptiveSoftScrollEdges()
            #if os(macOS)
            .frame(minWidth: 700, minHeight: 600)
            #endif
        }
        .fileImporter(
            isPresented: $showingThemeImporter,
            allowedContentTypes: [.text, .data],
            allowsMultipleSelection: false
        ) { result in
            handleThemeImport(result)
        }
        .sheet(item: $pendingCustomThemeSource) { source in
            CustomThemeSaveSheet(
                suggestedName: source.suggestedName,
                usePerAppearanceTheme: themeSelection.usePerAppearanceTheme
            ) { name, applyTarget in
                try onCreateTheme(name, source.content, applyTarget)
            }
            .adaptiveSoftScrollEdges()
        }
        .sheet(isPresented: $showingThemeBuilder) {
            ThemeBuilderSheet(
                usePerAppearanceTheme: themeSelection.usePerAppearanceTheme
            ) { name, content, applyTarget in
                try onCreateTheme(name, content, applyTarget)
            }
            .adaptiveSoftScrollEdges()
            #if os(macOS)
            .frame(minWidth: 700, minHeight: 600)
            #endif
        }
        .alert("Delete Custom Theme?", isPresented: deleteThemeAlertBinding) {
            Button("Delete", role: .destructive) {
                if let themePendingDeletion {
                    onDelete(themePendingDeletion.id)
                }
                themePendingDeletion = nil
            }
            Button("Cancel", role: .cancel) {
                themePendingDeletion = nil
            }
        } message: {
            Text("This cannot be undone.")
        }
        .alert("Custom Theme", isPresented: customThemeErrorAlertBinding) {
            Button("OK", role: .cancel) {}
        } message: {
            Text(customThemeErrorMessage ?? "")
        }
        .adaptiveSoftScrollEdges()
    }

    package var customThemesEmptyState: some View {
        VStack(spacing: 14) {
            Image(systemName: "paintpalette")
                .font(.system(size: 44))
                .foregroundStyle(.tertiary)

            Text("No Custom Themes")
                .font(.headline.weight(.semibold))

            Text("Create your first custom theme from clipboard, file import, or builder.")
                .font(.subheadline)
                .foregroundStyle(.secondary)
                .multilineTextAlignment(.center)
                .frame(maxWidth: 320)

            customThemeCompatibilityNote
                .multilineTextAlignment(.center)
                .frame(maxWidth: 320)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }

    package var customThemeCompatibilityNote: some View {
        Text("Clipboard content or imported files must be Ghostty-compatible theme text.")
            .font(.caption)
            .foregroundStyle(.secondary)
    }

    package func assignmentLabel(for theme: String) -> String? {
        if themeSelection.usePerAppearanceTheme {
            let usesDark = themeSelection.darkThemeName == theme
            let usesLight = themeSelection.lightThemeName == theme

            switch (usesDark, usesLight) {
            case (true, true):
                return String(localized: "Dark + Light")
            case (true, false):
                return String(localized: "Dark")
            case (false, true):
                return String(localized: "Light")
            case (false, false):
                return nil
            }
        }

        return themeSelection.darkThemeName == theme ? String(localized: "Active") : nil
    }

    @ViewBuilder
    package func applyMenuItems(themeName: String) -> some View {
        if let theme = customThemes.first(where: { $0.name == themeName }), !theme.canApply {
            Button("Repair Theme") {
                themePendingEdit = theme
            }
        } else if themeSelection.usePerAppearanceTheme {
            Button("Apply to Dark") {
                applyThemeSelection(themeName: themeName, applyTarget: .dark)
            }
            Button("Apply to Light") {
                applyThemeSelection(themeName: themeName, applyTarget: .light)
            }
            Button("Apply to Both") {
                applyThemeSelection(themeName: themeName, applyTarget: .both)
            }
        } else {
            Button("Use Theme") {
                applyThemeSelection(themeName: themeName, applyTarget: .dark)
            }
        }
    }

    @ViewBuilder
    package var createThemeMenuItems: some View {
        Button("Paste from Clipboard") {
            importThemeFromClipboard()
        }
        Button("Import from File") {
            showingThemeImporter = true
        }
        Button("Builder") {
            showingThemeBuilder = true
        }
    }

    private func importThemeFromClipboard() {
        guard let text = Clipboard.readString() else {
            customThemeErrorMessage = String(localized: "Clipboard does not contain text.")
            return
        }

        preparePendingCustomTheme(content: text, suggestedName: String(localized: "Pasted Theme"))
    }

    private func handleThemeImport(_ result: Result<[URL], Error>) {
        switch result {
        case .success(let urls):
            guard let url = urls.first else { return }
            guard url.startAccessingSecurityScopedResource() else {
                customThemeErrorMessage = String(localized: "Cannot access selected file.")
                return
            }
            defer { url.stopAccessingSecurityScopedResource() }

            do {
                let content = try String(contentsOf: url, encoding: .utf8)
                let suggestedName = url.deletingPathExtension().lastPathComponent
                preparePendingCustomTheme(content: content, suggestedName: suggestedName)
            } catch {
                customThemeErrorMessage = String(
                    format: String(localized: "Failed to import theme file: %@"),
                    error.localizedDescription
                )
            }
        case .failure(let error):
            customThemeErrorMessage = String(
                format: String(localized: "Failed to import theme file: %@"),
                error.localizedDescription
            )
        }
    }

    private func preparePendingCustomTheme(content: String, suggestedName: String) {
        do {
            let normalizedContent = try TerminalThemeValidator.validateAndNormalizeThemeContent(content)
            pendingCustomThemeSource = PendingCustomThemeSource(
                suggestedName: onSuggestThemeName(suggestedName),
                content: normalizedContent
            )
        } catch {
            customThemeErrorMessage = error.localizedDescription
        }
    }

    package func applyThemeSelection(
        themeName: String,
        applyTarget: TerminalThemeSelectionTarget
    ) {
        onApplyTheme(themeName, applyTarget)
    }
}
