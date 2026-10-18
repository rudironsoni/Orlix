import Foundation

nonisolated package struct RemoteFileBrowserPersistedState: Codable, Hashable, Sendable {
    package var lastVisitedPath: String?
    package var sort: RemoteFileSort
    package var sortDirection: RemoteFileSortDirection
    package var showHiddenFiles: Bool
    package var hasCustomizedHiddenFiles: Bool

    package init(
        lastVisitedPath: String? = nil,
        sort: RemoteFileSort = .name,
        sortDirection: RemoteFileSortDirection? = nil,
        showHiddenFiles: Bool = true,
        hasCustomizedHiddenFiles: Bool = false
    ) {
        self.lastVisitedPath = lastVisitedPath
        self.sort = sort
        self.sortDirection = sortDirection ?? sort.defaultDirection
        self.showHiddenFiles = showHiddenFiles
        self.hasCustomizedHiddenFiles = hasCustomizedHiddenFiles
    }

    private enum CodingKeys: String, CodingKey {
        case lastVisitedPath
        case sort
        case sortDirection
        case showHiddenFiles
        case hasCustomizedHiddenFiles
    }

    package init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        let sort = try container.decodeIfPresent(RemoteFileSort.self, forKey: .sort) ?? .name
        lastVisitedPath = try container.decodeIfPresent(String.self, forKey: .lastVisitedPath)
        self.sort = sort
        sortDirection = try container.decodeIfPresent(RemoteFileSortDirection.self, forKey: .sortDirection) ?? sort.defaultDirection
        hasCustomizedHiddenFiles = try container.decodeIfPresent(Bool.self, forKey: .hasCustomizedHiddenFiles) ?? false
        if hasCustomizedHiddenFiles {
            showHiddenFiles = try container.decodeIfPresent(Bool.self, forKey: .showHiddenFiles) ?? true
        } else {
            showHiddenFiles = true
        }
    }
}
