import GhosttyTerminal
import GhosttyTheme
import OrlixOS
import UIKit

final class TerminalViewController: UIViewController {
    private static let lightThemeKey = "SelectedTheme.light"
    private static let darkThemeKey = "SelectedTheme.dark"

    private var didStartBoot = false
    private let bootQueue = DispatchQueue(label: "com.rudironsoni.terminal.boot", qos: .userInitiated)
    private let launchConfiguration: OrlixTerminalLaunchConfiguration
    private lazy var linuxSessionResult = launchConfiguration.makeLinuxSession()
    private var terminalOutput: OrlixTerminalOutput?
    private lazy var terminalView = TerminalView(frame: .zero)
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

    init(launchConfiguration: OrlixTerminalLaunchConfiguration = .current()) {
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

        terminalSession.receive("OrlixTerminal\r\n")
        guard let session = linuxSession else {
            terminalSession.receive(launchConfiguration.failureMessage + "\r\n")
            return
        }
        terminalSession.receive(launchConfiguration.startMessage(for: session) + "\r\n")
        bootQueue.async { [weak self] in
            let status = session.boot()
            DispatchQueue.main.async { [weak self] in
                self?.terminalSession.receive(status.message + "\r\n")
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

            DispatchQueue.main.async { [weak self] in
                self?.terminalSession.receive(text)
            }
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
            image: UIImage(systemName: "paintpalette"),
            menu: buildThemeMenu()
        )
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
    }
}

struct OrlixTerminalLaunchConfiguration {
    static let environmentIDArgument = "--orlix-environment-id"
    static let environmentIDDefaultsKey = "OrlixTerminal.environmentID"
    static let runArgumentsArgument = "--orlix-run"
    static let runArgumentsDefaultsKey = "OrlixTerminal.runArguments"

    let environmentID: String?
    let runArguments: [String]?

    static func current(
        arguments: [String] = ProcessInfo.processInfo.arguments,
        defaults: UserDefaults = .standard
    ) -> OrlixTerminalLaunchConfiguration {
        OrlixTerminalLaunchConfiguration(
            environmentID: environmentID(from: arguments)
                ?? defaults.string(forKey: environmentIDDefaultsKey),
            runArguments: runArguments(from: arguments)
                ?? defaults.stringArray(forKey: runArgumentsDefaultsKey)
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
            return OrlixLinuxSession(
                bootConfig: OrlixBootConfig(profile: Self.defaultBootProfile())
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
