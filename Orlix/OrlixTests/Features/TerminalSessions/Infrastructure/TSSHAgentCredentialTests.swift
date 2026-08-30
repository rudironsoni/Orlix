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
}
