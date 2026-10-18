import Foundation

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
@MainActor
package protocol ServerManagerPreferences: AnyObject {
    var didBootstrapDefaultWorkspace: Bool { get set }
    var hasSeenWelcome: Bool { get }
    var freePlanGeneration: FreePlanGeneration? { get set }
    var pendingBootstrapWorkspaceID: UUID? { get set }
}
