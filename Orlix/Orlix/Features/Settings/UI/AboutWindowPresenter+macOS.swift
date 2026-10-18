#if os(macOS)
import AppKit
import SwiftUI

@MainActor
package protocol AboutWindowHandling: AnyObject {
    func presentAboutWindow()
}

extension NSWindow: AboutWindowHandling {
    package func presentAboutWindow() {
        makeKeyAndOrderFront(nil)
    }
}

/// Owns the macOS About window and reuses it after it is closed.
@MainActor
final package class AboutWindowPresenter {
    package typealias MakeWindow = @MainActor () -> any AboutWindowHandling
    package typealias ActivateApplication = @MainActor () -> Void

    private let makeWindow: MakeWindow
    private let activateApplication: ActivateApplication
    private var aboutWindow: (any AboutWindowHandling)?

    package init() {
        makeWindow = {
            Self.makeAboutWindow()
        }
        activateApplication = {
            NSApp.activate(ignoringOtherApps: true)
        }
    }

    package init(
        makeWindow: @escaping MakeWindow,
        activateApplication: @escaping ActivateApplication
    ) {
        self.makeWindow = makeWindow
        self.activateApplication = activateApplication
    }

    package func show() {
        let window: any AboutWindowHandling
        if let aboutWindow {
            window = aboutWindow
        } else {
            let newWindow = makeWindow()
            aboutWindow = newWindow
            window = newWindow
        }

        window.presentAboutWindow()
        activateApplication()
    }

    private static func makeAboutWindow() -> NSWindow {
        let aboutView = AboutView()
        let hostingView = NSHostingView(rootView: aboutView)
        hostingView.setFrameSize(hostingView.fittingSize)

        let window = NSWindow(
            contentRect: NSRect(
                x: 0,
                y: 0,
                width: hostingView.fittingSize.width,
                height: hostingView.fittingSize.height
            ),
            styleMask: [.titled, .closable],
            backing: .buffered,
            defer: false
        )
        window.title = String(localized: "About Orlix")
        window.contentView = hostingView
        window.center()
        window.isReleasedWhenClosed = false
        return window
    }
}
#endif
