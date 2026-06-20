import Foundation

@_spi(OrlixPrivateTesting)
public struct OrlixHostDirectoryExtendedAttribute: Equatable, Sendable {
    public let identifier: String
    public let relativePath: String
    public let name: String
    public let value: Data

    public init?(
        identifier: String,
        relativePath: String,
        name: String,
        value: Data
    ) {
        guard !identifier.isEmpty,
              let normalizedPath = Self.normalizedRelativePath(relativePath),
              Self.isLinuxExtendedAttributeName(name)
        else {
            return nil
        }

        self.identifier = identifier
        self.relativePath = normalizedPath
        self.name = name
        self.value = value
    }

    public static func lowering(
        identifier: String,
        manifest: [OrlixRootfsTarManifestEntry]
    ) -> [OrlixHostDirectoryExtendedAttribute] {
        manifest.flatMap { entry in
            entry.extendedAttributes
                .sorted { $0.key < $1.key }
                .compactMap { name, value in
                    OrlixHostDirectoryExtendedAttribute(
                        identifier: identifier,
                        relativePath: entry.path,
                        name: name,
                        value: Data(value.utf8)
                    )
                }
        }
    }

    private static func isLinuxExtendedAttributeName(_ name: String) -> Bool {
        guard !name.isEmpty, !name.hasPrefix("com.apple.") else {
            return false
        }

        return name.hasPrefix("security.")
            || name.hasPrefix("system.")
            || name.hasPrefix("trusted.")
            || name.hasPrefix("user.")
    }

    private static func normalizedRelativePath(_ path: String) -> String? {
        guard !path.hasPrefix("/") else {
            return nil
        }

        var components: [Substring] = []
        for component in path.split(separator: "/", omittingEmptySubsequences: true) {
            if component == "." {
                continue
            }
            guard component != ".." else {
                return nil
            }
            components.append(component)
        }

        return components.isEmpty ? "." : components.joined(separator: "/")
    }
}
