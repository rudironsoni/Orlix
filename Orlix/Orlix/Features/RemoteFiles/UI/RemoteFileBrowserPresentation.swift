import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package enum RemoteFileBrowserPresentation: Identifiable {
    package enum ID: Hashable {
        case upload
        case share
        case createFolder
        case rename
        case move
        case delete
        case permissions
        case downloadExport
        case operationError
        case transferCancellation
    }

    package struct CreateFolderDraft {
        package let destinationPath: String
        package var name = ""
        package var isSubmitting = false
    }

    package struct RenameDraft {
        package let entry: RemoteFileEntry
        package var name: String
        package var isSubmitting = false
    }

    package struct MoveDraft {
        package let entry: RemoteFileEntry
        package var destinationDirectory: String
        package var isSubmitting = false
    }

    package struct PermissionDraft {
        package let entry: RemoteFileEntry
        package var permissions: RemoteFilePermissionDraft
        package let originalAccessBits: UInt32
        package let preservedBits: UInt32
        package let fileTypeBits: UInt32
        package var isSubmitting = false
        package var errorMessage: String?
    }

    package struct DownloadExport {
        package let document: RemoteFileDownloadDocument
        package let filename: String
        package let transferID: UUID
    }

    case upload(destinationPath: String)
    case share(RemoteFileShareItem)
    case createFolder(CreateFolderDraft)
    case rename(RenameDraft)
    case move(MoveDraft)
    case delete(RemoteFileEntry)
    case permissions(PermissionDraft)
    case downloadExport(DownloadExport)
    case operationError(String)
    case transferCancellation(RemoteFileTransferCancellationRequest)

    package var id: ID {
        switch self {
        case .upload: .upload
        case .share: .share
        case .createFolder: .createFolder
        case .rename: .rename
        case .move: .move
        case .delete: .delete
        case .permissions: .permissions
        case .downloadExport: .downloadExport
        case .operationError: .operationError
        case .transferCancellation: .transferCancellation
        }
    }

    package var isAlert: Bool {
        switch self {
        case .operationError, .transferCancellation:
            true
        default:
            false
        }
    }

    package var isDownloadExport: Bool {
        if case .downloadExport = self { true } else { false }
    }

    package var isIOSSheet: Bool {
        switch self {
        case .upload, .share, .createFolder, .rename, .move, .delete, .permissions:
            true
        default:
            false
        }
    }

    package var isMacOSSheet: Bool {
        switch self {
        case .move, .permissions:
            true
        default:
            false
        }
    }
}

package enum RemoteFileTransferKind {
    case upload
    case transfer

    package var confirmationTitle: String {
        switch self {
        case .upload:
            String(localized: "Cancel Upload?")
        case .transfer:
            String(localized: "Cancel Transfer?")
        }
    }

    package var cancelButtonTitle: String {
        switch self {
        case .upload:
            String(localized: "Cancel Upload")
        case .transfer:
            String(localized: "Cancel Transfer")
        }
    }

    package var keepButtonTitle: String {
        switch self {
        case .upload:
            String(localized: "Keep Uploading")
        case .transfer:
            String(localized: "Continue Transfer")
        }
    }

    package var confirmationMessage: String {
        switch self {
        case .upload:
            String(localized: "The current upload will stop.")
        case .transfer:
            String(localized: "The current file transfer will stop.")
        }
    }
}

package struct RemoteFileTransferCancellationRequest: Identifiable {
    package let id: UUID
    package let kind: RemoteFileTransferKind
}
