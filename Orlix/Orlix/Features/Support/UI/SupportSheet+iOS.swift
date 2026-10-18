#if os(iOS)
import SwiftUI
import UIKit

package extension SupportSheet {
    package func openURL(_ urlString: String) {
        guard let url = URL(string: urlString) else { return }
        UIApplication.shared.open(url)
    }
}

// MARK: - Support Settings View (iOS)

package struct SupportSettingsView: View {
    private struct ContactOption: Identifiable {
        package let id = UUID()
        package let title: String
        package let subtitle: String
        package let icon: String
        package let iconImage: String?
        package let iconText: String?
        package let color: Color
        package let url: String
    }

    private let contactOptions: [ContactOption] = [
        ContactOption(title: String(localized: "GitHub"), subtitle: String(localized: "Report Issue"), icon: "exclamationmark.triangle.fill", iconImage: nil, iconText: nil, color: .red, url: "https://github.com/rudironsoni/Orlix/issues")
    ]

    package var body: some View {
        List {
            Section {
                ForEach(contactOptions) { option in
                    Button {
                        openURL(option.url)
                    } label: {
                        HStack(spacing: 14) {
                            Group {
                                if let imageName = option.iconImage {
                                    Image(imageName)
                                        .resizable()
                                        .aspectRatio(contentMode: .fit)
                                } else if let text = option.iconText {
                                    Text(text)
                                        .font(.system(size: 18, weight: .bold))
                                } else {
                                    Image(systemName: option.icon)
                                }
                            }
                            .frame(width: 24, height: 24)
                            .foregroundStyle(option.color)

                            VStack(alignment: .leading, spacing: 2) {
                                Text(option.title)
                                    .font(.body)
                                    .foregroundStyle(.primary)

                                Text(option.subtitle)
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                            }

                            Spacer()

                            Image(systemName: "arrow.up.right")
                                .font(.caption)
                                .foregroundStyle(.tertiary)
                        }
                    }
                }
            } header: {
                Text("Questions, feedback, or issues? Reach out anytime.")
                    .textCase(nil)
            }

            Section {
                Button {
                    openURL("https://github.com/rudironsoni/Orlix")
                } label: {
                    HStack {
                        Text("Orlix")
                            .foregroundStyle(.secondary)
                        Spacer()
                        Image(systemName: "arrow.up.right")
                            .font(.caption)
                            .foregroundStyle(.tertiary)
                    }
                }
            }
        }
        .adaptiveSoftScrollEdges()
    }

    private func openURL(_ urlString: String) {
        guard let url = URL(string: urlString) else { return }
        UIApplication.shared.open(url)
    }
}
#endif
