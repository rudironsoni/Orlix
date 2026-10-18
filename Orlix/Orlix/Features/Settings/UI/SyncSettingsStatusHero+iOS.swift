#if os(iOS)
import SwiftUI

package extension SyncSettingsStatusHero {
    package var platformBody: some View {
        StatusHeroLayout(
            state: state,
            lastSuccessfulSyncDate: lastSuccessfulSyncDate,
            primaryAction: primaryAction,
            onPrimaryAction: onPrimaryAction
        )
    }
}

private struct StatusHeroLayout: View {
    @ScaledMetric(relativeTo: .largeTitle) private var iconSize = 58.0

    package let state: SyncSettingsUserState
    package let lastSuccessfulSyncDate: Date?
    package let primaryAction: SyncSettingsPrimaryAction?
    package let onPrimaryAction: () -> Void

    package var body: some View {
        VStack(spacing: 12) {
            VStack(spacing: 8) {
                Image(systemName: state.statusHeroSystemImage)
                    .font(.system(size: iconSize, weight: .regular))
                    .foregroundStyle(state.statusHeroTint)
                    .accessibilityHidden(true)

                Text(state.title)
                    .font(.title2.weight(.semibold))

                SyncSettingsStatusDetail(
                    state: state,
                    lastSuccessfulSyncDate: lastSuccessfulSyncDate
                )
                .font(.subheadline)
            }
            .accessibilityElement(children: .combine)
            .accessibilityLabel("Sync Status")
            .accessibilityValue(state.title)
            .accessibilityIdentifier("orlix.settings.sync.statusHero")

            if let primaryAction {
                SyncSettingsPrimaryActionButton(
                    action: primaryAction,
                    perform: onPrimaryAction
                )
            }
        }
        .multilineTextAlignment(.center)
        .frame(maxWidth: .infinity)
        .padding(.horizontal, 24)
        .padding(.vertical, 12)
    }
}
#endif
