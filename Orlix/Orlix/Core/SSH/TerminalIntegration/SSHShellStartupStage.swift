package extension SSHSession {
    package enum ShellStartupStage: Equatable, Sendable {
        case channelOpenRetry
        case ptyRequest
        case shellRequest
        case processRequestStarted
    }
}
