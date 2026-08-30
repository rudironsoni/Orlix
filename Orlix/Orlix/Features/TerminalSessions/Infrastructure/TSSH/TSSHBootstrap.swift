import Foundation

nonisolated struct TSSHBootstrapResult: Sendable {
    let host: String
    let info: TSSHServerInfo
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

        let command = startCommand(profile: server.tsshProfile)
        let output: String
        do {
            output = try await client.execute(
                command,
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
        let info = try TSSHServerInfo.parse(output: output).assigningClientIDIfNeeded()
        let host = await client.remoteEndpointHost() ?? server.host
        return TSSHBootstrapResult(host: host, info: info)
    }

    static func supports(environment: RemoteEnvironment) -> Bool {
        environment.platform != .windows && environment.shellProfile.family == .posix
    }

    static func startCommand(profile: TSSHProfile, nonce: UUID = UUID()) -> String {
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
        let quotedArguments = arguments.map(RemoteTerminalBootstrap.shellQuoted).joined(separator: " ")
        let script = """
        export PATH="\(pathEntries.joined(separator: ":")):$PATH"
        umask 077
        binary=\(RemoteTerminalBootstrap.shellQuoted(binary))
        output=\(RemoteTerminalBootstrap.shellQuoted(outputPath))
        trap 'rm -f "$output"' EXIT HUP INT TERM
        if ! { [ -x "$binary" ] || command -v "$binary" >/dev/null 2>&1; }; then
          printf '%s\\n' TSSHD_NOT_FOUND
          exit 127
        fi
        : > "$output"
        nohup "$binary" \(quotedArguments) >"$output" 2>&1 </dev/null &
        pid=$!
        count=0
        while [ "$count" -lt 300 ]; do
          if grep -q '}' "$output" 2>/dev/null; then
            cat "$output"
            rm -f "$output"
            exit 0
          fi
          if ! kill -0 "$pid" 2>/dev/null; then
            cat "$output"
            rm -f "$output"
            exit 1
          fi
          count=$((count + 1))
          sleep 0.1
        done
        cat "$output"
        rm -f "$output"
        exit 1
        """
        return RemoteTerminalBootstrap.wrapPOSIXShellCommand(script)
    }
}
