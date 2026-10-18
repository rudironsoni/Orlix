import SwiftUI
import UniformTypeIdentifiers

#if os(iOS)
import UIKit

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package struct RemoteFileRow: View {
    package let entry: RemoteFileEntry

    package var body: some View {
        HStack(spacing: 12) {
            Image(systemName: entry.iconName)
                .font(.system(size: 22, weight: .medium))
                .foregroundStyle(entry.type == .directory ? folderTint : .secondary)
                .frame(width: 28)

            VStack(alignment: .leading, spacing: 3) {
                Text(entry.name)
                    .font(.body)
                    .foregroundStyle(.primary)
                    .lineLimit(1)

                Text(subtitle)
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                    .lineLimit(1)
            }

            Spacer(minLength: 8)

            if entry.type == .directory {
                Image(systemName: "chevron.right")
                    .font(.caption.weight(.semibold))
                    .foregroundStyle(.tertiary)
            }
        }
        .contentShape(Rectangle())
    }

    private var subtitle: String {
        var parts: [String] = []

        if let modifiedAt = entry.modifiedAt {
            parts.append(modifiedAt.formatted(date: .abbreviated, time: .omitted))
        }

        switch entry.type {
        case .directory:
            parts.append(String(localized: "Folder"))
        default:
            if let size = entry.size {
                parts.append(RemoteFileByteCountFormatter.string(from: size))
            }
        }

        return parts.isEmpty ? "—" : parts.joined(separator: " · ")
    }

    private var folderTint: Color {
        Color.blue
    }
}

package struct RemoteFileShareSheet: UIViewControllerRepresentable {
    package let item: RemoteFileShareItem
    package let onComplete: () -> Void

    package func makeCoordinator() -> Coordinator {
        Coordinator(onComplete: onComplete)
    }

    package func makeUIViewController(context: Context) -> UIActivityViewController {
        let controller = UIActivityViewController(
            activityItems: [item.sourceURL],
            applicationActivities: nil
        )
        controller.completionWithItemsHandler = { _, _, _, _ in
            context.coordinator.finish()
        }
        return controller
    }

    package func updateUIViewController(_ uiViewController: UIActivityViewController, context: Context) {}

    final package class Coordinator {
        private let onComplete: () -> Void
        private var didFinish = false

        package init(onComplete: @escaping () -> Void) {
            self.onComplete = onComplete
        }

        package func finish() {
            guard !didFinish else { return }
            didFinish = true
            onComplete()
        }
    }
}

package struct RemoteFileImportPicker: UIViewControllerRepresentable {
    package let onComplete: (Result<[URL], Error>) -> Void

    package func makeCoordinator() -> Coordinator {
        Coordinator(onComplete: onComplete)
    }

    package func makeUIViewController(context: Context) -> UIDocumentPickerViewController {
        let controller = UIDocumentPickerViewController(
            forOpeningContentTypes: [.item, .folder],
            asCopy: true
        )
        controller.delegate = context.coordinator
        controller.allowsMultipleSelection = true
        return controller
    }

    package func updateUIViewController(_ uiViewController: UIDocumentPickerViewController, context: Context) {}

    final package class Coordinator: NSObject, UIDocumentPickerDelegate {
        private let onComplete: (Result<[URL], Error>) -> Void
        private var didFinish = false

        package init(onComplete: @escaping (Result<[URL], Error>) -> Void) {
            self.onComplete = onComplete
        }

        package func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) {
            finish(.success(urls))
        }

        package func documentPickerWasCancelled(_ controller: UIDocumentPickerViewController) {
            finish(.success([]))
        }

        private func finish(_ result: Result<[URL], Error>) {
            guard !didFinish else { return }
            didFinish = true
            onComplete(result)
        }
    }
}
#endif
