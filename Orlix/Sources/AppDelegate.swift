import GhosttyTerminal
import UIKit

@main
final class AppDelegate: UIResponder, UIApplicationDelegate {
    private let launchStartedAt = Date()

    func application(
        _: UIApplication,
        didFinishLaunchingWithOptions _: [UIApplication.LaunchOptionsKey: Any]? = nil
    ) -> Bool {
        #if DEBUG
        TerminalDebugLog.enable([.lifecycle, .metrics, .input, .ime, .actions])
        #else
        TerminalDebugLog.disable()
        #endif
        OrlixTelemetry.shared.track(.appStarted)
        OrlixTelemetry.shared.record(
            .appLaunch(startedAt: launchStartedAt, finishedAt: Date())
        )
        return true
    }

    func applicationDidEnterBackground(_ application: UIApplication) {
        OrlixTelemetry.shared.flushDiagnostics()
    }

    func applicationWillTerminate(_ application: UIApplication) {
        OrlixTelemetry.shared.flushDiagnostics()
    }

    func application(
        _: UIApplication,
        configurationForConnecting connectingSceneSession: UISceneSession,
        options _: UIScene.ConnectionOptions
    ) -> UISceneConfiguration {
        let configuration = UISceneConfiguration(
            name: "Default Configuration",
            sessionRole: connectingSceneSession.role
        )
        configuration.delegateClass = SceneDelegate.self
        return configuration
    }
}
