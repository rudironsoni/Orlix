import SwiftUI

private struct SSHHostKeyTrustAlertModifier: ViewModifier {
    package let request: ServerSecurityApprovalRequest?
    @Binding package var isPresented: Bool
    package let onCancel: (ServerSecurityApprovalRequest) -> Void
    package let onApprove: (ServerSecurityApprovalRequest) -> Void

    private var presentation: SSHHostKeyTrustPresentation? {
        request.map(SSHHostKeyTrustPresentation.init)
    }

    package func body(content: Content) -> some View {
        content.alert(
            presentation?.title ?? String(localized: "Trust SSH Host?"),
            isPresented: $isPresented,
            presenting: request
        ) { request in
            let presentation = SSHHostKeyTrustPresentation(request: request)
            Button("Cancel", role: .cancel) {
                onCancel(request)
            }
            if presentation.isDestructive == false {
                Button(
                    presentation.approvalButtonTitle
                ) {
                    onApprove(request)
                }
            } else {
                Button(
                    presentation.approvalButtonTitle,
                    role: .destructive
                ) {
                    onApprove(request)
                }
            }
        } message: { request in
            Text(SSHHostKeyTrustPresentation(request: request).message)
        }
    }
}

package extension View {
    package func sshHostKeyTrustAlert(
        request: ServerSecurityApprovalRequest?,
        isPresented: Binding<Bool>,
        onCancel: @escaping (ServerSecurityApprovalRequest) -> Void,
        onApprove: @escaping (ServerSecurityApprovalRequest) -> Void
    ) -> some View {
        modifier(
            SSHHostKeyTrustAlertModifier(
                request: request,
                isPresented: isPresented,
                onCancel: onCancel,
                onApprove: onApprove
            )
        )
    }
}
