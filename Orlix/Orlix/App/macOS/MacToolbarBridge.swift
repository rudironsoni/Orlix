//
//  MacToolbarBridge.swift
//  Orlix
//
//  Typed SwiftUI to AppKit toolbar presentation state.
//

#if os(macOS)
import Combine
import SwiftUI

public enum MacToolbarCommandID: Equatable {
    public case selectView(String)
    public case filesParent
    public case filesRefresh
    public case filesUpload
    public case filesNewFolder
    public case filesToggleHidden
    public case filesCopyPath
    public case openSettings
    public case editServer
    public case duplicateServer
    public case disconnect
    public case enterZen
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
        let id: String
        let systemImage: String
        let help: String
    }

    package var segments: [Segment]
    package var selectedId: String
}

struct MacToolbarSnapshot: Equatable {
    var ownerId: String?
    var showsViewPicker = false
    var showsTabStrip = false
    var showsFilesMenu = false
    var isZenMode = false
    var zenTitle = ""
    var zenIcon = ""
    var zenSubtitle = ""
    var viewPicker: ToolbarViewPickerData?
    var filesMenu: [ToolbarMenuEntry] = []
    var serverMenu: [ToolbarMenuEntry] = []

    var isActive: Bool { ownerId != nil }
    static let inactive = MacToolbarSnapshot()
}

@MainActor
final class MacToolbarCommandDispatcher {
    private var ownerId: String?
    private var handler: (MacToolbarCommandID) -> Void = { _ in }

    func install(ownerId: String, handler: @escaping (MacToolbarCommandID) -> Void) {
        self.ownerId = ownerId
        self.handler = handler
    }

    func clear(ownerId: String) {
        guard self.ownerId == ownerId else { return }
        self.ownerId = nil
        handler = { _ in }
    }

    func dispatch(_ command: MacToolbarCommandID) {
        handler(command)
    }
}

@MainActor
final package class MacToolbarBridge: ObservableObject {
    static package let shared = MacToolbarBridge()

    @Published private(set) var snapshot: MacToolbarSnapshot = .inactive
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
