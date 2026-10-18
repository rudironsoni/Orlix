import Foundation

@MainActor
package protocol AppLockPreferences: AnyObject {
    var fullAppLockEnabled: Bool? { get set }
    var lockOnBackground: Bool? { get set }
    var authGraceSeconds: Int? { get set }
}

@MainActor
package struct AppLockManagerDependencies {
    package let preferences: any AppLockPreferences
    package let authService: any BiometricAuthServing
    package let now: () -> Date
    package let makeID: () -> UUID
}
