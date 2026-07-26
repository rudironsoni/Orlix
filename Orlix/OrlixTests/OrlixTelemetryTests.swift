import Foundation
import XCTest

@testable import Orlix

final class TelemetryTests: XCTestCase {
    private var defaults: UserDefaults!

    override func setUp() {
        super.setUp()
        defaults = UserDefaults(suiteName: "OrlixTests.\(UUID().uuidString)")!
    }

    func testAnalyticsEventVocabularyIsExact() {
        XCTAssertEqual(
            Set(OrlixAnalyticsEvent.allCases.map(\.rawValue)),
            [
                "app_started", "app_launched", "connection_succeeded", "paywall_viewed",
                "paywall_cta_tapped", "purchase_started", "purchased", "purchase_succeeded",
                "purchase_cancelled", "purchase_pending", "purchase_failed", "limit_hit",
                "free_plan_generation_assigned", "welcome_completed", "custom_action_created",
                "split_pane_created", "review_prompt_requested", "analytics_disabled",
                "terminal_activated", "linux_session_unavailable",
                "boot_started", "boot_finished", "first_terminal_output",
                "boot_watchdog_fired", "theme_changed",
            ]
        )
    }

    func testConfigurationUsesBuildMetadataAndRejectsMissingCredentials() {
        let configuration = OrlixTelemetryConfiguration(
            environment: [:],
            info: [
                "CFBundleShortVersionString": "0.1",
                "CFBundleVersion": "24",
                "ORLIXOpenPanelTrackEndpoint": "https://analytics.orlix.rudironsoni.cloud/track",
                "ORLIXOpenPanelClientID": "$(ORLIX_OPENPANEL_CLIENT_ID)",
                "ORLIXSigNozOTLPHTTPEndpoint": "https://otlp.orlix.rudironsoni.cloud",
                "ORLIXSigNozIngestionKey": "",
                "ORLIXTelemetryEnvironment": "testflight",
            ]
        )
        XCTAssertEqual(configuration.serviceVersion, "0.1.24")
        XCTAssertEqual(configuration.environment, .testflight)
        XCTAssertFalse(configuration.analyticsConfigured)
        XCTAssertFalse(configuration.diagnosticsConfigured)
    }

    func testTelemetryFeaturesDefaultOffEvenWithCredentials() {
        let configuration = OrlixTelemetryConfiguration(
            environment: [
                "ORLIX_OPENPANEL_TRACK_ENDPOINT": "https://analytics.orlix.rudironsoni.cloud/track",
                "ORLIX_OPENPANEL_CLIENT_ID": "client",
                "ORLIX_SIGNOZ_OTLP_HTTP_ENDPOINT": "https://otlp.orlix.rudironsoni.cloud",
                "ORLIX_SIGNOZ_INGESTION_KEY": "key",
            ],
            info: [:]
        )
        XCTAssertFalse(configuration.analyticsEnabled)
        XCTAssertFalse(configuration.observabilityEnabled)
        XCTAssertFalse(configuration.analyticsConfigured)
        XCTAssertFalse(configuration.diagnosticsConfigured)
    }

    func testConfigurationRejectsUnapprovedIngestionHostsAndPaths() {
        let configuration = OrlixTelemetryConfiguration(
            environment: [
                "ORLIX_OPENPANEL_TRACK_ENDPOINT": "https://example.com/track",
                "ORLIX_OPENPANEL_CLIENT_ID": "client",
                "ORLIX_SIGNOZ_OTLP_HTTP_ENDPOINT": "https://example.com",
                "ORLIX_SIGNOZ_INGESTION_KEY": "key",
            ],
            info: [:]
        )
        XCTAssertFalse(configuration.analyticsConfigured)
        XCTAssertFalse(configuration.diagnosticsConfigured)

        let wrongPath = OrlixTelemetryConfiguration(
            environment: [
                "ORLIX_OPENPANEL_TRACK_ENDPOINT": "https://analytics.orlix.rudironsoni.cloud/api",
                "ORLIX_OPENPANEL_CLIENT_ID": "client",
            ],
            info: [:]
        )
        XCTAssertFalse(wrongPath.analyticsConfigured)
    }

    func testOpenPanelRequestMatchesTrackAPIContract() throws {
        let transport = RecordingTransport(autoComplete: false)
        let telemetry = makeTelemetry(transport: transport)

        telemetry.track(.appStarted)
        telemetry.synchronizeForTesting()

        let request = try XCTUnwrap(transport.requests.first)
        XCTAssertEqual(request.url?.absoluteString, "https://analytics.orlix.rudironsoni.cloud/track")
        XCTAssertEqual(request.value(forHTTPHeaderField: "Content-Type"), "application/json")
        XCTAssertEqual(request.value(forHTTPHeaderField: "openpanel-client-id"), "beta-client")
        let body = try XCTUnwrap(request.httpBody)
        let json = try XCTUnwrap(JSONSerialization.jsonObject(with: body) as? [String: Any])
        XCTAssertEqual(json["type"] as? String, "track")
        XCTAssertNil(json["event"])
        let payload = try XCTUnwrap(json["payload"] as? [String: Any])
        XCTAssertEqual(payload["name"] as? String, "app_started")
        let properties = try XCTUnwrap(payload["properties"] as? [String: String])
        XCTAssertEqual(properties, ["environment": "testflight", "service_version": "0.1.24"])
    }

    func testOpenPanelRequestPreservesFeatureProperties() throws {
        let transport = RecordingTransport(autoComplete: false)
        let telemetry = makeTelemetry(transport: transport)

        telemetry.track(.purchaseFailed, properties: [
            "source": "settings",
            "product": "com.rudironsoni.Orlix.pro.monthly",
            "reason": "cancelled",
        ])
        telemetry.synchronizeForTesting()

        let request = try XCTUnwrap(transport.requests.first)
        let body = try XCTUnwrap(request.httpBody)
        let json = try XCTUnwrap(JSONSerialization.jsonObject(with: body) as? [String: Any])
        let payload = try XCTUnwrap(json["payload"] as? [String: Any])
        XCTAssertEqual(payload["name"] as? String, "purchase_failed")
        let properties = try XCTUnwrap(payload["properties"] as? [String: String])
        XCTAssertEqual(properties["source"], "settings")
        XCTAssertEqual(properties["product"], "com.rudironsoni.Orlix.pro.monthly")
        XCTAssertEqual(properties["reason"], "cancelled")
        XCTAssertEqual(properties["environment"], "testflight")
        XCTAssertEqual(properties["service_version"], "0.1.24")
    }

    func testAnalyticsQueueBoundsOutstandingWorkAndOptOutCancelsIt() {
        let transport = RecordingTransport(autoComplete: false)
        let telemetry = makeTelemetry(transport: transport)

        for _ in 0 ..< 1_000 {
            telemetry.track(.terminalActivated)
        }
        telemetry.synchronizeForTesting()

        XCTAssertEqual(transport.requests.count, 1)
        XCTAssertEqual(telemetry.pendingAnalyticsCount, 64)
        telemetry.setAnalyticsEnabled(false)
        XCTAssertEqual(telemetry.pendingAnalyticsCount, 0)
        XCTAssertEqual(transport.cancelCount, 1)
        XCTAssertTrue(defaults.bool(forKey: OrlixTelemetry.analyticsOptOutKey))
    }

    func testAnalyticsAndDiagnosticsOptOutsAreIndependent() {
        let transport = RecordingTransport()
        let diagnostics = RecordingDiagnosticsSink()
        let telemetry = makeTelemetry(transport: transport, diagnostics: diagnostics)

        telemetry.setAnalyticsEnabled(false)
        telemetry.record(.watchdog)
        telemetry.synchronizeForTesting()
        XCTAssertEqual(diagnostics.events, [.watchdog])
        XCTAssertFalse(defaults.bool(forKey: OrlixTelemetry.diagnosticsOptOutKey))

        telemetry.setDiagnosticsEnabled(false)
        telemetry.setAnalyticsEnabled(true)
        telemetry.track(.themeChanged)
        telemetry.synchronizeForTesting()
        XCTAssertTrue(diagnostics.didShutdown)
        XCTAssertEqual(transport.requests.count, 1)
    }

    func testDiagnosticEventVocabularyContainsOnlyApprovedTypedValues() {
        let diagnostics = RecordingDiagnosticsSink()
        let telemetry = makeTelemetry(transport: RecordingTransport(), diagnostics: diagnostics)
        let start = Date(timeIntervalSince1970: 100)
        let finish = Date(timeIntervalSince1970: 102)

        telemetry.record(.appLaunch(startedAt: start, finishedAt: finish))
        telemetry.record(.linuxBoot(startedAt: start, finishedAt: finish, outcome: .succeeded))
        telemetry.record(.watchdog)
        telemetry.record(.firstTerminalOutput(latencyMilliseconds: 15))
        telemetry.synchronizeForTesting()

        XCTAssertEqual(diagnostics.events.count, 4)
        XCTAssertEqual(
            diagnostics.events[1],
            .linuxBoot(startedAt: start, finishedAt: finish, outcome: .succeeded)
        )
    }

    func testDiagnosticsFlushesOnLifecycleRequest() {
        let diagnostics = RecordingDiagnosticsSink()
        let telemetry = makeTelemetry(transport: RecordingTransport(), diagnostics: diagnostics)

        telemetry.record(.watchdog)
        telemetry.flushDiagnostics()
        telemetry.synchronizeForTesting()

        XCTAssertEqual(diagnostics.events, [.watchdog])
        XCTAssertEqual(diagnostics.flushCount, 1)
    }

    func testPersistedOptOutsDisableNewTelemetryInstance() {
        defaults.set(true, forKey: OrlixTelemetry.analyticsOptOutKey)
        defaults.set(true, forKey: OrlixTelemetry.diagnosticsOptOutKey)
        let transport = RecordingTransport()
        let diagnostics = RecordingDiagnosticsSink()
        let telemetry = makeTelemetry(transport: transport, diagnostics: diagnostics)

        telemetry.track(.appStarted)
        telemetry.record(.watchdog)
        telemetry.flushDiagnostics()
        telemetry.synchronizeForTesting()

        XCTAssertFalse(telemetry.isAnalyticsEnabled)
        XCTAssertFalse(telemetry.isDiagnosticsEnabled)
        XCTAssertTrue(transport.requests.isEmpty)
        XCTAssertTrue(diagnostics.events.isEmpty)
        XCTAssertEqual(diagnostics.flushCount, 0)
    }

    func testPrivacyManifestDeclaresCollectedAnalyticsAndDiagnosticsWithoutTracking() throws {
        let testFile = URL(fileURLWithPath: #filePath)
        let appRoot = testFile.deletingLastPathComponent().deletingLastPathComponent()
        let manifest = appRoot.appendingPathComponent("Orlix/PrivacyInfo.xcprivacy")
        let data = try Data(contentsOf: manifest)
        let plist = try XCTUnwrap(
            PropertyListSerialization.propertyList(from: data, format: nil) as? [String: Any]
        )
        XCTAssertEqual(plist["NSPrivacyTracking"] as? Bool, false)
        let collected = try XCTUnwrap(plist["NSPrivacyCollectedDataTypes"] as? [[String: Any]])
        XCTAssertEqual(
            Set(collected.compactMap { $0["NSPrivacyCollectedDataType"] as? String }),
            [
                "NSPrivacyCollectedDataTypeProductInteraction",
                "NSPrivacyCollectedDataTypePurchaseHistory",
                "NSPrivacyCollectedDataTypePerformanceData",
                "NSPrivacyCollectedDataTypeOtherDiagnosticData",
            ]
        )
        XCTAssertTrue(collected.allSatisfy { ($0["NSPrivacyCollectedDataTypeTracking"] as? Bool) == false })
        XCTAssertTrue(collected.allSatisfy { ($0["NSPrivacyCollectedDataTypeLinked"] as? Bool) == false })
    }

    private func makeTelemetry(
        transport: RecordingTransport,
        diagnostics: RecordingDiagnosticsSink = RecordingDiagnosticsSink()
    ) -> OrlixTelemetry {
        OrlixTelemetry(
            configuration: OrlixTelemetryConfiguration(
                environment: [
                    "ORLIX_ANALYTICS_ENABLED": "YES",
                    "ORLIX_OBSERVABILITY_ENABLED": "YES",
                    "ORLIX_OPENPANEL_TRACK_ENDPOINT": "https://analytics.orlix.rudironsoni.cloud/track",
                    "ORLIX_OPENPANEL_CLIENT_ID": "beta-client",
                    "ORLIX_SIGNOZ_OTLP_HTTP_ENDPOINT": "https://otlp.orlix.rudironsoni.cloud",
                    "ORLIX_SIGNOZ_INGESTION_KEY": "beta-ingestion",
                    "ORLIX_TELEMETRY_ENVIRONMENT": "testflight",
                ],
                info: ["CFBundleShortVersionString": "0.1", "CFBundleVersion": "24"]
            ),
            transport: transport,
            defaults: defaults,
            diagnosticsFactory: { diagnostics }
        )
    }
}

private final class RecordingTransport: OrlixTelemetryTransport, @unchecked Sendable {
    private let lock = NSLock()
    private let autoComplete: Bool
    private(set) var requests: [URLRequest] = []
    private(set) var cancelCount = 0

    init(autoComplete: Bool = true) {
        self.autoComplete = autoComplete
    }

    func send(_ request: URLRequest, completion: @escaping @Sendable (Bool) -> Void) {
        lock.lock()
        requests.append(request)
        lock.unlock()
        if autoComplete {
            completion(true)
        }
    }

    func cancelAll() {
        lock.lock()
        cancelCount += 1
        lock.unlock()
    }
}

private final class RecordingDiagnosticsSink: OrlixDiagnosticsSink {
    private(set) var events: [OrlixDiagnosticEvent] = []
    private(set) var didShutdown = false
    private(set) var flushCount = 0

    func record(_ event: OrlixDiagnosticEvent) {
        events.append(event)
    }

    func shutdown() {
        didShutdown = true
    }

    func flush() {
        flushCount += 1
    }
}
