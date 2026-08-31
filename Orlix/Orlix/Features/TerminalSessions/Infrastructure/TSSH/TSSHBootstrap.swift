import Foundation

nonisolated struct TSSHBootstrapResult: Sendable {
    let host: String
    let info: TSSHServerInfo
    let serverProcess: TSSHServerProcessIdentity
}

nonisolated struct TSSHServerProcessIdentity: Codable, Equatable, Sendable {
    let pid: Int32
    let supervisorPath: String
}

nonisolated enum TSSHBootstrap {
    private static let pathEntries = [
        "/opt/homebrew/bin",
        "/usr/local/bin",
        "$HOME/go/bin",
        "/usr/local/go/bin",
        "/home/linuxbrew/.linuxbrew/bin",
        "/snap/bin",
        "/usr/bin",
        "/bin",
        "/usr/sbin",
        "/sbin",
    ]

    static func start(
        server: Server,
        credentials: ServerCredentials,
        client: SSHClient
    ) async throws -> TSSHBootstrapResult {
        guard server.tsshProfile.isValid else { throw TSSHRuntimeError.invalidProfile }
        _ = try await client.connect(to: server, credentials: credentials)

        return try await startUsingConnectedClient(server: server, client: client)
    }

    static func startUsingConnectedClient(
        server: Server,
        client: SSHClient
    ) async throws -> TSSHBootstrapResult {
        guard server.tsshProfile.isValid else { throw TSSHRuntimeError.invalidProfile }

        let environment = await client.remoteEnvironment()
        guard supports(environment: environment) else {
            throw TSSHRuntimeError.unsupportedRemoteEnvironment
        }

        let command = launchCommand(profile: server.tsshProfile)
        let output: String
        do {
            output = try await client.execute(
                command,
                timeout: .seconds(45),
                maxOutputBytes: 64 * 1024,
                retainPartialOutputOnFailure: true
            )
        } catch {
            if let executionError = error as? SSHCommandExecutionError,
               let serverProcess = try? serverProcessIdentity(
                   output: executionError.partialOutput
               ) {
                await terminateServer(serverProcess, using: client)
            }
            let description = error.localizedDescription
            if description.contains("not found") || description.contains("TSSHD_NOT_FOUND") {
                throw TSSHRuntimeError.tsshdNotFound
            }
            throw TSSHRuntimeError.bootstrapFailed(description)
        }
        if output.contains("TSSHD_NOT_FOUND") {
            throw TSSHRuntimeError.tsshdNotFound
        }
        let serverProcess = try serverProcessIdentity(output: output)
        let info: TSSHServerInfo
        do {
            let parsedInfo = try TSSHServerInfo.parse(output: output)
            info = try validatedServerInfo(
                parsedInfo,
                for: server.tsshProfile
            ).assigningClientIDIfNeeded()
        } catch {
            await terminateServer(serverProcess, using: client)
            throw error
        }
        let host = await client.remoteEndpointHost() ?? server.host
        return TSSHBootstrapResult(host: host, info: info, serverProcess: serverProcess)
    }

    static func parseServerPID(output: String) throws -> Int32 {
        let prefix = "ORLIX_TSSHD_PID="
        guard let line = output.split(whereSeparator: \.isNewline).first(where: {
            $0.hasPrefix(prefix)
        }),
        let pid = Int32(line.dropFirst(prefix.count)),
        pid > 1 else {
            throw TSSHRuntimeError.invalidServerResponse
        }
        return pid
    }

    static func serverProcessIdentity(output: String) throws -> TSSHServerProcessIdentity {
        TSSHServerProcessIdentity(
            pid: try parseServerPID(output: output),
            supervisorPath: try parseSupervisorPath(output: output)
        )
    }

    static func parseSupervisorPath(output: String) throws -> String {
        let prefix = "ORLIX_TSSHD_SUPERVISOR="
        guard let line = output.split(whereSeparator: \.isNewline).first(where: {
            $0.hasPrefix(prefix)
        }) else {
            throw TSSHRuntimeError.invalidServerResponse
        }
        let path = String(line.dropFirst(prefix.count))
        guard path.hasPrefix("/"), !path.contains("\0") else {
            throw TSSHRuntimeError.invalidServerResponse
        }
        return path
    }

    static func terminateServer(
        _ identity: TSSHServerProcessIdentity,
        using client: SSHClient
    ) async {
        try? await terminateServerForCleanup(identity, using: client)
    }

    static func terminateServerForCleanup(
        _ identity: TSSHServerProcessIdentity,
        using client: SSHClient
    ) async throws {
        let command = terminationCommand(for: identity)
        let output = try await Task.detached {
            try await client.execute(
                command,
                timeout: .seconds(5),
                maxOutputBytes: 4 * 1024
            )
        }.value
        guard output.contains("ORLIX_TSSHD_TERMINATED=1")
                || output.contains("ORLIX_TSSHD_ABSENT=1") else {
            throw TSSHRuntimeError.bootstrapFailed(
                "The saved TSSH supervisor identity did not match the remote process."
            )
        }
    }

    static func terminationCommand(for identity: TSSHServerProcessIdentity) -> String {
        let body = """
        pid=\(identity.pid)
        supervisor=\(RemoteTerminalBootstrap.shellQuoted(identity.supervisorPath))
        command=$(ps -p "$pid" -o args= 2>/dev/null || true)
        if [ -z "$command" ]; then
          printf '%s\n' ORLIX_TSSHD_ABSENT=1
          exit 0
        fi
        case "$command" in
          *"$supervisor"*)
            kill -TERM "$pid" 2>/dev/null || exit 1
            attempts=0
            while kill -0 "$pid" 2>/dev/null; do
              [ "$attempts" -lt 20 ] || exit 1
              sleep 0.1
              attempts=$((attempts + 1))
            done
            printf '%s\n' ORLIX_TSSHD_TERMINATED=1
            ;;
          *) exit 1 ;;
        esac
        """
        return "sh -lc \(RemoteTerminalBootstrap.shellQuoted(body))"
    }

    static func validatedServerInfo(
        _ info: TSSHServerInfo,
        for profile: TSSHProfile
    ) throws -> TSSHServerInfo {
        let expectedMode: TSSHServerInfo.Mode = switch profile.transportMode {
        case .kcp: .kcp
        case .quic: .quic
        }
        guard info.mode == expectedMode,
              (profile.udpPortMinimum...profile.udpPortMaximum).contains(info.port) else {
            throw TSSHRuntimeError.invalidServerResponse
        }
        return info
    }

    static func supports(environment: RemoteEnvironment) -> Bool {
        environment.platform != .windows && environment.shellProfile.family == .posix
    }

    static func startCommand(profile: TSSHProfile, nonce: UUID = UUID()) -> String {
        launchCommand(profile: profile, nonce: nonce)
    }

    private static func launchCommand(
        profile: TSSHProfile,
        nonce: UUID = UUID()
    ) -> String {
        let binary = profile.serverPath ?? "tsshd"
        var arguments = [
            "--attachable",
            "--port",
            "\(profile.udpPortMinimum)-\(profile.udpPortMaximum)",
        ]
        if profile.mtu > 0 {
            arguments.append(contentsOf: ["--mtu", String(profile.mtu)])
        }
        switch profile.transportMode {
        case .quic:
            arguments.append("--quic")
        case .kcp:
            arguments.append("--kcp")
        }

        let quotedArguments = arguments.map(RemoteTerminalBootstrap.shellQuoted).joined(separator: " ")
        let script = """
        export PATH="\(pathEntries.joined(separator: ":")):$PATH"
        umask 077
        binary=\(RemoteTerminalBootstrap.shellQuoted(binary))
        workspace=$(mktemp -d "${TMPDIR:-/tmp}/orlix-tsshd.\(nonce.uuidString).XXXXXXXX") || exit 1
        chmod 700 "$workspace" || { rmdir "$workspace" 2>/dev/null || true; exit 1; }
        workspace_metadata=$(LC_ALL=C ls -ldn "$workspace") || exit 1
        set -- $workspace_metadata
        case "$1" in drwx------*) ;; *) rmdir "$workspace" 2>/dev/null || true; exit 1 ;; esac
        [ ! -L "$workspace" ] && [ "$3" = "$(id -u)" ] || {
          rmdir "$workspace" 2>/dev/null || true
          exit 1
        }
        output="$workspace/output"
        supervisor="$workspace/supervisor"
        pid=
        ready=0
        cleanup() {
          if [ "$ready" -ne 1 ] && [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
            kill -TERM "$pid" 2>/dev/null || true
            wait "$pid" 2>/dev/null || true
          fi
          rm -f "$output"
          if [ "$ready" -ne 1 ]; then
            rm -f "$supervisor"
            rmdir "$workspace" 2>/dev/null || true
          fi
          return 0
        }
        trap cleanup EXIT
        trap 'exit 1' HUP INT TERM
        if ! { [ -x "$binary" ] || command -v "$binary" >/dev/null 2>&1; }; then
          printf '%s\\n' TSSHD_NOT_FOUND
          exit 127
        fi
        cat >"$supervisor" <<'ORLIX_TSSHD_SUPERVISOR'
        #!/bin/sh
        child=
        cleanup() {
          trap - EXIT HUP INT TERM
          if [ -n "$child" ] && kill -0 "$child" 2>/dev/null; then
            kill -TERM "$child" 2>/dev/null || true
            wait "$child" 2>/dev/null || true
          fi
          workspace=${0%/*}
          rm -f "$0"
          rmdir "$workspace" 2>/dev/null || true
        }
        trap cleanup EXIT
        trap 'exit 143' HUP INT TERM
        "$@" &
        child=$!
        wait "$child"
        ORLIX_TSSHD_SUPERVISOR
        chmod 700 "$supervisor"
        : > "$output"
        nohup "$supervisor" "$binary" \(quotedArguments) >"$output" 2>&1 </dev/null &
        pid=$!
        printf 'ORLIX_TSSHD_SUPERVISOR=%s\n' "$supervisor"
        printf 'ORLIX_TSSHD_PID=%s\n' "$pid"
        count=0
        while [ "$count" -lt 300 ]; do
          if grep -q '}' "$output" 2>/dev/null; then
            cat "$output"
            ready=1
            exit 0
          fi
          if ! kill -0 "$pid" 2>/dev/null; then
            cat "$output"
            exit 1
          fi
          count=$((count + 1))
          sleep 0.1
        done
        cat "$output"
        exit 1
        """
        return RemoteTerminalBootstrap.wrapPOSIXShellCommand(script)
    }
}
