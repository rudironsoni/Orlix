//
//  DetailHeaderBar.swift
//  Orlix
//
//  Shared header for sheet views
//

import SwiftUI

package struct DetailHeaderBar<Leading: View, Trailing: View>: View {
    package let showsBackground: Bool
    package let padding: EdgeInsets?
    package let leading: Leading
    package let trailing: Trailing

    package init(
        showsBackground: Bool = true,
        padding: EdgeInsets? = nil,
        @ViewBuilder leading: () -> Leading,
        @ViewBuilder trailing: () -> Trailing
    ) {
        self.showsBackground = showsBackground
        self.padding = padding
        self.leading = leading()
        self.trailing = trailing()
    }

    package init(
        showsBackground: Bool = true,
        padding: EdgeInsets? = nil,
        @ViewBuilder leading: () -> Leading
    ) where Trailing == EmptyView {
        self.showsBackground = showsBackground
        self.padding = padding
        self.leading = leading()
        self.trailing = EmptyView()
    }

    package var body: some View {
        let content = HStack {
            leading
            Spacer()
            trailing
        }

        if let padding = padding {
            content
                .padding(padding)
                .modifier(DetailHeaderBackground(enabled: showsBackground))
        } else {
            content
                .padding()
                .modifier(DetailHeaderBackground(enabled: showsBackground))
        }
    }
}

package struct DetailCloseButton: View {
    package let action: () -> Void
    package var size: CGFloat = 16
    package var color: Color = .secondary

    package init(
        action: @escaping () -> Void,
        size: CGFloat = 16,
        color: Color = .secondary
    ) {
        self.action = action
        self.size = size
        self.color = color
    }

    package var body: some View {
        Button(action: action) {
            Image(systemName: "xmark")
                .font(.system(size: size))
                .foregroundStyle(color)
        }
        .buttonStyle(.plain)
    }
}

private struct DetailHeaderBackground: ViewModifier {
    package let enabled: Bool

    package func body(content: Content) -> some View {
        if enabled {
            content.background(.ultraThinMaterial)
        } else {
            content
        }
    }
}
