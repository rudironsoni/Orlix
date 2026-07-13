import Foundation
import OpenTelemetryApi
import OpenTelemetryProtocolExporterHttp
import OpenTelemetrySdk

enum OrlixTelemetryEnvironment: String, Equatable {
    case development
    case simulatorValidation = "simulator-validation"
    case testflight
}

enum OrlixAnalyticsEvent: String, CaseIterable {
    case appStarted = "app_started"
    case terminalActivated = "terminal_activated"
    case linuxSessionUnavailable = "linux_session_unavailable"
    case bootStarted = "boot_started"
    case bootFinished = "boot_finished"
    case firstTerminalOutput = "first_terminal_output"
    case bootWatchdogFired = "boot_watchdog_fired"
    case themeChanged = "theme_changed"
}

struct OrlixTrackPayload: Equatable {
    let event: OrlixAnalyticsEvent
}

enum OrlixBootOutcome: String, Equatable {
    case succeeded
    case failed
}

enum OrlixDiagnosticEvent: Equatable {
    case appLaunch(startedAt: Date, finishedAt: Date)
    case linuxBoot(startedAt: Date, finishedAt: Date, outcome: OrlixBootOutcome)
    case watchdog
    case firstTerminalOutput(latencyMilliseconds: Double)
}

struct OrlixTelemetryConfiguration: Equatable {
    let analyticsEnabled: Bool
    let observabilityEnabled: Bool
    let openPanelEndpoint: URL?
    let openPanelClientID: String?
    let signozEndpoint: URL?
    let signozIngestionKey: String?
    let environment: OrlixTelemetryEnvironment
    let serviceName = "orlix-ios"
    let serviceVersion: String

    init(
        environment processEnvironment: [String: String] = ProcessInfo.processInfo.environment,
        info: [String: Any] = Bundle.main.infoDictionary ?? [:]
    ) {
        analyticsEnabled = Self.featureEnabled(
            processEnvironment["ORLIX_ANALYTICS_ENABLED"] ?? info["ORLIXAnalyticsEnabled"] as? String
        )
        observabilityEnabled = Self.featureEnabled(
            processEnvironment["ORLIX_OBSERVABILITY_ENABLED"] ?? info["ORLIXObservabilityEnabled"] as? String
        )
        openPanelEndpoint = Self.endpoint(
            processEnvironment["ORLIX_OPENPANEL_TRACK_ENDPOINT"] ?? info["ORLIXOpenPanelTrackEndpoint"] as? String,
            requiredHost: "analytics.orlix.rudironsoni.cloud",
            requiredPath: "/track"
        )
        openPanelClientID = Self.nonPlaceholder(
            processEnvironment["ORLIX_OPENPANEL_CLIENT_ID"] ?? info["ORLIXOpenPanelClientID"] as? String
        )
        signozEndpoint = Self.endpoint(
            processEnvironment["ORLIX_SIGNOZ_OTLP_HTTP_ENDPOINT"] ?? info["ORLIXSigNozOTLPHTTPEndpoint"] as? String,
            requiredHost: "otlp.orlix.rudironsoni.cloud"
        )
        signozIngestionKey = Self.nonPlaceholder(
            processEnvironment["ORLIX_SIGNOZ_INGESTION_KEY"] ?? info["ORLIXSigNozIngestionKey"] as? String
        )

        let marketingVersion = info["CFBundleShortVersionString"] as? String ?? "unknown"
        let buildNumber = info["CFBundleVersion"] as? String ?? "unknown"
        serviceVersion = "\(marketingVersion).\(buildNumber)"

        let configuredEnvironment = processEnvironment["ORLIX_TELEMETRY_ENVIRONMENT"]
            ?? info["ORLIXTelemetryEnvironment"] as? String
        if processEnvironment["SIMULATOR_UDID"] != nil || processEnvironment["XCTestConfigurationFilePath"] != nil {
            environment = .simulatorValidation
        } else {
            environment = configuredEnvironment.flatMap(OrlixTelemetryEnvironment.init(rawValue:)) ?? .development
        }
    }

    var analyticsConfigured: Bool {
        analyticsEnabled && openPanelEndpoint != nil && openPanelClientID != nil
    }

    var diagnosticsConfigured: Bool {
        observabilityEnabled && signozEndpoint != nil && signozIngestionKey != nil
    }

    private static func featureEnabled(_ value: String?) -> Bool {
        guard let value else { return false }
        return ["1", "true", "yes"].contains(value.lowercased())
    }

    private static func endpoint(
        _ value: String?,
        requiredHost: String,
        requiredPath: String? = nil
    ) -> URL? {
        guard let value = nonPlaceholder(value),
              let url = URL(string: value),
              url.scheme == "https",
              url.host == requiredHost,
              requiredPath == nil || url.path == requiredPath else {
            return nil
        }
        return url
    }

    private static func nonPlaceholder(_ value: String?) -> String? {
        guard let value else { return nil }
        let trimmed = value.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty, !trimmed.contains("$(") else { return nil }
        return trimmed
    }
}

protocol OrlixTelemetryTransport: AnyObject {
    func send(_ request: URLRequest, completion: @escaping @Sendable (Bool) -> Void)
    func cancelAll()
}

final class OrlixURLSessionTransport: OrlixTelemetryTransport, @unchecked Sendable {
    private let session: URLSession

    init(session: URLSession = URLSession(configuration: .ephemeral)) {
        self.session = session
    }

    func send(_ request: URLRequest, completion: @escaping @Sendable (Bool) -> Void) {
        session.dataTask(with: request) { _, response, error in
            let statusCode = (response as? HTTPURLResponse)?.statusCode ?? 0
            completion(error == nil && (200 ... 299).contains(statusCode))
        }.resume()
    }

    func cancelAll() {
        session.getAllTasks { tasks in tasks.forEach { $0.cancel() } }
    }
}

protocol OrlixDiagnosticsSink: AnyObject {
    func record(_ event: OrlixDiagnosticEvent)
    func flush()
    func shutdown()
}

final class OrlixOpenTelemetryDiagnosticsSink: OrlixDiagnosticsSink, @unchecked Sendable {
    private let tracerProvider: TracerProviderSdk
    private let meterProvider: MeterProviderSdk
    private let tracer: Tracer
    private var appLaunchCounter: any LongCounter
    private var bootOutcomeCounter: any LongCounter
    private var watchdogCounter: any LongCounter
    private var bootDurationHistogram: any DoubleHistogram
    private var firstOutputLatencyHistogram: any DoubleHistogram

    init?(configuration: OrlixTelemetryConfiguration) {
        guard let endpoint = configuration.signozEndpoint,
              let ingestionKey = configuration.signozIngestionKey else {
            return nil
        }

        let headers = [("signoz-ingestion-key", ingestionKey)]
        let resource = Resource(attributes: [
            "service.name": .string(configuration.serviceName),
            "service.version": .string(configuration.serviceVersion),
            "deployment.environment.name": .string(configuration.environment.rawValue),
        ])
        let traceExporter = OtlpHttpTraceExporter(
            endpoint: endpoint.appendingPathComponent("v1/traces"),
            envVarHeaders: headers
        )
        tracerProvider = TracerProviderBuilder()
            .with(resource: resource)
            .add(spanProcessor: SimpleSpanProcessor(spanExporter: traceExporter))
            .build()
        tracer = tracerProvider.get(instrumentationName: "orlix-ios")

        let metricExporter = OtlpHttpMetricExporter(
            endpoint: endpoint.appendingPathComponent("v1/metrics"),
            envVarHeaders: headers
        )
        let metricReader = PeriodicMetricReaderBuilder(exporter: metricExporter)
            .setInterval(timeInterval: 30)
            .build()
        meterProvider = MeterProviderSdk.builder()
            .setResource(resource: resource)
            .registerMetricReader(reader: metricReader)
            .build()
        let meter = meterProvider.get(name: "orlix-ios")
        appLaunchCounter = meter.counterBuilder(name: "orlix.app.launch.count").build()
        bootOutcomeCounter = meter.counterBuilder(name: "orlix.linux.boot.outcome.count").build()
        watchdogCounter = meter.counterBuilder(name: "orlix.linux.boot.watchdog.count").build()
        bootDurationHistogram = meter.histogramBuilder(name: "orlix.linux.boot.duration").setUnit("ms").build()
        firstOutputLatencyHistogram = meter.histogramBuilder(name: "orlix.terminal.first_output.latency").setUnit("ms").build()
    }

    func record(_ event: OrlixDiagnosticEvent) {
        switch event {
        case let .appLaunch(startedAt, finishedAt):
            let span = tracer.spanBuilder(spanName: "orlix.app.launch")
                .setNoParent()
                .setStartTime(time: startedAt)
                .startSpan()
            span.end(time: finishedAt)
            appLaunchCounter.add(value: 1)
        case let .linuxBoot(startedAt, finishedAt, outcome):
            let span = tracer.spanBuilder(spanName: "orlix.linux.boot")
                .setNoParent()
                .setStartTime(time: startedAt)
                .setAttribute(key: "orlix.outcome", value: outcome.rawValue)
                .startSpan()
            span.end(time: finishedAt)
            let attributes = ["orlix.outcome": AttributeValue.string(outcome.rawValue)]
            bootOutcomeCounter.add(value: 1, attributes: attributes)
            bootDurationHistogram.record(
                value: max(0, finishedAt.timeIntervalSince(startedAt) * 1_000),
                attributes: attributes
            )
        case .watchdog:
            watchdogCounter.add(value: 1)
        case let .firstTerminalOutput(latencyMilliseconds):
            firstOutputLatencyHistogram.record(value: max(0, latencyMilliseconds))
        }
    }

    func shutdown() {
        tracerProvider.shutdown()
        _ = meterProvider.shutdown()
    }

    func flush() {
        tracerProvider.forceFlush()
        _ = meterProvider.forceFlush()
    }
}

final class OrlixTelemetry: @unchecked Sendable {
    static let shared = OrlixTelemetry()

    static let analyticsOptOutKey = "Orlix.telemetry.analyticsOptOut"
    static let diagnosticsOptOutKey = "Orlix.telemetry.diagnosticsOptOut"

    private let configuration: OrlixTelemetryConfiguration
    private let transport: OrlixTelemetryTransport
    private let diagnosticsFactory: () -> OrlixDiagnosticsSink?
    private let queue = DispatchQueue(label: "com.rudironsoni.orlix.telemetry", qos: .utility)
    private let defaults: UserDefaults
    private let queueLimit = 64
    private var analyticsQueue: [OrlixTrackPayload] = []
    private var analyticsRequestInFlight = false
    private var analyticsGeneration = 0
    private var diagnosticsSink: OrlixDiagnosticsSink?
    private var analyticsEnabled: Bool
    private var diagnosticsEnabled: Bool

    init(
        configuration: OrlixTelemetryConfiguration = .init(),
        transport: OrlixTelemetryTransport = OrlixURLSessionTransport(),
        defaults: UserDefaults = .standard,
        diagnosticsFactory: (() -> OrlixDiagnosticsSink?)? = nil
    ) {
        self.configuration = configuration
        self.transport = transport
        self.defaults = defaults
        self.diagnosticsFactory = diagnosticsFactory ?? {
            OrlixOpenTelemetryDiagnosticsSink(configuration: configuration)
        }
        analyticsEnabled = !defaults.bool(forKey: Self.analyticsOptOutKey)
        diagnosticsEnabled = !defaults.bool(forKey: Self.diagnosticsOptOutKey)
        if diagnosticsEnabled && configuration.diagnosticsConfigured {
            diagnosticsSink = self.diagnosticsFactory()
        }
    }

    var isAnalyticsEnabled: Bool { queue.sync { analyticsEnabled } }
    var isDiagnosticsEnabled: Bool { queue.sync { diagnosticsEnabled } }
    var isAnalyticsAvailable: Bool { configuration.analyticsConfigured }
    var isDiagnosticsAvailable: Bool { configuration.diagnosticsConfigured }
    var pendingAnalyticsCount: Int { queue.sync { analyticsQueue.count } }

    func synchronizeForTesting() {
        queue.sync {}
    }

    func track(_ event: OrlixAnalyticsEvent) {
        queue.async { [self] in
            guard analyticsEnabled, configuration.analyticsConfigured else { return }
            guard analyticsQueue.count < queueLimit else { return }
            analyticsQueue.append(OrlixTrackPayload(event: event))
            drainAnalytics()
        }
    }

    func record(_ event: OrlixDiagnosticEvent) {
        queue.async { [self] in
            guard diagnosticsEnabled else { return }
            diagnosticsSink?.record(event)
        }
    }

    func flushDiagnostics() {
        queue.async { [self] in
            guard diagnosticsEnabled else { return }
            diagnosticsSink?.flush()
        }
    }

    func setAnalyticsEnabled(_ enabled: Bool) {
        queue.sync {
            guard analyticsEnabled != enabled else { return }
            analyticsEnabled = enabled
            defaults.set(!enabled, forKey: Self.analyticsOptOutKey)
            guard !enabled else { return }
            analyticsGeneration += 1
            analyticsQueue.removeAll(keepingCapacity: true)
            analyticsRequestInFlight = false
            transport.cancelAll()
        }
    }

    func setDiagnosticsEnabled(_ enabled: Bool) {
        queue.sync {
            guard diagnosticsEnabled != enabled else { return }
            diagnosticsEnabled = enabled
            defaults.set(!enabled, forKey: Self.diagnosticsOptOutKey)
            if enabled && configuration.diagnosticsConfigured {
                diagnosticsSink = diagnosticsFactory()
            } else {
                diagnosticsSink?.shutdown()
                diagnosticsSink = nil
            }
        }
    }

    private func drainAnalytics() {
        guard !analyticsRequestInFlight,
              let endpoint = configuration.openPanelEndpoint,
              let clientID = configuration.openPanelClientID,
              let payload = analyticsQueue.first else {
            return
        }
        analyticsRequestInFlight = true
        let generation = analyticsGeneration
        let body = OpenPanelRequest(
            type: "track",
            payload: .init(
                name: payload.event.rawValue,
                properties: [
                    "environment": configuration.environment.rawValue,
                    "service_version": configuration.serviceVersion,
                ]
            )
        )
        guard let data = try? JSONEncoder().encode(body) else {
            analyticsQueue.removeFirst()
            analyticsRequestInFlight = false
            drainAnalytics()
            return
        }
        var request = URLRequest(url: endpoint)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.setValue(clientID, forHTTPHeaderField: "openpanel-client-id")
        request.httpBody = data
        transport.send(request) { [weak self] _ in
            self?.queue.async { [weak self] in
                guard let self, generation == analyticsGeneration else { return }
                if !analyticsQueue.isEmpty {
                    analyticsQueue.removeFirst()
                }
                analyticsRequestInFlight = false
                drainAnalytics()
            }
        }
    }
}

private struct OpenPanelRequest: Encodable {
    struct Payload: Encodable {
        let name: String
        let properties: [String: String]
    }

    let type: String
    let payload: Payload
}
