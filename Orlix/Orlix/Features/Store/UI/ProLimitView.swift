import SwiftUI
#if ORLIX_MODULAR_APP
import OrlixModels
#endif

// MARK: - Pro Limit Banner

package struct ProLimitBanner: View {
    package let title: String
    package let message: String
    package let action: () -> Void

    package var body: some View {
        HStack(spacing: 12) {
            Image(systemName: "star.fill")
                .foregroundStyle(.orange)
                .font(.title3)

            VStack(alignment: .leading, spacing: 2) {
                Text(title)
                    .font(.subheadline)
                    .fontWeight(.semibold)
                Text(message)
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            Spacer()

            Button("Upgrade") {
                action()
            }
            .buttonStyle(.borderedProminent)
            .controlSize(.small)
        }
    }
}

// MARK: - Pro Feature Lock

package struct ProFeatureLock: View {
    package let feature: String
    package let description: String
    @Binding package var showUpgrade: Bool

    package var body: some View {
        VStack(spacing: 16) {
            Image(systemName: "lock.fill")
                .font(.largeTitle)
                .foregroundStyle(.secondary)

            Text(String(format: String(localized: "%@ is a Pro feature"), feature))
                .font(.headline)

            Text(description)
                .font(.subheadline)
                .foregroundStyle(.secondary)
                .multilineTextAlignment(.center)

            Button {
                showUpgrade = true
            } label: {
                Label("Upgrade to Pro", systemImage: "star.fill")
            }
            .buttonStyle(.borderedProminent)
        }
        .padding()
        .frame(maxWidth: .infinity)
    }
}

// MARK: - Limit Reached Alert

package struct LimitReachedAlert: ViewModifier {
    package let limitType: LimitType
    @Binding package var isPresented: Bool
    @EnvironmentObject private var serverManager: ServerManager
    @EnvironmentObject private var storeManager: StoreManager
    @State private var showUpgrade = false

    package enum LimitType {
        case servers
        case workspaces
        case tabs
        case fileTabs

        package var title: String {
            switch self {
            case .servers: return String(localized: "Server Limit Reached")
            case .workspaces: return String(localized: "Workspace Limit Reached")
            case .tabs: return String(localized: "Tab Limit Reached")
            case .fileTabs: return String(localized: "File Tab Limit Reached")
            }
        }

        package func message(serverLimit: Int) -> String {
            switch self {
            case .servers:
                return String(
                    format: String(localized: "You've reached the free limit of %@. Pro unlocks unlimited servers, workspaces, simultaneous connections, and split panes."),
                    FreeTierLimitPresentation.serverCountDescription(serverLimit)
                )
            case .workspaces:
                return String(format: String(localized: "You've reached the free limit of %lld workspace. Pro unlocks unlimited workspaces, servers, simultaneous connections, and split panes."), Int64(FreeTierLimits.maxWorkspaces))
            case .tabs:
                return String(format: String(localized: "The free plan runs %lld connection at a time. Pro unlocks simultaneous connections, unlimited servers, and split panes."), Int64(FreeTierLimits.maxTabs))
            case .fileTabs:
                return String(localized: "The free plan opens 1 file tab at a time. Pro unlocks multiple file tabs, simultaneous connections, and unlimited servers.")
            }
        }

        package var paywallSource: PaywallSource {
            switch self {
            case .servers: return .serverLimit
            case .workspaces: return .workspaceLimit
            case .tabs: return .tabLimit
            case .fileTabs: return .fileTabLimit
            }
        }
    }

    package func body(content: Content) -> some View {
        content
            .alert(limitType.title, isPresented: $isPresented) {
                Button("Upgrade to Pro") {
                    showUpgrade = true
                }
                .keyboardShortcut(.defaultAction)
                Button("Cancel", role: .cancel) {}
            } message: {
                Text(limitType.message(serverLimit: serverManager.freeServerLimit))
            }
            .onChange(of: isPresented) { presented in
                if presented {
                    trackLimitHit()
                }
            }
            .proUpgradePresentation(isPresented: $showUpgrade, source: limitType.paywallSource)
    }

    private func trackLimitHit() {
        let current: Int
        let limit: Int

        switch limitType {
        case .servers:
            current = serverManager.servers.count
            limit = serverManager.freeServerLimit
        case .workspaces:
            current = serverManager.workspaces.count
            limit = FreeTierLimits.maxWorkspaces
        case .tabs, .fileTabs:
            current = FreeTierLimits.maxTabs
            limit = FreeTierLimits.maxTabs
        }

        storeManager.noteLimitHit(
            source: limitType.paywallSource,
            generation: serverManager.freePlanGeneration,
            current: current,
            limit: limit
        )
    }
}

package extension View {
    package func limitReachedAlert(_ limitType: LimitReachedAlert.LimitType, isPresented: Binding<Bool>) -> some View {
        modifier(LimitReachedAlert(limitType: limitType, isPresented: isPresented))
    }
}

// MARK: - Pro Feature Alert

package struct ProFeatureAlert: ViewModifier {
    package let title: String
    package let message: String
    package let source: PaywallSource
    @Binding package var isPresented: Bool
    @State private var showUpgrade = false

    package func body(content: Content) -> some View {
        content
            .alert(title, isPresented: $isPresented) {
                Button("Upgrade to Pro") {
                    showUpgrade = true
                }
                .keyboardShortcut(.defaultAction)
                Button("Cancel", role: .cancel) {}
            } message: {
                Text(message)
            }
            .proUpgradePresentation(isPresented: $showUpgrade, source: source)
    }
}

package extension View {
    package func proFeatureAlert(title: String, message: String, source: PaywallSource = .general, isPresented: Binding<Bool>) -> some View {
        modifier(ProFeatureAlert(title: title, message: message, source: source, isPresented: isPresented))
    }

    package func splitPaneProFeatureAlert(isPresented: Binding<Bool>) -> some View {
        proFeatureAlert(
            title: String(localized: "Split Panes"),
            message: String(localized: "Upgrade to Pro to split terminal panes"),
            source: .splitPane,
            isPresented: isPresented
        )
    }
}


// MARK: - Pro Gate View

package struct ProGateView<Content: View, LockedContent: View>: View {
    @EnvironmentObject private var storeManager: StoreManager
    package let content: () -> Content
    package let lockedContent: () -> LockedContent

    package init(
        @ViewBuilder content: @escaping () -> Content,
        @ViewBuilder lockedContent: @escaping () -> LockedContent
    ) {
        self.content = content
        self.lockedContent = lockedContent
    }

    package var body: some View {
        if storeManager.allowsProFeatures {
            content()
        } else {
            lockedContent()
        }
    }
}

// MARK: - Usage Indicator

package struct UsageIndicator: View {
    package let current: Int
    package let limit: Int
    package let label: String
    @Binding package var showUpgrade: Bool

    package var isAtLimit: Bool { current >= limit }

    package var body: some View {
        HStack {
            Text(label)
                .font(.subheadline)

            Spacer()

            HStack(spacing: 4) {
                Text(String(format: String(localized: "%lld/%lld"), Int64(current), Int64(limit)))
                    .font(.subheadline)
                    .fontWeight(.medium)
                    .foregroundStyle(isAtLimit ? .orange : .secondary)

                if isAtLimit {
                    Button {
                        showUpgrade = true
                    } label: {
                        ProBadge(compact: true)
                    }
                    .buttonStyle(.plain)
                }
            }
        }
    }
}

// MARK: - Locked Item Alert (for downgraded users)

package struct LockedItemAlert: ViewModifier {
    package let itemType: ItemType
    package let itemName: String
    @Binding package var isPresented: Bool
    @EnvironmentObject private var serverManager: ServerManager
    @State private var showUpgrade = false

    package enum ItemType {
        case server
        case workspace

        package var title: String {
            switch self {
            case .server: return String(localized: "Server Locked")
            case .workspace: return String(localized: "Workspace Locked")
            }
        }

        package func message(serverLimit: Int) -> String {
            switch self {
            case .server:
                return String(
                    format: String(localized: "This server exceeds your free plan limit of %@. Renew your Pro subscription to access all your servers."),
                    FreeTierLimitPresentation.serverCountDescription(serverLimit)
                )
            case .workspace:
                return String(format: String(localized: "This workspace exceeds your free plan limit of %lld workspace. Renew your Pro subscription to access all your workspaces."), Int64(FreeTierLimits.maxWorkspaces))
            }
        }

        package var paywallSource: PaywallSource {
            switch self {
            case .server: return .serverLimit
            case .workspace: return .workspaceLimit
            }
        }
    }

    package func body(content: Content) -> some View {
        content
            .alert(itemType.title, isPresented: $isPresented) {
                Button("Renew Pro") {
                    showUpgrade = true
                }
                Button("Cancel", role: .cancel) {}
            } message: {
                Text(String(format: String(localized: "\"%@\" %@"), itemName, itemType.message(serverLimit: serverManager.freeServerLimit)))
            }
            .proUpgradePresentation(isPresented: $showUpgrade, source: itemType.paywallSource)
    }
}

package extension View {
    package func lockedItemAlert(_ itemType: LockedItemAlert.ItemType, itemName: String, isPresented: Binding<Bool>) -> some View {
        modifier(LockedItemAlert(itemType: itemType, itemName: itemName, isPresented: isPresented))
    }
}


// MARK: - Downgrade Banner (shown when user has locked items)

package struct DowngradeBanner: View {
    package let lockedServers: Int
    package let lockedWorkspaces: Int
    package let action: () -> Void

    private var message: String {
        var parts: [String] = []
        if lockedServers > 0 {
            let serverText = lockedServers == 1
                ? String(format: String(localized: "%lld server"), lockedServers)
                : String(format: String(localized: "%lld servers"), lockedServers)
            parts.append(serverText)
        }
        if lockedWorkspaces > 0 {
            let workspaceText = lockedWorkspaces == 1
                ? String(format: String(localized: "%lld workspace"), lockedWorkspaces)
                : String(format: String(localized: "%lld workspaces"), lockedWorkspaces)
            parts.append(workspaceText)
        }
        let conjunction = String(localized: " and ")
        let joined = parts.joined(separator: conjunction)
        return String(format: String(localized: "%@ locked"), joined)
    }

    package var body: some View {
        HStack(spacing: 12) {
            Image(systemName: "lock.fill")
                .foregroundStyle(.orange)
                .font(.title3)

            VStack(alignment: .leading, spacing: 2) {
                Text("Subscription Expired")
                    .font(.subheadline)
                    .fontWeight(.semibold)
                Text(message)
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            Spacer()

            Button("Renew") {
                action()
            }
            .buttonStyle(.borderedProminent)
            .controlSize(.small)
        }
    }
}

// MARK: - Preview

#Preview("Pro Limit Banner") {
    ProLimitBanner(
        title: "Server Limit Reached",
        message: "Upgrade to Pro for unlimited servers"
    ) {}
    .padding()
}

#Preview("Pro Feature Lock") {
    ProFeatureLock(
        feature: "Custom Environments",
        description: "Create custom environments to organize your servers beyond Production, Staging, and Development.",
        showUpgrade: .constant(false)
    )
}

#Preview("Usage Indicator") {
    VStack(spacing: 16) {
        UsageIndicator(current: 2, limit: 3, label: "Servers", showUpgrade: .constant(false))
        UsageIndicator(current: 3, limit: 3, label: "Servers", showUpgrade: .constant(false))
    }
    .padding()
}
