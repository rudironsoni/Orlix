# Orlix Terminal, Herdr, Local Runtime, and Docker Platform

## 1. Summary and release contract

Import pinned VVTerm directly as the Orlix iOS and iPadOS application, preserving its complete application and UX foundation while keeping OrlixOS as the delivered Linux Kit and upstream Linux as the runtime authority. Do not create an intermediate `OrlixTerminal` framework or rebuild VVTerm around the current UIKit terminal.

The work ships mobile first, followed by macOS:

1. **iOS and iPadOS Terminal and Local Runtime release**
   - The complete pinned VVTerm application compiled directly as Orlix on iOS and iPadOS.
   - Full SSH, Mosh, TSSH/tsshd, remote-file, forwarding, VPN, cloud-discovery, authentication, and Apple system-integration surface.
   - Herdr as the authoritative, first-class terminal multiplexer.
   - Complete local Orlix Linux runtime on iOS and iPadOS.
   - Local Instances, local terminals, and Herdr inside the Orlix distribution.

2. **iOS and iPadOS Container and Docker release**
   - Complete container-management UI inspired visually by Contained and structurally by Orchard.
   - Full Linux Docker Engine 29.6.1 API and behavioral compatibility, including Compose, BuildKit, Buildx, Swarm, services, configs, secrets, contexts, plugins, networks, volumes, builds, events, stats, exec, and version negotiation.
   - Windows Containers are the sole deliberate Docker Engine exclusion.
   - Linux platform selection and multi-platform metadata remain supported. A foreign-architecture Linux workload follows normal Docker behavior and requires a registered Linux binary-format handler or emulator; Orlix does not silently emulate unsupported instruction sets.

3. **Native macOS release**
   - Starts only after both mobile releases are in good shape and published to the App Store.
   - Delivers the complete terminal, remote transport, Herdr, Local Runtime, container, and Docker product on native Apple-silicon macOS.
   - Includes the macOS OrlixKernel and OrlixOS slice, user-scoped runtime service, embedded helpers, native Herdr CLI, external-shell access, and Docker contexts.
   - Has no public remote-only phase.

Each public release is App Store-only. Missing licensing approval, required entitlements, executable-content approval, helper validation, or App Review acceptance blocks the affected public release. Features are never advertised when unavailable.

Minimum deployment targets are:

- iOS and iPadOS 16.1.
- macOS 13.3, Apple silicon only.
- Later OS, entitlement, accessory, and hardware capabilities are feature-gated.
- visionOS is outside the current product scope.

## 2. Domain model, ownership, and interfaces

### Canonical language and authority

| Concept | Definition and authority |
|---|---|
| **Remote Host** | A remotely reachable server or provider resource. Orlix owns its metadata. |
| **Connection Profile** | Authentication, user, port, transport, jump chain, forwarding, VPN, and policy choices for connecting to a Remote Host. |
| **Terminal Target** | A typed destination: Remote Host, Local Instance, or Container. |
| **Local Runtime** | The single running Orlix Linux runtime containing one upstream OrlixKernel. |
| **Local Instance** | A persistent, namespaced Linux userspace system sharing the Local Runtime. It is not a VM or kernel. |
| **Container** | An OCI application workload assigned to exactly one Local Instance. It cannot exist without that instance. |
| **Herdr Session** | A persistent Herdr server namespace. Named Sessions have separate panes, sockets, and persisted state. |
| **Herdr Workspace** | The top-level project container, normally one per repo, task, or investigation. |
| **Herdr Tab** | A layout inside a Workspace. |
| **Herdr Pane** | A real terminal owned by the Herdr server. |
| **Terminal Workspace** | Orlix's native projection of one Herdr Workspace, never a competing topology model. |

Herdr is authoritative for Workspaces, Tabs, split trees, Pane identities, focus, agent state, and live terminal ownership. Orlix is a peer client alongside Herdr's raw TUI.

App-created and user-created Herdr resources coexist as follows:

- Orlix attaches to the default Herdr Session and creates the migrated initial Workspace there.
- Orlix does not create a hidden named Session such as `orlix`.
- App-created Workspaces, Tabs, and Panes are ordinary Herdr resources. Users may rename, move, split, attach, or close them.
- Named Sessions remain separate namespaces. Orlix lists and attaches them without merging them into the default Session.
- Workspaces are used for organization first. Named Sessions are used only when the user wants entirely separate panes, sockets, and persisted runtime state.
- Native UI and raw TUI may attach simultaneously. Controller ownership and takeover use Herdr's public terminal-session rules.
- Herdr mouse behavior, prefix bindings, agents, worktrees, integrations, plugins, marketplace behavior, persistence, CLI, socket API, raw TUI, and remote attach remain first-class.

### Code ownership

- **OrlixKernel and upstream Linux** own tasks, PTYs, process groups, signals, wait/reaping, namespaces, VFS, sockets, networking, cgroups, mounts, devices, and container semantics.
- **OrlixMLibC** owns libc and syscall support needed by Herdr, Docker-compatible userspace services, and packages.
- **OrlixOS** remains the Kit. It owns the Local Runtime and Local Instance lifecycle API, distribution policy, rootfs and package assembly, OCI control-plane integration, and app-facing Linux sessions.
- **OrlixHostAdapter** owns only private Apple and Darwin mechanics. It does not acquire Linux lifecycle, Docker, Herdr, or container policy.
- The **Orlix iOS and iPadOS app target** compiles the imported VVTerm application source directly. VVTerm's SwiftUI application root, feature-first organization, Ghostty rendering, catalogs, remote transports, CloudKit, Keychain, StoreKit, and platform integrations remain the product foundation.
- Do not create a reusable `OrlixTerminal` module or preserve the current UIKit lifecycle as the composition root. The production target has exactly one SwiftUI `@main`, supplied by the Orlix application fork.
- Isolate `TerminalViewController` in a separate developer-only diagnostic app target because it uses `libghostty-spm` while VVTerm vendors its own Ghostty build. Do not compile or link the controller into production, expose it in product navigation, or use it as a foundation for new work.
- Preserve VVTerm's macOS-compatible source, resources, packages, and conditional compilation during mobile work, but do not create or implement the Mac target until the published mobile releases unblock it.

### Direct import and upstream maintenance

- Import the pinned repository at `Orlix/App` using a non-squashed, history-preserving Git subtree. Record `https://github.com/vivy-company/vvterm` as the upstream remote and retain the upstream commit in merge history and provenance.
- Keep Orlix-specific changes as narrow commits on top of the imported tree. Do not reformat or reorganize upstream files without a product requirement.
- Update by fetching an explicitly reviewed immutable upstream commit and running the documented subtree pull or merge command. Never use a branch name alone as a release input.
- After every update, audit `project.yml` against upstream changes to sources, packages, resources, entitlements, privacy manifests, tests, UI tests, extensions, and targets. A newly added upstream input must be translated or explicitly recorded as unavailable with a reviewed reason.

### Dependency, identity, and compliance translation

- Provenance enumerates immutable commits for `mlx-swift`, `swift-cloudflared`, `swift-mosh`, `swift-numerics`, `swift-umami`, TweetNaCl, and ZIPFoundation, plus the exact vendored Ghostty, libssh2, and OpenSSL revisions. Record artifact hashes, rebuild commands, licenses, and notices for every vendored binary or generated library.
- Replace mutable dependency declarations before release. This includes Ghostty build scripts that default to mutable `custom-io` and any package declaration such as `swift-umami` on `main`, even when `Package.resolved` currently pins a commit.
- Maintain an explicit stale-identity map and fail the release scan if VVTerm-owned values remain in app, unit-test, UI-test, or Live Activity bundle IDs; CloudKit containers or record/storage prefixes; Keychain groups; Cloudflare URL schemes or constants; widget kinds; StoreKit products or team data; product names; usage strings; APNs environments; or network entitlements.
- Record provisioning-profile and CloudKit production-schema evidence for the translated Orlix identifiers before release.
- Merge the Orlix and VVTerm privacy manifests as a schema-valid union. Preserve every required API reason and collected-data declaration unless evidence proves it inapplicable.
- Document the encryption export classification and verify the exported app's `Info.plist` against its actual SSH, Mosh, libssh2, and OpenSSL behavior. A pre-existing false encryption declaration is not accepted as evidence.

### App-facing types

Introduce the following stable application contracts:

- `TerminalTarget`, with cases for `remoteHost(hostID, profileID)`, `localInstance(instanceID)`, and `container(instanceID, containerID)`.
- `RemoteHost` and `ConnectionProfile`, with credentials represented only by Keychain identity references.
- `LocalRuntimeDescriptor`, `LocalInstanceDescriptor`, `LocalInstanceState`, and typed lifecycle requests in OrlixOS.
- Opaque `HerdrSessionID`, `HerdrWorkspaceID`, `HerdrTabID`, and `HerdrPaneID` values matching Herdr's public identifiers.
- `ExternalPaneBackend`, the bridge between a Herdr Pane and an app-native SSH, Mosh, or TSSH transport.
- Existing `OrlixEnvironment` records remain readable during migration. Each persistent named environment becomes a stopped Local Instance or instance template, while the current default root becomes the Default Local Instance. Existing OCI records are assigned to their owning migrated instance.

### Herdr socket integration and external-pane protocol

Use Herdr's existing newline-delimited JSON socket API:

1. Call `session.snapshot` after initial connection and every reconnect.
2. Record its protocol version and snapshot revision.
3. Subscribe to Workspace, Tab, Pane, layout, agent, scroll, and lifecycle events.
4. Apply events only in monotonically increasing revision order.
5. Resnapshot on a revision gap, schema mismatch, reconnect, or stale-cache detection.
6. Use `terminal session control` for the writable native client and `terminal session observe` for read-only clients and diagnostics.

Add a commercially licensed upstream Herdr extension for externally managed Pane backends. Do not implement a parallel Orlix topology:

- `pane.external.create` creates or splits a Pane whose terminal backend is externally supplied.
- `terminal.backend.open` binds a backend owner to that Pane and begins a bidirectional NDJSON stream.
- The handshake carries minimum and maximum extension versions, the Herdr protocol version, a Pane ID, a backend lease epoch, and a resumable endpoint token.
- Terminal bytes are base64 encoded. Decoded payloads are limited to 64 KiB per frame.
- Each direction uses monotonic sequence numbers and acknowledgements. The initial unacknowledged window is 256 KiB and is replenished only through acknowledgements, providing bounded backpressure.
- Herdr-to-backend input and resize messages share one command sequence so a resize cannot overtake earlier input. Resizes include rows, columns, and optional pixel dimensions.
- Backend-to-Herdr output has its own ordered sequence. Duplicate acknowledged frames are ignored; a gap pauses consumption and requests replay.
- One backend lease owns a Pane. Takeover increments the lease epoch and invalidates all messages from the previous owner.
- Reconnect requires the stable endpoint token and last acknowledged sequences. Herdr preserves its terminal model and scrollback while the real transport performs protocol-specific reattachment.
- Explicit states are `connecting`, `ready`, `running`, `reconnecting`, `closed`, and `error`. Closed and error records include sanitized reason codes, never credentials, tokens, private keys, or terminal input.
- Raw Herdr TUI compatibility is mandatory. An external Pane must behave like any PTY-backed Pane to raw clients, native clients, plugins, agents, and socket consumers.
- If the Herdr maintainer does not approve the extension or the commercial agreement does not cover it, the first public release is blocked. Orlix must not replace it with a Swift-owned split or session model.

### Remote transport and security architecture

Replace VVTerm's libssh2 execution path after parity with a narrow XCFramework derived from pinned `trzsz-ssh` for standard SSH and TSSH. Keep protocol work upstream-shaped:

- Direct and jump-host SSH, PTY and exec requests, local, remote, and dynamic forwarding, SFTP, SCP, SSH configuration semantics, agent forwarding, and TSSH share one audited SSH core.
- TSSH QUIC and KCP remain upstream `trzsz-ssh` and `tsshd` behavior. Do not independently reimplement these protocols.
- Mosh uses VVTerm's existing Swift integration, validated against upstream Mosh 1.4.0 servers and protocol behavior.
- A Network.framework dialer is used for every app-native connection so Tailscale, NetBird, path changes, Wi-Fi/cellular handover, and VPN routing receive Apple path evaluation.
- Hardware signers are callbacks into an Apple identity broker, so private Secure Enclave, YubiKey PIV, and FIDO2 material never enters the transport library.
- Required authentication includes password, software keys, passphrases, keyboard-interactive, OpenSSH user certificates, trusted host certificate authorities, Secure Enclave, YubiKey PIV over supported NFC or wired transports, FIDO2, OpenPubkey, and in-app agent forwarding.
- OpenPubkey providers include Google, Microsoft, GitLab, Hellō, and custom OIDC issuers.
- Post-quantum negotiation covers `mlkem768x25519-sha256`, `sntrup761x25519-sha512`, and supported post-quantum host signatures. Missing upstream support requires an audited upstream contribution or narrowly maintained cryptographic extension, not protocol invention.
- TSSH detects client/server version skew, verifies `tsshd`, reports actionable diagnostics, and supports configured server paths and upgrades.
- Mosh and TSSH own real resumable transport state. TCP SSH receives only the OS-provided short background grace period.
- No location-tracking background workaround is permitted.
- VPN-over-SSH and VPN-over-TSSH use an approved Network Extension packet tunnel, with DNS, routes, exclusions, statistics, widgets, Control Center, Shortcuts, and Live Activities.
- Remote provider catalogs cover AWS, Azure, Linode, DigitalOcean, Tailscale, and NetBird. They include AWS access-key and SSO/STS accounts, EC2 and EKS discovery, EC2 serial console, Azure OAuth resources, Linode LISH, DigitalOcean resources, Tailscale OAuth, NetBird personal-access-token authentication, and self-hosted NetBird management URLs.
- Kubernetes covers multi-cluster kubeconfig import, browsing, exec/log terminals, EKS kubeconfig generation, and node debugging through normal Kubernetes APIs.
- Cloudflare Access and existing VVTerm connection modes remain supported.
- RootShell is a behavioral reference and regression oracle only. Its application source and proprietary implementation are not copied. Its unrelated AI-agent and local-tool features are outside this project.

Arbitrary external macOS `ssh-agent` and 1Password socket consumption is deferred. The first release supports Orlix-owned agent services, in-app keys, hardware identities, and app-group-safe communication. A later last-resort sprint may investigate an approved helper broker. No public UI advertises arbitrary external-agent access until it is proven in the exported Mac App Store package.

### Local Runtime and Local Instances

Implement one Local Runtime with one upstream OrlixKernel:

- Every Local Instance has its own init, root and state directories, PID, mount, UTS, IPC, network, and user namespaces, plus a cgroup v2 subtree.
- Local Instances share the kernel but never share process, mount, hostname, network, or resource-policy identity.
- A normal Linux userspace supervisor manages instance lifecycle through upstream-shaped Linux control mechanisms.
- Missing PTY, process, signal, namespace, polling, Unix-socket, process-inspection, or persistence behavior is fixed in upstream Linux port inputs or OrlixMLibC, never hidden in Swift or HostAdapter.
- The Default Local Instance starts on demand. Newly created instances are stopped by default. Autostart is explicit.
- Herdr is packaged as a real Linux arm64 program in the Orlix distribution, with its CLI, raw TUI, server, agents, plugins, worktrees, integrations, marketplace, default Session, and named Sessions.
- After the published mobile releases, macOS embeds a native Herdr runtime so `herdr`, `herdr --remote`, and local socket clients can be invoked outside the Orlix UI.
- The later Mac app adds an arm64 macOS OrlixKernel and OrlixOS slice. The Mac local terminal targets Orlix Linux, not a cosmetic host `zsh` substitute.
- Full Local Runtime means the ADR 0017 proof ladder passes through OrlixOS, including kernel dependency, kselftest, OrlixMLibC, syscall/UAPI, POSIX shell, dynamic loader, PTY, networking, persistence, and the jq, curl, and zsh package ladder.

### Container and Docker architecture

The second release extends the Local Instance model:

- Each Container belongs to one Local Instance.
- Each Local Instance exposes a normal Linux userspace Docker-compatible service backed by OrlixOS's OCI control plane.
- Do not run `dockerd`, `containerd`, or `runc` as the Orlix runtime.
- Do not parse Docker CLI commands in Swift.
- Do not move Docker behavior into OrlixKernel or HostAdapter.
- The instance-local Docker socket controls only that instance's Containers.
- The default Mac Docker context is named `orlix` and targets the Default Local Instance. Optional `orlix-<instance-slug>` contexts target additional instances.
- Docker Engine API negotiation advertises version 1.55, defaults the minimum to 1.40, and supports configured backward compatibility through 1.24, matching Docker Engine 29.6.1.
- `/version` reports Linux and arm64 plus honest Orlix engine component information.
- Windows image manifests and Windows Container creation fail with Docker-compatible unsupported-platform errors. No other Engine area is deliberately removed.
- Swarm mode, services, configs, secrets, daemon-plugin-compatible behavior, Engine events, versioned endpoints, health checks, restart policies, networks, bind mounts, volumes, builds, BuildKit sessions, Buildx, and Compose are release requirements.
- Docker `--privileged` is supported inside a Local Instance according to Linux namespace semantics. This does not grant macOS host root.
- BuildKit uses an Orlix executor and snapshot backend to run Dockerfile build steps through the Orlix OCI control plane.
- Compose uses upstream Docker Compose behavior. `up` reconciles configuration, recreates changed services, preserves unchanged services and named volumes, and retains removed services as orphans unless removal is explicitly requested.
- Compose supports start, stop, restart, down, pull, build, logs, exec, run, profiles, dependencies, health, exit-code propagation, orphan handling, and volume handling.
- Each Compose project targets an existing Local Instance or creates a dedicated Local Instance, according to an app-wide default that the project may override. The project-to-instance binding is persistent.
- Unsupported Compose fields or Engine operations fail explicitly. Nothing is silently discarded.

Container file operations resolve paths inside the selected Container root using directory-relative Linux operations and `openat2` confinement. Symlinks are allowed only when their resolved target remains inside that root. Upload defaults to conflict failure, uses a same-directory temporary file and atomic rename, and requires an explicit overwrite choice. Downloads hold an open descriptor and report if source metadata changes during transfer.

Stats come from cgroup v2 and Linux network accounting. Docker API stats and events retain their Docker-compatible wire behavior. Native UI history is a separate bounded, device-local audit log. Engine inspect responses remain protocol-compatible, including environment values. Native UI inspectors redact credential-shaped fields by default and require an explicit reveal action. Registry credentials, private keys, tokens, and secret payloads are never written to diagnostics.

Real networking is a release gate: virtio-net, per-instance network namespaces, veth pairs, bridges, DNS, outbound connectivity, service discovery, published ports, Swarm traffic, and unprivileged host-port forwarding must work. UI-only network records cannot stand in for runtime networking.

### macOS helpers and CLI integration

Use Apple's documented helper-tool packaging only for normal embedded executables:

- Build helper and CLI targets through Xcode or as signed universal external binaries.
- Embed them under `Orlix.app/Contents/MacOS` with an `Embed Helper Tools` copy phase and Code Sign On Copy.
- Set `SKIP_INSTALL=YES`, disable `CODE_SIGN_INJECT_BASE_ENTITLEMENTS`, and enable Hardened Runtime.
- App-launched helpers carry App Sandbox and sandbox-inheritance entitlements exactly as Apple documents.
- Validate every executable, identifier, architecture, signature, runtime flag, and entitlement from the exported App Store installer package.
- Use `SMAppService` for the approved user-scoped runtime service, with explicit user approval.
- Store runtime sockets in `group.com.rudironsoni.Orlix` app-group storage.
- Offer an explicit CLI setup flow using a user-selected bin directory, defaulting to `~/.local/bin`. Never modify shell startup files or overwrite unrelated commands automatically.
- Validate app-launched and external-shell invocation separately. Apple's embedded-helper article proves only the app-launched path.
- For Docker, use an existing upstream Docker CLI when available and install the `orlix` context without changing the current context. Offer pinned signed Docker, Compose, and Buildx tools only through explicit app-guided installation.
- Existing contexts and binaries are preserved. A conflicting `orlix` context requires user confirmation before replacement.

The first Docker release remains rootless:

- Host bind mounts require explicit shared-folder grants and persistent security-scoped bookmarks.
- Unshared paths fail with a "Share this folder with Orlix" action.
- Unprivileged host ports are the default.
- Host low ports, unrestricted host paths, host-root services, and arbitrary external agent sockets belong to a deferred helper or broker research sprint.
- That later sprint may proceed only through an App Store-compliant mechanism and cannot weaken or delay the rootless release.

### Persistence, sync, migration, and Pro

The update is an automatic, idempotent in-place migration:

- Preserve `com.rudironsoni.Orlix`.
- Preserve `SelectedTheme.light` and `SelectedTheme.dark`.
- Convert the current default local terminal into the initial Workspace in Herdr's default Session.
- Convert existing persistent environments and OCI records without deleting their roots or blobs.
- Do not migrate VVTerm application data, credentials, servers, workspaces, or purchases from the separate VVTerm app.
- Do not rerun interrupted commands.
- Remote Mosh and TSSH sessions may reattach only through real protocol state.
- If local processes no longer exist after app termination, restore labels, layout, cwd hints, environment, theme, and target binding, then show an explicit restart action.

Use `iCloud.com.rudironsoni.Orlix` for Orlix CloudKit data:

- Sync Remote Hosts, Connection Profiles, themes, presets, preferences, provider metadata, declarative Local Instance templates, and declarative Terminal Workspace copies.
- Do not sync instance roots, OCI blobs, Container state, runtime IDs, live processes, PTYs, scrollback, Pane IDs, live agents, or Herdr Session state.
- Merge independent field edits, retain tombstones, and create explicit conflict copies for incompatible concurrent edits.
- Eligible passwords and software keys use iCloud Keychain by default, with a device-only opt-out.
- Secure Enclave, YubiKey, FIDO2, and external-agent identities sync references and metadata only.

Preserve VVTerm's temporary universal Pro tier:

- Monthly: `com.rudironsoni.Orlix.pro.monthly`, USD 6.49.
- Yearly: `com.rudironsoni.Orlix.pro.yearly`, USD 24.99.
- Lifetime: `com.rudironsoni.Orlix.pro.lifetime`, USD 49.99.
- Free limits: one Workspace, one Remote Host, one Tab, no splits, one Local Instance, and one Container.
- Pro removes those quantity limits.
- Images, registries, networks, volumes, and supporting resources receive no speculative limits.

## 3. Implementation sequence

1. **Documentation, legal, and provenance**
   - Create `docs/plans/active/orlix-terminal-infrastructure-platform/PLAN.md` and `IMPLEMENT.md`.
   - Update the canonical glossary with all Remote Host, Connection Profile, Terminal Target, Herdr, Local Runtime, Local Instance, and Container terms.
   - Add ADRs for the VVTerm fork and GPL gate, Herdr authority and external Pane backend, one-kernel Local Instances, cross-platform App Store helper model, and full Docker Engine compatibility without `dockerd` or `runc`.
   - Reconcile the executable-content policy in ADR 0023. Downloadable Herdr marketplace content, Linux packages, images, and plugins remain blocked from public distribution until legal and App Review approval explicitly permits them.
   - Add third-party notices, corresponding-source commitments, immutable revision pins, modification notices, and retrieval dates.
   - Preserve the current TCTI active plan and proof ordering. This project references that lane as a dependency and does not redirect it.

2. **#50, direct full VVTerm mobile application foundation**
   - Establish a pristine build and test baseline for the pinned VVTerm revision before import.
   - Import the pinned VVTerm repository with preserved history and provenance, and compile its application source directly as the Orlix iOS and iPadOS app.
   - Preserve VVTerm's recognizable `App`, `Core`, `Features`, `GhosttyTerminal`, `Compatibility`, `Generated`, and `Resources` organization. Do not introduce a generic app framework or reusable terminal module.
   - Retain and validate the complete imported VVTerm mobile feature surface through a parity ledger: terminal, tabs and splits, remote files, hosts and Workspaces, discovery, themes, keyboard accessories, presets, stats, voice input, security, sync, StoreKit, onboarding, privacy, and Live Activities.
   - Translate the mobile app, Live Activity, packages, vendor libraries, resources, entitlements, unit tests, and UI tests into the XcodeGen source of truth.
   - Preserve Orlix telemetry controls, beta observability, simulator hooks, release wiring, and applicable launch arguments. Do not preserve the UIKit `AppDelegate` or `SceneDelegate` lifecycle as the production composition root.
   - Isolate `TerminalViewController` and `libghostty-spm` in a separate developer-only diagnostic app target. Production Orlix has only the Orlix application SwiftUI `@main`. Delete the fallback only at the verified mobile cutover.

3. **#49, imported-feature capability and provenance gate**
   - Run only after #50 has imported the complete pinned application. Derive the capability inventory from actual imported sources and built targets, never from a speculative pre-import catalog.
   - Complete the parity ledger, dependency and vendored-artifact provenance, identity map, privacy-manifest union, encryption classification, provisioning evidence, CloudKit schema evidence, and App Store availability reasons.
   - Fail closed when a feature, dependency, identifier, entitlement, privacy declaration, license, build input, or test target lacks immutable evidence.

4. **#51, Default Local Instance terminal target**
   - Run only after #49 and #50. Add the Default Local Instance as a typed `TerminalTarget` backed directly by OrlixOS, never as a VVTerm Server or SSH connection.
   - Prove terminal input, output, resize, close, background, foreground, and restart behavior through the Orlix application.
   - Keep Local Runtime and Linux policy out of the Swift application and OrlixHostAdapter. Route it through OrlixOS and the existing upstream-Linux ownership boundaries.

5. **Herdr and remote transport**
   - Obtain the commercial Herdr license before integration.
   - Port Herdr, its dependencies, agents, plugins, worktrees, terminal behavior, and socket API to Orlix Linux.
   - Add the external Pane backend upstream extension and native Orlix projection.
   - Build and bridge the SSH/TSSH XCFramework, Mosh transport, hardware identity broker, Network.framework dialer, remote files, cloud providers, Kubernetes, serial consoles, port forwarding, VPN extension, widgets, Shortcuts, and Live Activities.
   - Preserve the raw Herdr TUI and `herdr --remote` behavior inside Orlix Linux. Native Mac CLI work remains deferred.

6. **Full mobile Local Runtime and first public cutover**
   - Complete the current TCTI and Linux runtime proof lane before Herdr or Local Instance runtime claims.
   - Add one-kernel multi-instance namespaces and cgroups.
   - Complete automatic migration, CloudKit, Keychain, StoreKit, accessibility, localization, and restoration.
   - Publish iOS and iPadOS only after the complete terminal, remote, Herdr, and local-runtime gates pass.

7. **Containers, resources, and Docker compatibility**
   - Complete networking, instance-specific OCI services, resource APIs, files, logs, stats, history, inspect, health, restart policies, templates, and Compose placement.
   - Build the Docker-compatible Linux userspace service, BuildKit executor, CLI contexts, Compose, Buildx, Swarm, and plugin compatibility.
   - Add the Contained-inspired container UI and Orchard-inspired typed provider boundaries.
   - Apply Contained's visual hierarchy, cards, inspectors, charts, and resource-management polish only in this container phase. Do not copy its PolyForm Noncommercial source or assets.
   - Reuse Orchard's MIT-licensed typed-backend patterns only in this container phase, without introducing Apple Container or Virtualization.framework dependencies into Orlix runtime code.
   - Publish iOS and iPadOS only when all Linux Docker Engine areas pass and Windows Containers are the only deliberate exclusion.

8. **Native macOS product, last**
   - Start only after the mobile terminal and container releases are in good shape and published to the App Store.
   - Add the macOS app target, arm64 macOS OrlixKernel and OrlixOS slice, user-scoped runtime service, shared sockets, embedded helpers, native Herdr CLI, external-shell validation, shared-folder grants, and Docker contexts.
   - Reuse the macOS-compatible VVTerm source retained during the mobile implementation. Do not replace it with a separate Mac application architecture.
   - Release only after the complete terminal, remote, Herdr, Local Runtime, container, Docker, helper, and App Store gates pass on macOS.

## 4. Test and release gates

### VVTerm mobile baseline and parity gate

- Run every actual pristine upstream target at the pinned commit: one unit target containing 62 Swift test files and one UI target containing 5 Swift test files. Upstream has no separate integration or snapshot target at this pin, so do not claim those suites ran.
- Record the exact iPhone and iPad destinations, commands, pass/fail counts, skips with reasons, logs, and `.xcresult` paths for pristine upstream and Orlix-derived runs.
- Add and report Orlix integration or snapshot coverage separately from the pristine upstream target inventory.
- #50 parity includes the SwiftUI application root, every implemented mobile feature, Live Activity, resources and localizations, vendor libraries and packages, entitlements and privacy declarations, the unit target, the UI target, the parity ledger, and a pristine-to-Orlix comparison with no silent omission.

### Terminal and Herdr gate

- Run pristine Herdr upstream tests on native macOS and Orlix Linux.
- Test default and named Sessions, multiple native and raw clients, Workspace and Tab lifecycle, split dragging, Pane takeover, agents, worktrees, plugins, marketplace installation, persistence, remote attach, protocol-version mismatch, and snapshot/event resynchronization.
- Fault-inject external Pane framing, duplicate and missing sequences, resize/input races, credit exhaustion, controller takeover, backend crash, app suspension, reconnect, transport close, and sanitized errors.
- Verify raw Herdr TUI and native Orlix UI display and control the same Pane state.

### RootShell-derived regression gate

Convert every recurring release-note failure class from builds 34 through 120 into tests:

- Protected-data startup and background suspension.
- Display-link, renderer, Metal snapshot, lock, resume, and teardown behavior.
- TSSH/tsshd version skew and server upgrade diagnostics.
- QUIC, KCP, Mosh, and network-path reconnect timers.
- Ordered input/output, bounded buffering, and backpressure.
- Terminal-mode, alternate-screen, synchronized-output, and focus restoration.
- Resize coalescing during keyboard, rotation, split, and window animation.
- Stale Pane, Tab, controller, and renderer teardown without use-after-free.
- Software and hardware keyboard modifiers, IME, CJK, emoji, Unicode width, dictation, and layout handling.
- Host-key, host-CA, certificate, password, agent, Secure Enclave, YubiKey, FIDO2, and OpenPubkey states.
- Hostile server strings, filenames, topology counts, ANSI/control sequences, OAuth failures, and malformed provider data.
- App Store capability differences and redacted, stable diagnostics.
- Sustained ANSI output, scrollback, concurrent Panes, large remote transfers, reconnect backlog, and bounded memory without UI wedges.

### Remote interoperability gate

- Test direct and multi-hop SSH against pinned OpenSSH servers with every required authentication mode.
- Test host-key changes, host certificates, user certificates, expired certificates, keyboard-interactive retries, forwarding, SFTP, SCP, exec, PTY, terminal resize, and disconnect cleanup.
- Test TSSH against `tsshd` 0.1.8 over QUIC and KCP, including mismatched and missing servers, reconnect, fallback, roaming, forwards, and kill/reap behavior.
- Test Mosh against upstream 1.4.0, including roaming, delayed packets, sequence wrap, prediction, and app relaunch.
- Test AWS, Azure, Linode, DigitalOcean, Tailscale, NetBird, Kubernetes, EKS, LISH, and EC2 serial accounts with mocked contract suites plus opt-in live acceptance accounts.
- Test VPN through an exported Network Extension build, including DNS, TCP, UDP, HTTP/3, route exclusions, reconnect, app relaunch, widgets, and Live Activity state.
- No location-background mode is present in the release binary.

### Local Runtime gate

- Preserve the active no-phone golden ELF and switch-debug TCTI oracle order before gadgets or device work.
- Do not start physical-device TCTI work until the harness-selected pinned simulator has current passing readiness reports.
- Run the full ADR 0017 promotion order through OrlixOS.
- Run pristine Linux, mlibc, shell, and package tests. Do not edit generated upstream trees or tests.
- Prove PTYs, process groups, signals, polling, Unix sockets, process inspection, persistence, namespaces, cgroups, networking, jq, curl, zsh, and Herdr inside the delivered distribution.
- Prove two Local Instances can run concurrently with isolated PID, mount, hostname, network, root, and cgroup state while sharing one OrlixKernel.
- In the later macOS phase, prove the native app, runtime service, Orlix Linux terminal, native Herdr CLI, and `herdr --remote` from an external shell.
- Record exact workload, target, configuration, baseline, and measurements for ELF launch, PTY readiness, terminal throughput, storage, OCI import, and imported-binary startup. Simulator-only results remain labeled Simulator-only.

### Container and Docker gate

- Run Docker Engine API conformance across negotiated versions 1.24 through 1.55, with the default minimum at 1.40.
- Run pinned Docker CLI 29.6.1, Compose 5.3.1, Buildx 0.35.0, and BuildKit 0.31.1 acceptance suites against Orlix.
- Run pristine applicable upstream integration tests without editing or filtering them.
- Exercise Containers, images, registries, volumes, bind mounts, networks, builds, cache import/export, logs, exec, stats, events, contexts, Compose, Swarm, services, configs, secrets, plugins, health, restart, and `--privileged`.
- Record every failure and skip. Windows Container tests are the only product-scope skips. Environment-specific upstream harness exclusions require an explicit audited reason and may not be counted as behavioral success.
- Test Compose reconciliation, orphan behavior, dependencies, profiles, health gates, exit codes, named-volume preservation, and project-to-instance placement.
- Test path escapes, symlink escapes, transfer conflicts, changing downloads, sparse files, permissions, sensitive inspect fields, cgroup accounting, event retention, and hostile archive contents.
- Test bridge networking, DNS, published ports, IPv4/IPv6, Swarm traffic, VPN interaction, and multiple isolated instances.
- In the later macOS phase, test folder grants, bookmark revocation, rootless ports, context conflicts, CLI upgrades, socket permissions, runtime-service relaunch, and app-group cleanup.

### App Store and final verification

- Written legal approval is required for the VVTerm GPL/App Store model and any statically linked copyleft dependencies.
- A commercial Herdr agreement covering the pinned revision, native embedding, Orlix Linux distribution, protocol extension, plugins, marketplace, and all supported platforms is required.
- App Review and entitlement approval are required for Network Extension, CloudKit, Keychain groups, app groups, Live Activities, StoreKit, helper services, Herdr marketplace behavior, downloaded Linux content, images, and Docker plugins.
- If full Herdr or the requested Linux Docker surface cannot be approved, the affected public release stops. Do not silently ship a reduced substitute.
- Validate the exported iOS and iPadOS App Store packages first. Validate the macOS package, embedded binaries, signatures, Hardened Runtime, entitlements, privacy manifests, minimum OS version, source notices, and helper launch behavior only in the later Mac phase.
- Inspect simulator and app crash reports after mobile hosted failures, and macOS crash reports during the later Mac phase.
- Run `git diff --check`, stale-reference scans, plan consistency, license scans, `rtk proxy make tcti-gate TARGET=tcti-plan-consistency`, and `rtk proxy make agent-harness-check`.
- Completion reports must state exact commands, targets, logs, failures, skips, crash checks, measured workloads, and remaining gaps.

## 5. Assumptions and pinned references

All revisions were retrieved on 2026-07-13 and must be recorded as immutable full commits in the provenance document:

- VVTerm: `791eebae946b0831ffff3ac839e0f2b75d076458`, GPL-3.0 source. No VVTerm App Store EULA rights are assumed for Orlix.
- Herdr: `3a8490f6515dfea13292ae28e34f1174d2f68af1`, commercially licensed instead of relying on AGPL-3.0-or-later.
- RootShell public issue/reference repository: `d1062b80be3df0cbd2a9c9066f38404151b9f688`; release history read through 1.0.9 build 120.
- Orchard: `f4cd83796c8d24851fea4e17a95ec46fb371f56d`, MIT reference.
- Contained: `711800118c816839092a5897143a1f69b81a8dc1`, visual reference only because of PolyForm Noncommercial 1.0.0.
- `trzsz-ssh` 0.1.25: `bb4bd347a6d04668d5618e1025af768ddc2d82e0`.
- `tsshd` 0.1.8: `5a68bb5fe265b72d0f5dae67e8a1fcab750eafae`.
- Mosh 1.4.0 reference: `bc73a26316ede2a79259d859f8ee309b32412420`.
- Docker Engine 29.6.1 reference: `8ec5ab355a34b2a0e2b3238d67bdefe77fefa982`.
- Docker CLI 29.6.1: `8900f1d330cb39e93e16d780a26bff1d7e07ba03`.
- Docker Compose 5.3.1: `f32009d4a2c687dd405398cc7975d12dccaf8dff`.
- Docker Buildx 0.35.0: `a319e5b15052cf6557ceb666eb8ff6e32380b782`.
- BuildKit 0.31.1: `673b7e0196de0cac83308274b88aaed97a91af74`.
- Apple's "Embedding a helper tool in a sandboxed app" documentation governs app-launched helper packaging. It does not prove privilege escalation or arbitrary external-shell/socket access.
