import Foundation
import Testing
@testable import Orlix

@Suite("Open-source attribution catalog")
struct OpenSourceAttributionCatalogTests {
    @Test("The checked-in manifest has complete, readable documents")
    func checkedInManifest() throws {
        let documents = try OpenSourceAttributionCatalog.load(
            manifestURL: repositoryRoot
                .appendingPathComponent("Orlix/Resources/OpenSource/Attributions.json"),
            licenseDirectoryURL: repositoryRoot
                .appendingPathComponent("Orlix/Resources/OpenSource/Licenses")
        )

        #expect(documents.count == 20)
        #expect(Set(documents.map(\.id)).count == documents.count)
        #expect(documents.allSatisfy { !$0.legalText.isEmpty })
        #expect(documents.allSatisfy { $0.attribution.projectURL.scheme == "https" })
        #expect(documents.contains { $0.id == "ghostty" })
        #expect(documents.contains { $0.id == "nvidia-parakeet" })
        #expect(documents.contains { $0.id == "nerd-fonts" })
        #expect(documents.contains { $0.id == "trzsz-ssh-rootshell" })
    }

    private var repositoryRoot: URL {
        URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent()
            .deletingLastPathComponent()
            .deletingLastPathComponent()
            .deletingLastPathComponent()
    }
}
