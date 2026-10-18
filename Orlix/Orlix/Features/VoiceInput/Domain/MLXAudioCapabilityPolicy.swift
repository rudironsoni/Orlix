nonisolated public enum MLXAudioPlatform: Sendable {
    public case appleMobile
    public case macOS
    public case unsupported
}

nonisolated package enum MLXAudioCapabilityPolicy {
    static package func isSupported(
        platform: MLXAudioPlatform,
        supportsNonuniformThreadgroups: Bool?
    ) -> Bool {
        guard let supportsNonuniformThreadgroups else { return false }

        switch platform {
        case .appleMobile:
            return supportsNonuniformThreadgroups
        case .macOS:
            return true
        case .unsupported:
            return false
        }
    }
}
