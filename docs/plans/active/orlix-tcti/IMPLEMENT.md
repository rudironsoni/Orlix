# IMPLEMENT.md

## 2026-07-01

### Checkpoint: Executable-Proof Plan Tightening

- Updated `docs/plans/active/orlix-tcti/PLAN.md` so the next TCTI checkpoint cannot be completed by documentation alone.
- Recorded current source reality:
  - `hosted_exec.c` already includes `<asm/tcti.h>`.
  - `orlix_hosted_enter_user()` already calls `orlix_tcti_enter_user(regs)` when `CONFIG_ORLIX_HOSTED_EXEC_TCTI` is enabled.
  - `include/asm/tcti.h` and `hosted_exec/tcti/engine.c` already exist.
  - `orlix_tcti_enter_user()` compiles and links in the development KUnit path, but is not runtime-proven.
- Corrected syscall handoff plan:
  - current `orlix_syscall_dispatch(regs)` already writes return value, polls timer, runs `orlix_exit_to_user_mode_work(regs)`, and calls `forget_syscall(regs)` when appropriate.
  - TCTI must return to its loop without invoking exit-to-user work again after calling the current helper.
  - added a required test for no double-run of exit-to-user work.
- Added executable proof obligations:
  - `make tcti-contract`
  - `make tcti-golden-elf`
  - `make tcti-diff-switch`
  - `make tcti-memory-fuzz`
  - `make tcti-direct-chain-fuzz`
  - `make tcti-appstore-safety-audit`
  - `make tcti-report-schema-check`
- Added JSON-first report contracts, reducer artifact requirements, and failure-reduction rules before physical-device debugging.
- Added hard guardrails for:
  - host `x18/w18` token and object-disassembly audit
  - guest `TPIDR_EL0` versus host `TPIDR_EL0`
  - single TCTI runner per `mm` for milestone 1
  - `orlix-aarch64-v1` virtual CPU determinism
  - host-page-size fuzzing for 4 KiB, 16 KiB, and 64 KiB
  - virtio as device/I/O only, not CPU model or syscall escape hatch
  - App Store safety as an audit target, not a claim from prose
- Noted current defconfig state is premature for product defaults: development and release currently enable TCTI/debug switch, while the revised plan requires gated defaults until autonomous and physical first-syscall gates pass.

### Evidence

- `rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" make -f OrlixKernel/Makefile kunit PROFILE=development` exited 0 after the docs patch.
- No simulator runtime proof was performed in this checkpoint.
- No physical-device runtime proof was performed in this checkpoint.

## 2026-06-30

### Checkpoint: Plan And Backend Boundary

- Created active plan `docs/plans/active/orlix-tcti/PLAN.md`.
- Began clean-room Orlix TCTI implementation under `arch/orlix`.
- Preserved current uncommitted local changes. No unrelated revert.

### Correction: Expanded Active Plan

- Replaced the initial short active plan with a full decision-complete TCTI execution plan.
- New `PLAN.md` records exact inspected files, files to add/change, hosted-exec integration point, syscall dispatch target, TCTI ABI, hot-register candidates, user-page backing API, block cache, generation model, TLB lifetime, invalidation rules, direct chaining rollout, runtime gates, physical-device proof target, benchmark ladder, tests, and reference review.
- The expanded plan is 1061 lines and covers the ADR 0022 pivot at the required level of detail.

### Current Implementation Slice

- Added TCTI config/build scaffolding.
- Added TCTI public arch interface and private backend headers.
- Added debug switch backend as a correctness oracle only.
- Added `FETCH`, `READ`, and `WRITE` user-page helper API stub under `arch/orlix/mm`.
- Wired `orlix_hosted_enter_user()` to select TCTI only when explicitly configured.

### Evidence

- `git diff --check` passed.
- `bash -n tools/runtime/orlix-runtime-validation.sh` passed.
- `make -n runtime-validation` parsed and delegated to `tools/runtime/orlix-runtime-validation.sh`.
- `make -f OrlixKernel/Makefile __kernel-archive PROFILE=development ORLIX_KERNEL_ARCHIVE_PLATFORMS=iphonesimulator` passed after fixing the missing `task_pid_nr` include in `tcti/report.c`.
- The build compiled the new TCTI files: `engine.c`, `report.c`, `switch_debug.c`, `decode_aarch64.c`, `block_cache.c`, `tlb.c`, `tcti_user_page.c`, and `tcti_invalidate.c`.
- Not yet runtime-ready.
- Not yet physical-device validated.
- No performance claim is made from this checkpoint.

## 2026-07-01

### Checkpoint: Logical Immediate, Conditional Select, And Bounded Simulator Gate

- Continued the TCTI plan simulator-first.
- Kept the implementation in `arch/orlix` TCTI-owned code and the Orlix runtime-validation harness.
- Added class-based decode and debug-switch execution for AArch64 logical-immediate instructions:
  - `AND` immediate.
  - `ORR` immediate.
  - `EOR` immediate.
  - `ANDS` / `TST` immediate through flag-setting logical semantics.
- Added a local AArch64 logical-immediate bitmask decoder. Guest text remains host data and no host executable mapping is introduced.
- Updated `tools/orlix-a64-opprofile/orlix-a64-opprofile` so its classifier matches current TCTI support for:
  - `ANDS` / `TST` / `MVN` logical shifted-register aliases.
  - logical-immediate instructions.
- Refreshed `docs/plans/active/orlix-tcti/tcti-opprofile-init.md`.
- Added class-based decode and debug-switch execution for AArch64 conditional-select instructions:
  - `CSEL`.
  - `CSINC` / `CSET` aliases.
  - `CSINV`.
  - `CSNEG`.
- Used local `clang -target aarch64-linux-gnu` plus `xcrun llvm-objdump` to verify conditional-select sample encodings because `xcrun llvm-mc` is unavailable in this Xcode install.
- Kept observed `/init` words only as focused KUnit regression samples. The implementation uses AArch64 masks and fields, not device-specific candidate lists.
- Fixed product-adapter compile issues found by the simulator archive gate:
  - Added a forward declaration for `tcti_condition_passed()`.
  - Removed the product-context dependency on `U64_MAX` in the logical-immediate helper.
- Replaced the simulator bootstatus shell-loop timeout in `tools/runtime/orlix-runtime-validation.sh` with a Python `subprocess.run(..., timeout=...)` wrapper so simulator validation fails bounded instead of hanging.

Evidence:

- `bash -n tools/runtime/orlix-runtime-validation.sh` passed.
- `git diff --check` passed.
- `python3 -m py_compile tools/orlix-a64-opprofile/orlix-a64-opprofile` passed.
- `make -f OrlixKernel/Makefile kunit PROFILE=development` passed after the final changes.
- `make -f OrlixKernel/Makefile __kernel-archive PROFILE=development ORLIX_KERNEL_ARCHIVE_PLATFORMS=iphonesimulator` passed and wrote:
  - `Build/OrlixKernel/development/iphonesimulator/OrlixKernel.a`.
  - `Build/OrlixKernel/development/linux-object-manifest.txt`.
- Corrected `/init` opcode profile now reports these current supported classes:
  - `logical-shifted-register`: 4386.
  - `logical-immediate`: 462.
  - `conditional-select`: 674.
- The remaining top unsupported profile classes are:
  - `unsupported-unknown`: 2055, currently including SIMD, multiply, bitfield, and conditional-compare examples.
  - `unsupported-load-store-pair`: 9, currently FP/SIMD pair examples.
- `xcode-storage-doctor` still fails one environment check:
  - `CoreSimulator Caches is not mounted at /Library/Developer/CoreSimulator/Caches`.
- Simulator-first runtime gate was retried on the existing simulator:
  - simulator: `Orlix-iPhone-15-Pro-Max`.
  - UDID: `4C88CA42-EA50-463F-B989-7B0560075A9B`.
  - command: `ORLIX_SIMULATOR_ID=4C88CA42-EA50-463F-B989-7B0560075A9B make runtime-validation PROFILE=development DESTINATION=iphonesimulator GATE=tcti-init-first-syscall ORLIX_RUNTIME_GATE_CAPTURE_SECONDS=30 ORLIX_SIMULATOR_BOOT_TIMEOUT_SECONDS=90`.
  - report: `Build/Reports/runtime/tcti-init-first-syscall-20260701T005543Z.md`.
  - result: failed before app install or launch because simulator bootstatus did not reach a terminal state within 90 seconds.
  - bootstatus artifact ended in non-terminal Data Migration, including `00LaunchServicesMigrator`.

Still not complete:

- No simulator TCTI runtime evidence exists from this checkpoint because Orlix did not install or launch on the simulator.
- No physical iPhone retest was run because the requested simulator-first validation is still blocked before app launch.
- TCTI is not runtime-ready.
- No product runtime readiness or performance claim is made.

### Checkpoint: TCTI Compile Gate After User-Page Helper Fix

- Replaced the first-slice `arch/orlix/mm/tcti_user_page.c` implementation with a compileable `FETCH`/`READ`/`WRITE` resolver.
- The resolver enforces Linux VMA permissions first, then PTE presence, user, execute, and write bits as appropriate for the access class.
- Guest instruction fetch still reads guest text as host data and does not request host executable mappings.
- `tcti_pin_user_page()` remains a first-slice helper. It records the resolved backing pointer and page metadata, but it does not yet provide the final pinned lifetime model required by the full TCTI TLB design.

Evidence:

- `git diff --check` passed.
- `bash -n tools/runtime/orlix-runtime-validation.sh` passed.
- `make -f OrlixKernel/Makefile __kernel-archive PROFILE=development ORLIX_KERNEL_ARCHIVE_PLATFORMS=iphonesimulator` passed.
- The archive compile included `arch/orlix/hosted_exec/tcti/engine.c`, `report.c`, `switch_debug.c`, `decode_aarch64.c`, `block_cache.c`, `tlb.c`, `arch/orlix/mm/tcti_user_page.c`, and `tcti_invalidate.c`.

Still not complete:

- TCTI is not runtime-ready.
- The physical iPhone `tcti-init-first-syscall` gate has not passed.
- No TCTI performance claim is supported yet.

### Correction: No Separate TCTI Profile

- Removed the attempted separate `tcti` kernel profile approach.
- Backend selection is now part of both real kernel product profiles:
  - `OrlixKernel/Sources/ports/orlix/configs/development_defconfig`
  - `OrlixKernel/Sources/ports/orlix/configs/release_defconfig`
- Both configs set native hosted execution off and Orlix TCTI on:
  - `# CONFIG_ORLIX_HOSTED_EXEC_NATIVE is not set`
  - `CONFIG_ORLIX_HOSTED_EXEC_TCTI=y`
  - `CONFIG_ORLIX_TCTI_DEBUG_SWITCH=y`
- `OrlixKernel/Sources/ports/orlix/configs/tcti_defconfig` was removed.
- `ORLIX_PROFILES` remains `release development`.
- `tools/runtime/orlix-runtime-validation.sh` defaults back to `PROFILE=development`; `PROFILE=release` uses the same TCTI backend config.

Evidence:

- `bash -n tools/runtime/orlix-runtime-validation.sh` passed.
- `git diff --check` passed.
- `make -f OrlixKernel/Makefile __kernel-archive PROFILE=development ORLIX_KERNEL_ARCHIVE_PLATFORMS=iphonesimulator` passed with TCTI enabled.
- `make -f OrlixKernel/Makefile __kernel-archive PROFILE=release ORLIX_KERNEL_ARCHIVE_PLATFORMS=iphonesimulator` passed with TCTI enabled.

Still not complete:

- The physical iPhone `tcti-init-first-syscall` gate has not passed.
- The current TCTI backend still only fetches guest instructions, recognizes `svc #0`, reports unsupported instructions, and hands syscalls to the existing `arch/orlix` syscall path.

### Checkpoint: Fetch And SVC Exit Slice

- Added `tcti_fetch_instruction()` in `arch/orlix/mm/tcti_user_page.c`.
- Fetch path verifies Linux VMA execute permission and PTE execute permission, then reads the 32-bit instruction as host data.
- TCTI debug switch backend now fetches the current PC instruction, decodes `svc #0`, and returns `TCTI_EXIT_SYSCALL`.
- `orlix_tcti_enter_user()` now handles `TCTI_EXIT_SYSCALL` by setting `orig_x0`, `syscallno`, advancing PC by 4, and calling the existing `orlix_syscall_dispatch(regs)`.
- Added first TCTI decode KUnit tests for `svc #0` and non-SVC rejection.

### Evidence

- `git diff --check` passed.
- `bash -n tools/runtime/orlix-runtime-validation.sh` passed.
- `make -f OrlixKernel/Makefile __kernel-archive PROFILE=development ORLIX_KERNEL_ARCHIVE_PLATFORMS=iphonesimulator` passed after the fetch/syscall changes.
- Not yet runtime-ready.
- Not yet physical-device validated.
- No performance claim is made from this checkpoint.

### Checkpoint: Class-Based Instruction Expansion And Simulator Gate Blocker

- Corrected the TCTI bring-up approach away from instruction-word hardcoding.
- Added architectural decode and debug-switch execution for:
  - `ADR` / `ADRP` as `TCTI_DECODE_PC_RELATIVE_ADDRESS`.
  - `B` / `BL` as `TCTI_DECODE_UNCONDITIONAL_BRANCH_IMMEDIATE`.
- Kept observed `/init` words only as regression samples in TCTI tests:
  - `0x10000041` for `ADR x1, .+8`.
  - `0x94002283` for the first `BL` from `/init`.
- Replaced malformed untracked `tcti_decode_test.c` content with a clean class-based test file.
- Added simulator support to `tools/runtime/orlix-runtime-validation.sh` for `DESTINATION=iphonesimulator`:
  - uses the existing simulator selected by `ORLIX_SIMULATOR_ID` or auto-discovery.
  - builds the `iphonesimulator` kernel archive.
  - installs and launches through `simctl`.
  - keeps the same marker checks.
  - time-bounds `simctl bootstatus` with `ORLIX_SIMULATOR_BOOT_TIMEOUT_SECONDS`.

Evidence:

- Reviewed the actual development `/init` binary:
  - `/Volumes/1TB/Xcode/OrlixSystem/Build/OrlixOS/rootfs/development/base-tree/sbin/init`
  - `file`: `ELF 64-bit LSB pie executable, ARM aarch64, static-pie linked, stripped`.
  - first instructions: `mov x0, sp`, `nop`, `adr x1`, `bl`.
- `bash -n tools/runtime/orlix-runtime-validation.sh` passed.
- `git diff --check` passed.
- `make -f OrlixKernel/Makefile __kernel-archive PROFILE=development ORLIX_KERNEL_ARCHIVE_PLATFORMS=iphonesimulator` passed after the ADR/ADRP and branch-immediate changes.
- Attempted simulator-first runtime gate with:
  - `ORLIX_SIMULATOR_ID=4C88CA42-EA50-463F-B989-7B0560075A9B`
  - `make runtime-validation PROFILE=development DESTINATION=iphonesimulator GATE=tcti-init-first-syscall ORLIX_RUNTIME_GATE_CAPTURE_SECONDS=30`
- The simulator gate did not reach app launch. It blocked in `xcrun simctl bootstatus` before install/launch:
  - simulator: `Orlix-iPhone-15-Pro-Max (4C88CA42-EA50-463F-B989-7B0560075A9B)`
  - state: `Booted`
  - terminal state: `Waiting on Data Migration`
  - reason: `Running plugin com.apple.-0LaunchServicesMigrator`
  - elapsed observed: more than 11 minutes
  - warning: `CoreSimulator cache is not mounted at /Library/Developer/CoreSimulator/Caches`
- `xcode-storage-doctor` failed with one environment issue:
  - `CoreSimulator Caches is not mounted at /Library/Developer/CoreSimulator/Caches`.
- The local cache mount helper exists, but `sudo -n /Library/PrivilegedHelperTools/xcode-mount-coresimulator-caches` failed because the cache directory is not empty, and inspecting/moving the root-owned cache contents requires interactive sudo credentials.

Still not complete:

- Simulator runtime proof is blocked by CoreSimulator migration/cache state, not by a TCTI runtime result.
- Physical iPhone testing is intentionally deferred until the simulator gate can launch or the simulator environment is explicitly bypassed.
- TCTI still needs the next architectural classes from the `/init` trace, especially stack pair stores/loads, move-wide immediates, register moves/logical aliases, TPIDR_EL0 sysreg, conditional branches, compare aliases, scalar loads/stores, and syscall path execution.

### Checkpoint: First Stack And Register Setup Classes

- Continued from the actual development `/init` instruction stream instead of panic-word hardcoding.
- Added slow data memory helpers under `arch/orlix/mm`:
  - `tcti_read_user_data()`
  - `tcti_write_user_data()`
- These helpers resolve guest pages through the existing Linux-MM-owned `TCTI_ACCESS_READ` and `TCTI_ACCESS_WRITE` path and copy byte spans as host data.
- This is a correctness/debug-switch path, not the final fast TLB path and not a performance claim.
- Added architectural decode and debug-switch execution for:
  - `STP` / `LDP` GPR pair forms under `TCTI_DECODE_LOAD_STORE_PAIR`.
  - scalar unsigned-immediate `STR` / `LDR` under `TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE`.
  - `AND` / `ORR` / `EOR` shifted register under `TCTI_DECODE_LOGICAL_SHIFTED_REGISTER`.
  - `MOVN` / `MOVZ` / `MOVK` under `TCTI_DECODE_MOVE_WIDE_IMMEDIATE`.
- The immediate `/init` classes now covered include:
  - `mov x0, sp` through `ADD/SUB immediate`.
  - `nop` through `HINT`.
  - `adr x1, ...` through `PC-relative address`.
  - `bl ...` through unconditional branch immediate.
  - `stp x29, x30, [sp, #-0x20]!` through pair store pre-index.
  - `str x19, [sp, #0x10]` through scalar unsigned store.
  - `mov x19, x1` through logical shifted-register `ORR` alias.
  - `movz` / `movk` in the startup path through move-wide immediate.
- `tcti_switch_debug_execute_decoded()` now accepts the current `mm` and can report a data fault address for Linux user-fault handling.

Evidence:

- Re-inspected `/init` with `llvm-objdump` around:
  - entry `0x1a9a4`
  - first branch target `0x233bc`
- `bash -n tools/runtime/orlix-runtime-validation.sh` passed.
- `git diff --check` passed.
- `make -f OrlixKernel/Makefile __kernel-archive PROFILE=development ORLIX_KERNEL_ARCHIVE_PLATFORMS=iphonesimulator` passed after this stack/register setup slice.

Still not complete:

- Runtime simulator proof is still blocked by the previously recorded CoreSimulator migration/cache issue.
- The next `/init` classes still required include at least:
  - `MRS TPIDR_EL0`
  - conditional compare/branch forms such as `CBZ`, `TBZ`, and `B.cond`
  - register `ADD/SUB`
  - `BLR` and later `RET`
  - `CMP` aliases / flag-setting arithmetic
  - additional scalar load variants and sign-extending loads
- No physical-device retest was run in this checkpoint.

### Checkpoint: Generic Register-Offset Load/Store Slice And Simulator-First Block

- Re-read the active TCTI plan and kept this slice inside `arch/orlix`.
- Added architectural, class-based decode for scalar load/store register-offset forms under `TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET`.
- Fixed scalar load/store variant decoding so signed-load opcodes are not misclassified as stores.
- Added first-slice signed-load result metadata for unsigned-immediate and register-offset scalar load/store forms.
- Added debug-switch execution for register-offset scalar loads/stores through the existing Linux-MM-owned slow data helpers:
  - `tcti_read_user_data()`
  - `tcti_write_user_data()`
- Kept observed `/init` words only as KUnit regression samples. The implementation decodes masks and fields for the AArch64 class, not a hardcoded instruction candidate list.
- Used the public Arm register-offset addressing rule as a sanity check after the local `llvm-mc` tool was unavailable through Xcode.

Evidence:

- `bash -n tools/runtime/orlix-runtime-validation.sh` passed.
- `git diff --check` passed.
- `make -f OrlixKernel/Makefile kunit PROFILE=development` exited 0 and compiled `arch/orlix/hosted_exec/tcti/tests/tcti_decode_test.o`.
- `make -f OrlixKernel/Makefile __kernel-archive PROFILE=development ORLIX_KERNEL_ARCHIVE_PLATFORMS=iphonesimulator` passed and wrote the simulator archive.
- Attempted the simulator-first runtime gate on the single existing simulator:
  - simulator: `Orlix-iPhone-15-Pro-Max`
  - UDID: `4C88CA42-EA50-463F-B989-7B0560075A9B`
  - command: `make runtime-validation PROFILE=development DESTINATION=iphonesimulator GATE=tcti-init-first-syscall ORLIX_RUNTIME_GATE_CAPTURE_SECONDS=30 ORLIX_SIMULATOR_BOOT_TIMEOUT_SECONDS=900`
- The simulator gate blocked before install or app launch in `xcrun simctl bootstatus`.
- `xcrun simctl list devices available` reported the simulator as `Booted`, but CoreSimulator logs reported non-terminal `WaitingOnDataMigration`.
- CoreSimulator log evidence showed `DataMigrationPhaseDescription = "Running plugin com.apple.-0LaunchServicesMigrator (00LaunchServicesMigrator.migrator, user-agnostic)"`.
- `xcode-storage-doctor` still reported `CoreSimulator Caches is not mounted at /Library/Developer/CoreSimulator/Caches`.
- Stopped the blocked simulator gate after confirming no Orlix app launch or TCTI runtime evidence could be produced from that run.

Still not complete:

- Simulator runtime proof has not reached Orlix app launch.
- No physical-device retest was run because the requested simulator-first gate is blocked before the app starts.
- TCTI is still not runtime-ready.
- No performance claim is made from this checkpoint.

### Checkpoint: Control Flow, NZCV, And System Register Bring-Up

- Attempted the user-requested simulator-first path before physical-device work.
- The previously documented known-good `ExternalSSDProof` simulator UDID was no longer available to `simctl`.
- The only available simulator was `Orlix-iPhone-15-Pro-Max (4C88CA42-EA50-463F-B989-7B0560075A9B)`.
- `xcode-storage-doctor` still reported one environment issue:
  - `CoreSimulator Caches is not mounted at /Library/Developer/CoreSimulator/Caches`.
- Booted that single existing simulator and waited with:
  - `timeout 900 xcrun simctl bootstatus 4C88CA42-EA50-463F-B989-7B0560075A9B -b`
- The simulator did not reach terminal bootstatus before the 15-minute bound. It stayed in Data Migration under `com.apple.-0LaunchServicesMigrator`, so no Orlix app launch or simulator TCTI runtime evidence was collected.
- Continued implementation only in `arch/orlix` TCTI-owned code.
- Added architectural, class-based decode and debug-switch execution for:
  - `ADD/SUB` shifted-register.
  - `ADDS/SUBS` immediate and `CMP` aliases through NZCV updates in `pt_regs.pstate`.
  - `BR` / `BLR` / `RET`.
  - `CBZ` / `CBNZ`.
  - `TBZ` / `TBNZ`.
  - `B.cond`.
  - `MRS` / `MSR` for `TPIDR_EL0` and `NZCV`.
- Kept observed `/init` instruction words only as KUnit regression samples. Decode/execute logic uses AArch64 class masks and fields, not device-specific instruction hardcoding.
- Mapped guest `TPIDR_EL0` to `current->thread.user_tls` only in hosted product builds. The non-hosted KUnit compile does not fake this field.
- Added NZCV execution coverage to KUnit, and guarded hosted TLS execution coverage behind `ORLIX_APP_HOSTED_BOOT`.
- Wired the existing Orlix KUnit config and target to compile the TCTI decode test object:
  - `CONFIG_ORLIX_TCTI_KUNIT_TEST=y`
  - `arch/orlix/hosted_exec/tcti/tests/tcti_decode_test.o`

Evidence:

- `bash -n tools/runtime/orlix-runtime-validation.sh` passed.
- `git diff --check` passed.
- `make -f OrlixKernel/Makefile __kernel-archive PROFILE=development ORLIX_KERNEL_ARCHIVE_PLATFORMS=iphonesimulator` passed after the new TCTI classes.
- `make -f OrlixKernel/Makefile kunit PROFILE=development` passed after wiring the TCTI KUnit object. This is a KUnit object compile gate, not a full runtime KUnit execution report.

Still not complete:

- Simulator runtime proof is blocked before app launch by CoreSimulator migration/cache state.
- No physical-device retest was run, per simulator-first instruction.
- TCTI still needs load/store register-offset and sign-extending load coverage from the next `/init` trace step, plus the real `svc #0` runtime gate.

### Ordering Correction: Register-Offset Slice Is Now Implemented

- The preceding checkpoint was written before the generic register-offset slice.
- Current state after the latest implementation:
  - scalar load/store register-offset decode exists under `TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET`.
  - debug-switch execution exists for register-offset scalar loads/stores through the slow Linux-MM data helpers.
  - scalar signed-load variant metadata exists for unsigned-immediate and register-offset forms.
  - KUnit regression coverage includes the observed `/init` register-offset load sample as a class example, not as implementation hardcoding.
- The remaining runtime gap is not this decode class. The remaining proof gap is that the simulator gate still does not reach Orlix app launch because CoreSimulator is stuck in non-terminal data migration.

### Checkpoint: Scalar Imm9 Memory Decode And Simulator Retry

- Re-read the active TCTI plan and current `/init` disassembly.
- Identified the next generic memory class from the trace as scalar imm9 load/store forms:
  - post-index byte store, for example `strb w10, [x8], #0x1`.
  - unscaled byte loads, for example `ldurb w9, [x8, #-0x2]`.
  - `LDUR` / `STUR` 64-bit GPR forms used later in `/init`.
- Added architectural decode class `TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE`.
- Implemented generic scalar imm9 decode for:
  - signed unscaled offset.
  - pre-index writeback.
  - post-index writeback.
  - unsigned and sign-extending scalar load variants through the existing load/store variant decoder.
- Reused the existing scalar load/store executor for unsigned immediate and signed imm9 forms so writeback uses the same `tcti_indexed_address()` / `tcti_apply_memory_writeback()` path.
- Added focused KUnit decode coverage using observed `/init` samples only as class regression examples, not implementation hardcoding:
  - `0x3800150a` for post-index `strb`.
  - `0x385fe109` for unscaled `ldurb`.
  - `0xf81d03bf` for `stur xzr`.
  - `0xf85d03a9` for `ldur x9`.

Evidence:

- `bash -n tools/runtime/orlix-runtime-validation.sh` passed.
- `git diff --check` passed.
- `make -f OrlixKernel/Makefile kunit PROFILE=development` exited 0 and compiled `arch/orlix/hosted_exec/tcti/tests/tcti_decode_test.o`.
- `make -f OrlixKernel/Makefile __kernel-archive PROFILE=development ORLIX_KERNEL_ARCHIVE_PLATFORMS=iphonesimulator` passed and wrote:
  - `Build/OrlixKernel/development/iphonesimulator/OrlixKernel.a`
  - `Build/OrlixKernel/development/linux-object-manifest.txt`
- Retried simulator-first runtime gate on the single existing simulator:
  - simulator: `Orlix-iPhone-15-Pro-Max`
  - UDID: `4C88CA42-EA50-463F-B989-7B0560075A9B`
  - command: `make runtime-validation PROFILE=development DESTINATION=iphonesimulator GATE=tcti-init-first-syscall ORLIX_RUNTIME_GATE_CAPTURE_SECONDS=30 ORLIX_SIMULATOR_BOOT_TIMEOUT_SECONDS=900`
- The simulator gate again blocked before install or app launch in `xcrun simctl bootstatus`.
- Current CoreSimulator log evidence showed non-terminal `WaitingOnDataMigration` with `DataMigrationPhaseDescription = "Running plugin com.apple.-0LaunchServicesMigrator (00LaunchServicesMigrator.migrator, user-agnostic)"`.
- `xcode-storage-doctor` still reported `CoreSimulator Caches is not mounted at /Library/Developer/CoreSimulator/Caches`.
- Stopped the blocked simulator gate after confirming it had not reached Orlix app launch.

Still not complete:

- Simulator runtime proof has not reached Orlix app launch.
- Physical-device proof was not run because the requested simulator-first gate is still blocked before app launch.
- TCTI is not runtime-ready.
- No performance claim is made from this checkpoint.

## 2026-07-01

### Checkpoint: Scalar Bitfield Slice And Simulator-First Gate Attempt

- Continued the active Orlix TCTI plan inside `arch/orlix`; no HostAdapter or app-owned Linux semantics were added.
- Added architectural, class-based decode for the AArch64 bitfield instruction family:
  - `SBFM`
  - `BFM`
  - `UBFM`
- Added debug-switch execution for bitfield moves using decoded `sf`, `opc`, `N`, `immr`, `imms`, `rn`, and `rd` fields.
- Covered observed `/init` aliases as regression examples only:
  - `ubfx w23, w22, #8, #8`
  - `lsr x9, x24, #16`
  - `sbfx w0, w8, #0, #1`
- Added assembler-confirmed `BFM` coverage with `bfi w1, w2, #8, #8`.
- Updated `tools/orlix-a64-opprofile/orlix-a64-opprofile` so the `/init` profile marks scalar bitfield as supported only when the encoding satisfies the same validity checks as the decoder.
- Refreshed `docs/plans/active/orlix-tcti/tcti-opprofile-init.md`.

Evidence:

- `git diff --check` passed.
- `python3 -m py_compile tools/orlix-a64-opprofile/orlix-a64-opprofile` passed.
- `bash -n tools/runtime/orlix-runtime-validation.sh` passed.
- `make -f OrlixKernel/Makefile kunit PROFILE=development` passed.
- `make -f OrlixKernel/Makefile __kernel-archive PROFILE=development ORLIX_KERNEL_ARCHIVE_PLATFORMS=iphonesimulator` passed and wrote:
  - `Build/OrlixKernel/development/iphonesimulator/OrlixKernel.a`
  - `Build/OrlixKernel/development/linux-object-manifest.txt`
- Refreshed `/init` opcode profile now reports:
  - `bitfield`: 359, supported.
  - remaining unsupported classes include `unsupported-unknown`, `simd`, `multiply-add-sub`, `conditional-compare`, and `unsupported-load-store-pair`.

Simulator-first gate:

- Ran `make runtime-validation PROFILE=development DESTINATION=iphonesimulator GATE=tcti-init-first-syscall` with:
  - `ORLIX_SIMULATOR_ID=4C88CA42-EA50-463F-B989-7B0560075A9B`
  - `ORLIX_RUNTIME_GATE_CAPTURE_SECONDS=30`
  - `ORLIX_SIMULATOR_BOOT_TIMEOUT_SECONDS=180`
- Report: `Build/Reports/runtime/tcti-init-first-syscall-20260701T011418Z.md`.
- Result: failed before app install or launch. The simulator did not finish booting within 180 seconds.
- Bootstatus artifact remained non-terminal in Data Migration, ending on `com.apple.-0LaunchServicesMigrator`.
- `xcode-storage-doctor` still reports one environment issue:
  - `CoreSimulator Caches is not mounted at /Library/Developer/CoreSimulator/Caches`.
- This is not TCTI runtime evidence because Orlix did not install or launch on the simulator.

Physical-device gate:

- Verified local `devicectl` help for:
  - `xcrun devicectl --help`
  - `xcrun devicectl device --help`
  - `xcrun devicectl device process --help`
- Connected available phone:
  - `RRJ-iPhone-15-Pro-Max`
  - `7F8A1701-D612-5A9C-AAE7-8FD0AD77306C`
  - `iPhone16,2`
- First physical gate without `ORLIX_DEVELOPMENT_TEAM` failed at Xcode signing:
  - report `Build/Reports/runtime/tcti-init-first-syscall-20260701T011841Z.md`
  - result `Signing for "Orlix" requires a development team`.
- Retried with `ORLIX_DEVELOPMENT_TEAM=ZQ3L7M567L`.
- Report: `Build/Reports/runtime/tcti-init-first-syscall-20260701T012114Z.md`.
- Result: app built and the `iphoneos` kernel archive was reused, but launch failed because the device was locked:
  - `Unable to launch com.rudironsoni.Orlix because device was not, or could not be, unlocked`.
- No TCTI `svc #0` marker was captured.
- `host-exec-violations.txt` was empty for this run, but the app never launched, so this is not proof that guest ELF text stayed host non-executable during `/init` execution.

Still not complete:

- Simulator runtime proof remains blocked before app launch by CoreSimulator migration/cache state.
- Physical runtime proof remains blocked before app launch by the locked iPhone.
- The first TCTI physical acceptance target has not passed:
  - no first interpreted PC captured,
  - no first real `svc #0` captured,
  - no `/init` console line captured,
  - no app-runtime proof that guest ELF text stayed host non-executable during `/init`.

### Checkpoint: Simulator-First Gate Recovered To Install Boundary, Then Blocked By CoreSimulator Migration

- Re-read `AGENTS.md`, both active plan files, and both active implementation logs before mutating the runtime harness.
- Kept the change in `tools/runtime/orlix-runtime-validation.sh`; no TCTI/Linux semantics were moved into HostAdapter or the app.
- Changed simulator runtime validation so blocking CoreSimulator commands cannot hang the harness indefinitely:
  - `simctl bootstatus` timeout now terminates the whole process group.
  - if `simctl` already reports the selected simulator as `Booted`, the harness records `bootstatus-skipped-booted.txt` and proceeds to the install/launch probe instead of making non-terminal bootstatus the proof surface.
  - best-effort `simctl terminate` and `simctl uninstall` are now bounded.
  - simulator `simctl install` is bounded and still fails the gate if it cannot complete.
- Retried the simulator-first gate on the single available simulator:
  - simulator: `Orlix-iPhone-15-Pro-Max`.
  - UDID: `4C88CA42-EA50-463F-B989-7B0560075A9B`.
  - command: `ORLIX_SIMULATOR_ID=4C88CA42-EA50-463F-B989-7B0560075A9B ORLIX_RUNTIME_GATE_CAPTURE_SECONDS=30 ORLIX_SIMULATOR_BOOT_TIMEOUT_SECONDS=60 make runtime-validation PROFILE=development DESTINATION=iphonesimulator GATE=tcti-init-first-syscall`.
  - report: `Build/Reports/runtime/tcti-init-first-syscall-20260701T020106Z.md`.
  - result: failed before app launch because `simctl install` timed out.
  - CoreSimulator log showed the install request for `Orlix.app`, then CoreSimulator shut down the simulator from an unexpected `Booted` state.
- Performed same-simulator recovery without creating another simulator:
  - `xcrun simctl erase 4C88CA42-EA50-463F-B989-7B0560075A9B` exited 0.
  - `xcrun simctl boot 4C88CA42-EA50-463F-B989-7B0560075A9B` exited 0.
  - bounded `xcrun simctl bootstatus ... -b` stayed non-terminal in Data Migration for more than 10 minutes.
  - final observed migration reason remained `Running plugin com.apple.-0LaunchServicesMigrator (00LaunchServicesMigrator.migrator, user-agnostic)`.
- Environment evidence remains:
  - `xcode-storage-doctor` reports `CoreSimulator Caches is not mounted at /Library/Developer/CoreSimulator/Caches`.
  - the privileged cache helper can run, but refuses because `/Library/Developer/CoreSimulator/Caches` is not empty.
  - arbitrary passwordless `sudo` is not available to inspect or clear that root-owned cache mountpoint.

Evidence:

- `bash -n tools/runtime/orlix-runtime-validation.sh` passed.
- `git diff --check` passed.
- `xcrun simctl list devices available` showed only the single iOS 26.5 simulator above.
- Same-simulator erase and boot completed, but terminal bootstatus still did not complete within the 10-minute bound.

Boundary:

- This is not TCTI runtime evidence because Orlix did not install or launch on the simulator.
- The simulator-first requirement was attempted through the full runtime gate and then through same-simulator recovery.
- Physical-device validation should be treated as the next runtime proof surface only after accepting that the current simulator is blocked by CoreSimulator migration/cache state, not by Orlix TCTI.

### Checkpoint: Physical Device Gate Built And Installed, Launch Blocked By Locked iPhone

- Proceeded to the physical proof surface after simulator-first validation was blocked before app launch by CoreSimulator migration/cache state.
- Target physical device:
  - `RRJ-iPhone-15-Pro-Max`.
  - device id `7F8A1701-D612-5A9C-AAE7-8FD0AD77306C`.
  - product type `iPhone16,2`.
  - iOS `27.0`, build `24A5370h`.
  - Developer Mode enabled, `ddiServicesAvailable: true`.
- Ran:
  - `ORLIX_DEVICE_ID=7F8A1701-D612-5A9C-AAE7-8FD0AD77306C ORLIX_DEVELOPMENT_TEAM=ZQ3L7M567L ORLIX_RUNTIME_GATE_CAPTURE_SECONDS=45 make runtime-validation PROFILE=development DESTINATION=iphoneos GATE=tcti-init-first-syscall`.
- Report:
  - `Build/Reports/runtime/tcti-init-first-syscall-20260701T021642Z.md`.
- Result:
  - iPhoneOS development kernel archive built successfully.
  - Xcode Debug iPhoneOS app build succeeded.
  - `devicectl device install app` installed `com.rudironsoni.Orlix`.
  - `devicectl device process launch --console` failed because the device was locked.
  - Launch artifact says: `Unable to launch com.rudironsoni.Orlix because the device was not, or could not be, unlocked`.
  - No `Orlix TCTI: svc #0` marker was captured.
  - `host-exec-violations.txt` was empty, but this is not proof because the app did not launch.

Evidence:

- `bash -n tools/runtime/orlix-runtime-validation.sh` passed after the harness changes.
- `git diff --check` passed after the harness changes.
- Physical install artifact confirms the app installed on the iPhone.
- Physical launch artifact confirms the blocker is the locked device state, not a build or install failure.

Boundary:

- Physical runtime proof is still missing.
- Next step after unlocking the phone is to rerun the same physical gate and inspect `launch-console.log`, `launch.log`, `host-exec-violations.txt`, and `tcti-first-syscall.txt`.

### Checkpoint: Reference Hierarchy Re-read And Implementation Direction Corrected

- Re-read the primary TCTI reference before continuing implementation:
  - repository `https://github.com/rudironsoni/ish`
  - branch `feat/aarch64-migration`
  - commit `55d14a9fefe47a7ed9b3bb44e4ccca7429bd9363`
  - required files read: `README.md`, `project.yml`, `docs/plans/a64-tcti-proof-program.md`, `Sources/IXLandLinuxRuntime/emu/aarch64/cpu.c`, `cpu.h`, `block-cache.h`, `block-cache.c`, `fetch.h`, `fetch.c`, `memory.c`, `sysreg.c`, `emu/mmu.h`, `emu/tlb.h`, `emu/tlb.c`, `tcti/frame.h`, `tcti/aarch64/gen.h`, `gen.c`, `tcti-gadget-gen.py`, `kernel/memory.c`, and `kernel/page_map.c`.
- Secondary references were read only for comparison:
  - upstream iSH commit `997642f3787cc63e65f7134b7bb0362c74bff8e0`.
  - OpenMinis iSH ARM64 commit `a5e0a1e358e42a539ff916b4098628fd7f55e3fa`.
  - ios-linuxkit commit `312f1093bd008918036d845d0725a345f3bc342e`.
- Updated `docs/investigations/orlix-tcti-reference-review.md` with concrete carry and do-not-carry notes.

Corrections:

- Do not continue broad opcode work as the main implementation path.
- Do not make the debug switch oracle the beta backend.
- Prioritize the actual Orlix TCTI pipeline:
  - fetch through Orlix/Linux MM with explicit `FETCH`, `READ`, and `WRITE` access classes.
  - decode to Orlix gadget words.
  - execute data-only gadget streams through precompiled Orlix gadget functions.
  - exit on `svc #0`, fault, unsupported instruction, signal point, yield, or task exit.
- Keep the reference branch's useful lessons:
  - generated gadget table from a source-of-truth generator.
  - `tcti_entry_block(gadgets, cpu_state)` style entry shape.
  - generation-stamped TLB.
  - separate translation generation and code generation.
  - block cache keyed by guest PC plus code generation.
  - fetch/decode/lowering/dispatch/exit instrumentation vocabulary.
  - contract/system/perf/UI test separation.
- Do not carry:
  - IXLand runtime ownership model.
  - iSH fakefs, syscall emulator, process model, native offload, bind mounts, DebugServer app APIs, or V8 binary patching.
  - x0-x12 hot mapping or memory-backed x13-x30 as a final unmeasured design.
  - hardcoded PC-range diagnostics.
  - READ/FETCH permission conflation.
  - O(n) whole-cache invalidation as the serious block-chaining design.

Local cleanup:

- Removed the interrupted EXTR/ROR decode and switch-oracle change from this checkpoint. It was class-based, but it advanced the wrong layer before the TCTI lowering/gadget path was reconciled with the primary reference.
- Existing scalar support such as `SMULH`/`UMULH` remains part of the current scaffold, but future opcode expansion must be paired with the real TCTI lowering/gadget path and focused tests, not treated as product progress by switch execution alone.

Boundary:

- This checkpoint is not runtime proof.
- Simulator-first validation is still blocked before app install by CoreSimulator cache/migration state.
- Physical device validation has not been rerun after the reference correction.

### Checkpoint: Publish Preparation

- Staged the current TCTI checkpoint for commit.
- Removed the generated Python cache artifact from `tools/orlix-a64-opprofile/__pycache__` before commit scope review.
- Kept the staged scope to source, docs, active plan files, and validation tooling.

Evidence:

- `git diff --cached --check` passed.
- `bash -n tools/runtime/orlix-runtime-validation.sh` passed in a non-login shell after a transient Homebrew shell-startup fork error.
- `python3 -m py_compile tools/orlix-a64-opprofile/orlix-a64-opprofile` passed in a non-login shell after the same transient shell-startup fork error.

Boundary:

- No long simulator gate was rerun for publish preparation.
- No physical-device runtime gate was rerun for publish preparation.
- TCTI remains not runtime-ready.

### Checkpoint: Swift TCTI Rails And Product Defconfig Safety

- Restored product defconfigs to native hosted execution by default:
  - `OrlixKernel/Sources/ports/orlix/configs/development_defconfig`
  - `OrlixKernel/Sources/ports/orlix/configs/release_defconfig`
  - both now keep `CONFIG_ORLIX_HOSTED_EXEC_NATIVE=y`
  - both keep `CONFIG_ORLIX_HOSTED_EXEC_TCTI` unset
  - both no longer enable `CONFIG_ORLIX_TCTI_DEBUG_SWITCH=y`
- Added top-level Make targets routed through a Swift rail driver:
  - `make tcti-plan-consistency`
  - `make tcti-report-schema-check`
  - `make tcti-toolchain-check`
  - `make tcti-contract`
  - `make tcti-golden-elf`
  - `make tcti-golden-elf-refresh`
  - `make tcti-diff-switch`
  - `make tcti-memory-fuzz`
  - `make tcti-direct-chain-fuzz`
  - `make tcti-appstore-safety-audit`
  - `make tcti-repro`
- Added `tools/tcti/orlix-tcti-gate.swift`.
  - Uses Swift/Foundation for the repo rails.
  - Does not add Python tooling for this checkpoint.
  - Writes JSON reports atomically under `Build/TCTI/reports/<target>/report.json`.
  - Writes Markdown sidecars under `Build/TCTI/reports/<target>/report.md`.
  - Writes reducer artifacts under `Build/TCTI/reproducers/<target>/<case-id>.json`.
  - Defines report statuses: `pass`, `fail`, `todo`, `skipped`, `error`, and `evidence`.
  - Treats `todo` as `passed=false` and exits non-zero.
  - Treats `evidence` as `passed=false`, not release/readiness eligible.
- Added report-schema fixtures:
  - `tools/tcti/fixtures/report.pass.json`
  - `tools/tcti/fixtures/report.fail.missing-field.json`
  - `tools/tcti/fixtures/report.fail.invalid-status.json`
- Added x18 audit fixture:
  - `tools/tcti/fixtures/x18_forbidden/bad.S`
- Added seed golden ELF source and metadata:
  - `OrlixKernel/Tests/TCTI/golden_elf/init_001_exit/init_001_exit.S`
  - `OrlixKernel/Tests/TCTI/golden_elf/init_001_exit/golden.json`
  - source SHA256 `b03c642881ff3fe1f8397b3191cdf76a3e41467f3dcb2c81cac0f4f995c6b8ac`
  - expected binary SHA256 `93d1fe89ade104cd4c674c4a870211d94c5354d37575dfde61344de4434ff4a4`
- Hardened `tools/runtime/orlix-runtime-validation.sh` physical TCTI preflight:
  - physical TCTI gates refuse to run unless autonomous reports pass
  - emergency evidence collection requires `ORLIX_TCTI_DEVICE_OVERRIDE=I_ACCEPT_DEVICE_DEBUG_DEBT`
  - emergency evidence collection requires non-empty `ORLIX_TCTI_DEVICE_OVERRIDE_REASON`
  - override reports use `status=evidence`, `passed=false`, and are not release/readiness eligible
  - report filenames include the process id to avoid same-second JSON temp-file collisions
  - `ORLIX_RUNTIME_PREFLIGHT_ONLY=1` can verify blocking/override behavior without touching a device
- Updated ADR 0022 and `PLAN.md` to reflect:
  - product defconfig safety is a release blocker
  - Swift rail driver, not Python, owns these rails
  - report status semantics are explicit
  - schema check covers TCTI reports and top-level runtime JSON sidecars
  - golden ELF reproducibility records toolchain and hashes
  - milestone-1 single-runner mode is not a pthread/package-manager correctness claim

Evidence:

```sh
rtk proxy make tcti-plan-consistency
rtk proxy make tcti-report-schema-check
rtk proxy make tcti-toolchain-check
rtk proxy make tcti-golden-elf
rtk proxy make tcti-appstore-safety-audit
```

All five passed and wrote reports under `Build/TCTI/reports/`.

```sh
rtk proxy sh -c 'make tcti-contract; rc=$?; echo rc=$rc; exit 0'
```

`tcti-contract` emitted `status=todo`, wrote `Build/TCTI/reports/tcti-contract/report.json`, wrote `Build/TCTI/reproducers/tcti-contract/todo.json`, and exited non-zero through Make (`rc=2`).

```sh
rtk proxy sh -c 'make tcti-repro REPRO=Build/TCTI/reproducers/tcti-contract/todo.json; rc=$?; echo rc=$rc; exit 0'
```

The reducer replayed `make tcti-contract` and exited non-zero through Make (`rc=2`).

```sh
rtk proxy sh -c 'ORLIX_RUNTIME_PREFLIGHT_ONLY=1 DESTINATION=iphoneos GATE=tcti-init-first-syscall tools/runtime/orlix-runtime-validation.sh; rc=$?; echo rc=$rc; exit 0'
```

Physical TCTI preflight without autonomous passing reports failed before device work (`rc=1`).

```sh
rtk proxy sh -c 'ORLIX_RUNTIME_PREFLIGHT_ONLY=1 DESTINATION=iphoneos GATE=tcti-init-first-syscall ORLIX_TCTI_DEVICE_OVERRIDE=I_ACCEPT_DEVICE_DEBUG_DEBT tools/runtime/orlix-runtime-validation.sh; rc=$?; echo rc=$rc; exit 0'
```

Override without a reason failed before device work (`rc=1`).

```sh
rtk proxy sh -c 'ORLIX_RUNTIME_PREFLIGHT_ONLY=1 DESTINATION=iphoneos GATE=tcti-init-first-syscall ORLIX_TCTI_DEVICE_OVERRIDE=I_ACCEPT_DEVICE_DEBUG_DEBT ORLIX_TCTI_DEVICE_OVERRIDE_REASON="rails checkpoint evidence test" tools/runtime/orlix-runtime-validation.sh; rc=$?; echo rc=$rc; exit 0'
```

Override with a reason wrote runtime JSON with `status=evidence`, `passed=false`, `autonomous_tests_bypassed=true`, `release_gate_eligible=false`, and `readiness_gate_eligible=false`, then exited non-zero (`rc=1`).

Boundary:

- No TCTI runtime or assembly gadget work was added in this checkpoint.
- No physical device gate was run.
- `tcti-contract`, `tcti-diff-switch`, `tcti-memory-fuzz`, and `tcti-direct-chain-fuzz` are real failing rails, not implemented tests.
- This checkpoint proves rails, reporting, defconfig safety, golden seed reproducibility, and preflight behavior. It does not prove TCTI runtime readiness.

### Checkpoint: Seed Golden ELF Contract Validation

- Turned `make tcti-contract` from a pure TODO rail into a partial real no-phone contract rail.
- `tcti-contract` still exits non-zero with `status=todo` because deeper CPU execution contract groups are intentionally not implemented yet.
- Real contract groups now passing:
  - report schema status representation for `pass`, `fail`, `todo`, `skipped`, `error`, and `evidence`
  - reducer replay fixture
  - product defconfig safety
  - forbidden host `x18/w18` negative fixture
  - `init_001_exit` golden metadata, ELF header, entrypoint, and syscall shape
  - wrong-binary-SHA golden metadata negative fixture
- Contract groups still TODO:
  - guest instruction execution semantics
  - gadget ABI and register commit-back execution
  - `FETCH`/`READ`/`WRITE` memory execution
  - TLB, block-cache, invalidation, and direct-chain execution
- Strengthened `make tcti-golden-elf` for `init_001_exit`.
  - Builds the seed no-libc AArch64 Linux ELF.
  - Verifies source SHA256.
  - Verifies binary SHA256 against `golden.json`.
  - Verifies ELF64 AArch64 executable shape through `file` and `llvm-objdump`.
  - Verifies entrypoint matches `golden.json`.
  - Verifies syscall instruction shape by disassembly:
    - `mov x8, #93`
    - `mov x0, #42`
    - `svc #0`
- Added negative golden metadata fixture:
  - `tools/tcti/fixtures/golden_elf/init_001_exit_wrong_binary_sha.json`
- Strengthened `make tcti-repro REPRO=<path>` output.
  - Prints target.
  - Prints case id.
  - Prints original command.
  - Prints artifact paths.
  - Prints expected status.
  - Prints actual replay status.
  - Prints actual replay exit code.

Report and reducer paths:

- `Build/TCTI/reports/tcti-contract/report.json`
- `Build/TCTI/reports/tcti-golden-elf/report.json`
- `Build/TCTI/reproducers/tcti-contract/todo.json`
- `Build/TCTI/reproducers/tcti-contract-repro-pass-fixture/repro-pass.json`

Evidence:

```sh
rtk proxy git diff --check
rtk proxy make tcti-plan-consistency
rtk proxy make tcti-report-schema-check
rtk proxy make tcti-toolchain-check
rtk proxy make tcti-golden-elf
rtk proxy make tcti-appstore-safety-audit
```

All passed.

```sh
rtk proxy sh -c 'make tcti-contract; rc=$?; echo rc=$rc; exit 0'
```

Result:

- `tcti-contract` wrote `status=todo`.
- real contract groups were listed as passing.
- deeper CPU-state groups were listed as TODO.
- Make exited non-zero with `rc=2`.

```sh
rtk proxy sh -c 'make tcti-repro REPRO=Build/TCTI/reproducers/tcti-contract/todo.json; rc=$?; echo rc=$rc; exit 0'
```

Result:

- reducer replayed `make tcti-contract`.
- expected status was `todo`.
- actual replay status was `todo`.
- actual replay exit code was `2`.
- Make exited non-zero with `rc=2`.

```sh
rtk proxy sh -c 'if rg -n "CONFIG_ORLIX_HOSTED_EXEC_TCTI=y|CONFIG_ORLIX_TCTI_DEBUG_SWITCH=y" OrlixKernel/Sources/ports/orlix/configs/development_defconfig OrlixKernel/Sources/ports/orlix/configs/release_defconfig; then exit 1; else echo product-defconfigs-no-tcti-defaults; fi'
```

Result: no product defconfig TCTI/default debug-switch matches.

```sh
rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" make -f OrlixKernel/Makefile kunit PROFILE=development
rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" make -f OrlixKernel/Makefile kunit PROFILE=release
```

Both passed. The existing Xcode SDK `__alloc_size` redefinition warning remains present.

Boundary:

- No production TCTI assembly was added.
- No gadget dispatch was implemented.
- No switch-debug instruction execution was implemented.
- No simulator gate was run.
- No physical-device gate was run.
- This is structural and contractual no-phone proof for the seed golden ELF. It is not guest execution proof and not runtime readiness.

### Checkpoint: Seed ELF Switch-Debug Execution

- Added `make tcti-golden-elf CASE=init_001_exit EXECUTE=switch-debug`.
- The execution mode validates the existing seed golden metadata before execution:
  - source SHA256
  - binary SHA256
  - ELF64 AArch64 executable shape
  - entrypoint
  - syscall instruction shape
- The switch-debug harness reads the ELF entrypoint from the generated ELF and fetches 32-bit instruction words from PT_LOAD file-backed bytes.
- Exact instruction encodings observed and handled:
  - `0xd2800ba8`: `mov x8, #93`
  - `0xd2800540`: `mov x0, #42`
  - `0xd4000001`: `svc #0`
- Switch-debug semantic handlers added:
  - exact `0xd2800ba8` sets guest `x8 = 93`
  - exact `0xd2800540` sets guest `x0 = 42`
  - exact `0xd4000001` captures a test-harness syscall event
- Captured execution result:
  - backend `switch-debug`
  - case id `init_001_exit`
  - entered entrypoint `true`
  - guest instructions executed `3`
  - syscall `exit`, number `93`, args `[42]`, captured `true`
  - exit kind `guest_exit_syscall`, code `42`
- Added negative execution fixtures:
  - `tools/tcti/fixtures/golden_elf/init_001_exit_wrong_expected_exit.json`
  - `tools/tcti/fixtures/golden_elf/init_001_exit_unsupported_before_svc.S`
  - `tools/tcti/fixtures/golden_elf/init_001_exit_wrong_syscall.S`
- Negative execution reducer paths:
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-wrong-exit.json`
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-unsupported.json`
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-wrong-syscall.json`
- `tcti-contract` now has one more real passing group:
  - switch-debug executes `init_001_exit` to captured `exit(42)`
- `tcti-contract` still exits non-zero with `status=todo` because deeper groups remain TODO:
  - gadget ABI and register commit-back execution
  - `FETCH`/`READ`/`WRITE` memory execution
  - TLB, block-cache, invalidation, and direct-chain execution

Report paths:

- `Build/TCTI/reports/tcti-golden-elf/report.json`
- `Build/TCTI/golden_elf/init_001_exit/execution.json`
- `Build/TCTI/reports/tcti-contract/report.json`

Evidence:

```sh
rtk proxy git diff --check
rtk proxy make tcti-plan-consistency
rtk proxy make tcti-report-schema-check
rtk proxy make tcti-toolchain-check
rtk proxy make tcti-golden-elf
rtk proxy make tcti-appstore-safety-audit
rtk proxy make tcti-golden-elf CASE=init_001_exit EXECUTE=switch-debug
```

All passed.

```sh
rtk proxy sh -c 'make tcti-contract; rc=$?; echo rc=$rc; exit 0'
```

Result:

- real contract groups include switch-debug execution to captured `exit(42)`
- deeper groups remain TODO
- Make exited non-zero with `rc=2`

```sh
rtk proxy sh -c 'make tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-wrong-syscall.json; rc=$?; echo rc=$rc; exit 0'
```

Result:

- expected status `fail`
- actual replay status `fail`
- actual replay exit code `2`
- Make exited non-zero with `rc=2`

```sh
rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" make -f OrlixKernel/Makefile kunit PROFILE=development
rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" make -f OrlixKernel/Makefile kunit PROFILE=release
```

Both passed. The existing Xcode SDK `__alloc_size` redefinition warning remains present.

Boundary:

- No production TCTI assembly was added.
- No gadget dispatch was implemented.
- No simulator gate was run.
- No physical-device gate was run.

### Checkpoint: Autonomous Next-Task Loop

- Added agent-neutral Make targets:
  - `make agent-status AREA=orlix-tcti`;
  - `make agent-next AREA=orlix-tcti`;
  - `make agent-task-envelope-check AREA=orlix-tcti`.
- Added skill-owned roadmap data:
  - `.agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json`.
- Added skill-local scripts:
  - `.agents/skills/orlix-tcti-next-step/scripts/status`;
  - `.agents/skills/orlix-tcti-next-step/scripts/next`;
  - `.agents/skills/orlix-tcti-next-step/scripts/task-envelope-check`;
  - shared runner `.agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift`.
- `agent-status` writes `Build/AgentHarness/orlix-tcti/status.json`.
- `agent-next` writes:
  - `Build/AgentHarness/orlix-tcti/next-task.json`;
  - `Build/AgentHarness/orlix-tcti/next-task.md`.
- `agent-task-envelope-check` validates that selected gates exist in the roadmap, prerequisites pass, forbidden scope and validation commands are present, physical/device and gadget prerequisites are enforced, custom MCP references are absent, and next-task JSON is machine-parseable.
- The current repo state selects `switch-init-003-stack` because `init_001_exit` and `init_002_write` structural and switch-debug artifacts are already present and pass, while `init_003_stack` artifacts are missing.
- Updated TCTI skills, subagents, AGENTS guidance, and harness docs so the standard workflow is status, next, planner review, safety review, allowed-scope implementation, reducer handling, and release-gate review.

Boundary:

- No TCTI runtime feature was implemented.
- No production TCTI assembly was added.
- No gadget dispatch was implemented.
- No simulator gate was run.
- No physical-device gate was run.
- No custom MCP or `tools/agent` path was added.
- No HostAdapter, UIKit, or Darwin syscall path was used.
- No Linux runtime semantics were implemented.
- No syscall implementation was added beyond a captured test-harness syscall event.
- This is only the seed ELF switch-debug no-phone execution proof.

### Checkpoint: Decoded Seed ELF Switch Semantics

- Replaced exact full-word switch execution with a tiny decoded AArch64 semantic layer for the seed ELF.
- `make tcti-golden-elf CASE=init_001_exit EXECUTE=switch-debug` still captures guest `exit(42)`.
- Execution now decodes instructions into `decoded_instructions` before switch-debug semantics run.
- MOVZ decoder mask and fields:
  - class predicate: `(raw & 0x1f80_0000) == 0x1280_0000`
  - `sf = (raw >> 31) & 1`
  - `opc = (raw >> 29) & 3`
  - `hw = (raw >> 21) & 3`
  - `imm16 = (raw >> 5) & 0xffff`
  - `rd = raw & 0x1f`
  - supported subset: `sf == 1`, `opc == 2`, `hw == 0`
- SVC decoder mask and fields:
  - class predicate: `(raw & 0xffe0_001f) == 0xd400_0001`
  - `imm = (raw >> 5) & 0xffff`
  - supported subset: `imm == 0`
- Decoded seed instructions:
  - `0xd2800ba8` at `0x0000000000210120`: `move_wide_immediate`, `movz`, `sf=64`, `rd=8`, `imm=93`, `shift=0`
  - `0xd2800540` at `0x0000000000210124`: `move_wide_immediate`, `movz`, `sf=64`, `rd=0`, `imm=42`, `shift=0`
  - `0xd4000001` at `0x0000000000210128`: `exception_generation`, `svc`, `imm=0`
- Added decoder-specific negative fixtures:
  - `tools/tcti/fixtures/golden_elf/init_001_exit_movz_shift.S`
  - `tools/tcti/fixtures/golden_elf/init_001_exit_svc_imm1.S`
  - existing `tools/tcti/fixtures/golden_elf/init_001_exit_unsupported_before_svc.S` now covers unknown instruction
- Decoder-specific reducer paths:
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-unsupported-movz-shift.json`
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-unsupported-svc-immediate.json`
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-unknown.json`
- `tcti-contract` now reports the passing group:
  - minimal AArch64 decode semantics execute `init_001_exit` to captured `exit(42)`
- `tcti-contract` still exits non-zero with `status=todo` because deeper groups remain TODO:
  - gadget ABI and register commit-back execution
  - `FETCH`/`READ`/`WRITE` memory execution
  - TLB, block-cache, invalidation, and direct-chain execution

Report paths:

- `Build/TCTI/reports/tcti-golden-elf/report.json`
- `Build/TCTI/golden_elf/init_001_exit/execution.json`
- `Build/TCTI/reports/tcti-contract/report.json`

Evidence:

```sh
rtk proxy git diff --check
rtk proxy make tcti-plan-consistency
rtk proxy make tcti-report-schema-check
rtk proxy make tcti-toolchain-check
rtk proxy make tcti-golden-elf
rtk proxy make tcti-appstore-safety-audit
rtk proxy make tcti-golden-elf CASE=init_001_exit EXECUTE=switch-debug
```

All passed.

```sh
rtk proxy sh -c 'make tcti-contract; rc=$?; echo rc=$rc; exit 0'
```

Result:

- real contract groups include minimal decoded AArch64 semantics to captured `exit(42)`
- deeper groups remain TODO
- Make exited non-zero with `rc=2`

```sh
rtk proxy sh -c 'make tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-unsupported-svc-immediate.json; rc=$?; echo rc=$rc; exit 0'
```

Result:

- expected status `fail`
- actual replay status `fail`
- actual replay exit code `2`
- Make exited non-zero with `rc=2`

```sh
rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" make -f OrlixKernel/Makefile kunit PROFILE=development
rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" make -f OrlixKernel/Makefile kunit PROFILE=release
```

Both passed. The existing Xcode SDK `__alloc_size` redefinition warning remains present.

Boundary:

- No production TCTI assembly was added.
- No gadget dispatch was implemented.
- No simulator gate was run.
- No physical-device gate was run.
- No HostAdapter, UIKit, or Darwin syscall path was used.
- No Linux runtime semantics were implemented.
- No syscall implementation was added beyond a captured test-harness syscall event.
- This is only decoded seed ELF switch-debug no-phone execution proof.

### Checkpoint: Write ELF Switch-Debug Execution

- Added the second golden ELF fixture:
  - `OrlixKernel/Tests/TCTI/golden_elf/init_002_write/init_002_write.S`
  - `OrlixKernel/Tests/TCTI/golden_elf/init_002_write/golden.json`
- `init_002_write` is no-libc AArch64 Linux assembly:
  - load `x0 = 1`
  - compute `x1 = &msg` with `adr`
  - load `x2 = 6`
  - load `x8 = 64`
  - `svc #0`
  - load `x0 = 0`
  - load `x8 = 93`
  - `svc #0`
  - `msg` bytes are `hello\n`
- Exact emitted instruction encodings:
  - `0xd2800020`: `mov x0, #1`
  - `0x100000e1`: `adr x1, msg`
  - `0xd28000c2`: `mov x2, #6`
  - `0xd2800808`: `mov x8, #64`
  - `0xd4000001`: `svc #0`
  - `0xd2800000`: `mov x0, #0`
  - `0xd2800ba8`: `mov x8, #93`
  - `0xd4000001`: `svc #0`
- Extended the Swift no-phone switch-debug rail only for the emitted subset:
  - MOVZ 64-bit, `hw=0`
  - ADR to an X register, signed 21-bit immediate
  - SVC `#0`
- Added PT_LOAD file-backed byte reads to the tiny ELF harness so write capture can read the guest buffer without host-executing guest text or calling host syscalls.
- `make tcti-golden-elf CASE=init_002_write` now verifies:
  - source SHA256
  - binary SHA256
  - ELF64 AArch64 executable shape
  - entrypoint
  - expected syscall shape
  - emitted instruction words
  - `hello\n` message bytes in file-backed PT_LOAD guest memory
- `make tcti-golden-elf CASE=init_002_write EXECUTE=switch-debug` captures:
  - `write(1, "hello\n", 6)`
  - `exit(0)`
- Added write-specific negative execution fixtures:
  - `tools/tcti/fixtures/golden_elf/init_002_write_wrong_length.S`
  - `tools/tcti/fixtures/golden_elf/init_002_write_invalid_buffer.S`
  - `tools/tcti/fixtures/golden_elf/init_002_write_unsupported_adrp.S`
- Write negative reducer paths:
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-write-wrong-length.json`
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-write-invalid-buffer.json`
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-write-unsupported-adrp.json`
- `tcti-contract` now reports another real passing group:
  - decoded switch-debug executes `init_002_write` and captures `write(1, "hello\n", 6), exit(0)`
- `tcti-contract` still exits non-zero with `status=todo` because deeper groups remain TODO:
  - gadget ABI and register commit-back execution
  - `FETCH`/`READ`/`WRITE` memory execution
  - TLB, block-cache, invalidation, and direct-chain execution

Report paths:

- `Build/TCTI/reports/tcti-golden-elf/report.json`
- `Build/TCTI/golden_elf/init_002_write/execution.json`
- `Build/TCTI/reports/tcti-contract/report.json`

Boundary:

- No production TCTI assembly was added.
- No gadget dispatch was implemented.
- No simulator gate was run.
- No physical-device gate was run.
- No HostAdapter, UIKit, or Darwin syscall path was used.
- No Linux runtime semantics were implemented.
- No fd table, VFS, process, signal, scheduler, or real syscall behavior was implemented.
- Write and exit are captured test-harness syscall events only.

### Checkpoint: Agent Harness Cleanup

- Removed the homemade Orlix TCTI MCP implementation.
- Removed the repo-local LLDB MCP wrapper. LLDB MCP is now documented as an external tool configured by the user or environment.
- Removed the generic harness checker from `tools/`; reusable harness scripts now live inside skills.
- Replaced Codex-canonical harness wording with an agent-neutral model:
  - repo-local workflow logic lives in `.agents/skills/`;
  - skill scripts live in `.agents/skills/<skill-name>/scripts/`;
  - `.codex/` is only the Codex adapter;
  - MCP is reserved for external, proven tools.
- Added replacement TCTI skills for the former fake MCP surfaces:
  - `orlix-tcti-status`;
  - `orlix-tcti-report-reader`;
  - `orlix-tcti-reproducer`;
  - `orlix-tcti-golden-elf`;
  - `orlix-tcti-plan-consistency`.
- Normalized existing TCTI skills:
  - `orlix-tcti-next-step`;
  - `orlix-tcti-oracle`;
  - `orlix-tcti-safety`;
  - `orlix-tcti-debug`.
- Moved hook policy logic into skill-local scripts and left `.codex/hooks/` as thin adapters.
- Added agent-neutral harness docs:
  - `docs/harness/ORLIX_TCTI_AGENT_HARNESS.md`;
  - `docs/harness/MCP_POLICY.md`;
  - `docs/harness/future/ORLIX_TCTI_MCP_DEFERRED.md`.
- Added canonical agent-neutral Make targets:
  - `make agent-harness-check`;
  - `make agent-hooks-check`;
  - `make agent-skills-check`;
  - `make agent-subagents-check`;
  - `make agent-mcp-check`.
- Removed Codex-specific Make target aliases. The harness exposes `agent-*` targets only.

Evidence:

```sh
rtk proxy git diff --check
rtk proxy make tcti-plan-consistency
rtk proxy make tcti-report-schema-check
rtk proxy make tcti-toolchain-check
rtk proxy make tcti-golden-elf
rtk proxy make tcti-golden-elf CASE=init_001_exit EXECUTE=switch-debug
rtk proxy make tcti-appstore-safety-audit
rtk proxy make agent-harness-check
rtk proxy make agent-hooks-check
rtk proxy make agent-skills-check
rtk proxy make agent-subagents-check
rtk proxy make agent-mcp-check
```

All commands passed.

Alias removal check: a rejected-target-name scan across `.codex`, `.agents`, `AGENTS.md`, `Makefile`, `docs`, and `tools` produced no matches.

Kernel sanity:

```sh
rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" make -f OrlixKernel/Makefile kunit PROFILE=development
rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" make -f OrlixKernel/Makefile kunit PROFILE=release
```

Both passed. The existing Xcode SDK `__alloc_size` redefinition warning remains present.

Boundary:

- No TCTI runtime feature was implemented.
- No production TCTI assembly was added.
- No gadget dispatch was implemented.
- No simulator gate was run.
- No physical-device gate was run.
- No HostAdapter, UIKit, Darwin syscall, fd table, VFS, process, signal, scheduler, or Linux runtime behavior was added.

### Checkpoint: Agent Harness Policy Split

- Removed committed Codex execution policy from `.codex/config.toml`:
  - no repo-level `sandbox_mode`;
  - no repo-level `approval_policy`.
- Documented that sandbox mode, approval policy, secrets, and machine-local MCP credentials belong in `~/.codex/config.toml`.
- Renamed the broad `AGENTS.md` harness section from `Codex Harness` to `Agent Harness`.
- Added harness enforcement that fails if `.codex/config.toml` reintroduces committed `sandbox_mode` or `approval_policy`.

Boundary:

- No TCTI runtime feature was implemented.
- No production TCTI assembly was added.
- No gadget dispatch was implemented.
- No simulator gate was run.
- No physical-device gate was run.

### Checkpoint: PLAN Captures Autonomous Agent Loop

- Updated `PLAN.md` so the latest agent harness is a required part of the TCTI execution plan.
- Added `agent-status`, `agent-next`, and `agent-task-envelope-check` to success criteria and the autonomous test contract.
- Recorded `.agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json` as the skill-owned roadmap source, not an MCP.
- Recorded generated artifacts:
  - `Build/AgentHarness/orlix-tcti/status.json`
  - `Build/AgentHarness/orlix-tcti/next-task.json`
  - `Build/AgentHarness/orlix-tcti/next-task.md`
- Recorded current selected gate: `switch-init-003-stack`.
- Boundary:
  - No TCTI runtime feature implemented.
  - No production assembly.
  - No gadget dispatch.
  - No simulator or physical-device gate.
  - No custom MCP or `tools/agent` added.
