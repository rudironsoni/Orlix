import SwiftUI

#if os(macOS)
#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package struct RemoteFileTabsScrollView: View {
    package let tabs: [RemoteFileTab]
    @Binding package var selectedTabId: UUID?
    // Observed so the hosted tab strip redraws when navigation changes a tab's
    // path/title (the title closures read this store). The toolbar host itself
    // only observes the terminal manager, so without this the strip would keep
    // showing the old folder until a tab open/close/select forced a redraw.
    @ObservedObject package var fileBrowser: RemoteFileBrowserStore
    package let titleForTab: (RemoteFileTab) -> String
    package let onSelect: (RemoteFileTab) -> Void
    package let onClose: (RemoteFileTab) -> Void
    package let onCloseOtherTabs: (RemoteFileTab) -> Void
    package let onCloseTabsToLeft: (RemoteFileTab) -> Void
    package let onCloseTabsToRight: (RemoteFileTab) -> Void
    package let onDuplicate: (RemoteFileTab) -> Void
    package let onNew: () -> Void

    package var body: some View {
        ServerToolbarTabStrip(
            items: tabs,
            selectedId: selectedTabId,
            previousHelp: String(localized: "Previous file tab"),
            nextHelp: String(localized: "Next file tab"),
            newHelp: String(localized: "New file tab"),
            onPrevious: selectPrevious,
            onNext: selectNext,
            onNew: onNew
        ) { tab, tabWidth, glassNamespace in
            ServerToolbarTabCell(
                title: titleForTab(tab),
                isSelected: selectedTabId == tab.id,
                statusColor: .green,
                width: tabWidth,
                glassNamespace: glassNamespace,
                shortcutNumber: tabs.firstIndex(where: { $0.id == tab.id }).map { $0 + 1 },
                onSelect: { onSelect(tab) },
                onClose: { onClose(tab) }
            )
            .contextMenu {
                Button(String(localized: "Close Tab")) {
                    onClose(tab)
                }

                Divider()

                Button(String(localized: "Close Other Tabs")) {
                    onCloseOtherTabs(tab)
                }

                Button(String(localized: "Close All to the Left")) {
                    onCloseTabsToLeft(tab)
                }
                .disabled((tabs.firstIndex(where: { $0.id == tab.id }) ?? 0) == 0)

                Button(String(localized: "Close All to the Right")) {
                    onCloseTabsToRight(tab)
                }
                .disabled((tabs.firstIndex(where: { $0.id == tab.id }) ?? (tabs.count - 1)) >= tabs.count - 1)

                Divider()

                Button(String(localized: "Duplicate Tab")) {
                    onDuplicate(tab)
                }
            }
        }
    }

    private func selectPrevious() {
        guard let currentId = selectedTabId,
              let currentIndex = tabs.firstIndex(where: { $0.id == currentId }),
              currentIndex > 0 else { return }

        let target = tabs[currentIndex - 1]
        onSelect(target)
    }

    private func selectNext() {
        guard let currentId = selectedTabId,
              let currentIndex = tabs.firstIndex(where: { $0.id == currentId }),
              currentIndex < tabs.count - 1 else { return }

        let target = tabs[currentIndex + 1]
        onSelect(target)
    }

    package init(
        tabs: [RemoteFileTab],
        selectedTabId: Binding<UUID?>,
        fileBrowser: RemoteFileBrowserStore,
        titleForTab: @escaping (RemoteFileTab) -> String,
        onSelect: @escaping (RemoteFileTab) -> Void,
        onClose: @escaping (RemoteFileTab) -> Void,
        onCloseOtherTabs: @escaping (RemoteFileTab) -> Void,
        onCloseTabsToLeft: @escaping (RemoteFileTab) -> Void,
        onCloseTabsToRight: @escaping (RemoteFileTab) -> Void,
        onDuplicate: @escaping (RemoteFileTab) -> Void,
        onNew: @escaping () -> Void
    ) {
        self.tabs = tabs
        self._selectedTabId = selectedTabId
        self.fileBrowser = fileBrowser
        self.titleForTab = titleForTab
        self.onSelect = onSelect
        self.onClose = onClose
        self.onCloseOtherTabs = onCloseOtherTabs
        self.onCloseTabsToLeft = onCloseTabsToLeft
        self.onCloseTabsToRight = onCloseTabsToRight
        self.onDuplicate = onDuplicate
        self.onNew = onNew
    }
}
#endif
