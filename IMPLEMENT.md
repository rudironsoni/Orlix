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
