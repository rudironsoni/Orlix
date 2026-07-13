import GhosttyTerminal
import GhosttyTheme
import OrlixOS
import os
import UIKit

final class TerminalViewController: UIViewController {
    private static let lightThemeKey = "SelectedTheme.light"
    private static let darkThemeKey = "SelectedTheme.dark"
#if DEBUG || ORLIX_BETA_OBSERVABILITY
    private static let terminalUILogger = Logger(
        subsystem: "com.rudironsoni.Orlix",
        category: "terminal-ui"
    )
#endif

    private var didStartBoot = false
    private var bootStartedAt: Date?
    private var didRecordFirstTerminalOutput = false
    private let bootQueue = DispatchQueue(label: "com.rudironsoni.terminal.boot", qos: .userInitiated)
    private let terminalOutputLock = NSLock()
    private var pendingTerminalOutput = ""
    private var terminalOutputFlushScheduled = false
    private var bootWatchdogWorkItem: DispatchWorkItem?
    private let launchConfiguration: OrlixLaunchConfiguration
    private lazy var linuxSessionResult = launchConfiguration.makeLinuxSession()
    private var terminalOutput: OrlixTerminalOutput?
    private lazy var terminalView = TerminalView(frame: .zero)
    private let simulatorCapture = SimulatorTerminalCapture()
    private lazy var terminalSession: InMemoryTerminalSession = {
        return InMemoryTerminalSession(
            write: { [weak self] data in
                self?.linuxSession?.terminal.send(data)
            },
            resize: { _ in }
        )
    }()
    private lazy var controller = TerminalController(
        theme: Self.savedTerminalTheme()
    ) { builder in
        builder.withBackgroundOpacity(0)
    }

    init(launchConfiguration: OrlixLaunchConfiguration = .current()) {
        self.launchConfiguration = launchConfiguration
        super.init(nibName: nil, bundle: nil)
    }

    @available(*, unavailable)
    required init?(coder _: NSCoder) {
        fatalError("init(coder:) is unavailable")
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Orlix"
        view.backgroundColor = .systemBackground
        view.isOpaque = true
        configureTerminalView()
        attachTerminalOutput()
        configureThemeMenu()
        applyBackgroundForCurrentAppearance()
    }

    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        activateTerminal()
    }

    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        terminalView.fitToSize()
    }

    override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
        super.traitCollectionDidChange(previousTraitCollection)
        guard traitCollection.hasDifferentColorAppearance(comparedTo: previousTraitCollection) else {
            return
        }
        controller.setTheme(Self.savedTerminalTheme())
        applyBackgroundForCurrentAppearance()
    }

    private func configureTerminalView() {
        terminalView.delegate = self
        terminalView.configuration = TerminalSurfaceOptions(
            backend: .inMemory(terminalSession)
        )
        terminalView.controller = controller
        terminalView.backgroundColor = .clear
        terminalView.isOpaque = false
        terminalView.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(terminalView)

        NSLayoutConstraint.activate([
            terminalView.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            terminalView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            terminalView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            terminalView.bottomAnchor.constraint(equalTo: view.keyboardLayoutGuide.topAnchor),
        ])
    }

    private func activateTerminal() {
        terminalView.becomeFirstResponder()
        guard !Self.isRunningUnitTests() else {
            return
        }
        guard !didStartBoot else {
            return
        }
        didStartBoot = true
        OrlixTelemetry.shared.track(.terminalActivated)

        simulatorCapture.reset()
        terminalSession.receive("Orlix\r\n")
        simulatorCapture.append("Orlix\r\n")
        guard let session = linuxSession else {
            OrlixTelemetry.shared.track(.linuxSessionUnavailable)
#if DEBUG || ORLIX_BETA_OBSERVABILITY
            Self.terminalUILogger.error("linux session unavailable at terminal activation")
#endif
            terminalSession.receive(launchConfiguration.failureMessage + "\r\n")
            simulatorCapture.append(launchConfiguration.failureMessage + "\r\n")
            return
        }
#if DEBUG || ORLIX_BETA_OBSERVABILITY
        Self.terminalUILogger.info(
            "boot starting profile=\(Self.profileDisplayName(session.bootConfig.profile), privacy: .public)"
        )
#endif
        let startMessage = launchConfiguration.startMessage(for: session) + "\r\n"
        terminalSession.receive(startMessage)
        simulatorCapture.append(startMessage)
        let startedAt = Date()
        terminalOutputLock.lock()
        bootStartedAt = startedAt
        terminalOutputLock.unlock()
        OrlixTelemetry.shared.track(.bootStarted)
        startBootWatchdog(for: session)
        bootQueue.async { [weak self] in
            let status = session.boot()
            DispatchQueue.main.async { [weak self] in
#if DEBUG || ORLIX_BETA_OBSERVABILITY
                Self.terminalUILogger.info(
                    "boot finished status=\(status.message, privacy: .public)"
                )
#endif
                self?.cancelBootWatchdog()
                let finishedAt = Date()
                let outcome: OrlixBootOutcome = status == .ok ? .succeeded : .failed
                OrlixTelemetry.shared.track(.bootFinished)
                OrlixTelemetry.shared.record(
                    .linuxBoot(startedAt: startedAt, finishedAt: finishedAt, outcome: outcome)
                )
                let statusMessage = status.message + "\r\n"
                self?.terminalSession.receive(statusMessage)
                self?.simulatorCapture.append(statusMessage)
            }
        }
    }

    private var linuxSession: OrlixLinuxSession? {
        try? linuxSessionResult.get()
    }

    private static func defaultBootProfile() -> OrlixBootProfile {
        payloadBootProfile() ?? .release
    }

    private static func isRunningUnitTests() -> Bool {
        ProcessInfo.processInfo.environment["XCTestConfigurationFilePath"] != nil
    }

    private static func payloadBootProfile() -> OrlixBootProfile? {
        OrlixOSDistribution.bundledBootProfile
    }

    private static func profileDisplayName(_ profile: OrlixBootProfile) -> String {
        switch profile {
        case .release:
            return "release"
        case .development:
            return "development"
        }
    }

    private func attachTerminalOutput() {
        terminalOutput = linuxSession?.terminal.attachOutput { [weak self] data in
            let text = String(decoding: data, as: UTF8.self)
                .replacingOccurrences(of: "\r\n", with: "\n")
                .replacingOccurrences(of: "\n", with: "\r\n")

            self?.enqueueTerminalOutput(text)
        }
    }

    private func enqueueTerminalOutput(_ text: String) {
        cancelBootWatchdog()
        terminalOutputLock.lock()
        let isFirstOutput = !didRecordFirstTerminalOutput
        let firstOutputBootStart = bootStartedAt
        if isFirstOutput {
            didRecordFirstTerminalOutput = true
        }
        pendingTerminalOutput += text
        let needsFlush = !terminalOutputFlushScheduled
        terminalOutputFlushScheduled = true
        terminalOutputLock.unlock()

        if isFirstOutput {
            OrlixTelemetry.shared.track(.firstTerminalOutput)
            if let firstOutputBootStart {
                OrlixTelemetry.shared.record(
                    .firstTerminalOutput(
                        latencyMilliseconds: Date().timeIntervalSince(firstOutputBootStart) * 1_000
                    )
                )
            }
        }
#if DEBUG || ORLIX_BETA_OBSERVABILITY
        Self.terminalUILogger.info(
            "terminal ui output received bytes=\(text.utf8.count, privacy: .public)"
        )
#endif
        guard needsFlush else { return }

        DispatchQueue.main.async { [weak self] in
            self?.flushTerminalOutput()
        }
    }

    private func flushTerminalOutput() {
        terminalOutputLock.lock()
        let text = pendingTerminalOutput
        pendingTerminalOutput = ""
        terminalOutputFlushScheduled = false
        terminalOutputLock.unlock()

        guard !text.isEmpty else { return }
        terminalSession.receive(text)
        simulatorCapture.append(text)
    }

    private func startBootWatchdog(for session: OrlixLinuxSession) {
        cancelBootWatchdog()
        let workItem = DispatchWorkItem { [weak self, weak session] in
            guard let self, let session else { return }
            OrlixTelemetry.shared.track(.bootWatchdogFired)
            OrlixTelemetry.shared.record(.watchdog)
            let message = Self.bootWatchdogMessage(for: session)
            self.terminalSession.receive(message)
            self.simulatorCapture.append(message)
        }
        bootWatchdogWorkItem = workItem
        DispatchQueue.main.asyncAfter(deadline: .now() + 5, execute: workItem)
    }

    private func cancelBootWatchdog() {
        bootWatchdogWorkItem?.cancel()
        bootWatchdogWorkItem = nil
    }

    private static func bootWatchdogMessage(for session: OrlixLinuxSession) -> String {
        let recentConsoleOutput = session.recentConsoleOutputText
        guard recentConsoleOutput.isEmpty else {
            return """
            Linux console emitted output but the terminal UI is silent.\r
            \(Self.terminalOutputTail(recentConsoleOutput))\r

            """
        }

        let stage = session.latestBootProgress?.stage ?? .unknown
        return "Boot still running: \(Self.bootStageDisplayName(stage))\r\n"
    }

    private static func terminalOutputTail(_ text: String) -> String {
        let normalized = text
            .replacingOccurrences(of: "\r\n", with: "\n")
            .replacingOccurrences(of: "\r", with: "\n")
        let suffix = String(normalized.suffix(2048))
        return suffix.replacingOccurrences(of: "\n", with: "\r\n")
    }

    private static func bootStageDisplayName(_ stage: OrlixBootStage) -> String {
        switch stage {
        case .unknown:
            return "unknown"
        case .sessionCreated:
            return "session created"
        case .payloadRegistering:
            return "payload registering"
        case .payloadRegistered:
            return "payload registered"
        case .bootloaderEntered:
            return "bootloader entered"
        case .bootConfigValidated:
            return "boot config validated"
        case .hostResourcesReady:
            return "host resources ready"
        case .kernelHandoff:
            return "kernel handoff"
        case .archEntry:
            return "architecture entry"
        case .earlyConsoleReady:
            return "early console ready"
        case .linuxStartKernel:
            return "Linux start kernel"
        case .firstConsoleOutput:
            return "first console output"
        case .failed:
            return "failed"
        }
    }

    private static func savedTerminalTheme() -> TerminalTheme {
        let lightConfig = savedThemeDefinition(forKey: lightThemeKey)?
            .toTerminalConfiguration() ?? .alabaster
        let darkConfig = savedThemeDefinition(forKey: darkThemeKey)?
            .toTerminalConfiguration() ?? .afterglow
        return TerminalTheme(light: lightConfig, dark: darkConfig)
    }

    private static func savedThemeDefinition(
        forKey key: String
    ) -> GhosttyThemeDefinition? {
        guard let name = UserDefaults.standard.string(forKey: key) else {
            return nil
        }
        return GhosttyThemeCatalog.theme(named: name)
    }

    private var isDarkMode: Bool {
        traitCollection.userInterfaceStyle == .dark
    }

    private func saveTheme(_ theme: GhosttyThemeDefinition) {
        let key = isDarkMode ? Self.darkThemeKey : Self.lightThemeKey
        UserDefaults.standard.set(theme.name, forKey: key)
    }

    private func applyBackgroundForCurrentAppearance() {
        let key = isDarkMode ? Self.darkThemeKey : Self.lightThemeKey
        guard let theme = Self.savedThemeDefinition(forKey: key) else {
            return
        }
        if let backgroundColor = UIColor(hexString: theme.background) {
            view.backgroundColor = backgroundColor
        }
    }

    private func configureThemeMenu() {
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            image: UIImage(systemName: "ellipsis.circle"),
            menu: buildOptionsMenu()
        )
    }

    private func buildOptionsMenu() -> UIMenu {
        let telemetry = OrlixTelemetry.shared
        var privacyActions: [UIMenuElement] = []
        if telemetry.isAnalyticsAvailable {
            privacyActions.append(analyticsMenuAction())
        }
        if telemetry.isDiagnosticsAvailable {
            privacyActions.append(diagnosticsMenuAction())
        }
        var children: [UIMenuElement] = [buildThemeMenu()]
        if !privacyActions.isEmpty {
            children.append(UIMenu(title: "Privacy", children: privacyActions))
        }
        return UIMenu(title: "Options", children: children)
    }

    private func analyticsMenuAction() -> UIAction {
        let analytics = UIAction(
            title: "Share Anonymous Usage Analytics",
            image: UIImage(systemName: "chart.bar"),
            state: OrlixTelemetry.shared.isAnalyticsEnabled ? .on : .off
        ) { [weak self] _ in
            let telemetry = OrlixTelemetry.shared
            telemetry.setAnalyticsEnabled(!telemetry.isAnalyticsEnabled)
            self?.configureThemeMenu()
        }
        return analytics
    }

    private func diagnosticsMenuAction() -> UIAction {
        let diagnostics = UIAction(
            title: "Share Anonymous Diagnostics",
            image: UIImage(systemName: "waveform.path.ecg"),
            state: OrlixTelemetry.shared.isDiagnosticsEnabled ? .on : .off
        ) { [weak self] _ in
            let telemetry = OrlixTelemetry.shared
            telemetry.setDiagnosticsEnabled(!telemetry.isDiagnosticsEnabled)
            self?.configureThemeMenu()
        }
        return diagnostics
    }

    private func buildThemeMenu() -> UIMenu {
        let popular = buildSubmenu(
            title: "Popular",
            themes: [
                "Dracula", "Catppuccin Mocha", "Catppuccin Latte",
                "Nord", "Solarized Dark", "Solarized Light",
                "Gruvbox Dark", "Gruvbox Light", "Tokyo Night",
                "One Half Dark", "One Half Light", "Rose Pine",
                "Monokai Pro", "GitHub Dark", "GitHub Light",
            ]
        )

        let dark = UIMenu(
            title: "Dark",
            image: UIImage(systemName: "moon.fill"),
            children: alphabeticalSubmenus(
                themes: GhosttyThemeCatalog.allThemes.filter(\.isDark)
            )
        )

        let light = UIMenu(
            title: "Light",
            image: UIImage(systemName: "sun.max.fill"),
            children: alphabeticalSubmenus(
                themes: GhosttyThemeCatalog.allThemes.filter { !$0.isDark }
            )
        )

        return UIMenu(title: "Theme", children: [popular, dark, light])
    }

    private func buildSubmenu(
        title: String,
        themes names: [String]
    ) -> UIMenu {
        let actions = names.compactMap { name -> UIAction? in
            guard let theme = GhosttyThemeCatalog.theme(named: name) else {
                return nil
            }
            return themeAction(for: theme)
        }
        return UIMenu(
            title: title,
            image: UIImage(systemName: "star.fill"),
            children: actions
        )
    }

    private func alphabeticalSubmenus(
        themes: [GhosttyThemeDefinition]
    ) -> [UIMenu] {
        var grouped: [String: [GhosttyThemeDefinition]] = [:]
        for theme in themes {
            let letter = String(theme.name.prefix(1)).uppercased()
            let key = letter.first?.isLetter == true ? letter : "#"
            grouped[key, default: []].append(theme)
        }

        return grouped.keys.sorted().map { key in
            UIMenu(
                title: key,
                children: grouped[key]!.map { themeAction(for: $0) }
            )
        }
    }

    private func themeAction(for theme: GhosttyThemeDefinition) -> UIAction {
        UIAction(title: theme.name) { [weak self] _ in
            self?.applyTheme(theme)
        }
    }

    private func applyTheme(_ theme: GhosttyThemeDefinition) {
        saveTheme(theme)
        controller.setTheme(Self.savedTerminalTheme())

        if let backgroundColor = UIColor(hexString: theme.background) {
            view.backgroundColor = backgroundColor
        }
        OrlixTelemetry.shared.track(.themeChanged)
    }
}

struct OrlixLaunchConfiguration {
    static let environmentIDArgument = "--orlix-environment-id"
    static let environmentIDDefaultsKey = "Orlix.environmentID"
    static let runArgumentsArgument = "--orlix-run"
    static let runArgumentsDefaultsKey = "Orlix.runArguments"
    static let kernelCommandLineAppendArgument = "--orlix-kernel-command-line-append"
    static let kernelCommandLineAppendDefaultsKey = "Orlix.kernelCommandLineAppend"

    let environmentID: String?
    let runArguments: [String]?
    let kernelCommandLineAppend: String?

    static func current(
        arguments: [String] = ProcessInfo.processInfo.arguments,
        defaults: UserDefaults = .standard
    ) -> OrlixLaunchConfiguration {
        OrlixLaunchConfiguration(
            environmentID: environmentID(from: arguments)
                ?? defaults.string(forKey: environmentIDDefaultsKey),
            runArguments: runArguments(from: arguments)
                ?? defaults.stringArray(forKey: runArgumentsDefaultsKey),
            kernelCommandLineAppend: kernelCommandLineAppend(from: arguments)
                ?? defaults.string(forKey: kernelCommandLineAppendDefaultsKey)
        )
    }

    func makeLinuxSession() -> Result<OrlixLinuxSession, Error> {
        Result {
            if let runArguments, !runArguments.isEmpty {
                return try OrlixOCIEnvironmentInstaller()
                    .terminalSession(arguments: runArguments)
                    .linuxSession
            }
            if let environmentID, !environmentID.isEmpty {
                return try OrlixLinuxSession(environmentID: environmentID)
            }
            let kernelCommandLine = Self.appending(
                kernelCommandLineAppend,
                to: OrlixOSDistribution.bundledKernelCommandLine
            )
            return OrlixLinuxSession(
                bootConfig: OrlixBootConfig(
                    profile: Self.defaultBootProfile(),
                    kernelCommandLine: kernelCommandLine
                )
            )
        }
    }

    func startMessage(for session: OrlixLinuxSession) -> String {
        if let runArguments, !runArguments.isEmpty {
            return "Starting Orlix run environment \(runArguments.joined(separator: " "))."
        }
        if let environmentID, !environmentID.isEmpty {
            return "Starting Orlix environment \(environmentID)."
        }
        return "Starting Orlix bootloader with the \(Self.profileDisplayName(session.bootConfig.profile)) profile."
    }

    var failureMessage: String {
        if let runArguments, !runArguments.isEmpty {
            return "Unable to start Orlix run environment."
        }
        if let environmentID, !environmentID.isEmpty {
            return "Unable to start Orlix environment \(environmentID)."
        }
        return "Unable to start Orlix bootloader."
    }

    private static func environmentID(from arguments: [String]) -> String? {
        var index = arguments.startIndex
        while index < arguments.endIndex {
            let argument = arguments[index]
            if argument == environmentIDArgument {
                let valueIndex = arguments.index(after: index)
                guard valueIndex < arguments.endIndex else { return nil }
                return arguments[valueIndex]
            }
            let prefix = environmentIDArgument + "="
            if argument.hasPrefix(prefix) {
                return String(argument.dropFirst(prefix.count))
            }
            index = arguments.index(after: index)
        }
        return nil
    }

    private static func runArguments(from arguments: [String]) -> [String]? {
        var index = arguments.startIndex
        while index < arguments.endIndex {
            let argument = arguments[index]
            if argument == runArgumentsArgument {
                let valueIndex = arguments.index(after: index)
                guard valueIndex < arguments.endIndex else { return nil }
                return Array(arguments[valueIndex...])
            }
            let prefix = runArgumentsArgument + "="
            if argument.hasPrefix(prefix) {
                let value = String(argument.dropFirst(prefix.count))
                return value.isEmpty ? nil : value.split(separator: " ").map(String.init)
            }
            index = arguments.index(after: index)
        }
        return nil
    }

    private static func kernelCommandLineAppend(from arguments: [String]) -> String? {
        var index = arguments.startIndex
        while index < arguments.endIndex {
            let argument = arguments[index]
            if argument == kernelCommandLineAppendArgument {
                let valueIndex = arguments.index(after: index)
                guard valueIndex < arguments.endIndex else { return nil }
                return arguments[valueIndex]
            }
            let prefix = kernelCommandLineAppendArgument + "="
            if argument.hasPrefix(prefix) {
                let value = String(argument.dropFirst(prefix.count))
                return value.isEmpty ? nil : value
            }
            index = arguments.index(after: index)
        }
        return nil
    }

    private static func appending(_ suffix: String?, to commandLine: String?) -> String? {
        guard let suffix, !suffix.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
            return commandLine
        }
        guard let commandLine, !commandLine.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
            return suffix
        }
        return commandLine + " " + suffix
    }

    private static func defaultBootProfile() -> OrlixBootProfile {
        OrlixOSDistribution.bundledBootProfile ?? .release
    }

    private static func profileDisplayName(_ profile: OrlixBootProfile) -> String {
        switch profile {
        case .release:
            return "release"
        case .development:
            return "development"
        }
    }
}

extension TerminalViewController:
    TerminalSurfaceTitleDelegate,
    TerminalSurfaceResizeDelegate,
    TerminalSurfaceCloseDelegate
{
    func terminalDidChangeTitle(_ title: String) {
        self.title = title
    }

    func terminalDidResize(columns _: Int, rows _: Int) {}

    func terminalDidClose(processAlive _: Bool) {}
}

private final class SimulatorTerminalCapture {
    private static let enabledValue = "1"
    private let lock = NSLock()
    private let url: URL?

    init(environment: [String: String] = ProcessInfo.processInfo.environment) {
        guard environment["ORLIX_SIMULATOR_CAPTURE_TERMINAL_OUTPUT"] == Self.enabledValue else {
            url = nil
            return
        }
        url = URL(fileURLWithPath: NSTemporaryDirectory())
            .appendingPathComponent("orlix-simulator-terminal-output.txt")
    }

    func reset() {
        guard let url else { return }
        lock.lock()
        defer { lock.unlock() }
        try? Data().write(to: url, options: [.atomic])
    }

    func append(_ text: String) {
        guard let url, let data = text.data(using: .utf8), !data.isEmpty else {
            return
        }
        lock.lock()
        defer { lock.unlock() }
        if FileManager.default.fileExists(atPath: url.path),
           let handle = try? FileHandle(forWritingTo: url) {
            defer { try? handle.close() }
            try? handle.seekToEnd()
            try? handle.write(contentsOf: data)
            return
        }
        try? data.write(to: url, options: [.atomic])
    }
}

private extension UIColor {
    convenience init?(hexString: String) {
        let hex = hexString.hasPrefix("#") ? String(hexString.dropFirst()) : hexString
        guard hex.count == 6,
              let r = UInt8(hex.prefix(2), radix: 16),
              let g = UInt8(hex.dropFirst(2).prefix(2), radix: 16),
              let b = UInt8(hex.dropFirst(4), radix: 16)
        else {
            return nil
        }
        self.init(
            red: CGFloat(r) / 255,
            green: CGFloat(g) / 255,
            blue: CGFloat(b) / 255,
            alpha: 1
        )
    }
}
