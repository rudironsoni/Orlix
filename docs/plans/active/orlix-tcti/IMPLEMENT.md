# IMPLEMENT.md

## 2026-07-02

### Checkpoint: First Gadget Exit Diff Passes And Physical Gate Reaches DDI Blocker

- Harness-selected gate after fresh no-phone validation: `first-gadget-init-001-exit`.
- Selected command:
  - `make tcti-diff-switch CASE=init_001_exit BACKEND=gadget`.
- Roadmap correction:
  - Updated `.agents/skills/orlix-tcti-next-step/references/tcti-roadmap.json` so the selected first-gadget gate validates with `rtk proxy make tcti-diff-switch CASE=init_001_exit BACKEND=gadget`.
  - The prior validation command `rtk proxy make tcti-diff-switch` rewrote `Build/TCTI/diff_switch/init_001_exit/diff.json` back to switch-debug baseline mode, which made the first-gadget pass artifact disappear from harness status.
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
  - `make tcti-repro REPRO=Build/TCTI/reproducers/tcti-diff-switch/init_001_exit-gadget-x0-divergence.json`.
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
- Selected command: `make tcti-appstore-safety-audit`.
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
- Replay result: `make tcti-repro REPRO=Build/TCTI/reproducers/tcti-appstore-safety-audit/appstore-safety-pass-regression.json` produced expected status `pass`, actual status `pass`, exit code `0`.

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
rtk proxy make tcti-plan-consistency
rtk proxy make tcti-report-schema-check
rtk proxy make tcti-toolchain-check
rtk proxy make tcti-golden-elf
rtk proxy make tcti-golden-elf CASE=init_001_exit EXECUTE=switch-debug
rtk proxy make tcti-appstore-safety-audit
rtk proxy make tcti-repro REPRO=Build/TCTI/reproducers/tcti-appstore-safety-audit/appstore-safety-pass-regression.json
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
  - `make tcti-diff-switch CASE=init_001_exit BACKEND=gadget`.
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
- Selected command: `make tcti-golden-elf CASE=init_010_cpu_model EXECUTE=switch-debug`.
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
  - `make tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-cpu-model-unsupported-ctr-el0.json`.
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
- Selected command: `make tcti-golden-elf CASE=init_010_cpu_model`.
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
- Selected command: `make tcti-golden-elf CASE=init_006_memory`.
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
- Selected command: `make tcti-golden-elf CASE=init_006_memory EXECUTE=switch-debug`.
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
  - `make tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-memory-invalid-read.json` produced expected `fail`, actual `fail`, replay exit code `2`.

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
- Selected command: `make tcti-golden-elf CASE=init_005_branches EXECUTE=switch-debug`.
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
  - `make tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-branches-unsupported-cbnz.json` produced expected `fail`, actual `fail`, replay exit code `2`.
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

### Checkpoint: No-Phone Memory Fuzz Gate

- Harness-selected gate: `tcti-memory-fuzz`.
- Selected command: `make tcti-memory-fuzz`.
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
rtk proxy make tcti-memory-fuzz
```

Full checkpoint verification:

```text
rtk proxy git diff --check
rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift
rtk proxy make agent-harness-check
rtk proxy make tcti-plan-consistency
rtk proxy make tcti-toolchain-check
rtk proxy make tcti-golden-elf
rtk proxy make tcti-golden-elf CASE=init_001_exit EXECUTE=switch-debug
rtk proxy make tcti-diff-switch CASE=init_001_exit BACKEND=gadget
rtk proxy make tcti-appstore-safety-audit
rtk proxy make tcti-contract
rtk proxy make tcti-report-schema-check
rtk proxy make tcti-memory-fuzz
rtk proxy make tcti-repro REPRO=Build/TCTI/reproducers/tcti-memory-fuzz/memory-fuzz-pass-regression.json
rtk proxy sh -c 'make tcti-repro REPRO=Build/TCTI/reproducers/tcti-memory-fuzz/fetch-exec-permission.json; rc=$?; echo rc=$rc; exit 0'
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
- Selected command: `make tcti-golden-elf CASE=init_003_stack EXECUTE=switch-debug`.
- Why selected: `switch-init-002-write` was pass and `init_003_stack` validation/execution artifacts were missing.
- Added `init_003_stack` no-libc AArch64 Linux golden ELF fixture.
- Generated canonical `golden.json` with `make tcti-golden-elf-refresh CASE=init_003_stack`.
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
  - `make tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-stack-invalid-memory.json`: expected `fail`, actual `fail`, nonzero replay exit.
  - `make tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-stack-unsupported-preindex.json`: expected `fail`, actual `fail`, nonzero replay exit.
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
  - command `make tcti-golden-elf CASE=init_004_tls EXECUTE=switch-debug`

Evidence:

```sh
rtk proxy git diff --check
rtk proxy make agent-harness-check
rtk proxy make agent-status AREA=orlix-tcti
rtk proxy make agent-next AREA=orlix-tcti
rtk proxy make agent-task-envelope-check AREA=orlix-tcti
rtk proxy make tcti-plan-consistency
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
- Selected command: `make tcti-golden-elf CASE=init_004_tls EXECUTE=switch-debug`.
- Why selected: `switch-init-003-stack` passed and `init_004_tls` validation/execution artifacts were missing.
- Added `init_004_tls` no-libc AArch64 Linux golden ELF fixture.
- Generated canonical `golden.json` with `make tcti-golden-elf-refresh CASE=init_004_tls`.
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

- `make tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-tls-unsupported-sysreg.json`: expected `fail`, actual `fail`, exit code `2`.
- `make tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-tls-wrong-exit.json`: expected `fail`, actual `fail`, exit code `2`.

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
rtk proxy make tcti-plan-consistency
rtk proxy make tcti-report-schema-check
rtk proxy make tcti-toolchain-check
rtk proxy make tcti-golden-elf
rtk proxy make tcti-golden-elf CASE=init_004_tls
rtk proxy make tcti-golden-elf CASE=init_004_tls EXECUTE=switch-debug
rtk proxy make tcti-appstore-safety-audit
rtk proxy sh -c 'make tcti-contract; rc=$?; echo rc=$rc; exit 0'
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
- Selected command: `make tcti-diff-switch CASE=init_001_exit`.
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

- `make tcti-repro REPRO=Build/TCTI/reproducers/tcti-diff-switch/init_001_exit-exit-code-divergence.json`: expected `fail`, actual `fail`, exit code `2`.

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
rtk proxy make tcti-plan-consistency
rtk proxy make tcti-report-schema-check
rtk proxy make tcti-toolchain-check
rtk proxy make tcti-golden-elf
rtk proxy make tcti-golden-elf CASE=init_001_exit EXECUTE=switch-debug
rtk proxy make tcti-diff-switch
rtk proxy make tcti-appstore-safety-audit
rtk proxy sh -c 'make tcti-repro REPRO=Build/TCTI/reproducers/tcti-diff-switch/init_001_exit-exit-code-divergence.json; rc=$?; echo rc=$rc; exit 0'
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
- Selected command: `make tcti-diff-switch CASE=init_001_exit BACKEND=gadget`.
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
- `make tcti-repro REPRO=Build/TCTI/reproducers/tcti-diff-switch/init_001_exit-gadget-x0-divergence.json` replayed expected `fail`, actual `fail`, exit code `2`.

Harness state checkpoint:

- `agent-status` recognizes `first-gadget-init-001-exit` passed.
- `agent-next` advances to the next certification gate.
- Release and readiness gates remain ineligible.

### Checkpoint: Structural Golden `init_007_mprotect`

- Harness-selected gate: `golden-init-007-mprotect-structural`.
- Selected command: `make tcti-golden-elf CASE=init_007_mprotect`.
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
rtk proxy make tcti-golden-elf-refresh CASE=init_007_mprotect
rtk proxy make tcti-golden-elf CASE=init_007_mprotect
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
- Selected command: `make tcti-golden-elf CASE=init_007_mprotect EXECUTE=switch-debug`.
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
rtk proxy sh -c 'make tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-mprotect-exec-prot.json; rc=$?; echo rc=$rc; exit 0'
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
rtk proxy make tcti-golden-elf CASE=init_007_mprotect EXECUTE=switch-debug
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
- Selected command: `make tcti-golden-elf CASE=init_008_self_modify`.
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
rtk proxy make tcti-golden-elf-refresh CASE=init_008_self_modify
rtk proxy make tcti-golden-elf CASE=init_008_self_modify
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
- Selected command: `make tcti-golden-elf CASE=init_008_self_modify EXECUTE=switch-debug`.
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
rtk proxy sh -c 'make tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-self-modify-invalid-write.json; rc=$?; echo rc=$rc; exit 0'
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
rtk proxy make tcti-golden-elf CASE=init_008_self_modify EXECUTE=switch-debug
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
- Selected command: `make tcti-golden-elf CASE=init_009_faults`.
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
rtk proxy make tcti-golden-elf-refresh CASE=init_009_faults
rtk proxy make tcti-golden-elf CASE=init_009_faults
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
- Selected command: `make tcti-golden-elf CASE=init_009_faults EXECUTE=switch-debug`.
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
rtk proxy sh -c 'make tcti-repro REPRO=Build/TCTI/reproducers/tcti-golden-elf/execution-faults-wrong-address.json; rc=$?; echo rc=$rc; exit 0'
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
rtk proxy make tcti-golden-elf CASE=init_009_faults EXECUTE=switch-debug
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
rtk proxy make tcti-plan-consistency
rtk proxy make tcti-report-schema-check
rtk proxy make tcti-toolchain-check
rtk proxy make tcti-golden-elf
rtk proxy make tcti-golden-elf CASE=init_001_exit EXECUTE=switch-debug
rtk proxy make tcti-diff-switch
rtk proxy make tcti-diff-switch CASE=init_001_exit BACKEND=gadget
rtk proxy make tcti-appstore-safety-audit
rtk proxy sh -c "make tcti-repro REPRO=Build/TCTI/reproducers/tcti-diff-switch/init_001_exit-gadget-x0-divergence.json; rc=$?; echo rc=$rc; exit 0"
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
rtk proxy make tcti-plan-consistency
rtk proxy make tcti-report-schema-check
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
  - `make tcti-golden-elf CASE=init_006_memory EXECUTE=switch-debug`.
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
  - original command: `CASE=init_006_memory EXECUTE=switch-debug NEGATIVE_EXECUTION=memory-invalid-read make tcti-golden-elf`.
  - expected `fail`, actual `fail`, replay exit code `2`.
  - failure id: `execution-memory-read`.
  - reason: guest memory read outside file-backed `PT_LOAD` at address `0x0`, length `8`.
  - `Build/TCTI/reproducers/tcti-golden-elf/execution-memory-unsupported-store.json`.
  - original command: `CASE=init_006_memory EXECUTE=switch-debug NEGATIVE_EXECUTION=memory-unsupported-store make tcti-golden-elf`.
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
- Selected command: `make tcti-golden-elf CASE=init_005_branches`.
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
- `make tcti-golden-elf-refresh CASE=init_005_branches` passed.
- `make tcti-golden-elf CASE=init_005_branches` passed.
- `git diff --check` passed.
- `make agent-harness-check` passed.
- `make tcti-report-schema-check` passed.
- `make tcti-appstore-safety-audit` passed.

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
- Selected command: `make tcti-direct-chain-fuzz`.
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
rtk proxy make tcti-direct-chain-fuzz
```

Full checkpoint verification:

```text
rtk proxy git diff --check
rtk proxy swiftc -parse tools/tcti/orlix-tcti-gate.swift
rtk proxy make agent-harness-check
rtk proxy make tcti-plan-consistency
rtk proxy make tcti-toolchain-check
rtk proxy make tcti-golden-elf
rtk proxy make tcti-diff-switch CASE=init_001_exit BACKEND=gadget
rtk proxy make tcti-appstore-safety-audit
rtk proxy make tcti-contract
rtk proxy make tcti-report-schema-check
rtk proxy make tcti-direct-chain-fuzz
rtk proxy make tcti-repro REPRO=Build/TCTI/reproducers/tcti-direct-chain-fuzz/direct-chain-fuzz-pass-regression.json
rtk proxy sh -c 'make tcti-repro REPRO=Build/TCTI/reproducers/tcti-direct-chain-fuzz/stale-target-after-retire.json; rc=$?; echo rc=$rc; exit 0'
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
- Selected command: `make tcti-contract`.
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

- `make tcti-repro REPRO=Build/TCTI/reproducers/tcti-contract/contract-pass-regression.json`: expected `pass`, actual `pass`, exit code `0`.

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
