//
//  AppearanceSettingsComponents.swift
//  Orlix
//

import SwiftUI
import os.log
#if os(macOS)
import AppKit
#endif
#if os(iOS)
import UIKit
#endif

package enum AppearanceMode: String, CaseIterable {
    case system = "system"
    case light = "light"
    case dark = "dark"

    package var label: String {
        switch self {
        case .system: return String(localized: "System")
        case .light: return String(localized: "Light")
        case .dark: return String(localized: "Dark")
        }
    }

    package var colorScheme: ColorScheme? {
        switch self {
        case .system: return nil
        case .light: return .light
        case .dark: return .dark
        }
    }
}

/// View modifier that applies the app-wide appearance setting
package struct AppearanceModifier: ViewModifier {
    @AppStorage("appearanceMode") private var appearanceMode: String = AppearanceMode.system.rawValue

    private var colorScheme: ColorScheme? {
        switch AppearanceMode(rawValue: appearanceMode) ?? .system {
        case .system: return nil
        case .light: return .light
        case .dark: return .dark
        }
    }

    package func body(content: Content) -> some View {
        let mode = AppearanceMode(rawValue: appearanceMode) ?? .system
        #if os(iOS)
        content
            .background(
                AppearanceWindowBridge(mode: mode)
                    .frame(width: 0, height: 0)
            )
        #else
        content
            .preferredColorScheme(mode.colorScheme)
            .background(
                AppearanceWindowBridge(mode: mode)
                    .frame(width: 0, height: 0)
            )
        #endif
    }

    package init() {}
}

// MARK: - Appearance Picker View

#if os(macOS)
package struct AppearanceWindowBridge: NSViewRepresentable {
    package let mode: AppearanceMode

    package func makeNSView(context: Context) -> NSView {
        NSView(frame: .zero)
    }

    package func updateNSView(_ nsView: NSView, context: Context) {
        guard let window = nsView.window else { return }

        let targetName: NSAppearance.Name? = {
            switch mode {
            case .system:
                return nil
            case .light:
                return .aqua
            case .dark:
                return .darkAqua
            }
        }()

        if window.appearance?.name != targetName {
            window.appearance = targetName.flatMap(NSAppearance.init(named:))
        }
    }
}
#endif

#if os(iOS)
final package class AppearanceWindowController: UIViewController {
    package var mode: AppearanceMode = .system

    override package func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        applyAppearance()
    }

    override package func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        applyAppearance()
    }

    package func applyAppearance() {
        let targetStyle: UIUserInterfaceStyle = {
            switch mode {
            case .system:
                return .unspecified
            case .light:
                return .light
            case .dark:
                return .dark
            }
        }()

        if let controller = topHostingController(),
           controller.overrideUserInterfaceStyle != targetStyle {
            controller.overrideUserInterfaceStyle = targetStyle
            return
        }

        if let window = view.window, window.overrideUserInterfaceStyle != targetStyle {
            window.overrideUserInterfaceStyle = targetStyle
        }
    }

    private func topHostingController() -> UIViewController? {
        var controller: UIViewController? = self
        while let parent = controller?.parent {
            controller = parent
        }
        return controller
    }
}

package struct AppearanceWindowBridge: UIViewControllerRepresentable {
    package let mode: AppearanceMode

    package func makeUIViewController(context: Context) -> AppearanceWindowController {
        let controller = AppearanceWindowController()
        controller.view.isHidden = true
        return controller
    }

    package func updateUIViewController(_ uiViewController: AppearanceWindowController, context: Context) {
        uiViewController.mode = mode
        uiViewController.applyAppearance()
    }
}
#endif

package struct AppearancePickerView: View {
    @Binding package var selection: String

    package var body: some View {
        HStack(spacing: 0) {
            ForEach(AppearanceMode.allCases, id: \.self) { mode in
                let isSelected = selection == mode.rawValue
                Button {
                    selection = mode.rawValue
                } label: {
                    AppearanceOptionView(
                        mode: mode,
                        isSelected: isSelected
                    )
                    .frame(maxWidth: .infinity)
                }
                .buttonStyle(.plain)
                .accessibilityLabel(mode.label)
                .accessibilityAddTraits(isSelected ? .isSelected : [])
                .accessibilityIdentifier("orlix.settings.appearance.\(mode.rawValue)")
            }
        }
    }
}

package struct AppearanceOptionView: View {
    package let mode: AppearanceMode
    package let isSelected: Bool

    package var body: some View {
        VStack(spacing: 8) {
            ZStack {
                AppearancePreviewCard(mode: mode)
                    .frame(width: 100, height: 64)
                    .clipShape(RoundedRectangle(cornerRadius: 8))
                    .overlay(
                        RoundedRectangle(cornerRadius: 8)
                            .stroke(isSelected ? Color.accentColor : Color.clear, lineWidth: 3)
                    )
                    .shadow(color: .black.opacity(0.15), radius: 2, x: 0, y: 1)
            }

            Text(mode.label)
                .font(.caption)
                .foregroundStyle(isSelected ? .primary : .secondary)
        }
    }
}

package struct AppearancePreviewCard: View {
    package let mode: AppearanceMode

    package var body: some View {
        switch mode {
        case .system:
            HStack(spacing: 0) {
                miniWindowPreview(isDark: false)
                miniWindowPreview(isDark: true)
            }
        case .light:
            miniWindowPreview(isDark: false)
        case .dark:
            miniWindowPreview(isDark: true)
        }
    }

    private func miniWindowPreview(isDark: Bool) -> some View {
        let bgColor = isDark ? Color(white: 0.15) : Color(white: 0.95)
        let windowBg = isDark ? Color(white: 0.22) : Color.white
        let sidebarBg = isDark ? Color(white: 0.18) : Color(white: 0.92)
        let accentBar = isDark ? Color.pink.opacity(0.8) : Color.pink
        let dotColors: [Color] = [.red, .yellow, .green]

        return ZStack {
            bgColor

            VStack(spacing: 0) {
                HStack(spacing: 3) {
                    ForEach(0..<3, id: \.self) { i in
                        Circle()
                            .fill(dotColors[i])
                            .frame(width: 5, height: 5)
                    }
                    Spacer()
                }
                .padding(.horizontal, 6)
                .padding(.vertical, 4)
                .background(windowBg)

                HStack(spacing: 0) {
                    Rectangle()
                        .fill(sidebarBg)
                        .frame(width: 16)

                    VStack(spacing: 4) {
                        RoundedRectangle(cornerRadius: 2)
                            .fill(accentBar)
                            .frame(height: 8)
                            .padding(.horizontal, 4)
                            .padding(.top, 4)

                        Spacer()
                    }
                    .background(windowBg)
                }
            }
            .clipShape(RoundedRectangle(cornerRadius: 4))
            .padding(6)
        }
    }
}
