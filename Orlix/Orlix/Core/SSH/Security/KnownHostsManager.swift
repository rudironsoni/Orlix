import Foundation
import os.log

nonisolated final package class KnownHostsManager: @unchecked Sendable {
    static package let shared = KnownHostsManager()

    package struct Entry: Codable, Sendable {
        package let host: String
        package let port: Int
        package let fingerprint: String
        package let keyType: Int
        package let addedAt: Date
        package var lastSeenAt: Date

        package var id: String { "\(host):\(port)" }
    }

    package struct Challenge: Identifiable, Equatable, Sendable {
        package enum Kind: Equatable, Sendable {
            case firstUse
            case changed(previousFingerprint: String)
        }

        package let id: UUID
        package let host: String
        package let port: Int
        package let fingerprint: String
        package let keyType: Int
        package let keyTypeName: String
        package let kind: Kind
        package let createdAt: Date
    }

    package enum VerificationResult: Equatable, Sendable {
        case trusted
        case approvalRequired(Challenge)
    }

    private static let challengeLifetime: TimeInterval = 120

    private let defaults: UserDefaults
    private let storageKey: String
    private let logger = Logger(
        subsystem: Bundle.main.bundleIdentifier ?? "Orlix",
        category: "KnownHosts"
    )
    private let lock = NSLock()
    private var pendingChallenges: [String: Challenge] = [:]

    package init(
        defaults: UserDefaults = .standard,
        storageKey: String = "orlix.knownHosts"
    ) {
        self.defaults = defaults
        self.storageKey = storageKey
    }

    package func entry(for host: String, port: Int) -> Entry? {
        lock.lock()
        defer { lock.unlock() }
        return loadAll()[hostKey(host: host, port: port)]
    }

    package func evaluate(
        host: String,
        port: Int,
        fingerprint: String,
        keyType: Int,
        keyTypeName: String,
        now: Date = Date()
    ) -> VerificationResult {
        lock.lock()
        defer { lock.unlock() }

        purgeExpiredChallenges(now: now)
        let canonicalHost = Self.canonicalHost(host)
        let key = hostKey(host: canonicalHost, port: port)
        var entries = loadAll()

        if var entry = entries[key], entry.fingerprint == fingerprint {
            entry.lastSeenAt = now
            entries[key] = entry
            pendingChallenges.removeValue(forKey: key)
            saveAll(entries)
            return .trusted
        }

        let kind: Challenge.Kind
        if let entry = entries[key] {
            kind = .changed(previousFingerprint: entry.fingerprint)
        } else {
            kind = .firstUse
        }

        if let pending = pendingChallenges[key],
           pending.fingerprint == fingerprint,
           pending.keyType == keyType,
           pending.kind == kind {
            return .approvalRequired(pending)
        }

        let challenge = Challenge(
            id: UUID(),
            host: canonicalHost,
            port: port,
            fingerprint: fingerprint,
            keyType: keyType,
            keyTypeName: keyTypeName,
            kind: kind,
            createdAt: now
        )
        pendingChallenges[key] = challenge
        return .approvalRequired(challenge)
    }

    package func pendingChallenge(for host: String, port: Int, now: Date = Date()) -> Challenge? {
        lock.lock()
        defer { lock.unlock() }
        purgeExpiredChallenges(now: now)
        return pendingChallenges[hostKey(host: host, port: port)]
    }

    @discardableResult
    package func approve(_ challenge: Challenge, now: Date = Date()) -> Bool {
        lock.lock()
        defer { lock.unlock() }

        purgeExpiredChallenges(now: now)
        let key = hostKey(host: challenge.host, port: challenge.port)
        guard pendingChallenges[key] == challenge else { return false }

        var entries = loadAll()
        let current = entries[key]
        switch challenge.kind {
        case .firstUse:
            guard current == nil || current?.fingerprint == challenge.fingerprint else {
                pendingChallenges.removeValue(forKey: key)
                return false
            }
        case .changed(let previousFingerprint):
            guard current?.fingerprint == previousFingerprint
                    || current?.fingerprint == challenge.fingerprint else {
                pendingChallenges.removeValue(forKey: key)
                return false
            }
        }

        entries[key] = Entry(
            host: challenge.host,
            port: challenge.port,
            fingerprint: challenge.fingerprint,
            keyType: challenge.keyType,
            addedAt: current?.fingerprint == challenge.fingerprint
                ? current?.addedAt ?? now
                : now,
            lastSeenAt: now
        )
        saveAll(entries)
        pendingChallenges.removeValue(forKey: key)
        logger.info("Approved SSH host key for port \(challenge.port)")
        return true
    }

    package func reject(_ challenge: Challenge) {
        lock.lock()
        defer { lock.unlock() }
        let key = hostKey(host: challenge.host, port: challenge.port)
        guard pendingChallenges[key]?.id == challenge.id else { return }
        pendingChallenges.removeValue(forKey: key)
    }

    package func save(entry: Entry) {
        lock.lock()
        defer { lock.unlock() }
        var entries = loadAll()
        let canonicalEntry = Entry(
            host: Self.canonicalHost(entry.host),
            port: entry.port,
            fingerprint: entry.fingerprint,
            keyType: entry.keyType,
            addedAt: entry.addedAt,
            lastSeenAt: entry.lastSeenAt
        )
        entries[canonicalEntry.id] = canonicalEntry
        saveAll(entries)
    }

    package func remove(host: String, port: Int) {
        lock.lock()
        defer { lock.unlock() }
        var entries = loadAll()
        let key = hostKey(host: host, port: port)
        pendingChallenges.removeValue(forKey: key)
        guard entries.removeValue(forKey: key) != nil else { return }
        saveAll(entries)
        logger.info("Removed known host entry for port \(port)")
    }

    package func removeAll() {
        lock.lock()
        defer { lock.unlock() }
        pendingChallenges.removeAll()
        defaults.removeObject(forKey: storageKey)
        logger.info("Removed all known host entries")
    }

    package func entries() -> [Entry] {
        lock.lock()
        defer { lock.unlock() }
        return loadAll().values.sorted { lhs, rhs in
            if lhs.host == rhs.host {
                return lhs.port < rhs.port
            }
            return lhs.host < rhs.host
        }
    }

    private func purgeExpiredChallenges(now: Date) {
        let cutoff = now.addingTimeInterval(-Self.challengeLifetime)
        pendingChallenges = pendingChallenges.filter { $0.value.createdAt >= cutoff }
    }

    private func hostKey(host: String, port: Int) -> String {
        "\(Self.canonicalHost(host)):\(port)"
    }

    private static func canonicalHost(_ host: String) -> String {
        var value = host
            .trimmingCharacters(in: .whitespacesAndNewlines)
            .lowercased(with: Locale(identifier: "en_US_POSIX"))
        while value.last == "." {
            value.removeLast()
        }
        return value
    }

    private func loadAll() -> [String: Entry] {
        guard let data = defaults.data(forKey: storageKey),
              let decoded = try? JSONDecoder().decode([String: Entry].self, from: data) else {
            return [:]
        }

        var normalized: [String: Entry] = [:]
        for entry in decoded.values {
            let canonicalEntry = Entry(
                host: Self.canonicalHost(entry.host),
                port: entry.port,
                fingerprint: entry.fingerprint,
                keyType: entry.keyType,
                addedAt: entry.addedAt,
                lastSeenAt: entry.lastSeenAt
            )
            let key = canonicalEntry.id
            if let existing = normalized[key], existing.lastSeenAt >= canonicalEntry.lastSeenAt {
                continue
            }
            normalized[key] = canonicalEntry
        }
        return normalized
    }

    private func saveAll(_ entries: [String: Entry]) {
        guard let data = try? JSONEncoder().encode(entries) else {
            logger.error("Failed to encode known hosts store")
            return
        }
        defaults.set(data, forKey: storageKey)
    }
}
