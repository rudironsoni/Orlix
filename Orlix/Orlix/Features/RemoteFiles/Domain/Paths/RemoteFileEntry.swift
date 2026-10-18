import Foundation

nonisolated package struct RemoteFileEntry: Identifiable, Hashable, Codable, Sendable {
    package let name: String
    package let path: String
    package let type: RemoteFileType
    package let size: UInt64?
    package let modifiedAt: Date?
    package let permissions: UInt32?
    package let symlinkTarget: String?

    package var id: String { path }

    package var isHidden: Bool {
        name.hasPrefix(".") && name != "." && name != ".."
    }

    package var iconName: String {
        switch type {
        case .directory:
            return "folder.fill"
        case .symlink:
            return "link"
        case .other:
            return "questionmark.square.dashed"
        case .file:
            let lowercasedExtension = URL(fileURLWithPath: name).pathExtension.lowercased()
            switch lowercasedExtension {
            case "jpg", "jpeg", "png", "gif", "webp", "heic", "svg":
                return "photo"
            case "mov", "mp4", "mkv", "avi":
                return "film"
            case "zip", "tar", "gz", "tgz", "xz", "bz2":
                return "archivebox"
            case "log", "txt", "md", "json", "yaml", "yml", "toml", "xml", "plist", "ini", "conf", "config", "swift", "sh", "zsh", "bash", "py", "rb", "js", "ts", "tsx", "jsx", "html", "css", "sql":
                return "doc.text"
            default:
                return "doc"
            }
        }
    }

    package var metadataTypeLabel: String {
        type.displayName
    }

    package var supportsPreview: Bool {
        type != .directory
    }

    package var sortableModifiedAt: Date {
        modifiedAt ?? .distantPast
    }

    package var sortableSize: UInt64 {
        size ?? 0
    }

    package var formattedPermissions: String? {
        guard let permissions else { return nil }
        let octal = String(permissions & 0o7777, radix: 8)
        let padded = String(repeating: "0", count: max(0, 4 - octal.count)) + octal
        return "\(padded) (\(Self.symbolicPermissions(for: permissions)))"
    }

    package var specialPermissionBits: UInt32 {
        (permissions ?? 0) & 0o7000
    }

    static package func symbolicPermissions(for permissions: UInt32) -> String {
        func bits(_ read: UInt32, _ write: UInt32, _ execute: UInt32) -> String {
            [
                permissions & read != 0 ? "r" : "-",
                permissions & write != 0 ? "w" : "-",
                permissions & execute != 0 ? "x" : "-"
            ].joined()
        }

        return [
            bits(0o400, 0o200, 0o100),
            bits(0o040, 0o020, 0o010),
            bits(0o004, 0o002, 0o001)
        ].joined()
    }


    package init(

        name: String,

        path: String,

        type: RemoteFileType,

        size: UInt64? = nil,

        modifiedAt: Date? = nil,

        permissions: UInt32? = nil,

        symlinkTarget: String? = nil

    ) {

        self.name = name

        self.path = path

        self.type = type

        self.size = size

        self.modifiedAt = modifiedAt

        self.permissions = permissions

        self.symlinkTarget = symlinkTarget

    }
}
