import Combine
import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package extension RemoteFileBrowserStore {
    package func requestUploadPicker(for tab: RemoteFileTab, destinationPath: String) {
        setPendingToolbarCommand(
            ToolbarCommand(
                serverId: tab.serverId,
                tabId: tab.id,
                action: .upload(destinationPath: RemoteFilePath.normalize(destinationPath))
            )
        )
    }

    package func requestCreateFolder(for tab: RemoteFileTab, destinationPath: String) {
        setPendingToolbarCommand(
            ToolbarCommand(
                serverId: tab.serverId,
                tabId: tab.id,
                action: .createFolder(destinationPath: RemoteFilePath.normalize(destinationPath))
            )
        )
    }

    package func consumeToolbarCommand(_ command: ToolbarCommand) {
        guard pendingToolbarCommand?.id == command.id else { return }
        setPendingToolbarCommand(nil)
    }
}
