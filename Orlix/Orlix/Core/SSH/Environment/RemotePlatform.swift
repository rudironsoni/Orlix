import Foundation

nonisolated package enum RemotePlatform: String, Sendable {
    package case linux = "linux"
    package case darwin = "darwin"
    package case freebsd = "freebsd"
    package case openbsd = "openbsd"
    package case netbsd = "netbsd"
    package case windows = "windows"
    package case unknown = "unknown"

    static package func detect(from unameOutput: String) -> RemotePlatform {
        let os = unameOutput.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()

        if os.contains("darwin") {
            return .darwin
        } else if os.contains("freebsd") {
            return .freebsd
        } else if os.contains("openbsd") {
            return .openbsd
        } else if os.contains("netbsd") {
            return .netbsd
        } else if os.contains("linux") {
            return .linux
        } else if os.contains("mingw") || os.contains("msys") || os.contains("cygwin") || os.contains("windows") {
            return .windows
        }

        return .linux
    }
}
