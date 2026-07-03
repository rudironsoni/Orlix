# IMPLEMENT.md

## 2026-07-03

### Checkpoint: Static PIE Gate Passed And Legacy Reducer Replays Through Dispatcher

- Harness-selected sequence:
  - `simulator-tcti-init-first-syscall` because simulator reports were stale after `410bf832940270607b73799d360a48c73ad228b7`.
  - `tcti-static-pie-relocation-fix` after the simulator first-syscall report refreshed.
  - `blocked-physical-device-opt-in-required` after no-phone and simulator prerequisites passed and no explicit physical-device opt-in was present.
- Why selected:
  - The agent harness requires current simulator evidence before it can evaluate post-simulator TCTI gates.
  - The static PIE relocation gate initially failed only because the latest simulator stability report was stale relative to the current HEAD.
  - After refreshing simulator stability on the pinned simulator, `tcti-static-pie-relocation-fix` passed.
  - The next remaining roadmap step is physical-device certification, but `physical_device_allowed=false`, so the harness emitted the blocked physical opt-in envelope instead of selecting phone work.
- Simulator proof:
  - Pinned simulator:
    - `Orlix-iPhone-15-Pro-Max`.
    - UDID `C47ED88D-0D0A-420D-8C78-D4C1D34A276D`.
  - Fresh first-syscall report:
    - `Build/Reports/runtime/tcti-init-first-syscall-20260703T144615Z-53604.json`.
    - `git_sha=410bf832940270607b73799d360a48c73ad228b7`.
    - `status=pass`, `passed=true`, `destination=iphonesimulator`, `selected_device_id=C47ED88D-0D0A-420D-8C78-D4C1D34A276D`, `simulator_single_booted=true`, `preflight_only=false`, `autonomous_tests_bypassed=false`.
    - `release_gate_eligible=false`, `readiness_gate_eligible=false`.
  - Fresh simulator stability report:
    - `Build/Reports/runtime/tcti-simulator-stability-20260703T144932Z-70171.json`.
    - `git_sha=410bf832940270607b73799d360a48c73ad228b7`.
    - `status=pass`, `passed=true`, `destination=iphonesimulator`, `selected_device_id=C47ED88D-0D0A-420D-8C78-D4C1D34A276D`, `simulator_single_booted=true`, `failures=[]`, `coverage_warnings=[]`.
    - `release_gate_eligible=false`, `readiness_gate_eligible=false`.
- Static PIE proof:
  - Report:
    - `Build/TCTI/reports/tcti-static-pie-relocation-fix/report.json`.
    - `git_sha=410bf832940270607b73799d360a48c73ad228b7`.
    - `status=pass`, `passed=true`, `failures=[]`.
    - `release_gate_eligible=false`, `readiness_gate_eligible=false`.
- Reducer replay compatibility:
  - Updated `tools/tcti/orlix-tcti-gate.swift` so `tcti-repro` normalizes legacy reducer commands that reference retired direct make targets:
    - `make tcti-golden-elf-refresh` becomes `make tcti-gate TARGET=tcti-golden-elf-refresh`.
    - `make tcti-golden-elf` becomes `make tcti-gate TARGET=tcti-golden-elf`.
    - `make tcti-repro` becomes `make tcti-gate TARGET=tcti-repro`.
  - This preserves the single generic dispatcher model instead of restoring one Make target per TCTI function.
  - Replayed reducer:
    - `Build/TCTI/reproducers/tcti-golden-elf/execution-static-pie-got-unrelocated-byte-load.json`.
  - Replay report:
    - `Build/TCTI/reports/tcti-repro/report.json`.
    - `status=pass`, `passed=true`, `expected_status=fail`, `actual_replay_status=fail`, `failures=[]`.
- Harness state after the checkpoint:
  - Status JSON:
    - `Build/AgentHarness/orlix-tcti/status.json`.
  - Next-task JSON:
    - `Build/AgentHarness/orlix-tcti/next-task.json`.
  - Next-task Markdown:
    - `Build/AgentHarness/orlix-tcti/next-task.md`.
  - `agent-status` reported `simulator_gates_complete=true`, `physical_device_allowed=false`, `release_gate_eligible=false`, `readiness_gate_eligible=false`, and `next_eligible_gate=none`.
  - `agent-next` selected `blocked-physical-device-opt-in-required`.
- Verification:
  - `rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift` passed.
  - `rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-static-pie-got-unrelocated-byte-load.json` passed.
  - `rtk proxy make tcti-gate TARGET=tcti-static-pie-relocation-fix` passed.
  - `rtk proxy git diff --check` passed.
  - `rtk proxy make tcti-gate TARGET=tcti-plan-consistency` passed.
  - `rtk proxy make agent-harness-check` passed.
  - `rtk proxy make agent-status AREA=orlix-tcti` passed.
  - `rtk proxy make agent-next AREA=orlix-tcti` passed.
  - `rtk proxy make agent-task-envelope-check AREA=orlix-tcti` passed for the blocked physical opt-in envelope.
  - `rtk proxy make tcti-gate TARGET=tcti-report-schema-check` passed.
  - `rtk proxy make tcti-gate TARGET=tcti-toolchain-check` passed.
  - `rtk proxy make tcti-gate TARGET=tcti-golden-elf` passed.
  - `rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit` passed.
  - `rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" make -f OrlixKernel/Makefile kunit PROFILE=development` passed.
  - `rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" make -f OrlixKernel/Makefile kunit PROFILE=release` passed after rerunning serially. The first parallel attempt collided on the upstream Linux checkout `shallow.lock` while the development profile was still running.
- Boundary:
  - No physical-device gate was run in this checkpoint.
  - No TCTI runtime feature was implemented in this checkpoint.
  - No production TCTI assembly.
  - No gadget dispatch.
  - No HostAdapter Linux behavior.
  - No Darwin syscall guest side effect.
  - No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added outside Linux ownership.
  - No generated Linux or build tree edits.
  - No custom MCP added.
  - No `tools/agent` added.
  - No product defconfig flip.
  - Full TCTI remains incomplete.

### Checkpoint: Simulator Proof Refreshed And Physical Gate Requires Explicit Opt-In

- Harness-selected state:
  - `blocked-physical-device-opt-in-required`.
- Selected command:
  - `no-op: set ORLIX_TCTI_ALLOW_PHYSICAL_DEVICE=1 only after explicit human approval`.
- Why selected:
  - Current no-phone prerequisites, simulator first-syscall, simulator runtime stability, App Store safety, and report schema gates are passing at `d15a910f266844adcf79d3bb1ca6695d3673ccc5`.
  - The first missing roadmap gate is the physical first-syscall certification gate.
  - Simulator completion is required before phone work, but it is not permission to run a phone gate.
  - `agent-status` now reports `physical_device_allowed=false` unless `ORLIX_TCTI_ALLOW_PHYSICAL_DEVICE=1` or `ORLIX_TCTI_PHYSICAL_DEVICE_ALLOWED=1` is explicitly present.
- Implementation:
  - Updated `.agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift` so physical-device gates are not selected without explicit opt-in.
  - Added a machine-readable blocked envelope for the state where the only remaining eligible gate is physical-device work and opt-in is absent.
  - Updated `.agents/skills/orlix-tcti-next-step/SKILL.md` with the explicit physical opt-in stop condition.
- Simulator proof:
  - Only booted simulator before and after the gates:
    - `Orlix-iPhone-15-Pro-Max`.
    - UDID `C47ED88D-0D0A-420D-8C78-D4C1D34A276D`.
  - Fresh first-syscall report:
    - `Build/Reports/runtime/tcti-init-first-syscall-20260703T142113Z-94196.json`.
    - `Build/Reports/runtime/tcti-init-first-syscall-20260703T142113Z-94196.md`.
    - `status=pass`, `passed=true`, `destination=iphonesimulator`, `simulator_single_booted=true`, `preflight_only=false`, `autonomous_tests_bypassed=false`.
  - Fresh simulator stability report:
    - `Build/Reports/runtime/tcti-simulator-stability-20260703T143856Z-42565.json`.
    - `Build/Reports/runtime/tcti-simulator-stability-20260703T143856Z-42565.md`.
    - `status=pass`, `passed=true`, `destination=iphonesimulator`, `simulator_single_booted=true`, `failures=[]`, `coverage_warnings=[]`.
  - Both reports kept forbidden behavior fields false:
    - `generated_exec_memory=false`.
    - `host_exec_guest_text=false`.
    - `host_x18=false`.
    - `map_jit=false`.
    - `native_ios_api_exposure_to_guest=false`.
    - `rwx=false`.
- Reports:
  - Status JSON: `Build/AgentHarness/orlix-tcti/status.json`.
  - Next-task JSON: `Build/AgentHarness/orlix-tcti/next-task.json`.
  - Next-task Markdown: `Build/AgentHarness/orlix-tcti/next-task.md`.
  - `next-task.json` selected `blocked-physical-device-opt-in-required`.
  - `next-task.json` did not select `physical-tcti-init-first-syscall`.
- Verification:
  - `rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift` passed.
  - `rtk proxy git diff --check` passed.
  - `rtk proxy make agent-harness-check` passed.
  - `rtk proxy make agent-status AREA=orlix-tcti` passed and reported `physical_device_allowed=false`, `simulator_gates_complete=true`, `release_gate_eligible=false`, and `readiness_gate_eligible=false`.
  - `rtk proxy make agent-next AREA=orlix-tcti` passed and selected `blocked-physical-device-opt-in-required`.
  - `rtk proxy make agent-task-envelope-check AREA=orlix-tcti` passed for the blocked envelope.
  - `rtk proxy make tcti-gate TARGET=tcti-plan-consistency` passed.
  - `rtk proxy make tcti-gate TARGET=tcti-report-schema-check` passed.
  - `rtk proxy make tcti-gate TARGET=tcti-toolchain-check` passed.
  - `rtk proxy make tcti-gate TARGET=tcti-golden-elf` passed.
  - `rtk proxy make tcti-gate TARGET=tcti-golden-elf CASE=init_001_exit EXECUTE=switch-debug` passed.
  - `rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit` passed.
  - `rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" xcrun simctl list devices booted` showed only `Orlix-iPhone-15-Pro-Max (C47ED88D-0D0A-420D-8C78-D4C1D34A276D)`.
  - `rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" HOMEBREW_NO_AUTO_UPDATE=1 USER=rudironsoni LOGNAME=rudironsoni ORLIX_BUILD_ROOT="$PWD/Build" make runtime-validation DESTINATION=iphonesimulator GATE=tcti-init-first-syscall ORLIX_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=Orlix-iPhone-15-Pro-Max` passed.
  - `rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" HOMEBREW_NO_AUTO_UPDATE=1 USER=rudironsoni LOGNAME=rudironsoni ORLIX_BUILD_ROOT="$PWD/Build" make runtime-validation DESTINATION=iphonesimulator GATE=tcti-simulator-stability ORLIX_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=Orlix-iPhone-15-Pro-Max` passed.
- Boundary:
  - No physical-device gate was run.
  - No TCTI runtime feature was implemented in this checkpoint.
  - No production TCTI assembly.
  - No gadget dispatch.
  - No HostAdapter Linux behavior.
  - No Darwin syscall guest side effect.
  - No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added outside Linux ownership.
  - No generated Linux or build tree edits.
  - No custom MCP added.
  - No `tools/agent` added.
  - No product defconfig flip.
  - Full TCTI remains incomplete.

### Checkpoint: Physical First-Syscall Gate Still Blocked By Device DDI

- Harness-selected gate:
  - `physical-tcti-init-first-syscall`.
- Selected command:
  - `make runtime-validation DESTINATION=iphoneos GATE=tcti-init-first-syscall`.
- Why selected:
  - After refreshing the current-HEAD simulator first-syscall and simulator stability reports on `Orlix-iPhone-15-Pro-Max`, the harness reported `physical_device_allowed=true`.
  - `agent-next` selected the physical first-syscall gate.
  - `agent-task-envelope-check` passed for the physical first-syscall envelope.
- Simulator prerequisite refresh:
  - `Build/Reports/runtime/tcti-simulator-stability-20260703T094852Z-93913.json` passed.
  - `Build/Reports/runtime/tcti-init-first-syscall-20260703T095214Z-99564.json` passed.
  - The only booted simulator before these gates was `Orlix-iPhone-15-Pro-Max` with UDID `C47ED88D-0D0A-420D-8C78-D4C1D34A276D`.
- Physical result:
  - Report: `Build/Reports/runtime/tcti-init-first-syscall-20260703T095435Z-4244.json`.
  - Markdown: `Build/Reports/runtime/tcti-init-first-syscall-20260703T095435Z-4244.md`.
  - `status=fail`.
  - `passed=false`.
  - `destination=iphoneos`.
  - `selected_device_name=RRJ-iPhone-15-Pro-Max`.
  - `selected_device_id=7F8A1701-D612-5A9C-AAE7-8FD0AD77306C`.
  - `selected_xcode_device_id=00008130-001E74A11193803A`.
  - `release_gate_eligible=false`.
  - `readiness_gate_eligible=false`.
- Device blocker:
  - `devices.json` shows `developerModeStatus=enabled` for `RRJ-iPhone-15-Pro-Max`.
  - `devices.json` shows `ddiServicesAvailable=false`.
  - The gate failed before app build, install, launch, and before any TCTI guest execution.
  - No reducer was created because this is Xcode/device developer-disk-image readiness, not a guest execution failure.
- Forbidden behavior fields in the physical failure report remained false:
  - `generated_exec_memory=false`.
  - `host_exec_guest_text=false`.
  - `host_x18=false`.
  - `map_jit=false`.
  - `native_ios_api_exposure_to_guest=false`.
  - `rwx=false`.
- Boundary:
  - Physical gate did not pass.
  - No production TCTI code was changed for this device readiness failure.
  - No custom MCP added.
  - No `tools/agent` added.
  - No production TCTI assembly.
  - No gadget dispatch.
  - No HostAdapter Linux behavior.
  - No Darwin syscall guest side effect.
  - No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added outside Linux ownership.
  - No generated Linux or build tree edits.
  - No generated executable memory.
  - No host-executable guest text.
  - No product defconfig flip.
  - Full TCTI remains incomplete.

### Checkpoint: Historical Simulator Reducers Accept Fixed Structured State

- Harness-selected sequence:
  - `no-phone-tcti-simulator-user-fault-reducer`.
  - `no-phone-tcti-post-overlay-null-user-fault-reducer`.
  - `no-phone-tcti-post-bash-mmap-read-fault-reducer`.
- Why selected:
  - Rerunning historical reducer targets after the current simulator stability pass had rewritten their reports to `fail`.
  - The reducers were still requiring the latest `tcti-simulator-stability` report to be a live failure with old fatal log signatures.
  - The current simulator stability report is a structured pass at `1ab82adba184c343e86942cf282c9aebf4238b8c`, so the reducers were blocking the harness even though the simulator no longer reproduces those fatal states.
- Implementation:
  - Updated `tools/tcti/orlix-tcti-gate.swift` so the selected historical reducers accept either:
    - a current structured failure report matching their reduced fault shape, or
    - a current structured passing simulator report proving the relevant progress event and no fatal user fault.
  - Removed fixed PC/LR and panic-log string requirements from the post-overlay reducer decision path.
  - Kept reducer replay as the no-phone proof for the original failure fixtures.
  - Updated `.agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift` envelope wording so the reducer requirements describe structured report facts instead of stale "latest failure must match" log text.
- Reports:
  - `Build/TCTI/reports/tcti-simulator-user-fault-reducer/report.json` now passes.
  - `Build/TCTI/reports/tcti-post-overlay-null-user-fault-reducer/report.json` now passes.
  - `Build/TCTI/reports/tcti-post-bash-mmap-read-fault-reducer/report.json` now passes.
- Reducer replay:
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-static-pie-got-unrelocated-byte-load.json` replayed with expected `fail` and actual `fail`.
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-static-pie-got-relocation-invisible-byte-load.json` replayed with expected `fail` and actual `fail`.
  - `Build/TCTI/reproducers/tcti-post-bash-mmap-read-fault-reducer/post-bash-mmap-read-fault-pass-regression.json` replayed with expected `pass` and actual `pass`.
- Harness state after reducer repair:
  - `Build/AgentHarness/orlix-tcti/status.json` reports `physical_device_allowed=true`.
  - `Build/AgentHarness/orlix-tcti/status.json` reports `next_eligible_gate=physical-tcti-init-first-syscall`.
  - `Build/AgentHarness/orlix-tcti/next-task.json` selects `physical-tcti-init-first-syscall`.
  - Release and readiness eligibility remain false until the physical runtime gate produces a non-override passing report.
- Verification:
  - `rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift`.
  - `rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift`.
  - `rtk proxy make tcti-gate TARGET=tcti-simulator-user-fault-reducer`.
  - `rtk proxy make tcti-gate TARGET=tcti-post-overlay-null-user-fault-reducer`.
  - `rtk proxy make tcti-gate TARGET=tcti-post-bash-mmap-read-fault-reducer`.
  - `rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-static-pie-got-unrelocated-byte-load.json`.
  - `rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-static-pie-got-relocation-invisible-byte-load.json`.
  - `rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-post-bash-mmap-read-fault-reducer/post-bash-mmap-read-fault-pass-regression.json`.
  - `rtk proxy git diff --check`.
  - `rtk proxy make agent-harness-check`.
  - `rtk proxy make agent-status AREA=orlix-tcti`.
  - `rtk proxy make agent-next AREA=orlix-tcti`.
  - `rtk proxy make agent-task-envelope-check AREA=orlix-tcti`.
  - `rtk proxy make tcti-gate TARGET=tcti-plan-consistency`.
  - `rtk proxy make tcti-gate TARGET=tcti-report-schema-check`.
  - `rtk proxy make tcti-gate TARGET=tcti-toolchain-check`.
  - `rtk proxy make tcti-gate TARGET=tcti-golden-elf`.
  - `rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit`.
- Boundary:
  - No custom MCP added.
  - No `tools/agent` added.
  - No production TCTI assembly.
  - No gadget dispatch.
  - No simulator gate run in this checkpoint.
  - No physical-device gate run in this checkpoint.
  - No HostAdapter Linux behavior.
  - No Darwin syscall guest side effect.
  - No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added outside Linux ownership.
  - No generated Linux or build tree edits.
  - No generated executable memory.
  - No host-executable guest text.
  - No product defconfig flip.
  - Full TCTI remains incomplete.

### Checkpoint: Simulator Stability After Post-Bash Mmap Read Fault

- Harness-selected gate:
  - `simulator-tcti-runtime-stability`.
- Selected command:
  - `make runtime-validation DESTINATION=iphonesimulator GATE=tcti-simulator-stability ORLIX_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=Orlix-iPhone-15-Pro-Max`.
- Why selected:
  - `agent-status` reported all no-phone and first simulator prerequisites passing.
  - The latest current simulator stability evidence still failed with a post-`mmap(222)` `/bin/sh` TCTI read fault.
  - `agent-next` selected `simulator-tcti-runtime-stability`.
- Reducer evidence:
  - `Build/TCTI/reports/tcti-post-bash-mmap-read-fault-reducer/report.json`.
  - `Build/TCTI/reproducers/tcti-post-bash-mmap-read-fault-reducer/post-bash-mmap-read-fault-pass-regression.json`.
  - `make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-post-bash-mmap-read-fault-reducer/post-bash-mmap-read-fault-pass-regression.json` exited 0.
- Root cause evidence:
  - The failing report was `Build/Reports/runtime/tcti-simulator-stability-20260703T085507Z-30348.json`.
  - Its structured runtime event showed `task=sh`, `pid=32`, `faultPC=0x27387c6a8c64`, `faultAddress=0x27387c87ff20`, `access=1`, `si=1`, after `mmap(222)` with `addr=0`, `len=0x80000`, `prot=0x3`, `flags=0x22`.
  - Binary inspection found the report ELF entry and the extracted current Bash artifact differ by `0xc000`; after normalizing that delta, the fault site is `mrs x8, TPIDR_EL0` followed by `ldur w20, [x8, #-0x60]`.
  - The fault address matches `TPIDR_EL0 - 0x60`.
- Implementation:
  - Updated `OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/mm/tcti_user_page.c`.
  - After `tcti_copy_user_data()` successfully faults in a missing guest READ or WRITE page, it now synchronizes the hosted fault window with `orlix_sync_current_user_fault_window(address, 0)` before retrying the direct TCTI copy.
  - This keeps Linux MM authoritative and does not add HostAdapter Linux behavior or Linux runtime semantics to TCTI.
- Simulator proof before commit:
  - Only booted simulator:
    - `Orlix-iPhone-15-Pro-Max`.
    - UDID `C47ED88D-0D0A-420D-8C78-D4C1D34A276D`.
  - Passing report:
    - `Build/Reports/runtime/tcti-simulator-stability-20260703T091423Z-85985.json`.
    - `Build/Reports/runtime/tcti-simulator-stability-20260703T091423Z-85985.md`.
  - Report facts:
    - `status=pass`.
    - `passed=true`.
    - `destination=iphonesimulator`.
    - `selected_device_id=C47ED88D-0D0A-420D-8C78-D4C1D34A276D`.
    - `simulator_single_booted=true`.
    - `failures=[]`.
    - `coverage_warnings=[]`.
    - `fatal_user_fault` fields are null.
  - `agent-status` then advanced the next eligible gate to `physical-tcti-init-first-syscall`.
- Verification before commit:
  - `rtk proxy git diff --check`.
  - `rtk proxy make agent-task-envelope-check AREA=orlix-tcti`.
  - `rtk proxy make tcti-gate TARGET=tcti-post-bash-mmap-read-fault-reducer`.
  - `rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-post-bash-mmap-read-fault-reducer/post-bash-mmap-read-fault-pass-regression.json`.
  - `rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" make -f OrlixKernel/Makefile kunit PROFILE=development`.
  - `rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" make -f OrlixKernel/Makefile kunit PROFILE=release`.
  - `rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" USER=rudironsoni LOGNAME=rudironsoni ORLIX_BUILD_ROOT="$PWD/Build" HOMEBREW_NO_AUTO_UPDATE=1 make runtime-validation DESTINATION=iphonesimulator GATE=tcti-simulator-stability ORLIX_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=Orlix-iPhone-15-Pro-Max`.
- Boundary:
  - No custom MCP added.
  - No `tools/agent` added.
  - No production TCTI assembly.
  - No gadget dispatch.
  - No physical-device gate run.
  - No HostAdapter Linux behavior.
  - No Darwin syscall guest side effect.
  - No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added outside Linux ownership.
  - No generated Linux or build tree edits.
  - No generated executable memory.
  - No host-executable guest text.
  - No product defconfig flip.
  - Full TCTI remains incomplete.

## 2026-07-02

### Checkpoint: Simulator First Syscall Gate Certified

- Harness-selected gate:
  - `simulator-tcti-init-first-syscall`.
- Selected command:
  - `make runtime-validation DESTINATION=iphonesimulator GATE=tcti-init-first-syscall`.
- Why selected:
  - The harness reported `simulator-tcti-init-first-syscall` as the next stale or missing current-HEAD simulator runtime gate.
  - The prerequisite `tcti-direct-chain-fuzz` was already passing.
- Simulator used:
  - `Orlix-iPhone-15-Pro-Max`.
  - UDID `C47ED88D-0D0A-420D-8C78-D4C1D34A276D`.
  - `xcrun simctl list devices booted` showed only this simulator booted before the gate.
- Result:
  - Gate passed at git SHA `60ce939c2f15b27581b13a75f47e9b255bec4068`.
  - `preflight_only=false`.
  - `autonomous_tests_bypassed=false`.
  - `readiness_gate_eligible=false`.
  - `release_gate_eligible=false`.
- Reports:
  - `Build/Reports/runtime/tcti-init-first-syscall-20260702T193236Z-11635.json`.
  - `Build/Reports/runtime/tcti-init-first-syscall-20260702T193236Z-11635.md`.
  - Artifact directory:
    - `Build/Reports/runtime/tcti-init-first-syscall-20260702T193236Z-11635.artifacts`.
- Forbidden behavior fields remained false:
  - `generated_exec_memory=false`.
  - `host_exec_guest_text=false`.
  - `host_x18=false`.
  - `map_jit=false`.
  - `native_ios_api_exposure_to_guest=false`.
  - `rwx=false`.
- Boundary:
  - This checkpoint certifies the first simulator TCTI syscall marker only.
  - It does not certify simulator runtime stability.
  - It does not permit physical-device validation.
  - It does not make release or readiness gates eligible.
  - It did not change production TCTI runtime code.

Verification:

```text
rtk proxy make tcti-gate TARGET=tcti-plan-consistency
rtk proxy make tcti-gate TARGET=tcti-report-schema-check
rtk proxy make tcti-gate TARGET=tcti-toolchain-check
rtk proxy make tcti-gate TARGET=tcti-golden-elf
rtk proxy make tcti-gate TARGET=tcti-golden-elf CASE=init_001_exit EXECUTE=switch-debug
rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit
rtk proxy make agent-harness-check
rtk proxy make agent-task-envelope-check AREA=orlix-tcti
rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" xcrun simctl list devices booted
rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" USER=rudironsoni LOGNAME=rudironsoni ORLIX_BUILD_ROOT="$PWD/Build" ORLIX_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D make runtime-validation DESTINATION=iphonesimulator GATE=tcti-init-first-syscall
```

Results:

- All commands above exited 0.
- `agent-task-envelope-check` continued to validate the current first-syscall envelope before status regeneration.
- Full TCTI remains incomplete. The harness must select the next eligible gate before any further implementation.

### Checkpoint: No-Phone Reducer For Simulator Null User Fault

- Harness-selected gate before this checkpoint:
  - `simulator-tcti-runtime-stability`.
- Safety finding:
  - The selected simulator stability envelope required fatal simulator runtime errors to be reduced before production patching.
  - The harness did not yet expose that reduction as its own selectable gate.
- Harness fix:
  - Added no-phone generated gate `no-phone-tcti-simulator-user-fault-reducer`.
  - Added Make target `tcti-simulator-user-fault-reducer`.
  - `simulator-tcti-runtime-stability` now depends on the reducer gate.
  - The reducer gate command is:
    - `make tcti-gate TARGET=tcti-simulator-user-fault-reducer`.
  - The reducer gate stays no-phone and does not permit production TCTI assembly, gadget dispatch, HostAdapter Linux behavior, generated-tree edits, product defconfig flips, simulator reruns, or phone work.
- Reducer behavior:
  - Reuses the existing `init_011_static_pie_got_byte_load` static PIE golden ELF.
  - Positive switch-debug still applies the `R_AARCH64_RELATIVE` GOT relocation and exits `42`.
  - Negative mode `NEGATIVE_EXECUTION=static-pie-got-unrelocated-byte-load` disables relative relocation application only for this reducer.
  - The negative mode also guards the null page so the unrelocated GOT slot causes `LDRB` to capture a read fault at `0x0` instead of reading ELF header bytes.
  - This models the simulator fatal signature:
    - `Orlix TCTI: user fault ... addr=0x0 access=1`.
    - `Kernel panic - not syncing: Attempted to kill init!`.
- Reports:
  - `Build/TCTI/reports/tcti-simulator-user-fault-reducer/report.json`.
  - `Build/TCTI/simulator_user_fault_reducer/positive/init_011_static_pie_got_byte_load/execution.json`.
  - `Build/TCTI/simulator_user_fault_reducer/negative/init_011_static_pie_got_byte_load/static-pie-got-unrelocated-byte-load-execution.json`.
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-static-pie-got-unrelocated-byte-load.json`.
- Reducer replay:
  - `make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-static-pie-got-unrelocated-byte-load.json`.
  - Expected status: `fail`.
  - Actual replay status: `fail`.
  - Replay report: `Build/TCTI/reports/tcti-repro/report.json`.
- Harness state after reducer:
  - `agent-status` reports `no-phone-tcti-simulator-user-fault-reducer` as passing.
  - `agent-next` advances back to `simulator-tcti-runtime-stability`.
  - `physical_device_allowed=false`.
  - `release_gate_eligible=false`.
  - `readiness_gate_eligible=false`.
- Simulator rerun:
  - Command used the only allowed simulator, `Orlix-iPhone-15-Pro-Max` with UDID `C47ED88D-0D0A-420D-8C78-D4C1D34A276D`.
  - `xcrun simctl list devices booted` showed only that simulator booted.
  - `make runtime-validation DESTINATION=iphonesimulator GATE=tcti-simulator-stability` still failed.
  - Report:
    - `Build/Reports/runtime/tcti-simulator-stability-20260702T192124Z-89441.json`.
    - `Build/Reports/runtime/tcti-simulator-stability-20260702T192124Z-89441.md`.
  - Fatal facts:
    - `Orlix TCTI: svc #0 task=init pid=1 pc=0x74ab63c86a80 syscall=178 x0=0xb2 x1=0x74ab63c854b4 x2=0x0 x3=0x0 x4=0x0 x5=0x0`.
    - `Orlix TCTI: user fault task=init pid=1 pc=0x74ab63c9a1f4 lr=0x74ab63c99ffc sp=0x74ab73c3f800 addr=0x0 access=1 si=1`.
    - `Kernel panic - not syncing: Attempted to kill init! exitcode=0x0000000b`.
  - Forbidden behavior fields remained false:
    - `generated_exec_memory=false`.
    - `host_exec_guest_text=false`.
    - `host_x18=false`.
    - `map_jit=false`.
    - `native_ios_api_exposure_to_guest=false`.
    - `rwx=false`.
- Next implementation checkpoint:
  - Fix the production TCTI memory or relocation behavior that makes the simulator path reach the same null user fault.
  - Do not rerun the same stability gate as a pass claim until the reducer-derived fix exists.

Verification:

```text
rtk proxy git diff --check
rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift
rtk proxy swift .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift status
rtk proxy make agent-harness-check
rtk proxy make tcti-gate TARGET=tcti-plan-consistency
rtk proxy make tcti-gate TARGET=tcti-report-schema-check
rtk proxy make tcti-gate TARGET=tcti-toolchain-check
rtk proxy make tcti-gate TARGET=tcti-golden-elf CASE=init_011_static_pie_got_byte_load EXECUTE=switch-debug
rtk proxy make tcti-gate TARGET=tcti-simulator-user-fault-reducer
rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-static-pie-got-unrelocated-byte-load.json
rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit
rtk proxy make agent-status AREA=orlix-tcti
rtk proxy make agent-next AREA=orlix-tcti
rtk proxy make agent-task-envelope-check AREA=orlix-tcti
rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" xcrun simctl list devices booted
rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" USER=rudironsoni LOGNAME=rudironsoni ORLIX_BUILD_ROOT="$PWD/Build" ORLIX_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D make runtime-validation DESTINATION=iphonesimulator GATE=tcti-simulator-stability
```

Boundary:

- No custom MCP added.
- No `tools/agent` added.
- No production TCTI assembly added.
- No gadget dispatch added.
- No phone gate run.
- No HostAdapter Linux behavior added.
- No Darwin syscall guest side effect added.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No generated executable memory added.
- No host-executable guest text added.
- No product defconfig flip.
- Simulator stability is still failing and full TCTI remains incomplete.

### Checkpoint: Simulator Stability Gate Blocks Phone

- User constraint:
  - TCTI must advance without the phone first.
  - The simulator is mandatory.
  - The only allowed simulator is `Orlix-iPhone-15-Pro-Max` with UDID `C47ED88D-0D0A-420D-8C78-D4C1D34A276D`.
  - Physical-device validation remains blocked until the simulator build runs without fatal TCTI runtime errors.
- Harness fix:
  - Added generated gate `simulator-tcti-runtime-stability`.
  - The simulator stability gate command is:
    - `make runtime-validation DESTINATION=iphonesimulator GATE=tcti-simulator-stability`.
  - The gate requires the earlier `simulator-tcti-init-first-syscall` report to pass first.
  - The phone gate now explicitly depends on both:
    - `simulator-tcti-init-first-syscall`.
    - `simulator-tcti-runtime-stability`.
  - Direct physical `runtime-validation` preflight now also requires a current passing simulator stability JSON report before non-override device work can proceed.
- The initial stability gate allowed scope was intentionally narrow:
  - `tools/runtime/orlix-runtime-validation.sh`.
  - `docs/plans/active/orlix-tcti/IMPLEMENT.md`.
- Production TCTI files were not allowed before reduction. After the no-phone reducer gates captured the simulator fatal runtime signatures, the current `simulator-tcti-runtime-stability` envelope may allow scoped `arch/orlix` TCTI, process, and mm fixes required by the reducer-backed simulator failure. The gate remains simulator-only and still forbids physical-device work, production assembly, gadget expansion, HostAdapter Linux behavior, Darwin syscall behavior, generated-tree edits, product defconfig flips, and Linux runtime semantics outside upstream Linux ownership.
- Runtime validation behavior:
  - `tcti-simulator-stability` is simulator-only.
  - It requires the captured `Orlix TCTI: svc #0` marker.
  - It fails if captured logs contain fatal post-launch TCTI runtime evidence:
    - `Kernel panic`.
    - `Attempted to kill init`.
    - `Attempted kill init`.
    - `Orlix TCTI: user fault`.
    - `panic - not syncing`.
    - `BUG:`.
    - `Oops`.
    - `SIGSEGV`.
    - `fatal error`.
    - `crash`.

Simulator stability validation:

```text
rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" USER=rudironsoni LOGNAME=rudironsoni ORLIX_BUILD_ROOT="$PWD/Build" ORLIX_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D make runtime-validation DESTINATION=iphonesimulator GATE=tcti-simulator-stability
```

- Result: failed as intended.
- Report:
  - `Build/Reports/runtime/tcti-simulator-stability-20260702T181746Z-80916.json`.
  - `Build/Reports/runtime/tcti-simulator-stability-20260702T181746Z-80916.md`.
- Fatal artifact:
  - `Build/Reports/runtime/tcti-simulator-stability-20260702T181746Z-80916.artifacts/tcti-simulator-fatal-runtime.txt`.
- Fatal facts captured:
  - `Orlix TCTI: user fault task=init pid=1 pc=0x3a2b51e3a1f4 lr=0x3a2b51e39ffc sp=0x3a2b6184f800 addr=0x0 access=1 si=1`.
  - `Kernel panic - not syncing: Attempted to kill init! exitcode=0x0000000b`.

Current rerun after the direct-preflight and artifact-reporting patch:

- Report:
  - `Build/Reports/runtime/tcti-simulator-stability-20260702T183541Z-11257.json`.
  - `Build/Reports/runtime/tcti-simulator-stability-20260702T183541Z-11257.md`.
- Machine-readable artifacts now include:
  - `Build/Reports/runtime/tcti-simulator-stability-20260702T183541Z-11257.artifacts/tcti-first-syscall.txt`.
  - `Build/Reports/runtime/tcti-simulator-stability-20260702T183541Z-11257.artifacts/tcti-simulator-fatal-runtime.txt`.
- Current fatal facts captured:
  - `Orlix TCTI: svc #0 task=init pid=1 pc=0x2765630a6a80 syscall=178 x0=0xb2 x1=0x2765630a54b4 x2=0x0 x3=0x0 x4=0x0 x5=0x0`.
  - `Orlix TCTI: user fault task=init pid=1 pc=0x2765630ba1f4 lr=0x2765630b9ffc sp=0x2765728af800 addr=0x0 access=1 si=1`.
  - `Kernel panic - not syncing: Attempted kill init! exitcode=0x0000000b`.
- The updated JSON report includes artifact paths instead of an empty `artifacts` array.
- The fatal matcher now covers both `Attempted to kill init` and `Attempted kill init`.

Harness behavior after the failing stability report:

- `agent-status` keeps `physical_device_allowed=false`.
- `agent-next` selects `simulator-tcti-runtime-stability`.
- `agent-task-envelope-check` validates the machine-readable envelope.
- The next implementation checkpoint must reduce the simulator user-fault/panic into a no-phone TCTI fixture or reproducer before production TCTI changes.

Boundary:

- No physical-device gate was run.
- No custom MCP added.
- No `tools/agent` added.
- No production TCTI assembly added.
- No gadget dispatch added.
- No HostAdapter Linux behavior added.
- No Darwin syscall guest side effect added.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No generated executable memory added.
- No host-executable guest text added.
- No product defconfig flip.
- No production TCTI runtime fix was made in this checkpoint.

### Checkpoint: Simulator Gate Is Mandatory Before Phone Gate

- User constraint:
  - TCTI must advance without the phone first.
  - Simulator validation is mandatory.
  - Phone validation is not allowed until the simulator build is working.
- Harness fix:
  - Added generated gate `simulator-tcti-init-first-syscall` to the TCTI next-step selector.
  - The simulator gate command is:
    - `make runtime-validation DESTINATION=iphonesimulator GATE=tcti-init-first-syscall`.
  - The simulator gate requires a matching runtime JSON report with:
    - `destination=iphonesimulator`.
    - `gate=tcti-init-first-syscall`.
    - `status=pass`.
    - `passed=true`.
    - `backend=tcti`.
    - `profile=tcti_runtime`.
    - `preflight_only=false`.
    - `autonomous_tests_bypassed=false`.
    - all `forbidden_behavior` fields false.
  - The simulator report must match current `HEAD`; stale simulator evidence does not satisfy the gate.
  - Added `simulator-tcti-init-first-syscall` to the phone gate prerequisites in `.agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json`.

Harness behavior before simulator run:

- `agent-status` selected `simulator-tcti-init-first-syscall`.
- `physical_device_allowed=false`.
- `agent-task-envelope-check` validated the simulator task envelope.

Simulator validation:

```text
rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" USER=rudironsoni LOGNAME=rudironsoni ORLIX_BUILD_ROOT="$PWD/Build" ORLIX_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D make runtime-validation DESTINATION=iphonesimulator GATE=tcti-init-first-syscall
```

- Only booted simulator:
  - `Orlix-iPhone-15-Pro-Max (C47ED88D-0D0A-420D-8C78-D4C1D34A276D)`.
- Simulator runtime report:
  - `Build/Reports/runtime/tcti-init-first-syscall-20260702T175712Z-55382.json`.
  - `Build/Reports/runtime/tcti-init-first-syscall-20260702T175712Z-55382.md`.
- Report facts:
  - `git_sha=4247f46d383401a31614bc1d1df6cf3be273bed0`.
  - `destination=iphonesimulator`.
  - `gate=tcti-init-first-syscall`.
  - `status=pass`.
  - `passed=true`.
  - `backend=tcti`.
  - `profile=tcti_runtime`.
  - `preflight_only=false`.
  - `autonomous_tests_bypassed=false`.
  - `selected_device_id=C47ED88D-0D0A-420D-8C78-D4C1D34A276D`.
  - `release_gate_eligible=false`.
  - `readiness_gate_eligible=false`.
  - forbidden behavior fields all false.

Harness behavior after simulator run:

- `agent-status` accepted `simulator-tcti-init-first-syscall` as `pass`.
- `agent-next` generated the later phone task only after the simulator prerequisite was satisfied.
- `Build/AgentHarness/orlix-tcti/next-task.json` now includes `simulator-tcti-init-first-syscall` in `prerequisite_gates` with `state=pass`.

Subagent review:

- `tcti-planner` agreed with the correction:
  - no-phone chain first.
  - `simulator-tcti-init-first-syscall` after `tcti-direct-chain-fuzz`.
  - phone gate depends on simulator gate.
- `tcti-safety-reviewer` blocked the stale phone envelope before the simulator run and required the simulator JSON facts above before any later phone task can be selected.

Boundary:

- No phone validation was run in this checkpoint.
- No custom MCP added.
- No `tools/agent` added.
- No TCTI runtime feature code changed.
- No production TCTI assembly added.
- No gadget dispatch added.
- No HostAdapter Linux behavior added.
- No Darwin syscall guest side effect added.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No generated executable memory added.
- No host-executable guest text added.
- No product defconfig flip.

### Checkpoint: Required Simulator Pass, Physical Gate Still Blocked

- Harness-selected gate: `physical-tcti-init-first-syscall`.
- Selected command:
  - `make runtime-validation DESTINATION=iphoneos GATE=tcti-init-first-syscall`.
- Harness state:
  - `Build/AgentHarness/orlix-tcti/status.json`.
  - `Build/AgentHarness/orlix-tcti/next-task.json`.
  - `next_eligible_gate=physical-tcti-init-first-syscall`.
  - `physical_device_allowed=true`.
  - `release_gate_eligible=false`.
  - `readiness_gate_eligible=false`.
- Planner and safety review:
  - `tcti-planner` confirmed the selected gate and prerequisites are valid, but the physical gate remains blocked by device readiness.
  - `tcti-safety-reviewer` passed the exact simulator and physical `runtime-validation` commands, and blocked any claim that the physical gate is passing.

Simulator validation:

```text
rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" USER=rudironsoni LOGNAME=rudironsoni ORLIX_BUILD_ROOT="$PWD/Build" ORLIX_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D make runtime-validation DESTINATION=iphonesimulator GATE=tcti-init-first-syscall
```

- report:
  - `Build/Reports/runtime/tcti-init-first-syscall-20260702T174003Z-30548.json`.
  - `Build/Reports/runtime/tcti-init-first-syscall-20260702T174003Z-30548.md`.
- result:
  - `destination=iphonesimulator`.
  - `status=pass`.
  - `passed=true`.
  - `selected_device_id=C47ED88D-0D0A-420D-8C78-D4C1D34A276D`.
  - `release_gate_eligible=false`.
  - `readiness_gate_eligible=false`.
  - forbidden behavior fields remained false.
- simulator constraint:
  - Only `Orlix-iPhone-15-Pro-Max (C47ED88D-0D0A-420D-8C78-D4C1D34A276D)` was booted.
  - No other simulator was started.

Selected physical validation:

```text
rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" USER=rudironsoni LOGNAME=rudironsoni ORLIX_BUILD_ROOT="$PWD/Build" make runtime-validation DESTINATION=iphoneos GATE=tcti-init-first-syscall
```

- report:
  - `Build/Reports/runtime/tcti-init-first-syscall-20260702T174136Z-37086.json`.
  - `Build/Reports/runtime/tcti-init-first-syscall-20260702T174136Z-37086.md`.
- result:
  - `destination=iphoneos`.
  - `status=failed`.
  - `passed=false`.
  - `selected_device_name=RRJ-iPhone-15-Pro-Max`.
  - `selected_xcode_device_id=00008130-001E74A11193803A`.
  - `release_gate_eligible=false`.
  - `readiness_gate_eligible=false`.
  - forbidden behavior fields remained false.
- failure summary:
  - The physical iPhone is not ready for Xcode device builds because developer disk image services are unavailable.
  - `xcrun xctrace list devices` lists `RRJ-iPhone-15-Pro-Max (27.0) (00008130-001E74A11193803A)` under `Devices Offline`.

Boundary:

- Simulator pass is side validation only. It does not satisfy the selected `iphoneos` physical gate.
- Physical gate was attempted only through `runtime-validation`.
- No direct device bypass was used.
- No emergency override was used.
- No pass was claimed from the physical report.
- No TCTI runtime code was changed in this checkpoint.
- No custom MCP added.
- No `tools/agent` added.
- No production TCTI assembly added.
- No gadget dispatch added.
- No HostAdapter Linux behavior added.
- No Darwin syscall guest side effect added.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No generated executable memory added.
- No host-executable guest text added.
- No product defconfig flip.
- No reducer was created because the physical gate failed before app install/runtime execution and before any TCTI behavior was exercised.

### Checkpoint: Physical First-Syscall Gate Blocked By Device Readiness

- Harness-selected gate: `physical-tcti-init-first-syscall`.
- Selected command:
  - `make runtime-validation DESTINATION=iphoneos GATE=tcti-init-first-syscall`.
- Harness envelope:
  - `Build/AgentHarness/orlix-tcti/next-task.json`.
  - `physical_device=true`.
  - prerequisites `first-gadget-init-001-exit`, `appstore-safety`, and `report-schema` were `pass`.
- Physical validation was run through the selected `runtime-validation` path, not by a direct device command.

Physical runtime report:

- `Build/Reports/runtime/tcti-init-first-syscall-20260702T173331Z-18841.json`.
- `Build/Reports/runtime/tcti-init-first-syscall-20260702T173331Z-18841.md`.
- `destination=iphoneos`.
- `status=fail`.
- `passed=false`.
- `release_gate_eligible=false`.
- `readiness_gate_eligible=false`.
- selected physical device:
  - `selected_device_name=RRJ-iPhone-15-Pro-Max`.
  - `selected_xcode_device_id=00008130-001E74A11193803A`.
- failure summary:
  - physical iPhone is not ready for Xcode device builds because developer disk image services are unavailable.
  - device must be connected, unlocked, trusted, and have the developer disk image mounted by Xcode before rerunning runtime validation.
- forbidden behavior fields remained false:
  - `generated_exec_memory=false`.
  - `host_exec_guest_text=false`.
  - `host_x18=false`.
  - `map_jit=false`.
  - `native_ios_api_exposure_to_guest=false`.
  - `rwx=false`.

Device inspection:

```text
rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" xcrun xctrace list devices
```

Observed:

- `RRJ-iPhone-15-Pro-Max (27.0) (00008130-001E74A11193803A)` was listed under `Devices Offline`.
- `Orlix-iPhone-15-Pro-Max Simulator (26.5) (C47ED88D-0D0A-420D-8C78-D4C1D34A276D)` remained the only booted simulator.

Boundary:

- No direct physical-device command bypassed runtime-validation.
- No emergency override was used.
- No pass was claimed from this physical report.
- No production TCTI assembly was added.
- No gadget dispatch was added.
- No HostAdapter Linux behavior was added.
- No product defconfig was flipped.
- No reducer was created because the gate failed before app install/runtime execution and before any TCTI behavior was exercised. This is an external device-readiness blocker, not a guest execution failure.

### Checkpoint: Static PIE GOT Byte Load Golden Case

- Operator-scoped no-phone case: `init_011_static_pie_got_byte_load`.
- Added no-libc static PIE source:
  - `OrlixKernel/Tests/TCTI/golden_elf/init_011_static_pie_got_byte_load/init_011_static_pie_got_byte_load.S`.
- Added canonical metadata:
  - `OrlixKernel/Tests/TCTI/golden_elf/init_011_static_pie_got_byte_load/golden.json`.
- Fixture shape:
  - emits `ADRP x8` using the architectural 4 KiB page base.
  - emits GOT relocation form `LDR x8, [x8, #0xd8]`.
  - applies static PIE `R_AARCH64_RELATIVE` relocation in the no-phone oracle at load base zero.
  - emits `LDRB w0, [x8]` from relocated payload byte `42`.
  - exits through Linux `exit(42)`.
- Exact switch-debug instruction words:
  - `0x90000088` `adrp x8, 0x21000`.
  - `0xf9406d08` `ldr x8, [x8, #0xd8]`.
  - `0x39400100` `ldrb w0, [x8]`.
  - `0xd2800ba8` `mov x8, #93`.
  - `0xd4000001` `svc #0`.
- Execution proof:
  - `Build/TCTI/golden_elf/init_011_static_pie_got_byte_load/execution.json`.
  - `guest_instructions_executed=5`.
  - decoded `ADRP` effective address `0x0000000000021000`.
  - decoded `LDR` effective address `0x00000000000210d8`.
  - decoded `LDRB` effective address `0x0000000000001000`.
  - captured syscall `exit(42)`.
- Reducer coverage:
  - `Build/TCTI/reproducers/tcti-golden-elf/init_011_static_pie_got_byte_load-switch-debug-pass-regression.json`.
  - replay report `Build/TCTI/reports/tcti-repro/report.json`.
  - expected `pass`, actual `pass`, replay exit code `0`.
- Harness routing:
  - added `golden-init-011-static-pie-got-byte-load-structural` to `.agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json`.
  - added `switch-init-011-static-pie-got-byte-load` to `.agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json`.
  - made `diff-switch-init-001-exit` depend on `switch-init-011-static-pie-got-byte-load`.
  - added the `init_011` structural and switch-debug pass predicates to `.agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift`.
  - after the `init_011` reports passed, `agent-next` advanced to `physical-tcti-init-first-syscall`.

Verification:

```text
rtk proxy git diff --check
rtk proxy make agent-harness-check
rtk proxy make agent-status AREA=orlix-tcti
rtk proxy make agent-next AREA=orlix-tcti
rtk proxy make agent-task-envelope-check AREA=orlix-tcti
rtk proxy make tcti-gate TARGET=tcti-plan-consistency
rtk proxy make tcti-gate TARGET=tcti-report-schema-check
rtk proxy make tcti-gate TARGET=tcti-toolchain-check
rtk proxy make tcti-gate TARGET=tcti-golden-elf
rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift
rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift
rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" make tcti-gate TARGET=tcti-golden-elf-refresh CASE=init_011_static_pie_got_byte_load
rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" make tcti-gate TARGET=tcti-golden-elf CASE=init_011_static_pie_got_byte_load
rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" make tcti-gate TARGET=tcti-golden-elf CASE=init_011_static_pie_got_byte_load EXECUTE=switch-debug
rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/init_011_static_pie_got_byte_load-switch-debug-pass-regression.json
rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit
rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" ORLIX_BUILD_ROOT="$PWD/Build" make -f OrlixKernel/Makefile kunit PROFILE=development
rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" ORLIX_BUILD_ROOT="$PWD/Build" make -f OrlixKernel/Makefile kunit PROFILE=release
```

Simulator diagnostic:

```text
rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" xcrun simctl list devices booted
rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" xcode-storage-doctor
rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" USER=rudironsoni LOGNAME=rudironsoni ORLIX_BUILD_ROOT="$PWD/Build" ORLIX_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D make runtime-validation DESTINATION=iphonesimulator GATE=tcti-init-first-syscall
```

Simulator report:

- `Build/Reports/runtime/tcti-init-first-syscall-20260702T172340Z-70550.json`.
- `Build/Reports/runtime/tcti-init-first-syscall-20260702T172340Z-70550.md`.
- `git_sha=08d2653939fa50427b38a6f06831e40a5d6f90f5`.
- `status=pass`.
- `passed=true`.
- `destination=iphonesimulator`.
- `selected_device_id=C47ED88D-0D0A-420D-8C78-D4C1D34A276D`.
- `release_gate_eligible=false`.
- `readiness_gate_eligible=false`.
- forbidden behavior fields all false.
- Only `Orlix-iPhone-15-Pro-Max` was booted during the simulator diagnostic.
- The first simulator attempt failed before launch because `xcodegen` could not find `USER`; rerun with `USER=rudironsoni LOGNAME=rudironsoni` passed.

Boundary:

- No physical-device gate run.
- Simulator run was diagnostic only and did not satisfy physical or release readiness.
- No production TCTI assembly added.
- No gadget dispatch added.
- No HostAdapter Linux behavior added.
- No Darwin syscall guest side effect added.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No generated executable memory added.
- No host-executable guest text added.
- No product defconfig flip.
- No checkpoint-scoped `.serena/project.yml` edit; the pre-existing unrelated dirty `.serena/project.yml` file was left unstaged.

### Checkpoint: SIMD/FP Q/D Load-Store Uses Guest SIMD State

- Harness-selected gate remains `physical-tcti-init-first-syscall`.
- Per operator constraint, validation was run only on the `Orlix-iPhone-15-Pro-Max` simulator:
  - UDID `C47ED88D-0D0A-420D-8C78-D4C1D34A276D`.
  - No physical-device gate was run.
- Fixed the TCTI decoded execution path for SIMD/FP load-store instructions emitted by the current `/init` payload:
  - `0xad400901` `ldp q1, q2, [x8]`.
  - `0xad0103e2` `stp q2, q0, [sp, #0x20]`.
  - `0x3dc00100` `ldr q0, [x8]`.
  - `0x3d8007e1` `str q1, [sp, #0x10]`.
  - `0xfd005bea` `str d10, [sp, #0xb0]`.
  - `0x3c9a03a0` `stur q0, [x29, #-0x60]`.
  - `0x3cc383e0` `ldur q0, [sp, #0x38]`.
- Decoder changes:
  - SIMD/FP load-store pair now supports the narrow D and Q pair forms required by `/init`.
  - SIMD/FP unsigned immediate load-store now supports the required D and Q forms.
  - SIMD/FP signed immediate load-store now supports the required D and Q forms.
- Execution changes:
  - SIMD/FP D accesses move 8 bytes through `current->thread.user_simd`.
  - SIMD/FP Q accesses move 16 bytes through `current->thread.user_simd`.
  - D loads clear the upper stored SIMD lane.
  - Q loads/stores preserve both stored SIMD lanes.
  - Pair loads stage both SIMD/FP memory reads before committing register state, so a second-element fault cannot partially mutate guest SIMD state.
- Added KUnit decode coverage for the exact emitted D/Q load-store instruction words above.

Verification:

```text
rtk proxy git diff --check
rtk proxy make agent-harness-check
rtk proxy make agent-task-envelope-check AREA=orlix-tcti
rtk proxy make tcti-gate TARGET=tcti-plan-consistency
rtk proxy make tcti-gate TARGET=tcti-report-schema-check
rtk proxy make tcti-gate TARGET=tcti-toolchain-check
rtk proxy make tcti-gate TARGET=tcti-golden-elf
rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit
rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" ORLIX_BUILD_ROOT="$PWD/Build" make -f OrlixKernel/Makefile kunit PROFILE=release
rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" ORLIX_BUILD_ROOT="$PWD/Build" make -f OrlixKernel/Makefile kunit PROFILE=development
rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" ORLIX_BUILD_ROOT="$PWD/Build" ORLIX_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D make runtime-validation DESTINATION=iphonesimulator GATE=tcti-init-first-syscall
```

Simulator runtime report:

- `Build/Reports/runtime/tcti-init-first-syscall-20260702T141223Z-30088.json`.
- `Build/Reports/runtime/tcti-init-first-syscall-20260702T141223Z-30088.md`.
- `Build/Reports/runtime/tcti-init-first-syscall-20260702T141223Z-30088.artifacts/simulator-unified.log`.

Result:

- The simulator gate still captures the first TCTI syscall marker:
  - `status=pass`.
  - `passed=true`.
  - `destination=iphonesimulator`.
  - `selected_device_id=C47ED88D-0D0A-420D-8C78-D4C1D34A276D`.
- This report remains simulator-only:
  - `release_gate_eligible=false`.
  - `readiness_gate_eligible=false`.
- The later runtime blocker is not fixed:
  - `pc=0x62548a56a1f4`.
  - guest VMA `0x2a1f4`.
  - instruction `0x39400108`, `ldrb w8, [x8]`.
  - fault address `0x97ffb62f39083fff`.
  - panic `Attempted to kill init! exitcode=0x0000000b`.
- Disassembly shows the faulting scalar load follows:
  - `0x2a1e8`: `adrp x8, 0x43000`.
  - `0x2a1f0`: `ldr x8, [x8, #0x360]`.
  - `0x2a1f4`: `ldrb w8, [x8]`.
- The GOT slot at `0x43360` has `R_AARCH64_RELATIVE *ABS*+0x550a8`; the next investigation must determine why the guest value read from that slot is invalid.

Boundary:

- No custom MCP added.
- No `tools/agent` added.
- No production TCTI assembly added.
- No new gadget dispatch implementation added.
- No simulator other than `Orlix-iPhone-15-Pro-Max` used.
- No physical-device gate run.
- No HostAdapter behavior added.
- No Darwin syscall behavior added as a guest side effect.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No generated executable memory added.
- No host-executable guest text added.
- No product defconfig flip.

### Checkpoint: TCTI Syscall Handoff Has Product KUnit Coverage

- Added a narrow TCTI syscall handoff helper:
  - `tcti_prepare_syscall_handoff(struct pt_regs *regs)`.
- The existing runtime path still calls `orlix_syscall_dispatch(regs)` through `orlix_tcti_handle_syscall()`.
- The helper only prepares Linux syscall entry state:
  - `orig_x0 = regs[0]`.
  - `syscallno = regs[8]`.
  - `pc += 4`.
- Added product KUnit coverage:
  - `tcti_syscall_handoff_uses_guest_x8_and_advances_pc`.
  - It proves guest `x8=93`, `x0=42`, and `pc=svc` become Linux `syscallno=93`, `orig_x0=42`, and `pc=svc+4`.
  - It also proves the guest argument registers remain intact before Linux dispatch.
- This advances the real TCTI execution boundary between decoded guest `svc #0` and the existing Linux syscall dispatcher.
- It does not implement new syscall semantics.

Verification:

```text
rtk proxy git diff --check
rtk proxy make tcti-gate TARGET=tcti-plan-consistency
rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit
rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" ORLIX_BUILD_ROOT="$PWD/Build" make -f OrlixKernel/Makefile kunit PROFILE=development
```

Boundary:

- No custom MCP added.
- No `tools/agent` added.
- No production TCTI assembly added.
- No new gadget dispatch implementation added.
- No simulator gate run.
- No physical-device gate run.
- No HostAdapter behavior added.
- No Darwin syscall behavior added as a guest side effect.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No generated executable memory added.
- No host-executable guest text added.
- No product defconfig flip.

### Checkpoint: Runtime Preflight Is Evidence-Only And First Gadget Parity Is In KUnit

- Harness-selected gate remains `physical-tcti-init-first-syscall`.
- Physical runtime execution is still blocked until the iPhone developer disk image service is available.
- Safety review found that `ORLIX_RUNTIME_PREFLIGHT_ONLY=1` could write a physical-gate-shaped report with `status=pass`, `passed=true`, `release_gate_eligible=true`, and `readiness_gate_eligible=true` without installing, launching, or executing the app.
- Fixed `tools/runtime/orlix-runtime-validation.sh` so preflight-only mode now writes:
  - `status=evidence`.
  - `passed=false`.
  - `preflight_only=true`.
  - `release_gate_eligible=false`.
  - `readiness_gate_eligible=false`.
- The preflight command still exits zero when preflight checks pass, but its report cannot satisfy the runtime certification gate.
- Fixed `tools/tcti/orlix-tcti-gate.swift` reducer replay reporting:
  - `make tcti-gate TARGET=tcti-repro` now writes a fresh `Build/TCTI/reports/tcti-repro/report.json`.
  - Expected-fail reducers produce a passing replay report when actual replay status matches expected status.
  - The report records `expected_status`, `actual_replay_status`, and `counters.replay_exit_code`.
  - `tcti-repro` remains ineligible for release/readiness promotion.
- Added product KUnit oracle-parity coverage for the first gadget path:
  - `tcti_gadget_program_matches_switch_debug_init001_movz_prefix`.
  - The test decodes the `init_001_exit` MOVZ prefix.
  - It executes each decoded instruction through `tcti_switch_debug_execute_decoded()`.
  - It lowers and executes the same decoded instruction through `tcti_lower_decoded_instruction()` and `tcti_execute_gadget_program()`.
  - It compares `x0`, `x8`, `sp`, `pc`, and NZCV state after each instruction.
  - Final candidate state is `x0=42`, `x8=93`, `pc=0x210128`.
- This is product-tree test coverage for the first no-phone gadget proof. It is not a physical runtime pass.

Verification:

```text
rtk proxy bash -n tools/runtime/orlix-runtime-validation.sh
rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift
rtk proxy git diff --check
rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-diff-switch/init_001_exit-gadget-x0-divergence.json
rtk proxy jq '{target, status, passed, expected_status, actual_replay_status, counters, failures}' Build/TCTI/reports/tcti-repro/report.json
rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" ORLIX_RUNTIME_PREFLIGHT_ONLY=1 REPORT_DIR=/tmp/orlix-runtime-validation-preflight-selected PROFILE=development DESTINATION=iphoneos GATE=tcti-init-first-syscall make runtime-validation
rtk proxy jq '{status, passed, preflight_only, release_gate_eligible, readiness_gate_eligible, destination, selected_device_id, failure_context, summary}' /tmp/orlix-runtime-validation-preflight-selected/tcti-init-first-syscall-20260702T090717Z-98402.json
rtk proxy make tcti-gate TARGET=tcti-report-schema-check
rtk proxy make tcti-gate TARGET=tcti-diff-switch CASE=init_001_exit BACKEND=gadget
rtk proxy make tcti-gate TARGET=tcti-plan-consistency
rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit
rtk proxy make agent-task-envelope-check AREA=orlix-tcti
rtk proxy make agent-harness-check
rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" make -f OrlixKernel/Makefile kunit PROFILE=development
rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" make -f OrlixKernel/Makefile kunit PROFILE=release
```

Boundary:

- No custom MCP added.
- No `tools/agent` added.
- No production TCTI assembly added.
- No new gadget dispatch implementation added.
- No simulator gate run.
- No physical-device gate run.
- No HostAdapter behavior added.
- No Darwin syscall behavior added as a guest side effect.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No generated executable memory added.
- No host-executable guest text added.
- No product defconfig flip.
- Release and readiness remain ineligible until the physical runtime gate produces a real passing report.

### Checkpoint: Runtime Reports Include Manager Triage Evidence

- Ownership lane: runtime-validation evidence only.
- Changed only `tools/runtime/orlix-runtime-validation.sh`.
- Runtime JSON reports now include:
  - `destination`.
  - `configuration`.
  - `scheme`.
  - `bundle_id`.
  - `profile`.
  - `artifact_dir`.
  - `selected_device_id`.
  - `selected_device_name`.
  - `selected_xcode_device_id`.
  - `failure_context`.
- Runtime Markdown reports now include the same destination/configuration/scheme/bundle/device/artifact directory context in the report header.
- Simulator install failures now record failure context before preserving the existing fail path:
  - stage `simulator-install`.
  - install command exit status.
  - install timeout seconds.
  - whether `install.stdout` is empty.
  - whether `install.stderr` is empty.
- Pass/fail/evidence semantics were not changed.
- No direct device commands were added outside `runtime-validation`.
- No TCTI gate Swift, HostAdapter, kernel runtime, defconfig, generated tree, `.serena`, or MCP files were changed.

Verification:

```text
rtk proxy bash -n tools/runtime/orlix-runtime-validation.sh
rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" ORLIX_RUNTIME_PREFLIGHT_ONLY=1 REPORT_DIR=/tmp/orlix-runtime-validation-preflight PROFILE=development DESTINATION=iphoneos GATE=tcti-init-first-syscall make runtime-validation
rtk proxy jq '{status, passed, destination, configuration, scheme, bundle_id, artifact_dir, selected_device_id, selected_device_name, selected_xcode_device_id, failure_context, profile}' /tmp/orlix-runtime-validation-preflight/tcti-init-first-syscall-20260702T085152Z-71500.json
rtk proxy sed -n '1,40p' /tmp/orlix-runtime-validation-preflight/tcti-init-first-syscall-20260702T085152Z-71500.md
rtk proxy git diff --check -- tools/runtime/orlix-runtime-validation.sh
```

Preflight-only report:

- `/tmp/orlix-runtime-validation-preflight/tcti-init-first-syscall-20260702T085152Z-71500.json`.
- `/tmp/orlix-runtime-validation-preflight/tcti-init-first-syscall-20260702T085152Z-71500.md`.

Boundary:

- This is report evidence plumbing only.
- It does not prove physical TCTI runtime readiness.
- It does not prove simulator install or launch readiness.
- It does not satisfy the physical first-syscall gate.

### Checkpoint: First Gadget Exit Diff Passes And Physical Gate Reaches DDI Blocker

- Harness-selected gate after fresh no-phone validation: `first-gadget-init-001-exit`.
- Selected command:
  - `make tcti-gate TARGET=tcti-diff-switch CASE=init_001_exit BACKEND=gadget`.
- Roadmap correction:
  - Updated `.agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json` so the selected first-gadget gate validates with `rtk proxy make tcti-gate TARGET=tcti-diff-switch CASE=init_001_exit BACKEND=gadget`.
  - The prior validation command `rtk proxy make tcti-gate TARGET=tcti-diff-switch` rewrote `Build/TCTI/diff_switch/init_001_exit/diff.json` back to switch-debug baseline mode, which made the first-gadget pass artifact disappear from harness status.
- Gadget diff report:
  - `Build/TCTI/reports/tcti-diff-switch/report.json`.
  - status `pass`.
  - passed `true`.
  - summary `Diffed init_001_exit gadget data-program candidate against the switch-debug baseline.`
- Gadget diff artifact:
  - `Build/TCTI/diff_switch/init_001_exit/diff.json`.
  - mode `switch-vs-gadget`.
  - reference backend `switch-debug`.
  - candidate backend `gadget-data-program`.
  - gadget dispatch executed `true`.
  - production assembly executed `false`.
  - divergent fields `[]`.
  - checked fields include GPRs, SP, PC, PSTATE/NZCV, TPIDR_EL0, memory writes, exit kind/code, and fault address.
- Negative reducer replay:
  - `make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-diff-switch/init_001_exit-gadget-x0-divergence.json`.
  - Replayed the expected negative gadget x0 divergence.
  - Expected status `fail`.
  - Actual replay status `fail`.
  - Exit code `2`, as expected for the negative target replay.
  - The replay did not refresh `Build/TCTI/reports/tcti-repro/report.json`; that stale report remains a separate harness hygiene issue and must not be counted as green.
- After restoring the passing gadget diff, the harness advanced to:
  - selected gate `physical-tcti-init-first-syscall`.
  - selected command `make runtime-validation DESTINATION=iphoneos GATE=tcti-init-first-syscall`.
  - `physical_device_allowed=true`.
  - `readiness_gate_eligible=false`.
  - `release_gate_eligible=false`.
- Physical runtime command run:
  - `rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" PROFILE=development DESTINATION=iphoneos GATE=tcti-init-first-syscall make runtime-validation`.
- Physical runtime report:
  - `Build/Reports/runtime/tcti-init-first-syscall-20260702T074351Z-21044.json`.
  - `Build/Reports/runtime/tcti-init-first-syscall-20260702T074351Z-21044.md`.
- Physical runtime result:
  - status `fail`.
  - passed `false`.
  - readiness gate eligible `false`.
  - release gate eligible `false`.
  - failure occurred during physical device readiness before kernel build, app build, install, launch, `/init`, TCTI execution, `svc #0`, or `orlix_syscall_dispatch`.
- Device selected by runtime validation:
  - `RRJ-iPhone-15-Pro-Max`.
  - CoreDevice identifier `7F8A1701-D612-5A9C-AAE7-8FD0AD77306C`.
- Blocker:
  - Developer disk image services are unavailable for the physical iPhone.
  - Runtime validation instructs the operator to connect, unlock, trust the device, and let Xcode mount the developer disk image before rerunning.

Boundary:

- First no-phone gadget diff for `init_001_exit` now passes against switch-debug.
- Physical TCTI gate still does not pass.
- No `/init`, physical `svc #0`, `orlix_syscall_dispatch`, Linux console, or HostAdapter console mirror evidence was captured.
- No production TCTI assembly added.
- No broad gadget dispatch added beyond the bounded no-phone `init_001_exit` data-program diff proof.
- No simulator success claimed.
- No physical success claimed.
- No HostAdapter behavior added.
- No Darwin syscall behavior added as a guest side effect.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No generated executable memory added.
- No host-executable guest text added.
- No product defconfig flip.
- No custom MCP added.
- No `tools/agent` added.

### Checkpoint: Simulator First-Syscall Gate Fails Before Launch During Install

- User-directed diagnostic: test the TCTI first-syscall gate on the simulator before retrying the physical iPhone gate.
- Harness-selected gate remains `physical-tcti-init-first-syscall`.
- Physical selected command remains `make runtime-validation DESTINATION=iphoneos GATE=tcti-init-first-syscall`.
- Simulator diagnostic command run:
  - `rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" HOMEBREW_NO_AUTO_UPDATE=1 ORLIX_SIMULATOR_ID=C60BB34E-99F0-401E-AACF-ED599B7F2AC3 PROFILE=development DESTINATION=iphonesimulator GATE=tcti-init-first-syscall REPORT_DIR=Build/Reports/runtime-simulator make runtime-validation`.
- Simulator selected:
  - `Orlix-iPhone-15-Pro-Max`.
  - UDID `C60BB34E-99F0-401E-AACF-ED599B7F2AC3`.
  - Runtime `iOS 26.5`.
  - State `Booted`.
- Runtime report:
  - `Build/Reports/runtime-simulator/tcti-init-first-syscall-20260702T070728Z-48274.json`.
  - `Build/Reports/runtime-simulator/tcti-init-first-syscall-20260702T070728Z-48274.md`.
- Result:
  - status `fail`.
  - passed `false`.
  - readiness gate eligible `false`.
  - release gate eligible `false`.
  - failure occurred during simulator install before launch, `/init`, TCTI execution, `svc #0`, marker capture, or `orlix_syscall_dispatch`.
- Artifact evidence:
  - kernel archive built far enough for the script to produce `kernel-build.log`.
  - Xcode project generation and app build produced `xcodegen.log`, `xcodebuild.log`, `build-settings.json`, and `app-path.txt`.
  - simulator boot was already complete and `bootstatus` was skipped as booted.
  - `install.stdout` and `install.stderr` were empty at failure.
  - no `launch-console.log`, `launch.stderr`, or `tcti-first-syscall.txt` was produced.
- Environment preflight:
  - `xcode-storage-doctor` reported the CoreSimulator device sparsebundle mounted correctly.
  - `xcode-storage-doctor` also reported `CoreSimulator Caches is not mounted at /Library/Developer/CoreSimulator/Caches`.
  - The previously documented `ExternalSSDProof` UDID `4B85E297-7A59-45BD-A94B-189238EC48FA` was not a valid simulator on this machine at the time of the run.

Boundary:

- This simulator result does not satisfy the physical `tcti-init-first-syscall` gate.
- This simulator result does not advance release or readiness eligibility.
- No TCTI runtime behavior was reached.
- No no-phone reducer was added because the failure happened before app/TCTI execution and before a TCTI behavioral signal.
- No production TCTI assembly added.
- No gadget dispatch added.
- No physical device command was run for this checkpoint.
- No HostAdapter behavior added.
- No Darwin syscall behavior added as a guest side effect.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No generated executable memory added.
- No host-executable guest text added.
- No product defconfig flip.
- No custom MCP added.
- No `tools/agent` added.

### Checkpoint: Physical First-Syscall Gate Still Blocked By Device DDI Readiness After Safety Fix

- Harness-selected gate: `physical-tcti-init-first-syscall`.
- Selected command: `make runtime-validation DESTINATION=iphoneos GATE=tcti-init-first-syscall`.
- Why selected: after `c3f870bc57646e5e72fbbc9078b96508ab8a30de`, all no-phone and App Store safety prerequisites were passing and `agent-status` reported `physical_device_allowed=true`.
- Runtime report:
  - `Build/Reports/runtime/tcti-init-first-syscall-20260702T053620Z-98123.json`.
  - `Build/Reports/runtime/tcti-init-first-syscall-20260702T053620Z-98123.md`.
- Result:
  - status `fail`.
  - passed `false`.
  - readiness gate eligible `false`.
  - release gate eligible `false`.
  - failure occurred during physical device readiness before kernel build, app build, install, launch, `/init`, TCTI execution, `svc #0`, or `orlix_syscall_dispatch`.
- Device selected by runtime validation:
  - `RRJ-iPhone-15-Pro-Max`.
  - CoreDevice identifier `7F8A1701-D612-5A9C-AAE7-8FD0AD77306C`.
- Blocker:
  - Developer disk image services are unavailable for the physical iPhone.
  - The runtime report instructs the operator to connect, unlock, trust the device, and let Xcode mount the developer disk image before rerunning runtime validation.
- Report forbidden behavior fields stayed false:
  - `host_exec_guest_text=false`.
  - `native_ios_api_exposure_to_guest=false`.
  - `map_jit=false`.
  - `rwx=false`.
  - `generated_exec_memory=false`.
  - `host_x18=false`.

Boundary:

- Physical TCTI gate did not pass.
- No app or TCTI runtime evidence was captured.
- No no-phone reducer was added because the failure happened before app/TCTI execution and is device readiness, not TCTI behavior.
- No production TCTI assembly added.
- No gadget dispatch added.
- No simulator gate run.
- No direct device command outside `runtime-validation`.
- No HostAdapter behavior added.
- No Darwin syscall behavior added as a guest side effect.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No generated executable memory added.
- No host-executable guest text added.
- No product defconfig flip.
- No custom MCP added.
- No `tools/agent` added.

### Checkpoint: HostAdapter Native Path Quarantined From App Store Safety Gate

- Harness-selected gate: `appstore-safety`.
- Selected command: `make tcti-gate TARGET=tcti-appstore-safety-audit`.
- Why selected: the prior safety audit correctly failed on HostAdapter native hosted-exec behavior after the audit expanded to HostAdapter memory/runtime product sources.
- Parallel review lanes:
  - `tcti-planner` confirmed the original scanner-only envelope could not remediate the product failure.
  - `tcti-safety-reviewer` confirmed the failure was real and blocking, not evidence-only.
  - `tcti-test-reducer` found the safety gate needed replayable reducer artifacts.
- Updated the skill-owned roadmap for `appstore-safety` so the envelope authorizes bounded product-boundary remediation:
  - HostAdapter memory/runtime quarantine work.
  - `hosted_exec.c`, Kconfig/config policy, `project.yml`, audit tooling, and `IMPLEMENT.md`.
  - Explicitly forbids scanner ignorelists, product TCTI defconfig flips, moving Linux semantics into HostAdapter, generated-tree edits, simulator gates, physical gates, production assembly, and new gadget dispatch.
- Quarantined the native HostAdapter user-execution path:
  - Removed HostAdapter-side guest Linux syscall/TLS instruction translation from `kernel_mapping.c`.
  - Made HostAdapter guest user page mapping and refresh fail closed when the input page is executable.
  - Left the trusted syscall-gate executable helper unchanged; this checkpoint removes host-executable guest pages, not trusted kernel-owned trampoline mechanics.
  - Removed HostAdapter runtime classification of Linux syscall and TLS write instruction traps.
  - HostAdapter still handles private host trap mechanics and memory faults, but no longer owns Linux syscall/TLS instruction semantics for product safety.
- Strengthened `tcti-appstore-safety-audit` reducer evidence:
  - Passing audits now write `Build/TCTI/reproducers/tcti-appstore-safety-audit/appstore-safety-pass-regression.json`.
  - Failing audits now write one reducer per true `forbidden_behavior` family.
  - The safety report artifacts array includes generated reducer paths.

Reports:

- `Build/TCTI/reports/tcti-appstore-safety-audit/report.json`.
- `Build/AgentHarness/orlix-tcti/status.json`.
- `Build/AgentHarness/orlix-tcti/next-task.json`.
- `Build/AgentHarness/orlix-tcti/next-task.md`.

Reducer:

- `Build/TCTI/reproducers/tcti-appstore-safety-audit/appstore-safety-pass-regression.json`.
- Replay result: `make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-appstore-safety-audit/appstore-safety-pass-regression.json` produced expected status `pass`, actual status `pass`, exit code `0`.

Fresh safety report state:

- git SHA `b84ca4a4e0e6aa452657d472c7fc535e4b79ca34`.
- status `pass`.
- passed `true`.
- release/readiness eligible `true`.
- `forbidden_behavior.host_exec_guest_text=false`.
- `forbidden_behavior.native_ios_api_exposure_to_guest=false`.
- `forbidden_behavior.map_jit=false`.
- `forbidden_behavior.rwx=false`.
- `forbidden_behavior.generated_exec_memory=false`.
- `forbidden_behavior.host_x18=false`.

Harness result after checkpoint:

- `agent-status` reports `physical_device_allowed=true`.
- `agent-next` selects `physical-tcti-init-first-syscall`.
- `agent-task-envelope-check` passes for `physical-tcti-init-first-syscall`.
- Release/readiness remain false because the physical runtime report is still missing.

Verification:

```text
rtk proxy git diff --check
rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift
rtk proxy make agent-harness-check
rtk proxy make tcti-gate TARGET=tcti-plan-consistency
rtk proxy make tcti-gate TARGET=tcti-report-schema-check
rtk proxy make tcti-gate TARGET=tcti-toolchain-check
rtk proxy make tcti-gate TARGET=tcti-golden-elf
rtk proxy make tcti-gate TARGET=tcti-golden-elf CASE=init_001_exit EXECUTE=switch-debug
rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit
rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-appstore-safety-audit/appstore-safety-pass-regression.json
rtk proxy make agent-status AREA=orlix-tcti
rtk proxy make agent-next AREA=orlix-tcti
rtk proxy make agent-task-envelope-check AREA=orlix-tcti
rtk proxy clang -fsyntax-only -DORLIX_APP_HOSTED_BOOT=1 -IOrlixHostAdapter/Sources -IOrlixKernel/Sources/ports/orlix/overlay/arch/orlix/include OrlixHostAdapter/Sources/OrlixHostAdapter/memory/kernel_mapping.c
rtk proxy clang -fsyntax-only -Wno-implicit-function-declaration -DORLIX_APP_HOSTED_BOOT=1 -IOrlixHostAdapter/Sources -IOrlixKernel/Sources/ports/orlix/overlay/arch/orlix/include OrlixHostAdapter/Sources/OrlixHostAdapter/runtime/trap.c
rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" make -f OrlixKernel/Makefile kunit PROFILE=development
rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" make -f OrlixKernel/Makefile kunit PROFILE=release
```

Validation note:

- A direct generic iOS `xcodebuild` build was not run because the TCTI pre-tool hook blocks direct device-oriented `xcodebuild` invocations outside `runtime-validation` or explicit evidence-mode preflight.
- Local syntax checks covered the two touched HostAdapter C files.

Boundary:

- No custom MCP added.
- No `tools/agent` added.
- No production TCTI assembly added.
- No new gadget dispatch added.
- No simulator gate run.
- No physical-device gate run in this checkpoint.
- No HostAdapter Linux syscall/TLS/VFS/fd/process/signal/scheduler semantics added.
- No Darwin syscall behavior added as a guest side effect.
- No generated executable memory added.
- No host-executable guest text added.
- No product defconfig flip.
- Native hosted execution is intentionally quarantined from product safety; the next harness-selected gate is the physical first-syscall runtime certification.

### Checkpoint: App Store Safety Audit Covers HostAdapter Native Path

- Harness-selected work initially advanced through `first-gadget-init-001-exit` after stale no-phone reports were refreshed at `54d02b769a1a568e35ce1bafa0531da4bed7cb5f`.
- Refreshed no-phone reports:
  - `Build/TCTI/reports/tcti-diff-switch/report.json`.
  - `Build/TCTI/reports/tcti-contract/report.json`.
  - `Build/TCTI/reports/tcti-memory-fuzz/report.json`.
  - `Build/TCTI/reports/tcti-direct-chain-fuzz/report.json`.
- Executed harness-selected no-phone gadget gate:
  - `make tcti-gate TARGET=tcti-diff-switch CASE=init_001_exit BACKEND=gadget`.
  - `Build/TCTI/reports/tcti-diff-switch/report.json`.
  - `Build/TCTI/diff_switch/init_001_exit/diff.json`.
- The first gadget diff now proves:
  - mode `switch-vs-gadget`.
  - candidate backend `gadget-data-program`.
  - `gadget_dispatch_executed=true`.
  - `production_assembly_executed=false`.
  - divergent fields empty.
  - `x0=42`, `x8=93`, `pc=0x0000000000210128`, and `exit_code=42` match the switch-debug baseline.
- Safety review found the App Store safety audit was too narrow because it scanned TCTI kernel sources but did not scan HostAdapter memory/runtime product sources.
- Updated `tools/tcti/orlix-tcti-gate.swift` so `tcti-appstore-safety-audit` also scans HostAdapter memory and runtime source directories.
- Added scanner patterns for:
  - host-executable hosted guest user page mappings.
  - HostAdapter-owned Linux syscall and TLS trap semantics.
- Current safety report:
  - `Build/TCTI/reports/tcti-appstore-safety-audit/report.json`.
  - status `fail`.
  - passed `false`.
  - `forbidden_behavior.host_exec_guest_text=true`.
  - `forbidden_behavior.native_ios_api_exposure_to_guest=true`.
  - scanned 31 source/template files and 1 object file.
- Representative failures now report the existing native hosted-exec path:
  - HostAdapter memory code still translates Linux syscall instructions and grants host executable permissions to hosted guest user pages.
  - HostAdapter runtime trap code still classifies and dispatches Linux syscall and TLS traps.
- Harness result after the audit correction:
  - `agent-status` reports `physical_device_allowed=false`.
  - `agent-next` selects `appstore-safety`.
  - `agent-task-envelope-check` passes for `appstore-safety`.
- Boundary:
  - No custom MCP added.
  - No `tools/agent` added.
  - No production TCTI assembly added.
  - No gadget dispatch beyond the existing no-phone data-program candidate for `init_001_exit`.
  - No simulator gate run.
  - No physical-device gate accepted as passing.
  - No HostAdapter behavior changed.
  - No Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
  - No product defconfig flip.

### Checkpoint: Current-Head Physical Gate Still Blocked By Device DDI Readiness

- Harness-selected gate remained `physical-tcti-init-first-syscall`.
- Selected command remained `make runtime-validation DESTINATION=iphoneos GATE=tcti-init-first-syscall`.
- Parallel agent lanes used for this checkpoint:
  - `tcti-planner` reviewed the selected envelope and confirmed no no-phone gate remains eligible before the physical gate.
  - `tcti-safety-reviewer` found stale no-phone report evidence, so the no-phone reports were refreshed at current `HEAD` before rerunning the physical gate.
  - `tcti-test-reducer` reviewed the latest physical failure and confirmed no no-phone reducer is appropriate because the run failed before app or TCTI execution.
- Refreshed current-head no-phone reports:
  - `Build/TCTI/reports/tcti-plan-consistency/report.json`.
  - `Build/TCTI/reports/tcti-report-schema-check/report.json`.
  - `Build/TCTI/reports/tcti-toolchain-check/report.json`.
  - `Build/TCTI/reports/tcti-golden-elf/report.json`.
  - `Build/TCTI/reports/tcti-appstore-safety-audit/report.json`.
- Each refreshed report recorded git SHA `2e3d112dbab744aa2fd51d5744d2847d1e81f190` and `status=pass`.
- Agent harness after refresh still selected `physical-tcti-init-first-syscall`:
  - `Build/AgentHarness/orlix-tcti/status.json`.
  - `Build/AgentHarness/orlix-tcti/next-task.json`.
  - `Build/AgentHarness/orlix-tcti/next-task.md`.
- Current selected physical gate retry:
  - `Build/Reports/runtime/tcti-init-first-syscall-20260702T021252Z-61011.md`.
  - `Build/Reports/runtime/tcti-init-first-syscall-20260702T021252Z-61011.json`.
- Result:
  - status `fail`.
  - passed `false`.
  - readiness gate eligible `false`.
  - release gate eligible `false`.
  - failure occurred during physical device discovery/readiness before kernel build, app build, install, launch, `/init`, TCTI execution, `svc #0`, or `orlix_syscall_dispatch`.
- Blocker remained physical iPhone developer disk image readiness:
  - device `RRJ-iPhone-15-Pro-Max`.
  - CoreDevice identifier `7F8A1701-D612-5A9C-AAE7-8FD0AD77306C`.
  - summary says developer disk image services unavailable.
- Boundary:
  - No custom MCP added.
  - No `tools/agent` added.
  - No production TCTI assembly added.
  - No gadget dispatch added.
  - No simulator gate run.
  - No direct physical-device command outside `runtime-validation`.
  - No HostAdapter behavior added.
  - No Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
  - No product defconfig flip.

### Checkpoint: Safety Harness Physical-Report Review Tightening

- Harness-selected gate remained `physical-tcti-init-first-syscall`.
- Latest selected physical gate retry:
  - `Build/Reports/runtime/tcti-init-first-syscall-20260702T015016Z-22911.md`.
  - `Build/Reports/runtime/tcti-init-first-syscall-20260702T015016Z-22911.json`.
  - status `fail`.
  - passed `false`.
  - failure still occurred during physical device discovery/readiness before kernel build, app build, install, launch, `/init`, or TCTI execution.
  - blocker remained `ddiServicesAvailable=false` for `RRJ-iPhone-15-Pro-Max`.
- Safety review found the previous report-read hook exception could return before later device-policy checks on mixed payloads.
- Updated `.agents/skills/orlix-tcti-safety/scripts/pre-tool-use-policy` so:
  - report-only runtime artifact reads remain allowed.
  - `IMPLEMENT.md` log updates remain allowed.
  - mixed payloads that include device-oriented commands still reach the physical-device policy checks.
- Safety review also found non-x18 App Store forbidden-behavior fields were defaulted rather than scanner-backed.
- Updated `tools/tcti/orlix-tcti-gate.swift` App Store safety audit so production TCTI sources are scanned for:
  - `MAP_JIT`.
  - generated executable memory requests.
  - RWX permission requests.
  - host executable guest-text requests.
  - native iOS or HostAdapter API exposure to guest Linux.
- Added negative scanner fixture:
  - `tools/tcti/fixtures/appstore_safety/forbidden_exec.c`.
- Refreshed stale autonomous reports at current `HEAD`:
  - `Build/TCTI/reports/tcti-contract/report.json`.
  - `Build/TCTI/reports/tcti-diff-switch/report.json`.
  - `Build/TCTI/reports/tcti-memory-fuzz/report.json`.
  - `Build/TCTI/reports/tcti-direct-chain-fuzz/report.json`.
  - `Build/TCTI/reports/tcti-repro/report.json`.

Boundary:

- No TCTI physical runtime evidence was captured.
- No no-phone reducer was added for the DDI-readiness failure because it is not TCTI behavior.
- No production TCTI assembly was added.
- No gadget dispatch was added.
- No simulator gate was run.
- No HostAdapter behavior was added.
- No Darwin syscall behavior was added.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics were added.
- No generated executable memory was added.
- No host-executable guest text was added.
- No product defconfig was flipped.
- No custom MCP was added.
- No `tools/agent` directory was added.

### Checkpoint: Physical First-Syscall Gate Attempt Blocked By DDI Readiness

- Harness-selected gate attempted: `physical-tcti-init-first-syscall`.
- Selected command: `make runtime-validation DESTINATION=iphoneos GATE=tcti-init-first-syscall`.
- Actual command included previously recorded development team:
  - `ORLIX_DEVELOPMENT_TEAM=ZQ3L7M567L`.
- Runtime reports:
  - `Build/Reports/runtime/tcti-init-first-syscall-20260702T011640Z-58655.md`.
  - `Build/Reports/runtime/tcti-init-first-syscall-20260702T011640Z-58655.json`.
  - `Build/Reports/runtime/tcti-init-first-syscall-20260702T012504Z-74816.md`.
  - `Build/Reports/runtime/tcti-init-first-syscall-20260702T012504Z-74816.json`.
  - `Build/Reports/runtime/tcti-init-first-syscall-20260702T013920Z-99581.md`.
  - `Build/Reports/runtime/tcti-init-first-syscall-20260702T013920Z-99581.json`.
- Result:
  - status `fail`.
  - passed `false`.
  - readiness gate eligible `false`.
  - release gate eligible `false`.
  - failure occurred during physical device discovery/readiness before kernel build, app build, install, launch, `/init`, or TCTI execution.
  - latest retry report was generated at git SHA `f29405e13d53198b156ee711d3fdeb84842f9b1c` with the same DDI-readiness classification.
- Device selected by runtime harness:
  - `RRJ-iPhone-15-Pro-Max`.
  - device identifier `7F8A1701-D612-5A9C-AAE7-8FD0AD77306C`.
  - hardware UDID `00008130-001E74A11193803A`.
  - `developerModeStatus=enabled`.
  - `ddiServicesAvailable=false`.
- Blocker:
  - Xcode developer disk image services are unavailable for the physical iPhone.
  - The report directs the operator to connect, unlock, trust the device, and let Xcode mount the developer disk image before rerunning runtime validation.
- Parallel review:
  - Planner confirmed the harness selection is legitimate but not a runtime pass.
  - Safety review found product defconfigs and App Store safety rails still clean, with the physical result classified evidence-only.
  - Reducer review classified the failure as device readiness, not TCTI behavior. No no-phone reducer is appropriate unless a later run reaches app/TCTI execution and fails there.
  - Follow-up reducer/readiness review confirmed the `20260702T013920Z-99581` report still does not advance readiness or require a no-phone reducer.

Boundary:

- No TCTI physical runtime evidence was captured.
- No kernel build, app build, install, launch, or `/init` execution happened in the failed attempts.
- No phone log was used to patch production behavior.
- No no-phone reducer was added because this is device readiness, not TCTI behavior.
- No production TCTI assembly was added.
- No gadget dispatch was added.
- No HostAdapter behavior was added.
- No Darwin syscall behavior was added.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics were added.
- No generated executable memory was added.
- No host-executable guest text was added.
- No product defconfig was flipped.
- No custom MCP was added.
- No `tools/agent` directory was added.

### Checkpoint: CPU Model Switch-Debug Execution Gate

- Harness-selected gate implemented: `switch-init-010-cpu-model`.
- Selected command: `make tcti-gate TARGET=tcti-golden-elf CASE=init_010_cpu_model EXECUTE=switch-debug`.
- The switch-debug harness now executes `init_010_cpu_model` and captures:
  - fixed virtual CPU model payload `orlix-aarch64-v1\n` from file-backed PT_LOAD bytes via decoded `ADR x1`.
  - `exit(0)` as a captured test-harness syscall event.
- Exact decoded instruction stream:
  - `0x10000081` `adr x1, 0x210130 <cpu_model>`.
  - `0xd2800000` `mov x0, #0`.
  - `0xd2800ba8` `mov x8, #93`.
  - `0xd4000001` `svc #0`.
- Execution artifacts:
  - `Build/TCTI/reports/tcti-golden-elf/report.json`.
  - `Build/TCTI/golden_elf/init_010_cpu_model/execution.json`.
- Added negative fixtures:
  - `tools/tcti/fixtures/golden_elf/init_010_cpu_model_wrong_model.S`.
  - `tools/tcti/fixtures/golden_elf/init_010_cpu_model_unsupported_ctr_el0.S`.
- Generated reducers:
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-cpu-model-wrong-model.json`.
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-cpu-model-unsupported-ctr-el0.json`.
- Replayed reducer:
  - `make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-cpu-model-unsupported-ctr-el0.json`.
  - expected status `fail`.
  - actual replay status `fail`.
  - actual replay exit code `2`.

Boundary:

- No host CPU feature value was exposed to guest policy.
- No `CTR_EL0`, `DCZID_EL0`, auxv, `/proc/cpuinfo`, `AT_HWCAP`, or `AT_HWCAP2` implementation was added.
- No production TCTI assembly.
- No gadget dispatch.
- No simulator gate.
- No physical-device gate.
- No HostAdapter behavior.
- No Darwin syscall behavior.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No generated executable memory.
- No host-executable guest text.
- No product defconfig flip.
- No custom MCP.
- No `tools/agent`.

### Checkpoint: CPU Model Golden ELF Structural Gate

- Harness-selected gate implemented: `golden-init-010-cpu-model-structural`.
- Selected command: `make tcti-gate TARGET=tcti-golden-elf CASE=init_010_cpu_model`.
- Added no-libc AArch64 Linux source:
  - `OrlixKernel/Tests/TCTI/golden_elf/init_010_cpu_model/init_010_cpu_model.S`.
- Added canonical metadata:
  - `OrlixKernel/Tests/TCTI/golden_elf/init_010_cpu_model/golden.json`.
- Added structural validation wiring in:
  - `tools/tcti/orlix-tcti-gate.swift`.
- Structural fixture shape:
  - `adr x1, cpu_model`.
  - `mov x0, #0`.
  - `mov x8, #93`.
  - `svc #0`.
  - `cpu_model: .ascii "orlix-aarch64-v1\n"`.
- Exact emitted instruction words:
  - `0x10000081` `adr x1, 0x210130 <cpu_model>`.
  - `0xd2800000` `mov x0, #0`.
  - `0xd2800ba8` `mov x8, #93`.
  - `0xd4000001` `svc #0`.
- Metadata hashes:
  - source SHA256 `745e1961f862ad6f3188fcf9376e42bc5d4dfd5e47dfd8f06a107403f66dc7c6`.
  - binary SHA256 `c92ceaccd651fa209b16975c48049bf97b085da5578f06da3eed75ef16bd8621`.
- Validation artifacts:
  - `Build/TCTI/reports/tcti-golden-elf/report.json`.
  - `Build/TCTI/golden_elf/init_010_cpu_model/validation.json`.
- The structural gate passed.

Boundary:

- Structural-only gate. No `init_010_cpu_model` execution was claimed.
- No switch execution was implemented.
- No `CTR_EL0`, `DCZID_EL0`, auxv, or `/proc/cpuinfo` execution semantics were added.
- No production TCTI assembly.
- No gadget dispatch.
- No simulator gate.
- No physical-device gate.
- No HostAdapter behavior.
- No Darwin syscall behavior.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No generated executable memory.
- No host-executable guest text.
- No product defconfig flip.
- No custom MCP.
- No `tools/agent`.

## 2026-07-01

### Checkpoint: Memory Golden ELF Structural Gate

- Harness-selected gate implemented: `golden-init-006-memory-structural`.
- Selected command: `make tcti-gate TARGET=tcti-golden-elf CASE=init_006_memory`.
- Added no-libc AArch64 Linux source:
  - `OrlixKernel/Tests/TCTI/golden_elf/init_006_memory/init_006_memory.S`.
- Added canonical metadata:
  - `OrlixKernel/Tests/TCTI/golden_elf/init_006_memory/golden.json`.
- Structural fixture shape:
  - `adr x1, value`.
  - `ldr x0, [x1]`.
  - `mov x8, #93`.
  - `svc #0`.
  - `value: .quad 42`.
- Exact emitted instruction words:
  - `0x10000081` `adr x1, 0x210130 <value>`.
  - `0xf9400020` `ldr x0, [x1]`.
  - `0xd2800ba8` `mov x8, #93`.
  - `0xd4000001` `svc #0`.
- Exact embedded value bytes:
  - `2a 00 00 00 00 00 00 00`.
- Metadata hashes:
  - source SHA256 `3289121a29348f883200a5f6cb82085b4dec0c4ddf3d60452c935188c0591a56`.
  - binary SHA256 `ffda09c29871d3ea5546f1c651bfd7256a4d9f8c92f5ab89f3159321168c75fa`.
- Validation artifacts:
  - `Build/TCTI/reports/tcti-golden-elf/report.json`.
  - `Build/TCTI/golden_elf/init_006_memory/validation.json`.
- The structural gate passed.
- Harness result after checkpoint:
  - `agent-status` reports `golden-init-006-memory-structural` as pass.
  - `agent-next` selects `switch-init-006-memory`.
  - `physical_device_allowed=false`.

Boundary:

- Structural-only gate. No `init_006_memory` execution was claimed.
- No production TCTI assembly.
- No gadget dispatch.
- No simulator gate.
- No physical-device gate.
- No HostAdapter behavior.
- No Darwin syscall behavior.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No generated executable memory.
- No host-executable guest text.
- No product defconfig flip.
- No custom MCP.
- No `tools/agent`.

### Checkpoint: Memory Switch-Debug Execution Gate

- Harness-selected gate implemented: `switch-init-006-memory`.
- Selected command: `make tcti-gate TARGET=tcti-golden-elf CASE=init_006_memory EXECUTE=switch-debug`.
- Why selected: `agent-next` selected the first non-passing roadmap gate after `golden-init-006-memory-structural` passed.
- Added narrow decoded switch-debug semantics for the existing `init_006_memory` fixture:
  - `ADR x1, value` computes `x1 = pc + imm`.
  - `LDR x0, [x1]` reads one 64-bit little-endian value from file-backed `PT_LOAD` bytes only.
  - existing `MOVZ x8, #93` sets the Linux exit syscall number.
  - existing `SVC #0` captures Linux `exit(42)` as a test-harness event.
- Exact decoded execution:
  - `0x10000081` `adr x1, 0x210130 <value>`.
  - `0xf9400020` `ldr x0, [x1]`, effective address `0x0000000000210130`.
  - `0xd2800ba8` `mov x8, #93`.
  - `0xd4000001` `svc #0`.
- Execution report:
  - `Build/TCTI/golden_elf/init_006_memory/execution.json`.
- Captured syscall:
  - Linux `exit(42)`, captured by the switch-debug harness.
  - Host `exit` was not called.
  - Darwin syscalls were not called as guest side effects.
- Negative execution reducers:
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-memory-invalid-read.json`.
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-memory-unsupported-store.json`.
- Replayed reducer:
  - `make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-memory-invalid-read.json` produced expected `fail`, actual `fail`, replay exit code `2`.

Boundary:

- No production TCTI assembly.
- No gadget dispatch.
- No simulator gate.
- No physical-device gate.
- No HostAdapter behavior.
- No Darwin syscall behavior.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No generated executable memory.
- No host-executable guest text.
- No product defconfig flip.
- No custom MCP.
- No `tools/agent`.

### Checkpoint: Branch Switch-Debug Execution And Harness No-Phone Roadmap Guard

- Harness-selected gate implemented: `switch-init-005-branches`.
- Selected command: `make tcti-gate TARGET=tcti-golden-elf CASE=init_005_branches EXECUTE=switch-debug`.
- Added decoded switch-debug branch support in `tools/tcti/orlix-tcti-gate.swift`:
  - compare-and-branch immediate decode for 64-bit `CBZ` only.
  - unconditional branch immediate decode for `B`.
  - branch PC update semantics for the branch fixture path.
- Decoder predicates:
  - `CBZ/CBNZ`: `(raw & 0x7e00_0000) == 0x3400_0000`.
  - `B`: `(raw & 0xfc00_0000) == 0x1400_0000`.
- Scope kept deliberately narrow:
  - `CBNZ` is rejected as unsupported.
  - W-register compare-and-branch variants are rejected as unsupported.
  - no `BL`, `BR`, `RET`, conditional branch, `TBZ/TBNZ`, or broad branch model was added.
- Added negative execution fixture:
  - `tools/tcti/fixtures/golden_elf/init_005_branches_unsupported_cbnz.S`.
- Reducer:
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-branches-unsupported-cbnz.json`.
- Reducer replay:
  - `make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-branches-unsupported-cbnz.json` produced expected `fail`, actual `fail`, replay exit code `2`.
- Execution report:
  - `Build/TCTI/golden_elf/init_005_branches/execution.json`.
- Captured execution:
  - backend `switch-debug`.
  - case `init_005_branches`.
  - entered entrypoint.
  - executed 5 guest instructions through the taken `CBZ` path.
  - captured Linux `exit(42)` as a test-harness syscall event.
- Exact taken-path instruction words:
  - `0xd2800000` `mov x0, #0`.
  - `0xb4000060` `cbz x0, 0x210130`.
  - `0xd2800540` `mov x0, #42`.
  - `0xd2800ba8` `mov x8, #93`.
  - `0xd4000001` `svc #0`.
- Corrected the next-step harness after worker review found over-advance to physical work:
  - added roadmap gates through `init_010_cpu_model` before diff, gadget, or physical work.
  - moved `diff-switch-init-001-exit` behind `switch-init-010-cpu-model`.
  - made JSON status writes atomic without remove-and-move races under parallel harness calls.
  - made physical selection require all no-phone gates before the first physical gate to pass.
- Harness result after correction:
  - `agent-status` reports `physical_device_allowed=false`.
  - `agent-next` selects `golden-init-006-memory-structural`.
  - `agent-task-envelope-check` passes for `golden-init-006-memory-structural`.

Boundary:

- No custom MCP added.
- No `tools/agent` added.
- No production TCTI assembly.
- No gadget dispatch.
- No simulator gate run.
- No physical-device gate run.
- No HostAdapter behavior.
- No Darwin syscall behavior.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No generated executable memory.
- No host-executable guest text.
- No product defconfig flip.
- Full TCTI remains incomplete.

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
  - `make tcti-gate TARGET=tcti-contract`
  - `make tcti-gate TARGET=tcti-golden-elf`
  - `make tcti-gate TARGET=tcti-diff-switch`
  - `make tcti-gate TARGET=tcti-memory-fuzz`
  - `make tcti-gate TARGET=tcti-direct-chain-fuzz`
  - `make tcti-gate TARGET=tcti-appstore-safety-audit`
  - `make tcti-gate TARGET=tcti-report-schema-check`
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
  - `make tcti-gate TARGET=tcti-plan-consistency`
  - `make tcti-gate TARGET=tcti-report-schema-check`
  - `make tcti-gate TARGET=tcti-toolchain-check`
  - `make tcti-gate TARGET=tcti-contract`
  - `make tcti-gate TARGET=tcti-golden-elf`
  - `make tcti-gate TARGET=tcti-golden-elf-refresh`
  - `make tcti-gate TARGET=tcti-diff-switch`
  - `make tcti-gate TARGET=tcti-memory-fuzz`
  - `make tcti-gate TARGET=tcti-direct-chain-fuzz`
  - `make tcti-gate TARGET=tcti-appstore-safety-audit`
  - `make tcti-gate TARGET=tcti-repro`
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
rtk proxy make tcti-gate TARGET=tcti-plan-consistency
rtk proxy make tcti-gate TARGET=tcti-report-schema-check
rtk proxy make tcti-gate TARGET=tcti-toolchain-check
rtk proxy make tcti-gate TARGET=tcti-golden-elf
rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit
```

### Checkpoint: No-Phone Memory Fuzz Gate

- Harness-selected gate: `tcti-memory-fuzz`.
- Selected command: `make tcti-gate TARGET=tcti-memory-fuzz`.
- Why selected: `agent-status` reported `tcti-contract=pass` and selected `tcti-memory-fuzz` as the next eligible runtime-preflight gate. Physical device, release, and readiness gates remained ineligible.
- Implemented the gate as a no-phone Swift/Foundation contract model in `tools/tcti/orlix-tcti-gate.swift`.
- Positive contracts covered:
  - `FETCH` requires execute permission and reads guest text as host data.
  - `READ` requires read permission.
  - `WRITE` requires write permission and mutates only modeled guest bytes.
  - simulated host page sizes 4 KiB, 16 KiB, and 64 KiB with fixed 4 KiB guest pages.
  - guest-page offset inside a larger host allocation.
  - multiple guest pages sharing one host allocation without adjacent-page bleed.
  - cross-page instruction fetch.
  - cross-page data read/write with every guest page preflighted before mutation.
  - mprotect permission transitions invalidate stale translation generation.
  - munmap/remap and CoW replacement reject stale backing.
  - write to a translated executable page invalidates stale code generation.
- Negative reducers added:
  - `Build/TCTI/reproducers/tcti-memory-fuzz/fetch-exec-permission.json`
  - `Build/TCTI/reproducers/tcti-memory-fuzz/read-permission.json`
  - `Build/TCTI/reproducers/tcti-memory-fuzz/write-permission.json`
  - `Build/TCTI/reproducers/tcti-memory-fuzz/host-page-boundary.json`
  - `Build/TCTI/reproducers/tcti-memory-fuzz/generation-stale-backing.json`
  - `Build/TCTI/reproducers/tcti-memory-fuzz/memory-fuzz-pass-regression.json`
- Report path:
  - `Build/TCTI/reports/tcti-memory-fuzz/report.json`
- Per-case artifacts:
  - `Build/TCTI/memory_fuzz/*/result.json`

Evidence so far:

```text
rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift
rtk proxy make tcti-gate TARGET=tcti-memory-fuzz
```

Full checkpoint verification:

```text
rtk proxy git diff --check
rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift
rtk proxy make agent-harness-check
rtk proxy make tcti-gate TARGET=tcti-plan-consistency
rtk proxy make tcti-gate TARGET=tcti-toolchain-check
rtk proxy make tcti-gate TARGET=tcti-golden-elf
rtk proxy make tcti-gate TARGET=tcti-golden-elf CASE=init_001_exit EXECUTE=switch-debug
rtk proxy make tcti-gate TARGET=tcti-diff-switch CASE=init_001_exit BACKEND=gadget
rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit
rtk proxy make tcti-gate TARGET=tcti-contract
rtk proxy make tcti-gate TARGET=tcti-report-schema-check
rtk proxy make tcti-gate TARGET=tcti-memory-fuzz
rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-memory-fuzz/memory-fuzz-pass-regression.json
rtk proxy sh -c 'make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-memory-fuzz/fetch-exec-permission.json; rc=$?; echo rc=$rc; exit 0'
rtk proxy make agent-status AREA=orlix-tcti
rtk proxy make agent-next AREA=orlix-tcti
rtk proxy make agent-task-envelope-check AREA=orlix-tcti
rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" make -f OrlixKernel/Makefile kunit PROFILE=development
rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" make -f OrlixKernel/Makefile kunit PROFILE=release
```

Results:

- All commands above exited 0 except the expected negative reducer replay wrapper, which reported `rc=2` from the replayed failing fixture and exited 0.
- Development and release KUnit passed. Both printed the existing `__alloc_size` redefinition warning.
- `agent-status` advanced the next eligible gate to `tcti-direct-chain-fuzz`.
- `agent-next` wrote `Build/AgentHarness/orlix-tcti/next-task.json` and `Build/AgentHarness/orlix-tcti/next-task.md` for `tcti-direct-chain-fuzz`.
- `agent-task-envelope-check` passed for the `tcti-direct-chain-fuzz` envelope.

Boundary:

- No custom MCP added.
- No `tools/agent` added.
- No production TCTI assembly.
- No gadget dispatch.
- No simulator gate run.
- No physical-device gate run.
- No HostAdapter behavior.
- No Darwin syscall behavior.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No product defconfig flip.
- Release and physical-device readiness remain ineligible.

All five passed and wrote reports under `Build/TCTI/reports/`.

```sh
rtk proxy sh -c 'make tcti-gate TARGET=tcti-contract; rc=$?; echo rc=$rc; exit 0'
```

`tcti-contract` emitted `status=todo`, wrote `Build/TCTI/reports/tcti-contract/report.json`, wrote `Build/TCTI/reproducers/tcti-contract/todo.json`, and exited non-zero through Make (`rc=2`).

```sh
rtk proxy sh -c 'make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-contract/todo.json; rc=$?; echo rc=$rc; exit 0'
```

The reducer replayed `make tcti-gate TARGET=tcti-contract` and exited non-zero through Make (`rc=2`).

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

- Turned `make tcti-gate TARGET=tcti-contract` from a pure TODO rail into a partial real no-phone contract rail.
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
- Strengthened `make tcti-gate TARGET=tcti-golden-elf` for `init_001_exit`.
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
- Strengthened `make tcti-gate TARGET=tcti-repro REPRO=<path>` output.
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
rtk proxy make tcti-gate TARGET=tcti-plan-consistency
rtk proxy make tcti-gate TARGET=tcti-report-schema-check
rtk proxy make tcti-gate TARGET=tcti-toolchain-check
rtk proxy make tcti-gate TARGET=tcti-golden-elf
rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit
```

All passed.

```sh
rtk proxy sh -c 'make tcti-gate TARGET=tcti-contract; rc=$?; echo rc=$rc; exit 0'
```

Result:

- `tcti-contract` wrote `status=todo`.
- real contract groups were listed as passing.
- deeper CPU-state groups were listed as TODO.
- Make exited non-zero with `rc=2`.

```sh
rtk proxy sh -c 'make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-contract/todo.json; rc=$?; echo rc=$rc; exit 0'
```

Result:

- reducer replayed `make tcti-gate TARGET=tcti-contract`.
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

- Added `make tcti-gate TARGET=tcti-golden-elf CASE=init_001_exit EXECUTE=switch-debug`.
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
rtk proxy make tcti-gate TARGET=tcti-plan-consistency
rtk proxy make tcti-gate TARGET=tcti-report-schema-check
rtk proxy make tcti-gate TARGET=tcti-toolchain-check
rtk proxy make tcti-gate TARGET=tcti-golden-elf
rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit
rtk proxy make tcti-gate TARGET=tcti-golden-elf CASE=init_001_exit EXECUTE=switch-debug
```

All passed.

```sh
rtk proxy sh -c 'make tcti-gate TARGET=tcti-contract; rc=$?; echo rc=$rc; exit 0'
```

Result:

- real contract groups include switch-debug execution to captured `exit(42)`
- deeper groups remain TODO
- Make exited non-zero with `rc=2`

```sh
rtk proxy sh -c 'make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-wrong-syscall.json; rc=$?; echo rc=$rc; exit 0'
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
- `make tcti-gate TARGET=tcti-golden-elf CASE=init_001_exit EXECUTE=switch-debug` still captures guest `exit(42)`.
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
rtk proxy make tcti-gate TARGET=tcti-plan-consistency
rtk proxy make tcti-gate TARGET=tcti-report-schema-check
rtk proxy make tcti-gate TARGET=tcti-toolchain-check
rtk proxy make tcti-gate TARGET=tcti-golden-elf
rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit
rtk proxy make tcti-gate TARGET=tcti-golden-elf CASE=init_001_exit EXECUTE=switch-debug
```

All passed.

```sh
rtk proxy sh -c 'make tcti-gate TARGET=tcti-contract; rc=$?; echo rc=$rc; exit 0'
```

Result:

- real contract groups include minimal decoded AArch64 semantics to captured `exit(42)`
- deeper groups remain TODO
- Make exited non-zero with `rc=2`

```sh
rtk proxy sh -c 'make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-unsupported-svc-immediate.json; rc=$?; echo rc=$rc; exit 0'
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
- `make tcti-gate TARGET=tcti-golden-elf CASE=init_002_write` now verifies:
  - source SHA256
  - binary SHA256
  - ELF64 AArch64 executable shape
  - entrypoint
  - expected syscall shape
  - emitted instruction words
  - `hello\n` message bytes in file-backed PT_LOAD guest memory
- `make tcti-gate TARGET=tcti-golden-elf CASE=init_002_write EXECUTE=switch-debug` captures:
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
rtk proxy make tcti-gate TARGET=tcti-plan-consistency
rtk proxy make tcti-gate TARGET=tcti-report-schema-check
rtk proxy make tcti-gate TARGET=tcti-toolchain-check
rtk proxy make tcti-gate TARGET=tcti-golden-elf
rtk proxy make tcti-gate TARGET=tcti-golden-elf CASE=init_001_exit EXECUTE=switch-debug
rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit
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

### Correction: Full TCTI Remains Incomplete

- Corrected `PLAN.md` after an agent treated the scoped plan-documentation checkpoint as if the full TCTI objective were complete.
- Added an explicit completion claim boundary:
  - docs, harness rails, no-phone seed proofs, and next-task envelope validation are partial progress only;
  - full TCTI remains incomplete while the selected gate is still a no-phone switch-debug gate;
  - full TCTI remains incomplete while release/readiness gates are ineligible;
  - full TCTI completion requires the final reports listed in `PLAN.md`.
- Strengthened the current checkpoint language so `switch-init-003-stack` is sequencing proof only, not a TCTI completion claim.
- Boundary:
- No TCTI runtime feature implemented.
- No production assembly.
- No gadget dispatch.
- No app-hosted gate run.
- No custom MCP or `tools/agent` added.

### Checkpoint: Stack Golden ELF Executes In Switch Backend

- Harness-selected gate: `switch-init-003-stack`.
- Selected command: `make tcti-gate TARGET=tcti-golden-elf CASE=init_003_stack EXECUTE=switch-debug`.
- Why selected: `switch-init-002-write` was pass and `init_003_stack` validation/execution artifacts were missing.
- Added `init_003_stack` no-libc AArch64 Linux golden ELF fixture.
- Generated canonical `golden.json` with `make tcti-gate TARGET=tcti-golden-elf-refresh CASE=init_003_stack`.
- Exact emitted instruction words:
  - `0x910003e0` `mov x0, sp`
  - `0xd10043ff` `sub sp, sp, #0x10`
  - `0xd2800541` `mov x1, #0x2a`
  - `0xf90007e1` `str x1, [sp, #0x8]`
  - `0xf94007e0` `ldr x0, [sp, #0x8]`
  - `0x910043ff` `add sp, sp, #0x10`
  - `0xd2800ba8` `mov x8, #0x5d`
  - `0xd4000001` `svc #0`
- Added decoded switch-debug support for only the stack fixture subset:
  - `ADD/SUB immediate`, 64-bit, no flags, shift 0.
  - `MOV x0, sp` through the decoded `ADD x0, sp, #0` alias.
  - `LDR/STR unsigned immediate`, 64-bit, SP base.
  - bounded 4 KiB harness stack with little-endian 64-bit initialized-slot loads.
- Added stack negative fixtures:
  - wrong exit value;
  - invalid/uninitialized stack read;
  - unsupported pre-index stack store.
- Fixed case-specific reducer commands so execution reducers preserve `CASE`, `EXECUTE`, and `NEGATIVE_EXECUTION`.
- Reports written:
  - `Build/TCTI/golden_elf/init_003_stack/validation.json`
  - `Build/TCTI/golden_elf/init_003_stack/execution.json`
  - `Build/TCTI/reports/tcti-golden-elf/report.json`
- Reducers written:
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-stack-wrong-exit.json`
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-stack-invalid-memory.json`
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-stack-unsupported-preindex.json`
- Replayed reducers:
  - `make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-stack-invalid-memory.json`: expected `fail`, actual `fail`, nonzero replay exit.
  - `make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-stack-unsupported-preindex.json`: expected `fail`, actual `fail`, nonzero replay exit.
- `tcti-contract` now lists real passing group:
  - decoded switch-debug executes `init_003_stack` and captures stack-derived `exit(42)`.
- `tcti-contract` intentionally remains `todo` for:
  - gadget ABI and register commit-back execution;
  - FETCH/READ/WRITE memory execution;
  - TLB, block-cache, invalidation, and direct-chain execution.
- Boundary:
  - No production TCTI assembly.
  - No gadget dispatch.
  - No simulator gate run.
  - No physical-device gate run.
  - No HostAdapter, Darwin syscall, VFS, fd table, process, signal, scheduler, or Linux runtime behavior added.
  - No custom MCP or `tools/agent` added.

### Checkpoint: Harness Advances Past Stack Gate

- Fixed `agent-status` and `agent-next` detection for `switch-init-003-stack`.
- The next-step harness now reads `Build/TCTI/golden_elf/init_003_stack/execution.json` and treats the stack gate as pass only when:
  - backend is `switch-debug`;
  - entrypoint was entered;
  - eight guest instructions executed;
  - decoded instructions include stack load/store class evidence;
  - captured syscall is `exit(42)`.
- After the fix, the harness selected the next eligible gate:
  - `switch-init-004-tls`
  - command `make tcti-gate TARGET=tcti-golden-elf CASE=init_004_tls EXECUTE=switch-debug`

Evidence:

```sh
rtk proxy git diff --check
rtk proxy make agent-harness-check
rtk proxy make agent-status AREA=orlix-tcti
rtk proxy make agent-next AREA=orlix-tcti
rtk proxy make agent-task-envelope-check AREA=orlix-tcti
rtk proxy make tcti-gate TARGET=tcti-plan-consistency
```

All passed. Boundary:

- No TCTI runtime feature implemented.
- No production TCTI assembly.
- No gadget dispatch.
- No simulator gate run.
- No physical-device gate run.
- No custom MCP or `tools/agent` added.

### Checkpoint: TLS Golden ELF Executes In Switch Backend

- Harness-selected gate: `switch-init-004-tls`.
- Selected command: `make tcti-gate TARGET=tcti-golden-elf CASE=init_004_tls EXECUTE=switch-debug`.
- Why selected: `switch-init-003-stack` passed and `init_004_tls` validation/execution artifacts were missing.
- Added `init_004_tls` no-libc AArch64 Linux golden ELF fixture.
- Generated canonical `golden.json` with `make tcti-gate TARGET=tcti-golden-elf-refresh CASE=init_004_tls`.
- Exact emitted instruction words:
  - `0xd2800541` `mov x1, #0x2a`
  - `0xd51bd041` `msr TPIDR_EL0, x1`
  - `0xd2800000` `mov x0, #0x0`
  - `0xd53bd040` `mrs x0, TPIDR_EL0`
  - `0xd2800ba8` `mov x8, #0x5d`
  - `0xd4000001` `svc #0`
- Added decoded switch-debug support only for the TLS fixture subset:
  - AArch64 system-register transfer decode for `TPIDR_EL0` only.
  - `MSR TPIDR_EL0, Xt` writes a switch-debug guest-owned `guestTPIDREL0` cell.
  - `MRS Xt, TPIDR_EL0` reads that guest-owned cell.
  - Unsupported system registers remain structured unsupported-instruction failures.
- Guest TLS boundary: switch-debug does not read or write host `TPIDR_EL0`, does not use inline assembly, and does not call Darwin thread/TLS APIs.
- Added TLS negative fixtures:
  - wrong guest TLS exit value;
  - unsupported `TPIDRRO_EL0` system-register read.
- Added TLS reducer routing so `tls-*` negative fixtures replay under `CASE=init_004_tls`.
- Updated harness status detection so `switch-init-004-tls` passes only when:
  - backend is `switch-debug`;
  - entrypoint was entered;
  - six guest instructions executed;
  - decoded instructions include both `system_register` `msr` and `mrs` for `tpidr_el0`;
  - captured syscall is `exit(42)`.

Reports:

- `Build/TCTI/golden_elf/init_004_tls/validation.json`
- `Build/TCTI/golden_elf/init_004_tls/execution.json`
- `Build/TCTI/reports/tcti-golden-elf/report.json`
- `Build/TCTI/reports/tcti-contract/report.json`

Reducers:

- `Build/TCTI/reproducers/tcti-golden-elf/execution-tls-wrong-exit.json`
- `Build/TCTI/reproducers/tcti-golden-elf/execution-tls-unsupported-sysreg.json`

Reducer replay:

- `make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-tls-unsupported-sysreg.json`: expected `fail`, actual `fail`, exit code `2`.
- `make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-tls-wrong-exit.json`: expected `fail`, actual `fail`, exit code `2`.

Contract state:

- `tcti-contract` remains `todo` and exits nonzero.
- New real passing contract group: decoded switch-debug executes `init_004_tls` and captures guest `TPIDR_EL0`-derived `exit(42)` without host TLS mutation.
- Remaining TODO groups:
  - gadget ABI and register commit-back execution;
  - FETCH/READ/WRITE memory execution;
  - TLB, block-cache, invalidation, and direct-chain execution.

Harness state after checkpoint:

- `agent-status` recognizes `switch-init-004-tls` as passed.
- `agent-next` advanced to `diff-switch-init-001-exit`.
- Physical device work remains disallowed.
- Readiness and release gates remain ineligible.

Verification:

```text
rtk proxy git diff --check
rtk proxy make agent-harness-check
rtk proxy make agent-status AREA=orlix-tcti
rtk proxy make agent-next AREA=orlix-tcti
rtk proxy make agent-task-envelope-check AREA=orlix-tcti
rtk proxy make tcti-gate TARGET=tcti-plan-consistency
rtk proxy make tcti-gate TARGET=tcti-report-schema-check
rtk proxy make tcti-gate TARGET=tcti-toolchain-check
rtk proxy make tcti-gate TARGET=tcti-golden-elf
rtk proxy make tcti-gate TARGET=tcti-golden-elf CASE=init_004_tls
rtk proxy make tcti-gate TARGET=tcti-golden-elf CASE=init_004_tls EXECUTE=switch-debug
rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit
rtk proxy sh -c 'make tcti-gate TARGET=tcti-contract; rc=$?; echo rc=$rc; exit 0'
```

All pass except `tcti-contract`, which correctly reports `todo` with exit code `2`.

Boundary:

- No custom MCP added.
- No `tools/agent` added.
- No TCTI production runtime feature implemented.
- No production TCTI assembly.
- No gadget dispatch.
- No simulator gate run.
- No physical-device gate run.
- No HostAdapter behavior.
- No Darwin syscall behavior.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No host `TPIDR_EL0` mutation for guest TLS.

### Checkpoint: Switch Differential Gate For Exit ELF

- Harness-selected gate: `diff-switch-init-001-exit`.
- Selected command: `make tcti-gate TARGET=tcti-diff-switch CASE=init_001_exit`.
- Why selected: `switch-init-004-tls` passed and `tcti-diff-switch` was the first non-passing roadmap gate with prerequisites satisfied.
- Implemented a Swift harness diff-preparation rail for `CASE=init_001_exit`.
- Scope stayed inside `tools/tcti/orlix-tcti-gate.swift` and `docs/plans/active/orlix-tcti/IMPLEMENT.md`.
- The rail reuses the existing switch-debug oracle execution report. It does not execute or implement a gadget backend.
- The normalized architectural state records `gprs.x0`, `gprs.x8`, `sp`, `pc`, `pstate_nzcv`, `tpidr_el0`, `memory_writes`, `exit.kind`, `exit.code`, and `fault_address`.
- The positive diff-preparation artifact compares switch-debug `init_001_exit` against the canonical candidate contract and records zero divergent fields.
- The negative diff fixture mutates `exit.code` and fails with field-specific failure id `diff-architectural-state.exit.code`.

Reports:

- `Build/TCTI/reports/tcti-diff-switch/report.json`
- `Build/TCTI/diff_switch/init_001_exit/switch-state.json`
- `Build/TCTI/diff_switch/init_001_exit/candidate-state.json`
- `Build/TCTI/diff_switch/init_001_exit/diff.json`

Reducer:

- `Build/TCTI/reproducers/tcti-diff-switch/init_001_exit-exit-code-divergence.json`

Reducer replay:

- `make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-diff-switch/init_001_exit-exit-code-divergence.json`: expected `fail`, actual `fail`, exit code `2`.

Harness state after checkpoint:

- `agent-status` recognizes `diff-switch-init-001-exit` as passed.
- `agent-next` advanced to `first-gadget-init-001-exit`.
- Physical device work remains disallowed.
- Readiness and release gates remain ineligible.

Verification:

```text
rtk proxy git diff --check
rtk proxy make agent-harness-check
rtk proxy make agent-status AREA=orlix-tcti
rtk proxy make agent-next AREA=orlix-tcti
rtk proxy make agent-task-envelope-check AREA=orlix-tcti
rtk proxy make tcti-gate TARGET=tcti-plan-consistency
rtk proxy make tcti-gate TARGET=tcti-report-schema-check
rtk proxy make tcti-gate TARGET=tcti-toolchain-check
rtk proxy make tcti-gate TARGET=tcti-golden-elf
rtk proxy make tcti-gate TARGET=tcti-golden-elf CASE=init_001_exit EXECUTE=switch-debug
rtk proxy make tcti-gate TARGET=tcti-diff-switch
rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit
rtk proxy sh -c 'make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-diff-switch/init_001_exit-exit-code-divergence.json; rc=$?; echo rc=$rc; exit 0'
```

All passed except the reducer replay command, which correctly returned `rc=2` for expected `fail`.

Boundary:

- No custom MCP added.
- No `tools/agent` added.
- No production TCTI assembly.
- No gadget dispatch.
- No `BACKEND=gadget` execution.
- No simulator gate run.
- No physical-device gate run.
- No HostAdapter behavior.
- No Darwin syscall behavior.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.


### Checkpoint: First Exit Gadget Differential

- Harness-selected gate: `first-gadget-init-001-exit`.
- Selected command: `make tcti-gate TARGET=tcti-diff-switch CASE=init_001_exit BACKEND=gadget`.
- Harness selected this gate because `diff-switch-init-001-exit` and `appstore-safety` were passing and `first-gadget-init-001-exit` was the next eligible non-passing roadmap gate.
- Implemented bounded no-phone `BACKEND=gadget` handling in the Swift rail for `init_001_exit` only.
- Candidate backend: `gadget-data-program`.
- Compared switch-debug reference and gadget candidate state for `gprs.x0`, `gprs.x8`, `sp`, `pc`, `pstate_nzcv`, `tpidr_el0`, `memory_writes`, `exit.kind`, `exit.code`, and `fault_address`.
- `diff.json` records `mode: switch-vs-gadget`, `candidate_backend: gadget-data-program`, `gadget_dispatch_executed: true`, `production_assembly_executed: false`, and `divergent_fields: []`.
- Added focused KUnit coverage that the existing data-gadget program executes the `init_001_exit` MOVZ prefix:
  - `0xd2800ba8` sets `x8 = 93` and advances `pc` to `0x210124`.
  - `0xd2800540` sets `x0 = 42` and advances `pc` to `0x210128`.
  - `0xd4000001` remains an SVC boundary and is rejected by gadget lowering.
- Fixed App Store safety audit object-disassembly scanning to use file-backed command output, avoiding pipe deadlock on real `llvm-objdump -d` output.
- Verified object-disassembly coverage with repo-local KUnit object `Build/OrlixKernel/kunit/development/arch/orlix/hosted_exec/tcti/tests/tcti_decode_test.o`.
- Safety audit report records `scanned_objects: 1` and no coverage warnings.
- Updated the autonomous harness status script so `first-gadget-init-001-exit` is recognized from the `switch-vs-gadget` diff artifact and the next eligible gate can advance.

Reports:

- `Build/TCTI/reports/tcti-diff-switch/report.json`
- `Build/TCTI/diff_switch/init_001_exit/switch-state.json`
- `Build/TCTI/diff_switch/init_001_exit/candidate-state.json`
- `Build/TCTI/diff_switch/init_001_exit/diff.json`
- `Build/TCTI/reports/tcti-appstore-safety-audit/report.json`

Reducer:

- `Build/TCTI/reproducers/tcti-diff-switch/init_001_exit-gadget-x0-divergence.json`
- `make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-diff-switch/init_001_exit-gadget-x0-divergence.json` replayed expected `fail`, actual `fail`, exit code `2`.

Harness state checkpoint:

- `agent-status` recognizes `first-gadget-init-001-exit` passed.
- `agent-next` advances to the next certification gate.
- Release and readiness gates remain ineligible.

### Checkpoint: Simulator TCTI Runtime Stability Passes

- Harness-selected gate before the runtime fix: `simulator-tcti-runtime-stability`.
- Selected command:
  `make runtime-validation DESTINATION=iphonesimulator GATE=tcti-simulator-stability ORLIX_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=Orlix-iPhone-15-Pro-Max`.
- Simulator used: `Orlix-iPhone-15-Pro-Max` (`C47ED88D-0D0A-420D-8C78-D4C1D34A276D`).
- Single-simulator constraint: verified only the pinned simulator was booted before simulator gate runs.
- Structured failure reducer:
  - Added `tcti_runtime_events` to simulator runtime-validation JSON so reducers consume machine-readable failure facts instead of grepping prose logs.
  - Added `tcti-post-bash-mmap-read-fault-reducer`.
  - Report: `Build/TCTI/reports/tcti-post-bash-mmap-read-fault-reducer/report.json`.
  - Reducer: `Build/TCTI/reproducers/tcti-post-bash-mmap-read-fault-reducer/post-bash-mmap-read-fault-pass-regression.json`.
  - Replay: `make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-post-bash-mmap-read-fault-reducer/post-bash-mmap-read-fault-pass-regression.json` exited `0`.
- Runtime fix:
  - `tcti_handle_user_fault()` now synchronizes the hosted user fault window after Linux faults in a valid TCTI user page.
  - This keeps Linux MM as the authority and only refreshes the host-visible mapping window before TCTI retries the instruction.
  - No HostAdapter Linux behavior, Darwin syscall behavior, VFS, fd table, process, signal, scheduler, or custom Linux runtime semantics were added.
- Passing simulator report:
  - `Build/Reports/runtime/tcti-simulator-stability-20260703T082036Z-25210.json`.
  - `status=pass`, `passed=true`.
  - `selected_device_id=C47ED88D-0D0A-420D-8C78-D4C1D34A276D`.
  - `selected_device_name=Orlix-iPhone-15-Pro-Max`.
  - `simulator_single_booted=true`.
  - Forbidden behavior remained false: `generated_exec_memory`, `host_exec_guest_text`, `host_x18`, `map_jit`, `native_ios_api_exposure_to_guest`, `rwx`.
- Harness status after the pass:
  - `simulator-tcti-runtime-stability=pass`.
  - `physical_device_allowed=true`.
  - `release_gate_eligible=false`.
  - `readiness_gate_eligible=false`.
  - `agent-next` selects `physical-tcti-init-first-syscall`.
- Boundary:
  - No custom MCP added.
  - No `tools/agent` added.
  - No production TCTI assembly added.
  - No physical-device gate run in this checkpoint.
  - No product defconfig flip.
  - No generated Linux/build tree edits.
  - The simulator gate is now the evidence-backed prerequisite for any later physical-device request.
- Verification:
  - `rtk proxy git diff --check`: pass.
  - `rtk proxy make agent-harness-check`: pass.
  - `rtk proxy make agent-status AREA=orlix-tcti`: pass, simulator stability pass recorded.
  - `rtk proxy make agent-next AREA=orlix-tcti`: pass, next selected gate is physical first-syscall.
  - `rtk proxy make agent-task-envelope-check AREA=orlix-tcti`: pass.
  - `rtk proxy make tcti-gate TARGET=tcti-plan-consistency`: pass.
  - `rtk proxy make tcti-gate TARGET=tcti-report-schema-check`: pass.
  - `rtk proxy make tcti-gate TARGET=tcti-toolchain-check`: pass.
  - `rtk proxy make tcti-gate TARGET=tcti-golden-elf`: pass.
  - `rtk proxy make tcti-gate TARGET=tcti-golden-elf CASE=init_001_exit EXECUTE=switch-debug`: pass.
  - `rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit`: pass.
  - `rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" make -f OrlixKernel/Makefile kunit PROFILE=development`: pass.
  - `rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" make -f OrlixKernel/Makefile kunit PROFILE=release`: pass.

### Checkpoint: Simulator TCTI Progress Through Static PIE Relocation And SIMD Self-Move

- Harness-selected gates advanced through:
  - `tcti-static-pie-relocation-fix`
  - `no-phone-tcti-simd-self-move-reducer`
  - `tcti-simd-self-move-fix`
  - `no-phone-tcti-brk-trap-reducer`
- Required simulator: `Orlix-iPhone-15-Pro-Max` (`C47ED88D-0D0A-420D-8C78-D4C1D34A276D`).
- Simulator-only rule remained active. Physical-device gates stayed blocked.
- Static PIE `R_AARCH64_RELATIVE` relocation support moved the runtime past the prior null GOT byte-load user fault:
  - previous blocker: `user fault addr=0x0 access=1`, init killed with `exitcode=0x0000000b`;
  - current evidence shows `Orlix TCTI: applied static PIE R_AARCH64_RELATIVE relocations`;
  - the simulator now reaches real TCTI syscall markers again.
- Narrow decoded SIMD support added only for the observed no-op lane self-move:
  - instruction: `0x6e080400`;
  - disassembly: `mov v0.d[0], v0.d[0]`;
  - supported behavior: decoded no-op only when `rd == rn`, lane `0`, width `64`;
  - no broad SIMD/vector register model added.
- Reducer and fix reports:
  - `Build/TCTI/reports/tcti-static-pie-relocation-fix/report.json`
  - `Build/TCTI/reports/tcti-simd-self-move-reducer/report.json`
  - `Build/TCTI/reports/tcti-simd-self-move-fix/report.json`
  - `Build/TCTI/reports/tcti-brk-trap-reducer/report.json`
- Replayed reducers:
  - `Build/TCTI/reproducers/tcti-simd-self-move-reducer/execution-simd-self-move-unsupported.json`
  - `Build/TCTI/reproducers/tcti-brk-trap-reducer/execution-brk-trap-unsupported.json`
- Latest pinned simulator stability report:
  - `Build/Reports/runtime/tcti-simulator-stability-20260702T205020Z-95399.json`
  - status: `fail`;
  - selected simulator: `C47ED88D-0D0A-420D-8C78-D4C1D34A276D`;
  - reached syscall `178`;
  - reached syscall `222` (`mmap`);
  - current blocker: unsupported `0xd4200020`, `brk #0x1`, init killed with `exitcode=0x00000004`.
- Current interpretation:
  - `brk #0x1` is not a semantic success path and must not be implemented as a no-op;
  - disassembly shows a guard around GOT slot `0x43348`, which remains zero while neighboring static PIE relative GOT entries are relocated;
  - next production work must address the underlying static-PIE startup/GOT cause through a reduced no-phone proof before simulator stability can pass.

Boundary:

- No custom MCP added.
- No `tools/agent` added.
- No physical-device gate run.
- No production TCTI assembly added.
- No gadget dispatch added.
- No product defconfig flip.
- No HostAdapter Linux behavior added.
- No Darwin guest syscall side effects added.
- No VFS, fd table, process model, scheduler, signal, or Linux runtime semantics added.
- No BRK-as-success no-op added.

### Checkpoint: Structural Golden `init_007_mprotect`

- Harness-selected gate: `golden-init-007-mprotect-structural`.
- Selected command: `make tcti-gate TARGET=tcti-golden-elf CASE=init_007_mprotect`.
- Why selected: all earlier no-phone golden structural and switch-debug gates through `switch-init-006-memory` were passing, making the next missing ready gate the structural `mprotect` fixture.
- Added fixture:
  - `OrlixKernel/Tests/TCTI/golden_elf/init_007_mprotect/init_007_mprotect.S`.
  - `OrlixKernel/Tests/TCTI/golden_elf/init_007_mprotect/golden.json`.
- Structural intent:
  - no-libc AArch64 Linux ELF;
  - descriptive syscall shape only: `mprotect(page, 4096, PROT_READ)` then `exit(0)`;
  - no switch-debug execution for this gate;
  - no Linux `mprotect` semantics;
  - no host `mprotect`, `vm_protect`, `MAP_JIT`, RWX, or executable-memory behavior.
- Emitted entrypoint:
  - `0x0000000000211000`.
- Emitted instruction encodings:
  - `0x10008000`: `adr x0, 0x212000 <page>`.
  - `0xd2820001`: `mov x1, #4096`.
  - `0xd2800022`: `mov x2, #1`.
  - `0xd2801c48`: `mov x8, #226`.
  - `0xd4000001`: `svc #0`.
  - `0xd2800000`: `mov x0, #0`.
  - `0xd2800ba8`: `mov x8, #93`.
  - `0xd4000001`: `svc #0`.
- Validator updates:
  - accepts `init_007_mprotect` in structural `tcti-golden-elf`;
  - verifies source hash, binary hash, ELF64 AArch64 executable shape, entrypoint, `mprotect`/`exit(0)` expected syscall metadata, ADR-to-`page`, required instruction words, two `svc #0` instructions, and all forbidden behavior flags false;
  - derives refreshed metadata entrypoint from `llvm-objdump -f` output so aligned fixtures do not inherit the older default entrypoint.
- Reports:
  - `Build/TCTI/reports/tcti-golden-elf/report.json`.
  - `Build/TCTI/golden_elf/init_007_mprotect/validation.json`.
- Evidence so far:

```text
rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift
rtk proxy make tcti-gate TARGET=tcti-golden-elf-refresh CASE=init_007_mprotect
rtk proxy make tcti-gate TARGET=tcti-golden-elf CASE=init_007_mprotect
```

- Boundary:
  - No custom MCP added.
  - No `tools/agent` added.
  - No TCTI runtime feature implemented.
  - No switch-debug execution for `init_007_mprotect`.
  - No production TCTI assembly.
  - No gadget dispatch.
  - No generated executable memory.
  - No host-executable guest text.
  - No simulator gate run.
  - No phone gate run.
  - No HostAdapter behavior.
  - No Darwin syscall behavior.
  - No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
  - No product defconfig flip.

### Checkpoint: Switch-Debug Executes `init_007_mprotect`

- Harness-selected gate: `switch-init-007-mprotect`.
- Selected command: `make tcti-gate TARGET=tcti-golden-elf CASE=init_007_mprotect EXECUTE=switch-debug`.
- Why selected: `golden-init-007-mprotect-structural` passed and the next roadmap gate was the no-phone switch-debug execution proof for the same case.
- Added switch-debug semantic behavior:
  - capture guest Linux syscall `226` as `mprotect`;
  - record args `[page_address, 4096, 1]`;
  - continue execution after the non-exit captured syscall;
  - stop on the later captured guest `exit(0)`;
  - do not call host `mprotect`, `vm_protect`, Darwin syscalls, `MAP_JIT`, RWX, or any executable-memory path.
- Expected captured syscalls:
  - `mprotect("0x0000000000212000", 4096, 1)`;
  - `exit(0)`.
- Execution report:
  - `Build/TCTI/golden_elf/init_007_mprotect/execution.json`.
- Reducers:
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-mprotect-wrong-prot.json`.
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-mprotect-exec-prot.json`.
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-mprotect-wrong-syscall.json`.
- Replayed reducer:

```text
rtk proxy sh -c 'make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-mprotect-exec-prot.json; rc=$?; echo rc=$rc; exit 0'
```

Result:

```text
expected status: fail
actual replay status: fail
actual replay exit code: 2
rc=2
```

- Evidence so far:

```text
rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift
rtk proxy make tcti-gate TARGET=tcti-golden-elf CASE=init_007_mprotect EXECUTE=switch-debug
```

- Harness follow-up:
  - `agent-status` originally did not recognize `switch-init-007-mprotect` as pass even after the execution report existed.
  - Added the skill-local status recognizer for `init_007_mprotect` execution so the harness validates captured `mprotect(PROT_READ)` plus `exit(0)` and advances to `golden-init-008-self-modify-structural`.
  - Verified:

```text
rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift
rtk proxy make agent-harness-check
rtk proxy make agent-status AREA=orlix-tcti
rtk proxy make agent-next AREA=orlix-tcti
rtk proxy make agent-task-envelope-check AREA=orlix-tcti
```

  - New selected gate after this checkpoint: `golden-init-008-self-modify-structural`.

- Boundary:
  - No custom MCP added.
  - No `tools/agent` added.
  - No production TCTI assembly.
  - No gadget dispatch.
  - No simulator gate run.
  - No phone gate run.
  - No HostAdapter behavior.
  - No Darwin syscall behavior.
  - No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
  - No host `mprotect`, `vm_protect`, `MAP_JIT`, RWX, generated executable memory, or host-executable guest text added.
  - No product defconfig flip.

### Checkpoint: Structural Golden `init_008_self_modify`

- Harness-selected gate: `golden-init-008-self-modify-structural`.
- Selected command: `make tcti-gate TARGET=tcti-golden-elf CASE=init_008_self_modify`.
- Why selected: `switch-init-007-mprotect` passed and the harness advanced to the next structural no-phone golden ELF gate.
- Added fixture:
  - `OrlixKernel/Tests/TCTI/golden_elf/init_008_self_modify/init_008_self_modify.S`.
  - `OrlixKernel/Tests/TCTI/golden_elf/init_008_self_modify/golden.json`.
- Structural intent:
  - no-libc AArch64 Linux ELF;
  - compute a PC-relative address to `patch_slot` in `.text`;
  - store `42` into that text-backed slot;
  - exit with `exit(0)`;
  - do not execute modified code;
  - do not implement self-modifying execution semantics.
- Emitted entrypoint:
  - `0x0000000000210120`.
- Emitted instruction encodings:
  - `0x100000c1`: `adr x1, 0x210138 <patch_slot>`.
  - `0xd2800540`: `mov x0, #42`.
  - `0xf9000020`: `str x0, [x1]`.
  - `0xd2800000`: `mov x0, #0`.
  - `0xd2800ba8`: `mov x8, #93`.
  - `0xd4000001`: `svc #0`.
- Validator updates:
  - accepts `init_008_self_modify` in structural `tcti-golden-elf`;
  - verifies source hash, binary hash, ELF64 AArch64 executable shape, entrypoint, expected `exit(0)` metadata, ADR-to-`patch_slot`, store-to-`patch_slot`, required instruction words, and all forbidden behavior flags false;
  - keeps `init_008_self_modify` out of the switch-debug execution allow-list for this structural checkpoint.
- Reports:
  - `Build/TCTI/reports/tcti-golden-elf/report.json`.
  - `Build/TCTI/golden_elf/init_008_self_modify/validation.json`.
- Evidence so far:

```text
rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift
rtk proxy make tcti-gate TARGET=tcti-golden-elf-refresh CASE=init_008_self_modify
rtk proxy make tcti-gate TARGET=tcti-golden-elf CASE=init_008_self_modify
```

- Boundary:
  - No custom MCP added.
  - No `tools/agent` added.
  - No TCTI runtime feature implemented.
  - No switch-debug execution for `init_008_self_modify`.
  - No self-modifying execution semantics.
  - No production TCTI assembly.
  - No gadget dispatch.
  - No simulator gate run.
  - No phone gate run.
  - No HostAdapter behavior.
  - No Darwin syscall behavior.
  - No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
  - No generated executable memory or host-executable guest text added.
  - No product defconfig flip.

### Checkpoint: Switch-Debug Executes `init_008_self_modify`

- Harness-selected gate: `switch-init-008-self-modify`.
- Selected command: `make tcti-gate TARGET=tcti-golden-elf CASE=init_008_self_modify EXECUTE=switch-debug`.
- Why selected: `golden-init-008-self-modify-structural` passed and the harness advanced to the next switch-debug execution proof for the same case.
- Added switch-debug semantic behavior:
  - capture register-based 64-bit `STR` to file-backed guest `PT_LOAD` bytes as a test-harness memory-write event;
  - verify the target address is backed by readable ELF bytes before recording the write;
  - continue execution after the captured memory write;
  - stop on the later captured guest `exit(0)`;
  - do not mutate host executable mappings;
  - do not execute modified guest bytes;
  - do not add generated executable memory, host-executable guest text, or invalidation semantics.
- Expected captured memory write:
  - address: `0x0000000000210138`;
  - width: `64`;
  - value: `0x000000000000002a`;
  - captured: `true`.
- Expected captured syscall:
  - `exit(0)`.
- Execution report:
  - `Build/TCTI/golden_elf/init_008_self_modify/execution.json`.
- Reducers:
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-self-modify-wrong-value.json`.
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-self-modify-invalid-write.json`.
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-self-modify-unsupported-branch.json`.
- Replayed reducer:

```text
rtk proxy sh -c 'make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-self-modify-invalid-write.json; rc=$?; echo rc=$rc; exit 0'
```

Result:

```text
expected status: fail
actual replay status: fail
actual replay exit code: 2
rc=2
```

- Harness follow-up:
  - Added the skill-local status recognizer for `switch-init-008-self-modify`.
  - The recognizer requires switch-debug backend, entered entrypoint, 6 guest instructions, captured patch-slot write, decoded `STR x0, [x1]`, and captured `exit(0)`.
  - After recognition, `agent-next` selected `golden-init-009-faults-structural`.
- Evidence so far:

```text
rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift
rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift
rtk proxy make tcti-gate TARGET=tcti-golden-elf CASE=init_008_self_modify EXECUTE=switch-debug
rtk proxy make agent-status AREA=orlix-tcti
rtk proxy make agent-next AREA=orlix-tcti
rtk proxy make agent-task-envelope-check AREA=orlix-tcti
```

- Boundary:
  - No custom MCP added.
  - No `tools/agent` added.
  - No production TCTI assembly.
  - No gadget dispatch.
  - No simulator gate run.
  - No phone gate run.
  - No HostAdapter behavior.
  - No Darwin syscall behavior.
  - No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
  - No generated executable memory.
  - No host-executable guest text.
  - No execution of modified guest bytes.
  - No block invalidation semantics.
  - No product defconfig flip.

### Checkpoint: Structural Golden `init_009_faults`

- Harness-selected gate: `golden-init-009-faults-structural`.
- Selected command: `make tcti-gate TARGET=tcti-golden-elf CASE=init_009_faults`.
- Why selected: `switch-init-008-self-modify` passed and the harness advanced to the next structural no-phone golden ELF gate.
- Added fixture:
  - `OrlixKernel/Tests/TCTI/golden_elf/init_009_faults/init_009_faults.S`.
  - `OrlixKernel/Tests/TCTI/golden_elf/init_009_faults/golden.json`.
- Structural intent:
  - no-libc AArch64 Linux ELF;
  - set `x1 = 0`;
  - structurally attempt `ldr x0, [x1]` from guest address zero;
  - include an unreachable `exit(1)` tail only to keep deterministic ELF shape;
  - do not execute guest instructions in this checkpoint;
  - do not implement signal, process, scheduler, or Linux fault delivery semantics.
- Emitted entrypoint:
  - `0x0000000000210120`.
- Emitted instruction encodings:
  - `0xd2800001`: `mov x1, #0`.
  - `0xf9400020`: `ldr x0, [x1]`.
  - `0xd2800020`: `mov x0, #1`.
  - `0xd2800ba8`: `mov x8, #93`.
  - `0xd4000001`: `svc #0`.
- Validator updates:
  - accepts `init_009_faults` in structural `tcti-golden-elf`;
  - verifies source hash, binary hash, ELF64 AArch64 executable shape, entrypoint, empty expected syscall list, invalid-load instruction shape, and all forbidden behavior flags false;
  - keeps `init_009_faults` out of any runtime, signal, process, or physical-device behavior.
- Reports:
  - `Build/TCTI/reports/tcti-golden-elf/report.json`.
  - `Build/TCTI/golden_elf/init_009_faults/validation.json`.
- Evidence so far:

```text
rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift
rtk proxy make tcti-gate TARGET=tcti-golden-elf-refresh CASE=init_009_faults
rtk proxy make tcti-gate TARGET=tcti-golden-elf CASE=init_009_faults
```

- Boundary:
  - No custom MCP added.
  - No `tools/agent` added.
  - No TCTI runtime feature implemented.
  - No switch-debug execution for `init_009_faults`.
  - No signal, process, scheduler, VFS, fd table, or Linux runtime semantics added.
  - No production TCTI assembly.
  - No gadget dispatch.
  - No simulator gate run.
  - No phone gate run.
  - No HostAdapter behavior.
  - No Darwin syscall behavior.
  - No generated executable memory.
  - No host-executable guest text.
  - No product defconfig flip.

### Checkpoint: Switch-Debug Executes `init_009_faults`

- Harness-selected gate: `switch-init-009-faults`.
- Selected command: `make tcti-gate TARGET=tcti-golden-elf CASE=init_009_faults EXECUTE=switch-debug`.
- Why selected: `golden-init-009-faults-structural` passed and the harness advanced to the next switch-debug execution proof for the same case.
- Added switch-debug semantic behavior:
  - capture failed file-backed guest memory reads as a structured `guest_memory_fault`;
  - stop before syscall or guest exit when the fault is captured;
  - do not deliver Linux signals;
  - do not implement process, scheduler, VFS, fd table, or Linux runtime semantics.
- Expected captured fault:
  - kind: `guest_memory_fault`;
  - address: `0x0000000000000000`;
  - access: `read`;
  - captured: `true`.
- Expected execution shape:
  - `guest_instructions_executed = 2`;
  - decoded `LDR x0, [x1]` effective address `0x0000000000000000`;
  - no captured syscalls;
  - no captured guest exit.
- Execution report:
  - `Build/TCTI/golden_elf/init_009_faults/execution.json`.
- Reducers:
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-faults-wrong-address.json`.
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-faults-missing-fault.json`.
- Replayed reducer:

```text
rtk proxy sh -c 'make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-faults-wrong-address.json; rc=$?; echo rc=$rc; exit 0'
```

Result:

```text
expected status: fail
actual replay status: fail
actual replay exit code: 2
rc=2
```

- Harness follow-up:
  - Added the skill-local status recognizer for `switch-init-009-faults`.
  - The recognizer requires switch-debug backend, entered entrypoint, 2 guest instructions, captured read fault at guest address zero, no syscall, no exit, and decoded faulting LDR.
  - After recognition, `agent-next` selected `golden-init-010-cpu-model-structural`.
- Evidence so far:

```text
rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift
rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift
rtk proxy make tcti-gate TARGET=tcti-golden-elf CASE=init_009_faults EXECUTE=switch-debug
rtk proxy make agent-status AREA=orlix-tcti
rtk proxy make agent-next AREA=orlix-tcti
rtk proxy make agent-task-envelope-check AREA=orlix-tcti
```

- Boundary:
  - No custom MCP added.
  - No `tools/agent` added.
  - No Linux signal delivery semantics.
  - No process, scheduler, VFS, fd table, or Linux runtime semantics added.
  - No production TCTI assembly.
  - No gadget dispatch.
  - No simulator gate run.
  - No phone gate run.
  - No HostAdapter behavior.
  - No Darwin syscall behavior.
  - No generated executable memory.
  - No host-executable guest text.
  - No product defconfig flip.
- The next gate selected by the harness was not executed in this checkpoint.

Verification:

```text
rtk proxy git diff --check
rtk proxy make agent-harness-check
rtk proxy make agent-status AREA=orlix-tcti
rtk proxy make agent-next AREA=orlix-tcti
rtk proxy make agent-task-envelope-check AREA=orlix-tcti
rtk proxy make tcti-gate TARGET=tcti-plan-consistency
rtk proxy make tcti-gate TARGET=tcti-report-schema-check
rtk proxy make tcti-gate TARGET=tcti-toolchain-check
rtk proxy make tcti-gate TARGET=tcti-golden-elf
rtk proxy make tcti-gate TARGET=tcti-golden-elf CASE=init_001_exit EXECUTE=switch-debug
rtk proxy make tcti-gate TARGET=tcti-diff-switch
rtk proxy make tcti-gate TARGET=tcti-diff-switch CASE=init_001_exit BACKEND=gadget
rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit
rtk proxy sh -c "make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-diff-switch/init_001_exit-gadget-x0-divergence.json; rc=$?; echo rc=$rc; exit 0"
rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" make -f OrlixKernel/Makefile kunit PROFILE=development
rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" ORLIX_BUILD_ROOT="$PWD/Build" make -f OrlixKernel/Makefile kunit PROFILE=development
rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" make -f OrlixKernel/Makefile kunit PROFILE=release
```

Boundary:

- No custom MCP added.
- No `tools/agent` added.
- No production TCTI assembly.
- No generated executable memory.
- No host-executable guest text.
- No `MAP_JIT`, RWX, or `PROT_EXEC` guest-text path.
- No host `x18` or `w18`.
- No simulator gate run.
- No phone gate run.
- No HostAdapter behavior.
- No Darwin syscall behavior.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No product defconfig flip.
- Release and readiness gates remain ineligible for this checkpoint.

### Checkpoint: Runtime Preflight Reflected In Agent Next-Step

- Harness-selected gate before this checkpoint: the first runtime certification gate.
- Runtime validation rejected that gate before execution because the autonomous no-phone preflight reports are incomplete.
- The generated runtime report recorded `status: fail`, `passed: false`, `autonomous_tests_bypassed: false`, and no artifacts.
- Blockers observed:
  - `Build/TCTI/reports/tcti-contract/report.json` is not passing.
  - `Build/TCTI/reports/tcti-memory-fuzz/report.json` is missing.
  - `Build/TCTI/reports/tcti-direct-chain-fuzz/report.json` is missing.
- Updated the TCTI next-step skill driver to synthesize the runtime preflight gates before the certification gate:
  - `tcti-contract`
  - `tcti-memory-fuzz`
  - `tcti-direct-chain-fuzz`
- `agent-status` now reports certification work is not allowed while those preflight gates are incomplete.
- `agent-next` now selects `tcti-contract` instead of the certification gate.
- This keeps the autonomous harness aligned with `tools/runtime/orlix-runtime-validation.sh`.

Verification:

```text
swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift
rtk proxy make agent-harness-check
rtk proxy make agent-status AREA=orlix-tcti
rtk proxy make agent-next AREA=orlix-tcti
rtk proxy make agent-task-envelope-check AREA=orlix-tcti
rtk proxy make tcti-gate TARGET=tcti-plan-consistency
rtk proxy make tcti-gate TARGET=tcti-report-schema-check
```

Boundary:

- No custom MCP added.
- No `tools/agent` added.
- No production TCTI assembly.
- No generated executable memory.
- No host-executable guest text.
- No simulator gate run.
- No phone gate run.
- No HostAdapter behavior.
- No Darwin syscall behavior.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No product defconfig flip.
- Release and readiness gates remain ineligible.

### Checkpoint: Generic TCTI Gate Interface And Current Simulator Blocker

- Harness interface changed to one generic TCTI gate dispatcher:
  - `make tcti-gate TARGET=<gate>`
  - `make tcti-gate-list`
- The Makefile no longer exposes one Make target per TCTI gate.
- Active harness docs, skills, subagent prompts, and roadmap command envelopes now use `make tcti-gate TARGET=...` for TCTI gate execution.
- Harness enforcement now fails if the Makefile reintroduces a one-target-per-gate TCTI fanout.
- `make tcti-gate-list` lists the supported TCTI gate IDs for agents and humans.

No-phone MOVI 16B checkpoint:

- Gate: `tcti-simd-movi-16b-fix`
- Command: `make tcti-gate TARGET=tcti-simd-movi-16b-fix`
- Original simulator evidence:
  - `Build/Reports/runtime/tcti-simulator-stability-20260703T105406Z-27160.json`
  - Unsupported instruction: `0x4f06e7e0`
  - Decoded as `movi.16b v0, #0xdf`
  - Associated signal: SIGILL, `signal=4`
- No-phone fixture:
  - `tools/tcti/fixtures/golden_elf/init_001_exit_simd_movi_16b.S`
- Execution report:
  - `Build/TCTI/simd_movi_16b_fix/positive/init_001_exit_simd_movi_16b/execution.json`
- Reducer:
  - `Build/TCTI/reproducers/tcti-simd-movi-16b-fix/simd-movi-16b-pass-regression.json`
- Gate report:
  - `Build/TCTI/reports/tcti-simd-movi-16b-fix/report.json`
- The gate now validates recorded simulator evidence plus the no-phone regression, instead of requiring the latest simulator run to keep failing at the already-reduced MOVI instruction.

Current harness-selected runtime blocker:

- `agent-next` selects `simulator-tcti-runtime-stability`.
- Selected command:

```text
make runtime-validation DESTINATION=iphonesimulator GATE=tcti-simulator-stability ORLIX_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=Orlix-iPhone-15-Pro-Max
```

- Latest simulator report:
  - `Build/Reports/runtime/tcti-simulator-stability-20260703T112917Z-26978.json`
- Result:
  - `status=fail`
  - `summary=Simulator TCTI runtime captured a fatal post-launch error.`
  - selected simulator: `Orlix-iPhone-15-Pro-Max`
  - selected simulator UDID: `C47ED88D-0D0A-420D-8C78-D4C1D34A276D`
  - `simulator_single_booted=true`
  - `tcti_runtime_events.first_svc.syscall=178`
  - `tcti_runtime_events.static_pie_image.task=sh`
  - `tcti_runtime_events.signaled_process.pid=33`
  - `tcti_runtime_events.signaled_process.signal=6`
- Fatal artifact:
  - `Build/Reports/runtime/tcti-simulator-stability-20260703T112917Z-26978.artifacts/tcti-simulator-fatal-runtime.txt`

Next work:

- Reduce the current simulator `sh` SIGABRT (`signal=6`) into a no-phone reducer before production patching.
- Keep physical-device gates blocked until simulator stability passes.

Verification in this checkpoint:

```text
rtk proxy git diff --check
rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift
rtk proxy swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift
rtk proxy bash -n tools/runtime/orlix-runtime-validation.sh
rtk proxy make tcti-gate-list
rtk proxy make agent-harness-check
rtk proxy make agent-hooks-check
rtk proxy make agent-skills-check
rtk proxy make agent-subagents-check
rtk proxy make agent-mcp-check
rtk proxy make agent-status AREA=orlix-tcti
rtk proxy make agent-next AREA=orlix-tcti
rtk proxy make agent-task-envelope-check AREA=orlix-tcti
rtk proxy make tcti-gate TARGET=tcti-plan-consistency
rtk proxy make tcti-gate TARGET=tcti-report-schema-check
rtk proxy make tcti-gate TARGET=tcti-toolchain-check
rtk proxy make tcti-gate TARGET=tcti-golden-elf
rtk proxy make tcti-gate TARGET=tcti-golden-elf CASE=init_001_exit EXECUTE=switch-debug
rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit
rtk proxy make tcti-gate TARGET=tcti-simd-movi-16b-fix
rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-simd-movi-16b-fix/simd-movi-16b-pass-regression.json
rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" HOMEBREW_NO_AUTO_UPDATE=1 make runtime-validation DESTINATION=iphonesimulator GATE=tcti-simulator-stability ORLIX_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=Orlix-iPhone-15-Pro-Max
```

Results:

- Generic harness and no-phone gates above passed.
- Simulator stability gate failed with structured report `Build/Reports/runtime/tcti-simulator-stability-20260703T112917Z-26978.json`.
- Only `Orlix-iPhone-15-Pro-Max (C47ED88D-0D0A-420D-8C78-D4C1D34A276D)` was booted during the simulator check.

Boundary:

- No custom MCP added.
- No `tools/agent` added.
- No physical-device gate run.
- No production TCTI assembly.
- No gadget dispatch added.
- No HostAdapter behavior added.
- No Darwin syscall behavior added.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No product defconfig flip.
- Release and readiness gates remain ineligible until simulator stability passes.

### Checkpoint: Static PIE Relocation Marker Uses Structured Runtime Evidence

- Harness-selected gate: `tcti-static-pie-relocation-fix`.
- Selected command: `make tcti-gate TARGET=tcti-static-pie-relocation-fix`.
- Why selected: the current first-syscall simulator gate and no-phone reducer prerequisites were satisfied, but the static PIE relocation fix marker report was stale or failing at current `HEAD`.
- Simulator prerequisite refreshed on the single pinned simulator:
  - `Orlix-iPhone-15-Pro-Max`
  - UDID `C47ED88D-0D0A-420D-8C78-D4C1D34A276D`
  - `runtime-validation DESTINATION=iphonesimulator GATE=tcti-simulator-stability`
- Changed the marker gate to validate the structured `tcti_runtime_events.static_pie_image` fields from the simulator JSON report instead of requiring a unified-log string for the relocation marker.
- Kept the production scope check constrained to existing static PIE `R_AARCH64_RELATIVE` handling:
  - `tcti_apply_static_pie_relative_relocations`
  - `R_AARCH64_RELATIVE`
  - `TCTI_MAX_RELA_ENTRIES`
- Kept dynamic-loader expansion blocked by rejecting `PT_INTERP`, `DT_NEEDED`, and `R_AARCH64_JUMP_SLOT` markers in the TCTI engine.
- Treated the static PIE GOT null-read reducer as historical pass evidence. The reducer proves the failure class; it does not have to be regenerated at every later `HEAD` after the production fix and simulator stability pass.

Reports:

- `Build/Reports/runtime/tcti-init-first-syscall-20260703T084028Z-97596.json`
- `Build/Reports/runtime/tcti-simulator-stability-20260703T084428Z-9543.json`
- `Build/TCTI/reports/tcti-static-pie-relocation-fix/report.json`
- `Build/AgentHarness/orlix-tcti/status.json`
- `Build/AgentHarness/orlix-tcti/next-task.json`
- `Build/AgentHarness/orlix-tcti/next-task.md`

Boundary:

- No custom MCP added.
- No `tools/agent` added.
- No production assembly added.
- No gadget dispatch added.
- No physical-device gate run.
- No HostAdapter behavior added.
- No Darwin syscall behavior added.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No product defconfig flip.
- `agent-next` now selects `physical-tcti-init-first-syscall`, but this checkpoint deliberately stops before physical-device work.

### Checkpoint: Simulator First-Syscall Path Survives Static-PIE ADRP

- Harness-selected gate remains `physical-tcti-init-first-syscall`, but current operator scope required simulator-first validation only.
- Simulator used: `Orlix-iPhone-15-Pro-Max` (`C47ED88D-0D0A-420D-8C78-D4C1D34A276D`).
- Only that simulator was booted before the runtime run.
- Diagnosed the post-marker simulator panic after the first TCTI syscall marker:
  - previous fault address: `0x97ffb62f39083fff`;
  - faulting guest PC offset: `0x2a1f4`;
  - faulting instruction sequence:
    - `0xb00000c8` `adrp x8, 0x43000`;
    - `0xaa0003f3` `mov x19, x0`;
    - `0xf941b108` `ldr x8, [x8, #0x360]`;
    - `0x39400108` `ldrb w8, [x8]`;
  - bad value bytes came from `/init` file offset `0x21360`, guest VMA `0x31360`;
  - correct GOT slot was guest VMA `0x43360`, file offset `0x23360`, relocation `R_AARCH64_RELATIVE +0x550a8`.
- Root cause:
  - switch-debug TCTI executed ADRP with the kernel `PAGE_MASK`;
  - OrlixKernel uses 16 KiB pages, but AArch64 ADRP is architecturally based on a 4 KiB page;
  - the old calculation selected `0x41000 + 0x360`, which aliases the observed text bytes;
  - the fixed calculation uses a 4 KiB ADRP mask and selects `0x43000 + 0x360`.
- Added KUnit coverage for:
  - architectural 4 KiB ADRP page-base behavior on a 16 KiB-kernel build;
  - exact crash ADRP instruction `0xb00000c8`, producing `0x62548a583000`;
  - relocation-path register-offset store decode for `0xf8286920` (`str x0, [x9, x8]`).
- Added no-phone memory-fuzz coverage for the same fault shape:
  - positive case `vma-offset-alias-got-read`;
  - negative reducer `vma-offset-alias-broken-offset-selector`;
  - `vma_alias_cases=1`.

Reports:

- `Build/Reports/runtime/tcti-init-first-syscall-20260702T144818Z-20954.json`
- `Build/Reports/runtime/tcti-init-first-syscall-20260702T144818Z-20954.md`
- `Build/TCTI/reports/tcti-memory-fuzz/report.json`
- `Build/TCTI/memory_fuzz/vma-offset-alias-got-read/result.json`

Reducers:

- `Build/TCTI/reproducers/tcti-memory-fuzz/vma-offset-alias-broken-offset-selector.json`

Reducer replay:

- `make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-memory-fuzz/vma-offset-alias-broken-offset-selector.json`: expected `fail`, actual `fail`, exit code `0`.

Validation:

- `git diff --check`: pass.
- `make agent-harness-check`: pass.
- `make agent-task-envelope-check AREA=orlix-tcti`: pass.
- `make tcti-gate TARGET=tcti-plan-consistency`: pass.
- `make tcti-gate TARGET=tcti-report-schema-check`: pass.
- `make tcti-gate TARGET=tcti-toolchain-check`: pass.
- `make tcti-gate TARGET=tcti-golden-elf`: pass.
- `make tcti-gate TARGET=tcti-golden-elf CASE=init_001_exit EXECUTE=switch-debug`: pass.
- `make tcti-gate TARGET=tcti-memory-fuzz`: pass.
- `make tcti-gate TARGET=tcti-appstore-safety-audit`: pass.
- `make -f OrlixKernel/Makefile kunit PROFILE=development`: pass.
- `make -f OrlixKernel/Makefile kunit PROFILE=release`: pass.
- `make runtime-validation DESTINATION=iphonesimulator GATE=tcti-init-first-syscall`: pass on `Orlix-iPhone-15-Pro-Max`.

Boundary:

- No custom MCP added.
- No `tools/agent` added.
- No production TCTI assembly.
- No gadget dispatch.
- No generated executable memory.
- No host-executable guest text.
- No HostAdapter Linux behavior.
- No Darwin syscall behavior.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No physical-device gate run.
- No product defconfig flip.
- Simulator result is evidence-only: release and readiness gates remain ineligible.

### Checkpoint: Simulator TCTI Clears SIMD MOVI Zero Blocker

- Harness state: `agent-next` still selects `physical-tcti-init-first-syscall`, but active user constraint allows only simulator runtime validation on `Orlix-iPhone-15-Pro-Max` UDID `C47ED88D-0D0A-420D-8C78-D4C1D34A276D`.
- Implemented trace-driven TCTI runtime slice for the next observed simulator blocker after first `svc #0`.
- Previous simulator blocker:
  - `Orlix TCTI: unsupported instruction task=init pid=1 pc=0x79d6ab1f2f2c insn=0x6f00e400 ...`
  - Disassembly: `movi v0.2d, #0000000000000000`.
- Decoder change:
  - Added `TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE`.
  - Added narrow mask/predicate for only the observed `movi vN.2d, #0` class:
    - mask `0xffffffe0`
    - pattern `0x6f00e400`
  - `0x6f00e420` remains unsupported, proving nonzero modified immediates are not broadened by this checkpoint.
- Semantics change:
  - switch/data-program semantics zero both stored D lanes for the decoded SIMD register.
  - no FP arithmetic, vector arithmetic, host executable guest text, JIT, or production assembly added.
- Runtime validation:
  - command:
    - `rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" ORLIX_BUILD_ROOT="$PWD/Build" ORLIX_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D make runtime-validation DESTINATION=iphonesimulator GATE=tcti-init-first-syscall`
  - report:
    - `Build/Reports/runtime/tcti-init-first-syscall-20260702T131130Z-83995.json`
    - `Build/Reports/runtime/tcti-init-first-syscall-20260702T131130Z-83995.md`
  - artifact:
    - `Build/Reports/runtime/tcti-init-first-syscall-20260702T131130Z-83995.artifacts/simulator-unified.log`
  - result:
    - `status=pass`
    - `passed=true`
    - `destination=iphonesimulator`
    - `selected_device_id=C47ED88D-0D0A-420D-8C78-D4C1D34A276D`
    - `release_gate_eligible=false`
    - `readiness_gate_eligible=false`
    - forbidden behavior all false.
- Evidence:
  - old `insn=0x6f00e400` unsupported-instruction marker is gone from the new simulator log.
  - first TCTI syscall marker is still captured:
    - `Orlix TCTI: svc #0 task=init pid=1 pc=0xb3daeb76a80 syscall=178 x0=0xb2 x1=0xb3daeb754b4 x2=0x0 x3=0x0 x4=0x0 x5=0x0`
  - next concrete runtime blocker is now a user fault:
    - `Orlix TCTI: user fault task=init pid=1 pc=0xb3daeb8a1f4 lr=0xb3daeb89ffc sp=0xb3dbe787800 addr=0x97ffb62f39083fff access=1 si=1`
    - kernel panics because init receives SIGSEGV: `exitcode=0x0000000b`.
- Verification:
  - `rtk proxy git diff --check`
  - `rtk proxy make agent-harness-check`
  - `rtk proxy make agent-mcp-check`
  - `rtk proxy make tcti-gate TARGET=tcti-plan-consistency`
  - `rtk proxy make tcti-gate TARGET=tcti-toolchain-check`
  - `rtk proxy make tcti-gate TARGET=tcti-report-schema-check`
  - `rtk proxy make tcti-gate TARGET=tcti-golden-elf`
  - `rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit`
  - `rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" ORLIX_BUILD_ROOT="$PWD/Build" make -f OrlixKernel/Makefile kunit PROFILE=development`
  - `rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" ORLIX_BUILD_ROOT="$PWD/Build" make -f OrlixKernel/Makefile kunit PROFILE=release`
- Boundary:
  - No physical-device gate run.
  - No simulator other than `Orlix-iPhone-15-Pro-Max` used.
  - No production TCTI assembly.
  - No broad gadget dispatch.
  - No HostAdapter behavior.
  - No Darwin syscall behavior.
  - No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
  - No product defconfig flip.
  - No custom MCP.
  - No `tools/agent`.
  - TCTI remains incomplete. Next work is trace-driven investigation of the new TCTI user fault.

### Checkpoint: Simulator First Syscall Reaches TCTI

- Harness-selected gate: `physical-tcti-init-first-syscall`.
- Human runtime constraint for this checkpoint: do not run the physical device gate; use only the already-booted `Orlix-iPhone-15-Pro-Max` simulator, UDID `C47ED88D-0D0A-420D-8C78-D4C1D34A276D`.
- Diagnostic command:
  - `rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" ORLIX_BUILD_ROOT="$PWD/Build" ORLIX_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D make runtime-validation DESTINATION=iphonesimulator GATE=tcti-init-first-syscall`
- Result:
  - `status=pass`;
  - `passed=true`;
  - `profile=tcti_runtime`;
  - `destination=iphonesimulator`;
  - `selected_device_id=C47ED88D-0D0A-420D-8C78-D4C1D34A276D`;
  - `release_gate_eligible=false`;
  - `readiness_gate_eligible=false`.
- Runtime marker captured:
  - `Orlix TCTI: svc #0 task=init pid=1 pc=0x79d6ab1e6a80 syscall=178 x0=0xb2 x1=0x79d6ab1e54b4 x2=0x0 x3=0x0 x4=0x0 x5=0x0`
- Report path:
  - `Build/Reports/runtime/tcti-init-first-syscall-20260702T124435Z-42167.json`
- Artifact paths:
  - `Build/Reports/runtime/tcti-init-first-syscall-20260702T124435Z-42167.md`
  - `Build/Reports/runtime/tcti-init-first-syscall-20260702T124435Z-42167.artifacts/tcti-first-syscall.txt`
  - `Build/Reports/runtime/tcti-init-first-syscall-20260702T124435Z-42167.artifacts/simulator-unified.log`
  - `Build/Reports/runtime/tcti-init-first-syscall-20260702T124435Z-42167.artifacts/host-exec-violations.txt`
- `host-exec-violations.txt` is empty for this run.

Implementation details:

- Added a TCTI-owned Linux MM fault-in helper:
  - `tcti_handle_user_fault(struct pt_regs *regs, unsigned long address, enum tcti_access access)`.
- TCTI user faults now carry access class in `struct tcti_result`:
  - `TCTI_ACCESS_FETCH`;
  - `TCTI_ACCESS_READ`;
  - `TCTI_ACCESS_WRITE`.
- TCTI fetch faults now fault in executable Linux user pages through `handle_mm_fault()` and retry TCTI without calling native hosted mapping synchronization.
- TCTI does not call `orlix_sync_current_user_fault_window()` for guest instruction fetch.
- Runtime validation now captures bounded simulator unified logs into `simulator-unified.log` and uses that sanctioned artifact to find `Orlix TCTI: svc #0`.
- Simulator runtime passes remain diagnostic only; runtime JSON no longer marks `iphonesimulator` passes as release/readiness eligible.
- Added minimal decoded support for the observed `/init` prologue instruction:
  - raw instruction: `0x6d0123e9`;
  - disassembly: `stp d9, d8, [sp, #0x10]`;
  - decoder class: `TCTI_DECODE_LOAD_STORE_PAIR`;
  - `simd_fp=true`;
  - `access_size=8`;
  - `memory_index_mode=TCTI_MEMORY_INDEX_SIGNED_OFFSET`.
- Switch-debug/TCTI semantics now support D-register FP/SIMD pair loads and stores through the existing TCTI memory read/write path.

Verification:

```text
rtk proxy git diff --check
rtk proxy bash -n tools/runtime/orlix-runtime-validation.sh
rtk proxy make agent-harness-check
rtk proxy make agent-status AREA=orlix-tcti
rtk proxy make agent-next AREA=orlix-tcti
rtk proxy make agent-task-envelope-check AREA=orlix-tcti
rtk proxy make tcti-gate TARGET=tcti-plan-consistency
rtk proxy make tcti-gate TARGET=tcti-toolchain-check
rtk proxy make tcti-gate TARGET=tcti-report-schema-check
rtk proxy make tcti-gate TARGET=tcti-golden-elf
rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit
rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" ORLIX_BUILD_ROOT="$PWD/Build" make -f OrlixKernel/Makefile kunit PROFILE=development
rtk test env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" ORLIX_BUILD_ROOT="$PWD/Build" make -f OrlixKernel/Makefile kunit PROFILE=release
rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" ORLIX_BUILD_ROOT="$PWD/Build" ORLIX_SIMULATOR_ID=C47ED88D-0D0A-420D-8C78-D4C1D34A276D make runtime-validation DESTINATION=iphonesimulator GATE=tcti-init-first-syscall
```

Results:

- All commands above exited 0.
- The only booted simulator during the final runtime check was `Orlix-iPhone-15-Pro-Max (C47ED88D-0D0A-420D-8C78-D4C1D34A276D)`.
- `agent-next` still selects `physical-tcti-init-first-syscall`; this checkpoint does not satisfy that physical certification gate.

Boundary:

- No custom MCP added.
- No `tools/agent` added.
- No production TCTI assembly.
- No new gadget dispatch.
- No physical-device gate run.
- No HostAdapter Linux behavior.
- No Darwin syscall guest side effect.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added outside Linux.
- No generated executable memory.
- No host-executable guest text.
- No `MAP_JIT`, RWX, or `vm_protect(... EXECUTE ...)` guest text path.
- No product defconfig flip.
- Simulator proof does not make release/readiness eligible.

### Checkpoint: TCTI Runtime-Proof Kernel Profile

- Harness-selected gate: `physical-tcti-init-first-syscall`.
- Selected command: `make runtime-validation DESTINATION=iphoneos GATE=tcti-init-first-syscall`.
- Why selected: all no-phone TCTI prerequisites in the agent status report were passing, so the first missing roadmap gate was physical first-syscall certification.
- Fixed a real gate correctness gap before rerunning the physical gate: TCTI runtime validation now defaults `tcti-*` gates to the non-product `tcti_runtime` kernel profile instead of the product `development` profile.
- Added `OrlixKernel/Sources/ports/orlix/configs/tcti_runtime_defconfig` with `CONFIG_ORLIX_HOSTED_EXEC_TCTI=y` and `CONFIG_ORLIX_HOSTED_EXEC_NATIVE` unset.
- Added `tcti_runtime` to `ORLIX_PROFILES` so the kernel archive build can produce a TCTI-enabled proof profile without flipping product `development` or `release` defaults.
- Updated runtime validation to assert that TCTI runtime gates inspect a generated `.config` with TCTI enabled and native hosted execution disabled before app build/install/launch can proceed.
- Built the physical-platform archive successfully:
  - `Build/OrlixKernel/tcti_runtime/iphoneos/OrlixKernel.a`
  - `Build/OrlixKernel/tcti_runtime/linux-object-manifest.txt`
- Generated config evidence:
  - `# CONFIG_ORLIX_HOSTED_EXEC_NATIVE is not set`
  - `CONFIG_ORLIX_HOSTED_EXEC_TCTI=y`
  - `CONFIG_ORLIX_TCTI_DEBUG_SWITCH=y`
- Runtime-validation report:
  - `Build/Reports/runtime/tcti-init-first-syscall-20260702T105238Z-62387.json`
  - `status=fail`
  - `passed=false`
  - `profile=tcti_runtime`
  - `release_gate_eligible=false`
  - `readiness_gate_eligible=false`
  - failure occurred before app build/install/launch because the physical iPhone developer disk image services were unavailable.
- Device blocker:
  - device name: `RRJ-iPhone-15-Pro-Max`
  - CoreDevice id: `7F8A1701-D612-5A9C-AAE7-8FD0AD77306C`
  - Xcode device id: `00008130-001E74A11193803A`

Verification:

```text
rtk proxy git diff --check
rtk proxy bash -n tools/runtime/orlix-runtime-validation.sh
rtk proxy make agent-harness-check
rtk proxy make agent-status AREA=orlix-tcti
rtk proxy make agent-next AREA=orlix-tcti
rtk proxy make agent-task-envelope-check AREA=orlix-tcti
rtk proxy make tcti-gate TARGET=tcti-plan-consistency
rtk proxy make tcti-gate TARGET=tcti-report-schema-check
rtk proxy make tcti-gate TARGET=tcti-toolchain-check
rtk proxy make tcti-gate TARGET=tcti-golden-elf
rtk proxy make tcti-gate TARGET=tcti-golden-elf CASE=init_001_exit EXECUTE=switch-debug
rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit
rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" ORLIX_BUILD_ROOT="$PWD/Build" make -f OrlixKernel/Makefile __kernel-archive PROFILE=tcti_runtime ORLIX_KERNEL_ARCHIVE_PLATFORMS=iphoneos
rtk proxy env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" ORLIX_BUILD_ROOT="$PWD/Build" make runtime-validation DESTINATION=iphoneos GATE=tcti-init-first-syscall
```

Results:

- Static, harness, no-phone TCTI, toolchain, schema, and App Store safety checks passed.
- `tcti_runtime` `iphoneos` kernel archive built successfully.
- Physical runtime validation failed before TCTI execution because the device DDI service was unavailable.

Boundary:

- No custom MCP added.
- No `tools/agent` added.
- No production TCTI assembly.
- No new gadget dispatch.
- No generated executable memory.
- No host-executable guest text.
- No simulator gate run in this checkpoint.
- Physical gate did not pass.
- No HostAdapter behavior.
- No Darwin syscall behavior.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No product defconfig flip.

### Correction: init_006_memory Reducer Replay Set

- Correct reducer lane result for harness-selected gate `switch-init-006-memory`.
- Replayed negative reducers:
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-memory-invalid-read.json`.
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-memory-unsupported-ldur.json`.
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-memory-unsupported-store.json`.
- Replay results:
  - `execution-memory-invalid-read.json`: expected `fail`, actual `fail`, replay exit code `2`.
  - `execution-memory-unsupported-ldur.json`: expected `fail`, actual `fail`, replay exit code `2`.
  - `execution-memory-unsupported-store.json`: expected `fail`, actual `fail`, replay exit code `2`.
- Fixture sources:
  - `tools/tcti/fixtures/golden_elf/init_006_memory_invalid_read.S`.
  - `tools/tcti/fixtures/golden_elf/init_006_memory_unsupported_ldur.S`.
  - `tools/tcti/fixtures/golden_elf/init_006_memory_unsupported_store.S`.
- Boundary:
  - No simulator or physical-device gate was run.
  - No production TCTI assembly or gadget dispatch was added.
  - No HostAdapter, Darwin, VFS, fd table, process, signal, scheduler, or Linux runtime semantics were changed.
  - No reducer-lane edit was made to `tools/tcti/orlix-tcti-gate.swift`.

### Reducer Lane: init_006_memory Negative Fixture Scope

- Reducer lane for harness-selected gate: `switch-init-006-memory`.
- Positive switch-debug execution was run after the oracle rail update and passed:
  - `make tcti-gate TARGET=tcti-golden-elf CASE=init_006_memory EXECUTE=switch-debug`.
  - report: `Build/TCTI/reports/tcti-golden-elf/report.json`.
  - execution artifact: `Build/TCTI/golden_elf/init_006_memory/execution.json`.
- Smallest replayed negative fixture set for the memory gate:
  - invalid guest memory read from an unmapped guest address.
  - unsupported file-backed guest memory store.
- Added reducer-lane fixture sources:
  - `tools/tcti/fixtures/golden_elf/init_006_memory_invalid_read.S`.
  - `tools/tcti/fixtures/golden_elf/init_006_memory_unsupported_ldur.S`.
- Parallel oracle lane supplied and wired:
  - `tools/tcti/fixtures/golden_elf/init_006_memory_unsupported_store.S`.
- Verified reducer replay:
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-memory-invalid-read.json`.
  - original command: `CASE=init_006_memory EXECUTE=switch-debug NEGATIVE_EXECUTION=memory-invalid-read make tcti-gate TARGET=tcti-golden-elf`.
  - expected `fail`, actual `fail`, replay exit code `2`.
  - failure id: `execution-memory-read`.
  - reason: guest memory read outside file-backed `PT_LOAD` at address `0x0`, length `8`.
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-memory-unsupported-store.json`.
  - original command: `CASE=init_006_memory EXECUTE=switch-debug NEGATIVE_EXECUTION=memory-unsupported-store make tcti-gate TARGET=tcti-golden-elf`.
  - expected `fail`, actual `fail`, replay exit code `2`.
  - failure id: `execution-unsupported-instruction`.
  - reason: file-backed `STR` remains unsupported in the seed switch-debug memory fixture.

Boundary:

- No `tools/tcti/orlix-tcti-gate.swift` edits were made by this reducer lane.
- No production TCTI assembly.
- No gadget dispatch.
- No simulator gate.
- No physical-device gate.
- No HostAdapter behavior.
- No Darwin syscall behavior.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No generated executable memory.
- No host-executable guest text.
- No product defconfig flip.

Boundary:

- No production TCTI assembly.
- No gadget dispatch.
- No simulator gate.
- No physical-device gate.
- No HostAdapter behavior.
- No Darwin syscall behavior.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No generated executable memory.
- No host-executable guest text.
- No product defconfig flip.

### Checkpoint: Branches Golden ELF Structural Gate

- Harness-selected gate: `golden-init-005-branches-structural`.
- Selected command: `make tcti-gate TARGET=tcti-golden-elf CASE=init_005_branches`.
- Why selected: the agent roadmap was corrected to keep no-phone golden/switch-debug corpus work ahead of diff, gadget, and physical gates. After `switch-init-004-tls` passed, the next missing no-phone gate became `golden-init-005-branches-structural`.
- Added roadmap gates:
  - `golden-init-005-branches-structural`
  - `switch-init-005-branches`
- Updated the next-step status script so structural golden gates map to their case ids through skill-owned roadmap/status logic.
- Added no-libc AArch64 Linux source:
  - `OrlixKernel/Tests/TCTI/golden_elf/init_005_branches/init_005_branches.S`
- Added canonical metadata:
  - `OrlixKernel/Tests/TCTI/golden_elf/init_005_branches/golden.json`
- Structural fixture shape:
  - `mov x0, #0`
  - `cbz x0, taken`
  - untaken path sets `x0=1`
  - unconditional `b` skips the taken path
  - taken path sets `x0=42`
  - `mov x8, #93`
  - `svc #0`
- Exact emitted instruction words:
  - `0xd2800000` `mov x0, #0`
  - `0xb4000060` `cbz x0, 0x210130`
  - `0xd2800020` `mov x0, #1`
  - `0x14000002` `b 0x210134`
  - `0xd2800540` `mov x0, #42`
  - `0xd2800ba8` `mov x8, #93`
  - `0xd4000001` `svc #0`
- Metadata hashes:
  - source SHA256 `cb08adf22c77708447b8210881e2001f8473fcd226087af383cfc5611e1fdebb`
  - binary SHA256 `2a72efc65d627a85f9b88edba2a9e4a52faedffb84f4fdad52fbaabbef7f6ba4`
- Validation artifacts:
  - `Build/TCTI/reports/tcti-golden-elf/report.json`
  - `Build/TCTI/golden_elf/init_005_branches/validation.json`
- The structural gate passed. The next harness-selected gate is expected to be `switch-init-005-branches`.

Verification:

- `jq empty .agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json` passed.
- `swiftc -parse .agents/skills/orlix-tcti-next-step/scripts/tcti-next-step.swift` passed.
- `swiftc -parse tools/tcti/orlix-tcti-gate.swift` passed.
- `make tcti-gate TARGET=tcti-golden-elf-refresh CASE=init_005_branches` passed.
- `make tcti-gate TARGET=tcti-golden-elf CASE=init_005_branches` passed.
- `git diff --check` passed.
- `make agent-harness-check` passed.
- `make tcti-gate TARGET=tcti-report-schema-check` passed.
- `make tcti-gate TARGET=tcti-appstore-safety-audit` passed.

Boundary:

- Structural-only gate. No branch execution was claimed.
- No production TCTI assembly.
- No gadget dispatch.
- No simulator gate.
- No physical-device gate.
- No HostAdapter behavior.
- No Darwin syscall behavior.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No generated executable memory.
- No host-executable guest text.
- No product defconfig flip.
- No custom MCP.
- No `tools/agent`.

### Checkpoint: Physical First-Syscall Gate Blocked By Device DDI Readiness

- Harness-selected gate: `physical-tcti-init-first-syscall`.
- Selected command: `make runtime-validation DESTINATION=iphoneos GATE=tcti-init-first-syscall`.
- `agent-status` and `agent-next` selected the physical first-syscall gate because all no-phone prerequisites were current and passing at `0b1c74953535051285b4d879c439a6fed4ac7de1`.
- Read-only planner, safety, release-gate, and reducer lanes reviewed the envelope. They allowed the physical certification run, but release/readiness remained blocked until a non-override passing runtime JSON exists.
- First runtime attempt produced `Build/Reports/runtime/tcti-init-first-syscall-20260701T223106Z-14097.json` and failed before install or guest execution because `xcodebuild` could not use the physical destination: Xcode reported that the developer disk image could not be mounted on `RRJ-iPhone-15-Pro-Max`.
- Device discovery showed the iPhone was paired, physical, Developer Mode enabled, and iOS, but `ddiServicesAvailable=false`.
- Updated `tools/runtime/orlix-runtime-validation.sh` to keep the CoreDevice identifier used by `devicectl` separate from the hardware UDID used by `xcodebuild`.
- Updated `tools/runtime/orlix-runtime-validation.sh` to fail before kernel/app build when a discovered physical iPhone reports unavailable developer disk image services.
- Rerunning the selected gate failed fast with `Build/Reports/runtime/tcti-init-first-syscall-20260701T224306Z-62528.json`.
- The fast-fail report is explicit: `status=fail`, `passed=false`, `autonomous_tests_bypassed=false`, readiness/release eligibility false, and summary says the physical iPhone is not ready for Xcode device builds because developer disk image services are unavailable.
- `devices.txt` for the fast-fail run recorded CoreDevice id `7F8A1701-D612-5A9C-AAE7-8FD0AD77306C`, Xcode UDID `00008130-001E74A11193803A`, `ddiServicesAvailable=false`, and device name `RRJ-iPhone-15-Pro-Max`.

Verification:

- `bash -n tools/runtime/orlix-runtime-validation.sh` passed.
- `make runtime-validation DESTINATION=iphoneos GATE=tcti-init-first-syscall` failed fast as expected with report `Build/Reports/runtime/tcti-init-first-syscall-20260701T224306Z-62528.md`.

Boundary:

- Physical gate did not pass.
- No runtime TCTI semantics were changed.
- No production TCTI assembly.
- No gadget dispatch.
- No simulator gate.
- No emergency override or evidence-mode pass.
- No HostAdapter behavior.
- No Darwin syscall behavior.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No product defconfig flip.
- No custom MCP.
- No `tools/agent`.
- Next action is to make the physical device/Xcode DDI service available, then rerun the harness-selected physical gate.

### Checkpoint: No-Phone Direct-Chain Fuzz Gate

- Harness-selected gate: `tcti-direct-chain-fuzz`.
- Selected command: `make tcti-gate TARGET=tcti-direct-chain-fuzz`.
- Why selected: `tcti-memory-fuzz` passed and the harness selected the next runtime-preflight gate. Physical device, release, and readiness gates remained ineligible before this checkpoint.
- Implemented the gate as a no-phone Swift/Foundation data-structure contract in `tools/tcti/orlix-tcti-gate.swift`.
- Positive contracts covered:
  - source outgoing patch slots and target incoming patch slots.
  - same-page source-to-target chain patching.
  - page-index overlap lookup for a block spanning two guest pages.
  - invalidation removes outgoing and incoming patch slots before retiring blocks.
  - invalidation advances translation and code generations.
  - same-page chaining works before broader cross-page chaining.
- Negative reducers added:
  - `Build/TCTI/reproducers/tcti-direct-chain-fuzz/stale-target-after-retire.json`
  - `Build/TCTI/reproducers/tcti-direct-chain-fuzz/page-index-overlap-miss.json`
  - `Build/TCTI/reproducers/tcti-direct-chain-fuzz/retire-with-patched-incoming.json`
  - `Build/TCTI/reproducers/tcti-direct-chain-fuzz/duplicate-outgoing-patch.json`
  - `Build/TCTI/reproducers/tcti-direct-chain-fuzz/direct-chain-fuzz-pass-regression.json`
- Report path:
  - `Build/TCTI/reports/tcti-direct-chain-fuzz/report.json`
- Per-case artifacts:
  - `Build/TCTI/direct_chain_fuzz/*/result.json`

Evidence so far:

```text
rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift
rtk proxy make tcti-gate TARGET=tcti-direct-chain-fuzz
```

Full checkpoint verification:

```text
rtk proxy git diff --check
rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift
rtk proxy make agent-harness-check
rtk proxy make tcti-gate TARGET=tcti-plan-consistency
rtk proxy make tcti-gate TARGET=tcti-toolchain-check
rtk proxy make tcti-gate TARGET=tcti-golden-elf
rtk proxy make tcti-gate TARGET=tcti-diff-switch CASE=init_001_exit BACKEND=gadget
rtk proxy make tcti-gate TARGET=tcti-appstore-safety-audit
rtk proxy make tcti-gate TARGET=tcti-contract
rtk proxy make tcti-gate TARGET=tcti-report-schema-check
rtk proxy make tcti-gate TARGET=tcti-direct-chain-fuzz
rtk proxy make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-direct-chain-fuzz/direct-chain-fuzz-pass-regression.json
rtk proxy sh -c 'make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-direct-chain-fuzz/stale-target-after-retire.json; rc=$?; echo rc=$rc; exit 0'
rtk proxy make agent-status AREA=orlix-tcti
rtk proxy make agent-next AREA=orlix-tcti
rtk proxy make agent-task-envelope-check AREA=orlix-tcti
```

Results:

- All commands above exited 0 except the expected negative reducer replay wrapper, which reported `rc=2` from the replayed failing fixture and exited 0.
- `agent-status` reported that no-phone preflight permits the next runtime-certification envelope.
- `agent-next` wrote the next runtime-certification envelope under `Build/AgentHarness/orlix-tcti/next-task.json`.
- Release and readiness gates remained false because the runtime-certification report is still missing.

Boundary:

- No custom MCP added.
- No `tools/agent` added.
- No production TCTI assembly.
- No gadget dispatch.
- No generated executable memory.
- No host-executable guest text.
- No simulator gate run.
- No phone gate run.
- No HostAdapter behavior.
- No Darwin syscall behavior.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No product defconfig flip.
- Release and readiness gates remain ineligible pending harness status after verification.

### Checkpoint: No-Phone Contract Gate Passes

- Harness-selected gate: `tcti-contract`.
- Selected command: `make tcti-gate TARGET=tcti-contract`.
- Why selected: `first-gadget-init-001-exit` passed and `tcti-contract` was the first non-passing runtime-preflight roadmap gate.
- Implemented the smallest missing contract group:
  - gadget data-program ABI and `x0`/`x8` register commit-back for `init_001_exit`.
- The contract validates the existing no-phone switch-vs-gadget evidence:
  - reference backend: `switch-debug`;
  - candidate backend: `gadget-data-program`;
  - mode: `contract-gadget-abi-register-commit-back`;
  - `gadget_dispatch_executed=true`;
  - `production_assembly_executed=false`;
  - divergent fields: none;
  - checked fields: `gprs.x0`, `gprs.x8`, `sp`, `pc`, `pstate_nzcv`, `tpidr_el0`, `memory_writes`, `exit.kind`, `exit.code`, and `fault_address`;
  - candidate state commits `x0=42`, `x8=93`, `pc=0x0000000000210128`, and `exit_code=42`.
- The remaining runtime-preflight surfaces are not marked as pass inside `tcti-contract`; they are delegated to their own harness gates:
  - `tcti-memory-fuzz` for `FETCH`/`READ`/`WRITE` memory execution;
  - `tcti-direct-chain-fuzz` for TLB, block-cache, invalidation, and direct-chain execution.
- Updated the next-step harness status predicate so a later `switch-vs-gadget` diff report still satisfies the earlier `diff-switch-init-001-exit` baseline gate when the switch-debug reference backend, required fields, and zero divergent fields are present.

Reports:

- `Build/TCTI/reports/tcti-contract/report.json`
- `Build/TCTI/contract/gadget_abi/init_001_exit/execution.json`
- `Build/TCTI/contract/gadget_abi/init_001_exit/switch-state.json`
- `Build/TCTI/contract/gadget_abi/init_001_exit/candidate-state.json`
- `Build/TCTI/contract/gadget_abi/init_001_exit/diff.json`

Reducers:

- `Build/TCTI/reproducers/tcti-contract/contract-pass-regression.json`
- `Build/TCTI/reproducers/tcti-diff-switch/gadget-abi-init-001-exit-x0-divergence.json`

Reducer replay:

- `make tcti-gate TARGET=tcti-repro REPRO=Build/TCTI/reproducers/tcti-contract/contract-pass-regression.json`: expected `pass`, actual `pass`, exit code `0`.

Harness state:

- `agent-status` now reports `tcti-contract` as passing.
- `agent-next` advances to `tcti-memory-fuzz`.
- `physical_device_allowed=false`.
- `release_gate_eligible=false`.
- `readiness_gate_eligible=false`.

Boundary:

- No custom MCP added.
- No `tools/agent` added.
- No production TCTI assembly.
- No generated executable memory.
- No host-executable guest text.
- No simulator gate run.
- No phone gate run.
- No HostAdapter behavior.
- No Darwin syscall behavior.
- No VFS, fd table, process, signal, scheduler, or Linux runtime semantics added.
- No product defconfig flip.
- Release and readiness gates remain ineligible.
