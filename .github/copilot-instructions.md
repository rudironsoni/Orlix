# AGENTS.md

These rules apply to every task in this repository unless the user explicitly overrides them.

## Project Invariant

Orlix is the full first-party application. It consumes `OrlixKit.xcframework` as its sole local Linux runtime dependency. `OrlixKit` exposes `OrlixEngine`, which boots and hosts one running `OrlixOS` with one upstream Linux kernel.

If a change makes Orlix less suitable for real Linux userspace, the change is wrong.

OrlixKernel is Linux. It does not own shell behavior, libc behavior, package management, public syscall APIs, or a custom runtime facade. Shells and packages are normal Orlix Linux userspace binaries linked against OrlixMLibC and executed through Linux mechanisms.

`OrlixOS` hosts zero or more persistent, isolated `OrlixInstance`s. Each instance shares the kernel and runs ordinary `OrlixProcess` values and OCI `OrlixContainer` values. OrlixKit is an SDK boundary, not a runtime object. OrlixOS is the running OS, not its rootfs.

The canonical public vocabulary is `Orlix`, `OrlixKit`, `OrlixEngine`, `OrlixOS`, `OrlixInstance`, `OrlixProcess`, and `OrlixContainer`. `OrlixBootloader`, `OrlixKernel`, `OrlixHostAdapter`, and `OrlixTCTI` remain private implementation. `OrlixMLibC`, `OrlixCoreUtils`, packages, and rootfs are Linux guest artifacts, never Apple-native link dependencies.

ADR 0040 (`docs/objects/architecture-decision/0040-recover-orlixkit-product-boundaries-and-build-reuse.md`) supersedes conflicting SDK and lifecycle naming clauses. Unrelated clauses of earlier ADRs remain authoritative. Existing source names do not establish migration completion.

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

`OrlixKit` is the public Swift module and complete embeddable XCFramework. Host apps do not directly link private runtime implementation targets. The first-party vvterm/Ghostty terminal and remote features stay in Orlix.app. A consumer must be able to launch a Linux process through pipes without a terminal.

`OrlixEngine` owns process-wide host lifecycle, resource establishment, and boot orchestration. `OrlixBootloader` owns the private boot mechanism. Linux userspace supervision manages instance and container lifecycle through Linux mechanisms. Guest distribution assembly owns package/rootfs policy and resources packaged by OrlixKit; it does not make guest libraries Apple-native dependencies.

Herdr is authoritative for the terminal hierarchy: Session, Workspace, Tab, and Pane, including focus and topology. The native app presents that hierarchy; it does not maintain a parallel terminal model. OrlixKit provides process I/O to Herdr-owned panes without taking ownership of terminal topology.

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
- `.codex/agents/orlix-implementer.toml` for executing the selected structured task envelope and recording independently verified recovery checkpoints in `IMPLEMENT.md`.
- `.codex/agents/orlix-reviewer.toml` for skeptical review of assumptions, directives, upstream conformance, and evidence.
- `.agents/skills/orlix-implementation-boundaries/SKILL.md` before deciding which layer owns a fix.
- `.agents/skills/orlix-upstream-conformance/SKILL.md` for upstream Linux, mlibc, Coreutils, or other upstream test suites.
- `.agents/skills/orlix-runtime-claim-verification/SKILL.md` before claiming fixed, green, complete, runtime-ready, package-ready, or upstream-test success.
- `.agents/skills/orlix-write-to-harness/SKILL.md` before editing the ontology, harness, skills, agents, hooks, or generated rules.

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

Before physical TCTI/product validation, run `make agent-status AREA=orlix-tcti`, `make agent-next AREA=orlix-tcti`, then `make agent-task-envelope-check AREA=orlix-tcti`. Require current native simulator-ladder evidence and `physical_device_allowed=true`. Missing, stale, false, or unverified eligibility blocks device work. Network namespace isolation does not prove unrestricted external networking.

## Build Reuse

Bazel owns the product dependency graph after the single gated cutover. Kbuild, Meson/Ninja, and upstream package build systems retain their internal dependency graphs. Make remains the public interface.

Keep action identity, incremental-directory identity, artifact/content identity, and proof/provenance identity distinct. Consumers select only semantically relevant artifacts, never a producer's entire `DefaultInfo` output set. Provenance and proof changes must not invalidate compilation unless they affect its result.

Keep mutable build state worktree-local. Share only immutable, content-addressed or dependency-checked content with bounded retention. A missing or corrupt cache may slow a build but cannot change its product. Warm promoted builds require no artifact downloads. Promotion retains independent clean builds and verified signatures; unsigned proposals never update the lock.

## Working Rules

1. Read owning files, callers, docs, and ADRs before writing.
2. Prefer the smallest correct change; avoid speculative abstractions.
3. Touch only what the task requires.
4. Surface ownership conflicts directly.
5. Use Make as the only repository-owned executable developer interface. Keep implementation in the owning component's Make rules or non-executable source modules invoked only by private Make targets; do not add standalone command scripts.
6. Use `rg`/`rg --files` for searches.
7. Preserve unrelated dirty worktree changes. Identify and preserve user-owned generated-file deltas before regeneration, then reapply or incorporate their intent without loss.
8. Fail loud when evidence is missing, partial, skipped, or stale.
9. Check simulator/app crash reports after app-hosted test failures or crashes.
10. Commit and push after a coherent verified checkpoint when implementation work is complete.
11. Record each recovery checkpoint, verification result, preserved user delta, and remaining gate in `IMPLEMENT.md`. Keep raw evidence in `Build/AgentHarness/`.

## XcodeBuildMCP

If using XcodeBuildMCP, use the installed XcodeBuildMCP skill before calling XcodeBuildMCP tools.
