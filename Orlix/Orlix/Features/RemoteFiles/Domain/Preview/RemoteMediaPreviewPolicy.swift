import Foundation

nonisolated package enum RemoteMediaPreviewPolicy {
    static package let maxDimension = 16_384.0
    static package let maxPixelCount = 64_000_000.0
    static package let maxFrameCount = 600
    static package let maxVideoDurationSeconds = 6 * 60 * 60.0

    static package func permits(
        width: Double,
        height: Double,
        frameCount: Int,
        durationSeconds: Double? = nil
    ) -> Bool {
        guard width.isFinite,
              height.isFinite,
              width > 0,
              height > 0,
              width <= maxDimension,
              height <= maxDimension,
              width <= maxPixelCount / height,
              frameCount > 0,
              frameCount <= maxFrameCount else {
            return false
        }
        guard let durationSeconds else { return true }
        return durationSeconds.isFinite
            && durationSeconds >= 0
            && durationSeconds <= maxVideoDurationSeconds
    }
}
