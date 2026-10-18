package struct TerminalContextMenuActions {
    package let focus: () -> Void
    package let splitRight: () -> Void
    package let splitLeft: () -> Void
    package let splitDown: () -> Void
    package let splitUp: () -> Void
    package let currentTitle: () -> String
    package let setTitle: (String?) -> Void
}
