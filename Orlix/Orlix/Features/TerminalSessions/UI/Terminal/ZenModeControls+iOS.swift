#if os(iOS)
import SwiftUI

#if ORLIX_MODULAR_APP
import OrlixModels
import OrlixSettings
#endif
package struct IOSZenModePanel: View {
    package let width: CGFloat
    package let serverName: String
    package let selectedView: ConnectionViewTabID
    package let selectedViewBinding: Binding<ConnectionViewTabID>
    package let viewTabs: [ConnectionViewTabID]
    package let terminalTabs: [TerminalTab]
    package let selectedTerminalTabId: Binding<UUID?>
    package let terminalTabTitle: (TerminalTab) -> String
    package let paneState: (TerminalTab) -> TerminalPaneState?
    package let onCloseTerminalTab: (TerminalTab) -> Void
    package let fileTabs: [RemoteFileTab]
    package let selectedFileTabId: Binding<UUID?>
    package let fileTabTitle: (RemoteFileTab) -> String
    package let onSelectFileTab: (RemoteFileTab) -> Void
    package let onCloseFileTab: (RemoteFileTab) -> Void
    package let onNewTerminalTab: () -> Void
    package let onNewFileTab: () -> Void
    package let onOpenSettings: () -> Void
    package let onEditServer: () -> Void
    package let onDuplicateServer: () -> Void
    package let onDisconnect: () -> Void
    package let onBack: () -> Void
    package let onExitZen: () -> Void

    package var body: some View {
        ZenModePanelCard(width: width) {
            ZenModeStatusLine(
                title: serverName,
                subtitle: statusText,
                indicatorColor: indicatorColor
            )

            ZenModeSection("View") {
                HStack(spacing: 8) {
                    ForEach(viewTabs) { tab in
                        ZenModeChoiceChip(
                            title: LocalizedStringKey(tab.localizedKey),
                            systemImage: tab.icon,
                            isSelected: selectedView == tab.id
                        ) {
                            selectedViewBinding.wrappedValue = tab.id
                        }
                        .accessibilityIdentifier("orlix.terminal.zen.view.\(tab.rawValue)")
                    }
                }
            }

            ZenModeSection("Tabs") {
                ZenModeActionButton(title: "New Tab", systemImage: "plus") {
                    if selectedView == .files {
                        onNewFileTab()
                    } else {
                        onNewTerminalTab()
                    }
                }
                .accessibilityIdentifier("orlix.terminal.zen.newTab")

                tabList
            }

            ZenModeSection("Server") {
                ZenModeActionButton(title: "Settings", systemImage: "gear", action: onOpenSettings)
                    .accessibilityIdentifier("orlix.terminal.zen.settings")

                ZenModeActionButton(title: "Edit Server", systemImage: "pencil", action: onEditServer)
                    .accessibilityIdentifier("orlix.terminal.zen.editServer")

                ZenModeActionButton(
                    title: "Duplicate",
                    systemImage: "plus.square.on.square",
                    action: onDuplicateServer
                )
                .accessibilityIdentifier("orlix.terminal.zen.duplicateServer")

                ZenModeActionButton(title: "Back", systemImage: "chevron.left", action: onBack)
                    .accessibilityIdentifier("orlix.terminal.zen.back")
            }

            ZenModeSection("Session") {
                ZenModeActionButton(
                    title: "Disconnect",
                    systemImage: "xmark.circle",
                    tint: .red,
                    action: onDisconnect
                )
                .accessibilityIdentifier("orlix.terminal.zen.disconnect")
            }

            ZenModeSection("Zen") {
                ZenModeActionButton(
                    title: "Exit Zen Mode",
                    systemImage: "arrow.down.right.and.arrow.up.left",
                    action: onExitZen
                )
                .accessibilityIdentifier("orlix.terminal.exitZenMode")
            }
        }
        .accessibilityIdentifier("orlix.terminal.zenPanel")
    }

    @ViewBuilder
    private var tabList: some View {
        if selectedView == .files {
            if fileTabs.isEmpty {
                emptyTabsLabel("No file tabs open.")
            } else {
                VStack(spacing: 8) {
                    ForEach(fileTabs) { tab in
                        fileTabRow(tab)
                    }
                }
            }
        } else if terminalTabs.isEmpty {
            emptyTabsLabel("No terminals open.")
        } else {
            VStack(spacing: 8) {
                ForEach(terminalTabs) { tab in
                    terminalTabRow(tab)
                }
            }
        }
    }

    private func emptyTabsLabel(_ title: LocalizedStringKey) -> some View {
        Text(title)
            .font(.callout)
            .foregroundStyle(.secondary)
            .padding(.horizontal, 4)
    }

    private func terminalTabRow(_ tab: TerminalTab) -> some View {
        let isSelected = selectedTerminalTabId.wrappedValue == tab.id
        let tint = paneState(tab)?.connectionState.statusTintColor ?? .secondary

        return HStack(spacing: 8) {
            Button {
                selectedViewBinding.wrappedValue = .terminal
                selectedTerminalTabId.wrappedValue = tab.id
            } label: {
                HStack(spacing: 10) {
                    Circle()
                        .fill(tint)
                        .frame(width: 7, height: 7)

                    VStack(alignment: .leading, spacing: 2) {
                        Text(terminalTabTitle(tab))
                            .font(.callout.weight(isSelected ? .semibold : .regular))
                            .lineLimit(1)

                        if tab.paneCount > 1 {
                            Text(String(format: String(localized: "%lld panes"), Int64(tab.paneCount)))
                                .font(.caption)
                                .foregroundStyle(.secondary)
                        }
                    }

                    Spacer()
                }
                .padding(.horizontal, 12)
                .padding(.vertical, 10)
                .background(tabBackground(isSelected: isSelected))
            }
            .buttonStyle(.plain)

            closeButton {
                onCloseTerminalTab(tab)
            }
        }
    }

    private func fileTabRow(_ tab: RemoteFileTab) -> some View {
        let isSelected = selectedFileTabId.wrappedValue == tab.id

        return HStack(spacing: 8) {
            Button {
                selectedViewBinding.wrappedValue = .files
                selectedFileTabId.wrappedValue = tab.id
                onSelectFileTab(tab)
            } label: {
                HStack(spacing: 10) {
                    Image(systemName: "folder")
                        .foregroundStyle(.secondary)

                    Text(fileTabTitle(tab))
                        .font(.callout.weight(isSelected ? .semibold : .regular))
                        .lineLimit(1)

                    Spacer()
                }
                .padding(.horizontal, 12)
                .padding(.vertical, 10)
                .background(tabBackground(isSelected: isSelected))
            }
            .buttonStyle(.plain)

            closeButton {
                onCloseFileTab(tab)
            }
        }
    }

    private func tabBackground(isSelected: Bool) -> some View {
        RoundedRectangle(cornerRadius: 12, style: .continuous)
            .fill(Color.primary.opacity(isSelected ? 0.14 : 0.07))
    }

    private func closeButton(action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Image(systemName: "xmark")
                .font(.system(size: 11, weight: .bold))
                .foregroundStyle(Color.primary.opacity(0.92))
                .frame(width: 28, height: 28)
                .background(
                    Circle()
                        .fill(Color.primary.opacity(0.12))
                        .overlay(
                            Circle()
                                .stroke(Color.primary.opacity(0.12), lineWidth: 1)
                        )
                )
        }
        .buttonStyle(.plain)
        .accessibilityLabel(String(localized: "Close"))
    }

    private var statusText: String {
        if selectedView == .files {
            return fileTabs.isEmpty
                ? String(localized: "No open file tabs")
                : String(format: String(localized: "%lld open file tabs"), Int64(fileTabs.count))
        }

        return terminalTabs.isEmpty
            ? String(localized: "No open terminals")
            : String(format: String(localized: "%lld open tabs"), Int64(terminalTabs.count))
    }

    private var indicatorColor: Color {
        if selectedView == .files {
            return fileTabs.isEmpty ? .secondary : .green
        }

        guard let selectedId = selectedTerminalTabId.wrappedValue,
              let selectedTab = terminalTabs.first(where: { $0.id == selectedId }) else {
            return terminalTabs.isEmpty ? .secondary : .green
        }
        return paneState(selectedTab)?.connectionState.statusTintColor ?? .secondary
    }

    package init(
        width: CGFloat,
        serverName: String,
        selectedView: ConnectionViewTabID,
        selectedViewBinding: Binding<ConnectionViewTabID>,
        viewTabs: [ConnectionViewTabID],
        terminalTabs: [TerminalTab],
        selectedTerminalTabId: Binding<UUID?>,
        terminalTabTitle: @escaping (TerminalTab) -> String,
        paneState: @escaping (TerminalTab) -> TerminalPaneState?,
        onCloseTerminalTab: @escaping (TerminalTab) -> Void,
        fileTabs: [RemoteFileTab],
        selectedFileTabId: Binding<UUID?>,
        fileTabTitle: @escaping (RemoteFileTab) -> String,
        onSelectFileTab: @escaping (RemoteFileTab) -> Void,
        onCloseFileTab: @escaping (RemoteFileTab) -> Void,
        onNewTerminalTab: @escaping () -> Void,
        onNewFileTab: @escaping () -> Void,
        onOpenSettings: @escaping () -> Void,
        onEditServer: @escaping () -> Void,
        onDuplicateServer: @escaping () -> Void,
        onDisconnect: @escaping () -> Void,
        onBack: @escaping () -> Void,
        onExitZen: @escaping () -> Void
    ) {
        self.width = width
        self.serverName = serverName
        self.selectedView = selectedView
        self.selectedViewBinding = selectedViewBinding
        self.viewTabs = viewTabs
        self.terminalTabs = terminalTabs
        self.selectedTerminalTabId = selectedTerminalTabId
        self.terminalTabTitle = terminalTabTitle
        self.paneState = paneState
        self.onCloseTerminalTab = onCloseTerminalTab
        self.fileTabs = fileTabs
        self.selectedFileTabId = selectedFileTabId
        self.fileTabTitle = fileTabTitle
        self.onSelectFileTab = onSelectFileTab
        self.onCloseFileTab = onCloseFileTab
        self.onNewTerminalTab = onNewTerminalTab
        self.onNewFileTab = onNewFileTab
        self.onOpenSettings = onOpenSettings
        self.onEditServer = onEditServer
        self.onDuplicateServer = onDuplicateServer
        self.onDisconnect = onDisconnect
        self.onBack = onBack
        self.onExitZen = onExitZen
    }
}
#endif
