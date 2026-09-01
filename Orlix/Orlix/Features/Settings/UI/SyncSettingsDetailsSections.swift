import SwiftUI

struct SyncSettingsDetailsSections: View {
    let summary: SyncSettingsContentSummary
    let contentSyncState: SyncSettingsContentSyncState
    let syncEnabled: Bool
    let lastSuccessfulSyncDate: Date?
    let pendingChangeCount: Int
    let lastError: SyncSettingsErrorRecord?
    let diagnostics: String
    let requestCredentialRemoval: () -> Void

    @State private var copiedDiagnostics: String?

    var body: some View {
        syncedDataSection
        syncDetailsSection
        if !syncEnabled {
            credentialRemovalSection
        }
    }

    private var syncedDataSection: some View {
        Section {
            SyncSettingsDetailsCountRow(
                title: "Servers",
                systemImage: "server.rack",
                count: summary.serverCount,
                accessibilityIdentifier: "orlix.settings.sync.details.servers"
            )
            SyncSettingsDetailsCountRow(
                title: "Workspaces",
                systemImage: "folder",
                count: summary.workspaceCount,
                accessibilityIdentifier: "orlix.settings.sync.details.workspaces"
            )
            SyncSettingsDetailsCountRow(
                title: "Server Credentials",
                systemImage: "key.fill",
                count: summary.serverCredentialCount,
                accessibilityIdentifier: "orlix.settings.sync.details.serverCredentials"
            )
            SyncSettingsDetailsCountRow(
                title: "Reusable SSH Keys",
                systemImage: "key.horizontal.fill",
                count: summary.reusableSSHKeyCount,
                accessibilityIdentifier: "orlix.settings.sync.details.reusableSSHKeys"
            )
            SyncSettingsDetailsCountRow(
                title: "Custom Themes",
                systemImage: "paintpalette",
                count: summary.customThemeCount,
                accessibilityIdentifier: "orlix.settings.sync.details.customThemes"
            )
            SyncSettingsDetailsCountRow(
                title: "Custom Fonts",
                systemImage: "textformat",
                count: summary.customFontCount,
                accessibilityIdentifier: "orlix.settings.sync.details.customFonts"
            )
            SyncSettingsDetailsStatusRow(
                title: "Terminal Appearance",
                systemImage: "circle.lefthalf.filled",
                status: storageStatus
            )
            SyncSettingsDetailsStatusRow(
                title: "Keyboard Toolbar",
                systemImage: "keyboard",
                status: storageStatus
            )
            SyncSettingsDetailsStatusRow(
                title: "Stats Layout",
                systemImage: "chart.xyaxis.line",
                status: storageStatus
            )
            SyncSettingsDetailsStatusRow(
                title: "Cloudflare Tokens",
                systemImage: "lock.shield",
                status: storageStatus
            )
        } header: {
            Text(contentSyncState.sectionTitle)
        } footer: {
            if syncEnabled {
                Text("Passwords, private keys, passphrases, and Cloudflare tokens use iCloud Keychain.")
            }
        }
    }

    private var syncDetailsSection: some View {
        Section {
            if let lastSuccessfulSyncDate {
                SyncSettingsDetailsRow(title: String(localized: "Last Successful Sync")) {
                    Text(
                        lastSuccessfulSyncDate,
                        format: .dateTime.year().month().day().hour().minute()
                    )
                    .foregroundStyle(.secondary)
                }
                .accessibilityIdentifier("orlix.settings.sync.details.lastSuccessful")
            }

            if pendingChangeCount > 0 {
                SyncSettingsDetailsRow(title: String(localized: "Pending Changes")) {
                    Text(pendingChangeCount, format: .number)
                        .foregroundStyle(.secondary)
                }
                .accessibilityIdentifier("orlix.settings.sync.details.pendingChanges")
            }

            if let lastError {
                SyncSettingsDetailsRow(title: String(localized: "Last Sync Error")) {
                    VStack(alignment: .trailing, spacing: 2) {
                        Text(lastError.category.title)
                        Text(
                            lastError.date,
                            format: .dateTime.year().month().day().hour().minute()
                        )
                        .font(.caption)
                    }
                    .foregroundStyle(.secondary)
                }
                .accessibilityIdentifier("orlix.settings.sync.details.lastError")
            }

            Button {
                Clipboard.copy(diagnostics)
                copiedDiagnostics = diagnostics
                SyncSettingsAccessibilityAnnouncement.post(
                    String(localized: "Copied")
                )
            } label: {
                Label(
                    copiedDiagnostics == diagnostics
                        ? String(localized: "Copied")
                        : String(localized: "Copy Diagnostics"),
                    systemImage: copiedDiagnostics == diagnostics
                        ? "checkmark"
                        : "doc.on.doc"
                )
            }
            .accessibilityIdentifier("orlix.settings.sync.copyDiagnostics")
        } header: {
            Text("Sync Details")
        }
    }

    private var credentialRemovalSection: some View {
        Section {
            Button(role: .destructive, action: requestCredentialRemoval) {
                Label("Remove Credentials from iCloud Keychain", systemImage: "key.slash")
            }
            .accessibilityIdentifier("orlix.settings.sync.removeCredentials")
        } footer: {
            Text("Credentials remain on this device. Existing app data in iCloud is not deleted.")
        }
    }

    private var storageStatus: String {
        contentSyncState.rowTitle
    }
}

private struct SyncSettingsDetailsCountRow: View {
    let title: String
    let systemImage: String
    let count: Int
    let accessibilityIdentifier: String

    var body: some View {
        SyncSettingsDetailsRow(title: title, systemImage: systemImage) {
            Text(count, format: .number)
                .foregroundStyle(.secondary)
        }
        .accessibilityIdentifier(accessibilityIdentifier)
    }
}

private struct SyncSettingsDetailsStatusRow: View {
    let title: String
    let systemImage: String
    let status: String

    var body: some View {
        SyncSettingsDetailsRow(title: title, systemImage: systemImage) {
            Text(status)
                .foregroundStyle(.secondary)
        }
    }
}

private struct SyncSettingsDetailsRow<Content: View>: View {
    let title: String
    let systemImage: String?
    @ViewBuilder let content: () -> Content

    init(
        title: String,
        systemImage: String? = nil,
        @ViewBuilder content: @escaping () -> Content
    ) {
        self.title = title
        self.systemImage = systemImage
        self.content = content
    }

    var body: some View {
        HStack {
            if let systemImage {
                Label(title, systemImage: systemImage)
            } else {
                Text(title)
            }
            Spacer()
            content()
        }
    }
}
