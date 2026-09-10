# PR #228 recovery checkpoints

The approved recovery contract belongs to [ADR 0040](docs/objects/architecture-decision/0040-recover-orlixkit-product-boundaries-and-build-reuse.md) and the [Bazel migration epic](docs/objects/epic/doing/adopt-bazel-product-graph.md). This file records independently verified checkpoints. Architecture acceptance is not implementation or runtime proof.

## Checkpoint 1: repository authority and prerequisites

Status: verified locally and published. Current-head CI results remain separate checks.

Starting point: [rudironsoni/Orlix#228](https://github.com/rudironsoni/Orlix/pull/228), branch `fix/build-optimizations`, commit `5e2cdebcd88a5f080522cfbf65ea555405bc8e67`. The PR is open against `main`.

The documentation implementation agent encoded the accepted architecture in ADR 0040, the amended ADRs, component pages, migration contracts, and active work pages. The coordinator updated RuleSync sources and generated instructions, repaired prerequisite workflows, and verified the integrated checkpoint. Runtime source changes belong to later checkpoints.

### Preserved user work

The initial `AGENTS.md` delta adds this duplicate bullet after the existing XcodeBuildMCP instruction:

```text
- If using XcodeBuildMCP, use the installed XcodeBuildMCP skill before calling XcodeBuildMCP tools.
```

The original file and patch were preserved before generation. RuleSync generated the architecture updates, its canonical check passed, and the exact user bullet was reapplied. It remains a user-owned working-tree delta, separate from the generated checkpoint change.

The original file SHA-256 is `336d9eb542d056ba4825f60422aeb89da5a5e5340980e9278a61db0e049602a1`. Raw preservation evidence and the generated diff are under `Build/AgentHarness/bazel-migration/recovery-checkpoint-1/` in `agents-preservation.json`, `agents-user.patch`, and `agents-generated.patch`.

RuleSync source drift also retained obsolete command-wrapper requirements and an older documentation scanner. The source now preserves the effective committed rules and the scanner's exclusions for secret files and disposable Swift caches.

### Verification recorded so far

| Check | Result |
| --- | --- |
| Live PR identity and base | Head matches the starting commit; open against `main` |
| Existing Bazel PR workflow | Failed before the matrix because `/opt/homebrew/bin/gmake` was missing |
| Existing iOS 15 runtime workflow | Successful at the starting commit; separate from recovery proof |
| Baseline `gmake agent-rules-check` | Exit 2; existing source/generated drift and dirty `AGENTS.md` |
| Canonical generation check for rules, subagents, and skills | Exit 0 before reapplying the preserved user delta |
| Canonical rule check across all configured targets | Exit 0 after regenerating Copilot, Cursor, Claude, and Codex instructions; exact user delta then reapplied |
| `gmake __bazel-matrix-check` | Exit 0; 115 Python tests passed and 28 Bazel analysis tests reused passing cached results |
| `actionlint` on the six changed Bazel workflows | Exit 0 |
| `gmake __bazel-migration-inventory-check` | Exit 0 after regenerating the inventories for the changed Makefile and workflows |
| `gmake docs-index` | Exit 0; regenerated the index from canonical pages |
| `gmake docs-check agent-harness-check` | Exit 0; 0 ontology problems, 0 legacy-path problems, 13 harness tests passed |
| Independent governance and CI review | No remaining checkpoint 1 blocker after regenerating all configured rule targets |
| Independent documentation authority review | No remaining checkpoint 1 blocker after correcting ADR 0004, ADR 0037, and README |
| `/usr/bin/time -l gmake __bazel-cache-equivalence` | Exit 0; cached and uncached UAPI, mlibc, and rootfs trees match in paths, content, modes, and symlink targets |

The existing Make generation targets now accept `ORLIX_AGENT_TARGETS` and `ORLIX_AGENT_FEATURES`, retaining their previous defaults. The checkpoint uses `ORLIX_AGENT_TARGETS=codexcli ORLIX_AGENT_FEATURES=rules,subagents,skills`.

The Bazel prerequisite workflows now install the declared Brewfile before Make. GNU Make was already declared. The benchmark's existing host `jq` requirement now belongs to the Brewfile too.

The first component-baseline attempt exited 2 at the inventory check before any component build. `gmake __bazel-migration-inventory` repaired the generated maps. The retry records component execution logs, build events, and profiles under `Build/Bazel/cache-equivalence.GTQx8O`. This baseline covers UAPI, mlibc, and rootfs; it does not replace the required developer-loop mutation matrix.

The fresh baseline completed before any foreign component-rule changes. Build-event wall times were 590.006 seconds for seed, 37.691 seconds for cached, and 506.699 seconds for uncached. The cached run used 15 disk-cache results; seed and uncached each ran 15 local foreign actions. All three runs also had 10 internal Bazel actions. The entire command took 1152.79 seconds. Its new disposable run directory used 7,656,132 KiB according to `du -sk`.

`Build/AgentHarness/bazel-migration/recovery-checkpoint-1/component-baseline.json` records artifact tree digests, critical paths, action runners, measured resource fields, and missing measurements. These are baseline observations, not a claim that this checkpoint improved build performance.

### Remaining gates

The prerequisite and authority changes were published as `db776fe84c08e1587ddc62c780950f1510f66fde` and `9dfa083646cd43da29c2cdbc41ab67d6d56c2f82`. The remote branch and PR head matched the latter commit. The original dirty `AGENTS.md` bullet remains unstaged.

The new [Bazel PR run](https://github.com/rudironsoni/Orlix/actions/runs/34515244045) installed GNU Make but failed before the matrix because Homebrew rejected the untrusted `xcodesorg/made/xcodes` formula. Rudi authorized tap trust. The repository's sole declared tap, `xcodesorg/made`, now has `trusted: true` in `Brewfile`; `brew trust --tap xcodesorg/made` completed locally with exit 0. `brew bundle check --file Brewfile --no-upgrade` and `ruby -c Brewfile` passed. CI must verify this correction on the new commit.

The current TCTI scope envelope reports unverified completion and does not emit `physical_device_allowed`. Its checker validates scope only. The required executable eligibility check remains implementation work for the proof gate. Successful scope commands alone do not authorize physical-device validation.

TAP remains stopped. No physical-device validation, runtime completion, cache-performance improvement, parity, cutover, or release is established by this checkpoint. `artifacts.lock.json`, `.xcodebuildmcp/`, and `third_party/swift/.build/` remain outside its changes.

## Next checkpoint: remove unsafe output persistence

Read-only preparation identified two stale-output shortcuts: Bazel's incomplete UAPI persistence key in `bazel/feasibility/kernel/kbuild_persist.py`, and the `.orlix-headers-ready` timestamp shortcut in the owning Kernel Make rules. Checkpoint 2 removes both authorizations while retaining upstream incremental state and deterministic artifact serialization. It must prove stale-state rejection and semantic UAPI content identities before claiming the cache-correctness gate passes.
