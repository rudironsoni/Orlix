import Foundation

nonisolated package enum TerminalRemoteClipboardReadPolicy: String, CaseIterable, Identifiable, Sendable {
    case deny
    case ask
    case allow

    static package let userDefaultsKey = "terminalRemoteClipboardReadPolicy"
    static package let defaultValue: TerminalRemoteClipboardReadPolicy = .ask

    package var id: String { rawValue }

    package var settingsTitle: String {
        switch self {
        case .deny: return String(localized: "Deny")
        case .ask: return String(localized: "Ask Every Time")
        case .allow: return String(localized: "Always Allow")
        }
    }

    package var settingsDescription: String {
        switch self {
        case .deny:
            return String(localized: "Remote programs cannot read your clipboard.")
        case .ask:
            return String(localized: "Orlix asks before a remote program reads your clipboard.")
        case .allow:
            return String(localized: "Warning: Remote programs can read your clipboard without asking.")
        }
    }

    static package func resolved(defaults: UserDefaults = .standard) -> TerminalRemoteClipboardReadPolicy {
        guard let rawValue = defaults.string(forKey: userDefaultsKey),
              let policy = TerminalRemoteClipboardReadPolicy(rawValue: rawValue) else {
            return defaultValue
        }
        return policy
    }
}

nonisolated package enum TerminalClipboardWriteAction: Equatable, Sendable {
    case writeImmediately
    case requestConfirmation
}

nonisolated package enum TerminalClipboardWritePolicy {
    static package func action(requiresConfirmation: Bool) -> TerminalClipboardWriteAction {
        requiresConfirmation ? .requestConfirmation : .writeImmediately
    }
}

nonisolated package enum TerminalClipboardConfirmationKind: Equatable, Sendable {
    case remoteRead
    case remoteWrite
    case unsafePaste

    package var title: String {
        switch self {
        case .remoteRead: return String(localized: "Allow Clipboard Read?")
        case .remoteWrite: return String(localized: "Allow Clipboard Change?")
        case .unsafePaste: return String(localized: "Allow Paste?")
        }
    }

    package var message: String {
        switch self {
        case .remoteRead:
            return String(localized: "The remote terminal wants to read your clipboard.")
        case .remoteWrite:
            return String(localized: "The remote terminal wants to replace your clipboard.")
        case .unsafePaste:
            return String(localized: "The clipboard text may contain commands or multiple lines.")
        }
    }
}

nonisolated package enum TerminalClipboardConfirmationDecision: Equatable, Sendable {
    case cancel
    case allow
}

nonisolated package enum TerminalClipboardPresentationRetryAction: Equatable, Sendable {
    case retry(nextAttempt: Int)
    case cancel
}

nonisolated package enum TerminalClipboardPresentationRetryPolicy {
    static package let maximumAttempts = 480

    static package func action(after attemptCount: Int) -> TerminalClipboardPresentationRetryAction {
        guard attemptCount < maximumAttempts else { return .cancel }
        return .retry(nextAttempt: attemptCount + 1)
    }
}

@MainActor
final package class TerminalClipboardConfirmationQueue {
    static package let maximumRequestCount = 8

    package struct Request: Identifiable {
        package let id: UUID
        package let kind: TerminalClipboardConfirmationKind
        fileprivate let completion: (TerminalClipboardConfirmationDecision) -> Void
    }

    private var pending: [Request] = []
    private(set) var current: Request?

    package var requestToPresent: Request? {
        if current == nil, !pending.isEmpty {
            current = pending.removeFirst()
        }
        return current
    }

    @discardableResult
    package func enqueue(
        kind: TerminalClipboardConfirmationKind,
        completion: @escaping (TerminalClipboardConfirmationDecision) -> Void
    ) -> UUID {
        let request = Request(id: UUID(), kind: kind, completion: completion)
        if kind == .remoteWrite,
           current?.kind == .remoteWrite || pending.contains(where: { $0.kind == .remoteWrite }) {
            completion(.cancel)
            return request.id
        }
        let requestCount = pending.count + (current == nil ? 0 : 1)
        guard requestCount < Self.maximumRequestCount else {
            completion(.cancel)
            return request.id
        }
        pending.append(request)
        return request.id
    }

    @discardableResult
    package func resolve(
        id: UUID,
        decision: TerminalClipboardConfirmationDecision
    ) -> Bool {
        guard let current, current.id == id else { return false }
        self.current = nil
        current.completion(decision)
        return true
    }

    package func cancelAll() {
        var requests = pending
        if let current {
            requests.insert(current, at: 0)
        }
        pending.removeAll(keepingCapacity: false)
        current = nil
        for request in requests {
            request.completion(.cancel)
        }
    }
}
