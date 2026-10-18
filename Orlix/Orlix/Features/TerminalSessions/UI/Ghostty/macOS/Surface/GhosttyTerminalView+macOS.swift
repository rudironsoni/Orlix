//
//  GhosttyTerminalView+macOS.swift
//  Orlix
//
//  macOS NSView implementation for Ghostty terminal rendering
//

#if os(macOS)
import AppKit
import CoreVideo
import OSLog
#if ORLIX_MODULAR_APP
import OrlixModels
#endif

/// NSView that embeds a Ghostty terminal surface with Metal rendering
///
/// This view handles:
/// - Metal layer setup for terminal rendering
/// - Input forwarding (keyboard, mouse, scroll)
/// - Focus management
/// - Surface lifecycle management
@MainActor
package class GhosttyTerminalView: NSView, NSUserInterfaceValidations {
    // MARK: - Properties

    package var ghosttyApp: ghostty_app_t?
    weak package var ghosttyAppWrapper: GhosttyRuntime?
    package var surface: Ghostty.Surface?
    package var surfaceReference: Ghostty.SurfaceReference?
    package let worktreePath: String
    package let paneId: String?
    package let initialCommand: String?
    package let useCustomIO: Bool

    /// Callback invoked when the terminal process exits
    package var onProcessExit: (() -> Void)?

    /// Callback invoked when the terminal title changes
    package var onTitleChange: ((String) -> Void)?

    /// Callback invoked when the terminal reports working directory changes (OSC 7)
    package var onPwdChange: ((String) -> Void)?

    /// Callback when the surface has produced its first layout/draw (used to hide loading UI)
    package var onReady: (() -> Void)?

    /// Callback for OSC 9;4 progress reports
    package var onProgressReport: ((GhosttyProgressState, Int?) -> Void)?

    /// Callback when terminal size changes (cols, rows) - used for SSH PTY resize
    package var onResize: ((Int, Int) -> Void)?

    /// Callback invoked when a magnification gesture requests terminal pane zoom.
    package var onZoomAction: ((TerminalZoomAction) -> TerminalZoomResult?)?

    /// Per-surface presentation overrides used to preserve pane zoom across global config reloads.
    package var surfacePresentationOverrides: TerminalPresentationOverrides = .empty

    /// Optional app-level paste interceptor used for rich clipboard routing.
    package var richPasteInterceptor: ((GhosttyTerminalView) -> Bool)?

    /// Optional pane/session actions exposed in the macOS contextual menu.
    package var terminalContextMenuActions: TerminalContextMenuActions?

    package var didSignalReady = false
    package var readonly = false
    package let clipboardConfirmationQueue = TerminalClipboardConfirmationQueue()
    package var presentedClipboardConfirmation: NSAlert?
    package var clipboardConfirmationRetryWorkItem: DispatchWorkItem?
    package var clipboardConfirmationRetryCount = 0

    /// Cell size in points for row-to-pixel conversion (used by scroll view)
    package var cellSize: NSSize = .zero

    /// Current scrollbar state from Ghostty core (used by scroll view)
    package var scrollbar: Ghostty.Action.Scrollbar?

    static package let logger = Logger(subsystem: Bundle.main.bundleIdentifier ?? "com.rudironsoni.orlix", category: "GhosttyTerminal")

    // MARK: - Display Link Rendering (event-driven for SSH)

    package var displayLink: CVDisplayLink?
    package var needsRender = false
    package var accumulatedMagnification: CGFloat = 0
    package let zoomIndicatorView = TerminalZoomIndicatorView()
    package var zoomIndicatorHideWorkItem: DispatchWorkItem?

    /// Idle detection for display link - stops after timeout to save CPU
    package var lastActivityTime: CFAbsoluteTime = 0
    package var idleCheckTimer: DispatchSourceTimer?
    static package let idleTimeout: CFTimeInterval = 0.1  // 100ms idle before stopping display link

    // MARK: - Handler Components

    package var imeHandler: GhosttyIMEHandler!
    package var inputHandler: GhosttyInputHandler!
    package let renderingSetup = GhosttyRenderingSetup()

    /// Observation for appearance changes
    package var appearanceObservation: NSKeyValueObservation?

    /// Observer for config reload notifications
    package var configReloadObserver: NSObjectProtocol?

    // MARK: - Rendering Control

    /// Flag to prevent operations during cleanup
    package var isShuttingDown = false

    // MARK: - Initialization

    /// Create a new Ghostty terminal view
    ///
    /// - Parameters:
    ///   - frame: The initial frame for the view
    ///   - worktreePath: Working directory for the terminal session
    ///   - ghosttyApp: The shared Ghostty app instance (C pointer)
    ///   - appWrapper: The GhosttyRuntime wrapper for surface tracking (optional)
    ///   - paneId: Unique identifier for this pane (used for tmux session persistence)
    ///   - command: Optional command to run instead of default shell
    ///   - useCustomIO: If true, uses callback backend for custom I/O (SSH clients)
    package init(frame: NSRect, worktreePath: String, ghosttyApp: ghostty_app_t, appWrapper: GhosttyRuntime? = nil, paneId: String? = nil, command: String? = nil, useCustomIO: Bool = false) {
        self.worktreePath = worktreePath
        self.ghosttyApp = ghosttyApp
        self.ghosttyAppWrapper = appWrapper
        self.paneId = paneId
        self.initialCommand = command
        self.useCustomIO = useCustomIO

        // Use a reasonable default size if frame is zero
        let initialFrame = frame.width > 0 && frame.height > 0 ? frame : NSRect(x: 0, y: 0, width: 800, height: 600)
        super.init(frame: initialFrame)

        // Initialize handlers before setup
        self.imeHandler = GhosttyIMEHandler(view: self, surface: nil)
        self.inputHandler = GhosttyInputHandler(view: self, surface: nil, imeHandler: self.imeHandler)

        setupLayer()
        setupSurface()
        setupTrackingArea()
        setupAppearanceObservation()
        setupFrameObservation()
        setupConfigReloadObservation()
        zoomIndicatorView.isHidden = true
        zoomIndicatorView.alphaValue = 0
        addSubview(zoomIndicatorView)
        if useCustomIO {
            setupDisplayLink()
        }
    }

    required package init?(coder: NSCoder) {
        fatalError("init(coder:) not supported")
    }

    isolated deinit {
        cleanup()
    }

    // MARK: - Setup

    /// Setup mouse tracking area for the entire view
    private func setupTrackingArea() {
        let options: NSTrackingArea.Options = [
            .mouseEnteredAndExited,
            .mouseMoved,
            .inVisibleRect,
            .activeAlways  // Track even when not focused
        ]

        let trackingArea = NSTrackingArea(
            rect: bounds,
            options: options,
            owner: self,
            userInfo: nil
        )
        addTrackingArea(trackingArea)
    }

    /// Callback context retained by display link - must be released when display link stops
    package var displayLinkCallbackContext: Unmanaged<DisplayLinkCallbackContext>?

    // MARK: - NSView Overrides

    override package var acceptsFirstResponder: Bool {
        return true
    }

    override package func becomeFirstResponder() -> Bool {
        let result = super.becomeFirstResponder()
        if result, let surface = surface?.unsafeCValue {
            ghostty_surface_set_focus(surface, true)
        }
        return result
    }

    override package func resignFirstResponder() -> Bool {
        let result = super.resignFirstResponder()
        if result, let surface = surface?.unsafeCValue {
            ghostty_surface_set_focus(surface, false)
        }
        return result
    }

    override package func updateTrackingAreas() {
        super.updateTrackingAreas()

        // Remove old tracking areas
        trackingAreas.forEach { removeTrackingArea($0) }

        // Recreate with current bounds
        setupTrackingArea()
    }

    // Track last size sent to Ghostty to avoid redundant updates
    package var lastSurfaceSize: CGSize = .zero

    // MARK: - Custom I/O API (for SSH clients)

    /// Callback invoked when user types in the terminal (keyboard input to send to SSH)
    package var writeCallback: ((Data) -> Void)?
}

#endif
