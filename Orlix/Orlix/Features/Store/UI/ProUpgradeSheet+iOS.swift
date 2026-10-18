#if os(iOS)
import SwiftUI
import UIKit

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package extension ProUpgradeSheet {
    package func platformBody<Content: View>(
        sheetContent: Content,
        source: PaywallSource,
        onClose: @escaping () -> Void
    ) -> some View {
        NavigationView {
            sheetContent
                .navigationTitle("")
                .navigationBarTitleDisplayMode(.inline)
                .toolbar {
                    ToolbarItem(placement: .principal) {
                        VStack(spacing: 1) {
                            Text(source.paywallTitle)
                                .font(.headline)
                                .lineLimit(1)
                                .minimumScaleFactor(0.8)
                            Text(source.paywallSubtitle)
                                .font(.caption2)
                                .foregroundStyle(.secondary)
                                .lineLimit(1)
                                .minimumScaleFactor(0.8)
                        }
                    }

                    ToolbarItem(placement: .topBarTrailing) {
                        Button(action: onClose) {
                            Image(systemName: "xmark")
                                .font(.system(size: 16, weight: .semibold))
                                .symbolRenderingMode(.hierarchical)
                                .foregroundStyle(.secondary)
                        }
                    }
                }
        }
        .adaptiveSoftScrollEdges()
    }

    package func platformSheetLayout<Content: View, Footer: View>(
        content: Content,
        footer: Footer,
        source: PaywallSource
    ) -> some View {
        VStack(spacing: 0) {
            ScrollView {
                content
                    .padding(.horizontal, 20)
                    .padding(.top, 18)
                    .padding(.bottom, 18)
            }
            .orlixVisibleScrollIndicators()

            footer
        }
        .background(sheetBackground.ignoresSafeArea())
    }

    package func openSubscriptionManagement() {
        showManageSubscription = true
    }

    package var sheetBackground: Color {
        Color(uiColor: .systemGroupedBackground)
    }
}

package extension ProUpgradePresentationModifier {
    package func platformBody(content: Content) -> some View {
        content
            .sheet(isPresented: $isPresented) {
                ProUpgradeSheet(source: source)
                    .adaptiveSoftScrollEdges()
            }
    }
}

package var paywallTableGridColor: Color {
    Color.primary.opacity(0.10)
}

package var paywallCardFillColor: Color {
    Color(uiColor: .secondarySystemGroupedBackground)
}

package var paywallCardBorderColor: Color {
    Color.primary.opacity(0.10)
}
#endif
