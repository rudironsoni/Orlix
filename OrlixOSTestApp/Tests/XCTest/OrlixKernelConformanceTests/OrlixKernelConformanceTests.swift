import XCTest
@testable import OrlixOSTestApp

final class OrlixKernelConformanceTests: XCTestCase {
    func testProcessLifecycleProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelProcessLifecycle)
    }

    func testMountNamespaceProbeVerifiesMountinfoThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelMountNamespace)
    }

    func testEnvironmentEntryProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelEnvironmentEntry)
    }

    func testInitExecProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelInitExec)
    }

    func testFDExecProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelFDExec)
    }

    func testFDAliasProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelFDAlias)
    }

	func testPTYTerminalProbeCompletesThroughOrlixOSTerminalSession() throws {
		try OrlixUpstreamXCTest.run(.kernelPTYTerminal)
	}

	func testOrlixTCTICryptoProbeCompletesThroughOrlixOSTerminalSession() throws {
		try OrlixUpstreamXCTest.run(.kernelTCTICrypto)
	}

	func testSignalWaitProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelSignalWait)
    }

    func testPipePollProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelPipePoll)
    }

    func testPipeSelectProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelPipeSelect)
    }

    func testPipeEpollProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelPipeEpoll)
    }

    func testPseudoFSProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelPseudoFS)
    }

    func testCgroupV2ProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelCgroupV2)
    }

    func testCgroupIOProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelCgroupIO)
    }

    func testCgroupNamespaceProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelCgroupNamespace)
    }

    func testCgroupPidsProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelCgroupPids)
    }

    func testUserNamespaceProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelUserNamespace)
    }

    func testOverlayFSProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelOverlayFS)
    }

    func testTimeNamespaceProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelTimeNamespace)
    }

    func testTimeSurfaceProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelTimeSurface)
    }

    func testIPCNamespaceProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelIPCNamespace)
    }

    func testPathErrnoProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelPathErrno)
    }

    func testCloneThreadProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelCloneThread)
    }

    func testFutexWaitWakeProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelFutexWaitWake)
    }

    func testTmpfsTruncWriteProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelTmpfsTruncWrite)
    }

    func testPthreadAttrProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelPthreadAttr)
    }

    func testTCTISystemProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelTCTISystem)
    }

    func testBootProfileContractVerifiesVirtioConsoleThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelBootProfile)
    }

    func testVirtioMMIOContractProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelVirtioMMIOContract)
    }

    func testVirtioNetDeviceProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelVirtioNetDevice)
    }

    func testRandomDeviceProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelRandomDevice)
    }

    func testVirtioFSMountProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelVirtioFSMount)
    }

    func testVirtioFSMountProbeSpecUsesHostDirectoryFixture() {
        XCTAssertTrue(OrlixUpstreamTestRunSpec.kernel.hostDirectoryFixture)
        XCTAssertTrue(OrlixUpstreamTestRunSpec.kernelVirtioFSMount.hostDirectoryFixture)
        XCTAssertFalse(OrlixUpstreamTestRunSpec.kernelVirtioMMIOContract.hostDirectoryFixture)
        XCTAssertFalse(OrlixUpstreamTestRunSpec.kernelVirtioNetDevice.hostDirectoryFixture)
    }

    func testVirtioBlockEnvironmentProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelVirtioBlockEnvironment)
    }

    func testRlimitProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelRlimit)
    }

    func testStackGrowthProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelStackGrowth)
    }

    func testProcessCapabilityProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelProcessCapability)
    }

    func testUmaskProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelUmask)
    }

    func testReadonlyRootProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelReadonlyRoot)
    }

    func testHostnameDomainnameProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelHostnameDomainname)
    }

    func testNamespaceProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelNamespace)
    }

    func testNetworkNamespaceProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelNetworkNamespace)
    }

    func testEnvironmentStateWritebackProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelEnvironmentStateWriteback)
    }

    func testEnvironmentStateCrossbootWriteProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelEnvironmentStateCrossbootWrite)
    }

    func testEnvironmentStateCrossbootVerifyProbeCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelEnvironmentStateCrossbootVerify)
    }

    func testKselftestRootfsCompletesThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernel)
    }

    func testOrlixTCTIAtomicMemoryKUnitDiagnosticRunsThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelTCTIAtomicMemoryDiagnostic)
    }

    func testOrlixTCTIKthreadHandoffKUnitDiagnosticRunsThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelTCTIKthreadHandoffDiagnostic)
    }

    func testOrlixTCTINativeObservationKUnitDiagnosticRunsThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelTCTINativeObservationDiagnostic)
    }

    func testOrlixTCTINativeObservationProductionKUnitDiagnosticRunsThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelTCTINativeObservationProductionDiagnostic)
    }

    func testOrlixTCTIBaseLoadStoreKUnitDiagnosticRunsThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelTCTIBaseLoadStoreDiagnostic)
    }

    func testOrlixTCTIAdvsimdLoadStoreKUnitDiagnosticRunsThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelTCTIAdvsimdLoadStoreDiagnostic)
    }

    func testOrlixTCTIBaseAtomicKUnitDiagnosticRunsThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelTCTIBaseAtomicDiagnostic)
    }

    func testOrlixTCTIBaseAddSubKUnitDiagnosticRunsThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelTCTIBaseAddSubDiagnostic)
    }

    func testOrlixTCTIBaseControlFlowKUnitDiagnosticRunsThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelTCTIBaseControlFlowDiagnostic)
    }

    func testOrlixTCTIBaseExceptionsKUnitDiagnosticRunsThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelTCTIBaseExceptionsDiagnostic)
    }

    func testOrlixTCTIBaseConditionalKUnitDiagnosticRunsThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelTCTIBaseConditionalDiagnostic)
    }

    func testOrlixTCTIBaseBitfieldUnaryKUnitDiagnosticRunsThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelTCTIBaseBitfieldUnaryDiagnostic)
    }

    func testOrlixTCTIBaseMultiplyDivideKUnitDiagnosticRunsThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelTCTIBaseMultiplyDivideDiagnostic)
    }

    func testOrlixTCTIAdvSIMDCryptoKUnitDiagnosticRunsThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelTCTIAdvSIMDCryptoDiagnostic)
    }

    func testOrlixTCTIAdvSIMDPermuteMoveKUnitDiagnosticRunsThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelTCTIAdvSIMDPermuteMoveDiagnostic)
    }

    func testOrlixTCTIAdvSIMDIntegerKUnitDiagnosticRunsThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelTCTIAdvSIMDIntegerDiagnostic)
    }

    func testOrlixTCTIAdvSIMDFPKUnitDiagnosticRunsThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelTCTIAdvSIMDFPDiagnostic)
    }

    func testOrlixTCTIScalarFPKUnitDiagnosticRunsThroughOrlixOSTerminalSession() throws {
        try OrlixUpstreamXCTest.run(.kernelTCTIScalarFPDiagnostic)
    }
}
