---
name: orlix-runtime-claim-verification
description: >-
  Use before claiming Orlix work is fixed, green, complete, runtime-ready,
  package-ready, or upstream-test successful. Verifies that exact commands,
  logs, failure/skip accounting, crash reports, and ADR proof order support the
  claim.
---
# Orlix Runtime Claim Verification

Use this skill before any completion claim.

## Claim Check

Identify:

- the exact claim being made;
- the proof lane it belongs to;
- the required command or runtime evidence;
- the observed command output and logs;
- OrlixKit consumer and app-hosted OrlixOS evidence when claiming boot, process I/O, PTY, shell, instance, container, or package behavior;
- failure and skip counts;
- simulator or app crash reports checked;
- missing evidence.
- whether the recorded evidence is current and directly relevant to the exact claim, not just any prior command output.

## Promotion Order

Product runtime claims must follow ADR 0017:

1. Kernel dependency proof
2. Kselftest kernel-interface proof
3. OrlixMLibC libc proof
4. OrlixMLibC-built syscall/UAPI proof
5. POSIX shell environment proof
6. Third-party package ladder: jq, curl, zsh

Product boot, process, PTY, shell, instance, container, or package claims must go through OrlixKit and the app-hosted Linux path. Host records, a hardcoded HostAdapter bundle lookup, terminal text, and packaging alone do not prove Linux behavior.

Before physical TCTI/product validation, run `make agent-status AREA=orlix-tcti`, `make agent-next AREA=orlix-tcti`, and `make agent-task-envelope-check AREA=orlix-tcti` in order. Require current native simulator-ladder evidence and `physical_device_allowed=true`. Missing, stale, false, or unverified eligibility blocks the device run. Simulator, golden-ELF, first-syscall, or partial-package results do not establish physical-device proof.

Network namespace identity and namespace-local state prove isolation only. External networking claims must stay within the current virtio/network proof tier.

## Output

Say what is proven, what is not proven, and what command or log would close the gap. Do not soften missing proof into success.
