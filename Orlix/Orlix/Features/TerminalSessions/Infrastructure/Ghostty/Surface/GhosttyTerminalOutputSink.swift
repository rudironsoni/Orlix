#if ORLIX_TEST_HOST
import OrlixTestImplementation
#else
import OrlixImplementation
#endif
import Foundation

extension GhosttyTerminalView: TerminalOutputSink {
    func receiveTerminalOutput(_ data: Data) {
        feedData(data)
    }
}
