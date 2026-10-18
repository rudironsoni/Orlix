#if os(iOS)
import UIKit

@MainActor
private final class UIApplicationIdleTimerController: TerminalIdleTimerControlling {
    func setIdleTimerDisabled(_ isDisabled: Bool) {
        UIApplication.shared.isIdleTimerDisabled = isDisabled
    }
}

package extension TerminalScreenAwakeCoordinator {
    convenience package init() {
        self.init(idleTimer: UIApplicationIdleTimerController())
    }
}
#endif
