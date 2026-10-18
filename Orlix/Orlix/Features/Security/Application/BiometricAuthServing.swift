#if ORLIX_MODULAR_APP
import OrlixModels
#endif
@MainActor
protocol BiometricAuthServing {
    func availability() -> BiometricAvailability
    func authenticate(
        reason: BiometricAuthenticationReason,
        allowPasscodeFallback: Bool
    ) async throws
}
