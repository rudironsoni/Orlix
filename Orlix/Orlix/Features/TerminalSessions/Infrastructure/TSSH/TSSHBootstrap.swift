import Foundation

nonisolated struct TSSHBootstrapResult: Sendable {
    let host: String
    let info: TSSHServerInfo
    let serverProcess: TSSHServerProcessIdentity
}

nonisolated struct TSSHServerProcessIdentity: Sendable {
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

        let launch = launchCommand(profile: server.tsshProfile)
        let output: String
        do {
            output = try await client.execute(
                launch.command,
                timeout: .seconds(45),
                maxOutputBytes: 64 * 1024
            )
        } catch {
            let description = error.localizedDescription
            if description.contains("not found") || description.contains("TSSHD_NOT_FOUND") {
                throw TSSHRuntimeError.tsshdNotFound
            }
            throw TSSHRuntimeError.bootstrapFailed(description)
        }
        if output.contains("TSSHD_NOT_FOUND") {
            throw TSSHRuntimeError.tsshdNotFound
        }
        let serverProcess = TSSHServerProcessIdentity(
            pid: try parseServerPID(output: output),
            supervisorPath: launch.supervisorPath
        )
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

    static func terminateServer(
        _ identity: TSSHServerProcessIdentity,
        using client: SSHClient
    ) async {
        let command = terminationCommand(for: identity)
        await Task.detached {
            _ = try? await client.execute(
                command,
                timeout: .seconds(5),
                maxOutputBytes: 4 * 1024
            )
        }.value
    }

    static func terminationCommand(for identity: TSSHServerProcessIdentity) -> String {
        let body = """
        pid=\(identity.pid)
        supervisor=\(RemoteTerminalBootstrap.shellQuoted(identity.supervisorPath))
        command=$(ps -p "$pid" -o args= 2>/dev/null || true)
        case "$command" in
          *"$supervisor"*) kill -TERM "$pid" 2>/dev/null || true ;;
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
        launchCommand(profile: profile, nonce: nonce).command
    }

    private static func launchCommand(
        profile: TSSHProfile,
        nonce: UUID = UUID()
    ) -> (command: String, supervisorPath: String) {
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

        let outputPath = "/tmp/orlix-tsshd-\(nonce.uuidString).out"
        let supervisorPath = "/tmp/orlix-tsshd-\(nonce.uuidString).sh"
        let quotedArguments = arguments.map(RemoteTerminalBootstrap.shellQuoted).joined(separator: " ")
        let script = """
        export PATH="\(pathEntries.joined(separator: ":")):$PATH"
        umask 077
        binary=\(RemoteTerminalBootstrap.shellQuoted(binary))
        output=\(RemoteTerminalBootstrap.shellQuoted(outputPath))
        supervisor=\(RemoteTerminalBootstrap.shellQuoted(supervisorPath))
        pid=
        ready=0
        cleanup() {
          if [ "$ready" -ne 1 ] && [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
            kill -TERM "$pid" 2>/dev/null || true
            wait "$pid" 2>/dev/null || true
          fi
          rm -f "$output"
          if [ "$ready" -ne 1 ]; then rm -f "$supervisor"; fi
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
          rm -f "$0"
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
        return (
            RemoteTerminalBootstrap.wrapPOSIXShellCommand(script),
            supervisorPath
        )
    }
}
