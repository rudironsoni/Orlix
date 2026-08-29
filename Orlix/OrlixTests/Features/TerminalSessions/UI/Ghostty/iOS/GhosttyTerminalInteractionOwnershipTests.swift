#if os(iOS)
import CoreGraphics
import Foundation
import Testing
import UIKit
@testable import Orlix

@Suite(.serialized)
@MainActor
struct GhosttyTerminalInteractionOwnershipTests {
    @Test
    func mouseReportingOverrideControlsTheActualGhosttySurface() throws {
        let app = GhosttyRuntime()
        defer { app.cleanup() }
        let appHandle = try #require(app.app)
        let terminal = GhosttyTerminalView(
            frame: CGRect(x: 0, y: 0, width: 800, height: 600),
            worktreePath: FileManager.default.currentDirectoryPath,
            ghosttyApp: appHandle,
            appWrapper: app,
            paneId: "mouse-reporting-override",
            terminalAccessoryInputSnapshot: TerminalAccessoryInputSnapshot(
                profile: .defaultValue(lastWriterDeviceId: "mouse-reporting-override-test"),
                showsDismissKeyboardButton: true
            ),
            useCustomIO: true
        )
        defer { terminal.cleanup() }
        terminal.setupWriteCallback()
        terminal.feedData(Data("\u{1B}[?1000h\u{1B}[?1006h".utf8))
        let surface = try #require(terminal.surface)
        #expect(surface.mouseCaptured)

        var observedStates: [Bool] = []
        terminal.onMouseReportingSuppressionChange = { observedStates.append($0) }

        #expect(terminal.setMouseReportingSuppressed(true))
        #expect(terminal.isMouseReportingSuppressed)
        #expect(!surface.mouseCaptured)
        #expect(observedStates == [true])

        #expect(terminal.setMouseReportingSuppressed(true))
        #expect(observedStates == [true])

        #expect(terminal.setMouseReportingSuppressed(false))
        #expect(!terminal.isMouseReportingSuppressed)
        #expect(surface.mouseCaptured)
        #expect(observedStates == [true, false])

        terminal.cleanup()
        #expect(!terminal.setMouseReportingSuppressed(true))
        #expect(!terminal.isMouseReportingSuppressed)
        #expect(observedStates == [true, false])
    }

    @Test
    func mouseCaptureAccessoryAndKeyboardCommandToggleTheSameSurfaceState() throws {
        let app = GhosttyRuntime()
        defer { app.cleanup() }
        let appHandle = try #require(app.app)
        let terminal = GhosttyTerminalView(
            frame: CGRect(x: 0, y: 0, width: 800, height: 600),
            worktreePath: FileManager.default.currentDirectoryPath,
            ghosttyApp: appHandle,
            appWrapper: app,
            paneId: "mouse-reporting-controls",
            terminalAccessoryInputSnapshot: TerminalAccessoryInputSnapshot(
                profile: .defaultValue(lastWriterDeviceId: "mouse-reporting-controls-test"),
                showsDismissKeyboardButton: true
            ),
            useCustomIO: true
        )
        defer { terminal.cleanup() }
        terminal.setupWriteCallback()
        terminal.feedData(Data("\u{1B}[?1000h\u{1B}[?1006h".utf8))

        let toolbar = try #require(terminal.resolvedInputAccessoryView() as? TerminalInputAccessoryView)
        let button = try #require(findView(
            in: toolbar,
            accessibilityIdentifier: "orlix.keyboard.accessory.system.mouseCapture"
        ) as? UIButton)
        button.sendActions(for: .touchUpInside)
        #expect(terminal.isMouseReportingSuppressed)
        #expect(button.isSelected)
        #expect(!terminal.mouseCaptureIndicatorView.isHidden)
        #expect(terminal.mouseCaptureIndicatorView.accessibilityLabel == "Mouse Capture Off")

        let command = try #require(terminal.keyCommands?.first {
            $0.input == "m" && $0.modifierFlags == [.command, .alternate]
        })
        terminal.handleMouseCaptureCommand(command)
        #expect(!terminal.isMouseReportingSuppressed)
        #expect(!button.isSelected)
    }

    @Test
    func splitCommandsRouteFromTextInputOwnerToTerminalResponder() throws {
        let app = GhosttyRuntime()
        defer { app.cleanup() }
        let appHandle = try #require(app.app)
        let terminal = GhosttyTerminalView(
            frame: CGRect(x: 0, y: 0, width: 800, height: 600),
            worktreePath: FileManager.default.currentDirectoryPath,
            ghosttyApp: appHandle,
            appWrapper: app,
            paneId: "split-command-ownership",
            terminalAccessoryInputSnapshot: TerminalAccessoryInputSnapshot(
                profile: .defaultValue(lastWriterDeviceId: "split-command-ownership-test"),
                showsDismissKeyboardButton: true
            ),
            useCustomIO: true
        )
        defer { terminal.cleanup() }

        let presenter = UIViewController()
        let windowScene = try #require(
            UIApplication.shared.connectedScenes
                .compactMap { $0 as? UIWindowScene }
                .first
        )
        let previousKeyWindow = windowScene.windows.first(where: \.isKeyWindow)
        let window = UIWindow(windowScene: windowScene)
        window.frame = CGRect(x: 0, y: 0, width: 800, height: 600)
        window.rootViewController = presenter
        window.makeKeyAndVisible()
        defer {
            window.isHidden = true
            previousKeyWindow?.makeKey()
        }
        presenter.view.addSubview(terminal)
        terminal.acceptsTerminalInput = true

        #expect(terminal.becomeFirstResponder())
        #expect(terminal.imeProxyTextView.isFirstResponder)

        let proxySplitCommand = terminal.imeProxyTextView.keyCommands?.first {
            $0.input == "d" && $0.modifierFlags == .command
        }
        #expect(proxySplitCommand == nil)

        let command = try #require(terminal.keyCommands?.first {
            $0.input == "d" && $0.modifierFlags == .command
        })
        let action = try #require(command.action)
        let target = terminal.imeProxyTextView.target(
            forAction: action,
            withSender: command
        )
        #expect((target as AnyObject?) === terminal)

        var routedCommand: TerminalSplitCommand?
        terminal.onPaneKeyboardShortcut = { routedCommand = $0 }
        #expect(
            UIApplication.shared.sendAction(
                action,
                to: nil,
                from: command,
                for: nil
            )
        )
        #expect(routedCommand == .splitRight)
    }

    @Test
    func textInteractionBeginsOnlyForSettledNativeSelection() throws {
        let app = GhosttyRuntime()
        defer { app.cleanup() }
        let appHandle = try #require(app.app)
        let terminal = GhosttyTerminalView(
            frame: CGRect(x: 0, y: 0, width: 800, height: 600),
            worktreePath: FileManager.default.currentDirectoryPath,
            ghosttyApp: appHandle,
            appWrapper: app,
            paneId: "interaction-begin-policy",
            terminalAccessoryInputSnapshot: TerminalAccessoryInputSnapshot(
                profile: .defaultValue(lastWriterDeviceId: "interaction-begin-policy-test"),
                showsDismissKeyboardButton: true
            ),
            useCustomIO: true
        )
        defer { terminal.cleanup() }
        let interaction = try #require(terminal.nativeTextInteraction)

        #expect(!terminal.interactionShouldBegin(interaction, at: .zero))

        terminal.feedData(Data("one two".utf8))
        terminal.refreshNativeSelectionSnapshot()
        let selectionLength = min(3, terminal.nativeSelectionSnapshot.length)
        try #require(selectionLength > 0)
        terminal.nativeSelectionLifecycle.prepare(restoreTerminalInput: false)
        terminal.nativeSelectionLifecycle.beginInteraction(restoreTerminalInput: false)
        _ = terminal.nativeSelectionLifecycle.setSelection(
            NSRange(location: 0, length: selectionLength)
        )
        _ = terminal.nativeSelectionLifecycle.endInteraction()
        #expect(
            terminal.nativeSelectedRange == NSRange(location: 0, length: selectionLength)
        )

        #expect(terminal.interactionShouldBegin(interaction, at: .zero))
        terminal.nativeSelectionLifecycle.cancel()
        #expect(terminal.nativeSelectionLifecycle.phase == .inactive)
    }

    @Test
    func terminalInteractionsAreInstalledAndReleased() async throws {
        let app = GhosttyRuntime()
        defer { app.cleanup() }
        let appHandle = try #require(app.app)
        let terminal = GhosttyTerminalView(
            frame: CGRect(x: 0, y: 0, width: 800, height: 600),
            worktreePath: FileManager.default.currentDirectoryPath,
            ghosttyApp: appHandle,
            appWrapper: app,
            paneId: "interaction-ownership",
            terminalAccessoryInputSnapshot: TerminalAccessoryInputSnapshot(
                profile: .defaultValue(lastWriterDeviceId: "interaction-ownership-test"),
                showsDismissKeyboardButton: true
            ),
            useCustomIO: true
        )
        defer { terminal.cleanup() }

        let presenter = UIViewController()
        let window = UIWindow(frame: CGRect(x: 0, y: 0, width: 800, height: 600))
        window.rootViewController = presenter
        window.makeKeyAndVisible()
        presenter.view.addSubview(terminal)

        terminal.terminalContextMenuActions = TerminalContextMenuActions(
            focus: {},
            splitRight: {},
            splitLeft: {},
            splitDown: {},
            splitUp: {},
            currentTitle: { "" },
            setTitle: { _ in }
        )

        let nativeSelection = try #require(terminal.nativeTextInteraction)
        let pointerMenu = try #require(terminal.editMenuInteraction)
        let interactionTextInput = nativeSelection.textInput as AnyObject?

        terminal.layoutIfNeeded()
        #expect(nativeSelection.view === terminal.imeProxyTextView)
        #expect(nativeSelection.textInteractionMode == .editable)
        #expect(interactionTextInput === terminal.imeProxyTextView)
        #expect(interactionTextInput !== terminal)
        #expect(terminal.imeProxyTextView.textInputView === terminal.imeProxyTextView)
        #expect(terminal.imeProxyTextView.frame == terminal.bounds)
        #expect(terminal.imeProxyTextView.isMultipleTouchEnabled)
        #expect(terminal.pinchRecognizer.view === terminal)
        #expect(terminal.directTouchTapRecognizer.view === terminal.imeProxyTextView)
        #expect(terminal.nativeSelectionLongPressRecognizer.view === terminal)
        let proxyTapCounts = terminal.imeProxyTextView.gestureRecognizers?
            .compactMap { ($0 as? UITapGestureRecognizer)?.numberOfTapsRequired }
            ?? []
        #expect(proxyTapCounts.contains(1))
        #expect(proxyTapCounts.contains(2))
        #expect(proxyTapCounts.contains(3))
        #expect(
            terminal.imeProxyTextView.point(
                inside: CGPoint(x: terminal.bounds.midX, y: terminal.bounds.midY),
                with: nil
            )
        )
        #expect(pointerMenu.view === terminal)
        #expect(
            !terminal.gestureRecognizer(
                terminal.pinchRecognizer,
                shouldRecognizeSimultaneouslyWith: terminal.scrollRecognizer
            )
        )

        terminal.keyboardUITestSetHardwareKeyboardAttached(false)

        terminal.nativeSelectionSnapshot = TerminalNativeTextSnapshot(
            lines: ["one two"],
            cellSize: CGSize(width: 10, height: 20),
            columns: 7
        )
        let textInput = terminal.imeProxyTextView
        #expect(terminal.terminalInputConfiguration == .systemWithAccessory)
        #expect(textInput.inputView == nil)
        #expect(!terminal.shouldHideKeyboardAccessoryBar)
        #expect(textInput.inputAccessoryView != nil)
        #expect(!terminal.keyboardCoordinatorDiagnosticSnapshot().isSoftwareKeyboardSuppressed)

        #expect(terminal.focusTerminalInputWithoutShowingSoftwareKeyboard())
        #expect(terminal.isKeyboardInBrowseMode)
        #expect(terminal.terminalInputConfiguration == .suppressed)
        #expect(textInput.inputView === terminal.hiddenKeyboardInputView)
        #expect(textInput.inputAccessoryView == nil)

        #expect(terminal.forceSoftwareKeyboardInput())
        #expect(!terminal.isKeyboardInBrowseMode)
        #expect(terminal.terminalInputConfiguration == .systemWithAccessory)
        #expect(textInput.inputView == nil)
        #expect(textInput.inputAccessoryView != nil)

        terminal.nativeSelectionLifecycle.prepare(restoreTerminalInput: true)
        terminal.nativeSelectionLifecycle.beginInteraction(restoreTerminalInput: true)
        _ = terminal.nativeSelectionLifecycle.setSelection(
            NSRange(location: 0, length: 3)
        )
        _ = terminal.nativeSelectionLifecycle.endInteraction()

        #expect(textInput.documentMode == .nativeSelection)
        #expect(textInput.text(in: try #require(textInput.selectedTextRange)) == "one")
        #expect(terminal.terminalInputConfiguration == .systemWithAccessory)
        #expect(textInput.inputView == nil)
        #expect(!terminal.shouldHideKeyboardAccessoryBar)
        #expect(textInput.inputAccessoryView != nil)
        #expect(!terminal.keyboardCoordinatorDiagnosticSnapshot().isSoftwareKeyboardSuppressed)
        let selectionMenuTitles = Set(
            terminal.nativeSelectionMenuElements().compactMap {
                ($0 as? UIAction)?.title
            }
        )
        #expect(selectionMenuTitles.contains(String(localized: "Copy")))
        #expect(selectionMenuTitles.contains(String(localized: "Paste")))
        #expect(selectionMenuTitles.contains(String(localized: "Select All")))
        #expect(selectionMenuTitles.contains(String(localized: "Find")))

        let minimumPosition = TerminalNativeTextPosition(offset: Int.min)
        let maximumPosition = TerminalNativeTextPosition(offset: Int.max)
        #expect(textInput.offset(from: minimumPosition, to: maximumPosition) == 7)
        let extremeRange = try #require(
            textInput.textRange(from: minimumPosition, to: maximumPosition)
                as? TerminalNativeTextRange
        )
        #expect(extremeRange.nsRange == NSRange(location: 0, length: Int.max))
        #expect(
            textInput.characterOffset(of: maximumPosition, within: extremeRange) == 7
        )

        terminal.clearNativeSelectionStateForTerminalInput()
        #expect(textInput.documentMode == .terminalInput)

        let titleEditor = UIAlertController(
            title: "Test",
            message: nil,
            preferredStyle: .alert
        )
        terminal.terminalTitleEditor = titleEditor
        presenter.present(titleEditor, animated: false)
        #expect(presenter.presentedViewController === titleEditor)

        terminal.cleanup()
        #expect(nativeSelection.view == nil)
        let titleEditorDismissed = await waitUntil {
            titleEditor.presentingViewController == nil
        }

        #expect(terminal.editMenuInteraction == nil)
        #expect(pointerMenu.view == nil)
        #expect(terminal.terminalTitleEditor == nil)
        #expect(titleEditorDismissed)
        #expect(terminal.terminalContextMenuActions == nil)
    }

    private func waitUntil(_ condition: @MainActor () -> Bool) async -> Bool {
        let clock = ContinuousClock()
        let deadline = clock.now.advanced(by: .seconds(1))
        while clock.now < deadline {
            if condition() {
                return true
            }
            await Task.yield()
        }
        return condition()
    }
}

@MainActor
private func findView(
    in root: UIView,
    accessibilityIdentifier: String
) -> UIView? {
    if root.accessibilityIdentifier == accessibilityIdentifier {
        return root
    }
    for subview in root.subviews {
        if let match = findView(
            in: subview,
            accessibilityIdentifier: accessibilityIdentifier
        ) {
            return match
        }
    }
    return nil
}
#endif
