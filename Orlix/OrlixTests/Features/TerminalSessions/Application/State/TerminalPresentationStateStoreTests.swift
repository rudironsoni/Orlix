import Foundation
import Testing
@testable import Orlix

@MainActor
struct TerminalPresentationStateStoreTests {
    @Test
    func splitZoomIsEphemeralAndClearsWithItsTab() {
        let store = TerminalPresentationStateStore()
        let tabId = UUID()

        store.toggleSplitZoom(for: tabId)
        #expect(store.splitZoomedTabIds == [tabId])

        store.toggleSplitZoom(for: tabId)
        #expect(store.splitZoomedTabIds.isEmpty)

        store.toggleSplitZoom(for: tabId)
        store.removeTab(tabId)
        #expect(store.splitZoomedTabIds.isEmpty)
    }

    @Test
    func resetClearsTemporaryPresentationState() {
        let store = TerminalPresentationStateStore()
        store.toggleSplitZoom(for: UUID())
        #if os(iOS)
        let paneId = UUID()
        store.setTerminalFindNavigatorVisible(true, for: paneId)
        store.applyVoiceEvent(.recordingStarted, for: paneId)
        store.setMouseReportingSuppressed(true, for: paneId)
        #endif

        store.reset()

        #expect(store.splitZoomedTabIds.isEmpty)
        #if os(iOS)
        #expect(store.terminalFindNavigatorVisibleByPane.isEmpty)
        #expect(store.terminalVoicePresentationByPane.isEmpty)
        #expect(store.mouseReportingSuppressedPaneIds.isEmpty)
        #endif
    }

    #if os(iOS)
    @Test
    func paneCleanupRemovesTemporaryPresentationState() {
        let store = TerminalPresentationStateStore()
        let paneId = UUID()

        store.setTerminalFindNavigatorVisible(true, for: paneId)
        store.applyVoiceEvent(.recordingStarted, for: paneId)
        store.setMouseReportingSuppressed(true, for: paneId)
        #expect(store.terminalFindNavigatorVisibleByPane[paneId] == true)
        #expect(store.voicePresentation(for: paneId) == .recording)
        #expect(store.isMouseReportingSuppressed(for: paneId))

        store.removePane(paneId)
        #expect(store.terminalFindNavigatorVisibleByPane[paneId] == nil)
        #expect(store.terminalVoicePresentationByPane[paneId] == nil)
        #expect(store.voicePresentation(for: paneId) == .idle)
        #expect(!store.isMouseReportingSuppressed(for: paneId))
    }

    @Test
    func mouseReportingSuppressionIsIndependentByPane() {
        let store = TerminalPresentationStateStore()
        let firstPaneId = UUID()
        let secondPaneId = UUID()

        store.setMouseReportingSuppressed(true, for: firstPaneId)
        #expect(store.isMouseReportingSuppressed(for: firstPaneId))
        #expect(!store.isMouseReportingSuppressed(for: secondPaneId))

        store.setMouseReportingSuppressed(false, for: firstPaneId)
        #expect(!store.isMouseReportingSuppressed(for: firstPaneId))
    }

    @Test
    func idleVoicePresentationIsNotStored() {
        let store = TerminalPresentationStateStore()
        let paneId = UUID()

        store.applyVoiceEvent(.recordingStarted, for: paneId)
        store.applyVoiceEvent(.recordingStopped, for: paneId)

        #expect(store.terminalVoicePresentationByPane[paneId] == nil)
        #expect(store.voicePresentation(for: paneId) == .idle)
    }
    #endif
}
