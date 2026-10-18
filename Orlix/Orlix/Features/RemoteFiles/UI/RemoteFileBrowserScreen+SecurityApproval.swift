import SwiftUI

#if ORLIX_MODULAR_APP
import OrlixModels
import OrlixSSH
#endif
package extension RemoteFileBrowserScreen {
    package func securityApprovalPresentation<Content: View>(_ content: Content) -> some View {
        content
            .sshHostKeyTrustAlert(
                request: effectiveSecurityApprovalRequest,
                isPresented: hostKeyApprovalBinding,
                onCancel: { _ in cancelSecurityApproval() },
                onApprove: { _ in approveHostKeyAndRetry() }
            )
    }

    package func remoteOperationErrorMessage(for error: Error) -> String {
        RemoteFileBrowserError.map(error).errorDescription ?? error.localizedDescription
    }

    @MainActor
    package func presentOperationError(
        _ error: Error,
        retry: (@MainActor () -> Void)? = nil,
        onCancellation: (@MainActor () -> Void)? = nil
    ) {
        if presentSecurityApproval(
            for: error,
            retry: retry,
            onCancellation: onCancellation
        ) {
            return
        }
        presentation = .operationError(remoteOperationErrorMessage(for: error))
    }

    package var hostKeyApprovalChallenge: KnownHostsManager.Challenge? {
        guard case .hostKey(let challenge) = effectiveSecurityApprovalRequest else {
            return nil
        }
        return challenge
    }

    package var effectiveSecurityApprovalRequest: ServerSecurityApprovalRequest? {
        operationCoordinator.securityApprovalRequest
    }

    package var hostKeyApprovalBinding: Binding<Bool> {
        Binding(get: { hostKeyApprovalChallenge != nil }, set: { _ in })
    }

    @discardableResult
    @MainActor
    package func presentSecurityApproval(
        for error: Error,
        retry: (@MainActor () -> Void)?,
        onCancellation: (@MainActor () -> Void)? = nil
    ) -> Bool {
        guard effectiveSecurityApprovalRequest == nil else { return false }
        guard operationCoordinator.requestSecurityApproval(
            for: error,
            retry: retry ?? {},
            onCancellation: onCancellation ?? {}
        ) else {
            if case .hostKeyApprovalRequired = RemoteFileBrowserError.map(error) {
                presentation = .operationError(ServerSecurityApprovalError.unavailable.localizedDescription)
                return true
            }
            return false
        }
        return true
    }

    @MainActor
    package func presentDirectorySecurityApprovalIfNeeded() {
        guard let error = browser.error(for: fileTab) else { return }
        _ = presentSecurityApproval(for: error, retry: {
            Task {
                await browser.refresh(server: server, tab: fileTab)
            }
        })
    }

    @MainActor
    package func presentViewerSecurityApprovalIfNeeded() {
        guard let error = browser.viewerError(for: fileTab),
              let entry = snapshot.selectedEntry else { return }
        _ = presentSecurityApproval(for: error, retry: {
            Task {
                await browser.loadPreview(for: entry, in: fileTab, server: server)
            }
        })
    }

    @MainActor
    package func cancelSecurityApproval() {
        guard effectiveSecurityApprovalRequest != nil else { return }
        if let error = operationCoordinator.cancelSecurityRequest() {
            presentation = .operationError(error.localizedDescription)
        }
    }

    @MainActor
    package func approveHostKeyAndRetry() {
        guard effectiveSecurityApprovalRequest != nil else { return }
        if let error = operationCoordinator.approveSecurityRequest() {
            presentation = .operationError(error.localizedDescription)
        }
    }
}
