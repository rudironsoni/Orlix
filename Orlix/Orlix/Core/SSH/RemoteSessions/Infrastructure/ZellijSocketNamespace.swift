import Foundation

nonisolated enum ZellijSocketNamespace: Sendable {
    case user
    case managed

    init(ownership: RemoteSessionOwnership) {
        self = ownership == .managed ? .managed : .user
    }

    var setupScript: String {
        switch self {
        case .user:
            return ""
        case .managed:
            return """
            orlixZellijUID="$(id -u)"
            case "$orlixZellijUID" in ''|*[!0-9]*) exit 1 ;; esac
            ZELLIJ_SOCKET_DIR="/tmp/orlix-zellij-$orlixZellijUID"
            if [ ! -d "$ZELLIJ_SOCKET_DIR" ]; then
              (umask 077; mkdir "$ZELLIJ_SOCKET_DIR") 2>/dev/null || true
            fi
            [ -d "$ZELLIJ_SOCKET_DIR" ] && [ ! -L "$ZELLIJ_SOCKET_DIR" ] || exit 1
            orlixZellijOwner="$(stat -f '%u' "$ZELLIJ_SOCKET_DIR" 2>/dev/null || true)"
            case "$orlixZellijOwner" in
              ''|*[!0-9]*) orlixZellijOwner="$(stat -c '%u' "$ZELLIJ_SOCKET_DIR" 2>/dev/null || true)" ;;
            esac
            [ "$orlixZellijOwner" = "$orlixZellijUID" ] || exit 1
            chmod 700 "$ZELLIJ_SOCKET_DIR" || exit 1
            export ZELLIJ_SOCKET_DIR
            """
        }
    }
}
