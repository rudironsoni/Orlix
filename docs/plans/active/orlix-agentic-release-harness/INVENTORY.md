# Gate And Command Inventory

## Current Roadmap

The current TCTI roadmap contains 115 gates:

| Disposition | Count |
| --- | ---: |
| L0 source, architecture, policy | 4 |
| L1 unit, oracle, golden | 25 |
| L2 component integration | 29 |
| L3 simulator product runtime | 16 |
| L4 physical product runtime | 1 |
| L5 release and TestFlight | 0 |
| Historical remediation outside permanent frontier | 40 |

The 40 historical remediation entries are 19 reducers, 19 fixes, one diagnostic, and one root-cause gate. They remain available for dynamic failure reduction but must not participate in ordinary frontier ordering.

The roadmap records these counts in compact `pyramid_level_gate_ids` and `historical_remediation_gate_ids` lists. Harness validation requires complete, unique membership and prevents permanent gates from depending on historical remediation.

## Product Matrix Gap

The target L3/L4 matrix has the same 19 capability instances on both destinations: app launch, OrlixOS session and payload, kernel boot, first syscall, runtime stability, console output, BusyBox start and command, full shell, mlibc process, Coreutils, package behavior, dynamic loader, signals, VFS, OCI rootfs command, interactive terminal, full readiness, userland marker, and clean exit.

The current roadmap has 16 simulator entries, including overlapping first-syscall evidence, one physical entry, and no release entries. L4 symmetry and L5 publication are missing.

## Command Ownership

- Keep `make tcti-gate TARGET=...` as the focused L0-L2 interface.
- Keep component-owned Kernel, OrlixMLibC, and OrlixOS Make targets.
- Keep `make runtime-validation DESTINATION=... GATE=...` as the sole L3/L4 product-runtime interface.
- Keep archive, archive validation, export, upload, and signing diagnostics as separate serialized L5 phases.
- Add a missing App Store Connect processing and confirmation command.

## Duplication To Remove

- Product semantic paths and version parsing are repeated in root Make targets.
- Signing option assembly is duplicated between archive and export.
- Simulator scope and single-boot assertions are duplicated in runtime validation.
- Beta simulator installation duplicates runtime-validation build, install, payload inspection, and launch behavior.
- The beta simulator gate repeats Xcode command construction.
- The Swift TCTI gate and next-step selector centralize too many unrelated responsibilities.

## Initial Refactor Boundary

`selectedStatusWithSafety` is the single live selection boundary. Existing eligibility and safety filtering can remain while ranking moves into a pure semantic-frontier function. Envelope generation and validation already consume the selected ID and can remain unchanged during the first refactor.
