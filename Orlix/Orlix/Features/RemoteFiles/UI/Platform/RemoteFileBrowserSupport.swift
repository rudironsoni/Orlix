import SwiftUI
import UniformTypeIdentifiers

package struct RemoteFileDownloadDocument: FileDocument {
    static package var readableContentTypes: [UTType] { [.data] }

    package let sourceURL: URL

    package init(sourceURL: URL) {
        self.sourceURL = sourceURL
    }

    package init(configuration: ReadConfiguration) throws {
        self.sourceURL = FileManager.default.temporaryDirectory
            .appendingPathComponent(UUID().uuidString)
    }

    package func fileWrapper(configuration: WriteConfiguration) throws -> FileWrapper {
        try FileWrapper(url: sourceURL, options: .immediate)
    }
}

package struct RemoteFileShareItem: Identifiable {
    package let id: UUID
    package let sourceURL: URL
    package let title: String
}
