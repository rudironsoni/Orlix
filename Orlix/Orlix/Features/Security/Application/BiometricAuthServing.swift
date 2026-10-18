#if ORLIX_MODULAR_APP
import OrlixModels
#endif
@MainActor
package protocol BiometricAuthServing {
    func availability() -> BiometricAvailability
    func authenticate(
        reason: BiometricAuthenticationReason,
        allowPasscodeFallback: Bool
    ) async throws
}
