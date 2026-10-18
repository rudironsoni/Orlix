//
//  MacToolbarBridge.swift
//  Orlix
//
//  Typed SwiftUI to AppKit toolbar presentation state.
//

#if os(macOS)
import Combine
import SwiftUI

package enum MacToolbarCommandID: Equatable {
    case selectView(String)
    case filesParent
    case filesRefresh
    case filesUpload
    case filesNewFolder
    case filesToggleHidden
    case filesCopyPath
    case openSettings
    case editServer
    case duplicateServer
    case disconnect
    case enterZen
}

package struct ToolbarMenuEntry: Equatable {
    package let command: MacToolbarCommandID?
    package var title: String
    package var systemImage: String?
    package var isEnabled: Bool = true
    package var isDestructive: Bool = false

    static package let separator = ToolbarMenuEntry(
        command: nil,
        title: "-",
        systemImage: nil,
        isEnabled: false
    )

    package var isSeparator: Bool { command == nil }
}

package struct ToolbarViewPickerData: Equatable {
    package struct Segment: Equatable {
        package let id: String
        package let systemImage: String
        package let help: String
    }

    package var segments: [Segment]
    package var selectedId: String
}

package struct MacToolbarSnapshot: Equatable {
    package var ownerId: String?
    package var showsViewPicker = false
    package var showsTabStrip = false
    package var showsFilesMenu = false
    package var isZenMode = false
    package var zenTitle = ""
    package var zenIcon = ""
    package var zenSubtitle = ""
    package var viewPicker: ToolbarViewPickerData?
    package var filesMenu: [ToolbarMenuEntry] = []
    package var serverMenu: [ToolbarMenuEntry] = []

    package var isActive: Bool { ownerId != nil }
    static package let inactive = MacToolbarSnapshot()
}

@MainActor
final package class MacToolbarCommandDispatcher {
    private var ownerId: String?
    private var handler: (MacToolbarCommandID) -> Void = { _ in }

    package func install(ownerId: String, handler: @escaping (MacToolbarCommandID) -> Void) {
        self.ownerId = ownerId
        self.handler = handler
    }

    package func clear(ownerId: String) {
        guard self.ownerId == ownerId else { return }
        self.ownerId = nil
        handler = { _ in }
    }

    package func dispatch(_ command: MacToolbarCommandID) {
        handler(command)
    }
}

@MainActor
final package class MacToolbarBridge: ObservableObject {
    static package let shared = MacToolbarBridge()

    @Published package private(set) var snapshot: MacToolbarSnapshot = .inactive
    package let dispatcher = MacToolbarCommandDispatcher()

    // Type erasure is limited to the dynamic SwiftUI content hosted by AppKit.
    private var tabStripProvider: () -> AnyView = { AnyView(EmptyView()) }
    private var zenPanelProvider: () -> AnyView = { AnyView(EmptyView()) }

    private init() {}

    package func activate<TabStrip: View, ZenPanel: View>(
        snapshot: MacToolbarSnapshot,
        dispatch: @escaping (MacToolbarCommandID) -> Void,
        @ViewBuilder tabStrip: @escaping () -> TabStrip,
        @ViewBuilder zenPanel: @escaping () -> ZenPanel
    ) {
        guard let ownerId = snapshot.ownerId else { return }
        dispatcher.install(ownerId: ownerId, handler: dispatch)
        tabStripProvider = { AnyView(tabStrip()) }
        zenPanelProvider = { AnyView(zenPanel()) }
        self.snapshot = snapshot
    }

    package func deactivate(ownerId: String) {
        guard snapshot.ownerId == ownerId else { return }
        dispatcher.clear(ownerId: ownerId)
        tabStripProvider = { AnyView(EmptyView()) }
        zenPanelProvider = { AnyView(EmptyView()) }
        snapshot = .inactive
    }

    package func makeTabStripView() -> AnyView {
        tabStripProvider()
    }

    package func makeZenPanelView() -> AnyView {
        zenPanelProvider()
    }
}
#endif
