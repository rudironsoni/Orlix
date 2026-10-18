import Foundation

nonisolated package struct RemoteFileViewerPayload: Identifiable, Hashable, Sendable {
    package let previewKind: RemoteFilePreviewKind
    package let entry: RemoteFileEntry
    package let textPreview: String?
    package let previewFileURL: URL?
    package let isTruncated: Bool
    package let unavailableMessage: String?
    package let requiresExplicitDownload: Bool
    package let previewByteCount: UInt64?

    package var id: String { entry.id }

    package var isInlinePreviewAvailable: Bool {
        previewKind != .unavailable && !requiresExplicitDownload
    }

    package var canEditText: Bool {
        previewKind == .text && textPreview != nil && !isTruncated
    }
}

nonisolated public enum RemoteFilePreviewKind: Hashable, Sendable {
    public case text
    public case image
    public case video
    public case unavailable
}

nonisolated package enum RemoteFilePreviewDetector {
    private static let nullByte = UInt8(ascii: "\0")

    static package func decodeTextPreview(from data: Data) -> String? {
        guard isProbablyText(data) else { return nil }

        if let utf8 = String(data: data, encoding: .utf8) {
            return utf8
        }

        if let utf16LittleEndian = String(data: data, encoding: .utf16LittleEndian) {
            return utf16LittleEndian
        }

        if let utf16BigEndian = String(data: data, encoding: .utf16BigEndian) {
            return utf16BigEndian
        }

        return nil
    }

    static package func isProbablyText(_ data: Data) -> Bool {
        guard !data.isEmpty else { return true }

        let sample = data.prefix(1024)
        let nullCount = sample.filter { $0 == nullByte }.count
        if nullCount > 0 {
            return false
        }

        if String(data: sample, encoding: .utf8) != nil {
            return true
        }

        return String(data: sample, encoding: .utf16LittleEndian) != nil
            || String(data: sample, encoding: .utf16BigEndian) != nil
    }
}
