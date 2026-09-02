import Foundation
import XCTest
@testable import Orlix

final class TSSHNativeLiveTests: XCTestCase {
    func testPinnedNativeKCPTransportAgainstLiveTSSHD() async throws {
        guard let json = ProcessInfo.processInfo.environment["ORLIX_TSSH_LIVE_SERVER_INFO"] else {
            throw XCTSkip("Set ORLIX_TSSH_LIVE_SERVER_INFO to run the live native TSSH test.")
        }
        let host = ProcessInfo.processInfo.environment["ORLIX_TSSH_LIVE_HOST"] ?? "127.0.0.1"
        let info = try TSSHServerInfo.parse(output: json).assigningClientIDIfNeeded()
        let gate = TSSHCallGate.shared
        let transport = try await gate.connect(TSSHTransportParameters(
            host: host,
            info: info,
            mtu: 0,
            connectTimeoutSeconds: 10,
            aliveTimeoutSeconds: 60,
            heartbeatTimeoutSeconds: 5,
            debugLabel: "Orlix live test"
        ))

        let state = TSSHStateBridge { _ in }
        let health = TSSHHealthBridge { _, _ in }
        let discard = TSSHDiscardBridge { _, _, _ in }
        try await gate.configure(
            transport,
            keepPendingInput: false,
            keepPendingOutput: false,
            state: state,
            health: health,
            discard: discard
        )

        let commandOutput = try await gate.runCommand(
            "printf orlix-tssh-command-ok",
            on: transport
        )
        XCTAssertEqual(String(decoding: commandOutput, as: UTF8.self), "orlix-tssh-command-ok")

        let exec = try await gate.openExec("printf orlix-tssh-exec-ok", on: transport)
        var execOutput = Data()
        while let chunk = try await gate.readExec(exec, standardError: false, maximumBytes: 4_096) {
            execOutput.append(chunk)
        }
        XCTAssertEqual(String(decoding: execOutput, as: UTF8.self), "orlix-tssh-exec-ok")
        let execExitCode = await gate.execExitCode(exec)
        XCTAssertEqual(execExitCode, 0)
        await gate.closeExec(exec)

        let terminalOutput = TSSHLiveOutputBuffer()
        let exited = expectation(description: "TSSH command session exited")
        let output = TSSHOutputBridge(
            output: { terminalOutput.append($0) },
            failure: { XCTFail($0) },
            exit: { code in
                XCTAssertEqual(code, 0)
                exited.fulfill()
            },
            close: {}
        )
        let session = try await gate.openSession(
            on: transport,
            term: "xterm-256color",
            rows: 24,
            columns: 80,
            requestAgent: false,
            command: "printf orlix-tssh-session-ok",
            output: output
        ).0
        await fulfillment(of: [exited], timeout: 10)
        XCTAssertTrue(terminalOutput.string.contains("orlix-tssh-session-ok"))

        let statistics = await gate.statistics(for: transport)
        let transportHealth = await gate.health(for: transport)
        XCTAssertNotNil(statistics)
        XCTAssertEqual(transportHealth?.isAlive, true)
        await gate.closeSession(session)
        await gate.closeTransport(transport, preserveServer: false)
    }
}

private final class TSSHLiveOutputBuffer: @unchecked Sendable {
    private let lock = NSLock()
    private var data = Data()

    func append(_ value: Data) { lock.withLock { data.append(value) } }
    var string: String { lock.withLock { String(decoding: data, as: UTF8.self) } }
}
