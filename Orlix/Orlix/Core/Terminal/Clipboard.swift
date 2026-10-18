//
//  Clipboard.swift
//  Orlix
//
//  Shared pasteboard helper for text and image clipboard payloads
//

#if os(macOS)
import AppKit
import UniformTypeIdentifiers

nonisolated package struct ClipboardImagePayload: Sendable {
    nonisolated package let data: Data
    nonisolated package let mimeType: String
    nonisolated package let utType: String
    nonisolated package let suggestedExtension: String

    nonisolated package var sizeBytes: Int { data.count }
}

nonisolated package struct ClipboardSnapshot: Sendable {
    nonisolated package let text: String?
    nonisolated package let image: ClipboardImagePayload?

    nonisolated package var hasText: Bool {
        guard let text else { return false }
        return !text.isEmpty
    }

    nonisolated package var hasImage: Bool {
        image != nil
    }
}

package enum Clipboard {
    static package func copy(_ text: String) {
        let pasteboard = NSPasteboard.general
        pasteboard.clearContents()
        pasteboard.setString(text, forType: .string)
    }

    static package func readString() -> String? {
        NSPasteboard.general.string(forType: .string)
    }

    static package func copy(lines: [String], separator: String = "\n") {
        copy(lines.joined(separator: separator))
    }

    @MainActor
    static package func snapshot() -> ClipboardSnapshot {
        let pasteboard = NSPasteboard.general
        return ClipboardSnapshot(
            text: normalizedText(pasteboard.string(forType: .string)),
            image: imagePayload(from: pasteboard)
        )
    }

    private static func normalizedText(_ text: String?) -> String? {
        guard let text, !text.isEmpty else { return nil }
        return text
    }

    private static func imagePayload(from pasteboard: NSPasteboard) -> ClipboardImagePayload? {
        if let pngData = pasteboard.data(forType: .png),
           !pngData.isEmpty {
            return ClipboardImagePayload(
                data: pngData,
                mimeType: UTType.png.preferredMIMEType ?? "image/png",
                utType: UTType.png.identifier,
                suggestedExtension: "png"
            )
        }

        let imageData = pasteboard.data(forType: .png) ?? pasteboard.data(forType: .tiff)
        let image = imageData.flatMap(NSImage.init(data:))
            ?? (pasteboard.readObjects(forClasses: [NSImage.self], options: nil)?.first as? NSImage)

        guard let image else { return nil }
        return encodedImagePayload(from: image)
    }

    private static func encodedImagePayload(from image: NSImage) -> ClipboardImagePayload? {
        guard let tiffData = image.tiffRepresentation,
              let bitmap = NSBitmapImageRep(data: tiffData) else {
            return nil
        }

        guard let data = bitmap.representation(using: .png, properties: [:]),
              !data.isEmpty else {
            return nil
        }

        return ClipboardImagePayload(
            data: data,
            mimeType: UTType.png.preferredMIMEType ?? "image/png",
            utType: UTType.png.identifier,
            suggestedExtension: "png"
        )
    }
}
#else
import UIKit
import UniformTypeIdentifiers

nonisolated package struct ClipboardImagePayload: Sendable {
    nonisolated package let data: Data
    nonisolated package let mimeType: String
    nonisolated package let utType: String
    nonisolated package let suggestedExtension: String

    nonisolated package var sizeBytes: Int { data.count }
}

nonisolated package struct ClipboardSnapshot: Sendable {
    nonisolated package let text: String?
    nonisolated package let image: ClipboardImagePayload?

    nonisolated package var hasText: Bool {
        guard let text else { return false }
        return !text.isEmpty
    }

    nonisolated package var hasImage: Bool {
        image != nil
    }
}

package enum Clipboard {
    static package func copy(_ text: String) {
        UIPasteboard.general.string = text
    }

    static package func readString() -> String? {
        UIPasteboard.general.string
    }

    static package func copy(lines: [String], separator: String = "\n") {
        copy(lines.joined(separator: separator))
    }

    @MainActor
    static package func snapshot() -> ClipboardSnapshot {
        let pasteboard = UIPasteboard.general
        return ClipboardSnapshot(
            text: normalizedText(pasteboard.string),
            image: pasteboard.image.flatMap { encodedImagePayload(from: $0) }
        )
    }

    private static func normalizedText(_ text: String?) -> String? {
        guard let text, !text.isEmpty else { return nil }
        return text
    }

    private static func encodedImagePayload(from image: UIImage) -> ClipboardImagePayload? {
        guard let data = image.pngData(), !data.isEmpty else { return nil }

        return ClipboardImagePayload(
            data: data,
            mimeType: UTType.png.preferredMIMEType ?? "image/png",
            utType: UTType.png.identifier,
            suggestedExtension: "png"
        )
    }
}
#endif
