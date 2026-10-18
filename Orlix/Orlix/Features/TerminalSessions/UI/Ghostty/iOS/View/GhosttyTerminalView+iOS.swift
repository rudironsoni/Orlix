//
//  GhosttyTerminalView+iOS.swift
//  Orlix
//
//  iOS UIView implementation for Ghostty terminal rendering
//

#if os(iOS)
import UIKit
import Metal
import OSLog
import SwiftUI
import IOSurface
import CoreImage

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package enum TerminalSurfaceGeometryUpdate: Equatable, Sendable {
    case apply
    case preserveCurrentGrid
}

nonisolated package enum TerminalSurfaceGeometryPolicy {
    static package func update(
        renderingIsPaused: Bool,
        preservesForegroundKeyboardGrid: Bool,
        currentSize: CGSize,
        proposedSize: CGSize
    ) -> TerminalSurfaceGeometryUpdate {
        if renderingIsPaused {
            return .preserveCurrentGrid
        }
        if preservesForegroundKeyboardGrid,
           abs(proposedSize.width - currentSize.width) < 0.5,
           proposedSize.height > currentSize.height + 0.5 {
            return .preserveCurrentGrid
        }
        return .apply
    }
}

/// UIView that embeds a Ghostty terminal surface with Metal rendering
///
/// This view handles:
/// - Metal layer setup for terminal rendering (Ghostty handles this internally)
/// - Touch and keyboard input
/// - Surface lifecycle management
@MainActor
package class GhosttyTerminalView: UIView {
    static package let textInputContextID = "com.rudironsoni.orlix.GhosttyTerminalView"
    // MARK: - Properties

    package var ghosttyApp: ghostty_app_t?
    weak package var ghosttyAppWrapper: GhosttyRuntime?
    package var surface: Ghostty.Surface?
    package var surfaceReference: Ghostty.SurfaceReference?
    package let worktreePath: String
    package let paneId: String?
    package let initialCommand: String?
    package let useCustomIO: Bool
    package var terminalAccessoryInputSnapshot: TerminalAccessoryInputSnapshot
    package var keyboardToolbar: TerminalInputAccessoryView?

    /// Callback invoked when the terminal process exits
    package var onProcessExit: (() -> Void)?

    /// Callback invoked when the terminal title changes
    package var onTitleChange: ((String) -> Void)?

    /// Callback invoked when the terminal reports working directory changes (OSC 7)
    package var onPwdChange: ((String) -> Void)?

    /// Callback when the surface has produced its first layout/draw (used to hide loading UI)
    package var onReady: (() -> Void)?

    /// Callback invoked when the terminal grid changes (cols, rows).
    /// In custom I/O mode (SSH), the embedder should send a window-change.
    package var onResize: ((Int, Int) -> Void)?

    /// Optional UI-layer observer used by the opt-in keyboard viewport policy.
    /// It is called only when the rendered cursor rect changes.
    package var onKeyboardAvoidanceCursorRectChange: ((CGRect) -> Void)?
    /// Reports input-accessory host movement independently of software-keyboard
    /// geometry. Floating iPad keyboards can leave this view docked at the
    /// bottom of the screen.
    package var onKeyboardAvoidanceAccessoryFrameChange: (() -> Void)?
    package var lastKeyboardAvoidanceAccessoryFrame: CGRect?
    package var keyboardAvoidancePreservedSurfaceSize: CGSize?
    package var keyboardAvoidanceReferenceSurfaceSize: CGSize?
    package var tracksKeyboardAvoidanceReferenceSize = false

    /// Callback invoked when a pinch gesture requests terminal pane zoom.
    package var onZoomAction: ((TerminalZoomAction) -> TerminalZoomResult?)?

    /// Reports a successful per-surface mouse reporting override change.
    package var onMouseReportingSuppressionChange: ((Bool) -> Void)?
    package var isMouseReportingSuppressed = false

    /// App-owned pane actions invoked by local iPad keyboard shortcuts.
    package var onPaneKeyboardShortcut: ((TerminalSplitCommand) -> Void)?

    /// Per-surface presentation overrides used to preserve pane zoom across global config reloads.
    package var surfacePresentationOverrides: TerminalPresentationOverrides = .empty

    /// Callback for OSC 9;4 progress reports
    package var onProgressReport: ((GhosttyProgressState, Int?) -> Void)?

    /// Callback invoked when the voice input button is tapped
    package var onVoiceButtonTapped: (() -> Void)? {
        didSet {
            keyboardToolbar?.onVoice = onVoiceButtonTapped
        }
    }

    @discardableResult
    package func triggerVoiceInput() -> Bool {
        guard let onVoiceButtonTapped else { return false }
        onVoiceButtonTapped()
        return true
    }

    @discardableResult
    package func sendReturnKey() -> Bool {
        guard canRouteTerminalInput else { return false }
        sendToolbarKey(.enter)
        return true
    }

    /// Optional app-level paste interceptor used for rich clipboard routing.
    package var richPasteInterceptor: ((GhosttyTerminalView) -> Bool)?

    /// Callback invoked when custom terminal I/O emits user input.
    package var writeCallback: ((Data) -> Void)?

    /// Optional pane/session actions exposed in the iPad pointer contextual menu.
    package var terminalContextMenuActions: TerminalContextMenuActions?

    package var didSignalReady = false
    package var readonly = false
    package let clipboardConfirmationQueue = TerminalClipboardConfirmationQueue()
    package var presentedClipboardConfirmation: UIAlertController?
    package var clipboardConfirmationRetryWorkItem: DispatchWorkItem?
    package var clipboardConfirmationRetryCount = 0

    /// Prevent rendering when the view is offscreen or being torn down.
    package var isShuttingDown = false
    package var isPaused = false
    package var preservesForegroundKeyboardGrid = false
    #if DEBUG
    package var keyboardUITestSurfaceFocused = false
    package var keyboardUITestGridResizeCount = 0
    #endif
    package var customIORedrawScheduled = false
    package var keyRepeatTimer: DispatchSourceTimer?
    package var hardwareKeyRepeatState = TerminalHardwareKeyRepeatState<Ghostty.Input.KeyEvent>()
    #if DEBUG
    package var keyboardUITestUsesManualHardwareKeyRepeatClock = false
    #endif

    /// Track last surface size in pixels to avoid redundant resize/draw work.
    package var lastPixelSize: CGSize = .zero
    package var lastContentScale: CGFloat = 0
    package var lastReportedGrid: (cols: Int, rows: Int) = (0, 0)

    package var currentTerminalGridSize: (cols: Int, rows: Int)? {
        guard let size = terminalSize() else { return nil }
        let cols = Int(size.columns)
        let rows = Int(size.rows)
        guard cols > 0, rows > 0 else { return nil }
        return (cols, rows)
    }

    package var currentTerminalPixelSize: TerminalPixelSize? {
        TerminalPixelSize(size: lastPixelSize)
    }
    package var lastKeyboardAvoidanceCursorRect: CGRect?
    /// Cell size in points for row-to-pixel conversion
    package var cellSize: CGSize = .zero

    /// Current scrollbar state from Ghostty core
    package var scrollbar: Ghostty.Action.Scrollbar?

    static package let logger = Logger(subsystem: Bundle.main.bundleIdentifier ?? "com.rudironsoni.orlix", category: "GhosttyTerminal")

    package var isScrolling = false
    package var isPinchingTerminalZoom = false
    package var pinchReferenceScale: CGFloat = 1
    package let zoomIndicatorView = TerminalZoomIndicatorView()
    package var zoomIndicatorHideWorkItem: DispatchWorkItem?
    package let mouseCaptureIndicatorView = TerminalMouseCaptureIndicatorView()
    package var mouseCaptureIndicatorHideWorkItem: DispatchWorkItem?
    package var nativeSelectionSnapshot = TerminalNativeTextSnapshot.empty
    package var nativeSelectionLifecycle = TerminalNativeSelectionLifecycle()
    package var nativeSelectionLongPressAnchor: NSRange?
    package var nativeSelectedRange: NSRange? { nativeSelectionLifecycle.selection }
    package var nativeSelectionInteractionActive: Bool { nativeSelectionLifecycle.interactionIsActive }
    package var prefersNativeSelectionFirstResponder: Bool { nativeSelectionLifecycle.keepsFirstResponder }
    package var nativeTextInteraction: UITextInteraction?
    private var nativeFindInteractionStorage: AnyObject?
    private var nativeFindSessionStorage: AnyObject?
    @available(iOS 16.0, *)
    package var nativeFindInteraction: UIFindInteraction? {
        get { nativeFindInteractionStorage as? UIFindInteraction }
        set { nativeFindInteractionStorage = newValue }
    }
    @available(iOS 16.0, *)
    package var nativeFindSession: GhosttyNativeFindSession? {
        get { nativeFindSessionStorage as? GhosttyNativeFindSession }
        set { nativeFindSessionStorage = newValue }
    }
    package var ghosttyFindReportedTotal: Int?
    package var ghosttyFindReportedSelectedIndex: Int?
    package let nativeFindDocumentIdentifier = "terminal"
    package let nativeFindOverlay = TerminalNativeFindOverlayView()
    package var nativeFindDecorations: [TerminalNativeFindDecoration] = [] {
        didSet {
            updateNativeFindOverlay()
        }
    }
    lazy package var directTouchTapRecognizer: UITapGestureRecognizer = {
        let recognizer = UITapGestureRecognizer(
            target: self,
            action: #selector(handleDirectTouchTap(_:))
        )
        recognizer.numberOfTapsRequired = 1
        recognizer.numberOfTouchesRequired = 1
        recognizer.cancelsTouchesInView = false
        recognizer.allowedTouchTypes = [
            NSNumber(value: UITouch.TouchType.direct.rawValue)
        ]
        return recognizer
    }()
    lazy package var nativeSelectionLongPressRecognizer: UILongPressGestureRecognizer = {
        let recognizer = UILongPressGestureRecognizer(
            target: self,
            action: #selector(handleNativeSelectionLongPress(_:))
        )
        recognizer.minimumPressDuration = 0.2
        recognizer.cancelsTouchesInView = false
        recognizer.allowedTouchTypes = [
            NSNumber(value: UITouch.TouchType.direct.rawValue)
        ]
        recognizer.delegate = self
        return recognizer
    }()
    lazy package var scrollRecognizer: UIPanGestureRecognizer = {
        let recognizer = UIPanGestureRecognizer(
            target: self,
            action: #selector(handlePanGesture(_:))
        )
        recognizer.maximumNumberOfTouches = 1
        recognizer.requiresExclusiveTouchType = false
        recognizer.allowedTouchTypes = [
            NSNumber(value: UITouch.TouchType.direct.rawValue),
            NSNumber(value: UITouch.TouchType.indirectPointer.rawValue),
        ]
        if #available(iOS 13.4, *) {
            recognizer.allowedScrollTypesMask = .all
        }
        return recognizer
    }()
    private lazy var pointerHoverRecognizer: UIHoverGestureRecognizer = {
        UIHoverGestureRecognizer(target: self, action: #selector(handlePointerHover(_:)))
    }()
    lazy package var pinchRecognizer: UIPinchGestureRecognizer = {
        let recognizer = UIPinchGestureRecognizer(
            target: self,
            action: #selector(handlePinchGesture(_:))
        )
        recognizer.requiresExclusiveTouchType = false
        recognizer.allowedTouchTypes = [
            NSNumber(value: UITouch.TouchType.direct.rawValue)
        ]
        return recognizer
    }()
    private var editMenuInteractionStorage: AnyObject?
    @available(iOS 16.0, *)
    package var editMenuInteraction: UIEditMenuInteraction? {
        get { editMenuInteractionStorage as? UIEditMenuInteraction }
        set { editMenuInteractionStorage = newValue }
    }
    weak package var terminalTitleEditor: UIAlertController?
    package var editMenuPresentation: TerminalEditMenuPresentation = .selection
    package var activePointerButton: TerminalPointerButton?

    /// Observer for config reload notifications
    package var configReloadObserver: NSObjectProtocol?
    package var inputModeObserver: NSObjectProtocol?
    package var hardwareKeyboardObservers: [NSObjectProtocol] = []
    package var hasHardwareKeyboardAttached = false

    // MARK: - Text Input (for spacebar cursor control)
    package var textInputModel = TerminalTextInputModel()
    package var pendingSystemTextInputHardwareKeys: [UIKey] = []
    package var suppressIMEProxyCallbacks = false
    package var renderedIMEPreeditText: String?
    lazy package var imeProxyTextView: TerminalIMEProxyTextView = {
        let textView = TerminalIMEProxyTextView(frame: bounds)
        textView.terminalOwner = self
        textView.backgroundColor = .clear
        textView.isOpaque = false
        textView.isUserInteractionEnabled = true
        textView.autocorrectionType = .no
        textView.autocapitalizationType = .none
        textView.spellCheckingType = .no
        textView.smartQuotesType = .no
        textView.smartDashesType = .no
        textView.smartInsertDeleteType = .no
        if #available(iOS 17.0, *) {
            textView.inlinePredictionType = .no
        }
        return textView
    }()
    lazy package var terminalZoomCommands = makeTerminalZoomKeyCommands(
        action: #selector(handleTerminalZoomCommand(_:))
    )
    lazy package var terminalSplitCommands = makeTerminalSplitKeyCommands(
        action: #selector(handleTerminalSplitCommand(_:))
    )
    lazy package var mouseCaptureCommand: UIKeyCommand = {
        let command = UIKeyCommand(
            input: "m",
            modifierFlags: [.command, .alternate],
            action: #selector(handleMouseCaptureCommand(_:))
        )
        command.discoverabilityTitle = String(localized: "Toggle Mouse Capture")
        if #available(iOS 15.0, *) {
            command.wantsPriorityOverSystemBehavior = true
            command.allowsAutomaticLocalization = false
            command.allowsAutomaticMirroring = false
        }
        return command
    }()
    package var hardwarePressesSentToGhostty: [UInt16: Ghostty.Input.KeyEvent] = [:]
    package var systemTextInputPresses: Set<UInt16> = []
    package var terminalAltOptionKeyCodes: Set<UInt16> = []

    package struct HardwarePressResult {
        package var forwardedToSystem: Set<UIPress> = []
        package var didHandleGhosttyInput = false
    }

    // MARK: - Rendering Components

    package let renderingSetup = GhosttyRenderingSetup()


    // MARK: - Initialization

    /// Create a new Ghostty terminal view
    ///
    /// - Parameters:
    ///   - frame: The initial frame for the view
    ///   - worktreePath: Working directory for the terminal session
    ///   - ghosttyApp: The shared Ghostty app instance (C pointer)
    ///   - appWrapper: The GhosttyRuntime wrapper for surface tracking (optional)
    ///   - paneId: Unique identifier for this pane
    ///   - command: Optional command to run instead of default shell
    ///   - terminalAccessoryInputSnapshot: App-owned keyboard accessory configuration
    ///   - useCustomIO: If true, uses callback backend for custom I/O (SSH clients)
    package init(
        frame: CGRect,
        worktreePath: String,
        ghosttyApp: ghostty_app_t,
        appWrapper: GhosttyRuntime? = nil,
        paneId: String? = nil,
        command: String? = nil,
        terminalAccessoryInputSnapshot: TerminalAccessoryInputSnapshot,
        useCustomIO: Bool = false
    ) {
        self.worktreePath = worktreePath
        self.ghosttyApp = ghosttyApp
        self.ghosttyAppWrapper = appWrapper
        self.paneId = paneId
        self.initialCommand = command
        self.terminalAccessoryInputSnapshot = terminalAccessoryInputSnapshot
        self.useCustomIO = useCustomIO

        // Use a reasonable default size if frame is zero
        let initialFrame = frame.width > 0 && frame.height > 0 ? frame : CGRect(x: 0, y: 0, width: 800, height: 600)
        super.init(frame: initialFrame)
        // The default guide collapses undocked/floating keyboards to the
        // bottom safe area. Track their real frame so stale docked geometry
        // can be rejected during floating/full transitions.
        keyboardLayoutGuide.followsUndockedKeyboard = true

        // Set content scale factor for retina rendering (important before surface
        // creation). Avoid UIScreen.main (stale instance risk on iOS 26); the
        // window's screen scale is applied again in didMoveToWindow.
        self.contentScaleFactor = max(UITraitCollection.current.displayScale, 1)

        setupSurface()
        addSubview(imeProxyTextView)
        zoomIndicatorView.isHidden = true
        zoomIndicatorView.alpha = 0
        zoomIndicatorView.translatesAutoresizingMaskIntoConstraints = false
        addSubview(zoomIndicatorView)
        NSLayoutConstraint.activate([
            zoomIndicatorView.centerXAnchor.constraint(equalTo: centerXAnchor),
            zoomIndicatorView.centerYAnchor.constraint(equalTo: centerYAnchor),
            zoomIndicatorView.widthAnchor.constraint(greaterThanOrEqualToConstant: TerminalZoomPresentation.indicatorMinimumWidth),
            zoomIndicatorView.heightAnchor.constraint(greaterThanOrEqualToConstant: TerminalZoomPresentation.indicatorMinimumHeight)
        ])
        mouseCaptureIndicatorView.isHidden = true
        mouseCaptureIndicatorView.alpha = 0
        mouseCaptureIndicatorView.translatesAutoresizingMaskIntoConstraints = false
        addSubview(mouseCaptureIndicatorView)
        NSLayoutConstraint.activate([
            mouseCaptureIndicatorView.centerXAnchor.constraint(equalTo: centerXAnchor),
            mouseCaptureIndicatorView.centerYAnchor.constraint(equalTo: centerYAnchor),
            mouseCaptureIndicatorView.widthAnchor.constraint(greaterThanOrEqualToConstant: TerminalZoomPresentation.indicatorMinimumWidth),
            mouseCaptureIndicatorView.heightAnchor.constraint(greaterThanOrEqualToConstant: TerminalZoomPresentation.indicatorMinimumHeight)
        ])
        nativeFindOverlay.frame = bounds
        addSubview(nativeFindOverlay)

        // Setup gesture recognizers with delegate for simultaneous recognition
        directTouchTapRecognizer.delegate = self
        scrollRecognizer.delegate = self
        pinchRecognizer.delegate = self
        directTouchTapRecognizer.require(toFail: nativeSelectionLongPressRecognizer)
        addGestureRecognizer(nativeSelectionLongPressRecognizer)
        imeProxyTextView.addGestureRecognizer(directTouchTapRecognizer)
        addGestureRecognizer(scrollRecognizer)
        addGestureRecognizer(pointerHoverRecognizer)
        addGestureRecognizer(pinchRecognizer)
        isUserInteractionEnabled = true

        setupNativeTextSelectionInteractions()
        setupNativeFindInteraction()
        if #available(iOS 16.0, *) {
            let editMenuInteraction = UIEditMenuInteraction(delegate: self)
            addInteraction(editMenuInteraction)
            self.editMenuInteraction = editMenuInteraction
        }

        setupConfigReloadObservation()
        setupInputModeObservation()
        registerColorSchemeObserver()
        setupHardwareKeyboardObservation()
    }

    required package init?(coder: NSCoder) {
        fatalError("init(coder:) not supported")
    }

    isolated deinit {
        cleanup()
    }

    /// Explicitly cleanup the terminal before removal from view hierarchy.
    /// Call this in dismantleUIView to ensure proper cleanup.

    // MARK: - Layer Type
    // On iOS, Ghostty adds its own IOSurfaceLayer as a sublayer of the view's
    // existing CALayer. Keep the default layer type to avoid CAMetalLayer
    // interfering with sublayer rendering/compositing.




    // MARK: - UIView Overrides

    package var acceptsTerminalInput = true {
        didSet {
            if !acceptsTerminalInput {
                cancelTrackedHardwareInput()
            }
        }
    }
    package var keyboardFocusPolicy = TerminalKeyboardFocusPolicy()
    package var suppressDirectTouchKeyboardFocusUntil = Date.distantPast
    package var suppressAccessoryForMissingSoftwareKeyboard = false
    package let hiddenKeyboardInputView = TerminalSuppressedKeyboardInputView()
    #if DEBUG
    package enum KeyboardUITestSoftwareKeyboardFailure: Equatable {
        case none
        case untilSessionRebuild
    }

    package var keyboardHideRequestCount = 0
    package var keyboardInputSessionRebuildCount = 0
    package var keyboardInputViewReloadCount = 0
    package var keyboardUITestHardwareKeyboardOverride: Bool?
    package var keyboardUITestSoftwareKeyboardFailure = KeyboardUITestSoftwareKeyboardFailure.none
    #endif
    package var onWindowAttachmentChange: ((Bool) -> Void)?
    /// Reports terminal touches; isFocusTap is true for the plain
    /// tap-to-focus gesture, which also restores a user-hidden keyboard.
    package var onTerminalDirectTouch: ((_ isFocusTap: Bool) -> Void)?
    package var onKeyboardBrowseModeChange: ((Bool) -> Void)?
    package var onKeyboardAccessoryHideRequested: (() -> Void)?
    package var onFindNavigatorVisibilityChange: ((Bool) -> Void)?
    package var findNavigatorLifecycle = TerminalFindNavigatorLifecycle()

    // MARK: - Scroll Gesture

    /// Scroll speed multiplier for iOS touch scrolling
    static package let scrollMultiplier: Double = 1.5
    static package let selectionAutoscrollEdgeInset: Double = 56
    static package let selectionAutoscrollMaximumDelta: Double = 12

    /// Momentum deceleration rate (0.0-1.0, higher = slower deceleration)
    static package let momentumDeceleration: Double = 0.92

    /// Minimum velocity to trigger momentum scrolling
    static package let minimumMomentumVelocity: Double = 50.0

    /// Display link for momentum animation
    package var momentumDisplayLink: CADisplayLink?
    package var momentumVelocity: CGPoint = .zero
    package var momentumPhase: Ghostty.Input.Momentum = .none
    package var selectionAutoscrollDisplayLink: CADisplayLink?
    package var selectionAutoscrollLocation: CGPoint?
    package var selectionAutoscrollMods: Ghostty.Input.Mods = []

}

#endif
