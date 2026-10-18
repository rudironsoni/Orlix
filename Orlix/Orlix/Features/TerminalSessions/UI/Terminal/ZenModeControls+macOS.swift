#if os(macOS)
import SwiftUI
import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
import OrlixSettings
#endif
package struct ZenModePanel: View {
    package let width: CGFloat
    package let serverName: String
    package let statusText: String
    package let statusColor: Color
    package let selectedView: ConnectionViewTabID
    package let selectedViewBinding: Binding<ConnectionViewTabID>
    package let viewTabs: [ConnectionViewTabID]
    package let terminalTabs: [TerminalTab]
    package let selectedTerminalTabId: Binding<UUID?>
    package let terminalTabTitle: (TerminalTab) -> String
    package let paneState: (TerminalTab) -> TerminalPaneState?
    package let fileTabs: [RemoteFileTab]
    package let selectedFileTabId: Binding<UUID?>
    package let fileTabTitle: (RemoteFileTab) -> String
    package let onPreviousTab: () -> Void
    package let onNextTab: () -> Void
    package let onNewTerminalTab: () -> Void
    package let onCloseTerminalTab: (TerminalTab) -> Void
    package let onNewFileTab: () -> Void
    package let onCloseFileTab: (RemoteFileTab) -> Void
    package let onSelectFileTab: (RemoteFileTab) -> Void
    package let onSplitRight: () -> Void
    package let onSplitDown: () -> Void
    package let onClosePane: () -> Void
    package let canSplit: Bool
    package let canClosePane: Bool
    package let isSidebarVisible: Bool
    package let onToggleSidebar: () -> Void
    package let onDisconnect: () -> Void
    package let canFilesGoUp: Bool
    package let filesShowHiddenBinding: Binding<Bool>
    package let onFilesGoUp: () -> Void
    package let onFilesRefresh: () -> Void
    package let onExitZen: () -> Void

    package var body: some View {
        ScrollView(showsIndicators: false) {
            VStack(alignment: .leading, spacing: 16) {
                panelContent
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(16)
        }
        .frame(width: width)
        .frame(maxHeight: 430)
        .background(.clear)
    }

    @ViewBuilder
    private var panelContent: some View {
        ZenModeStatusLine(
            title: serverName,
            subtitle: statusText,
            indicatorColor: statusColor
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
                }
            }
        }

        ZenModeSection("Tabs") {
            HStack(spacing: 8) {
                ZenModeActionButton(title: "Previous Tab", systemImage: "chevron.left") {
                    onPreviousTab()
                }
                .frame(maxWidth: .infinity)
                .disabled(activeTabCount <= 1)

                ZenModeActionButton(title: "Next Tab", systemImage: "chevron.right") {
                    onNextTab()
                }
                .frame(maxWidth: .infinity)
                .disabled(activeTabCount <= 1)
            }

            ZenModeActionButton(title: "New Tab", systemImage: "plus") {
                if selectedView == .files {
                    onNewFileTab()
                } else {
                    onNewTerminalTab()
                }
            }

            if selectedView == .files {
                if fileTabs.isEmpty {
                    Text("No file tabs open.")
                        .font(.callout)
                        .foregroundStyle(.secondary)
                        .padding(.horizontal, 4)
                } else {
                    VStack(spacing: 8) {
                        ForEach(fileTabs) { tab in
                            fileTabRow(tab)
                        }
                    }
                }
            } else if terminalTabs.isEmpty {
                Text("No terminals open.")
                    .font(.callout)
                    .foregroundStyle(.secondary)
                    .padding(.horizontal, 4)
            } else {
                VStack(spacing: 8) {
                    ForEach(terminalTabs) { tab in
                        terminalTabRow(tab)
                    }
                }
            }
        }

        if selectedView == .terminal {
            ZenModeSection("Pane") {
                ZenModeActionButton(
                    title: "Split Right",
                    systemImage: "rectangle.split.2x1"
                ) {
                    onSplitRight()
                }
                .disabled(!canSplit)

                ZenModeActionButton(
                    title: "Split Down",
                    systemImage: "rectangle.split.1x2"
                ) {
                    onSplitDown()
                }
                .disabled(!canSplit)

                ZenModeActionButton(
                    title: "Close Pane",
                    systemImage: "xmark.square",
                    tint: .red
                ) {
                    onClosePane()
                }
                .disabled(!canClosePane)
            }
        }

        if selectedView == .files {
            ZenModeSection("Files") {
                HStack(spacing: 8) {
                    ZenModeActionButton(title: "Parent", systemImage: "arrow.turn.up.left") {
                        onFilesGoUp()
                    }
                    .frame(maxWidth: .infinity)
                    .disabled(!canFilesGoUp)

                    ZenModeActionButton(title: "Refresh", systemImage: "arrow.clockwise") {
                        onFilesRefresh()
                    }
                    .frame(maxWidth: .infinity)
                }

                ZenModeActionButton(
                    title: filesShowHiddenBinding.wrappedValue
                        ? "Hide Hidden Files"
                        : "Show Hidden Files",
                    systemImage: filesShowHiddenBinding.wrappedValue
                        ? "eye.slash"
                        : "eye",
                    tint: filesShowHiddenBinding.wrappedValue ? .orange : .primary
                ) {
                    filesShowHiddenBinding.wrappedValue.toggle()
                }
                .frame(maxWidth: .infinity)
            }
        }

        ZenModeSection("Window") {
            ZenModeActionButton(
                title: LocalizedStringKey(isSidebarVisible ? "Hide Sidebar" : "Show Sidebar"),
                systemImage: "sidebar.left"
            ) {
                onToggleSidebar()
            }
        }

        ZenModeSection("Session") {
            ZenModeActionButton(
                title: "Disconnect",
                systemImage: "xmark.circle",
                tint: .red
            ) {
                onDisconnect()
            }
        }

        ZenModeSection("Zen") {
            ZenModeActionButton(
                title: "Exit Zen Mode",
                systemImage: "arrow.down.right.and.arrow.up.left"
            ) {
                onExitZen()
            }
        }
    }

    private func terminalTabRow(_ tab: TerminalTab) -> some View {
        let state = paneState(tab)
        let tint = state?.connectionState.statusTintColor ?? .secondary
        let isSelected = selectedTerminalTabId.wrappedValue == tab.id

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
                .background(
                    RoundedRectangle(cornerRadius: 12, style: .continuous)
                        .fill(Color.primary.opacity(isSelected ? 0.14 : 0.07))
                )
            }
            .buttonStyle(.plain)

            Button {
                onCloseTerminalTab(tab)
            } label: {
                Image(systemName: "xmark")
                    .font(.system(size: 10, weight: .semibold))
                    .foregroundStyle(.secondary)
                    .frame(width: 28, height: 28)
                    .background(
                        Circle()
                            .fill(Color.primary.opacity(0.08))
                    )
            }
            .buttonStyle(.plain)
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
                .background(
                    RoundedRectangle(cornerRadius: 12, style: .continuous)
                        .fill(Color.primary.opacity(isSelected ? 0.14 : 0.07))
                )
            }
            .buttonStyle(.plain)

            Button {
                onCloseFileTab(tab)
            } label: {
                Image(systemName: "xmark")
                    .font(.system(size: 10, weight: .semibold))
                    .foregroundStyle(.secondary)
                    .frame(width: 28, height: 28)
                    .background(
                        Circle()
                            .fill(Color.primary.opacity(0.08))
                    )
            }
            .buttonStyle(.plain)
        }
    }

    private var activeTabCount: Int {
        selectedView == .files ? fileTabs.count : terminalTabs.count
    }
}
#endif
