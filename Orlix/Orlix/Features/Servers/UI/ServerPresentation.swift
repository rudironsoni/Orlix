import SwiftUI

#if ORLIX_MODULAR_APP
import OrlixModels
#endif
package extension Workspace {
    package var color: Color {
        Color.fromHex(colorHex)
    }

    static package let defaultColors: [String] = [
        "#007AFF", "#AF52DE", "#FF2D55", "#FF3B30", "#FF9500",
        "#FFCC00", "#34C759", "#5AC8FA", "#00C7BE", "#5856D6"
    ]
}

package extension ServerEnvironment {
    package var color: Color {
        Color.fromHex(colorHex)
    }

    nonisolated package var displayName: String {
        guard isBuiltIn else { return name }
        switch id {
        case ServerEnvironment.production.id: return String(localized: "Production")
        case ServerEnvironment.staging.id: return String(localized: "Staging")
        case ServerEnvironment.development.id: return String(localized: "Development")
        default: return name
        }
    }

    nonisolated package var displayShortName: String {
        guard isBuiltIn else { return shortName }
        switch id {
        case ServerEnvironment.production.id: return String(localized: "Prod")
        case ServerEnvironment.staging.id: return String(localized: "Stag")
        case ServerEnvironment.development.id: return String(localized: "Dev")
        default: return shortName
        }
    }
}

package extension CloudflareAccessMode {
    package var displayName: String {
        switch self {
        case .oauth: return String(localized: "OAuth")
        case .serviceToken: return String(localized: "Service Token")
        }
    }
}

package extension AuthMethod {
    package var displayName: String {
        switch self {
        case .password: return String(localized: "Password")
        case .sshKey: return String(localized: "SSH Key")
        case .sshKeyWithPassphrase: return String(localized: "SSH Key + Passphrase")
        }
    }

    package var icon: String {
        switch self {
        case .password: return "key.fill"
        case .sshKey: return "lock.doc.fill"
        case .sshKeyWithPassphrase: return "lock.shield.fill"
        }
    }
}
