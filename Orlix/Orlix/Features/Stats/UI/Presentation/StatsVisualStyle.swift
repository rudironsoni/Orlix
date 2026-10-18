import SwiftUI
#if os(iOS)
import UIKit
#elseif os(macOS)
import AppKit
#endif

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package struct ClassicStatsCardSurfaceStyle: Equatable {
    package let fill: Color
    package let stroke: Color
    package let pageBackground: Color

    static package func make(for backgroundColor: Color) -> ClassicStatsCardSurfaceStyle {
        #if os(iOS)
        ClassicStatsCardSurfaceStyle(
            fill: Color(UIColor.secondarySystemGroupedBackground),
            stroke: .clear,
            pageBackground: Color(UIColor.systemGroupedBackground)
        )
        #else
        ClassicStatsCardSurfaceStyle(
            fill: Color.primary.opacity(0.06),
            stroke: Color.primary.opacity(0.08),
            pageBackground: backgroundColor
        )
        #endif
    }
}

private struct ClassicStatsCardModifier: ViewModifier {
    package let surfaceStyle: ClassicStatsCardSurfaceStyle

    package func body(content: Content) -> some View {
        let shape = RoundedRectangle(cornerRadius: 12, style: .continuous)
        content
            .background(surfaceStyle.fill, in: shape)
            .overlay {
                shape.stroke(surfaceStyle.stroke, lineWidth: 1)
            }
    }
}

package extension View {
    package func classicStatsCardSurface(_ surfaceStyle: ClassicStatsCardSurfaceStyle) -> some View {
        modifier(ClassicStatsCardModifier(surfaceStyle: surfaceStyle))
    }
}

package enum StatsIcon {
    static package let gpu = "display"
}

package struct StatsVisualStyle {
    package enum Density {
        case compact
        case detailed
        case classic
    }

    package enum Surface {
        case dashboard
        case grouped
    }

    package let density: Density
    package let pageBackground: Color
    package let cardFill: Color
    package let cardStroke: Color
    package let primaryText: Color
    package let secondaryText: Color
    package let tertiaryText: Color
    package let meterTrack: Color

    package init(
        preferencesStyle: StatsPreferences.Style = .cardsCompact,
        surface: Surface = .dashboard,
        colorScheme: ColorScheme = .dark
    ) {
        switch preferencesStyle {
        case .cardsCompact:
            density = .compact
        case .cardsDetailed:
            density = .detailed
        case .classic:
            density = .classic
        }

        switch surface {
        case .dashboard:
            #if os(macOS)
            pageBackground = colorScheme == .light ? Self.nativeGroupedBackground : .clear
            #else
            pageBackground = Self.nativeGroupedBackground
            #endif
            if preferencesStyle == .classic {
                cardFill = Color.white.opacity(0.08)
                cardStroke = Color.white.opacity(0.06)
            } else {
                if colorScheme == .light {
                    cardFill = Self.nativeGroupedCardFill
                    cardStroke = Self.nativeGroupedCardStroke
                } else {
                    #if os(macOS)
                    cardFill = Color.white.opacity(0.045)
                    cardStroke = Color.white.opacity(0.075)
                    #else
                    cardFill = Color(red: 0.11, green: 0.11, blue: 0.12)
                    cardStroke = Color.white.opacity(0.04)
                    #endif
                }
            }
            if colorScheme == .light, preferencesStyle != .classic {
                primaryText = Color.primary
                secondaryText = Color.secondary
                tertiaryText = Color.secondary.opacity(0.45)
                meterTrack = Color.primary.opacity(0.10)
            } else {
                primaryText = Color.white
                secondaryText = Color.white.opacity(0.58)
                tertiaryText = Color.white.opacity(0.34)
                meterTrack = Color.white.opacity(0.10)
            }
        case .grouped:
            pageBackground = Self.nativeGroupedBackground
            cardFill = Self.nativeGroupedCardFill
            cardStroke = Self.nativeGroupedCardStroke
            primaryText = Color.primary
            secondaryText = Color.secondary
            tertiaryText = Color.secondary.opacity(0.35)
            meterTrack = Color.primary.opacity(0.10)
        }
    }

    package var cardSpacing: CGFloat {
        density == .detailed ? 18 : 14
    }

    package var gridMinimumColumnWidth: CGFloat {
        density == .detailed ? 320 : 292
    }

    package var gridMaximumWidth: CGFloat {
        density == .detailed ? 1_360 : 1_180
    }

    package var horizontalPadding: CGFloat {
        density == .detailed ? 18 : 14
    }

    package var topPadding: CGFloat {
        density == .detailed ? 22 : 16
    }

    package var bottomPadding: CGFloat {
        density == .detailed ? 28 : 22
    }

    package var cardPadding: CGFloat {
        density == .detailed ? 22 : 18
    }

    package var cardCornerRadius: CGFloat {
        density == .classic ? 22 : 28
    }

    package var titleSize: CGFloat {
        density == .detailed ? 44 : 38
    }

    package var prominentValueSize: CGFloat {
        density == .detailed ? 44 : 38
    }

    package var metricValueSize: CGFloat {
        density == .detailed ? 44 : 38
    }

    package var networkValueSize: CGFloat {
        density == .detailed ? 34 : 30
    }

    package var metricPreviewWidth: CGFloat {
        density == .detailed ? 168 : 136
    }

    package var metricPreviewHeight: CGFloat {
        density == .detailed ? 118 : 92
    }

    package var overviewMinHeight: CGFloat {
        density == .detailed ? 164 : 136
    }

    package var metricMinHeight: CGFloat {
        density == .detailed ? 196 : 164
    }

    package var networkMinHeight: CGFloat {
        density == .detailed ? 246 : 222
    }

    package var networkChartHeight: CGFloat {
        density == .detailed ? 142 : 122
    }

    package var networkValuesWidth: CGFloat {
        density == .detailed ? 150 : 132
    }

    package var processLimit: Int {
        density == .detailed ? 5 : 4
    }

    package var volumeLimit: Int {
        density == .detailed ? 6 : 4
    }

    private static var nativeGroupedBackground: Color {
        #if os(iOS)
        Color(UIColor.systemGroupedBackground)
        #elseif os(macOS)
        Color(nsColor: .windowBackgroundColor)
        #else
        Color.clear
        #endif
    }

    private static var nativeGroupedCardFill: Color {
        #if os(iOS)
        Color(UIColor.secondarySystemGroupedBackground)
        #elseif os(macOS)
        Color(nsColor: .controlBackgroundColor)
        #else
        Color.primary.opacity(0.06)
        #endif
    }

    private static var nativeGroupedCardStroke: Color {
        #if os(iOS)
        Color(UIColor.separator).opacity(0.28)
        #elseif os(macOS)
        Color(nsColor: .separatorColor).opacity(0.45)
        #else
        Color.primary.opacity(0.08)
        #endif
    }
}

package enum StatsResolvedAppearance {
    static package let storageKey = "appearanceMode"

    static package func colorScheme(from rawValue: String, fallback: ColorScheme) -> ColorScheme {
        switch rawValue {
        case "light":
            return .light
        case "dark":
            return .dark
        default:
            return fallback
        }
    }
}
