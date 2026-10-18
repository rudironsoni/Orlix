import CoreGraphics

nonisolated package enum TerminalAccessoryAppearancePolicy {
    package enum InterfaceStyle: Equatable {
        case unspecified
        case light
        case dark
    }

    static package func resolvedInterfaceStyle(
        owner: InterfaceStyle,
        host: InterfaceStyle
    ) -> InterfaceStyle {
        owner == .unspecified ? host : owner
    }

    static package func isDarkBackground(red: CGFloat, green: CGFloat, blue: CGFloat) -> Bool {
        let luminance = (0.2126 * red) + (0.7152 * green) + (0.0722 * blue)
        return luminance < 0.55
    }
}
