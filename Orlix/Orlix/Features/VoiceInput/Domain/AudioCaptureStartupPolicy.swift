import Foundation

package struct AudioCaptureLifecycleState: Equatable, Sendable {
    package let applicationIsActive: Bool
    package let sceneIsActive: Bool

    package var allowsCapture: Bool {
        applicationIsActive && sceneIsActive
    }
}

enum AudioCaptureStartupRejection: Equatable {
    case inactiveLifecycle
    case unavailableInputFormat
}

package enum AudioCaptureStartupPolicy {
    static package func rejection(
        lifecycle: AudioCaptureLifecycleState,
        sampleRate: Double,
        channelCount: Int
    ) -> AudioCaptureStartupRejection? {
        guard lifecycle.allowsCapture else { return .inactiveLifecycle }
        guard sampleRate.isFinite, sampleRate > 0, channelCount > 0 else {
            return .unavailableInputFormat
        }
        return nil
    }

    static package func outputFrameCapacity(
        inputFrameCount: UInt32,
        inputSampleRate: Double,
        targetSampleRate: Double
    ) -> UInt32? {
        guard inputSampleRate.isFinite,
              inputSampleRate > 0,
              targetSampleRate.isFinite,
              targetSampleRate > 0 else {
            return nil
        }
        let scaledFrameCount = Double(inputFrameCount) * targetSampleRate / inputSampleRate
        guard scaledFrameCount.isFinite,
              scaledFrameCount >= 0,
              scaledFrameCount < Double(UInt32.max) else {
            return nil
        }
        return UInt32(scaledFrameCount) + 1
    }
}
