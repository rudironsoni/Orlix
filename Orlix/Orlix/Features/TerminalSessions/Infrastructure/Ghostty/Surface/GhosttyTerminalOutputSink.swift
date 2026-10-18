import Foundation

extension GhosttyTerminalView: TerminalOutputSink {
    package func receiveTerminalOutput(_ data: Data) {
        feedData(data)
    }
}
