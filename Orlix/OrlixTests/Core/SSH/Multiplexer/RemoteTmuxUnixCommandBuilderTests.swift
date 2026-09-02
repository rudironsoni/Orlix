import Foundation
import Testing
@testable import Orlix

struct RemoteTmuxUnixCommandBuilderTests {
    @Test
    func invalidSyncedThemeNameFallsBackAtFeatureBoundary() {
        let theme = TerminalRemoteSessionLiveComposition.themeStyle(
            for: "safe\n'@\nrun-shell attacker"
        )

        #expect(theme.name == "Orlix Dark")
        #expect(!theme.modeStyle.contains("run-shell"))
    }
    @Test
    func attachExistingCommandFallsBackToLoginShell() {
        let command = RemoteTmuxCommandBuilder.attachExistingCommand(
            themeStyle: deterministicRemoteSessionThemeStyle,
            sessionName: "team session",
            ownership: .external
        )
        #expect(command.contains("'tmux' has-session"))
        #expect(command.contains("attach-session"))
        #expect(command.contains("exec \"${SHELL:-/bin/sh}\" -l"))
    }

    @Test
    func managedLifecycleCommandReportsDetachOrSessionEnd() {
        let command = RemoteTmuxCommandBuilder.attachCommand(
            themeStyle: deterministicRemoteSessionThemeStyle,
            sessionName: "orlix_managed",
            workingDirectory: "/work",
            lifecycleEnvelope: deterministicRemoteSessionLifecycleEnvelope
        )

        #expect(command.contains("new-session -d -s"))
        #expect(command.contains("has-session -t '=orlix_managed'"))
        #expect(command.contains(RemoteSessionLifecycleMarker.sequence(
            envelope: deterministicRemoteSessionLifecycleEnvelope,
            event: .detached
        )))
        #expect(command.contains(RemoteSessionLifecycleMarker.sequence(
            envelope: deterministicRemoteSessionLifecycleEnvelope,
            event: .terminated
        )))
        #expect(command.contains(RemoteSessionLifecycleMarker.sequence(
            envelope: deterministicRemoteSessionLifecycleEnvelope,
            event: .creationFailed
        )))
        #expect(command.contains("orlixTmuxCreateStatus=$?"))
        #expect(!command.contains("exec tmux"))
    }

    @Test
    func tsshManagedLifecycleUsesDirectTmuxSessionCreation() {
        let command = RemoteTmuxCommandBuilder.attachCommand(
            themeStyle: deterministicRemoteSessionThemeStyle,
            sessionName: "orlix_managed",
            workingDirectory: "/work",
            lifecycleEnvelope: deterministicRemoteSessionLifecycleEnvelope,
            transport: .tssh
        )

        #expect(command.contains("new-session -d -s 'orlix_managed' -c '/work'"))
        #expect(command.contains("set-option -q -t '=orlix_managed:' @orlix-managed 1"))
        #expect(command.contains("show-options -v -q -t '=orlix_managed:' @orlix-managed"))
        #expect(command.contains("set-option -q -t '=orlix_managed:' status off"))
        #expect(command.contains("set-environment -t '=orlix_managed' TERM 'xterm-kitty'"))
        #expect(command.contains("set-hook -t '=orlix_managed:' 'after-new-window[1000]'"))
        #expect(command.contains("attach-session -t '=orlix_managed'"))
        #expect(command.contains(RemoteSessionLifecycleMarker.sequence(
            envelope: deterministicRemoteSessionLifecycleEnvelope,
            event: .attached
        )))
        #expect(command.contains(RemoteSessionLifecycleMarker.sequence(
            envelope: deterministicRemoteSessionLifecycleEnvelope,
            event: .creationFailed
        )))
        #expect(!command.contains("__orlix_bootstrap__"))
        #expect(!command.contains("new-window"))
    }

    @Test
    func tsshManagedReattachDoesNotCreateMissingSession() {
        let command = RemoteTmuxCommandBuilder.attachExistingCommand(
            themeStyle: deterministicRemoteSessionThemeStyle,
            sessionName: "orlix_managed",
            ownership: .managed,
            lifecycleEnvelope: deterministicRemoteSessionLifecycleEnvelope,
            transport: .tssh
        )

        #expect(command.contains("has-session -t '=orlix_managed'"))
        #expect(command.contains("show-options -v -q -t '=orlix_managed:' @orlix-managed"))
        #expect(command.contains("set-option -q -t '=orlix_managed:' status off"))
        #expect(command.contains("set-hook -t '=orlix_managed:' 'after-new-window[1000]'"))
        #expect(command.contains("attach-session -t '=orlix_managed'"))
        #expect(command.contains(RemoteSessionLifecycleMarker.sequence(
            envelope: deterministicRemoteSessionLifecycleEnvelope,
            event: .terminated
        )))
        #expect(!command.contains("new-session"))
    }

    @Test(arguments: [
        "/tmp/$(touch /tmp/orlix-injected)",
        "/tmp/`touch /tmp/orlix-injected`",
        "/tmp/$HOME/project",
        "/tmp/quote'and\"backslash\\",
        "/tmp/line\nbreak",
        "/tmp/ユニコード",
        "-leading-option"
    ])
    func unixWorkingDirectoryIsOneOpaqueShellArgument(_ workingDirectory: String) {
        let command = RemoteTmuxCommandBuilder.attachCommand(
            themeStyle: deterministicRemoteSessionThemeStyle,
            sessionName: "orlix_secure",
            workingDirectory: workingDirectory
        )
        let quoted = RemoteTerminalBootstrap.shellQuoted(workingDirectory)

        #expect(command.contains("-c \(quoted)"))
        #expect(!command.contains("-c \"\(workingDirectory)\""))
    }

    @Test
    func unixHomeWorkingDirectoryUsesQuotedExpansion() {
        let command = RemoteTmuxCommandBuilder.attachCommand(
            themeStyle: deterministicRemoteSessionThemeStyle,
            sessionName: "orlix_secure",
            workingDirectory: "~"
        )

        #expect(command.contains("-c \"${HOME}\""))
        #expect(!command.contains("-c $HOME"))
    }

    @Test
    func unixSessionPresenceProbeUsesExactSessionAndPrivateMarkers() {
        let command = RemoteTmuxCommandBuilder.sessionPresenceProbeCommand(
            sessionName: "orlix_managed",
            backend: .unixTmux,
            existsMarker: "private-exists",
            missingMarker: "private-missing"
        )

        #expect(command.contains("has-session -t"))
        #expect(command.contains("=orlix_managed"))
        #expect(command.contains("private-exists"))
        #expect(command.contains("private-missing"))
    }

    @Test
    func managedReattachDoesNotRecreateMissingSession() {
        let command = RemoteTmuxCommandBuilder.attachExistingCommand(
            themeStyle: deterministicRemoteSessionThemeStyle,
            sessionName: "orlix_managed",
            ownership: .managed,
            lifecycleEnvelope: deterministicRemoteSessionLifecycleEnvelope
        )

        #expect(command.contains("attach-session"))
        #expect(command.contains("set-option -wq -t \"$orlixWindow\" scroll-on-clear 'off'"))
        #expect(command.contains(RemoteSessionLifecycleMarker.sequence(
            envelope: deterministicRemoteSessionLifecycleEnvelope,
            event: .terminated
        )))
        #expect(!command.contains(RemoteSessionLifecycleMarker.sequence(
            envelope: deterministicRemoteSessionLifecycleEnvelope,
            event: .creationFailed
        )))
        #expect(!command.contains("new-session"))
        #expect(!command.contains("exec \"${SHELL:-/bin/sh}\" -l"))
    }

    @Test
    func installAndAttachScriptIncludesScopedManagedConfiguration() {
        let script = RemoteTmuxCommandBuilder.installAndAttachScript(
            themeStyle: deterministicRemoteSessionThemeStyle,
            sessionName: "orlix_demo",
            workingDirectory: "/tmp/work dir",
            terminalType: .xtermGhostty
        )
        #expect(script.contains("new-session -d -s"))
        #expect(script.contains("orlix_demo"))
        #expect(script.contains("/tmp/work dir"))
        #expect(script.contains("set-option -q -t"))
        #expect(script.contains("status off"))
        #expect(script.contains("RGB,hyperlinks"))
        #expect(!script.contains("~/.orlix/tmux.conf"))
        #expect(!script.contains("set -g"))
    }

    @Test
    func installOnlyScriptDoesNotEnterUntrackedTmuxSession() {
        let script = RemoteTmuxCommandBuilder.installAndAttachScript(
            themeStyle: deterministicRemoteSessionThemeStyle,
            sessionName: "orlix_demo",
            workingDirectory: "/tmp/work",
            terminalType: .xtermGhostty,
            attachAfterInstall: false
        )

        #expect(script.contains("apt-get install -y tmux"))
        #expect(!script.contains("new-session"))
        #expect(!script.contains("attach-session"))
        #expect(!script.contains("exec tmux"))
        #expect(!script.contains("~/.orlix/tmux.conf"))
    }

    @Test
    func managedSessionClearBehaviorIsWindowScoped() {
        let create = RemoteTmuxCommandBuilder.attachCommand(
            themeStyle: deterministicRemoteSessionThemeStyle,
            sessionName: "orlix_managed",
            workingDirectory: "/tmp"
        )
        let reattach = RemoteTmuxCommandBuilder.attachExistingCommand(
            themeStyle: deterministicRemoteSessionThemeStyle,
            sessionName: "orlix_managed",
            ownership: .managed
        )
        let external = RemoteTmuxCommandBuilder.attachExistingCommand(
            themeStyle: deterministicRemoteSessionThemeStyle,
            sessionName: "shared",
            ownership: .external
        )

        let scopedOption = "set-option -wq -t \"$orlixWindow\" scroll-on-clear 'off'"
        #expect(create.contains(scopedOption))
        #expect(reattach.contains(scopedOption))
        #expect(!external.contains("scroll-on-clear"))
        #expect(!reattach.contains("source-file"))
        #expect(!reattach.contains("~/.orlix/tmux.conf"))
    }

    @Test
    func managedUnixSessionConfigurationIsScopedToItsSession() {
        let create = RemoteTmuxCommandBuilder.attachCommand(
            themeStyle: deterministicRemoteSessionThemeStyle,
            sessionName: "orlix_managed",
            workingDirectory: "/tmp"
        )
        let reattach = RemoteTmuxCommandBuilder.attachExistingCommand(
            themeStyle: deterministicRemoteSessionThemeStyle,
            sessionName: "orlix_managed",
            ownership: .managed
        )
        let external = RemoteTmuxCommandBuilder.attachExistingCommand(
            themeStyle: deterministicRemoteSessionThemeStyle,
            sessionName: "shared",
            ownership: .external
        )

        for command in [create, reattach] {
            #expect(command.contains("set-option -q -t '=orlix_managed:' status off"))
            #expect(command.contains(
                "list-windows -t '=orlix_managed:' -F '#{window_linked}'"
            ))
            #expect(command.contains("set-option -q -t '=orlix_managed:' history-limit 10000"))
            #expect(command.contains("set-option -q -t '=orlix_managed:' mouse on"))
            #expect(command.contains("set-environment -t '=orlix_managed' TERM_PROGRAM 'ghostty'"))
            #expect(command.contains("-F '#{window_id} #{window_linked}'"))
            #expect(command.contains("[ \"$orlixLinked\" = 0 ] || continue"))
            #expect(command.contains("set-hook -t '=orlix_managed:' 'after-new-window[1000]'"))
            #expect(command.contains("#{==:#{window_linked},0}"))
            #expect(!command.contains("source-file"))
            #expect(!command.contains("-f ~/.orlix/tmux.conf"))
            #expect(!command.contains("set -g"))
            #expect(!command.contains("bind -n"))
        }

        #expect(!external.contains("set-option"))
        #expect(!external.contains("set-environment"))
        #expect(!external.contains("source-file"))
        #expect(!external.contains("~/.orlix/tmux.conf"))
    }

    @Test
    func managedETSessionPropagatesSnacksHintWithoutFabricatingSSHEnvironment() throws {
        let create = RemoteTmuxCommandBuilder.attachCommand(
            themeStyle: deterministicRemoteSessionThemeStyle,
            sessionName: "orlix_et",
            workingDirectory: "/tmp",
            lifecycleEnvelope: deterministicRemoteSessionLifecycleEnvelope,
            transport: .eternalTerminal
        )
        let reattach = RemoteTmuxCommandBuilder.attachExistingCommand(
            themeStyle: deterministicRemoteSessionThemeStyle,
            sessionName: "orlix_et",
            ownership: .managed,
            lifecycleEnvelope: deterministicRemoteSessionLifecycleEnvelope,
            transport: .eternalTerminal
        )
        let external = RemoteTmuxCommandBuilder.attachExistingCommand(
            themeStyle: deterministicRemoteSessionThemeStyle,
            sessionName: "shared",
            ownership: .external,
            lifecycleEnvelope: deterministicRemoteSessionLifecycleEnvelope,
            transport: .eternalTerminal
        )

        let assignment = "set-environment -t '=orlix_et' SNACKS_SSH '1'"
        #expect(create.contains(assignment))
        #expect(reattach.contains(assignment))
        #expect(!external.contains("SNACKS_SSH"))
        for command in [create, reattach, external] {
            #expect(!command.contains("SSH_CONNECTION"))
            #expect(!command.contains("SSH_CLIENT"))
            #expect(!command.contains("SSH_TTY"))
        }

        let hint = try #require(create.range(of: assignment))
        let terminalWindow = try #require(create.range(of: "new-window -d"))
        #expect(hint.lowerBound < terminalWindow.lowerBound)
    }

    @Test
    func managedSSHAndMoshSessionsClearStaleETCompatibilityHint() {
        for transport in [ShellTransport.ssh, .sshFallback, .mosh] {
            let command = RemoteTmuxCommandBuilder.attachCommand(
                themeStyle: deterministicRemoteSessionThemeStyle,
                sessionName: "orlix_transport",
                workingDirectory: "/tmp",
                transport: transport
            )
            #expect(command.contains("set-environment -u -t '=orlix_transport' SNACKS_SSH"))
            #expect(!command.contains("SNACKS_SSH '1'"))
            #expect(!command.contains("SSH_CONNECTION"))
            if transport == .mosh {
                #expect(command.contains("set-environment -u -t '=orlix_transport' TERM_PROGRAM"))
                #expect(command.contains("set-environment -u -t '=orlix_transport' TERM_PROGRAM_VERSION"))
                #expect(!command.contains("TERM_PROGRAM 'ghostty'"))
            }
        }
    }

    @Test
    func managedUnixCreationBootstrapsLegacyTmuxBeforeStartingTerminalShell() {
        let command = RemoteTmuxCommandBuilder.attachCommand(
            themeStyle: deterministicRemoteSessionThemeStyle,
            sessionName: "orlix_managed",
            workingDirectory: "/tmp"
        )

        #expect(command.contains("'tmux' -u -T RGB,hyperlinks -V"))
        #expect(command.contains("'tmux' -u -T RGB,hyperlinks attach-session"))
        #expect(command.contains("else exec 'tmux' -u attach-session"))
        #expect(command.components(separatedBy: "new-session -d -s").count == 2)
        #expect(!command.contains("-e 'COLORTERM=truecolor'"))
        #expect(command.contains("__orlix_bootstrap__"))
        #expect(command.contains("new-window -d -t '=orlix_managed:'"))
        #expect(command.contains("new-window -d -t '=orlix_managed:' -c '/tmp' /bin/sh -lc"))
        #expect(command.contains("if [ -n \\\"\\$SHELL\\\" ]; then exec \\\"\\$SHELL\\\" -l; fi;"))
        #expect(command.contains("kill-window -t '=orlix_managed:__orlix_bootstrap__'"))
        #expect(command.contains("move-window -r -t '=orlix_managed:'"))
        #expect(command.contains("set-option -q -t '=orlix_managed:' @orlix-managed 1"))

        let createWindowOffset = command.range(of: "new-window -d -t '=orlix_managed:'")
            .map { command.distance(from: command.startIndex, to: $0.lowerBound) }
        let removeBootstrapOffset = command.range(of: "kill-window -t '=orlix_managed:__orlix_bootstrap__'")
            .map { command.distance(from: command.startIndex, to: $0.lowerBound) }
        #expect((createWindowOffset ?? .max) < (removeBootstrapOffset ?? .min))
    }

    @Test
    func externalUnixSessionAttachDoesNotLoadOrlixConfiguration() {
        let command = RemoteTmuxCommandBuilder.attachExistingCommand(
            themeStyle: deterministicRemoteSessionThemeStyle,
            sessionName: "external's; $session",
            ownership: .external
        )
        let exactSession = "'=external'\\''s; $session'"

        #expect(command.contains("has-session -t \(exactSession)"))
        #expect(command.contains("attach-session -t \(exactSession)"))
        #expect(!command.contains("source-file"))
        #expect(!command.contains("~/.orlix/tmux.conf"))
    }

    @Test
    func listingRequestsTheExplicitManagedOwnershipOption() {
        let backend = RemoteTmuxBackend.unixTmux(
            executablePath: "/opt/tools/tmux",
            rawVersion: "tmux 3.7b"
        )
        let commands = RemoteTmuxCommandBuilder.listSessionCommands(backend: backend)

        #expect(commands[0].contains("#{@orlix-managed}"))
        #expect(commands[0].contains("/opt/tools/tmux"))
    }

}
