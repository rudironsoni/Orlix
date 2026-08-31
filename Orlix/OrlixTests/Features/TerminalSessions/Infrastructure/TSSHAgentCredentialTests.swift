import Foundation
import Testing
@testable import Orlix

struct TSSHAgentCredentialTests {
    @Test
    func validatorAcceptsOnlyTheMatchingEd25519PublicKey() throws {
        let first = try SSHKeyGenerator.generate(type: .ed25519)
        let second = try SSHKeyGenerator.generate(type: .ed25519)
        let privateKey = String(decoding: first.privateKey, as: UTF8.self)

        #expect(TSSHAgentCredentialValidator.isSupported(
            privateKey: privateKey,
            publicKey: first.publicKey
        ))
        #expect(!TSSHAgentCredentialValidator.isSupported(
            privateKey: privateKey,
            publicKey: second.publicKey
        ))
    }

    @Test
    func validatorAcceptsOnlyTheMatchingRSAPublicKey() throws {
        let rsa = try SSHKeyGenerator.generate(type: .rsa4096)
        let ed25519 = try SSHKeyGenerator.generate(type: .ed25519)
        let privateKey = String(decoding: rsa.privateKey, as: UTF8.self)

        #expect(TSSHAgentCredentialValidator.isSupported(
            privateKey: privateKey,
            publicKey: rsa.publicKey
        ))
        #expect(!TSSHAgentCredentialValidator.isSupported(
            privateKey: privateKey,
            publicKey: ed25519.publicKey
        ))
    }

    @Test
    func suspendedAgentRejectsSigning() throws {
        let key = try SSHKeyGenerator.generate(type: .ed25519)
        var credentials = ServerCredentials(serverId: UUID())
        credentials.privateKey = key.privateKey
        credentials.publicKey = Data(key.publicKey.utf8)
        let bridge = try TSSHAgentBridge(
            credentials: credentials,
            comment: "test",
            approvalMode: .automatic
        )
        let identitiesData = try #require(bridge.listIdentities(nil).data(using: .utf8))
        let identities = try #require(
            JSONSerialization.jsonObject(with: identitiesData) as? [[String: String]]
        )
        let encodedBlob = try #require(identities.first?["blob"])
        let blob = try #require(Data(base64Encoded: encodedBlob))

        bridge.suspend()

        #expect(throws: Error.self) {
            try bridge.sign(blob, data: Data("payload".utf8), flags: 0)
        }
    }
}
