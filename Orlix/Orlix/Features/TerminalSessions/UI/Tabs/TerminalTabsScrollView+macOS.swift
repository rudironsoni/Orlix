#if os(macOS)
import SwiftUI

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package struct TerminalTabsScrollView: View {
    package let tabs: [TerminalTab]
    @Binding package var selectedTabId: UUID?
    package let onClose: (TerminalTab) -> Void
    package let onNew: () -> Void
    @ObservedObject package var projection: TerminalServerToolbarTabStripProjection

    package var body: some View {
        ServerToolbarTabStrip(
            items: tabs,
            selectedId: selectedTabId,
            previousHelp: String(localized: "Previous tab"),
            nextHelp: String(localized: "Next tab"),
            newHelp: String(localized: "New terminal tab"),
            onPrevious: selectPrevious,
            onNext: selectNext,
            onNew: onNew
        ) { tab, tabWidth, glassNamespace in
            let item = projection.state.items.first { $0.id == tab.id }
            return TerminalTabButton(
                tab: tab,
                item: item,
                isSelected: selectedTabId == tab.id,
                width: tabWidth,
                glassNamespace: glassNamespace,
                shortcutNumber: tabs.firstIndex(where: { $0.id == tab.id }).map { $0 + 1 },
                onSelect: { selectedTabId = tab.id },
                onClose: { onClose(tab) }
            )
        }
    }

    private func selectPrevious() {
        guard let currentId = selectedTabId,
              let currentIndex = tabs.firstIndex(where: { $0.id == currentId }),
              currentIndex > 0 else { return }
        selectedTabId = tabs[currentIndex - 1].id
    }

    private func selectNext() {
        guard let currentId = selectedTabId,
              let currentIndex = tabs.firstIndex(where: { $0.id == currentId }),
              currentIndex < tabs.count - 1 else { return }
        selectedTabId = tabs[currentIndex + 1].id
    }
}

private struct TerminalTabButton: View {
    package let tab: TerminalTab
    package let item: TerminalServerToolbarTabItem?
    package let isSelected: Bool
    package let width: CGFloat
    package var glassNamespace: Namespace.ID?
    package var shortcutNumber: Int?
    package let onSelect: () -> Void
    package let onClose: () -> Void

    private var statusColor: Color {
        guard let connectionState = item?.connectionState else { return .secondary }
        switch connectionState {
        case .connected:
            return .green
        case .connecting, .reconnecting:
            return .orange
        case .disconnected, .idle:
            return .secondary
        case .failed:
            return .red
        }
    }

    package var body: some View {
        ServerToolbarTabCell(
            title: tabTitle,
            isSelected: isSelected,
            statusColor: statusColor,
            width: width,
            accessibilityLabel: item?.title ?? tab.title,
            glassNamespace: glassNamespace,
            shortcutNumber: shortcutNumber,
            onSelect: onSelect,
            onClose: onClose
        )
    }

    private var tabTitle: String {
        let title = item?.title ?? tab.title
        guard tab.paneCount > 1 else { return title }
        return "\(title) ⊞"
    }
}
#endif
