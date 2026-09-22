#if ORLIX_TEST_HOST
import OrlixTestImplementation
#else
import OrlixImplementation
#endif
#if os(iOS)
extension GhosttyTerminalView: TerminalKeyboardInputSession {}
#endif
