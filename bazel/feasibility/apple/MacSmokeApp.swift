import AppKit
import OrlixNativeLinkProbe

@main
final class MacAppDelegate: NSObject, NSApplicationDelegate {
    private var window: NSWindow?

    func applicationDidFinishLaunching(_ notification: Notification) {
        let window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 640, height: 400),
            styleMask: [.titled, .closable, .miniaturizable, .resizable],
            backing: .buffered,
            defer: false
        )
        window.title = NativeLinkProbe.label
        window.makeKeyAndOrderFront(nil)
        self.window = window
    }
}
