import SwiftUI

#if os(macOS)
import AppKit
#endif

package struct ServerViewTabNavigationButton: View {
    package let icon: String
    package let action: () -> Void
    package var help: String = ""

    @State private var isHovering = false

    package var body: some View {
        Button(action: action) {
            Image(systemName: icon)
                .font(.system(size: 11))
                .frame(width: 24, height: 24)
                #if os(macOS)
                .background(
                    isHovering ? Color(nsColor: .separatorColor).opacity(0.5) : Color.clear,
                    in: Circle()
                )
                #else
                .background(
                    isHovering ? Color.gray.opacity(0.3) : Color.clear,
                    in: Circle()
                )
                #endif
        }
        .buttonStyle(.plain)
        #if os(macOS)
        .onHover { isHovering = $0 }
        #endif
        .help(help)
    }
}

package struct ServerViewNewTabButton: View {
    package let help: String
    package let action: () -> Void

    @State private var isHovering = false

    package var body: some View {
        Button(action: action) {
            Image(systemName: "plus")
                .font(.system(size: 11))
                .frame(width: 24, height: 24)
                #if os(macOS)
                .background(
                    isHovering ? Color(nsColor: .separatorColor).opacity(0.5) : Color.clear,
                    in: Circle()
                )
                #else
                .background(
                    isHovering ? Color.gray.opacity(0.3) : Color.clear,
                    in: Circle()
                )
                #endif
        }
        .buttonStyle(.plain)
        #if os(macOS)
        .onHover { isHovering = $0 }
        #endif
        .help(Text(help))
    }
}

package enum ServerViewTopTabBarMetrics {
    static package let tabHeight: CGFloat = 36
    static package let tabVerticalPadding: CGFloat = 7
    static package let barVerticalInset: CGFloat = 4
    static package let tabSpacing: CGFloat = 4
    static package let horizontalPadding: CGFloat = 4
    static package let outerHorizontalPadding: CGFloat = 12
    #if os(macOS)
    static package let toolbarTabCapsuleHeight: CGFloat = 28
    /// Outer height of a toolbar tab cell. Kept close to the native toolbar
    /// control height so the tab strip isn't taller than the other items.
    static package let toolbarTabStripHeight: CGFloat = 30
    static package let toolbarTabStripIdealWidth: CGFloat = 640
    static package let toolbarTabStripFallbackWidth: CGFloat = 1_600
    /// Per-tab sizing shared by terminal and file tabs (browser-like).
    static package let toolbarTabMinimumWidth: CGFloat = 120
    static package let toolbarTabMaximumWidth: CGFloat = 240
    #endif
    static package var barHeight: CGFloat { tabHeight + barVerticalInset * 2 }
}

#if os(macOS)
package struct ServerToolbarTabCell: View {
    package let title: String
    package let isSelected: Bool
    package let statusColor: Color
    package let width: CGFloat
    package var accessibilityLabel: String?
    /// Shared namespace so the selected glass capsule morphs between tabs.
    package var glassNamespace: Namespace.ID?
    /// 1-based tab position; shows a ⌘N hint on hover for the first nine tabs.
    package var shortcutNumber: Int?
    package let onSelect: () -> Void
    package let onClose: () -> Void

    @Environment(\.accessibilityReduceTransparency) private var reduceTransparency
    @State private var isHovering = false

    package var body: some View {
        Button(action: onSelect) {
            ZStack {
                tabSurface
                tabLabel
            }
            .frame(width: width, height: ServerViewTopTabBarMetrics.toolbarTabCapsuleHeight)
            .frame(height: ServerViewTopTabBarMetrics.toolbarTabStripHeight)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .onHover { isHovering = $0 }
        .accessibilityLabel(accessibilityLabel ?? title)
        .accessibilityValue(isSelected ? String(localized: "Selected") : "")
    }

    @ViewBuilder
    private var tabLabel: some View {
        HStack(spacing: 6) {
            Button(action: onClose) {
                Image(systemName: "xmark")
                    .font(.system(size: 8, weight: .semibold))
                    .foregroundStyle(.secondary)
                    .frame(width: 14, height: 14)
                    .background(
                        Circle()
                            .fill(Color.primary.opacity(isHovering ? 0.1 : 0))
                    )
            }
            .buttonStyle(.plain)

            Circle()
                .fill(statusColor)
                .frame(width: 6, height: 6)

            Text(title)
                .font(.callout)
                .lineLimit(1)
                .truncationMode(.tail)

            if let shortcutNumber, shortcutNumber <= 9 {
                Spacer(minLength: 4)
                Text("⌘\(shortcutNumber)")
                    .font(.system(size: 10, weight: .medium))
                    .monospacedDigit()
                    .foregroundStyle(.tertiary)
            }
        }
        .padding(.leading, 6)
        .padding(.trailing, 12)
        .frame(
            width: width,
            height: ServerViewTopTabBarMetrics.toolbarTabCapsuleHeight,
            alignment: .leading
        )
    }

    @ViewBuilder
    private var tabSurface: some View {
        if isSelected {
            selectedGlassSurface
        } else {
            // No background at rest — only a subtle highlight on hover.
            Capsule()
                .fill(Color(nsColor: .separatorColor).opacity(isHovering ? 0.5 : 0))
        }
    }

    @ViewBuilder
    private var selectedGlassSurface: some View {
        if reduceTransparency {
            Capsule()
                .fill(.regularMaterial)
        } else if #available(iOS 26, macOS 26, *) {
            selectedLiquidGlass
        } else {
            Capsule()
                .fill(.ultraThinMaterial)
        }
    }

    /// The selected tab's glass capsule. matchedGeometryEffect with a shared id
    /// makes the capsule slide/morph from the previously selected tab to the new
    /// one on switch. The glass stays per-cell (behind the label only), so the
    /// label is never composited into the glass and stays sharp.
    @available(iOS 26, macOS 26, *)
    @ViewBuilder
    private var selectedLiquidGlass: some View {
        let glass = GlassEffectContainer(spacing: 0) {
            Capsule()
                .fill(.clear)
                .frame(width: width, height: ServerViewTopTabBarMetrics.toolbarTabCapsuleHeight)
                .glassEffect(.regular.interactive(), in: .capsule)
        }
        if let glassNamespace {
            glass.matchedGeometryEffect(id: ServerToolbarTabCell.selectedGlassID, in: glassNamespace)
        } else {
            glass
        }
    }

    private static let selectedGlassID = "orlix.selectedTab"

    package init(
        title: String,
        isSelected: Bool,
        statusColor: Color,
        width: CGFloat,
        accessibilityLabel: String? = nil,
        glassNamespace: Namespace.ID? = nil,
        shortcutNumber: Int? = nil,
        onSelect: @escaping () -> Void,
        onClose: @escaping () -> Void
    ) {
        self.title = title
        self.isSelected = isSelected
        self.statusColor = statusColor
        self.width = width
        self.accessibilityLabel = accessibilityLabel
        self.glassNamespace = glassNamespace
        self.shortcutNumber = shortcutNumber
        self.onSelect = onSelect
        self.onClose = onClose
    }
}

package struct AdaptiveServerTabSizing: Equatable {
    package let tabWidth: CGFloat
    package let isScrollable: Bool

    static package func resolve(
        containerWidth: CGFloat,
        itemCount: Int,
        minimumTabWidth: CGFloat = ServerViewTopTabBarMetrics.toolbarTabMinimumWidth,
        maximumTabWidth: CGFloat = ServerViewTopTabBarMetrics.toolbarTabMaximumWidth,
        horizontalPadding: CGFloat = ServerViewTopTabBarMetrics.horizontalPadding,
        tabSpacing: CGFloat = ServerViewTopTabBarMetrics.tabSpacing
    ) -> AdaptiveServerTabSizing {
        let resolvedContainerWidth = containerWidth.isFinite
            ? containerWidth
            : ServerViewTopTabBarMetrics.toolbarTabStripFallbackWidth

        guard itemCount > 0, resolvedContainerWidth > 0 else {
            return AdaptiveServerTabSizing(tabWidth: minimumTabWidth, isScrollable: false)
        }

        let availableWidth = max(resolvedContainerWidth - horizontalPadding * 2, 0)
        let totalSpacing = tabSpacing * CGFloat(max(itemCount - 1, 0))
        let candidateWidth = (availableWidth - totalSpacing) / CGFloat(itemCount)

        // Below the minimum usable width we stop dividing evenly and scroll the
        // tabs horizontally at their minimum width instead.
        guard candidateWidth.isFinite, candidateWidth >= minimumTabWidth else {
            return AdaptiveServerTabSizing(tabWidth: minimumTabWidth, isScrollable: true)
        }

        // Cap individual tabs so a single tab never grows comically wide; the
        // leftover width is left to the rest of the toolbar (leading aligned).
        return AdaptiveServerTabSizing(
            tabWidth: min(candidateWidth, maximumTabWidth),
            isScrollable: false
        )
    }
}

package struct AdaptiveServerTabStrip<Item: Identifiable, TabContent: View>: View where Item.ID: Hashable {
    package let items: [Item]
    package let selectedId: Item.ID?
    package var minimumTabWidth: CGFloat = ServerViewTopTabBarMetrics.toolbarTabMinimumWidth
    package var maximumTabWidth: CGFloat = ServerViewTopTabBarMetrics.toolbarTabMaximumWidth
    package var tabContent: (Item, CGFloat, Namespace.ID) -> TabContent

    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @Namespace private var glassNamespace

    package var body: some View {
        GeometryReader { proxy in
            let sizing = AdaptiveServerTabSizing.resolve(
                containerWidth: proxy.size.width,
                itemCount: items.count,
                minimumTabWidth: minimumTabWidth,
                maximumTabWidth: maximumTabWidth
            )

            ScrollViewReader { scrollProxy in
                Group {
                    if sizing.isScrollable {
                        ScrollView(.horizontal, showsIndicators: false) {
                            tabStack(tabWidth: sizing.tabWidth)
                        }
                    } else {
                        tabStack(tabWidth: sizing.tabWidth)
                            .frame(maxWidth: .infinity, alignment: .leading)
                    }
                }
                .transaction { transaction in
                    if reduceMotion {
                        transaction.animation = nil
                    }
                }
                // Animate only when tabs are added/removed, not on every
                // container width change — otherwise the capsule eases behind
                // the window edge during live resize.
                .animation(
                    reduceMotion ? nil : .easeInOut(duration: 0.14),
                    value: items.count
                )
                // Morph the selected glass capsule between tabs on switch.
                .animation(
                    reduceMotion ? nil : .smooth(duration: 0.3),
                    value: selectedId
                )
                .onChange(of: selectedId) { newValue in
                    guard sizing.isScrollable, let newValue else { return }
                    withOptionalTabAnimation {
                        scrollProxy.scrollTo(newValue, anchor: .center)
                    }
                }
                .onChange(of: items.map(\.id)) { _ in
                    guard sizing.isScrollable, let selectedId else { return }
                    withOptionalTabAnimation {
                        scrollProxy.scrollTo(selectedId, anchor: .center)
                    }
                }
            }
        }
        .frame(
            minWidth: minimumTabWidth,
            idealWidth: ServerViewTopTabBarMetrics.toolbarTabStripIdealWidth,
            maxWidth: .infinity,
            minHeight: ServerViewTopTabBarMetrics.toolbarTabStripHeight,
            maxHeight: ServerViewTopTabBarMetrics.toolbarTabStripHeight
        )
    }

    private func tabStack(tabWidth: CGFloat) -> some View {
        HStack(spacing: ServerViewTopTabBarMetrics.tabSpacing) {
            ForEach(items) { item in
                tabContent(item, tabWidth, glassNamespace)
                    .id(item.id)
            }
        }
        .padding(.horizontal, ServerViewTopTabBarMetrics.horizontalPadding)
        .frame(height: ServerViewTopTabBarMetrics.toolbarTabStripHeight)
    }

    private func withOptionalTabAnimation(_ action: () -> Void) {
        if reduceMotion {
            action()
        } else {
            withAnimation(.easeInOut(duration: 0.14), action)
        }
    }
}

package struct ServerToolbarTabStrip<Item: Identifiable, TabContent: View>: View where Item.ID: Hashable {
    package let items: [Item]
    package let selectedId: Item.ID?
    package let previousHelp: String
    package let nextHelp: String
    package let newHelp: String
    package let onPrevious: () -> Void
    package let onNext: () -> Void
    package let onNew: () -> Void
    package var tabContent: (Item, CGFloat, Namespace.ID) -> TabContent

    package var body: some View {
        HStack(spacing: 4) {
            HStack(spacing: 4) {
                ServerViewTabNavigationButton(
                    icon: "chevron.left",
                    action: onPrevious,
                    help: previousHelp
                )
                .disabled(items.count <= 1)

                ServerViewTabNavigationButton(
                    icon: "chevron.right",
                    action: onNext,
                    help: nextHelp
                )
                .disabled(items.count <= 1)
            }
            .padding(.leading, 8)

            // Tabs stretch to fill the strip and shrink/scroll when there are
            // too many. The strip simply fills its container — its width is set
            // by the hosting NSToolbarItem, which provides native fill + overflow.
            AdaptiveServerTabStrip(
                items: items,
                selectedId: selectedId,
                maximumTabWidth: .infinity,
                tabContent: tabContent
            )
            .layoutPriority(1)

            ServerViewNewTabButton(
                help: newHelp,
                action: onNew
            )
            .padding(.trailing, 8)
        }
        .frame(maxWidth: .infinity)
    }


    package init(

        items: [Item],

        selectedId: Item.ID? = nil,

        previousHelp: String,

        nextHelp: String,

        newHelp: String,

        onPrevious: @escaping () -> Void,

        onNext: @escaping () -> Void,

        onNew: @escaping () -> Void,

        tabContent: @escaping (Item, CGFloat, Namespace.ID) -> TabContent

    ) {

        self.items = items

        self.selectedId = selectedId

        self.previousHelp = previousHelp

        self.nextHelp = nextHelp

        self.newHelp = newHelp

        self.onPrevious = onPrevious

        self.onNext = onNext

        self.onNew = onNew

        self.tabContent = tabContent

    }
}

#endif
