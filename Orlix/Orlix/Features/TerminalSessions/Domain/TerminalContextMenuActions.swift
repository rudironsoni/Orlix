package struct TerminalContextMenuActions {
    package let focus: () -> Void
    package let splitRight: () -> Void
    package let splitLeft: () -> Void
    package let splitDown: () -> Void
    package let splitUp: () -> Void
    package let currentTitle: () -> String
    package let setTitle: (String?) -> Void

    package init(
        focus: @escaping () -> Void,
        splitRight: @escaping () -> Void,
        splitLeft: @escaping () -> Void,
        splitDown: @escaping () -> Void,
        splitUp: @escaping () -> Void,
        currentTitle: @escaping () -> String,
        setTitle: @escaping (String?) -> Void
    ) {
        self.focus = focus
        self.splitRight = splitRight
        self.splitLeft = splitLeft
        self.splitDown = splitDown
        self.splitUp = splitUp
        self.currentTitle = currentTitle
        self.setTitle = setTitle
    }
}
