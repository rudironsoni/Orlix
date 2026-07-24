---
type: story
tags:
  - story
  - orlix-tcti
updated: 2026-07-24
status: doing
summary: "Prove safe TCTI execution through owning kernel, userspace, and app-hosted tests."
story_of:
  - "[Orlix TCTI](../../epic/doing/orlix-tcti.md)"
has_task:
  - "[Complete the pinned simulator TCTI ladder](../../task/doing/complete-pinned-simulator-tcti-ladder.md)"
  - "[Complete AArch64 ISA-on-ISA coverage](../../task/doing/complete-aarch64-isa-on-isa-coverage.md)"
  - "[Run authorized TCTI device validation](../../task/todo/run-authorized-tcti-device-validation.md)"
  - "[Promote TCTI as the product default](../../task/todo/promote-tcti-as-product-default.md)"
blocks:
  - "[Validate and publish the mobile terminal release](validate-and-publish-mobile-terminal-release.md)"
---

# Prove TCTI product execution

As an Orlix user, I want ordinary AArch64 Linux ELF instructions to execute safely through TCTI so Linux userspace reaches syscall dispatch and console output without executable guest mappings or a second process model.

The TCTI scope envelope records the owning test order without replacing native results. This story advances from explicit classification of every leaf in the pinned Arm source and complete typed proof of the union of applicable AArch64 EL0 feature configurations through kernel dependency proof and KUnit, Linux-visible kselftest, upstream mlibc, OrlixMLibC-built syscall and UAPI proof, a POSIX shell environment, then jq, curl, and zsh package proof. Coreutils, HostAdapter, OrlixOS, native app, pinned simulator, authorized device validation when selected, and product-default promotion follow without replacing an earlier tier. The runtime HWCAP profile is a separate advertisement projection and cannot narrow the completion target. Package suites expose compatibility regressions, but do not define or prove ISA completeness.
