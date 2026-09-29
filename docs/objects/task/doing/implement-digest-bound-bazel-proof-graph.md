---
type: task
tags: [task, bazel, proof]
updated: 2026-09-27
status: doing
summary: "Map all current proof targets to Bazel and bind reports to exact artifact and buildset digests."
task_of:
  - "[Bind proof to Bazel artifact identities](../../story/doing/bind-proof-to-bazel-artifact-identities.md)"
blocks:
  - "[Prove all supported Apple builds and feature gates](../doing/prove-all-supported-apple-builds-and-feature-gates.md)"
owned_paths:
  - "bazel/**"
  - "make/bazel-migration.mk"
  - "docs/objects/task/doing/implement-digest-bound-bazel-proof-graph.md"
  - "docs/log.md"
  - "docs/index.md"
read_only_paths:
  - "artifacts.lock.json"
  - ".github/workflows/**"
forbidden_paths:
  - ".github/workflows/bazel-ci.yml"
  - "Makefile"
  - "Orlix/**"
required_skills:
  - "orlix-bazel"
  - "orlix-implementation-boundaries"
required_role: "orlix-implementer"
required_proof:
  - "kernel proof uses the locked artifact-identity-v2 digest for the selected profile and destination"
  - "a live kernel identity that disagrees with the lock is rejected"
  - "uapi and mlibc proof inputs use the locked artifact-identity-v2 digests"
  - "a live uapi or mlibc identity that disagrees with the lock is rejected"
  - "rootfs proof uses the locked artifact-identity-v2 digest"
  - "a live rootfs identity that disagrees with the lock is rejected"
  - "product-integration uses the artifact-identity-v2 digest of Orlix.ipa"
  - "a live app identity that disagrees with the locked app digest is rejected"
build_intents:
  - "bind kernel proof subjects to the locked profile and destination identity"
  - "bind uapi and mlibc proof subjects to the locked artifact-identity-v2 digests"
  - "bind the rootfs proof subject to the locked artifact-identity-v2 digest"
  - "bind product-integration to the Orlix.ipa artifact-identity-v2 digest"
verification_intents:
  - "python unittest for the proof graph lock binding"
---

# Implement The Digest-Bound Bazel Proof Graph

Preserve ADR 0017 ordering and current test ownership. Use analysis tests only for graph properties, execution tests for generated contents, workflow tests for automation policy, and fault injection for tampering and environment failures.

Kernel-dependency, KUnit, and kselftest bind the locked `kernel-<profile>-<destination>` artifact-identity-v2 digest. The live identity for that same profile and destination must match the lock. A mismatch is recorded and rejected. The raw Mach-O archive hash is not the proof subject.

The UAPI proof input and the mlibc subject for orlixmlibc and syscall-uapi bind the locked `uapi` and `mlibc` artifact-identity-v2 digests. The live UAPI identity is required and must match the lock. The live mlibc identity, when that file is present, must match the lock. A mismatch is recorded and rejected. The semantic `uapi.sha256` and `sysroot.sha256` markers are not those subjects.

POSIX shell, jq, curl, and zsh bind the locked `rootfs` artifact-identity-v2 digest. The live rootfs identity, when that file is present, must match the lock. A mismatch is recorded and rejected. The semantic `source-input.sha256` marker is not that subject.

Product-integration binds the artifact-identity-v2 digest of `Orlix.ipa`. The raw IPA byte hash is not that subject. When the IPA is present, its live identity must match the locked `app` unsigned digest if that lock entry exists. A mismatch is recorded and rejected.
