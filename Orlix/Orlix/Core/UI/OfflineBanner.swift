import SwiftUI

// MARK: - Offline Banner

package struct OfflineBanner: View {
    @ObservedObject package var networkMonitor: NetworkMonitor = .shared

    private var noticeItem: NoticeItem {
        NoticeItem(
            id: "offline-banner",
            lane: .topBanner,
            level: .warning,
            leading: .icon("wifi.slash"),
            title: String(localized: "Offline"),
            message: String(localized: "No network connection. Network-dependent features are paused.")
        )
    }

    package var body: some View {
        if networkMonitor.isOffline {
            NoticeBannerView(item: noticeItem)
                .frame(maxWidth: .infinity)
                .padding(.horizontal, 12)
                .padding(.top, 8)
            .transition(.move(edge: .top).combined(with: .opacity))
        }
    }
}

// MARK: - Compact Offline Indicator

package struct CompactOfflineIndicator: View {
    @ObservedObject package var networkMonitor: NetworkMonitor = .shared

    package var body: some View {
        if networkMonitor.isOffline {
            HStack(spacing: 4) {
                Image(systemName: "wifi.slash")
                    .font(.caption2)
                Text("Offline")
                    .font(.caption2)
            }
            .foregroundStyle(.orange)
            .padding(.horizontal, 8)
            .padding(.vertical, 4)
            .background(.orange.opacity(0.15), in: Capsule())
        }
    }
}

// MARK: - Network Status Indicator

package struct NetworkStatusIndicator: View {
    @ObservedObject package var networkMonitor: NetworkMonitor = .shared
    package var showLabel: Bool = true

    package var body: some View {
        HStack(spacing: 6) {
            Circle()
                .fill(networkMonitor.isConnected ? Color.green : (networkMonitor.isOffline ? Color.orange : Color.secondary))
                .frame(width: 8, height: 8)

            if showLabel {
                Text(networkMonitor.isOffline ? String(localized: "Offline") : networkMonitor.connectionType.displayName)
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
    }
}

// MARK: - Network Aware View Modifier

package struct NetworkAwareModifier: ViewModifier {
    @ObservedObject package var networkMonitor: NetworkMonitor = .shared
    package let showBanner: Bool

    package func body(content: Content) -> some View {
        VStack(spacing: 0) {
            if showBanner {
                OfflineBanner()
            }
            content
        }
        .animation(.easeInOut(duration: 0.3), value: networkMonitor.readiness)
    }
}

package extension View {
    package func networkAware(showBanner: Bool = true) -> some View {
        modifier(NetworkAwareModifier(showBanner: showBanner))
    }
}

// MARK: - Offline-Safe Button

package struct OfflineSafeButton<Label: View>: View {
    @ObservedObject package var networkMonitor: NetworkMonitor = .shared
    package let requiresNetwork: Bool
    package let action: () -> Void
    package let label: () -> Label

    package init(
        requiresNetwork: Bool = true,
        action: @escaping () -> Void,
        @ViewBuilder label: @escaping () -> Label
    ) {
        self.requiresNetwork = requiresNetwork
        self.action = action
        self.label = label
    }

    package var body: some View {
        Button(action: action) {
            HStack {
                label()
                if requiresNetwork && networkMonitor.isOffline {
                    Image(systemName: "wifi.slash")
                        .font(.caption2)
                        .foregroundStyle(.orange)
                }
            }
        }
        .disabled(requiresNetwork && networkMonitor.isOffline)
    }
}

// MARK: - Preview

#Preview("Offline Banner") {
    VStack {
        OfflineBanner()
        Spacer()
    }
}

#Preview("Network Indicators") {
    VStack(spacing: 20) {
        CompactOfflineIndicator()
        NetworkStatusIndicator()
        NetworkStatusIndicator(showLabel: false)
    }
    .padding()
}

#Preview("Network Aware View") {
    NavigationView {
        List {
            Text("Server 1")
            Text("Server 2")
            Text("Server 3")
        }
        .navigationTitle("Servers")
    }
    .networkAware()
}
