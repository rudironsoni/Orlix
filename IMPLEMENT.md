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

The new [Bazel PR run](https://github.com/rudironsoni/Orlix/actions/runs/34515244045) installed GNU Make but failed before the matrix because Homebrew rejected the untrusted `xcodesorg/made/xcodes` formula. Rudi authorized tap trust. The repository's sole declared tap, `xcodesorg/made`, now has `trusted: true` in `Brewfile`; `brew trust --tap xcodesorg/made` completed locally with exit 0. `brew bundle check --file Brewfile --no-upgrade` and `ruby -c Brewfile` passed. The correction was published as `332704c12088980a7e64c8822a7e12a550a247bd`. Its [Bazel PR run](https://github.com/rudironsoni/Orlix/actions/runs/34516175684) passed provisioning and the matrix, including 28 freshly executed Bazel analysis tests. The preceding iOS 15 run was cancelled when the newer commit started its replacement; it is not recorded as a passing run.

The current TCTI scope envelope reports unverified completion and does not emit `physical_device_allowed`. Its checker validates scope only. The required executable eligibility check remains implementation work for the proof gate. Successful scope commands alone do not authorize physical-device validation.

TAP remains stopped. No physical-device validation, runtime completion, cache-performance improvement, parity, cutover, or release is established by this checkpoint. `artifacts.lock.json`, `.xcodebuildmcp/`, and `third_party/swift/.build/` remain outside its changes.

## Checkpoint 2: remove unsafe output persistence

Read-only preparation identified two stale-output shortcuts: Bazel's incomplete UAPI persistence key in `bazel/feasibility/kernel/kbuild_persist.py`, and the `.orlix-headers-ready` timestamp shortcut in the owning Kernel Make rules. Checkpoint 2 removes both authorizations while retaining upstream incremental state and deterministic artifact serialization. It must prove stale-state rejection and semantic UAPI content identities before claiming the cache-correctness gate passes.


Status: verified locally. Publication and current-head CI remain separate checks.

The old Bazel helper accepted an owned test directory containing a bogus `linux/unistd.h` and invalid archive bytes when its `archive-v2` stamp matched the Linux revision, tag, and Xcode build. The executed legacy `can_reuse` check returned true. The fixture and its byte digests are recorded in `Build/AgentHarness/bazel-migration/recovery-checkpoint-2/stale-state.json`.

Independent review also found that the source Make path retained installed and staging headers through `resume_headers`. Removing only the ready-stamp early return does not close that path. This checkpoint must remove both output shortcuts, prove stale-header removal, run a real upstream UAPI build with the poisoned legacy persistence directory present, and verify the shared semantic digest with the actual action interpreter.


The source Make regression fails against the prior committed rule because obsolete output survives, and passes after the cleanup change. The fixture invokes the real owning Make recipe with a minimal upstream install fixture; it proves cleanup and preservation of unrelated Kbuild state, not Linux runtime behavior. The post-edit `gmake __bazel-matrix-check` passed with 113 Python tests and 28 cached Bazel analysis tests.

A fresh `gmake __bazel-kernel-uapi` used an isolated output base and initially empty disk cache while the poisoned legacy persistence directory remained configured. The first upstream build succeeded, but Make correctly failed the new semantic digest check: the action hashed modes 0644/0755 before Bazel materialized the outputs as 0555. The action now sets the delivered modes before hashing. The retry exited 0, executed one local header-install action, and verified digest `5664459c80f50b46ea399fd4d0e2c37154ba6ff7f96ec281e31aaf5f74e128a0`. Its actual header tree matches the clean checkpoint 1 baseline in paths, bytes, modes, and symlink targets. The poisoned fixture remains unchanged.

The one shared digest implementation serves UAPI artifact identity and promotion tree comparison. It works with both system Python 3.9.6 and repository PATH Python 3.14.7. Removing the old persistence API does not remove upstream Kbuild incremental state. Kernel source-preparation cost and broader foreign-build incremental reuse remain checkpoint 3 work.


`gmake __bazel-mlibc-from-uapi` exited 0 using the checkpoint's explicit isolated output base. Its headers, libraries, complete sysroot, compiler-rt archive, loader, and ABI manifest match the clean baseline. The initial comparison used `bazel-bin` after the matrix had repointed that link to an older output base; repeating against the explicit checkpoint output directory removed that measurement error. Evidence always names that explicit directory in `mlibc-baseline-comparison.json`.

The final `gmake __bazel-matrix-check` exited 0 with 113 Python tests and 28 cached Bazel analysis tests. Migration inventory checks and diff whitespace checks passed. Raw commands, output roots, failed first attempt, regression results, and artifact comparisons are recorded in `Build/AgentHarness/bazel-migration/recovery-checkpoint-2/verification.json`.

This checkpoint proves removal of the unsafe UAPI final-output reuse paths and unchanged semantic UAPI/mlibc products. It does not establish Kernel Mach-O incrementalism, the full mutation matrix, current promoted acquisition behavior, runtime proof, Apple packaging parity, or cutover. TAP remains stopped and `artifacts.lock.json` remains unchanged.

The final legacy-persistence test uses the exact original `ORLIX_KBUILD_PERSIST/headers_install` lookup. The first fixture layout had tested the helper directly one directory above that lookup, so it was corrected before final proof. The old lookup accepts the corrected poisoned fixture. A fresh output base and empty disk cache then produced real upstream headers with one local action, exit 0, matching the clean baseline while leaving every poison byte unchanged. The final log is `uapi-exact.log`, and `verification.json` names its explicit output root.


## Checkpoint 3A: deterministic Kernel archive inputs

Status: compiler path and version changes verified locally. Full Kernel incremental-state recovery remains open.

The initial clean Kernel archives contained 500 temporary source paths and two wall-clock banner timestamps. The native compiler now maps prepared-source and build-directory paths to stable paths. Version generation uses the declared deterministic timestamp and build number, and leaves its header untouched when the bytes are unchanged.

Two isolated Kernel builds exited 0 in 518.72 and 538.70 seconds, compiling 918 product source objects each. The second disabled action reuse, disk cache, and remote cache. Their complete output trees match in paths, bytes, modes, and symlink targets with digest `e61eb44ff71602e79d79b3bee5768cc38058668be9daef77b4db046b6507ee11`. Both archives have SHA-256 `f96ef88aaba316b4d09d02d09e99031e18a009b0008ef77ff9f665887d99622b`, no temporary-path strings, and the two declared timestamps. No binary normalization was used. These builds used the pending declared ISA input graph; that graph's maintainer integration is a separate checkpoint. Inputs stayed unchanged during both runs.

Review found that the five nested Kbuild calls also need the fixed build number. They now pass it with the existing timestamp, user, and host values. An actual upstream `init/Makefile` fixture with persistent counters 17 and 700 produces different headers without that setting and identical headers with it. This follow-up occurred after the two full builds; its fixture and the existing archive-cache tests passed. The prior build-graph check passed 113 Python tests and 28 cached Bazel analysis tests.

Evidence is under `Build/AgentHarness/bazel-migration/recovery-checkpoint-3/`: `kernel-reproducibility.json`, `prefix-map-fixture.json`, `kbuild-version-fixture.json`, `repro-a.log`, `repro-b.log`, `repro-b.execution.json`, and the archive-cache test logs. This proves the bounded compiler/version correction. Stable Kbuild state, cache invalidation locality, developer-loop improvements, full runtime proof, and cutover remain open. TAP stays stopped and `artifacts.lock.json` is unchanged.


## CI checkpoint: separate iOS 15 build and test deadlines

The iOS 15 run `34519606349` timed out in the combined 45-minute Make step. The test build succeeded at 20:19:43 UTC, app validation passed at 20:19:49, and the test runner started at 20:20:28. The step was killed at 20:20:31. This produced no completed runtime result.

The existing source gate now has private build and test phases. The public `make ios15-simulator-gate` runs them in order under the current source authority. CI gives the build 60 minutes and the test phase 10 minutes within the unchanged 90-minute job. The test phase retains app validation, `test-without-building`, and the existing 120/180-second XCTest limits. Bazel routing and cutover status remain unchanged.

Verification: 38 focused routing, workflow, and iOS 15 unit checks passed. The new routing case executes Make dry runs to verify phase separation, ordering, app validation, and XCTest timeout flags. `make __bazel-matrix-check` exited 0 with 114 Python tests and 28 cached Bazel analysis tests. Inventory generation and checks passed against the exact staged source snapshot. Remote runtime validation is pending the next CI run; no local simulator or device execution was performed for this change.


## Checkpoint 3B: declared ISA inputs

Status: maintainer generation, source consumption, and sandboxed Bazel extraction verified locally.

Kernel compilation now consumes the extracted ISA tree. The archive, pin, member declaration, and serializer belong to the separate extraction action. The canonical member list comes from the existing contributor Make module. Source Make uses the same checked extractor; the obsolete mutable restore path is removed from Make, CI, and Xcode.

`make prepare type=tcti-isa` completed with exit 0 against the pinned external Arm inputs in a fresh output root. The existing importer, feature, register, publisher, and full refresh transaction tests passed. All ten published files match the newly selected immutable generation and the previous table contents byte for byte. The archive now excludes AppleDouble entries and uses fixed metadata. Its SHA-256 is `d362cb636638754122e7198a6c9de7c4c76253bcc3ba248c3ba0ba42a258120b`. The component buildset lock did not change.

Two source prepare runs returned 0, retained unchanged file timestamps, and preserved unrelated generator state. The first real Bazel extraction exposed its precreated empty output directory; the extractor now accepts that directory while rejecting symlinks and nonempty destinations. The retry executed the sandboxed extraction successfully. Existing tests cover this output shape. Independent source review found no remaining ISA integration blocker.

Earlier actual pin mutations proved that a whitespace-only pin change reruns extraction while reusing the unchanged Kernel result, and an invalid digest fails before compilation. Those mutations were restored. The full current build-graph check and final focused serializer check are recorded in `Build/AgentHarness/bazel-migration/recovery-checkpoint-3/isa-matrix.log` and `isa-final-check.log`. The maintainer and source-output comparisons are in `isa-refresh-result.json` and `isa-source-prepare.json`.

This checkpoint corrects declared inputs and publication. Stable Kernel build state and measured incremental recompilation remain the next phase 3 work. TAP remains stopped.


## Checkpoint 3C: retain Kernel build state

Status: verified locally. The full recovery remains in progress.

The Kernel source action retains its prepared source and upstream Kbuild output beneath the worktree's Bazel output base. Preparation patches only the touched files in temporary staging and replaces materialized source only when bytes or modes change. Kbuild owns its generated headers, host tools, initramfs, and console map. The native projection checks each object's compiler command and dependency file. A source edit no longer deletes Kbuild output or recompiles every native object.

The action identity includes declared source, overlays, patches, configuration, ISA artifacts, build rules, and observed tool identities. The incremental directory uses the worktree output base, profile, and destination; source digests do not select a fresh directory. State compatibility includes the generated action script, Kbuild rules, and full tool identity. Before reuse, the state record checks actual output content and modes. Compiler-cache identity excludes unrelated archive-processing tools. The prepared-source digest is computed from current inputs and supplies the embedded proof identity without an undeclared Git read.

A file lock covers preparation, Kbuild, native compilation, and state recording. Symlinked state ancestors, malformed records, and corrupted object bytes cannot authorize reuse. The existing seven focused tests cover unchanged-file timestamps, same-size edits with preserved source timestamps, fresh-tree equivalence, lock exclusion, corruption, and path redirection. GNU find is now a declared host prerequisite because upstream Kbuild uses its `-printf` operation. No guest package was added.

Measured results for the release simulator Kernel:

| Workload | Wall time | Native compilation requests | Upstream object/tool compilation requests | Reuse |
| --- | ---: | ---: | ---: | --- |
| Seed with compiler cache enabled | 620.87 s | 918 | 51 | Fresh Kbuild state |
| No source change | 1.30 s | 0 | 0 | Bazel action result |
| Kernel `time.c` constant change | 208.99 s | 2 | 0 | 916 native objects retained |
| Restore original source | 1.72 s | 0 | 0 | One Bazel disk-cache hit |
| Independent caches-disabled build | 516.21 s | 918 | 51 | No build-result, compiler, or incremental reuse |

The mutation preserved source length and mtime. Only `time.c` and the embedded Kernel proof registry required native compilation; downstream composition outputs changed as expected. The source was restored exactly. Compiler-cache counters across the mutation showed two misses and 209 direct hits, including Kbuild configuration probes. These counters are machine-wide; the action logs and object snapshots establish the bounded object-reuse claim. Cached action stdout is retained in raw logs but is not counted as current compiler execution.

The independent build disabled action, disk, remote, compiler-cache, and persistent Kbuild reuse. The existing full-tree comparator checked paths, file bytes, modes, and symlink targets. Both output trees have digest `71dc84a37e6179b39090c16e907810b1ebea2de8394b780cfb9e31d30bbc20fb`; both archives have SHA-256 `ac0fa15c5b06603aee06aefea5f12912c7afc9ced9f1fee9db7ff71160c5abe7`. All implementation input hashes remained unchanged during these builds. No binary normalization was used.

The retained Kernel state occupies 2,213,404 KiB after the seed and 2,221,052 KiB after the mutation, an increase of 7,648 KiB. The compiler cache is shared and bounded to 20 GB; prepared sources and mutable build state remain local to this worktree. These measurements do not establish the full developer-loop disk comparison or cross-worktree Kernel performance. The cache-enabled seed was slower than the cache-disabled full build; the demonstrated improvement is avoiding 916 native compilations on the measured edit. Batch-build maximum RSS was 1,380,433,920 bytes. The persistent-server client timing does not measure total build memory.

The full graph check passed 119 Python tests and 28 cached Bazel analysis tests. Archive contract checks, migration inventory, documentation checks, 13 repository hook tests, generated-hook checks, and diff whitespace checks passed. Independent source review found no remaining checkpoint 3 blocker. Evidence is under `Build/AgentHarness/bazel-migration/recovery-checkpoint-3/`: `kernel-locality-summary.json`, `kernel-final-seed2.json`, `kernel-mutation.json`, `kernel-cache-equivalence.json`, their execution/BEP/profile logs, and the `kernel-final-*` check logs. Earlier failed launcher and generator attempts remain in their original logs.

This checkpoint proves Kernel incremental object reuse and real cache-on/cache-off product equivalence for the tested profile and destination. The full mutation matrix, mlibc/package incremental state, promoted reuse, SDK graph, runtime ladder, Apple parity, and authority cutover remain open. Phase 4 is next. TAP remains stopped and `artifacts.lock.json` is unchanged.


## Checkpoint 4A: separate the compiler runtime artifact

Status: compiler-rt action boundary verified locally. Meson/Ninja incremental state remains the next phase 4 change.

The existing compiler-rt loop is now a separate Bazel action producing `libcompiler_rt.a`. Its inputs contain the 34 selected source files, builtin headers, and a specific compiler-runtime tool identity. That identity covers Xcode clang, compiler resources, llvm-ar, recursively linked Homebrew libraries, and host identity. The mlibc action consumes the archive without inheriting compiler-rt sources or its tool-identity record. No compilation flags or source behavior changed in this checkpoint. Both affected actions disable remote cache reads and writes.

The pre-change build executed installed-UAPI work in 132.593 seconds and mlibc in 23.827 seconds, with 739 Ninja compilation requests. After separation, the runtime action executed in a Darwin sandbox in 1.589 seconds and produced the same 44,524-byte archive, SHA-256 `a40d3fda08ee68dc4161290a335118d83f4273207a9ad16bb1aa7cb0f548c959`. The complete mlibc output tree still has digest `0134adcf6b86d753c22fd70959528ae9d770e5db28a7b58c76f9c10fc053e69e`, verified against a preserved independent baseline tree by paths, bytes, modes, and symlinks.

A temporary implementation change in the existing wait3 patch preserved the patch's size and mtime. The build executed only `OrlixMLibCSysroot`, changed `libc.a`, and retained the identical compiler-runtime archive and installed UAPI. It took 25.49 seconds and still compiled all 739 Ninja objects. The patch was restored exactly, and cache restoration returned the full baseline output tree. Adding the runtime tool-identity output did not invalidate Kernel compilation: a Kernel build completed in 1.55 seconds with zero executed actions. These results establish the new artifact boundary, not mlibc incremental compilation.

The full graph check passed 119 Python tests and 28 cached Bazel analysis tests. The existing sysroot analysis case now verifies both action input sets and the archive-only dependency. Results are recorded under `Build/AgentHarness/bazel-migration/recovery-checkpoint-4/` in `mlibc-before.json`, `runtime-split.json`, `runtime-split-mutation.json`, and `kernel-after-runtime-split.json`, with their action logs. Build-event collection now uses an explicit non-secret client environment; older local command-line events containing inherited client environment were removed without changing build output or action-result records. No raw reports were committed or published.

Next: preserve exact mlibc preparation and Meson/Ninja state, apply the declared guest build configuration and subproject patches correctly, and verify actual partial recompilation and cache equivalence. Compiler-cache integration for this guest path, the full recovery matrix, runtime proof, and cutover remain open. TAP remains stopped and the signed buildset lock remains unchanged.


## Checkpoint 4B: honor declared mlibc build inputs

Status: source preparation and compiler configuration verified locally. Persistent Meson/Ninja state remains open.

Bazel previously copied subprojects directly into Meson's working source tree. Meson treats an existing subproject with its build file as already prepared, so the declared frigg `diff_files` were skipped. The action now exposes its declared immutable source trees through Meson's standard `MESON_PACKAGE_CACHE_DIR`. Meson copies selected sources into its working tree and applies their existing wrap patches. This uses upstream source-preparation behavior without a custom patch engine or additional guest package.

The previous bare `arch-defs.hpp` include directory did not override `<mlibc/arch-defs.hpp>`, so upstream's 4 KB constant remained selected. The action now uses the explicit C++ forced include already present in the owning OrlixMLibC Make rules. C/C++ and compiler-rt use the owning fixed-x18 flags, and C/C++ retain the owning function/data section flags. Review also removed manual freestanding include paths that bypassed prepared header trees. Meson supplies those include dependencies, and configuration forces the four declared subprojects while retaining download prohibition. No upstream source or patch content changed.

An actual AArch64 compiler fixture rejects a 16 KB static assertion with the previous include path and passes with the owning Make forced include. The final real build returned 0 in 24.43 seconds. Meson reported applying `frigg-long-double-format.patch` and `frigg-hex-alt-prefix.patch`; the final mlibc action executed while compiler-rt and installed UAPI remained reused. The resulting complete tree digest is `8995cb3d6da6f55ea046c8b0da8727b3890bcc0589557a6579ba4f0f8f54a070`. The compiler-runtime archive has SHA-256 `24fc4cbb20a3c243a677398f211d56ba84503cc7c2c31db7e357e615de7bbac8`, and `libc.a` has SHA-256 `3135b8bf45c0adcdc63492b659bf5bf3b6e20b0336eacadde06f64ded4bf189d`.

The full graph check passed 119 Python tests and 28 cached Bazel analysis tests. The existing sysroot case checks the package-cache, forced-include, and fixed-register configuration. Source/compiler checks identify the correction; no binary-debugging or runtime claim is made. Evidence is recorded in `page-size-include-fixture.json`, `mlibc-prepared-final.json`, and their build/check logs under `Build/AgentHarness/bazel-migration/recovery-checkpoint-4/`.

Next: retain Meson/Ninja state with exact input and output integrity, then prove partial object recompilation and full cache equivalence. These corrected unsigned source products do not update `artifacts.lock.json`, complete runtime proof, or authorize cutover. TAP remains stopped.


## Checkpoint 4C: retain checked mlibc Ninja state

Status: verified locally. Coreutils/package incremental state, the public product graph, and the full recovery remain open.

The mlibc action now retains its prepared source, installed UAPI, compiler runtime, Meson configuration, and Ninja objects beneath the worktree's Bazel output base. The directory identity is `aarch64-linux-gnu`; source changes do not select a new directory. Preparation replaces only changed source/header content and preserves unchanged timestamps. Meson still prepares the declared subprojects and applies their wrap patches. Changed UAPI or subproject inputs clear Meson's configuration checks. A changed compiler-runtime digest changes the link command without changing the build directory.

Kernel and mlibc share the existing content-sync, lock, and state-verification implementation, now in `bazel/build_state.py`. The entire mutable operation stays under one file lock. State reuse checks actual file bytes, executable modes, symlink targets, and mtimes. Invalid records, changed object bytes, future-dated objects, metadata directories, and redirected metadata cannot authorize stale output. The existing failure test covers these cases and preserves unrelated external bytes. A real future-mtime/source-mutation build rejected its previous state, invoked 739 Ninja compilation tasks, and produced the changed object in 20.666 seconds. Its 743 compiler-cache hits include compiler setup checks.

Action identity includes the declared upstream source, patches, wrap inputs, installed UAPI, compiler-runtime archive, build script, state modules, and observed tool identities. Persistent-state compatibility includes the full tool identity, the separate compiler identity, build rules, and selected launcher. Source and dependency content are reconciled before Ninja runs. The mlibc tool identity records 215 executable/library files and six resource/module trees, including Xcode compiler resources, host SDK inputs, Meson and Python modules, Ninja, link/archive tools, system action tools, and Python's transitive native-library dependencies. The compiler cache uses the narrower guest compiler identity plus normal compiler source/header/flag inputs. It is shared and bounded at 20 GiB. Mutable source, configuration, and object databases remain worktree-local.

The compiler-runtime action remains independent and now uses the compiler cache in developer mode. Promotion disables both compiler-cache and mlibc incremental reuse. Native and guest file-prefix mappings make output independent of temporary or worktree paths. The private generated build script remains outside the component product directory.

Measured mlibc results:

| Workload | Bazel elapsed time | Ninja compilation requests | Observed reuse |
| --- | ---: | ---: | --- |
| Previous wait3 source mutation | 25.49 s | 739 | Fresh Meson/Ninja build |
| Incremental mutation with warm compiler objects | 9.448 s | 2 | Two compiler-cache hits |
| Fresh source mutation after state verification | 9.952 s | 2 | Two compiler-cache misses, other objects retained |
| Restore the original source | 1.442 s | 0 | Bazel disk-cache result |
| Final independent cache-enabled output base | 49.315 s | 739 | UAPI/runtime disk-cache hits; 743 compiler-cache hits |
| Final independent cache-disabled output base | 162.149 s | 739 | UAPI, runtime, and mlibc actions executed locally |

The real mutation preserves source length and mtime. Only the two static/shared `sys-wait.cpp` objects changed among 747 recorded object files. All patch bytes and timestamps were restored. Actual action execution records distinguish current compilation from cached stdout. Retained mlibc state measured 209,408 KiB before one mutation and 209,276 KiB after it. This checkpoint trades bounded local state for incremental work; it does not claim lower total machine disk use. The full temporary-disk and multi-worktree benchmark matrix remains checkpoint 9 work.

The final independent output bases used the current rules. The cache-disabled build disabled action, disk, remote, compiler-cache, and persistent Kbuild/mlibc reuse. The existing comparator checked all eight component outputs, including file paths, bytes, modes, symlink targets, headers, libraries, loader, ABI, and digest records. Both complete trees have SHA-256 `d4e65eb82b0781956ce4d17b2bb65c6f4d1fb54088ee0b906be99be19f6df06b`. The compiler-runtime archive remains `24fc4cbb20a3c243a677398f211d56ba84503cc7c2c31db7e357e615de7bbac8`; libc.a is `3a192997962c23c1518e93ecd188b0c5a952cbaedd384cee3a1837779f33c004`. Wall-clock durations including batch startup/shutdown were 50.98 and 163.72 seconds. The cache-disabled batch peak memory footprint was 1,762,101,408 bytes.

The full `make __bazel-matrix-check` passed 119 Python tests and 28 cached Bazel analysis tests. Documentation, migration inventory, diff checks, all 13 lifecycle-hook tests, and generated-hook checks passed. Independent source review found and rechecked the timestamp, metadata, and tool-identity corrections. This checkpoint does not run or claim Linux runtime, TAP, simulator, device, packaging parity, or cutover proof. TAP remains stopped, `artifacts.lock.json` is unchanged, and the original dirty `AGENTS.md` delta and local caches stay outside the commit.

Raw commands, failed initial attempts, execution logs, object snapshots, cache counts, and final independently preserved products are under `Build/AgentHarness/bazel-migration/recovery-checkpoint-4/`. `ninja-final-equivalence.json`, `ninja-reviewed-mutation-result.json`, `ninja-future-result.json`, and the matching action records identify the verified results. The checkpoint's reports were captured with a minimal client environment.


## Checkpoint 5A: retain checked Coreutils Make state

Status: Coreutils incremental compilation and cache equivalence verified locally. The remaining package boundaries and full recovery remain open.

Coreutils now retains its source, configure state, Make dependency files, and objects under the worktree's Bazel output base at `orlix-package-state/coreutils/aarch64-linux-gnu`. Source edits do not select a new directory. The existing shared state implementation locks the complete operation and verifies recorded bytes, executable modes, symlinks, and timestamps before reuse. Promotion disables package incremental state and the compiler cache.

Preparation tracks declared source identities separately from upstream-generated files. It replaces changed declared inputs and removes deleted declared inputs without rewriting valid generated source. A failing regression exposed identical readonly generated headers being rewritten and triggering four unrelated compilation requests. The corrected existing sync operation preserves these files and their timestamps. The package test covers source edits with preserved timestamps, readonly upstream regeneration, compiler-runtime changes, and `config.hin` changes. Shared Kernel-state tests retain coverage for corrupt bytes, timestamps, records, locks, and redirected paths.

Action inputs include source, installed UAPI headers, mlibc headers and libraries, compiler runtime, the curated program list, build rules, and observed tools. Unused UAPI/provenance digest files are no longer compilation inputs. Persistent compatibility includes build scripts, state modules, full tool identity, guest compiler identity, and launcher selection. Configure identity covers actual Autotools inputs, including `lib/config.hin`, and consumed dependency content. Ordinary source changes preserve Make's object graph.

The tool identity records 184 executable/library files and nine resource trees. It includes selected GNU Make, Autoconf/Automake and their modules, the selected versioned Automake/aclocal binaries, GNU grep/sed/awk/tar/ln, Xcode compiler and SDK inputs, LLVM tools, Python, and native tool dependencies. The compiler cache uses the narrower guest compiler identity plus compiler inputs and flags. It remains shared and bounded to 20 GiB. GNU awk, tar, and grep are now declared host prerequisites in `Brewfile`; no guest package was added.

Both configuration and compilation use the declared GNU Make. The curated 103 program targets now run in one Make invocation through upstream `GNUmakefile`, retaining its `Makefile`, `cfg.mk`, and `maint.mk` rules. The added prerequisite file describes only externally supplied link artifacts. It does not model internal Coreutils objects. Installed copies are stripped; upstream build products remain available for incremental linking. File manifests use relative paths.

Actual cache comparison exposed a compiler-path difference in 37 binaries. For example, `cat` embedded `src/src/cat.c` without ccache and `../src/src/cat.c` with it. A real compiler fixture verified the additional file-prefix mapping. The final installed tree matches the pre-change install tree by paths, bytes, modes, and symlinks, with digest `6b730b9305b5bf7a4b3b79de5cbdaa4e28eafb032efa33b19a41deb41e2cf148`. No binary normalization was used.

Measured results:

| Workload | Bazel elapsed time | Coreutils compilation requests | Coreutils links | Observed reuse |
| --- | ---: | ---: | ---: | --- |
| Previous rule, action miss | 255.746 s | 508 | 103 | Fresh configure/Make tree |
| Retained state, before batching | 26.972 s | 1 | 1 | Other 507 objects retained |
| Final warm source edit | 8.787 s | 1 | 1 | One compiler-cache miss; other 507 objects retained |
| Restore original source | 1.416 s | 0 | 0 | One Bazel disk-cache hit |
| Warm no-op | 0.492 s | 0 | 0 | No executed build action |
| Final cache-enabled clean state | 239.884 s | 508 | 103 | Compiler-cache reuse, including configure probes |
| Independent cache-disabled build | 369.526 s | 508 | 103 | UAPI, compiler runtime, mlibc, and Coreutils rebuilt |

The final edit changes the `yes.c` program-name constant in a separate source fixture while preserving its size and timestamp. Only `yes.o` changed in bytes or mtime among 508 objects. Only the Coreutils action executed; Kernel, UAPI, mlibc, and compiler-runtime actions did not execute. The first final edit also paid for Bazel's analysis-cache reset after the test configuration, so its 31.360-second total is retained separately from the warm measurement. Its actual package action took 8.395 seconds. Every fixture edit was restored, and the recovered product again matches the preserved original product.

The independent build disabled action-result, disk, remote, compiler-cache, Kernel, mlibc, and package incremental reuse. Execution records confirm local UAPI, mlibc, and Coreutils work and sandboxed compiler-runtime compilation. All four actions succeeded without cache hits. The complete cached and uncached product trees match with digest `50f392f6991b3c6bbf96a2a2195ef68a58aeab8fc4a08912e91f424ce3fca4fa`, including manifests and the curated program list. The clean proof builds partly overlapped, so their timings do not establish an isolated cold-build speed comparison. The cache-disabled batch peak memory footprint was 1,265,485,648 bytes.

Retained Coreutils state measured 244,416 KiB after the final seed and 241,840 KiB after the edit. This adds disposable local state to avoid cold rebuilds; it does not claim lower total disk use. The full disk, download, cross-worktree, and developer-loop matrix remains checkpoint 9 work.

Verification passed: 120 Python tests, 28 cached Bazel analysis tests, build prerequisites, documentation and inventory checks, 13 lifecycle-hook tests, generated-hook checks, and diff checks. Independent source review rechecked the configuration inputs, selected tools, generated-file preservation, upstream Make entry point, and batched build. Raw commands, failed checks, action records, object snapshots, counters, and independent products are under `Build/AgentHarness/bazel-migration/recovery-checkpoint-5/`. `coreutils-summary.json`, `coreutils-warm-mutation-result.json`, and `coreutils-cache-equivalence.json` identify the final evidence.

Next in phase 5: preserve the other package engines' incremental state, remove fake Autotools generator commands, and restore the seven existing guest library dependencies and corresponding Coreutils features missing from the Bazel graph. This checkpoint does not claim package-feature parity, TAP, Linux runtime, Apple product readiness, or cutover. TAP stays stopped, `artifacts.lock.json` is unchanged, and the original dirty `AGENTS.md` change and local caches remain outside the commit.

## Checkpoint 5B: retain isolated Bash Make state

Phase 5: IN PROGRESS. 5A Coreutils: verified. 5B Bash: verified locally. 5C remaining packages/features: pending. 5D rootfs and `artifact-identity-v2`: pending. Publication is a separate final check.

Bash now retains its source, configuration, and upstream Make objects beneath `orlix-package-state/bash/aarch64-linux-gnu` in the worktree's Bazel output base. Coreutils uses its separate `coreutils/aarch64-linux-gnu` root. Each root contains its own `build.lock`; the real locks have different inodes. Successful package records include an explicit `compatibility` object with `package` and `target`. Compatibility also requires the script, state implementation, full tool identity, compiler identity, launcher, and verified retained file content. Missing or invalid records cause clean state preparation. An explicit foreign package owner raises `package state mismatch` before source preparation or upstream execution, preserving the foreign record rather than adopting it.

The bidirectional substitution regression invokes the actual package runner with identical script, compiler, and tool inputs. It creates both real state roots and locks, substitutes the Coreutils record into Bash and the Bash record into Coreutils, and asserts both calls reject the recorded owner before the upstream subprocess is called. The test also checks explicit record fields and distinct lock files. Three focused package tests pass. Existing Kernel-state tests still cover corrupt content, timestamps, records, and redirected metadata.

The Bash action consumes source, installed UAPI headers, mlibc headers/libraries, and the independent compiler-runtime archive. Unused UAPI/provenance digests are no longer compilation inputs. The dedicated Bash tool identity includes Autoconf, GNU Make, compiler and SDK inputs, LLVM tools, GNU utilities, and their module/native-library dependencies. It does not inherit unrelated Automake/Gettext resource trees. The shared compiler cache remains bounded to 20 GiB; mutable state remains worktree-local. Remote action caching and execution remain disabled for this foreign action. Promotion disables compiler and retained-state reuse.

Real configure and GNU Make remain authoritative. Ordinary C edits preserve configuration and object state. The private build script stays outside the product tree. The upstream version counter starts at zero so cache removal cannot change the shipped build version. Prefix mappings cover root, builtins, and library compilation depths. Only installed copies are stripped, and manifests use relative paths.

Source review found omitted upstream generator prerequisites. The existing Make prerequisite fragment now relates `version.h` to `support/mkversion.sh`, and both the top-level builtins archive and recursive `pipesize.h` to `builtins/psize.sh`. `MAKEFILES` passes these prerequisites to recursive Make without adding competing recipes. Separate same-size/same-mtime generator mutations produced the changed generated headers through actual upstream recipes. Both source fixtures were restored exactly. No generated upstream source was edited.

| Workload | Bazel elapsed time | Actual compilation requests | Links | Observed reuse |
| --- | ---: | ---: | ---: | --- |
| Final rules, clean Bash state | 77.599 s | 166 | 1 | Compiler-cache reuse; unrelated components reused |
| Final fully cache-disabled build | 264.221 s | 166 Bash requests | 1 Bash link | UAPI, compiler-rt, mlibc, and Bash executed without cache hits |
| Final C-only source mutation | 3.904 s | 4 | 1 | One compiler miss and three preprocessed cache hits |
| Restore original source | 1.022 s | 0 | 0 | One Bazel disk-cache result |
| Warm no-op | 0.868 s | 0 | 0 | No executed build action |

The final C-only mutation changes `lib/sh/itos.c` in a separate source fixture while preserving its size and timestamp. Only `itos.o` changes bytes among 166 Bash objects. The action takes 3.143 seconds; its single link consumes the changed archive. Upstream's missing out-of-tree `parser-built` file causes three parser-related compilation requests whose contents hit the compiler cache. The other 162 objects remain untouched. This is four compiler requests, not a claim of one request. Execution logs contain only the Bash action; UAPI, mlibc, compiler-rt, and Coreutils work stays reused. Kernel compilation is outside this package graph and is not invoked. Final retained Bash state measures 68,752 KiB; the preceding clean state measured 68,824 KiB. These component measurements do not establish the full cross-worktree, configuration, integrity-hashing, or disk matrix required in phase 9.

The restored complete Bash product matches the preserved original and final independently built cache-disabled product by paths, bytes, modes, and symlink targets, with digest `d4ab93dd791fae6b7d73a4ab1f219b7ec651ec97d147bc8cdff6ed5b587357f8`. Coreutils remains byte-identical with complete tree digest `50f392f6991b3c6bbf96a2a2195ef68a58aeab8fc4a08912e91f424ce3fca4fa`. The independent command disabled action, disk, remote, compiler-cache, Kernel, mlibc, and package retained-state reuse. Its four real actions all returned zero with no cache hits. Peak memory footprint was 1,602,897,912 bytes. Clean proof work partly overlapped other checks, so these durations are not an isolated cold-build speed comparison. `bash-final-equivalence-5b.json` records the products, action results, actual package records, and distinct lock identities.

The RuleSync source now permits non-executable implementation modules called by private Make targets or Bazel actions, matching ADR 0033. Make remains the public developer/CI interface; repository wrapper Makefiles remain prohibited inside Bazel actions. Generation updated the Codex, Claude, Cursor, and Copilot rule bodies. The canonical rules check passed before the exact original user-owned duplicate XcodeBuildMCP bullet was reapplied to `AGENTS.md`. That bullet remains unstaged and outside the checkpoint.

Final verification passed: 122 Python tests, 28 cached Bazel analysis tests, documentation generation/checks, migration inventory, 13 lifecycle-hook tests, and generated-hook checks. The final matrix reused all 28 passing analysis results; those results are not runtime proof. Real Bash builds separately exercised the final generator-rule correction. All build and test jobs completed with exit zero before publication.

Raw evidence is under `Build/AgentHarness/bazel-migration/recovery-checkpoint-5/`: `bash-isolation-unit.log`, `bash-final-matrix-5b.log`, `bash-final-leaf-result-5b.json`, `bash-final-before-objects-5b.json`, `bash-psize-mutation.execution.json`, `bash-version-mutation.execution.json`, and the matching seed, restore, no-op, and independent cache-disabled logs. Initial sandbox process-inspection failure and intermediate generator-rule checks do not substitute for the final checks. Independent review passed the final bounded source change; it did not run the build tests.

No TAP, Linux runtime, device, package-feature parity, promoted-store, or cutover claim is made. TAP stays stopped. `artifacts.lock.json`, the original user-owned `AGENTS.md` delta, `.xcodebuildmcp/`, and `third_party/swift/.build/` remain unchanged by publication. Phase 5C must restore the seven existing guest libraries/features; phase 5D must enforce install-tree-only rootfs assembly and `artifact-identity-v2`. Shared promoted storage remains phase 6 work.


## Checkpoint 5C1: retain generic upstream package state

Phase 5: IN PROGRESS. 5A Coreutils and 5B Bash are verified and pushed. This checkpoint verifies retained upstream builds for grep, jq, findutils, e2fsprogs, curl, ncurses, and zsh. Restoring the seven missing guest libraries and features remains 5C work. The rootfs boundary and `artifact-identity-v2` remain 5D work. Phase 6 has not started.

The existing package runner now supports each package's real configure/Make engine, including jq's in-source build. Each package retains a distinct worktree-local root and lock, explicit package compatibility, declared-source reconciliation, and checked retained content. Recipe, rule implementation, tool identity, compiler identity, and launcher determine state compatibility. Source edits retain the directory. Configuration inputs include actual Autotools files and auxiliary scripts, consumed headers, libraries, compiler runtime, and selected dependency content. Future-dated retained files are rejected even when the saved record contains the same timestamp.

Actions consume semantic headers and libraries without unused UAPI or provenance digests. A separate zsh input-selection action exports only ncurses headers, libncursesw.a, and libtinfo.a. Changes to other ncurses libraries can execute selection without invalidating zsh compilation. External link prerequisites supplement upstream Make recipes without replacing its object dependency graph. Dedicated tool identity includes the selected GNU tools and their native dependencies. Mutable source, configuration, objects, and locks remain beneath the worktree's Bazel output base. Shared compiler cache remains bounded to 20 GiB; foreign remote action caching and execution remain disabled.

Real pinned Autotools commands replace fake generator commands. Actual compilation exposed bundled Oniguruma's old C definitions after configure selected C23; jq now declares GNU C17. Cache comparison exposed absolute source paths in e2fsprogs and ncurses products. Relative configure invocation fixes their production paths. Ncurses now creates deterministic archives through upstream Make's ARFLAGS=crD, replacing its timestamp-preserving U flag. No generated upstream tree was edited and no output normalization hides differences.

| Workload | Bazel elapsed | Actual work and reuse |
| --- | ---: | --- |
| Final seven-package seed | 336.673 s | Completed package builds |
| Independent fully cache-disabled build | 492.533 s | Eleven actions, all exit zero, no action-cache hits |
| grep C edit | 5.430 s | One compilation miss and one link; only grep.o changed among 135 objects |
| jq C edit | 4.806 s | Two compiler requests: jv_aux.o miss and main.o direct hit; libjq and jq linked; other 68 objects untouched |
| Unused ncurses form-library edit | 6.593 s | One compilation miss and form archive update; selection output unchanged; no zsh build |
| jq ltmain.sh edit | 28.126 s | Real configure regenerated libtool with changed input |
| Restore all source fixtures | 1.138 s | Original products recovered |
| Warm seven-package no-op | 0.912 s | No package compilation |

The C mutations preserve source size and mtime in separate source fixtures. Grep changes only grep.o. Jq changes only jv_aux.o bytes, while both jv_aux.o and main.o timestamps change. Ncurses changes only fld_newftyp.o among 250 objects. The ncurses action and zsh input-selection action execute, but the selected interface and zsh product remain byte-identical. Unchanged UAPI, mlibc, compiler runtime, and unrelated packages do not execute for these source edits. Kernel compilation is outside this package graph. The auxiliary-file check also restores the prior jq C edit, so its compiler count does not establish auxiliary-only locality. All fixture bytes and timestamps were restored.

The complete canonical comparator checks paths, contents, modes, and symlink targets. Each restored product equals both its preserved original and the independent cache-disabled product:

| Package | Complete tree SHA-256 |
| --- | --- |
| grep | `be71927ac2f2a4d98854915f47abb1c0ce89d1fc29f5c220ff250e0dbda24bf4` |
| jq | `25bdad236876d48db76a43a8f91da8b7d8201a61da01f81f6eb4e35948999252` |
| findutils | `ea9e47f612339026c5564cc25103232b6c82d82327edaa27b2096b316d945808` |
| e2fsprogs | `d67f5e8742ac44e1d38533afafca272483913edd34ff62362e1a4e0c8a552f4e` |
| curl | `0a8a539d0a70b212cbf66ca12343cb602ae3f7bd1c63d2b1839d7924b4d742cc` |
| ncurses | `a46d24bb990e24e7b36699c286b5b1b8ea9ac9c848c59370ed22b3d9848b8524` |
| zsh | `8d8c967c45988651053af8b21e4580d375a88285ad87600fb9b1be2195ff3a01` |

The independent build disables action, disk, remote, compiler-cache, Kernel, mlibc, and package incremental reuse. Its eleven actions cover installed UAPI, compiler runtime, mlibc, seven packages, and selected zsh inputs. Peak memory footprint was 1,512,245,120 bytes. Seed and clean work overlapped other activity, so these durations do not establish an isolated cold-build speed comparison.

Final retained state in KiB: grep 57,816; jq 39,068; findutils 72,388; e2fsprogs 75,816; curl 66,056; ncurses 45,680; zsh 53,656. This checkpoint does not claim reduced total disk use. Cross-worktree, profile, disk amplification, and integrity-check cost measurements remain phase 9 work.

Verification passed: 122 Python tests, 28 cached Bazel analysis tests, documentation and migration inventory checks, 13 lifecycle-hook tests, generated-hook checks, and focused package/Kernel state tests. Independent bounded source review rechecked the tool identity, auxiliary configuration inputs, external link prerequisites, dependency selection, relative configure, and deterministic archive changes. The reviewer did not execute builds; the implementation session executed the reported checks.

Evidence resides under `Build/AgentHarness/bazel-migration/recovery-checkpoint-5/`. `autotools-checkpoint-summary.json` records products, actions, timings, and state sizes. `autotools-final-equivalence.json`, `grep-mutation-result.json`, `jq-mutation-result.json`, `ncurses-unused-archive-result.json`, and `jq-auxiliary-result.json` carry the detailed comparisons and mutation results. Matching execution logs distinguish real compiler work from replayed cached stdout. `autotools-final-matrix.log` and `autotools-final-checks.log` record the checks.

TAP remains stopped. No Linux runtime, package-feature parity, device, promoted-storage, or cutover proof is claimed. The signed artifacts.lock.json remains unchanged. The original duplicate XcodeBuildMCP bullet in AGENTS.md, .xcodebuildmcp/, and third_party/swift/.build/ remain outside this checkpoint.


## Checkpoint 5C2: restore guest libraries and features

Phase 5C2: ACCEPTED locally. Attr 2.5.2, ACL 2.3.2, PCRE2 10.47, musl-fts 1.2.7, libsepol 3.10, libcap 2.78, and libselinux 3.10 build through their upstream engines. Coreutils 9.11 consumes their selected headers and static archives with xattr, ACL, SELinux, and capability support enabled. The product proof verifies every declared archive, header, utility, feature macro, and linked symbol.

The complete install trees match an independent build by paths, bytes, modes, and symlink targets. SHA-256 values are attr `f2ef28f501df75c2dfe3ff37c473beb70d96df75d4bd6f47c39c82c11d1b79c1`, ACL `c76f67c3ea93a77a694092ecd1670b476e89312b4e568e6357d5171ce6b19cc3`, PCRE2 `1ac812d5efefe43d5d62b13db1540b173f11f5e6f2d2b7d18e830c515be19938`, musl-fts `4d4b020f6564c93a2400b7357bcaa4440710403264d31ae2a6503616179de4b3`, libsepol `ab140c96d30808c18e54c288a6990e07e18969cf933a9d43e6889a2f992f1de2`, libcap `ba8a95fb09609d9d41ccbc5cae78cadb50db50dfafb8b1f559989e71d9d884ce`, libselinux `e2e3f9bb6fcc1fb4057000fc467cd42c29564c5f35a75ee2fefb5f8d00ad1ddf`, and Coreutils `ac6d80650112b9f5d375d94810a4f93c66570c7ef0d70a120fc500781826c1d2`.

Seven isolated source mutations executed the expected upstream compile and link path. Attr rebuilt attr, ACL, and Coreutils. ACL rebuilt ACL and Coreutils. PCRE2, musl-fts, and libsepol each rebuilt that library, libselinux, and Coreutils. Libcap rebuilt libcap and Coreutils. Libselinux rebuilt libselinux and Coreutils. Their execution logs contain no Kernel, mlibc, compiler-runtime, or unrelated package action. The mutation pass exposed and fixed a make-only incremental link fault by keeping dependency headers as Make prerequisites while linking direct C utilities from `$<` instead of `$^`.

All fixtures were restored byte-for-byte. A final cache-free reset exited 0, and the restored products match the independent build. `make check-build-tools __bazel-attr __bazel-acl __bazel-pcre2 __bazel-musl-fts __bazel-libsepol __bazel-libcap __bazel-libselinux __bazel-coreutils` exited 0. `make __bazel-matrix-check` exited 0 with 32 passing analysis tests. Evidence is under `Build/AgentHarness/bazel-migration/recovery-checkpoint-5/`, including `phase5c2-mutation-*-isolated3.log`, `phase5c2-mutation-attr-isolated2.log`, their execution logs, `phase5c2-mutation-reset-final.log`, `phase5c2-final-products.log`, `phase5c2-final-matrix.log`, and `phase5c2-independent-equivalence.json`.

TAP stays stopped. The signed `artifacts.lock.json`, the original user-owned `AGENTS.md` delta, `.xcodebuildmcp/`, and `third_party/swift/.build/` remain unchanged. Phase 5D follows below. Phase 6 promoted-store work remains pending.

## Checkpoint 5D: rootfs and artifact identity

Phase 5D: ACCEPTED locally. Rootfs assembly consumes completed package install trees and the built `gen_init_cpio` tool. Its action inputs exclude package sources, configure files, package metadata, source digests, the Kernel archive, and wrapper Makefiles. Package build engines remain outside rootfs assembly.

`artifact-identity-v2` records the format and version, logical relative path, entry type, semantic mode, regular-file contents, and symlink target. Proof and dependency closure stay outside the artifact identity. The content namespace is `artifact-identity-v2/sha256/<digest>`, and the signed legacy lock remains unchanged. The artifact serializer no longer consumes compiler identity because compiler provenance does not affect the content-only result. The current `no-sandbox` exception remains required because retained Bazel sandbox evidence changes a regular tree entry into an absolute output-base symlink before serialization.

Two independent cache-disabled builds in separate output bases produced 23 identical v2 manifests and digest files. The rootfs identity is `f963fecfe44435d6dc46956040c256c7e1c72a26404ed5a07dbdb21117a861cb`. Product SHA-256 values are base ext4 `8048063bebdfec43e0dadce140855a13406b314a6a3b6e9572df528ca42c12d5`, initramfs `470aaf929675aaa79768fce6bbd993b72d04b7345c4c9e5aa04825a5e49348e6`, and state ext4 `e983e65d47dc51474718f5cd828c4571faacf971f79467b8e4a2219160f4834d`. Every emitted manifest has the canonical domain, format, version, sorted relative paths, valid digest file, and no output-base symlink target.

A rootfs-policy-only mutation executed only `OrlixRootfs` and the rootfs `OrlixArtifactIdentityV2` action. A same-size, same-mtime Coreutils `src/yes.c` mutation executed only Coreutils `OrlixGuestPackage`, `OrlixRootfs`, and the rootfs identity action. A copied rootfs-toolchain configuration-identity mutation executed only `OrlixRootfs`. `/opt/homebrew/etc/mke2fs.conf` is now selected explicitly through `MKE2FS_CONFIG`, hashed in `rootfs-identity.json`, and watched by the repository rule. All fixtures and source files were restored byte-for-byte. `artifacts.lock.json` remains SHA-256 `d8b90ca1fa57a2e0cd61fb35335345e11f442c29219c225026923d110ffca174`.

Fourteen focused Python tests and 32 Bazel analysis tests pass. `make test` exited 0 after building the current KUnit objects; it does not establish runtime execution. `make agent-harness-check` exited 0 with 13 tests and current generated files. Independent review returned PASS after the mke2fs configuration input fix. Evidence is under `Build/AgentHarness/bazel-migration/recovery-checkpoint-5d/`, led by `verification.json`, `independent-equivalence.json`, `independent-a-retry-build.log`, `independent-b2-build.log`, `rootfs-policy-mutation.execution.json`, `coreutils-source-mutation.execution.json`, `mke2fs-config-identity-mutation.execution.json`, and `sandbox-tree-artifact-evidence.json`.

The first independent A probe and first rootfs-policy probe had environment-shape failures before corrected runs passed. The first independent B process was interrupted at action 90 of 94; B2 passed in a new output base. These failed probes are retained in `verification.json`. Phase 6 promoted storage, runtime, device, authority-cutover, and release readiness are not established. Current-head CI is a separate gate.

## Command wrapper removal checkpoint

Removed the retired command wrapper instructions, command-policy variants, and hook recognition. Inspector commands and policy tests now invoke tools directly. Existing bare-command rules are unchanged. Two stale test expectations now match the original rule file, which returned an empty matchedRules list for the timeout-prefixed Make commands.

The machine package uninstall exited 0. `make agent-harness-check` exited 0 with 13 hook tests passing, current generated hooks, and clean documentation checks. Six policy tests and `git diff --check` passed. The tracked-source scan found no wrapper references, and the shell cannot resolve the removed command. Raw final checks are in `Build/AgentHarness/command-wrapper-removal/checks.json`. After explicit user approval, the narrow machine configuration scan passed with no remaining active references. The two package cache downloads and their symlinks were removed. The two historical instruction backups were subsequently deleted on explicit user request, and their absence was verified. Machine verification is recorded in `Build/AgentHarness/command-wrapper-removal/machine-check.json`. Application and runtime tests were not run because this change only removes developer-tool integration. Existing build-optimization edits, the original AGENTS.md delta, and untracked tool/build directories remain preserved.


## User-requested publication of local work

On 2026-09-11, Rudi stopped implementation and requested publication of all local work. This preserves partial 5C2 implementation and command-wrapper removal without accepting Phase 5C2. Final libcap/libsepol integration, libselinux, Coreutils feature parity, and integrated mutation/equivalence checks remain pending. Phase 5D and later phases remain pending.

Fresh publication checks: `gmake __bazel-matrix-check` exited 0 with 122 Python tests and 32 passing Bazel analysis tests (four executed, 28 cached). `gmake agent-harness-check` exited 0 with 13 hook tests and current generated hooks. `python3 .codex/rules/tests/test_execpolicy_rules.py` exited 0 with six tests. `git diff --check` passed. Logs: `/private/tmp/orlix-recovery-current-matrix.log`, `/private/tmp/orlix-publish-harness.log`, and `/private/tmp/orlix-publish-policy.log`. No new package/runtime build completed during this publication pass.

The inspected remote head was `9df61327255e923a8317cd217a6d72c8895f1fcf`. Its Bazel CI passed. Its iOS 15 run `34599070354` failed because OrlixUITests-Runner hung before establishing connection, error 65 through Make exit 2. Root cause remains unverified. The failed log is `/private/tmp/orlix-ios15-failed.log`. New-commit CI is a separate gate.

The duplicate XcodeBuildMCP bullet in AGENTS.md is included under the explicit all-local-work publication request. Untracked `.xcodebuildmcp/` and `third_party/swift/.build/` remain excluded machine/build state. TAP stays stopped and artifacts.lock.json remains unchanged. No runtime, device, promotion, cutover, or release readiness is established.

## Checkpoint 6A: local-first trust-bound promoted reconstruction

Phase 6A: ACCEPTED locally. Promoted reconstruction consults a digest-addressed local artifact store before network acquisition. Warm hits reuse verified local trees without `oras` or `cosign`. Trust-policy changes reverify signatures using `cosign` without re-downloading valid immutable blobs. A verified staged replacement removes corrupt cache bytes under the same store lock, so no persistent quarantine is retained. Active reconstruction leases protect their pinned artifacts across GC as long as the recorded consumer directory exists on disk, regardless of lease age, while dead or malformed leases are purged and their objects collected.

Reconstruction routes through `ArtifactStore.materialize_local` under a shared exclusive store lock (`fcntl.flock`), guaranteeing that validation, signature reverification, touch, and destination tree copying are atomic and cannot race with concurrent GC or replacement across worktrees. `ArtifactStore._validate` defensively checks all metadata types, and `_lookup_unlocked` treats malformed records (e.g. non-dict JSON or null markers) as cache misses, allowing clean reacquisition and complete reconstruction.

Verification passed on both system `python3` (3.14) and macOS system `/usr/bin/python3` (Python 3.9 compatibility contract):
- 56 promotion unit tests passed on system `python3`, exit 0.
- 56 promotion unit tests passed on `/usr/bin/python3`, exit 0.
- `make __bazel-matrix-check` exited 0 with 17 Make routing tests, 126 Python tests, and 32 passing Bazel analysis tests.
- `make test` exited 0 after building current KUnit objects; it did not execute runtime tests.
- `make agent-harness-check` exited 0 with 13 lifecycle-hook tests and current generated documentation.
- Deterministic proof tests in `bazel/promotion/test_artifact_store.py`:
  - `test_concurrent_reader_and_gc_synchronization`: proves mutual exclusion between `materialize_local` and `gc`.
  - `test_replacement_does_not_retain_corrupt_bytes`: proves corrupt cache bytes do not survive verified replacement.
  - `test_durable_live_consumer_pin_across_gc`: proves live-consumer retention beyond 30 days and dead lease cleanup.
  - `test_malformed_metadata_recovers_via_reacquisition`: proves malformed records return cache miss and reacquire cleanly.
- `git diff --check` passed cleanly.
- `artifacts.lock.json` remains SHA-256 `d8b90ca1fa57a2e0cd61fb35335345e11f442c29219c225026923d110ffca174` (untouched).

Raw evidence resides under `Build/AgentHarness/bazel-migration/recovery-checkpoint-6/verification.json`.

Independent review returned PASS after the ordered tar extraction preserved read-only directory metadata, rejected chained symlink escapes, and malformed lease directories stopped aborting GC.

Phase 6B (kernel promotion into the signed buildset), Phase 7 through 10, physical-device validation, TAP, runtime readiness, cutover, and release remain separate future gates. Untracked `.build/` and machine state remain excluded.

## Checkpoint 6B: Kernel promotion foundation

Phase 6B local foundation: VERIFIED locally. Full Phase 6B acceptance remains open until protected automation signs and publishes all seven components, applies one signed schema-2 lock, and atomically switches Kernel consumers to the promoted variants.

Four independently addressable Kernel components now exist for release and development on iphoneos and iphonesimulator. Each component's `artifact-identity-v2` covers exactly `OrlixKernel.a`, `arch/orlix/boot/dts/development.dtb`, and `arch/orlix/boot/dts/release.dtb`. Compiler, symbol, proof, provenance, OCI, and output-base identity stay outside that content identity.

| Component | Independent A/B artifact identity |
| --- | --- |
| `kernel-release-iphoneos` | `c99bd1f1fe0208d9a294ec87ac60c262fdd91a5fbb1cd91dbec67606bfde5162` |
| `kernel-release-iphonesimulator` | `a002988e35ec3228c22055970c18a86db1c0012895043b24e6d821c02c1d7702` |
| `kernel-development-iphoneos` | `980868f597cb9cd94161870a832bc0454c6316fb83304b63468383de462d5abf` |
| `kernel-development-iphonesimulator` | `a157b3084a9297801f30fd527f288f15ef32f9be7cffb3862e5c16fcfeb28393` |

Every Make-owned promotion proof used two isolated output bases with Bazel action reuse, remote cache and execution, ccache, and retained Kernel state disabled. Each target compared the canonical identities and exited 0 only after the expected unsigned signing probe failed with `ORLIX_COSIGN_KEY is required to sign; refusing to invent a signature`.

Schema 1 parsing and buildset digest behavior remain unchanged for the current signed `uapi`, `mlibc`, and `rootfs` lock. Schema 2 binds each component key to a typed artifact identity and immutable OCI digest. It requires explicit legacy marker identities for the three existing components and `artifact-identity-v2` for all four Kernel variants. Kernel packages contain only `product/`, `artifact-identity-v2.json`, and `artifact-identity-v2.sha256`; trust transitions recompute the product identity before reuse.

One integration test assembles and applies a complete synthetic seven-component signed schema-2 proposal, performs seven cold signature checks and OCI pulls, stores each verified object, then reconstructs the same buildset from a second consumer with zero network-tool calls. This proves the local contract without inventing a signature or writing the live lock.

Verification passed:

- 63 promotion tests passed on `python3`, exit 0.
- 63 promotion tests passed on `/usr/bin/python3`, exit 0.
- 29 focused Make-routing and workflow-policy tests passed, exit 0.
- `make __bazel-matrix-check` exited 0 with 18 Make routing tests and 36 passing Bazel analysis tests.
- `make test` exited 0 after building the current Kernel and KUnit objects; it did not execute product runtime validation.
- `make agent-harness-check` exited 0 with 13 lifecycle-hook tests, current generated files, and zero documentation problems.
- `artifacts.lock.json` remains byte-unchanged.

Independent review returned PASS with no material finding after the schema-1 compatibility, schema-2 identity contract, Kernel product boundary, promoted shell arguments, workflow routing, and final integration proof were inspected. The reviewer did not rerun builds or tests.

Protected signing, GHCR publication, signed schema-2 lock activation, promoted Kernel target execution against imported artifacts, and consumer cutover remain open. Phase 7 through Phase 10, runtime readiness, TAP, physical-device validation, authority cutover, and release remain separate gates. `third_party/swift/.build/` remains excluded.

## 2026-09-13 Agentic engineering system checkpoint

RuleSync 16.26.1 generated the configured rules, MCP, subagents, skills, hooks, and permissions in isolated output. The generated Codex, Claude Code, Cursor, and Copilot hook files invoke `Tools/AgentHarness/hooks/`. The generated Codex roles preserve high reasoning for planning, review, and TCTI inspection, medium reasoning for implementation and Bazel inspection, and read-only native restrictions for inspection roles.

`make agent-harness-check`, `make docs-check`, `make agent-graph-check`, `make agent-frontier`, and the three `AREA=orlix-tcti` gates pass. The live ontology graph reports zero problems and the leaf frontier remains `#50`, `#232`, `#233`, `#242`, and `#261`. RuleSync reports that Copilot and Cursor do not support `permissionRequest`, Codex permissions do not support the shared `bash` category, and RuleSync 16.26.1 does not manage `max_concurrent_threads_per_session`; the capability report records those limits.

GitHub refused the `github-actions` integration bypass for `main` ruleset `21800281` with HTTP 422: `Actor GitHub Actions integration must be part of the ruleset source or owner organization`. The ruleset remains disabled. Authoritative post-merge generation and direct-main write proof remain blocked until a repository-installed RuleSync CI identity can receive ruleset bypass. Normal contributors must not receive that bypass.

## 2026-09-13 Agentic engineering system repair

The output inventory now inspects isolated RuleSync 16.26.1 output for each target and feature. The Orlix validator rejects output outside the four configured client destinations, binds each mixed-ownership `.github` destination to its exact RuleSync feature, and keeps workflows, actions, and issue templates outside generated protection. Source-validation CI uploads the inventory and capability reports as structured evidence.

The first real `origin/main...HEAD` guard run failed because `origin/main` predates `.rulesync/VERSION`; the temporary base snapshot had no version pin. Base and head snapshots now receive the current 16.26.1 pin before RuleSync derives ownership. The repeated real guard passed.

GitHub source-validation runs `34769153172` and `34769445793` exposed malformed RuleSync 16.26.1 JSON generation output. The second run identified `cursor/skills` at byte `884730`. Inventory now uses RuleSync's isolated generated output tree instead of parsing that broken JSON surface.

Lifecycle enforcement now rejects stale task envelopes before mutation, subagent launch, and completion proof. It recognizes the repository's required `git -C` command shape, blocks the banned history-changing Git operations, and applies the existing physical-device and signing gates to XcodeBuildMCP device tools. Post-tool state treats only explicit errors and failed exit statuses as failures.

`rulesync doctor`, `rulesync generate --dry-run`, `make docs-check`, `make agent-harness-check`, `make agent-graph-check`, `make agent-frontier`, `actionlint`, `git diff --check`, and `make test` exited 0. The focused harness suite passed 11 tests. The live frontier remains `#50`, `#232`, `#233`, `#242`, and `#261`. The installed XcodeBuildMCP CLI does not expose `doctor`, so the Apple skill requires the operation to be listed before use. RuleSync 16.26.1 still skips Codex's `agents` table, so the four-thread limit remains instruction-only and is reported as such.

Ruleset `21800281` remains disabled. Its live bypass is `RepositoryRole` ID `5`. The Actions secret-name list contains `GROK_AUTH_JSON`, `ORLIX_CI_BUILDBUDDY_READ_API_KEY`, `ORLIX_CI_BUILDBUDDY_WRITE_API_KEY`, `ORLIX_COSIGN_KEY`, and `ORLIX_COSIGN_KEY_PASSWORD`; none identifies RuleSync GitHub App credentials. Main-generation proof remains blocked on a repository-installed GitHub App with the narrow contents-write permission and ruleset bypass.

## Canonical Apple CI and BuildBuddy policy checkpoint

Status: source and policy gates pass locally. Runtime, remote-cache, transfer, and account-state acceptance remain `UNVERIFIED`, so this checkpoint is not runtime-verified or complete.

Changed behavior: `.github/workflows/bazel-ci.yml` replaces the separate pull-request, main, and iOS 15 workflows while preserving the required `Bazel matrix check` job name. One macOS job restores download caches, selects Xcode once, configures BuildBuddy once, builds the iOS 15-floor simulator app and UI-test bundle in one Bazel invocation, then installs and launches the same extracted `Orlix.app` on current and iOS 15.5 simulators. `make __bazel-apple-ci` owns orchestration. The retained `__bazel-ios15-simulator-gate` is runtime-only and contains no Bazel build or test command.

BuildBuddy uses `grpcs://remote.buildbuddy.io`, compression, content-defined chunking, minimal downloads by default, BuildBuddy BES, the existing local disk cache, and instance `orlix/apple/bazel-9.2.0/xcode-17F113/v1`. `ORLIX_BAZEL_CACHE_EPOCH` defaults to `v1`. `ORLIX_BUILDBUDDY_CACHE_MODE` accepts only `normal`, `conserve`, or `off`. Main selects `ORLIX_CI_BUILDBUDDY_WRITE_API_KEY`, same-repository pull requests select `ORLIX_CI_BUILDBUDDY_READ_API_KEY` plus `--remote_upload_local_results=false`, and forks receive no key. Promotion, independent reconstruction, TestFlight, App Review, and App Store release do not enable BuildBuddy. The credential rc is created under `$RUNNER_TEMP` with mode `0600`, linked through the existing untracked `.bazelrc.local` import, and removed by an `if: always()` step.

The canonical workflow persists the checksum-verified Bazel repository cache and the iOS 15.5 download archive only from trusted `main`. It does not persist Bazel disk-cache or ccache results. The runtime cache key includes archive SHA-256 `71fd7d0159a4439ebef1abb2b1e0b26204b74af903dcc579a7b6e131065a1150`. `artifacts.lock.json` remains unchanged at SHA-256 `d8b90ca1fa57a2e0cd61fb35335345e11f442c29219c225026923d110ffca174`, selecting buildset `ce931c2561ba324daccb854c2aa5cfa875238956156ae91d304a46807731c05d`.

Verification: 33 focused workflow and Make-routing tests passed. The cache-observation test passed. `actionlint .github/workflows/*.yml`, `zizmor .github/workflows`, `gmake __bazel-migration-inventory-check`, `gmake docs-check agent-harness-check`, `gmake __bazel-matrix-check`, and `gmake test` exited 0. The matrix passed 36 Bazel analysis tests from cache. `gmake test` built KUnit objects but did not execute product runtime proof. Structured evidence is `Build/AgentHarness/bazel-migration/recovery-checkpoint-ci-cache/verification.json`.

Current CI: PR [rudironsoni/Orlix#230](https://github.com/rudironsoni/Orlix/pull/230) exercises the canonical workflow at head `54ac94866a3f00ed4f00ea331ae14d8183ad9a72`. Run `34719627887`, job `103622961666`, restored and booted both simulator runtimes, selected same-repository pull-request BuildBuddy read access, created the private credential rc, and removed it during successful cleanup. The Make operation then failed before Bazel product compilation because `test_forged_signed_flag_cannot_change_lock` inherited `ORLIX_ORAS_REGISTRY_CONFIG=/Users/runner/work/_temp/orlix-registry.json` and attempted to copy that not-yet-created file instead of reaching its mocked invalid-signature result. This is a promotion unit-test isolation failure. It is not a runtime-installation, simulator-boot, BuildBuddy-authentication, compilation, GHCR-publication, or product-runtime result. The test now clears that unrelated environment variable. The exact regression command with a nonexistent registry path and the complete 63-test promotion suite both exit 0 locally. A current-head GitHub retry remains the next acceptance gate.

Run `34720477768`, job `103625295118`, at head `f5c11e7b70f27ad02a598244434badc6405c424e` again restored and booted both runtimes and passed the corrected lock-proposal test. It then exposed the shared workflow defect in three signing tests: canonical CI exported `ORLIX_ORAS_REGISTRY_CONFIG=/Users/runner/work/_temp/orlix-registry.json` without creating that file or authenticating to GHCR. This is another pre-build test-environment failure. Pull requests now use source mode because Phase 6 changes component build and promotion rules, and canonical CI no longer exports a nonexistent registry config. Main retains promoted mode. App-only pull-request use of promoted foundations remains deferred until the complete signed schema-2 buildset is activated atomically, because selecting the current partial schema-1 buildset would omit the four Kernel variants and create the mixed authority prohibited by this checkpoint. A new exact-head GitHub run remains the acceptance gate.

Run `34721225010`, job `103627340023`, at head `374479e831df37412b4c432dad8d31fa0304b6e2` passed both simulator preparation and the complete source product build. It compiled one canonical simulator product, launched that same application on iOS 26.5 and iOS 15.5, and wrote `runtime-proof.json` with compile count 1 and both results `PASS`. BuildBuddy read-only configuration was active, uploads were disabled, and invocation `81bfe969-9d18-435d-b700-aab9fb4a7451` completed the product build. The workflow failed only after both runtime proofs because Bazel 9 wrote concatenated pretty-printed JSON objects to `execution.json`, while `cache_observation.py` accepted JSON Lines only. Downloaded evidence is under `/private/tmp/orlix-run-34721225010/bazel-apple-ci-e467b86926d538dd672bd1ec0c234be48cbe7f1c/work/Orlix/Orlix/Build/AgentHarness/bazel-ci/`. The parser now uses the standard JSON decoder to consume adjacent JSON values. Applying it to the downloaded evidence reports 840 created actions, 531 executed actions, 0 remote hits, 0 local-cache hits, and 2,364,494 ms. Remote misses, uploads, downloads, and transfer bytes remain `UNVERIFIED` because those values are absent from the execution and BEP records. This run established the runtime evidence but did not make the required check green.

Run `34724381178` at head `25cd548780ba1f15a43ac0a2ff2cc0f33bf8f3c9` exercised the parser fix. Attempt 1 failed before product compilation when `/usr/bin/xcrun --sdk iphoneos --show-sdk-build-version` exceeded the existing 30-second toolchain-probe limit. Attempt 2 failed earlier with Xcodes `failedMountingDMG` while installing a freshly downloaded iOS 15.5 runtime; the pull-request cache restore reported `Cache not found`, and the low-trust job correctly did not write that shared cache. Attempt 3, job `103638785910`, passed in 1 hour 2 minutes 27 seconds. It selected effective cache mode `normal`, context `pr`, access `read`, used `ORLIX_CI_BUILDBUDDY_READ_API_KEY`, and applied `--remote_upload_local_results=false`. It compiled one simulator product and launched the same application on iOS 26.5 and iOS 15.5. Both runtime results are `PASS`. Its cache evidence reports 840 created actions, 531 executed actions, 0 remote hits, 0 local-cache hits, and 2,417,967 ms; transfer bytes and remote miss counts remain `UNVERIFIED`. Downloaded evidence is under `/private/tmp/orlix-run-34724381178-attempt3/bazel-apple-ci-8c14239b36658d57578ee97f4177479fef107d1f/work/Orlix/Orlix/Build/AgentHarness/bazel-ci/`, including `runtime-proof.json`, `cache-observation.json`, `execution.json`, `build-events.json`, `remote-grpc.log`, and both runtime launch records. No secret value appears in the recorded evidence or logs.

Runtime and external gates: the exact iOS 15.5 archive downloaded successfully, but local installation needs an interactive administrator password. `xcrun simctl runtime add` rejected the older HFS package image with `mount_apfs exited with code 65`. The GitHub runner retains the existing passwordless Xcodes install path. Rudi reports that GHCR package Actions access and the `ORLIX_CI_BUILDBUDDY_WRITE_API_KEY` and `ORLIX_CI_BUILDBUDDY_READ_API_KEY` secrets are configured. Actual GHCR write access, BuildBuddy authentication, cache traffic, main and pull-request transfer per build, monthly projection, the repository variable value, and the 80 GB ceiling remain `UNVERIFIED` until representative trusted workflows run.

The canonical workflow and BuildBuddy wiring share the one required workflow and one Make operation. Splitting them would publish an intermediate required check without the mandated cache setup on its sole runner, so this checkpoint keeps them atomic. The Cosign correction remains the separate preceding commit `185b9f9df5e2ded602ef471e7133bac316d7f66e`; `cosign public-key --help` confirms that deriving the public key from the private key file is supported. This checkpoint does not describe that correction as build-optimization completion.

TAP remains stopped. No simulator runtime result, BuildBuddy performance result, transfer budget, promotion, App Store release, or physical-device claim is established.

## Buildset-level promotion checkpoint

Status: the source reconstruction and policy gates pass locally. Protected Cosign signing, GHCR publication, GitHub attestation, and signed schema-2 lock proposal creation remain `UNVERIFIED`, so this checkpoint does not activate a new buildset.

Changed behavior: `make __bazel-promote-buildset` now builds `uapi`, `mlibc`, `rootfs`, and all four Kernel profile/destination components in one Bazel graph under clean root A, then repeats the same graph under clean root B. Both invocations use `--nouse_action_cache`, `--disk_cache=`, `--remote_cache=`, `--remote_executor=`, `--batch`, and `--config=promotion`. The promotion config disables ccache and retained Kernel, mlibc, and package state. The only reusable input store is Bazel's checksum-verified repository download cache.

The Make operation stages each component from the two shared output bases and uses the existing full comparator. It compares paths, entry types, modes, file bytes, and symlink targets. The four Kernel products also validate `artifact-identity-v2`. The three legacy components retain explicit marker identities. Per-component promotion targets remain for diagnosis, but the protected workflow no longer uses them for reconstruction.

`.github/workflows/bazel-promote.yml` is now the sole normal promotion authority. It invokes the buildset Make operation, publishes all seven components through the existing Cosign and GHCR targets, writes one signed lock proposal, and attests that proposal. The retired `.github/workflows/bazel-lock-proposal.yml` no longer joins seven separate runs. The workflow restores and saves only the Bazel repository cache. It does not persist Bazel action results, compiler objects, output bases, or retained build state. `artifacts.lock.json` is not changed by promotion.

The aggregate proposal now binds a component type for every key plus the signing-key fingerprint, trust-policy digest, and verification-policy version. `make __bazel-lock-proposal` writes the proposal, Cosign signs its exact bytes into `buildset-lock-proposal.sigstore.json`, and Cosign verifies that bundle. `make __bazel-lock-from-signed` verifies the same bundle again immediately before atomic lock replacement. The protected trust policy names only `.github/workflows/bazel-promote.yml`, requires `bazel-promotion`, and permits `main` plus the exact pre-merge `fix/build-optimizations` branch requested for this protected proof. Pull-request events still have no automatic package-write path.

Artifact identities from the local independent A/B proof:

| Component | Matching A/B identity |
| --- | --- |
| `uapi` | `5664459c80f50b46ea399fd4d0e2c37154ba6ff7f96ec281e31aaf5f74e128a0` |
| `mlibc` | `66bb6f940a1b7b4eac7ec3c5b7779debed2b921d575eb661e7f702813cb37a5d` |
| `rootfs` | `68e0a778107c9402ee7f76b686fd05b55e8a522fe2556798a7a0a35db180b9f2` |
| `kernel-release-iphoneos` | `c99bd1f1fe0208d9a294ec87ac60c262fdd91a5fbb1cd91dbec67606bfde5162` |
| `kernel-release-iphonesimulator` | `a002988e35ec3228c22055970c18a86db1c0012895043b24e6d821c02c1d7702` |
| `kernel-development-iphoneos` | `980868f597cb9cd94161870a832bc0454c6316fb83304b63468383de462d5abf` |
| `kernel-development-iphonesimulator` | `a157b3084a9297801f30fd527f288f15ef32f9be7cffb3862e5c16fcfeb28393` |

Verification results: the combined seven-target Bazel analysis passed. Clean build A executed 88 actions in 695.702 seconds with a 654.26-second critical path. Clean build B executed 88 actions in 692.427 seconds with a 651.75-second critical path. All seven A/B identities and staged product trees matched. The seven unsigned schema-2 proposals contain no OCI digest or signature. The final `artifacts.lock.json` SHA-256 remains `d8b90ca1fa57a2e0cd61fb35335345e11f442c29219c225026923d110ffca174`.

Verification commands: `gmake __bazel-promote-buildset`; `PYTHONPATH=bazel/promotion python3 -m unittest discover -v -s bazel/promotion -p 'test_*.py'`; the same 63-test suite on `/usr/bin/python3`; `python3 -m unittest bazel.migration.test_make_routing bazel.migration.test_workflow_policy`; `actionlint .github/workflows/*.yml`; `zizmor .github/workflows`; `gmake __bazel-migration-inventory`; `gmake __bazel-migration-inventory-check`; `gmake __bazel-matrix-check`; `gmake test`; and `gmake docs-check agent-harness-check`. The clean-build evidence is under `Build/Bazel/promote/buildset/{a,b}/`, including `build-events.json`, `execution.json`, and `profile.json.gz`. Component proposals and staged comparisons are under `Build/Bazel/promote/<component>/`.

Remaining gates: a protected workflow must publish all seven GHCR artifacts successfully before GHCR access is verified. The workflow must produce its GitHub attestation and signed buildset proposal before those claims are verified. Applying a verified schema-2 proposal to `artifacts.lock.json` remains an atomic transition after all seven artifacts pass pull, signature, and identity verification.

Claims not established: no GHCR write, Cosign signature, signed buildset identity, lock activation, BuildBuddy transfer, simulator runtime, physical-device, TestFlight, App Review, or App Store result is claimed. TAP remains stopped. The user-owned `AGENTS.md` content, current signed `artifacts.lock.json`, and `third_party/swift/.build/` remain outside this checkpoint.

Checkpoint status remains separate:

- 6B1 Kernel promotion mechanics: VERIFIED LOCALLY.
- 6B2 protected GHCR publication: UNVERIFIED.
- 6B3 signed schema-2 activation: PENDING.
- 6B4 promoted consumer cutover: PENDING.
- Phase 6B: IN PROGRESS.
- Canonical Apple CI and BuildBuddy: IN PROGRESS.

Commit separation constraint: `f264e6d074d79079726ae07b45fe40a0b91516fe` already published the canonical Apple workflow and BuildBuddy configuration together before the current instruction. Splitting that published commit would require forbidden history rewriting and a force-push. The current work therefore preserves it and adds the corrected configured secret identifiers with the separately reviewable buildset-promotion checkpoint.

## Workflow security checkpoint

Homebrew `zizmor` 1.30.1 is now declared beside `actionlint`. Its first audit found three checkout credential-persistence findings and one writable Ruby cache finding in the tag-triggered TestFlight job. The narrow corrections set `persist-credentials: false` on those checkouts and replace the TestFlight Ruby cache with an explicit pinned dependency install. The final offline `zizmor .github/workflows` audit exited 0 with no findings and 14 repository suppressions. `actionlint .github/workflows/*.yml` exited 0.

These static checks do not establish GitHub-hosted execution, TestFlight eligibility, App Review state, or release readiness. Canonical Apple CI and Phase 6B remain in progress. No release workflow was dispatched. TAP remains stopped.

## Public-key-only signed-lock activation checkpoint

Status: VERIFIED LOCALLY. Protected promotion and activation remain `UNVERIFIED`, so Phase 6B remains in progress.

Changed behavior: `make __bazel-lock-from-signed` now depends only on `__bazel-version-check`. It loads the existing `Build/Bazel/promote/buildset-lock-proposal.json` and `buildset-lock-proposal.sigstore.json` produced by protected promotion. It does not call `__bazel-lock-proposal`, accept signed component inputs, regenerate proposal bytes, sign, or read `ORLIX_COSIGN_KEY`. `lock_proposal.py` requires `ORLIX_COSIGN_PUB`, verifies the exact proposal bytes with the existing Sigstore bundle, requires schema 2 and exactly the seven Phase 6B components, validates component types and trust metadata, validates every OCI digest and artifact identity, verifies every component signature, and uses the existing atomic temporary-file replacement for `artifacts.lock.json`.

Artifact identities: this correction changes activation policy only. It creates no component artifact and changes no artifact identity. Before and after verification, `artifacts.lock.json` remains schema 1 at SHA-256 `d8b90ca1fa57a2e0cd61fb35335345e11f442c29219c225026923d110ffca174`, selecting buildset `ce931c2561ba324daccb854c2aa5cfa875238956156ae91d304a46807731c05d`.

Verification results: 65 promotion tests passed on `python3`, exit 0, and the same 65 tests passed on `/usr/bin/python3`, exit 0. The activation regression succeeds with `ORLIX_COSIGN_PUB` present and `ORLIX_COSIGN_KEY` absent. Its parameterized failure path rejects invalid proposal signatures, bundles, component sets, OCI digests, artifact identities, and schemas without changing the lock. All 35 Make-routing and workflow-policy tests passed, exit 0. `actionlint .github/workflows/*.yml`, `zizmor .github/workflows`, and `gmake docs-check agent-harness-check` exited 0; Zizmor reported no findings and 14 repository suppressions. `gmake __bazel-matrix-check` exited 0 with 36 passing Bazel analysis tests. `gmake test` exited 0 after building KUnit-selected objects; it did not execute app-hosted runtime proof. Evidence locations are `bazel/promotion/test_lock_proposal.py`, `bazel/migration/test_make_routing.py`, and the public activation recipe in `make/bazel-migration.mk`.

CI status: the accepted canonical PR run remains `34728287230` at head `92e9fd71322c6f2948c4fc9916d397546cf30eeb`. The activation correction has not yet been pushed or exercised by protected automation.

Remaining gates: publish this correction, dispatch `.github/workflows/bazel-promote.yml` at its exact branch head, require all seven GHCR publications and verifications plus the proposal attestation, then activate only the downloaded signed proposal. No lock activation, promoted consumer, warm-reuse, BuildBuddy transfer, physical-device, TestFlight, App Review, App Store, or Phase 7 claim is established. TAP remains stopped. The user-owned `AGENTS.md` content and `third_party/swift/.build/` remain untouched.

Protected promotion run `34746160338`, job `103694374678`, exercised `.github/workflows/bazel-promote.yml` at exact head `d7806221a39f2c5a1fe2572df197d37a380d6b29` under the `bazel-promotion` environment. Checkout, Xcode 26.6, verified source-download restoration, tool installation, and `make __bazel-promote-buildset` passed. Clean build A executed 88 actions in 1,258.980 seconds with a 602.39-second critical path. Clean build B executed 88 actions in 1,273.739 seconds with a 629.66-second critical path. Both used the promotion target's disabled Bazel action cache, disk cache, BuildBuddy endpoint, remote executor, ccache, and persistent component state.

The job declared `packages: write`, authenticated `oras` with the job `GITHUB_TOKEN`, and reported `Login Succeeded`. Its first publication attempted `ghcr.io/rudironsoni/orlix/uapi:5664459c80f50b46ea399fd4d0e2c37154ba6ff7f96ec281e31aaf5f74e128a0`. GHCR rejected the component upload with `Error response from registry: denied: permission_denied: write_package`. This is a package authorization failure. It is not a source-build, reproducibility, signing, artifact-identity, runtime, or cache failure. The affected component is `uapi`; the other six publication operations did not run. No signed component proposal, immutable OCI digest, aggregate proposal, Sigstore bundle, or GitHub attestation was produced. The `if: always()` artifact upload failed closed because none of the required signed files existed.

Phase 6B activation is stopped. `artifacts.lock.json` remains schema 1 at SHA-256 `d8b90ca1fa57a2e0cd61fb35335345e11f442c29219c225026923d110ffca174`, selecting buildset `ce931c2561ba324daccb854c2aa5cfa875238956156ae91d304a46807731c05d`. 6B1 remains VERIFIED LOCALLY. 6B2 protected GHCR publication remains UNVERIFIED. 6B3 signed schema-2 activation remains PENDING. 6B4 promoted consumer cutover remains PENDING. Phase 6B remains IN PROGRESS. The exact external gate is Actions write access for the existing account-owned `orlix/uapi` GHCR package from repository `rudironsoni/Orlix`; the namespace must not change.

Canonical run `34746150300`, job `103694344966`, at head `d7806221a39f2c5a1fe2572df197d37a380d6b29` passed setup, BuildBuddy read-only configuration, both simulator preparations, promotion tests, and all 36 Bazel analysis tests. It stopped before product compilation because the public-key-only Make dependency change made `bazel/migration/legacy-target-map.json` and `bazel/migration/proof-map.json` stale. `gmake __bazel-migration-inventory` refreshed only those two tracked generated maps. The semantic change records `__bazel-lock-from-signed` as depending on `__bazel-version-check` instead of `__bazel-lock-proposal`; the remaining differences are source-line shifts and the updated Makefile digest. `gmake __bazel-migration-inventory-check` now exits 0. This generated-inventory failure is separate from the protected GHCR authorization failure.

Protected promotion run `34751408211` resolved exact PR head `39be2c74b9db073fbdb169859220084d23bf21d2`, entered `bazel-promotion`, and ran with job-level `packages: write`. Attempts 1 and 2, jobs `103708478114` and `103708968029`, stopped during the first cache-disabled source build. Both received `GET returned 502 Bad Gateway` for checksum-pinned `https://download.savannah.nongnu.org/releases/acl/acl-2.3.2.tar.xz`. Neither attempt reached GHCR, Cosign signing, proposal generation, or attestation. This is a source-download failure, not a build, reproducibility, registry-authorization, signing, or artifact-identity failure.

The ACL repository now retains that primary URL and adds Savannah's documented direct mirror, `https://download-mirror.savannah.gnu.org/releases/acl/acl-2.3.2.tar.xz`. A direct download from the mirror produced SHA-256 `97203a72cae99ab89a067fe2210c1cbf052bc492b479eca7d226d9830883b0bd`, equal to the existing repository pin. Bazel therefore accepts either byte-identical source through the same integrity check. `/usr/bin/python3 -m unittest bazel.extensions.test_native_sources` passed six tests, the complete 65-test promotion suite passed, and 41 Make-routing and workflow-policy tests passed. `make __bazel-module-lock-update`, `make __bazel-matrix-check`, `make test`, and `make docs-check agent-harness-check` exited 0. The matrix reported all 36 Bazel analysis tests passing from cache. `make test` built KUnit-selected objects but did not execute app-hosted runtime proof. `actionlint`, `zizmor`, and `git diff --check` passed; Zizmor reported no findings and 14 repository suppressions. `artifacts.lock.json` remains schema 1 and byte-identical at SHA-256 `d8b90ca1fa57a2e0cd61fb35335345e11f442c29219c225026923d110ffca174`.

Phase 6B remains IN PROGRESS. Protected GHCR publication, seven component signatures, the aggregate proposal, GitHub attestation, schema-2 activation, promoted consumers, and cold/warm reconstruction remain unverified. TAP remains stopped. The user-owned `third_party/swift/.build/` remains untouched.

Protected promotion run `34751943538`, job `103709861682`, at exact head `9f132d37150c192c6d6b81cb0b7a10255de79e00` proved the ACL fallback: Bazel continued analysis after the primary ACL URL timed out. The same run then stopped on `GET returned 502 Bad Gateway` for the checksum-pinned adjacent Attr archive. The Attr source now uses the same Savannah direct-mirror policy. The direct Attr mirror download produced SHA-256 `f2e97b0ab7ce293681ab701915766190d607a1dba7fae8a718138150b700a70b`, equal to the existing pin. The run did not reach GHCR, signing, proposal generation, or attestation, and it did not change `artifacts.lock.json`.

Protected promotion run `34752230147`, job `103710587963`, at exact head `2d6f04b48925c64b69a537669be7d652c63f0976` passed both clean source builds and all seven comparisons. GHCR login succeeded. `uapi`, `mlibc`, and `rootfs` passed push, Cosign verification, pull-back, and identity verification with OCI digests `sha256:10dabc3b23d4a0883c145076c74f0396544bdb68dcfa79f5e5a22fdc7d5135df`, `sha256:aa7583241bba907bb598c785e56b2060854f6dccee4974212b1251ea2e94ae20`, and `sha256:c17089de4d35c59503266f87e4ec87a173186ff7cd6203dcd7f075146fe505f9`. The first Kernel artifact, `kernel-release-iphoneos`, was pushed and signed at `sha256:de2e06a434a400e5ad85b93d172ab38ce3be6a0efa3a828f4bceacfa6d5b4cdb`, then pull-back identity validation failed with `v2 artifact identity manifest is not canonical`. The remaining three Kernel components, aggregate proposal, Sigstore bundle, and GitHub attestation were not attempted.

The failure reproduces on local Python 3.14.7. The signed product records mode `0555`, while Python 3.14's default tar data filter extracts the regular files as `0755`; `/usr/bin/python3` 3.9.6 does not apply that default filter. The existing safe extractor now reapplies each validated regular member's archived mode after extraction. Path, device, FIFO, symlink, and hard-link checks remain unchanged. The existing extraction test now verifies an exact `0555` regular-file mode. The real local Kernel tar round trip passes on Python 3.14.7 and preserves `0555`. The complete 65-test promotion suite passed on Python 3.14.7 and `/usr/bin/python3` 3.9.6. All 35 Make-routing and workflow-policy tests, 36 Bazel analysis tests, documentation and harness checks, `actionlint`, and `zizmor` passed. `make test` exited 0 after building KUnit-selected objects; it did not execute app-hosted runtime proof. `artifacts.lock.json` remains unchanged at schema 1.

## 2026-09-14 PR 230 review corrections

The eleven Codex review comments on PR [rudironsoni/Orlix#230](https://github.com/rudironsoni/Orlix/pull/230) are addressed. Two were already fixed by earlier branch commits: same-repository pull requests already used source mode (comment 3, fixed by `374479e8`), and lock activation already verified downloaded proposal bytes through `__bazel-lock-from-signed` without invoking the signing target (comment 8, fixed by `d7806221`). The remaining nine received source corrections.

`.github/workflows/bazel-ci.yml` gained `packages: read`, a `Select the component mode` step that reads the `artifacts.lock.json` schema, and a promoted-mode-only `Prepare promoted-mode verification inputs` step that materializes the `ORLIX_COSIGN_PUB` repository variable to a `$RUNNER_TEMP` file, authenticates `oras` to GHCR, and exports the file path. Main therefore uses source mode while the committed lock is schema 1 and can select promoted mode only after a schema-2 lock lands; pull requests always use source mode. `.github/workflows/bazel-promote.yml` restricts dispatches to the `trust-policy.json` `allowed_refs` and removes the private-repository build-provenance attestation step and its `id-token` and `attestations` permissions; the Sigstore bundle remains the promotion provenance. `.github/workflows/testflight-beta.yml`, `app-store-review.yml`, and `grok-pr-review.yml` now authenticate their protected `git fetch` operations with a per-call basic `http.extraheader` credential instead of relying on checkout-persisted credentials.

`.bazelrc` no longer hardcodes the BuildBuddy remote instance namespace tail; `__bazel-buildbuddy-configure` derives it from `ORLIX_BAZEL_CACHE_EPOCH`, making the epoch knob effective for every BuildBuddy-enabled CI build, and `.bazelrc.local.example` documents the local equivalent. `__bazel-simulator-runtime-proof` now validates the launched process pid after five seconds, fails on new `Orlix-*.ips` crash reports, and records `launch.alive`; a screenshot alone no longer counts as runtime proof. `publish.trusted_public_key()` materializes an inline PEM `ORLIX_COSIGN_PUB` value to a temporary file and still enforces the `trust-policy.json` fingerprint, which also repairs the nightly reconstruction key path.

Verification: the complete 56-test migration, 71-test promotion, and 8-test config suites exit 0; `make __bazel-migration-inventory-check`, `make docs-check`, and `actionlint .github/workflows/bazel-ci.yml .github/workflows/bazel-promote.yml .github/workflows/testflight-beta.yml .github/workflows/app-store-review.yml .github/workflows/grok-pr-review.yml` exit 0. `artifacts.lock.json` remains byte-unchanged. The promoted GHCR path, BuildBuddy epoch rotation, and simulator liveness gate remain `UNVERIFIED` until a trusted workflow run exercises them.

## 2026-09-15 complete protected seven-component promotion at current branch head

Protected promotion run `34927558114`, job `Dual-build promote`, exercised `.github/workflows/bazel-promote.yml` at exact head `334afabac5ded3572504de84c68028b6b3f80760` on `refs/heads/fix/build-optimizations` under the `bazel-promotion` environment, within the `trust-policy.json` `allowed_refs`. The mode-preserving extraction fix `f54bec0` now has its complete protected proof.

Both clean source builds passed with the promotion target's disabled action cache, remote cache, remote executor, disk cache, and no reusable local state: build A executed 88 actions in 1761.013 seconds with an 881.37-second critical path; build B executed 88 actions in 1339.800 seconds with a 655.94-second critical path. The recipe staged each side's seven component trees, compared every digest with `bazel/promotion/compare.py`, asserted `artifacts.lock.json` remained byte-identical across the unsigned promotion, and asserted no unsigned record was Cosign-signed. The dual-build step exited 0.

`oras` authenticated to GHCR with the job token (`Login Succeeded`). All seven components passed push, Cosign verification against the published reference, `oras` pull-back, and v2 artifact-identity validation inside `bazel/promotion/publish.py` (`__bazel-publish-*` exited 0 for each). Recorded OCI digests at this head:

- `uapi`: `sha256:be360e682efe452f9e90cdc064e85fecb24374c817f3cf44aec86e9069887b76`
- `mlibc`: `sha256:e38e05439ed21f0dbc4feb3e05a405c51f38787f1dc381101d501e7756d9e1a4`
- `rootfs`: `sha256:01b2dbcfebe59f031aa3cabc9c2f73ec8a1dcaa0094c1eaec61fde86782207c1`
- `kernel-release-iphoneos`: `sha256:7b12757dfc975ae2d62b5ff8158398c69a5564de90d8b9c903e06b7d1711cc24`
- `kernel-release-iphonesimulator`: `sha256:885dcf22269d13f784bb2593e83cce4a8fd7ca4dff5e2024d97a25f5b71a7ef6`
- `kernel-development-iphoneos`: `sha256:d07dee5b98568ba7838c4062cdf7f924ae4ea55fa1132c689f3eb7d5f4a8d01e`
- `kernel-development-iphonesimulator`: `sha256:8aa6e5a53edf8911bc883bfa65cf22e5c195d15c040216c54cbbcfd0cb65cb5f`

`make __bazel-lock-proposal` created the exact seven-component schema-2 proposal (buildset digest `570bd6425e4f22b9818149b53c882e7782648a4714c53bdec67b7be71031d8a8`), signed it with Cosign into `buildset-lock-proposal.sigstore.json`, verified the bundle with the public key, and asserted `artifacts.lock.json` stayed byte-identical. The workflow uploaded the artifact `signed-buildset-334afabac5ded3572504de84c68028b6b3f80760` (artifact ID `10381488403`, nine files: seven signed component records plus the proposal and Sigstore bundle, 6661 bytes, SHA-256 `f8aef49f553b1eb8d85b164248f1fdba2e59044fdd1119bdda49c628343066e0`).

Publication completed without mutating `artifacts.lock.json`; it still has schema 1 and no seven-component activation. Phase 6B gates now: 6B2 complete protected publication VERIFIED at this head. 6B3 signed schema-2 activation, 6B4 promoted consumer cutover, the four promoted Kernel variants, cold reconstruction, and the zero-download warm reconstruction remain PENDING, and require the activation path `__bazel-lock-from-signed` with `ORLIX_COSIGN_PUB` present and `ORLIX_COSIGN_KEY` absent against the run's uploaded proposal and bundle. TAP remains stopped. The user-owned `third_party/swift/.build/` remains untouched.

## 2026-09-15 signed schema-2 activation

Activated the exact schema-2 proposal from protected run `34927558114` at branch head `334afabac5ded3572504de84c68028b6b3f80760` (branch head at activation: `ec02dfa8602bde870223d637a2bfc5ecc92289d1`, docs-only). Downloaded the run artifact `signed-buildset-334afabac5ded3572504de84c68028b6b3f80760`, placed its exact proposal and Sigstore bundle at `Build/Bazel/promote/buildset-lock-proposal.json` and `buildset-lock-proposal.sigstore.json` (proposal SHA-256 `42478ff1896963e6dc89d0050024944cf88ec7a15daadb67b3e6e9526ee4f8c2`), and ran `make __bazel-lock-from-signed` with `ORLIX_COSIGN_PUB=$HOME/Library/Orlix/cosign/cosign.pub` present and `ORLIX_COSIGN_KEY` absent. The local public key's SHA-256 fingerprint is `7d2b6c7a15b2b8d063a25f4fd9b4004db272f39de46c1b98087de42e343f9e8d`, equal to the `trust-policy.json` `accepted_key_ids` entry, which `trusted_public_key()` enforces before verification. GHCR verification used a local `ORLIX_ORAS_REGISTRY_CONFIG` (read-only job-token auth) attached to `cosign verify` as `DOCKER_CONFIG`; no private key was available to the process.

The activation verified the Sigstore bundle over the exact proposal bytes, required schema 2, the exact seven-component set, component types, OCI digests, artifact identities, trust metadata, the buildset identity, and every component signature, then atomically replaced `artifacts.lock.json`. It called no signing target and regenerated no proposal. The resulting lock is schema 2 selecting buildset `570bd6425e4f22b9818149b53c882e7782648a4714c53bdec67b7be71031d8a8` with exactly the seven OCI digests recorded from run `34927558114`. `uapi`, `mlibc`, and `rootfs` keep `legacy-marker-sha256` identities; the four Kernel variants keep `artifact-identity-v2` identities.

Remaining gates: 6B4 promoted consumer cutover and proof for all four Kernel variants, cold reconstruction from an empty immutable local store, warm reconstruction with zero measured downloads, semantic PR component selection, exact-head canonical Apple CI reproof, and the independent Standards and Spec merge-readiness review. TAP remains stopped.

## 2026-09-15 promoted reconstruction proof and store staging fix

Cold reconstruction: with the promoted store absent (`~/Library/Caches/Orlix/Artifacts` did not exist), `make __bazel-reconstruct` with `ORLIX_COSIGN_PUB` present, `ORLIX_COSIGN_KEY` absent, and a local `ORLIX_ORAS_REGISTRY_CONFIG` executed exactly seven `oras pull` invocations, one per locked component (measured by instrumented `oras`/`cosign` PATH wrappers logging each invocation), and reconstructed all seven trees at `Build/Bazel/reconstruct/570bd6425e4f22b9818149b53c882e7782648a4714c53bdec67b7be71031d8a8/`. The store populated its immutable object records and one active-reconstruction lease. Warm reconstruction: a second `__bazel-reconstruct` run against the same populated store logged zero `oras` and zero `cosign` invocations (the instrumented wrappers never executed), so warm reconstruction performed zero network downloads and zero signature subprocesses by direct invocation-count measurement. `make __bazel-substitute-promoted` staged all seven components into `bazel/promotion/imported/` as symlinks to the reconstructed trees, asserted the promoted-components manifest matched the schema-2 lock exactly, and left `artifacts.lock.json` byte-identical.

Store staging fix: the interrupted schema-2 activation publication had left `.staging/artifact-*/replaced/tree/product` directories with archived `0555` directory modes (the mode-preserving Kernel extraction). `publish.py`'s `os.replace` cleanup and `_cleanup_staging_unlocked` used plain `shutil.rmtree`, which cannot delete read-only directories, so every later store use failed with `Permission denied`. `artifact_store._rmtree_force` now adds owner write permission to every directory before deletion, and all six store removal sites (`_cleanup_staging_unlocked`, the replaced-object cleanup, both staging cleanups, and both `gc` removals) use it. The 9-test artifact-store suite, the 29-test reconstruction/activation/publication/substitute suites, and the full 56-test migration suite pass. No artifact identity changed; `artifacts.lock.json` remains byte-identical.

## 2026-09-15 product toolchain pin moved to Xcode 27.0

Decision: the user removed the local Xcode 26.6 installation and chose to move the product pin to Xcode 27.0. `bazel/config/toolchain-pin.json` now pins Xcode `27.0` build `27A266a` at `/Applications/Xcode-27.0.0-Release.Candidate.app` with iphoneos and iphonesimulator SDKs `27.0` and disk-cache namespace `bazel-9.2.0-xcode-27A266a`; Xcode 26.6 build 17F113 remains an allowed local identity with `product_pin: false`. `toolchain_pin.py` no longer hard-rejects Xcode 27.0. Make defaults (`ORLIX_XCODE_VERSION`, `ORLIX_XCODE_BUILD`, `ORLIX_PINNED_DEVELOPER_DIR`), the `.bazelrc` rules_xcodeproj block, `.bazelrc.local.example`, and `apple-build-matrix.json` follow. The BuildBuddy remote instance namespace now derives its Xcode tail from `$(ORLIX_XCODE_BUILD)` instead of the hardcoded `xcode-17F113`, so the accepted instance is `orlix/apple/bazel-9.2.0/xcode-27A266a/v1` under the tracked epoch. `bazel-ci.yml`, `bazel-promote.yml`, `bazel-benchmark.yml`, `bazel-nightly.yml`, `bazel-gc.yml`, and `testflight-beta.yml` select Xcode 27.0 and assert build 27A266a; the canonical CI current-runtime gate selects iOS 27.0 while the iOS 15.5 runtime proof is unchanged. `Tools/AgentHarness` rulesync-guard tests were updated for the current rulesync-generate automation-app-token authority (the assertion still banned `create-github-app-token` after main commit `a59ce5c6` introduced it, a pre-existing failure on this branch).

The activated schema-2 buildset `570bd6425e4f22b9818149b53c882e7782648a4714c53bdec67b7be71031d8a8` was built and signed under Xcode 26.6 and remains authorized: promotion components are guest-side Kbuild, Meson, and packaging products, and consumption verifies locked OCI digests and artifact identities, not the local app-side Xcode. Whether the Xcode 27.0 pin reproduces the 26.6 feasibility gate is OPEN until canonical Apple CI proves it; any future promotion will build under the new pin.

Docs: the Xcode 26.6 feasibility experiment task is done with a dated resolution note; the feasibility-gate story, migration concept page, and shared-cache task reference the pinned product Xcode; `docs/index.md` regenerated; log entry added.

## 2026-09-15 kernel product link survives Xcode 27 without ld-classic

The Xcode 27.0 toolchain removed `ld-classic`, and its new linker silently ignores `-order_file` (verified with a test binary whose symbol order never changed), so the ordered Orlix kernel product link failed with `ordered Orlix product link requires ld-classic`. The owning change stays in the OrlixKernel port build mechanics.

Changes:

- `OrlixKernel/Sources/ports/orlix/kbuild/product-compile-adapter.mk` now emits one small per-level start-boundary stub object (`orlix-product-boundaries.o.stub.<level>.o`) per initcall level instead of placing those labels inside the aggregate boundary object, adds the stub objects to the ordered link, and drops the `ld-classic` and `-order_file` arguments. The existing `-Wl,-rename_section` arguments are unchanged; both the Xcode 26.6 classic linker and the Xcode 27 linker accept the four-argument form they already produce, so the level sections still merge into one `__DATA,__initcalls` section.
- New `OrlixKernel/Sources/ports/orlix/kbuild/product_initcall_reorder.py` permutes the merged `__initcalls` section after the relocatable link: it sorts the 8-byte entries, their relocations, and the entry symbol values to match the expected upstream order, and repositions the level-start boundary labels before each level's first entry. A second phase permutes whole `__DATA,__sched_class` struct ranges (bytes, relocations, and contained symbols) into the scheduler class priority order. It ignores assembler `ltmp` temporaries, classifies symbols by their owning section index, fails loudly on any unrecognized structure, and is idempotent.
- The link now feeds all product objects in input order; the tool handles objects that register initcalls at multiple levels (13 upstream objects in the current config, e.g. `kernel/resource.c`) and objects that define multiple scheduler classes (`kernel/sched/build_policy.c`), so no upstream source splitting and no maintained file list is required.

Proof: the promoted-mode consumer build `make __bazel-orlix-app` (Xcode 27.0 build 27A266a, buildset `570bd6425e4f22b9818149b53c882e7782648a4714c53bdec67b7be71031d8a8`) passed all promoted-mode IPA checks: the embedded `locked-buildset.json` matches `artifacts.lock.json`, `composition.json` records the locked buildset, the IPA initramfs matches the reconstructed rootfs OCI tree, the embedded OrlixOS framework contains all four kernel variant products and DTBs, and `_OrlixBoot`/`_arch_boot_entry` are defined with no undefined references. The full 71-test promotion suite, the 56-test migration suite, and the config suites pass; `actionlint`, `zizmor`, `make docs-check`, and `make agent-harness-check` pass. The unit-level reorder behavior was additionally verified against hand-built Mach-O objects (entry permutation, stub repositioning, sched struct-range permutation, idempotency, and a final link resolving relocations).

Remaining gates: 6B4 promoted consumer runtime proof, cold/warm reconstruction measurements already recorded above remain valid (the store objects are unchanged), semantic PR component selection, exact-head canonical Apple CI reproof under the new pin, and the independent merge-readiness review. Whether the Xcode 27.0 pin reproduces the 26.6 feasibility gate is exercised by this app build but still requires canonical CI.

## 2026-09-15 product pin returned to Xcode 26.6; canonical CI runner constraint

The Xcode 27.0 product pin failed canonical CI because the GitHub `macos-26` runner image cannot select it: `Could not find Xcode version that satisfied version spec: '27.0'` (runs `35010354505`, `35037598779`). The pin therefore returned to Xcode 26.6 build 17F113 with SDKs 26.5 and namespace `bazel-9.2.0-xcode-17F113`; Xcode 27.0 build 27A266a remains an allowed local identity. `toolchain-pin.json`, Make defaults, `.bazelrc`, `.bazelrc.local.example`, `apple-build-matrix.json`, the release test fixture, the toolchain-pin and matrix tests, the canonical-CI current-runtime gate (iOS 26.5), and the promotion, benchmark, nightly, GC, and TestFlight workflows follow. The BuildBuddy instance namespace stays derived from `$(ORLIX_XCODE_BUILD)` (currently `orlix/apple/bazel-9.2.0/xcode-17F113/v1` under the tracked epoch).

The linker-agnostic kernel ordering from the previous checkpoint is kept unchanged: it does not depend on `ld-classic`, was proven under Xcode 27.0 locally, and uses only mechanisms both linkers accept. Local development on this Mac without Xcode 26.6 can override `ORLIX_XCODE_VERSION`, `ORLIX_XCODE_BUILD`, and `ORLIX_PINNED_DEVELOPER_DIR` to the 27.0 identity, which remains in `allowed_local`.

Also fixed on CI: `test_canonical_workflow_keeps_promoted_inputs_off_pull_requests` asserted lock schema 1 while the activated lock is schema 2; the assertion now accepts both schemas (the PR source-mode rule is unchanged).

## 2026-09-15 semantic PR component-mode selection

`bazel/migration/component_selection.py` now selects the Bazel component mode deterministically from the changed paths and the committed lock schema. A pull request selects promoted mode only when the committed lock is schema 2 AND every changed path is proven outside the promoted foundations: `OrlixKernel/`, `OrlixMLibc/`, `OrlixCoreUtils/`, `OrlixOS/Sources/make`, `OrlixOS/Sources/init`, `OrlixOS/Sources/patches`, `OrlixOS/Sources/distribution`, `OrlixOS/Makefile`, `OrlixOS/Tests`, `bazel/feasibility/`, `bazel/promotion/`, `bazel/config/`, `bazel/extensions/`, `bazel/migration/`, `make/`, `.github/workflows/`, `third_party/patches/`, `xcode/`, plus the build files `artifacts.lock.json`, `upstreams.lock.json`, `MODULE.bazel*`, `.bazelrc*`, `Makefile`, `Brewfile`, `Gemfile*`, and `project.yml`. Proven app-only surfaces are `Orlix/`, `OrlixHostAdapter/`, `OrlixOSTestApp/`, `OrlixTestApp/`, `OrlixOS/Sources/include`, `OrlixOS/Sources/Session`, `third_party/swift/`, `docs/`, `Resources/`, `fastlane/`, `Tools/`, and the repository docs files. Unknown paths, an empty path set, a failed changed-path query, or any mixed change select source mode conservatively. Non-pull-request builds keep the schema rule: schema 2 selects promoted mode, schema 1 selects source.

`.github/workflows/bazel-ci.yml` reads the pull-request changed paths through the API (paginated), runs `bazel/migration/select_component_mode.py`, exports the mode, writes the selection evidence (event, lock schema, changed paths, selected mode) into the job step summary, and records the same evidence via `--out`. Promotion of a failed changed-path query stays conservative: source mode, without failing the pull request.

Verification: the 7-test selection suite and the full 63-test migration suite pass, `make __bazel-migration-inventory-check`, `make docs-check`, and `make agent-harness-check` pass, and `actionlint .github/workflows/*.yml` passes.

## 2026-09-15 exact-head canonical Apple CI and merge-readiness review

Canonical Apple CI passed at exact branch heads: run `35059400872` at head `a5fbf4f2f07940e9ad5ee6c37e050d4a54dcbdb2` and run `35066386463` at head `22aa021623a8f47f7864b0170b2c8dc44a71190d` (pull-request CI, source mode, `Bazel matrix check` job success). Each run selected Xcode 26.6 on the runner, exercised the linker-agnostic kernel product link (no `ld-classic` dependency), built one Orlix.app, and proved the same application on the current iOS 26.5 runtime and the iOS 15.5 runtime with `launch.alive` process-liveness verification and `Orlix-*.ips` crash-report detection. The component-mode selection ran under the semantic selector (PR #230 touches promotion foundations and correctly selected source mode); RuleSync generated-output guard passed at both heads.

Independent Standards and Spec review findings and their dispositions:

- `Tools/` was wrongly app-only in the semantic selector; agent-harness changes gate promotion behavior. `Tools/` now selects source mode conservatively, with a regression test. docs/ stays app-only (pure knowledge).
- The promotion task page still claimed a GitHub build-provenance attestation; the Codex review correction (commit `334afaba`) deliberately removed that attestation path and its token permissions, with the Sigstore bundle as promotion provenance. The page now states the current policy.
- Standards review findings accepted as deliberate or deferred: the `.bazelrc.local` symlink is the sanctioned BuildBuddy credential mechanism (worktree-local, gitignored, cleaned by `__bazel-buildbuddy-cleanup` under `always()`); the post-link initcall/sched reordering tool is product-link mechanics under ADR 0020, not a replacement of upstream build engines (Kbuild, Meson, and the autotools engines still run unchanged); the duplicated seven-component lists in the Make recipes are asserted by tests and deferred to a later refactor checkpoint; the `--insecure-ignore-tlog` blob verification is the documented `protected-cosign-key` trust model in `trust-policy.json`.
- Spec-review scope-cream findings (kernel-link rework, credential rework in release workflows) were forced by the Xcode 27 toolchain move and the PR review corrections, respectively, and are recorded in their own checkpoints above.

Phase 6B gate status after this checkpoint: 6B2 complete protected publication VERIFIED (run `34927558114`); 6B3 schema-2 activation VERIFIED and committed; promoted-consumer consumption, cold reconstruction (7 downloads), and warm reconstruction (0 downloads) VERIFIED; semantic PR selection VERIFIED in source; exact-head canonical Apple CI VERIFIED (`35066386463` at `22aa0216`). Independent Standards and Spec review complete with the dispositions above. The remaining unproven set is post-merge only: trusted-main BuildBuddy writes, useful remote hits, transfer-byte accounting, and the monthly usage projection. TAP remains stopped.

## 2026-09-16 [CORRECTION] activated buildset is stale; Phase 6B reopens

[CORRECTION]: the committed schema-2 buildset `570bd6425e4f22b9818149b53c882e7782648a4714c53bdec67b7be71031d8a8` (promoted at `334afaba`, activated in `154c99d2`) predates commit `418f2d43`, which changed Kbuild-owned Kernel product construction (per-level boundary stubs, input-ordered link inputs, post-link section reorder). The earlier checkpoint claim that the buildset "remains authorized" holds only for app-side Xcode changes, not for component-producing build-system changes. The four signed Kernel OCI artifacts in `artifacts.lock.json` were produced before the current Kernel build algorithm existed, so the current promoted Kernel products are STALE RELATIVE TO CURRENT KERNEL BUILD MECHANICS. Independent review also found: PR #230 is merge-BLOCKED on three unresolved non-outdated review threads (runtime liveness, TestFlight base-fetch auth, Grok base-fetch auth); canonical CI checks out the PR merge result, not the exact head; and the previous "merge-ready" claim is withdrawn.

Corrected status:

```text
Phase 6B infrastructure: IMPLEMENTED
original 7-component promotion: VERIFIED @ 334afaba
schema-2 activation: VERIFIED (superseded buildset, retained as evidence only)
cold/warm reuse: VERIFIED for buildset 570bd642... (superseded buildset)
Kernel-link rework: IMPLEMENTED + SOURCE-MODE CI VERIFIED
current promoted Kernel products: STALE RELATIVE TO CURRENT KERNEL BUILD MECHANICS
current-head promoted buildset: NOT YET VERIFIED
canonical Apple CI: GREEN but merge-result proof, not literal head checkout
review implementation findings: apparently fixed
review-thread resolution: INCOMPLETE
trusted-main BuildBuddy proof: CORRECTLY POST-MERGE
PR #230: NOT READY TO MERGE
```

Recovery order (promotion last): correct status -> fix producers/selectors/verifiers (PR-head checkout pin; source-safe selector on PR + main push; Mach-O rewriter section-ownership fix + regression suite; seven artifact-identity-v2 + payload boundaries; restored XCTest under one canonical app; real CycloneDX 1.6 SBOM + SLSA provenance + toolchain/proof-index binding with byte-verifying activation; measured combined GC; canonical component registry) -> make evidence truthful -> freeze component-affecting code -> promote exact frozen HEAD -> activate exact signed evidence -> prove promoted consumption -> exact-head canonical CI -> resolve review threads with owning evidence -> stop before merge. TAP remains stopped. No Phase 7 work.

## 2026-09-16 recovery Steps 0-8 implemented, promotion frozen pending

Steps 0 through 8 are committed at `88e7ee13` (nine commits from `60ed374b`). No protected promotion has been dispatched; the committed lock remains the superseded schema-2 buildset `570bd642…` and must not be treated as authorizing the current tree.

- Step 0 (`60ed374b`): `[CORRECTION]` marking buildset `570bd642…` stale relative to `418f2d43`; Phase 6B reopened with corrected status labels.
- Step 1 (`028986af`): PR checkout and evidence artifact pinned to `${{ github.event.pull_request.head.sha || github.sha }}` with policy tests.
- Step 2 (`2efaae32`): selector state machine has no event-name shortcut. PRs use the files API only when the count matches `changed_files`; pushes use the `before..after` git range; dispatch, empty, unknown, or failed discovery select source. Eight-case suite green.
- Step 3 (`c5a0face`): Mach-O rewriter requires owning `n_sect` for scheduler-class rewrites (initcall path already section-indexed; relocations validated on fast paths too); 10-test synthetic-Mach-O regression suite green.
- Step 4 (`a791bbae`): all seven components promote through `artifact-identity-v2` with exact-manifest payload staging (`stage_v2_product.py`); zero `legacy-marker-sha256` references in Make promotion paths and workflows; schema-2 validation rejects legacy everywhere.
- Step 5 (`ce398f0c`): XCTest smoke executes on both runtimes against the one canonical app (cquery identity equality gate + BEP pass assertion + proof-record identity check). Runtime execution itself is proven only by canonical CI (Step 11), not locally.
- Step 6 (`abf769ce`): deterministic CycloneDX 1.6 SBOM (software only, serial from component identity) and SLSA v1 in-toto provenance (source, builder, invocation, materials, toolchain); `promotion-proof-index.json` binds source, toolchain, seven v2 identities, A/B evidence, executed gates, workflow/run/policy identity; proposal binds toolchain/proof/SBOM/provenance digests; activation recomputes and rejects on absent/mismatch/wrong-schema/wrong-names.
- Step 7 (`5b68764f`): GC measures prepared (`tree/`) and promoted (blob+records) bytes with combined accounting and an overflow-eviction regression test.
- Step 8 (`88e7ee13`): `bazel/promotion/components.json` is canonical; Make loops/labels/facts and the workflow publish loop derive from it; eval lines, YAML artifact list, and Python tuples are parity-tested; eighth-component flow-through proven.

Verification at this head: 89 promotion + 70 migration + 7 config + 10 Mach-O + 7 release tests green; `actionlint`, `docs-check`, `agent-harness-check`, `__bazel-migration-inventory-check` green. Local Xcode remains 27.0-only, so Xcode-26.6-gated Make targets were not executed locally; their proof comes from canonical CI.

UNPROVED: exact-head promotion at the frozen head, activation of its proposal, cold/warm/consumer proof against the new buildset, exact-head canonical CI, and review-thread resolution. The freeze rule is in effect: any component-, identity-, registry-, toolchain-, proof-, or promotion-affecting change restarts Step 9 from a new head.

## 2026-09-16 Xcode discovery and selection no longer guesses paths

[CORRECTION]: the repository hardcoded `/Applications/Xcode-26.6.0.app/Contents/Developer` as the `ORLIX_PINNED_DEVELOPER_DIR` default, hardcoded `26.6` in Make defaults and six CI workflows, and duplicated hardcoded Xcode paths through the `.bazelrc` rules_xcodeproj block. Xcode discovery is now `xcodes`: committed `.xcode-version` (`26.6`) is the single requested-release source; `ORLIX_XCODE_VERSION` defaults from that file; `ORLIX_PINNED_DEVELOPER_DIR` defaults from the observed `xcode-select -p`; `make __xcode-select` runs `xcodes select` then verifies; `__bazel-feasibility-bootstrap` verifies via `bazel/config/xcode_select.py` (xcodes presence, requested version installed, env dir equals selected dir, exact `Xcode 26.6` / `Build version 17F113`). Missing `xcodes` or a missing Xcode fails with the install command; a wrong version, wrong build, or stale 27-pointing directory fails without fallback; Xcode 27 can never leak into an acceptance build. CI reads the version from `.xcode-version` into the (temporarily retained) setup-xcode provisioning step, exports the observed directory, and runs the same repo verifier after tool installation (Brewfile already vendors `xcodesorg/made/xcodes`). `ORLIX_RELEASE_XCODE_VERSION` now defaults from `.xcode-version` in `make/release.mk`. `17F113` remains only as the explicit accepted-build verification constraint. The `.bazelrc` hardcoded block is removed; the xcodeproj target already passed equivalent flags explicitly. Verified locally read-only (`xcode_select.py verify` reports 26.6/17F113 from this machine's selected Xcode without changing selection); 10 selection unit tests plus routing/policy coverage green. This change precedes protected promotion because Xcode selection is part of the toolchain identity the promotion proof binds.

[RETRY 2026-09-17]: branch-wide search found one remaining guessed path outside the first pass: `bazel/extensions/native_sources.bzl` defaulted `DEVELOPER_DIR` to `/Applications/Xcode-26.6.0.app/Contents/Developer`. The default is now empty with an explicit fail refusing to guess an Xcode path; the no-constructed-paths selection test now also scans `bazel/extensions` and `.bzl` files, and `test_native_sources.py` pins the refusal. [CORRECTION]: this Mac currently has only Xcode 27.0 RC (`27A266a`) installed and selected; Xcode 26.6 is absent, so live read-only verification correctly fails with `Xcode 26.6 is not installed; install with: xcodes install 26.6` and never falls back to 27. Selection plus native-sources tests (17), `actionlint`, `docs-check`, `agent-harness-check`, and the migration inventory check are green. The one `test_workflow_policy` failure and the inventory drift reproduce only with the unrelated uncommitted Step 5 XCTest rewrite (`ORLIX_CANONICAL_IPA_REL` to `ORLIX_CANONICAL_APP_PATH`) in the worktree and are untouched by this change. Do not promote `d96fb8ea`; Step 9 starts from the new frozen HEAD below.

[CORRECTION 2026-09-17]: `78285320` was claimed as the frozen Steps 0-8 head while Step 5 files (`make/bazel-migration.mk` XCTest rewrite, `make/xctestrun.py`, `make/test_xctestrun.py`) were still uncommitted. That claim was false and is withdrawn; the paragraph above mischaracterized the Step 5 rewrite as unrelated. The selector was also rewritten per review: the hand-written `xcodes installed` listing parser and exact token-membership check are removed. Selection now runs `xcodes select --print-path` in the repository root so xcodes consumes `.xcode-version` itself, and the read-only preflight uses xcodes' direct lookup `xcodes installed <version>`; the configured Xcode directory is honored by inheriting the environment and never passing `--directory`. Resolution failure now reports `xcodes could not resolve Xcode 26.6 in its configured Xcode directory` with the `xcodes install 26.6` hint and never claims global absence. Live evidence on this machine: `xcodes installed` lists only `27.0 Release Candidate (27A266a)`, `xcodes installed 26.6` fails, filesystem and Spotlight show no 26.6 Xcode, so the live verifier fails at xcodes' own resolution step; `--select` was not run live because it changes machine selection and may prompt for privilege elevation. Step 5 is finished and committed: the XCTest proof resolves the test bundle through the tested `xctestrun.py --resolve-test-bundle` helper (no inline resolver), generates the xctestrun against the canonical app path, asserts host identity, runs `xcodebuild test-without-building` on the pre-booted simulator UDID for both current and iOS 15.5 runtimes, and the runtime-proof writer checks both per-label identity files against the same canonical app path (`same_application_path`/`xctest_app_path`, compile count 1). `test_xcode_select` (16) and `test_xctestrun` (5) now run in `__bazel-matrix-check`; the workflow-policy and routing tests track the `ORLIX_CANONICAL_APP_PATH` contract; the migration inventory was refreshed. The full Python owning-test battery, `actionlint`, `docs-check`, `agent-harness-check`, and the inventory check are green. Local and CI runtime execution remains CI-owned. Only the new HEAD below may become the Step 9 promotion candidate; promotion stays undispatched pending review.

[DIAGNOSIS 2026-09-17]: exact-head CI `35206333133` at `acbfe810` failed in `toolchain_pin.capture_manifest()` when `/usr/bin/xcrun --sdk iphonesimulator --show-sdk-build-version` exceeded the hard-coded 30s timeout (raw `TimeoutExpired` traceback, `__bazel-feasibility-bootstrap`). Hypotheses ranked and tested: (1) transient runner stall; (2) cold first-xcrun cost; (3) simulator runtime-install contention; (4) pathological query. A no-code rerun of the same SHA passed `capture_manifest()` and failed later in analysis-test Swift compilation (different bug, out of scope), confirming (1). A temporary dispatch-scoped probe workflow measured all 17 xcrun discovery commands at post-selection, post-runtime-install, and post-simulator-boot stages: 51/51 fast (stage maxima 3.3s/3.4s/2.8s; the failing query 1.0s/0.1s/0.2s), falsifying (2), (3), and (4). Fix from evidence: timeout stays at the measured 30s (not increased); `observe()` retries once on `TimeoutExpired` only (bounded: at most 2 attempts per command) and wraps all failures in `PinError` reporting command, Xcode version/build, SDK, timeout, and elapsed time with no traceback leak and no environment dump; `CalledProcessError` is not retried. Regression tests cover retry-then-success, persistent-timeout reporting (message content, output file untouched, exactly 2 attempts), and no-retry-on-exit-failure. `acbfe810` is therefore no longer the frozen candidate; only the new HEAD below may enter Step 9. The temporary probe branch `ci/xcrun-timing-probe` was deleted locally; its remote branch still needs manual deletion. Observation (not changed): `capture_kernel_manifest()` uses `check_output` with no timeout at all.

[LIFECYCLE 2026-09-17]: exact-head CI `35216011066` at `c738e5b` reproduced the discovery stall after the retry change (`macosx --show-sdk-version`, elapsed 67.3s over 2 attempts, same `__bazel-feasibility-bootstrap` site). Retry improved the diagnostic but is not the reliability mechanism, so no more retries and no timeout increase. Toolchain capture moved to its correct lifecycle point: new Make-owned `__bazel-toolchain-manifest` verifies Xcode through the `xcodes` contract, bootstraps the pinned Bazel, captures `Build/Bazel/toolchain.json` atomically once per run (temp file plus rename), and validates the result. `toolchain_pin` gained `validate_manifest()` (run id, developer dir, accepted Xcode/Bazel identity; never captures) and `ensure_current_manifest()` (reuse when current, replace when stale, never reuse stale). `ORLIX_BAZEL_RUN_ID` binds a manifest to its run (`github.run_id`-`run_attempt` in CI, uuidgen locally). Canonical CI captures before simulator-runtime installation/boot; `__bazel-feasibility-bootstrap` and all promotion entry points consume the manifest through the same operation. Policy/routing tests prove the order Xcode, tools, manifest, runtimes, Apple CI, and the promotion path.

[XCTESTRUN 2026-09-17]: [CORRECTION] the rerun failure first described as a Swift analysis-compile failure was not that: the `.standard(proto:)` lines are warnings in third-party generated protobuf code, and all analysis tests passed. The actual failure was deterministic in `__bazel-xctest-runtime-proof`: `xcodebuild test-without-building` rejected the hand-written xctestrun because the UI test host was the app under test instead of an XCTRunner host and selection used a two-part `-only-testing` flag the transient scheme cannot resolve. The generator now mirrors rules_apple's XCUITEST runner: it assembles `<Module>-Runner.app` from the selected Xcode's XCTRunner.app plus UI-testing frameworks (paths derived from the verified developer directory only), stages the bundle in the runner's PlugIns, references the canonical app as UITargetAppPath, and carries selection as in-file OnlyTestIdentifiers/OnlyTestingIdentifiers. The `-only-testing` flag is gone and derived data is contained per label. `c738e5b` is therefore not a Step-9 candidate; only the new HEAD below may enter Step 9 after exact-head CI proves manifest capture, the analysis tests, and the restored XCTest path.

[RECIPE 2026-09-17]: exact-head CI `35228235185` at `bcac76d` passed checkout, Xcode 26.6/17F113 selection, the pre-simulator toolchain manifest, source mode, and all 36 analysis tests, then failed in `__bazel-feasibility-bootstrap` on a shell/Python quoting defect: the `>/dev/null` redirection sat inside the `python3 -c` source string, raising `SyntaxError`. Fixed to shell position. A branch-wide audit of all 45 `python3 -c` invocations found no sibling defect. Regression coverage at the Make seam: every `python3 -c` payload in the Makefiles must compile after mechanical make-variable substitution with no shell operators inside, and the exact bootstrap validate recipe executes against a fixture manifest (both proven red-capable against the old shape). `bcac76d` is therefore not a Step-9 candidate; only the new HEAD below may be considered after exact-head CI reaches the restored XCTest path.

[XCTEST-PROFILE 2026-09-17]: exact-head CI `35228235185` at `c3fc98c` proved checkout, Xcode 26.6/17F113 selection, pre-simulator manifest capture (`toolchain manifest: 26.6 17F113 bazel 9.2.0`), source mode, 36/36 analysis tests, bootstrap manifest consumption, and the canonical app compile, then failed deterministically in `__bazel-xctest-runtime-proof` (mk:905, exit 70): `Failed to build workspace temporary with scheme Transient Testing.: No output directory for profile data was provided.` Diagnosis: the Runner-hosted xctestrun declares coverage buildables but omits `ClangProfileDataDirectoryPath`, which rules_apple's template always emits; the previous scheme-membership error is gone, so Runner assembly plus in-file selection resolved the test plan. Fix: the generator emits the key into a per-label `coverage` directory it creates. `c3fc98c` is therefore not a Step-9 candidate; only the new HEAD below may be considered after exact-head CI proves the XCTest path.

[XCTEST-INSTALL 2026-09-17]: exact-head CI `35245030397` at `724fa31` proved checkout, Xcode 26.6/17F113 selection, pre-simulator manifest capture, source mode, 36/36 analysis tests, bootstrap consumption, and the canonical app compile, then executed the test plan for 46s and failed installing `OrlixUITests-Runner.app`: `Missing bundle ID`. Diagnosis: the byte-level Info.plist patch assumes settable `WRAPPEDPRODUCT*` variables, which the failing Xcode's shipped plist evidently does not carry, so the assembled runner kept an unresolvable identifier. Fix: resolve plist symlinks before patching, then parse the result and set `CFBundleIdentifier`/`CFBundleName`/`CFBundleExecutable` explicitly when the shipped form does not resolve. Unit coverage pins the assembled identifier with and without shipped placeholders, and the Make-seam assertion now checks the install-blocking identifier on the assembled runner. `724fa31` is therefore not a Step-9 candidate; only the new HEAD below may be considered after exact-head CI proves the XCTest path.

[XCTEST-ROOT 2026-09-17]: exact-head CI `35253357159` at `c594703` proved checkout, Xcode 26.6/17F113 selection, pre-simulator manifest capture, source mode, 36/36 analysis tests, bootstrap consumption, the canonical app compile, Runner installation, and `** TEST EXECUTE SUCCEEDED **` on the current runtime, then failed writing `runtime-proof.json`: `xctest evidence mismatch for current`. Diagnosis, reproduced locally with perfect fixture evidence: the writer rooted label lookups at `Path(sys.argv[1])`, which names the `runtime-proof.json` file, so `root/"current"/...` could never exist. One-word fix: root at the proof file's parent. The Make-seam test executes the exact writer recipe against a fixture (red on the old shape, green on the fixed one). `c594703` is therefore not a Step-9 candidate; only the new HEAD below may be considered after exact-head CI proves both XCTest runtimes plus liveness.

[IOS-BUILDER 2026-09-18]: the branch intentionally advanced past `5873bac` with MobAI ios-builder (`6e82f5a`: ios-build.yml, ios-share.yml, builder.json), so `5873bac`+`35264415890` is historical evidence, not the Step-9 candidate. Upstream contract established from source: `builder init` unconditionally rewrites both workflows and `Save`s builder.json (dropping unknown fields, so no custom Orlix fields there); dispatch sends the 10 declared inputs (empty omitted) and always runs the default-branch workflow against a snapshot ref whose parent is HEAD; tag builds run the tagged commit's workflow with builder.json params. Adaptation keeps the CLI contract intact (all inputs declared, snapshot/tag/artifact/mobai-ci semantics unchanged) and routes the native path through Make/Bazel: `orlix` detection from repo files; repo-pin Xcode (never latest-stable); declared Brewfile deps; run-bound toolchain manifest before product work; snapshot component mode from the existing classifier (status plus snapshot-parent plus main merge-base diffs, uncertainty forces source); `__bazel-product-app` stages the canonical app at a stable path; `__builder-package-ipa` zips the unsigned device IPA to `build/<id>.ipa`; share hands the Bazel sim app to `mobai-ci share`. Signed builder builds fail clearly toward the Orlix-owned release flow (no IOS_* sets exist). Xcode schemes own nothing. Security: all actions SHA-pinned (20 unpinned fixed), persist-credentials removed with per-command auth, snapshot fetch depth 2 for the parent diff, no pull_request trigger, least-privilege contents:write kept for tag deletion. Remaining zizmor notes are pre-existing upstream cache/adhoc findings on generic steps the Orlix path does not consume. Dispatch-path proof is pending merge (workflow must exist on the default branch); tag-path proof to follow on the candidate HEAD.

[SIGNING-TRUTH 2026-09-18]: no Apple signing secrets exist in this repository (only GROK/ORLIX automation, BuildBuddy, and cosign secrets), so no Bazel device product can link on a fresh runner today: `ios_application` requires device signing and the team development profile/certificate are absent. The orlix build branch therefore preflights Apple Development identity plus team profile presence and fails clearly before the long build; on success the staged copy is signature-stripped (proven unsigned by assertion) so the builder IPA honors the unsigned contract for sideload distribution without recompiling. Signed builder requests fail toward the Orlix-owned TestFlight/release flow. Simulator share needs no signing. `MOBAI_API_KEY` is likewise absent, so the final `mobai-ci share` call needs user provisioning.

[MODULE-LOCK 2026-09-18]: the ios-share tag run failed bootstrap's `mod deps --lockfile_mode=error`: the committed lock still carried the `native_sources` digest from before the Xcode-path fix, which changed that file without refreshing the lock. Reproduced locally and refreshed via the owned `__bazel-module-lock-update` target (single-digest hunk; the co-occurring pybind11 path-form difference is output-base layout, stable under the repo-relative CI layout). Open question, stated honestly: several earlier runs passed error-mode with the same stale lock, so the check appears evaluation-state dependent; the guard itself is sound (it just fired correctly) but its past silence is unexplained. Lesson: any `.bzl` change must refresh the lock in the same commit.

[MAKE-AT 2026-09-18]: the share run then built the simulator product successfully and failed only on a stray `@` before the final `echo` of `__bazel-product-app`: proven with gmake 4.x that a leading `@` is honored on a recipe's first physical line and on backslash-continued lines, but reaches the shell literally on a final line (`@echo: command not found`). One-line fix plus a static regression test encoding exactly that rule (proven red against the defect shape).

[PROMOTE-V2 2026-09-18]: Step 9 run `35323626070` at `3000601` built all sides cleanly, then failed the first A/B comparison: tree-mode identity manifests legitimately contain directory entries (the generator test asserts their presence), which the file-only promotion validator rejects. This gate never went green before (the September success predates the seven-component design), so there is no regression. Fix at the source, gate untouched: the promotion serializer gains opt-in files-only tree manifests (directories implicit, symlinks fail loudly instead of vanishing), and the Bazel identity action uses it. Validator, stage, digest equality, and all strict checks are unchanged. New HEAD required below; canonical CI must re-verify before any further Step 9 attempt.
