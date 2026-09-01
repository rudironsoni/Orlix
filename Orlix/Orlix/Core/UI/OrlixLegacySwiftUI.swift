import SwiftUI

struct OrlixLabeledContent<Label: View, Content: View>: View {
    private let label: Label
    private let content: Content

    init(
        @ViewBuilder content: () -> Content,
        @ViewBuilder label: () -> Label
    ) {
        self.label = label()
        self.content = content()
    }

    var body: some View {
        HStack {
            label
            Spacer()
            content
        }
    }
}

extension OrlixLabeledContent where Label == Text {
    init(_ title: String, @ViewBuilder content: () -> Content) {
        self.init(content: content) {
            Text(title)
        }
    }

    init(_ title: LocalizedStringKey, @ViewBuilder content: () -> Content) {
        self.init(content: content) {
            Text(title)
        }
    }
}

extension OrlixLabeledContent where Label == Text, Content == Text {
    init(_ title: String, value: String) {
        self.init {
            Text(value)
        } label: {
            Text(title)
        }
    }
}

extension View {
    @ViewBuilder
    func orlixNavigationDestination<Destination: View>(
        isPresented: Binding<Bool>,
        @ViewBuilder destination: @escaping () -> Destination
    ) -> some View {
        if #available(iOS 16.0, macOS 13.0, *) {
            navigationDestination(isPresented: isPresented, destination: destination)
        } else {
            background(
                NavigationLink(
                    destination: destination(),
                    isActive: isPresented,
                    label: EmptyView.init
                )
                .hidden()
            )
        }
    }

    @ViewBuilder
    func orlixScrollContentBackgroundHidden() -> some View {
        if #available(iOS 16.0, macOS 13.0, *) {
            scrollContentBackground(.hidden)
        } else {
            self
        }
    }

    @ViewBuilder
    func orlixLargePresentationDetent() -> some View {
        if #available(iOS 16.0, macOS 13.0, *) {
            presentationDetents([.large])
        } else {
            self
        }
    }

    @ViewBuilder
    func orlixMediumAndLargePresentationDetents() -> some View {
        if #available(iOS 16.0, macOS 13.0, *) {
            presentationDetents([.medium, .large])
        } else {
            self
        }
    }

    @ViewBuilder
    func orlixPresentationDetents(height: CGFloat) -> some View {
        if #available(iOS 16.0, macOS 13.0, *) {
            presentationDetents([.height(height), .large])
        } else {
            self
        }
    }

    @ViewBuilder
    func orlixNavigationBarBackgroundHidden() -> some View {
        if #available(iOS 16.0, macOS 13.0, *) {
            toolbarBackground(.hidden, for: .navigationBar)
        } else {
            self
        }
    }

    @ViewBuilder
    func orlixGroupedFormStyle() -> some View {
        if #available(iOS 16.0, macOS 13.0, *) {
            formStyle(.grouped)
        } else {
            self
        }
    }

    @ViewBuilder
    func orlixVisiblePresentationDragIndicator() -> some View {
        if #available(iOS 16.0, macOS 13.0, *) {
            presentationDragIndicator(.visible)
        } else {
            self
        }
    }

    @ViewBuilder
    func orlixNavigationBarHidden(_ hidden: Bool) -> some View {
        if #available(iOS 16.0, macOS 13.0, *) {
            toolbar(hidden ? .hidden : .visible, for: .navigationBar)
        } else {
            navigationBarHidden(hidden)
        }
    }

    @ViewBuilder
    func orlixListRowSeparatorLeadingZero() -> some View {
        if #available(iOS 16.0, macOS 13.0, *) {
            alignmentGuide(.listRowSeparatorLeading) { _ in 0 }
        } else {
            self
        }
    }

    @ViewBuilder
    func orlixVisibleScrollIndicators() -> some View {
        if #available(iOS 16.0, macOS 13.0, *) {
            scrollIndicators(.visible)
        } else {
            self
        }
    }
}
