nonisolated package enum WelcomeFeatureID: String, CaseIterable, Codable, Hashable, Identifiable, Sendable {
    package case sshTerminal = "ssh_terminal"
    package case sftpFiles = "sftp_files"
    package case serverStats = "server_stats"
    package case companionPlatform = "companion_platform"
    package case iCloudSync = "icloud_sync"
    package case sessionPersistence = "session_persistence"
    package case secureStorage = "secure_storage"
    package case voiceCommands = "voice_commands"

    package var id: String { rawValue }
}
