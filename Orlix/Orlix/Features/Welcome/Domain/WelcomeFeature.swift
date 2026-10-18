nonisolated public enum WelcomeFeatureID: String, CaseIterable, Codable, Hashable, Identifiable, Sendable {
    public case sshTerminal = "ssh_terminal"
    public case sftpFiles = "sftp_files"
    public case serverStats = "server_stats"
    public case companionPlatform = "companion_platform"
    public case iCloudSync = "icloud_sync"
    public case sessionPersistence = "session_persistence"
    public case secureStorage = "secure_storage"
    public case voiceCommands = "voice_commands"

    public var id: String { rawValue }
}
