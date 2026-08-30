import Foundation
import NetworkExtension
import Testing
@testable import Orlix

struct TSSHConcurrencyTests {
    @Test
    func cancellingNativeOperationReturnsBeforeBlockingWorkFinishes() async throws {
        let operationStarted = DispatchSemaphore(value: 0)
        let releaseOperation = DispatchSemaphore(value: 0)
        let lateValueDiscarded = DispatchSemaphore(value: 0)
        let task = Task {
            try await tsshPerformCancellable(
                on: DispatchQueue(label: "TSSHConcurrencyTests.native"),
                operation: {
                    operationStarted.signal()
                    releaseOperation.wait()
                    return 42
                },
                discardLateValue: { value in
                    if value == 42 { lateValueDiscarded.signal() }
                }
            )
        }

        #expect(operationStarted.wait(timeout: .now() + 1) == .success)
        task.cancel()
        do {
            _ = try await task.value
            Issue.record("Cancelled native operation returned a value")
        } catch is CancellationError {
        } catch {
            Issue.record("Cancelled native operation returned \(error)")
        }

        releaseOperation.signal()
        #expect(lateValueDiscarded.wait(timeout: .now() + 1) == .success)
    }

    @Test @MainActor
    func secondVPNOwnerCannotReplaceOrReleaseFirstOwner() throws {
        let coordinator = TSSHVPNOwnershipCoordinator()
        let firstOwner = UUID()
        let secondOwner = UUID()

        #expect(try coordinator.acquire(firstOwner) == .vacant)
        #expect(try coordinator.acquire(firstOwner) == .existingOwner)
        #expect(throws: TSSHRuntimeError.self) {
            try coordinator.acquire(secondOwner)
        }
        coordinator.release(secondOwner)
        #expect(coordinator.isOwned(by: firstOwner))
        coordinator.release(firstOwner)
        #expect(coordinator.ownerID == nil)
    }

    @Test
    func VPNStartupRequiresConnectedAndRejectsFailedStates() {
        var monitor = TSSHVPNStartupMonitor()
        #expect(monitor.decision(for: .disconnected, elapsedSeconds: 0) == .waiting)
        #expect(monitor.decision(for: .connecting, elapsedSeconds: 0.1) == .waiting)
        #expect(monitor.decision(for: .disconnected, elapsedSeconds: 0.2) == .failed(
            "The VPN disconnected during startup."
        ))

        monitor = TSSHVPNStartupMonitor()
        #expect(monitor.decision(for: .invalid, elapsedSeconds: 0) == .failed(
            "The VPN configuration is invalid."
        ))
        #expect(monitor.decision(for: .connected, elapsedSeconds: 0) == .connected)
    }
}
