import SwiftUI

package struct BlockingStatusView<Content: View>: View {
    package var maxWidth: CGFloat = NoticeMetrics.blockingMaxWidth
    package var showsScrim: Bool = true
    package var cornerRadius: CGFloat = NoticeMetrics.cornerRadius
    package var surfaceStyle: NoticeSurfaceStyle = .standard
    package let content: Content

    @Environment(\.colorScheme) private var colorScheme
    package init(
        maxWidth: CGFloat = NoticeMetrics.blockingMaxWidth,
        showsScrim: Bool = true,
        cornerRadius: CGFloat = NoticeMetrics.cornerRadius,
        surfaceStyle: NoticeSurfaceStyle = .standard,
        @ViewBuilder content: () -> Content
    ) {
        self.maxWidth = maxWidth
        self.showsScrim = showsScrim
        self.cornerRadius = cornerRadius
        self.surfaceStyle = surfaceStyle
        self.content = content()
    }

    package var body: some View {
        ZStack {
            if showsScrim {
                Color.black
                    .opacity(colorScheme == .dark ? 0.32 : 0.22)
                    .ignoresSafeArea()
            }

            NoticeGlassGroup(spacing: 12) {
                content
                    .foregroundStyle(surfaceStyle.primaryForegroundColor)
                    .frame(maxWidth: maxWidth)
                    .padding(.horizontal, 28)
                    .padding(.vertical, contentVerticalPadding)
                    .noticeSurface(
                        style: surfaceStyle,
                        cornerRadius: cornerRadius,
                        shadowRadius: 18,
                        shadowY: 10
                    )
                    .padding(24)
            }
        }
    }

    private var contentVerticalPadding: CGFloat {
        #if os(iOS)
        return 26
        #else
        return 22
        #endif
    }
}
