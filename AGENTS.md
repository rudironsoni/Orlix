# AGENTS.md

These rules apply to every task in this repository unless the user explicitly overrides them.

## Project Invariant

Orlix compiles upstream Linux into an iOS-hosted `OrlixKernel.xcframework`, pairs it with `OrlixMLibC`, and delivers the curated OS as the `OrlixOS` Kit/framework.

If a change makes Orlix less suitable for real Linux userspace, the change is wrong.

OrlixKernel is Linux. It does not own shell behavior, libc behavior, package management, public syscall APIs, or a custom runtime facade. Shells and packages are normal Orlix Linux userspace binaries linked against OrlixMLibC and executed through Linux mechanisms.

Apps consume `OrlixOS` for the delivered OS session and payload surface. Do not recreate a separate `OrlixKit` module or move OS delivery into `Orlix` or `OrlixHostAdapter`.

## First Reads

- Architecture: `docs/architecture/ORLIX_UPSTREAM_LINUX_IOS_PORT.md`
- Glossary: `docs/reference/ORLIX_GLOSSARY.md`
- ADR index: `docs/adr/README.md`
- Harness guide: `docs/harness/README.md`
- Agent memory: `docs/harness/MEMORY.md`
- Active plans: `docs/plans/active/`

## Ownership

Upstream Linux owns Linux behavior: VFS, tasks, fd tables, signals, wait/reaping, procfs, sysfs, devtmpfs, cgroups, namespaces, sockets, syscall semantics, exec, and interpreter behavior.

Durable Orlix Linux port inputs live under:

- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix`
- `OrlixKernel/Sources/ports/orlix/overlay/drivers/orlix`
- `OrlixKernel/Sources/ports/orlix/configs`
- `OrlixKernel/Sources/ports/orlix/patches`

`OrlixHostAdapter/Sources` owns private iOS and Darwin mechanics only. It must not own Linux policy or public Linux ABI.

`OrlixMLibC/Sources` owns OrlixMLibC sysdeps, configs, and patches. OrlixMLibC consumes Linux UAPI through `headers_install` for upstream `ARCH=arm64` and calls Linux-shaped syscalls.

`OrlixOS` is the Kit: it owns curated distribution policy, package/rootfs assembly, product payload packaging, target-derived payload metadata, and the app-facing Linux session API. It must not own kernel semantics, libc semantics, syscall ABI, private iOS host mechanics, or terminal UI rendering.

## Generated Trees

Generated upstream and disposable build trees are read-only inputs for agents:

- `Build/OrlixKernel/upstream/linux-*.git`
- `Build/OrlixKernel/src/linux-*-port`
- `Build/OrlixMLibC`
- `Build/OrlixMLibC/upstream/mlibc-*.git`
- `Build/OrlixMLibC/src/mlibc-*`
- `Build/OrlixOS`

Do not edit generated upstream trees, adapted upstream tests, generated package sources, or disposable build output to make tests pass.

## Retired Prototype

The local kernel prototype is retired. Do not restore `LegacyOrlix/`, `OrlixKernel/fs`, `OrlixKernel/kernel`, or `OrlixKernel/runtime`. Useful behavior must move by ownership into upstream Linux, `arch/orlix`, Linux-native drivers, boot code, or narrow host-adapter seams.

## Codex Harness

Use Codex-native surfaces deliberately:

- `.codex/agents/orlix-planner.toml` for non-trivial planning and `docs/plans/active/<task>/PLAN.md`.
- `.codex/agents/orlix-implementer.toml` for executing an active plan and maintaining `IMPLEMENT.md`.
- `.codex/agents/orlix-reviewer.toml` for skeptical review of assumptions, directives, upstream conformance, and evidence.
- `.agents/skills/orlix-implementation-boundaries/SKILL.md` before deciding which layer owns a fix.
- `.agents/skills/orlix-upstream-conformance/SKILL.md` for upstream Linux, mlibc, Coreutils, or other upstream test suites.
- `.agents/skills/orlix-runtime-claim-verification/SKILL.md` before claiming fixed, green, complete, runtime-ready, package-ready, or upstream-test success.
- `.agents/skills/orlix-write-to-harness/SKILL.md` before editing this harness, plans, skills, agents, hooks, rules, ADR indexes, or agent memory.

`rtk` only shrinks command output. Harness rules and hooks must treat `rtk <command>` as equivalent to `<command>` for approval and block decisions.

## Proof Rules

Define success criteria before implementation and verify them before claiming completion.

Product runtime claims follow ADR 0017's promotion order:

1. Kernel dependency proof
2. Kselftest kernel-interface proof
3. OrlixMLibC libc proof
4. OrlixMLibC-built syscall/UAPI proof
5. POSIX shell environment proof
6. Third-party package ladder: jq, curl, zsh

Do not claim product runtime readiness from KUnit, kselftest, no-init boot logs, packaging, simulator launch, host-side harnesses, or partial upstream test runs.

For upstream conformance work, upstream sources and tests are authoritative. Fix Orlix to match upstream Linux/mlibc/package behavior; do not edit generated upstream sources or reinterpret failures as success.

## Working Rules

1. Read owning files, callers, docs, and ADRs before writing.
2. Prefer the smallest correct change; avoid speculative abstractions.
3. Touch only what the task requires.
4. Surface ownership conflicts directly.
5. Use `rtk` for shell commands in this workspace.
6. Use `rg`/`rg --files` for searches.
7. Preserve unrelated dirty worktree changes.
8. Fail loud when evidence is missing, partial, skipped, or stale.
9. Check simulator/app crash reports after app-hosted test failures or crashes.
10. Start each Orlix session by checking active plans before implementation or status claims.
11. Keep `IMPLEMENT.md` current after each coherent checkpoint.
12. Commit and push after a coherent verified checkpoint when implementation work is complete.
13. Host virtual address implementation, diagnostics, and tests must derive address ranges from the OS/runtime under test. Do not hardcode host VM candidate addresses to simulate device behavior, and do not expose test-only HostAdapter APIs or ABI to reach internals.

## Xcode / Simulator / External SSD Environment Rules

This machine has a deliberately externalized Xcode/CoreSimulator storage setup.
Treat it as part of the environment contract. Do not bypass it, do not
"simplify" it, and do not hardcode `/Volumes/1TB` in project scripts.

### Core Principle

Xcode, `xcodebuild`, `xcrun`, `simctl`, DerivedData, SwiftPM package caches,
CoreSimulator devices, and CoreSimulator caches are configured to use the
external SSD transparently.

Agents should use the normal Apple paths and commands. The relocation is handled
by wrapper scripts and mounted APFS sparsebundles.

The intended behavior is:

- Agents do not need to know where the external SSD is mounted.
- Agents do not pass custom DerivedData paths unless explicitly required by the
  user.
- Agents do not create project-specific workarounds for the external SSD.
- Agents do not edit source code to compensate for simulator/storage issues.
- Agents diagnose environment problems as environment problems.

### Required PATH

Before running Xcode, `xcrun`, `simctl`, or build/test commands, use this PATH:

```sh
export PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin"
```

This ensures the wrappers are used:

```text
~/.local/bin/xcrun
~/.local/bin/simctl
~/.local/bin/xcodebuild
```

Keeping `/opt/homebrew/bin` after `$HOME/.local/bin` preserves `rtk`,
`fastlane`, and Homebrew tools without bypassing the Xcode wrappers.

Do not call these directly unless debugging wrapper behavior:

```text
/usr/bin/xcrun
/usr/bin/xcodebuild
/Applications/Xcode.app/Contents/Developer/usr/bin/xcodebuild
```

Direct Apple binaries bypass some of the fail-closed validation.

### Do Not Hardcode External SSD Paths

Do not hardcode:

```text
/Volumes/1TB
/Volumes/1TB/Xcode
/Volumes/1TB/Xcode/DerivedData
/Volumes/1TB/Xcode/CoreSimulator
```

The correct abstraction is:

```text
external-ssd-root
```

If an external path is truly needed for diagnostics, resolve it dynamically:

```sh
ROOT="$(external-ssd-root)"
echo "$ROOT/Xcode"
```

But normal project commands should not need this.

### Xcode Build/Test Commands

Use plain `xcodebuild` through the PATH wrapper.

Good:

```sh
export PATH="$HOME/.local/bin:/usr/bin:/bin:/usr/sbin:/sbin"

xcodebuild \
  -project Orlix.xcodeproj \
  -scheme "OrlixKernel Conformance" \
  -configuration Debug \
  -destination 'platform=iOS Simulator,id=4B85E297-7A59-45BD-A94B-189238EC48FA' \
  test
```

Avoid adding:

```text
-derivedDataPath ...
-clonedSourcePackagesDirPath ...
SYMROOT=...
OBJROOT=...
CLANG_MODULE_CACHE_PATH=...
SWIFT_MODULE_CACHE_PATH=...
```

The wrapper enforces those to external SSD-backed locations.

If you pass your own `-derivedDataPath`, the wrapper may remove or override it.
That is expected.

### Preferred Simulator Destination

The currently known-good simulator is:

```text
ExternalSSDProof
UDID: 4B85E297-7A59-45BD-A94B-189238EC48FA
Runtime: iOS 26.5
```

Prefer this destination for Xcode tests unless the task explicitly requires a
fresh simulator:

```text
-destination 'platform=iOS Simulator,id=4B85E297-7A59-45BD-A94B-189238EC48FA'
```

This simulator is booted and has already completed the expensive first-boot data
migration.

Do not prefer name-based destinations if a UDID is known. Name-based
destinations can become ambiguous after agents create additional simulators.

Prefer:

```text
-destination 'platform=iOS Simulator,id=4B85E297-7A59-45BD-A94B-189238EC48FA'
```

Over:

```text
-destination 'platform=iOS Simulator,name=iPhone 17,OS=26.5'
```

### Creating New Simulators

If a fresh simulator is required, create it through wrapped `xcrun` or `simctl`:

```sh
export PATH="$HOME/.local/bin:/usr/bin:/bin:/usr/sbin:/sbin"

UDID="$(xcrun simctl create "AgentTest-$(date +%H%M%S)" "iPhone 17" "com.apple.CoreSimulator.SimRuntime.iOS-26-5")"
echo "$UDID"
```

New simulator directories should appear under the normal Apple path:

```text
~/Library/Developer/CoreSimulator/Devices/<UDID>
```

That path is externally backed by the mounted sparsebundle. Do not move it
manually.

After creating a simulator, verify storage backing:

```sh
mount | grep "$HOME/Library/Developer/CoreSimulator/Devices"
```

Expected shape:

```text
/dev/disk... on /Users/rudironsoni/Library/Developer/CoreSimulator/Devices (apfs, ...)
```

Do not assume first boot will be quick. On iOS 26.5, first boot can take around
10 minutes because Apple data migration runs inside the simulator.

Use a generous timeout and inspect state instead of killing it early.

Example:

```sh
xcrun simctl boot "$UDID"
xcrun simctl bootstatus "$UDID" -b
```

Important: `simctl bootstatus` may return success even when the printed terminal
state indicates a migration failure. Read the output. Look for terminal success,
not just exit code.

Successful terminal boot looks like:

```text
Device already booted, nothing to do.
```

or CoreSimulator log state:

```text
status = Finished
DataMigrationPhaseDescription = kDMMigrationPhaseDescriptionDidFinishWithSuccess
```

Bad terminal state includes:

```text
Data Migration Failed
DataMigrationPhaseDescription = kDMMigrationPhaseDescriptionDidFinishWithFailure
```

If a newly-created simulator fails migration, delete that simulator and use the
known-good booted simulator instead unless the user specifically wants to debug
simulator creation.

Cleanup:

```sh
xcrun simctl shutdown "$UDID" 2>/dev/null || true
xcrun simctl delete "$UDID"
```

### CoreSimulator Storage Contract

The simulator device store must remain:

```text
~/Library/Developer/CoreSimulator/Devices
```

It is backed by:

```text
/Volumes/1TB/Xcode/CoreSimulator/DeviceSet.sparsebundle
```

The CoreSimulator cache store must remain:

```text
/Library/Developer/CoreSimulator/Caches
```

It is backed by:

```text
/Volumes/1TB/Xcode/CoreSimulator/Caches.sparsebundle
```

Do not replace these with symlinks.

Do not move device directories manually.

Do not mount a raw APFS volume over
`~/Library/Developer/CoreSimulator/Devices`. That approach was tested and
rejected because CoreSimulatorService hit permission/state errors.

The certified backend is the sparsebundle mounted at the normal Apple path.

### Health Check

Before diagnosing Xcode or simulator failures, run:

```sh
export PATH="$HOME/.local/bin:/usr/bin:/bin:/usr/sbin:/sbin"
xcode-storage-doctor
```

Expected outside restrictive sandboxes:

```text
OK xcode external storage doctor passed
```

If this fails, treat it as an environment issue first. Do not change project
source code until the environment is healthy.

Useful checks:

```sh
external-ssd-root

mount | grep "$HOME/Library/Developer/CoreSimulator/Devices"
mount | grep '/Library/Developer/CoreSimulator/Caches'

xcrun simctl list runtimes available
xcrun simctl list devices available
xcodebuild -version
xcodebuild -project Orlix.xcodeproj -list
```

### Sandbox Caveat

Some coding-agent sandboxes block Apple CoreSimulator XPC and DiskManagement
access. In that case, `simctl` can fail with errors that look like broken
CoreSimulator services, for example:

```text
Code=61
Code=409
Code=410
Cannot talk to simdiskimaged
simdiskimaged crashed or is not responding
```

If the agent is running in a restrictive sandbox, do not conclude the machine
environment is broken until the same command has been tested outside the
sandbox.

Known sandbox marker:

```sh
echo "$CODEX_SANDBOX"
```

If it prints:

```text
seatbelt
```

then CoreSimulator commands may need unsandboxed/escalated execution.

For real simulator/Xcode tests, use an unsandboxed command execution path.

### Diagnosing XCTest Failures

Distinguish these cases:

#### Environment / runner attach failure

Symptoms:

```text
The test runner hung before establishing connection
Connection to remote process was not established
launch failed
SIGTERM(15)
```

Check environment first:

```sh
xcode-storage-doctor
xcrun simctl bootstatus 4B85E297-7A59-45BD-A94B-189238EC48FA -b
xcrun simctl list devices available
```

Then inspect CoreSimulator logs:

```sh
tail -300 "$HOME/Library/Logs/CoreSimulator/CoreSimulator.log" | grep -E 'OrlixTestRunner|XCTest|testmanager|launch failed|hung|SIGTERM|Error'
```

Do not modify project code until runner attachment has been proven healthy.

#### Project/test failure

If logs show:

```text
Testing started
Test Suite ...
Test Case ...
Executed ...
```

then XCTest attached successfully. Failures after that are project/test
behavior, not the external storage migration.

The environment has already proven that hosted XCTest can attach and execute
against the booted iPhone 17 simulator.

### Result Bundles And Build Products

Build products and test logs should land under external SSD-backed DerivedData:

```text
/Volumes/1TB/Xcode/DerivedData/Build/Products
/Volumes/1TB/Xcode/DerivedData/Logs/Test
```

You may reference these paths in diagnostics after resolving through the
environment, but do not bake them into source code, scripts, or docs as fixed
assumptions.

To find recent test results:

```sh
ls -dt "$(external-ssd-root)"/Xcode/DerivedData/Logs/Test/*.xcresult | head
```

### Do Not Do These

Do not move Xcode.app.

Do not move application binaries to the external SSD.

Do not replace CoreSimulator directories with symlinks.

Do not create project-specific DerivedData directories under the repo.

Do not delete or recreate simulator runtimes casually.

Do not kill a simulator during first-boot data migration unless you are
intentionally abandoning that simulator and will delete it afterward.

Do not trust `simctl bootstatus` exit code alone. Inspect the terminal state.

Do not use `/usr/bin/xcrun` or `/usr/bin/xcodebuild` in normal work.

Do not hardcode `/Volumes/1TB`.

Do not "fix" XCTest runner issues by changing Linux, OCI, kernel, or app code
until the hosted XCTest attachment path is independently verified.

### TestFlight Release Flow

Use the repository beta targets and direct fastlane upload path for TestFlight.
Do not switch to Xcode Organizer, ad hoc upload scripts, or a new release system
unless the user explicitly asks.

`make beta-archive` is the one sanctioned exception to the normal wrapped
`xcodebuild` rule. It calls `/usr/bin/xcodebuild` directly while passing
`-derivedDataPath "$(external-ssd-root)/Xcode/DerivedData"` and
`-clonedSourcePackagesDirPath "$(external-ssd-root)/Xcode/PackageCache"`.
Do not add `SYMROOT`, `OBJROOT`, module-cache build settings, or wrapper
workarounds to archive finalization. Those settings break Xcode archive
finalization with a missing `BuildProductsPath`.

The default release flow is:

```sh
export PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin"
make beta-simulator-gate
make beta-archive ORLIX_DEVELOPMENT_TEAM=<Apple team id>
make beta-export-archive ORLIX_BETA_EXPORT_OPTIONS_PLIST=<export options plist>
make beta-upload
```

`beta-archive` must keep automatic build-number bumping enabled unless the user
explicitly disables it. The bump must account for App Store Connect/TestFlight
when the fastlane API key JSON is available, so a new upload does not reuse an
already uploaded build number.

### Known-Good Baseline Commands

Use these when starting a task that depends on Xcode or Simulator:

```sh
export PATH="$HOME/.local/bin:/usr/bin:/bin:/usr/sbin:/sbin"

xcode-storage-doctor

xcrun simctl bootstatus 4B85E297-7A59-45BD-A94B-189238EC48FA -b

xcodebuild \
  -project Orlix.xcodeproj \
  -scheme "OrlixTestRunner Tests" \
  -configuration Debug \
  -destination 'platform=iOS Simulator,id=4B85E297-7A59-45BD-A94B-189238EC48FA' \
  -only-testing:OrlixTestRunnerTests \
  test
```

If that smoke test reaches `Testing started` and test suites execute, the
hosted XCTest attach path is healthy.

If a later, more specific test fails after test execution begins, treat it as a
test/project/runtime behavior issue, not as an external SSD storage issue.

## XcodeBuildMCP

If using XcodeBuildMCP, use the installed XcodeBuildMCP skill before calling XcodeBuildMCP tools.


<!-- headroom:rtk-instructions -->
# RTK (Rust Token Killer) - Token-Optimized Commands

When running shell commands, **always prefix with `rtk`**. This reduces context
usage by 60-90% with zero behavior change. If rtk has no filter for a command,
it passes through unchanged — so it is always safe to use.

## Key Commands
```bash
# Git (59-80% savings)
rtk git status          rtk git diff            rtk git log

# Files & Search (60-75% savings)
rtk ls <path>           rtk read <file>         rtk grep <pattern>
rtk find <pattern>      rtk diff <file>

# Test (90-99% savings) — shows failures only
rtk pytest tests/       rtk cargo test          rtk test <cmd>

# Build & Lint (80-90% savings) — shows errors only
rtk tsc                 rtk lint                rtk cargo build
rtk prettier --check    rtk mypy                rtk ruff check

# Analysis (70-90% savings)
rtk err <cmd>           rtk log <file>          rtk json <file>
rtk summary <cmd>       rtk deps                rtk env

# GitHub (26-87% savings)
rtk gh pr view <n>      rtk gh run list         rtk gh issue list

# Infrastructure (85% savings)
rtk docker ps           rtk kubectl get         rtk docker logs <c>

# Package managers (70-90% savings)
rtk pip list            rtk pnpm install        rtk npm run <script>
```

## Rules
- In command chains, prefix each segment: `rtk git add . && rtk git commit -m "msg"`
- For debugging, use raw command without rtk prefix
- `rtk proxy <cmd>` runs command without filtering but tracks usage
<!-- /headroom:rtk-instructions -->
