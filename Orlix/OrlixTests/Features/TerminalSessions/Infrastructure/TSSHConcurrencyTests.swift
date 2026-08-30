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

    @Test
    func nativeTransportDoesNotPublishConnectedBeforeRequestedSetupIsReady() {
        #expect(tsshPublishedTransportState(
            isHealthy: true,
            startupReady: false
        ) == .connecting)
        #expect(tsshPublishedTransportState(
            isHealthy: true,
            startupReady: true
        ) == .connected)
        #expect(tsshPublishedTransportState(
            isHealthy: false,
            startupReady: true
        ) == .reconnecting(attempt: 1))
    }

    @Test
    func postConnectVPNDropFailsClosedAtTheOwningPane() {
        #expect(tsshVPNPostConnectDecision(for: .connected) == .healthy)
        #expect(tsshVPNPostConnectDecision(for: .reasserting) == .healthy)
        #expect(tsshVPNPostConnectDecision(for: .disconnected) == .failed(
            "The TSSH VPN disconnected after startup."
        ))
        #expect(tsshVPNPostConnectDecision(for: .invalid) == .failed(
            "The TSSH VPN became invalid after startup."
        ))
    }

    @Test
    func reclaimedVPNRequiresTheExactPersistedConfiguration() throws {
        let info = try TSSHServerInfo.parse(output: #"{"ServerVer":"0.2.2","ProtoVer":1,"Port":61000,"Mode":"KCP","Pass":"aa","Salt":"bb","ProxyKey":"cc","ProxyMode":"TCP","MTU":1400,"ClientID":1,"ServerID":2}"#)
        let original = TSSHVPNConfiguration(
            host: "vpn.example.com",
            info: info,
            profile: TSSHProfile(
                mtu: 1_400,
                vpnEnabled: true,
                blockQUICInVPN: true,
                vpnDNSServers: ["1.1.1.1"],
                vpnExcludedRoutes: ["10.0.0.0/8"]
            )
        )
        let changedPolicy = TSSHVPNConfiguration(
            host: "vpn.example.com",
            info: info,
            profile: TSSHProfile(
                mtu: 1_280,
                vpnEnabled: true,
                blockQUICInVPN: false,
                vpnDNSServers: ["9.9.9.9"],
                vpnExcludedRoutes: ["192.168.0.0/16"]
            )
        )
        let fingerprint = try #require(original.fingerprint)

        #expect(tsshVPNConfigurationMatches(
            existingHost: "vpn.example.com",
            existingFingerprint: fingerprint,
            requestedConfiguration: original
        ))
        #expect(!tsshVPNConfigurationMatches(
            existingHost: "vpn.example.com",
            existingFingerprint: fingerprint,
            requestedConfiguration: changedPolicy
        ))
        #expect(!tsshVPNConfigurationMatches(
            existingHost: "other.example.com",
            existingFingerprint: fingerprint,
            requestedConfiguration: original
        ))
        #expect(!tsshVPNConfigurationMatches(
            existingHost: "vpn.example.com",
            existingFingerprint: nil,
            requestedConfiguration: original
        ))
    }
}
