import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package enum TerminalLiveActivityTarget: Equatable, Sendable {
    case end
    case active(TerminalLiveActivitySnapshot)
}

@MainActor
package protocol TerminalLiveActivityControlling: AnyObject {
    func reconcile(toward target: TerminalLiveActivityTarget) async
    func endForApplicationTermination() -> Bool
}

@MainActor
final package class LiveActivityManager {
    private let controller: any TerminalLiveActivityControlling
    private var requestedTarget: TerminalLiveActivityTarget?
    private var reconciliationTask: Task<Void, Never>?

    package init(controller: any TerminalLiveActivityControlling) {
        self.controller = controller
    }

    package func refresh(with connectionStates: [ConnectionState]) {
        requestedTarget = TerminalLiveActivityPolicy.snapshot(for: connectionStates)
            .map(TerminalLiveActivityTarget.active) ?? .end
        guard reconciliationTask == nil else { return }

        reconciliationTask = Task { [weak self] in
            await self?.reconcileRequestedTargets()
        }
    }

    @discardableResult
    package func endForApplicationTermination() -> Bool {
        requestedTarget = .end
        return controller.endForApplicationTermination()
    }

    private func reconcileRequestedTargets() async {
        while let target = requestedTarget {
            requestedTarget = nil
            await controller.reconcile(toward: target)
        }
        reconciliationTask = nil
    }
}
