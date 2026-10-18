import SwiftUI

package struct ProSettingsStatusHero: View {
    package let state: ProSettingsUserState
    package let onPrimaryAction: (ProSettingsPrimaryAction) -> Void

    package var body: some View {
        platformBody
    }
}

package struct ProSettingsPrimaryActionButton: View {
    package let action: ProSettingsPrimaryAction
    package let perform: () -> Void

    package var body: some View {
        Button(action: perform) {
            Text(action.title)
        }
        .buttonStyle(.borderedProminent)
        .controlSize(.large)
        .tint(.orange)
        .accessibilityIdentifier("orlix.settings.pro.action.primary")
    }
}

package struct ProSettingsStatusContent: View {
    package let state: ProSettingsUserState
    package let alignment: HorizontalAlignment
    package let textAlignment: TextAlignment

    package var body: some View {
        VStack(alignment: alignment, spacing: 4) {
            Text(state.title)
                .font(.title2.weight(.semibold))

            if let renewalDate = state.renewalDate {
                Text("Renews \(renewalDate, format: .dateTime.month().day().year())")
                    .font(.subheadline)
                    .foregroundStyle(.secondary)
            }
        }
        .multilineTextAlignment(textAlignment)
        .accessibilityElement(children: .combine)
        .accessibilityIdentifier("orlix.settings.pro.statusHero")
    }
}
