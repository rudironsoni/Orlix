import Foundation

package extension GhosttyTerminalView: TerminalOutputSink {
    package func receiveTerminalOutput(_ data: Data) {
        feedData(data)
    }
}
