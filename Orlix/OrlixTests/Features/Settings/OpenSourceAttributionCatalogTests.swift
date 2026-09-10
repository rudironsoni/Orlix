import Foundation
import Testing
@testable import Orlix

@Suite("Open-source attribution catalog")
struct OpenSourceAttributionCatalogTests {
    @Test("The bundled manifest has complete, readable documents")
    func checkedInManifest() throws {
        let documents = try OpenSourceAttributionCatalog.load(from: .main)

        #expect(documents.count == 19)
        #expect(Set(documents.map(\.id)).count == documents.count)
        #expect(documents.allSatisfy { !$0.legalText.isEmpty })
        #expect(documents.allSatisfy { $0.attribution.projectURL.scheme == "https" })
        #expect(documents.contains { $0.id == "ghostty" })
        #expect(documents.contains { $0.id == "nvidia-parakeet" })
        #expect(documents.contains { $0.id == "nerd-fonts" })
    }

}
