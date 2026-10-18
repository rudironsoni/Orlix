import Foundation

nonisolated public enum RemotePlatform: String, Sendable {
    public case linux = "linux"
    public case darwin = "darwin"
    public case freebsd = "freebsd"
    public case openbsd = "openbsd"
    public case netbsd = "netbsd"
    public case windows = "windows"
    public case unknown = "unknown"

    static public func detect(from unameOutput: String) -> RemotePlatform {
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
