import Foundation

nonisolated enum ZellijRemoteSessionCommandBuilder {
    static let availableMarker = "__VVTERM_ZELLIJ_OK__"
    static let missingMarker = "__VVTERM_ZELLIJ_NO__"
    static let pathMarker = "__VVTERM_ZELLIJ_PATH__"

    private static let maximumLaunchCommandBytes = 96 * 1_024
    private static let clearedEnvironment: Set<String> = [
        "ZELLIJ",
        "ZELLIJ_AUTO_ATTACH",
        "ZELLIJ_AUTO_EXIT",
        "ZELLIJ_PANE_ID",
        "ZELLIJ_SESSION_NAME"
    ]

    static func availabilityProbeCommand() -> String {
        let script = """
        \(RemoteTerminalBootstrap.shellPathExport());
        orlixZellij="$(command -v zellij 2>/dev/null || true)";
        if [ -z "$orlixZellij" ]; then
          for candidate in /opt/homebrew/bin/zellij /usr/local/bin/zellij /usr/bin/zellij /snap/bin/zellij "$HOME/.local/bin/zellij" "$HOME/.cargo/bin/zellij"; do
            if [ -x "$candidate" ] && [ ! -d "$candidate" ]; then orlixZellij="$candidate"; break; fi;
          done;
        fi;
        case "$orlixZellij" in
          /*) ;;
          *) orlixZellij="" ;;
        esac;
        if [ -n "$orlixZellij" ] && [ -x "$orlixZellij" ] && [ ! -d "$orlixZellij" ]; then
          orlixZellijVersion="$(env -u ZELLIJ -u ZELLIJ_AUTO_ATTACH -u ZELLIJ_AUTO_EXIT -u ZELLIJ_PANE_ID -u ZELLIJ_SESSION_NAME "$orlixZellij" --version 2>/dev/null | sed -n '1p')";
          if [ -n "$orlixZellijVersion" ]; then
            printf '%s\n%s%s\n%s\n' '\(availableMarker)' '\(pathMarker)' "$orlixZellij" "$orlixZellijVersion";
          else
            printf '%s\n' '__VVTERM_ZELLIJ_INVALID__';
          fi;
        else
          printf '%s\n' '\(missingMarker)';
        fi
        """
        return RemoteTerminalBootstrap.wrapPOSIXShellCommand(script)
    }

    static func listCommand(
        scope: RemoteSessionListScope,
        runtime: RemoteSessionRuntime
    ) throws -> String {
        let namespace: ZellijSocketNamespace = switch scope {
        case .userVisible: .user
        case .managedCleanup: .managed
        }
        let ownership = switch scope {
        case .userVisible: RemoteSessionOwnership.external.rawValue
        case .managedCleanup: RemoteSessionOwnership.managed.rawValue
        }
        let list = try render(
            arguments: ["list-sessions", "--short", "--no-formatting"],
            runtime: runtime
        )
        let executable = try render(arguments: [], runtime: runtime)
        let ownershipFilter = scope == .managedCleanup
            ? """
              orlixZellijOwnershipMarker="$orlixZellijOwnershipRoot/$orlixZellijSession"
              [ -f "$orlixZellijOwnershipMarker" ] \
                && [ ! -L "$orlixZellijOwnershipMarker" ] || continue
            """
            : ":"

        let script = """
        \(namespace.setupScript)
        \(scope == .managedCleanup ? managedMetadataSetupScript() : "")
        umask 077
        orlixZellijListFile="$(mktemp "${TMPDIR:-/tmp}/orlix-zellij-list.XXXXXX")" || exit 1
        orlixZellijProbeFile="$(mktemp "${TMPDIR:-/tmp}/orlix-zellij-probe.XXXXXX")" || {
          rm -f "$orlixZellijListFile"
          exit 1
        }
        orlixZellijStatusFile="$(mktemp "${TMPDIR:-/tmp}/orlix-zellij-status.XXXXXX")" || {
          rm -f "$orlixZellijListFile" "$orlixZellijProbeFile"
          exit 1
        }
        chmod 600 "$orlixZellijListFile" "$orlixZellijProbeFile" \
          "$orlixZellijStatusFile" || {
          rm -f "$orlixZellijListFile" "$orlixZellijProbeFile" \
            "$orlixZellijStatusFile"
          exit 1
        }
        orlixZellijCleanupListFiles() {
          rm -f "$orlixZellijListFile" "$orlixZellijProbeFile" \
            "$orlixZellijStatusFile"
        }
        orlixZellijCapture() {
          orlixZellijCaptureOutput="$1"
          orlixZellijCaptureStatusFile="$2"
          shift 2
          : >"$orlixZellijCaptureOutput" \
            && : >"$orlixZellijCaptureStatusFile" || return 1
          ("$@"; printf '%s\n' "$?" >"$orlixZellijCaptureStatusFile") 2>&1 \
            | head -c \(ZellijRemoteSessionParser.maximumOutputBytes + 1) \
              >"$orlixZellijCaptureOutput"
          [ "$?" -eq 0 ] || return 1
          orlixZellijCapturedBytes="$(wc -c <"$orlixZellijCaptureOutput" | tr -d '[:space:]')"
          case "$orlixZellijCapturedBytes" in ''|*[!0-9]*) return 1 ;; esac
          [ "$orlixZellijCapturedBytes" -le \(ZellijRemoteSessionParser.maximumOutputBytes) ] \
            || return 1
          orlixZellijCapturedStatus="$(sed -n '1p' "$orlixZellijCaptureStatusFile")"
          case "$orlixZellijCapturedStatus" in ''|*[!0-9]*) return 1 ;; esac
          [ "$orlixZellijCapturedStatus" -le 255 ] || return 1
        }
        trap 'orlixZellijCleanupListFiles; exit 1' HUP INT TERM
        trap orlixZellijCleanupListFiles EXIT

        orlixZellijCapture "$orlixZellijListFile" "$orlixZellijStatusFile" \
          \(list) || exit 1
        orlixZellijListStatus="$orlixZellijCapturedStatus"
        if [ "$orlixZellijListStatus" -eq 1 ] \
           && [ "$(cat "$orlixZellijListFile")" = 'No active zellij sessions found.' ]; then
          : >"$orlixZellijListFile"
        elif [ "$orlixZellijListStatus" -ne 0 ]; then
          exit 1
        fi

        orlixZellijCandidateCount=0
        while IFS= read -r orlixZellijSession || [ -n "$orlixZellijSession" ]; do
          [ -n "$orlixZellijSession" ] || continue
          orlixZellijCandidateCount=$((orlixZellijCandidateCount + 1))
          [ "$orlixZellijCandidateCount" -le \(ZellijRemoteSessionParser.maximumSessionCount) ] || exit 1
          case "$orlixZellijSession" in '.'|'..'|*/*) exit 1 ;; esac
          orlixZellijNameBytes="$(printf '%s' "$orlixZellijSession" | wc -c | tr -d '[:space:]')"
          case "$orlixZellijNameBytes" in ''|*[!0-9]*) exit 1 ;; esac
          [ "$orlixZellijNameBytes" -le \(RemoteSessionIdentifier.maximumRawValueLength) ] || exit 1
          orlixZellijCleanName="$(printf '%s' "$orlixZellijSession" | LC_ALL=C tr -d '[:cntrl:]')"
          [ "$orlixZellijCleanName" = "$orlixZellijSession" ] || exit 1

          \(ownershipFilter)
          orlixZellijCapture "$orlixZellijProbeFile" "$orlixZellijStatusFile" \
            \(executable) '--session' "$orlixZellijSession" \
              'action' 'list-panes' '--json' || exit 1
          orlixZellijProbeStatus="$orlixZellijCapturedStatus"
          [ "$orlixZellijProbeStatus" -eq 0 ] || continue

          if orlixZellijCapture "$orlixZellijProbeFile" "$orlixZellijStatusFile" \
               \(executable) '--session' "$orlixZellijSession" \
                 'action' 'list-clients' \
             && [ "$orlixZellijCapturedStatus" -eq 0 ]; then
            orlixZellijClientCount="$(awk '
              NR == 1 {
                if (NF != 3 || $1 != "CLIENT_ID" || $2 != "ZELLIJ_PANE_ID" || $3 != "RUNNING_COMMAND") exit 2
                next
              }
              NF {
                count += 1
                if (count > \(ZellijRemoteSessionParser.maximumAttachedClientCount)) exit 2
              }
              END {
                if (NR == 0) exit 2
                print count + 0
              }
            ' "$orlixZellijProbeFile")"
            orlixZellijClientStatus=$?
            if [ "$orlixZellijClientStatus" -ne 0 ]; then
              orlixZellijClientCount='?'
            fi
          else
            orlixZellijClientCount='?'
          fi
          case "$orlixZellijClientCount" in
            '?'|[0-9]|[0-9][0-9]|[0-9][0-9][0-9]|[0-9][0-9][0-9][0-9]) ;;
            *) orlixZellijClientCount='?' ;;
          esac
          printf 'name=%s\townership=%s\tclients=%s\n' \
            "$orlixZellijSession" '\(ownership)' "$orlixZellijClientCount"
        done <"$orlixZellijListFile"
        """
        return try wrappedBoundedLaunchScript(script)
    }

    static func listPanesCommand(
        attachment: RemoteSessionAttachment,
        runtime: RemoteSessionRuntime
    ) throws -> String {
        let identifier = attachment.identifier
        try requireValid(identifier)
        let namespace = ZellijSocketNamespace(ownership: attachment.ownership)
        let command = try render(
            arguments: [
                "--session",
                identifier.rawValue,
                "action",
                "list-panes",
                "--all",
                "--json"
            ],
            runtime: runtime
        )
        let ownershipGuard = attachment.ownership == .managed
            ? managedOwnershipGuardScript(identifier: identifier)
            : ""
        let script = """
        \(namespace.setupScript)
        \(attachment.ownership == .managed ? managedMetadataSetupScript() : "")
        \(ownershipGuard)
        \(command)
        """
        return RemoteTerminalBootstrap.wrapPOSIXShellCommand(script)
    }

    static func launchCommand(
        request: RemoteSessionLaunchRequest,
        runtime: RemoteSessionRuntime
    ) throws -> String {
        let attachment = request.intent.attachment
        let identifier = attachment.identifier
        try requireValid(identifier)
        let namespace = ZellijSocketNamespace(ownership: attachment.ownership)
        let executable = try render(arguments: [], runtime: runtime)
        let attach = try render(
            arguments: ["attach", identifier.rawValue],
            runtime: runtime
        )
        let attached = quotedMarker(request.lifecycleEnvelope, .attached)
        let detached = quotedMarker(request.lifecycleEnvelope, .detached)
        let terminated = quotedMarker(request.lifecycleEnvelope, .terminated)
        let creationFailed = quotedMarker(request.lifecycleEnvelope, .creationFailed)
        let attachFailed = quotedMarker(request.lifecycleEnvelope, .attachFailed)
        let failureMarker: String
        let intentScript: String

        switch request.intent {
        case .attach(let existingAttachment):
            failureMarker = attachFailed
            let ownershipGuard = existingAttachment.ownership == .managed
                ? """
                  if [ ! -f "$orlixZellijOwnershipMarker" ] \
                     || [ -L "$orlixZellijOwnershipMarker" ]; then
                    printf '%s' \(attachFailed)
                    exit 0
                  fi
                """
                : ""
            intentScript = """
            \(ownershipGuard)
            orlixZellijProbe
            if [ "$orlixZellijState" != live ]; then
              printf '%s' \(attachFailed)
              exit 0
            fi
            """
        case .ensureManaged(_, let initialCommand):
            failureMarker = creationFailed
            guard attachment.ownership == .managed else {
                throw SSHError.unknown("Orlix can create only managed Zellij sessions")
            }
            let command = try validatedStartupCommand(initialCommand)
            let create = try managedCreateCommand(
                identifier: identifier,
                initialCommand: command,
                workingDirectory: request.workingDirectory,
                runtime: runtime
            )
            intentScript = """
            if ! orlixZellijAcquireCreationLock; then
              printf '%s' \(creationFailed)
              exit 0
            fi
            orlixZellijProbe
            case "$orlixZellijState" in
              live)
                if [ ! -f "$orlixZellijOwnershipMarker" ] \
                   || [ -L "$orlixZellijOwnershipMarker" ]; then
                  orlixZellijReleaseCreationLock
                  printf '%s' \(creationFailed)
                  exit 0
                fi
                ;;
              missing)
                if [ -e "$orlixZellijOwnershipMarker" ] \
                   && { [ ! -f "$orlixZellijOwnershipMarker" ] \
                        || [ -L "$orlixZellijOwnershipMarker" ]; }; then
                  orlixZellijReleaseCreationLock
                  printf '%s' \(creationFailed)
                  exit 0
                fi
                if [ ! -f "$orlixZellijOwnershipMarker" ]; then
                  (umask 077; printf '%s\n' 'managed-v1' \
                    >"$orlixZellijOwnershipMarker") 2>/dev/null \
                    && chmod 600 "$orlixZellijOwnershipMarker" || {
                      orlixZellijReleaseCreationLock
                      printf '%s' \(creationFailed)
                      exit 0
                    }
                fi
                \(create) >/dev/null 2>&1
                orlixZellijCreateStatus=$?
                orlixZellijProbe
                if [ "$orlixZellijCreateStatus" -ne 0 ] \
                   || [ "$orlixZellijState" != live ]; then
                  orlixZellijReleaseCreationLock
                  printf '%s' \(creationFailed)
                  exit 0
                fi
                ;;
              *)
                orlixZellijReleaseCreationLock
                printf '%s' \(creationFailed)
                exit 0
                ;;
            esac
            orlixZellijReleaseCreationLock
            """
        }

        let managedSetup = attachment.ownership == .managed
            ? """
              \(managedMetadataSetupScript())
              orlixZellijOwnershipMarker="$orlixZellijOwnershipRoot/$orlixZellijSessionName"
              orlixZellijCreationLock="$orlixZellijLockRoot/$orlixZellijSessionName.lock"
              \(creationLockFunctions())
            """
            : "orlixZellijReleaseCreationLock() { :; }"
        let terminalEnvironment = RemoteTerminalBootstrap.environmentExportScript(
            transport: request.transport
        )
        let script = """
        \(RemoteTerminalBootstrap.shellPathExport())
        \(terminalEnvironment)
        \(namespace.setupScript)
        orlixZellijSessionName=\(RemoteTerminalBootstrap.shellQuoted(identifier.rawValue))
        \(managedSetup)
        orlixZellijOwnsCreationLock=0
        umask 077
        orlixZellijProbeFile="$(mktemp "${TMPDIR:-/tmp}/orlix-zellij-probe.XXXXXX")" || {
          printf '%s' \(failureMarker)
          exit 0
        }
        orlixZellijProbeStatusFile="$(mktemp "${TMPDIR:-/tmp}/orlix-zellij-status.XXXXXX")" || {
          rm -f "$orlixZellijProbeFile"
          printf '%s' \(failureMarker)
          exit 0
        }
        chmod 600 "$orlixZellijProbeFile" "$orlixZellijProbeStatusFile" || {
          rm -f "$orlixZellijProbeFile" "$orlixZellijProbeStatusFile"
          printf '%s' \(failureMarker)
          exit 0
        }
        orlixZellijCleanup() {
          orlixZellijReleaseCreationLock
          rm -f "$orlixZellijProbeFile" "$orlixZellijProbeStatusFile"
        }
        trap 'orlixZellijCleanup; exit 0' HUP INT TERM
        trap orlixZellijCleanup EXIT
        \(liveProbeFunction(executable: executable))
        \(intentScript)
        printf '%s' \(attached)
        \(attach)
        orlixZellijAttachStatus=$?
        if [ "$orlixZellijAttachStatus" -ne 0 ]; then
          printf '%s' \(attachFailed)
        else
          orlixZellijProbe
          case "$orlixZellijState" in
            live) printf '%s' \(detached) ;;
            missing) printf '%s' \(terminated) ;;
          esac
        fi
        """
        return try wrappedBoundedLaunchScript(script)
    }

    static func presenceProbe(
        attachment: RemoteSessionAttachment,
        runtime: RemoteSessionRuntime
    ) throws -> RemoteSessionPresenceProbe {
        let identifier = attachment.identifier
        try requireValid(identifier)
        let markerID = UUID().uuidString
        let existsMarker = "__VVTERM_SESSION_EXISTS_\(markerID)__"
        let missingMarker = "__VVTERM_SESSION_MISSING_\(markerID)__"
        let namespace = ZellijSocketNamespace(ownership: attachment.ownership)
        let executable = try render(arguments: [], runtime: runtime)
        let managedSetup = attachment.ownership == .managed
            ? """
              \(managedMetadataSetupScript())
              orlixZellijOwnershipMarker="$orlixZellijOwnershipRoot/$orlixZellijSessionName"
            """
            : ""
        let ownershipCondition = attachment.ownership == .managed
            ? "[ -f \"$orlixZellijOwnershipMarker\" ] && [ ! -L \"$orlixZellijOwnershipMarker\" ]"
            : "true"
        let script = """
        \(namespace.setupScript)
        orlixZellijSessionName=\(RemoteTerminalBootstrap.shellQuoted(identifier.rawValue))
        \(managedSetup)
        umask 077
        orlixZellijProbeFile="$(mktemp "${TMPDIR:-/tmp}/orlix-zellij-probe.XXXXXX")" || exit 0
        orlixZellijProbeStatusFile="$(mktemp "${TMPDIR:-/tmp}/orlix-zellij-status.XXXXXX")" || {
          rm -f "$orlixZellijProbeFile"
          exit 0
        }
        chmod 600 "$orlixZellijProbeFile" "$orlixZellijProbeStatusFile" || {
          rm -f "$orlixZellijProbeFile" "$orlixZellijProbeStatusFile"
          exit 0
        }
        orlixZellijCleanupProbeFiles() {
          rm -f "$orlixZellijProbeFile" "$orlixZellijProbeStatusFile"
        }
        trap 'orlixZellijCleanupProbeFiles; exit 0' HUP INT TERM
        trap orlixZellijCleanupProbeFiles EXIT
        \(liveProbeFunction(executable: executable))
        if [ "$orlixZellijState" = live ]; then
          if \(ownershipCondition); then
            printf '%s' \(RemoteTerminalBootstrap.shellQuoted(existsMarker))
          else
            printf '%s' \(RemoteTerminalBootstrap.shellQuoted(missingMarker))
          fi
        elif [ "$orlixZellijState" = missing ]; then
          printf '%s' \(RemoteTerminalBootstrap.shellQuoted(missingMarker))
        fi
        """
        return RemoteSessionPresenceProbe(
            command: RemoteTerminalBootstrap.wrapPOSIXShellCommand(script),
            existsMarker: existsMarker,
            missingMarker: missingMarker
        )
    }

    static func killManagedCommand(
        identifier: RemoteSessionIdentifier,
        runtime: RemoteSessionRuntime
    ) throws -> String {
        try requireValid(identifier)
        let kill = try render(
            arguments: ["kill-session", identifier.rawValue],
            runtime: runtime
        )
        let script = """
        \(ZellijSocketNamespace.managed.setupScript)
        \(managedMetadataSetupScript())
        orlixZellijSessionName=\(RemoteTerminalBootstrap.shellQuoted(identifier.rawValue))
        orlixZellijOwnershipMarker="$orlixZellijOwnershipRoot/$orlixZellijSessionName"
        if [ -f "$orlixZellijOwnershipMarker" ] \
           && [ ! -L "$orlixZellijOwnershipMarker" ] \
           && \(kill) >/dev/null 2>&1; then
          rm -f "$orlixZellijOwnershipMarker"
        fi
        """
        return RemoteTerminalBootstrap.wrapPOSIXShellCommand(script)
    }

    private static func managedCreateCommand(
        identifier: RemoteSessionIdentifier,
        initialCommand: String?,
        workingDirectory: String,
        runtime: RemoteSessionRuntime
    ) throws -> String {
        var arguments: [String] = []
        if let initialCommand {
            let encodedCommand = try ZellijKDLStringEncoder.encode(initialCommand)
            let layout = "layout { pane command=\"/bin/sh\" { "
                + "args \"-lc\" \(encodedCommand); }; }"
            guard layout.utf8.count <= ZellijKDLStringEncoder.maximumEncodedByteCount else {
                throw SSHError.outputLimitExceeded
            }
            arguments.append(contentsOf: ["--layout-string", layout])
        }
        arguments.append(contentsOf: [
            "attach",
            "--create-background",
            identifier.rawValue,
            "options",
            "--session-serialization",
            "false"
        ])
        return try render(
            arguments: arguments,
            workingDirectory: workingDirectory == "~" ? nil : workingDirectory,
            runtime: runtime
        )
    }

    private static func validatedStartupCommand(_ command: String?) throws -> String? {
        guard let command,
              !command.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
            return nil
        }
        do {
            return try RemoteShellStartupAction(command: command).command
        } catch {
            throw SSHError.unknown("Invalid custom startup command")
        }
    }

    private static func managedMetadataSetupScript() -> String {
        """
        orlixZellijMetadataRoot="$ZELLIJ_SOCKET_DIR/.orlix"
        orlixZellijOwnershipRoot="$orlixZellijMetadataRoot/owned"
        orlixZellijLockRoot="$orlixZellijMetadataRoot/locks"
        for orlixZellijDirectory in "$orlixZellijMetadataRoot" \
                                      "$orlixZellijOwnershipRoot" \
                                      "$orlixZellijLockRoot"; do
          if [ ! -d "$orlixZellijDirectory" ]; then
            (umask 077; mkdir "$orlixZellijDirectory") 2>/dev/null || true
          fi
          [ -d "$orlixZellijDirectory" ] \
            && [ ! -L "$orlixZellijDirectory" ] \
            && chmod 700 "$orlixZellijDirectory" || exit 1
        done
        """
    }

    private static func managedOwnershipGuardScript(
        identifier: RemoteSessionIdentifier
    ) -> String {
        """
        orlixZellijSessionName=\(RemoteTerminalBootstrap.shellQuoted(identifier.rawValue))
        orlixZellijOwnershipMarker="$orlixZellijOwnershipRoot/$orlixZellijSessionName"
        [ -f "$orlixZellijOwnershipMarker" ] \
          && [ ! -L "$orlixZellijOwnershipMarker" ] || exit 1
        """
    }

    private static func creationLockFunctions() -> String {
        """
        orlixZellijReleaseCreationLock() {
          if [ "${orlixZellijOwnsCreationLock:-0}" -eq 1 ]; then
            rmdir "$orlixZellijCreationLock" 2>/dev/null || true
            orlixZellijOwnsCreationLock=0
          fi
        }
        orlixZellijAcquireCreationLock() {
          orlixZellijLockAttempt=0
          while ! (umask 077; mkdir "$orlixZellijCreationLock") 2>/dev/null; do
            orlixZellijLockAttempt=$((orlixZellijLockAttempt + 1))
            [ "$orlixZellijLockAttempt" -lt 100 ] || return 1
            sleep 0.05
          done
          orlixZellijOwnsCreationLock=1
        }
        """
    }

    private static func liveProbeFunction(executable: String) -> String {
        """
        orlixZellijProbe() {
          : >"$orlixZellijProbeFile" \
            && : >"$orlixZellijProbeStatusFile" || {
            orlixZellijState=unknown
            return
          }
          (\(executable) '--session' "$orlixZellijSessionName" \
            'action' 'list-panes' '--json'; \
            printf '%s\n' "$?" >"$orlixZellijProbeStatusFile") 2>&1 \
              | head -c \(ZellijRemoteSessionParser.maximumOutputBytes + 1) \
                >"$orlixZellijProbeFile"
          orlixZellijCaptureStatus=$?
          orlixZellijProbeBytes="$(wc -c <"$orlixZellijProbeFile" | tr -d '[:space:]')"
          orlixZellijProbeStatus="$(sed -n '1p' "$orlixZellijProbeStatusFile")"
          case "$orlixZellijProbeBytes" in
            ''|*[!0-9]*) orlixZellijState=unknown; return ;;
          esac
          case "$orlixZellijProbeStatus" in
            ''|*[!0-9]*) orlixZellijState=unknown; return ;;
          esac
          if [ "$orlixZellijCaptureStatus" -ne 0 ] \
             || [ "$orlixZellijProbeBytes" -gt \(ZellijRemoteSessionParser.maximumOutputBytes) ] \
             || [ "$orlixZellijProbeStatus" -gt 255 ]; then
            orlixZellijState=unknown
          elif [ "$orlixZellijProbeStatus" -eq 0 ]; then
            orlixZellijState=live
          else
            orlixZellijProbeFirstLine="$(sed -n '1p' "$orlixZellijProbeFile")"
            orlixZellijExpectedMissing="Session '$orlixZellijSessionName' not found. The following sessions are active:"
            if [ "$orlixZellijProbeFirstLine" = 'There is no active session!' ] \
               || [ "$orlixZellijProbeFirstLine" = "$orlixZellijExpectedMissing" ]; then
              orlixZellijState=missing
            else
              orlixZellijState=unknown
            fi
          fi
        }
        """
    }

    private static func render(
        arguments: [String],
        workingDirectory: String? = nil,
        runtime: RemoteSessionRuntime
    ) throws -> String {
        guard runtime.probe.backendIdentifier == .zellij,
              runtime.probe.shellFamily == .posix else {
            throw SSHError.unknown("Zellij requires a POSIX remote shell")
        }
        return try RemoteSessionCommandRenderer.render(
            RemoteSessionCommandPlan(
                executable: runtime.probe.executable,
                arguments: arguments,
                environmentRemovals: clearedEnvironment,
                workingDirectory: workingDirectory
            ),
            for: .posix
        )
    }

    private static func requireValid(_ identifier: RemoteSessionIdentifier) throws {
        guard identifier.backendIdentifier == .zellij,
              ZellijRemoteSessionParser.isValidSessionName(identifier.rawValue) else {
            throw SSHError.unknown("Invalid Zellij session identifier")
        }
    }

    private static func wrappedBoundedLaunchScript(_ script: String) throws -> String {
        let command = RemoteTerminalBootstrap.wrapPOSIXShellCommand(script)
        guard command.utf8.count <= maximumLaunchCommandBytes else {
            throw SSHError.outputLimitExceeded
        }
        return command
    }

    private static func quotedMarker(
        _ envelope: RemoteSessionLifecycleEnvelope,
        _ event: RemoteSessionEvent
    ) -> String {
        RemoteTerminalBootstrap.shellQuoted(
            RemoteSessionLifecycleMarker.sequence(envelope: envelope, event: event)
        )
    }
}
