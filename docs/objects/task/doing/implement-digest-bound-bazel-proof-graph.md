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
build_intents:
  - "bind kernel proof subjects to the locked profile and destination identity"
verification_intents:
  - "python unittest for the proof graph lock binding"
---

# Implement The Digest-Bound Bazel Proof Graph

Preserve ADR 0017 ordering and current test ownership. Use analysis tests only for graph properties, execution tests for generated contents, workflow tests for automation policy, and fault injection for tampering and environment failures.

Kernel-dependency, KUnit, and kselftest bind the locked `kernel-<profile>-<destination>` artifact-identity-v2 digest. The live identity for that same profile and destination must match the lock. A mismatch is recorded and rejected. The raw Mach-O archive hash is not the proof subject.
