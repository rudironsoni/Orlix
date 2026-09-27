---
type: task
tags: [task, bazel, artifacts]
updated: 2026-09-27
status: doing
summary: "Implement clean component promotion, signed buildsets, lock proposals, provenance, verification, and retention."
task_of:
  - "[Promote signed component buildsets](../../story/doing/promote-signed-component-buildsets.md)"
blocks:
  - "[Implement shared Bazel cache and buildset reuse](implement-shared-bazel-cache-and-buildset-reuse.md)"
owned_paths:
  - "bazel/feasibility/kernel/BUILD.bazel"
  - "bazel/feasibility/mlibc/BUILD.bazel"
  - "bazel/feasibility/packages/BUILD.bazel"
  - "bazel/feasibility/rootfs/BUILD.bazel"
  - "bazel/product/**"
  - "bazel/selection/**"
  - "bazel/promotion/promoted.bzl"
  - "bazel/promotion/promoted_execution.py"
  - "bazel/promotion/test_promoted_execution.py"
  - "bazel/migration/test_apple_build_matrix.py"
  - "bazel/migration/test_make_routing.py"
  - "bazel/test_content_digest.py"
  - "make/bazel-migration.mk"
  - "Orlix/BUILD.bazel"
  - "MODULE.bazel"
  - "MODULE.bazel.lock"
  - "bazel/migration/legacy-target-map.json"
  - "bazel/migration/proof-map.json"
  - "docs/objects/task/doing/implement-component-and-buildset-promotion.md"
  - "docs/log.md"
required_skills:
  - "orlix-bazel"
  - "orlix-implementation-boundaries"
required_role: "orlix-implementer"
required_proof:
  - "configured per-boundary origin selection"
  - "promoted composition stamp records the locked buildset off the kernel action key"
build_intents:
  - "instantiate selected kernel, UAPI, sysroot, and rootfs"
  - "stamp promoted composition provenance from the locked buildset"
verification_intents:
  - "bazel query, cquery, and aquery of origin configurations"
  - "aquery action keys for a lock-only buildset change"
---

# Implement Component And Buildset Promotion

Build each changed component twice without action-result reuse, compare reproducible unsigned outputs, run owning proof, sign and publish by digest, verify after pull, and propose compatible buildset lock changes through a protected pull request.

UAPI dual-build comparison now checks the entire output tree, including the Kbuild archive. Kbuild records relative source paths, and the archive fixes ownership and timestamps. This closes the archive reproducibility defect without changing upstream file contents. MLibC publishes the same stripped dynamic loader as its sysroot, which closes its full-tree comparison defect.

Rootfs generation fixes initramfs and ext4 timestamps, the ext4 hash seed, Linux file ownership, and relative manifest paths. Its digest includes all three filesystem images. The rootfs provider exposes those images and their metadata. Filesystem assembly trees stay inside the producing action. Before publishing its outputs, the action checks executable modes, root ownership, the shell link, and required mount and state directories in the ext4 images.

Unsigned promotion disables remote action reuse and clears signing inputs for its expected signing-rejection check. Signed publication and reconstruction still require the existing trusted key, owning runtime proof, and protected automation.

The trust policy registers the SHA-256 fingerprint of the existing GHCR signing public key. Reconstruction verifies each locked OCI signature with that key before extracting and staging the component trees. Oras and Cosign use the same registry configuration; the local configuration uses the macOS Keychain credential helper. Public-key trust does not replace owning runtime proof or protected automation.

The promoted OrlixKit build verifies the embedded initramfs against the reconstructed guest rootfs and checks the locked buildset in its composition metadata. The Apple matrix treats this as supported guest-resource consumption, with runtime proof, parity, and cutover still separate gates. Warm reconstruction uses local immutable CAS entries before any network request.

Normal protected promotion builds all seven compatible components in one Bazel graph under clean root A and again under clean root B. Both builds disable Bazel action-result reuse, the local disk cache, BuildBuddy, ccache, retained Kbuild state, retained Meson/Ninja state, and retained package state. They may restore only Bazel's checksum-verified repository downloads. The existing per-component Make targets remain diagnostic operations.

The protected workflow compares every component between A and B before it publishes any component. It runs only by manual dispatch in `rudironsoni/Orlix` and requires the `bazel-promotion` environment, so the protected environment can approve an exact pre-merge `fix/build-optimizations` commit without granting pull-request code an automatic package-write path. It signs and publishes all seven immutable GHCR artifacts in the same job, then creates one buildset lock proposal without changing `artifacts.lock.json`. The proposal binds the component types, artifact identities, immutable OCI digests, signing-key fingerprint, and trust-policy digest. Cosign signs and verifies the exact proposal bytes before the proposal can enter the atomic lock target. A separate workflow no longer assembles a buildset from seven independent promotion runs. The Cosign Sigstore bundle is the promotion provenance for the signed proposal, while Cosign component signatures and runtime proof remain separate. The workflow carries no build-provenance attestation path or its token permissions.

The product graph selects each boundary through the origin resolver. Source and promoted producers feed `selected` kernel, UAPI, sysroot, and rootfs targets, and those selected targets are what the app, packages, and rootfs payload consume. `component_mode` remains the all-source or all-promoted request. Promoted mode records the locked buildset on `kernel_composition_stamp`, whose inputs are the byte-hashed composition and `OrlixLockedBuildset`. The byte-hashed `OrlixKernelComposition` action keeps `"buildset": null` and does not take the lock, so a provenance-only lock change does not recompile the Kernel. Source mode embeds that byte-hashed composition and does not claim the promoted buildset.
