import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
nonisolated package enum TerminalThemeStoragePaths {
    nonisolated static package func customThemesDirectoryURL() -> URL {
        let fm = FileManager.default
        let appSupport = fm.urls(for: .applicationSupportDirectory, in: .userDomainMask).first
            ?? URL(fileURLWithPath: NSTemporaryDirectory(), isDirectory: true)
        let bundleComponent = Bundle.main.bundleIdentifier ?? "com.rudironsoni.orlix"
        return appSupport
            .appendingPathComponent(bundleComponent, isDirectory: true)
            .appendingPathComponent("CustomThemes", isDirectory: true)
    }

    nonisolated static package func customThemesDirectoryPath() -> String {
        customThemesDirectoryURL().path
    }

    nonisolated static package func customThemeFileURL(for themeName: String) -> URL? {
        guard let name = try? TerminalThemeValidator.validateAndNormalizeThemeName(themeName) else {
            return nil
        }
        let directoryURL = customThemesDirectoryURL().standardizedFileURL.resolvingSymlinksInPath()
        let fileURL = directoryURL
            .appendingPathComponent(name, isDirectory: false)
            .standardizedFileURL
            .resolvingSymlinksInPath()
        guard fileURL.deletingLastPathComponent() == directoryURL else { return nil }
        return fileURL
    }

    nonisolated static package func customThemeFilePath(for themeName: String) -> String? {
        customThemeFileURL(for: themeName)?.path
    }
}

nonisolated package struct TerminalThemeFileStore: Sendable {
    package let directoryURL: URL

    static package var appStorage: TerminalThemeFileStore {
        TerminalThemeFileStore(directoryURL: TerminalThemeStoragePaths.customThemesDirectoryURL())
    }

    package func synchronize(_ themes: [TerminalTheme], fileManager: FileManager = .default) throws {
        try fileManager.createDirectory(at: directoryURL, withIntermediateDirectories: true)

        for theme in themes {
            guard let fileURL = fileURL(for: theme.name) else { continue }

            if theme.isDeleted {
                if fileManager.fileExists(atPath: fileURL.path) {
                    try fileManager.removeItem(at: fileURL)
                }
                continue
            }

            guard case .ready(let normalizedContent) = theme.validationState else {
                continue
            }
            try normalizedContent.write(to: fileURL, atomically: true, encoding: .utf8)
        }
    }

    package func fileURL(for themeName: String) -> URL? {
        guard let name = try? TerminalThemeValidator.validateAndNormalizeThemeName(themeName) else {
            return nil
        }
        let standardizedDirectory = directoryURL.standardizedFileURL.resolvingSymlinksInPath()
        let fileURL = standardizedDirectory
            .appendingPathComponent(name, isDirectory: false)
            .standardizedFileURL
            .resolvingSymlinksInPath()
        guard fileURL.deletingLastPathComponent() == standardizedDirectory else { return nil }
        return fileURL
    }
}

extension TerminalThemeFileStore: TerminalThemeFileSynchronizing {
    package func synchronize(_ themes: [TerminalTheme]) throws {
        try synchronize(themes, fileManager: .default)
    }
}

@MainActor
package struct BundleTerminalThemeCatalog: BuiltInTerminalThemeCatalog {
    package func themeNames() -> [String] {
        guard let resourcePath = Bundle.main.resourcePath else { return [] }
        let fileManager = FileManager.default

        let structuredPath = (resourcePath as NSString).appendingPathComponent("ghostty/themes")
        if fileManager.fileExists(atPath: structuredPath),
           let files = try? fileManager.contentsOfDirectory(atPath: structuredPath) {
            return files
                .filter { file in
                    let fullPath = (structuredPath as NSString).appendingPathComponent(file)
                    var isDirectory: ObjCBool = false
                    fileManager.fileExists(atPath: fullPath, isDirectory: &isDirectory)
                    return !isDirectory.boolValue && !file.hasPrefix(".")
                }
                .sorted()
        }

        guard let files = try? fileManager.contentsOfDirectory(atPath: resourcePath) else {
            return []
        }
        let knownNonThemes = Set([
            "Info", "Assets", "PkgInfo", "ghostty", "xterm-ghostty",
            "CodeSignature", "embedded", "_CodeSignature"
        ])
        return files
            .filter { file in
                let fullPath = (resourcePath as NSString).appendingPathComponent(file)
                var isDirectory: ObjCBool = false
                fileManager.fileExists(atPath: fullPath, isDirectory: &isDirectory)
                guard !isDirectory.boolValue else { return false }
                guard !file.hasPrefix(".") else { return false }
                guard !file.contains(".") else { return false }
                guard !knownNonThemes.contains(file) else { return false }
                return true
            }
            .sorted()
    }
}
