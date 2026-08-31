import CryptoKit
import Foundation
import LocalAuthentication
import Security
@preconcurrency import TrzszSSH

nonisolated enum TSSHAgentCredentialValidator {
    static func isSupported(privateKey: String, publicKey: String) -> Bool {
        var credentials = ServerCredentials(serverId: UUID())
        credentials.privateKey = Data(privateKey.utf8)
        credentials.publicKey = Data(publicKey.utf8)
        return (try? TSSHAgentIdentity(credentials: credentials, comment: "")) != nil
    }
}

nonisolated protocol TSSHAgentForwardingBridge: AnyObject, Sendable {
    func suspend()
    func resume()
}

nonisolated final class TSSHAgentBridge:
    NSObject,
    IosbridgeAgentCallbackProtocol,
    TSSHAgentForwardingBridge,
    @unchecked Sendable {
    private let identity: TSSHAgentIdentity
    private let approvalMode: TSSHAgentApprovalMode
    private let stateLock = NSLock()
    private var sessionApproved = false
    private var suspended = false

    init(
        credentials: ServerCredentials,
        comment: String,
        approvalMode: TSSHAgentApprovalMode
    ) throws {
        identity = try TSSHAgentIdentity(credentials: credentials, comment: comment)
        self.approvalMode = approvalMode
    }

    func suspend() { stateLock.withLock { suspended = true } }
    func resume() { stateLock.withLock { suspended = false } }

    func listIdentities(_ error: NSErrorPointer) -> String {
        guard !stateLock.withLock({ suspended }) else {
            error?.pointee = TSSHAgentIdentity.failure("SSH agent is unavailable in the background")
            return "[]"
        }
        let value = [[
            "blob": identity.publicBlob.base64EncodedString(),
            "comment": identity.comment,
        ]]
        guard let data = try? JSONSerialization.data(withJSONObject: value),
              let result = String(data: data, encoding: .utf8) else {
            error?.pointee = TSSHAgentIdentity.failure("Cannot encode SSH agent identity")
            return "[]"
        }
        return result
    }

    func sign(_ publicKeyBlob: Data?, data: Data?, flags: Int32) throws -> Data {
        guard let publicKeyBlob, let data,
              publicKeyBlob == identity.publicBlob else {
            throw TSSHAgentIdentity.failure("No matching SSH agent identity")
        }
        guard data.count <= 1_048_576 else {
            throw TSSHAgentIdentity.failure("The SSH agent signing request is too large")
        }
        try authorizeSigning()
        return try stateLock.withLock {
            guard !suspended else {
                throw TSSHAgentIdentity.failure("SSH agent is unavailable in the background")
            }
            return try identity.sign(data, flags: flags)
        }
    }

    private func authorizeSigning() throws {
        guard !stateLock.withLock({ suspended }) else {
            throw TSSHAgentIdentity.failure("SSH agent is unavailable in the background")
        }
        switch approvalMode {
        case .automatic:
            return
        case .perSession where stateLock.withLock({ sessionApproved }):
            return
        case .perSession, .perRequest:
            break
        }

        let context = LAContext()
        var authorizationError: NSError?
        guard context.canEvaluatePolicy(.deviceOwnerAuthentication, error: &authorizationError) else {
            throw authorizationError ?? TSSHAgentIdentity.failure("Device-owner approval is unavailable")
        }
        let result = TSSHAgentApprovalResult()
        let semaphore = DispatchSemaphore(value: 0)
        context.evaluatePolicy(
            .deviceOwnerAuthentication,
            localizedReason: "Approve SSH agent signing for \(identity.comment)"
        ) { approved, error in
            result.set(approved: approved, error: error)
            semaphore.signal()
        }
        guard semaphore.wait(timeout: .now() + 30) == .success else {
            context.invalidate()
            throw TSSHAgentIdentity.failure("SSH agent approval timed out")
        }
        let decision = result.get()
        guard decision.approved else {
            throw decision.error ?? TSSHAgentIdentity.failure("SSH agent signing was denied")
        }
        try stateLock.withLock {
            guard !suspended else {
                throw TSSHAgentIdentity.failure("SSH agent is unavailable in the background")
            }
            if approvalMode == .perSession {
                sessionApproved = true
            }
        }
    }
}

private nonisolated final class TSSHAgentApprovalResult: @unchecked Sendable {
    private let lock = NSLock()
    private var value: (approved: Bool, error: Error?) = (false, nil)

    func set(approved: Bool, error: Error?) {
        lock.withLock { value = (approved, error) }
    }

    func get() -> (approved: Bool, error: Error?) {
        lock.withLock { value }
    }
}

private nonisolated struct TSSHAgentIdentity: @unchecked Sendable {
    enum Key {
        case ed25519(Curve25519.Signing.PrivateKey)
        case rsa(SecKey)
    }

    let publicBlob: Data
    let comment: String
    let key: Key

    init(credentials: ServerCredentials, comment: String) throws {
        guard let privateKey = credentials.privateKey,
              let publicKey = credentials.publicKey,
              let publicLine = String(data: publicKey, encoding: .utf8),
              let suppliedPublicBlob = Self.publicBlob(from: publicLine) else {
            throw Self.failure("The active SSH key has no public identity")
        }

        guard let pem = String(data: privateKey, encoding: .utf8) else {
            throw Self.failure("The active SSH private key is not text")
        }
        let resolvedKey: Key
        let derivedPublicBlob: Data
        if pem.contains("BEGIN RSA PRIVATE KEY") {
            let der = try Self.decodePEM(pem, label: "RSA PRIVATE KEY")
            let attributes: [CFString: Any] = [
                kSecAttrKeyType: kSecAttrKeyTypeRSA,
                kSecAttrKeyClass: kSecAttrKeyClassPrivate,
            ]
            var error: Unmanaged<CFError>?
            guard let secKey = SecKeyCreateWithData(der as CFData, attributes as CFDictionary, &error) else {
                throw error?.takeRetainedValue() ?? Self.failure("Cannot import RSA key")
            }
            resolvedKey = .rsa(secKey)
            derivedPublicBlob = try Self.rsaPublicBlob(from: secKey)
        } else if pem.contains("BEGIN OPENSSH PRIVATE KEY") {
            let seed = try Self.decodeEd25519Seed(pem)
            let signingKey = try Curve25519.Signing.PrivateKey(rawRepresentation: seed)
            resolvedKey = .ed25519(signingKey)
            var blob = Data()
            blob.appendSSHString(Data("ssh-ed25519".utf8))
            blob.appendSSHString(signingKey.publicKey.rawRepresentation)
            derivedPublicBlob = blob
        } else {
            throw Self.failure("This SSH key format cannot be forwarded")
        }
        guard suppliedPublicBlob == derivedPublicBlob else {
            throw Self.failure("The SSH public key does not match the private key")
        }
        publicBlob = derivedPublicBlob
        self.comment = comment
        key = resolvedKey
    }

    func sign(_ data: Data, flags: Int32) throws -> Data {
        let algorithm: String
        let signature: Data
        switch key {
        case .ed25519(let key):
            algorithm = "ssh-ed25519"
            signature = try key.signature(for: data)
        case .rsa(let key):
            let secAlgorithm: SecKeyAlgorithm
            switch flags {
            case 0:
                algorithm = "ssh-rsa"
                secAlgorithm = .rsaSignatureMessagePKCS1v15SHA1
            case 2:
                algorithm = "rsa-sha2-256"
                secAlgorithm = .rsaSignatureMessagePKCS1v15SHA256
            case 4:
                algorithm = "rsa-sha2-512"
                secAlgorithm = .rsaSignatureMessagePKCS1v15SHA512
            default:
                throw Self.failure("The SSH agent requested unsupported RSA flags")
            }
            guard SecKeyIsAlgorithmSupported(key, .sign, secAlgorithm) else {
                throw Self.failure("The RSA key does not support the requested signature")
            }
            var error: Unmanaged<CFError>?
            guard let value = SecKeyCreateSignature(key, secAlgorithm, data as CFData, &error) else {
                throw error?.takeRetainedValue() ?? Self.failure("RSA signing failed")
            }
            signature = value as Data
        }
        var wire = Data()
        wire.appendSSHString(Data(algorithm.utf8))
        wire.appendSSHString(signature)
        return wire
    }

    static func publicBlob(from line: String) -> Data? {
        let parts = line.split(whereSeparator: { $0.isWhitespace })
        guard parts.count >= 2 else { return nil }
        return Data(base64Encoded: String(parts[1]))
    }

    static func decodePEM(_ pem: String, label: String) throws -> Data {
        let body = pem
            .replacingOccurrences(of: "-----BEGIN \(label)-----", with: "")
            .replacingOccurrences(of: "-----END \(label)-----", with: "")
            .filter { !$0.isWhitespace }
        guard let data = Data(base64Encoded: body) else {
            throw failure("Invalid \(label) data")
        }
        return data
    }

    static func decodeEd25519Seed(_ pem: String) throws -> Data {
        var reader = SSHWireReader(data: try decodePEM(pem, label: "OPENSSH PRIVATE KEY"))
        guard reader.read(count: 15) == Data("openssh-key-v1\0".utf8),
              reader.readString() == Data("none".utf8),
              reader.readString() == Data("none".utf8),
              reader.readString() != nil,
              reader.readUInt32() == 1,
              reader.readString() != nil,
              let privateBlock = reader.readString() else {
            throw failure("Encrypted or invalid OpenSSH key")
        }
        var privateReader = SSHWireReader(data: privateBlock)
        guard let checkA = privateReader.readUInt32(),
              checkA == privateReader.readUInt32(),
              privateReader.readString() == Data("ssh-ed25519".utf8),
              privateReader.readString() != nil,
              let privateBytes = privateReader.readString(),
              privateBytes.count == 64 else {
            throw failure("Invalid Ed25519 OpenSSH key")
        }
        return privateBytes.prefix(32)
    }

    static func rsaPublicBlob(from privateKey: SecKey) throws -> Data {
        guard let publicKey = SecKeyCopyPublicKey(privateKey) else {
            throw failure("Cannot derive RSA public key")
        }
        var error: Unmanaged<CFError>?
        guard let der = SecKeyCopyExternalRepresentation(publicKey, &error) as Data? else {
            throw error?.takeRetainedValue() ?? failure("Cannot export RSA public key")
        }
        var outerReader = ASN1Reader(data: der)
        let sequence = try outerReader.readElement(tag: 0x30)
        guard outerReader.isAtEnd else {
            throw failure("Invalid RSA public key")
        }
        var sequenceReader = ASN1Reader(data: sequence)
        let modulus = try sequenceReader.readElement(tag: 0x02)
        let exponent = try sequenceReader.readElement(tag: 0x02)
        guard sequenceReader.isAtEnd else {
            throw failure("Invalid RSA public key")
        }
        var blob = Data()
        blob.appendSSHString(Data("ssh-rsa".utf8))
        blob.appendSSHMPInt(exponent)
        blob.appendSSHMPInt(modulus)
        return blob
    }

    static func failure(_ message: String) -> NSError {
        NSError(
            domain: "com.rudironsoni.orlix.tssh-agent",
            code: 1,
            userInfo: [NSLocalizedDescriptionKey: message]
        )
    }
}

private nonisolated struct ASN1Reader {
    let data: Data
    var offset = 0

    var isAtEnd: Bool { offset == data.count }

    mutating func readElement(tag: UInt8) throws -> Data {
        guard offset < data.count, data[offset] == tag else {
            throw TSSHAgentIdentity.failure("Invalid RSA public key")
        }
        offset += 1
        let length = try readLength()
        guard length >= 0, offset <= data.count - length else {
            throw TSSHAgentIdentity.failure("Invalid RSA public key")
        }
        defer { offset += length }
        return data.subdata(in: offset..<(offset + length))
    }

    private mutating func readLength() throws -> Int {
        guard offset < data.count else {
            throw TSSHAgentIdentity.failure("Invalid RSA public key")
        }
        let first = data[offset]
        offset += 1
        if first < 0x80 { return Int(first) }
        let byteCount = Int(first & 0x7f)
        guard byteCount > 0, byteCount <= MemoryLayout<Int>.size,
              offset <= data.count - byteCount else {
            throw TSSHAgentIdentity.failure("Invalid RSA public key")
        }
        var length = 0
        for _ in 0..<byteCount {
            guard length <= (Int.max >> 8) else {
                throw TSSHAgentIdentity.failure("Invalid RSA public key")
            }
            length = (length << 8) | Int(data[offset])
            offset += 1
        }
        return length
    }
}

private nonisolated struct SSHWireReader {
    let data: Data
    var offset = 0

    mutating func read(count: Int) -> Data? {
        guard count >= 0, offset + count <= data.count else { return nil }
        defer { offset += count }
        return data.subdata(in: offset..<(offset + count))
    }

    mutating func readUInt32() -> UInt32? {
        guard let bytes = read(count: 4) else { return nil }
        return bytes.reduce(UInt32(0)) { ($0 << 8) | UInt32($1) }
    }

    mutating func readString() -> Data? {
        guard let length = readUInt32() else { return nil }
        return read(count: Int(length))
    }
}

private nonisolated extension Data {
    mutating func appendSSHString(_ value: Data) {
        let length = UInt32(value.count)
        append(UInt8((length >> 24) & 0xff))
        append(UInt8((length >> 16) & 0xff))
        append(UInt8((length >> 8) & 0xff))
        append(UInt8(length & 0xff))
        append(value)
    }

    mutating func appendSSHMPInt(_ value: Data) {
        var normalized = value
        while normalized.count > 1, normalized.first == 0 {
            normalized.removeFirst()
        }
        if let first = normalized.first, first & 0x80 != 0 {
            var positive = Data([0])
            positive.append(normalized)
            normalized = positive
        }
        appendSSHString(normalized)
    }
}
