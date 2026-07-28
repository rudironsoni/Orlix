# AGENTS.md

These rules apply to every task in this repository unless the user explicitly overrides them.

## Project Invariant

Orlix compiles upstream Linux into an iOS-hosted `OrlixKernel.xcframework`, pairs it with `OrlixMLibC`, and delivers the curated OS as the `OrlixOS` Kit/framework.

If a change makes Orlix less suitable for real Linux userspace, the change is wrong.

OrlixKernel is Linux. It does not own shell behavior, libc behavior, package management, public syscall APIs, or a custom runtime facade. Shells and packages are normal Orlix Linux userspace binaries linked against OrlixMLibC and executed through Linux mechanisms.

Apps consume `OrlixOS` for the delivered OS session and payload surface. Do not recreate a separate `OrlixKit` module or move OS delivery into `OrlixTerminal` or `OrlixHostAdapter`.

The canonical product layers are `OrlixOS`, `OrlixKernel`, `OrlixMLibC`, `OrlixCoreUtils`, `OrlixMachine`, `Containers`, `Herdr`, `OrlixTCTI`, and `OrlixHostAdapter`. Use these names in source, project configuration, tests, and harness guidance. Do not introduce aliases or parallel facades for them.

## First Reads

- Knowledge index: `docs/index.md`
- Ontology and maintenance protocol: `docs/ontology.md` and `docs/AGENTS.md`
- Architecture ownership: `docs/concepts/upstream-linux-ownership.md` and `docs/concepts/component-ownership.md`
- Work hierarchy: epics, stories, and tasks under matching `todo/`, `doing/`, and `done/` folders in `docs/objects/`
- Current work context: every page under `docs/objects/{epic,story,task}/doing/`
- Current execution state: structured reports and task envelopes under `Build/AgentHarness/`

## Ownership

Upstream Linux owns Linux behavior: VFS, tasks, fd tables, signals, wait/reaping, procfs, sysfs, devtmpfs, cgroups, namespaces, sockets, syscall semantics, exec, and interpreter behavior.

Durable Orlix Linux port inputs live under:

- `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix`
- `OrlixKernel/Sources/ports/orlix/overlay/drivers/orlix`
- `OrlixKernel/Sources/ports/orlix/configs`
- `OrlixKernel/Sources/ports/orlix/patches`

`OrlixHostAdapter/Sources` owns private iOS and Darwin mechanics only. It must not own Linux policy or public Linux ABI.

`OrlixMLibC/Sources` owns OrlixMLibC sysdeps, configs, and patches. OrlixMLibC consumes Linux UAPI through `headers_install` for upstream `ARCH=arm64` and calls Linux-shaped syscalls.

`OrlixOS` is the only public Kit/framework and the app-facing product boundary. `OrlixKernel`, `OrlixMLibC`, and `OrlixCoreUtils` are private implementation products behind that boundary. Do not expose them as parallel app APIs.

`OrlixOS` owns curated distribution policy, package/rootfs assembly, product payload packaging, target-derived payload metadata, and the app-facing Linux session API. `OrlixMachine` owns Linux machine and session composition behind that API. `Containers` owns container lifecycle and integration with a machine. These layers must not absorb kernel semantics, libc semantics, syscall ABI, private iOS host mechanics, or terminal UI topology.

Herdr is authoritative for the terminal hierarchy: Session, Workspace, Tab, and Pane, including focus and topology. The native app presents that hierarchy; it does not maintain a parallel terminal model. `OrlixOS`, `OrlixMachine`, and `Containers` provide session targets to Herdr-owned panes without taking ownership of terminal topology.

`OrlixTCTI` owns guest instruction translation and execution beneath Linux. It does not own Linux process, syscall, signal, fault, scheduling, or filesystem semantics. `OrlixHostAdapter` owns private Apple and Darwin execution mechanics used by the lower layers. It must not decode guest instructions, own Linux policy, expose public Linux ABI, or become an app-facing runtime facade.

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

- `.codex/agents/orlix-planner.toml` for creating or refining epic, story, task, and decision pages.
- `.codex/agents/orlix-implementer.toml` for executing the selected structured task envelope without maintaining an implementation journal.
- `.codex/agents/orlix-reviewer.toml` for skeptical review of assumptions, directives, upstream conformance, and evidence.
- `.agents/skills/orlix-implementation-boundaries/SKILL.md` before deciding which layer owns a fix.
- `.agents/skills/orlix-upstream-conformance/SKILL.md` for upstream Linux, mlibc, Coreutils, or other upstream test suites.
- `.agents/skills/orlix-runtime-claim-verification/SKILL.md` before claiming fixed, green, complete, runtime-ready, package-ready, or upstream-test success.
- `.agents/skills/orlix-write-to-harness/SKILL.md` before editing the ontology, harness, skills, agents, hooks, or generated rules.

`rtk` only shrinks command output. Harness rules and hooks must treat `rtk <command>` as equivalent to `<command>` for approval and block decisions.

Repository lifecycle hooks enforce only repo-wide workflow invariants. They must
not select, authorize, or validate OrlixTCTI work, release readiness,
physical-device work, or machine-specific Xcode storage. OrlixTCTI kernel
correctness is validated by `make orlix-tcti-kernel-tests` through KUnit and
Linux kselftest execution inside the app-hosted OrlixTCTI kernel.

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
6. Use Make as the only repository-owned executable developer interface. Keep implementation in the owning component's Make rules or non-executable source modules invoked only by private Make targets; do not add standalone command scripts.
7. Use `rg`/`rg --files` for searches.
8. Preserve unrelated dirty worktree changes.
9. Fail loud when evidence is missing, partial, skipped, or stale.
10. Check simulator/app crash reports after app-hosted test failures or crashes.
11. Commit and push after a coherent verified checkpoint when implementation work is complete.

## XcodeBuildMCP

If using XcodeBuildMCP, use the installed XcodeBuildMCP skill before calling XcodeBuildMCP tools.
