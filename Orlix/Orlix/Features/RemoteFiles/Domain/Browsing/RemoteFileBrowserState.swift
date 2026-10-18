import Foundation

nonisolated package struct RemoteFileFilesystemStatus: Hashable, Sendable {
    package let blockSize: UInt64
    package let totalBlocks: UInt64
    package let freeBlocks: UInt64
    package let availableBlocks: UInt64

    package var totalBytes: UInt64 {
        blockSize.saturatingMultiply(totalBlocks)
    }

    package var freeBytes: UInt64 {
        blockSize.saturatingMultiply(freeBlocks)
    }

    package var availableBytes: UInt64 {
        blockSize.saturatingMultiply(availableBlocks)
    }
}

nonisolated public enum RemoteFileFilesystemCapacity: Hashable, Sendable {
    public case known(RemoteFileFilesystemStatus)
    public case unavailable

    public var status: RemoteFileFilesystemStatus? {
        guard case .known(let status) = self else { return nil }
        return status
    }
}

nonisolated public enum RemoteFileDirectoryPhase: Equatable, Sendable {
    public case notLoaded
    public case loading(requestID: UUID, hasLoadedDirectory: Bool)
    public case loaded
    public case failed(RemoteFileBrowserError, hasLoadedDirectory: Bool)

    public var isLoading: Bool {
        if case .loading = self { return true }
        return false
    }

    public var hasLoadedDirectory: Bool {
        switch self {
        case .notLoaded:
            return false
        case .loading(_, let hasLoadedDirectory), .failed(_, let hasLoadedDirectory):
            return hasLoadedDirectory
        case .loaded:
            return true
        }
    }

    public var error: RemoteFileBrowserError? {
        guard case .failed(let error, _) = self else { return nil }
        return error
    }

    mutating public func begin(requestID: UUID) {
        self = .loading(requestID: requestID, hasLoadedDirectory: hasLoadedDirectory)
    }

    @discardableResult
    mutating public func complete(requestID: UUID) -> Bool {
        guard case .loading(let currentRequestID, _) = self,
              currentRequestID == requestID else { return false }
        self = .loaded
        return true
    }

    @discardableResult
    mutating public func fail(requestID: UUID, error: RemoteFileBrowserError) -> Bool {
        guard case .loading(let currentRequestID, let hasLoadedDirectory) = self,
              currentRequestID == requestID else { return false }
        self = .failed(error, hasLoadedDirectory: hasLoadedDirectory)
        return true
    }
}

nonisolated public enum RemoteFileViewerPhase: Equatable, Sendable {
    public case idle
    public case selected(path: String)
    public case loading(path: String, requestID: UUID)
    public case loaded(RemoteFileViewerPayload)
    public case failed(path: String, RemoteFileBrowserError)

    public var selectedEntryPath: String? {
        switch self {
        case .idle:
            return nil
        case .selected(let path), .loading(let path, _), .failed(let path, _):
            return path
        case .loaded(let payload):
            return payload.entry.path
        }
    }

    public var isLoading: Bool {
        if case .loading = self { return true }
        return false
    }

    public var payload: RemoteFileViewerPayload? {
        guard case .loaded(let payload) = self else { return nil }
        return payload
    }

    public var error: RemoteFileBrowserError? {
        guard case .failed(_, let error) = self else { return nil }
        return error
    }

    public func isLoading(requestID: UUID) -> Bool {
        guard case .loading(_, let currentRequestID) = self else { return false }
        return currentRequestID == requestID
    }

    mutating public func select(path: String) {
        self = .selected(path: path)
    }

    mutating public func beginLoading(path: String, requestID: UUID) {
        self = .loading(path: path, requestID: requestID)
    }

    @discardableResult
    mutating public func complete(requestID: UUID, payload: RemoteFileViewerPayload) -> Bool {
        guard case .loading(let path, let currentRequestID) = self,
              path == payload.entry.path,
              currentRequestID == requestID else { return false }
        self = .loaded(payload)
        return true
    }

    @discardableResult
    mutating public func fail(requestID: UUID, error: RemoteFileBrowserError) -> Bool {
        guard case .loading(let path, let currentRequestID) = self,
              currentRequestID == requestID else { return false }
        self = .failed(path: path, error)
        return true
    }
}

nonisolated private extension UInt64 {
    func saturatingMultiply(_ other: UInt64) -> UInt64 {
        let result = multipliedReportingOverflow(by: other)
        return result.overflow ? .max : result.partialValue
    }
}
