---
type: task
tags: [task, bazel, artifacts]
updated: 2026-09-10
status: doing
summary: "Implement clean component promotion, signed buildsets, lock proposal pull requests, provenance, verification, and retention."
task_of:
  - "[Promote signed component buildsets](../../story/doing/promote-signed-component-buildsets.md)"
blocks:
  - "[Implement shared Bazel cache and buildset reuse](implement-shared-bazel-cache-and-buildset-reuse.md)"
---

# Implement Component And Buildset Promotion

Build each changed component twice without action-result reuse, compare reproducible unsigned outputs, run owning proof, sign and publish by digest, verify after pull, and propose compatible buildset lock changes through a protected pull request.

UAPI dual-build comparison now checks the entire output tree, including the Kbuild archive. Kbuild records relative source paths, and the archive fixes ownership and timestamps. This closes the archive reproducibility defect without changing upstream file contents. MLibC publishes the same stripped dynamic loader as its sysroot, which closes its full-tree comparison defect.

Rootfs generation fixes initramfs and ext4 timestamps, the ext4 hash seed, Linux file ownership, and relative manifest paths. Its digest includes all three filesystem images. The rootfs provider exposes those images and their metadata. Filesystem assembly trees stay inside the producing action. Before publishing its outputs, the action checks executable modes, root ownership, the shell link, and required mount and state directories in the ext4 images.

Unsigned promotion disables remote action reuse and clears signing inputs for its expected signing-rejection check. Signed publication and reconstruction still require the existing trusted key, owning runtime proof, and protected automation.

The trust policy registers the SHA-256 fingerprint of the existing GHCR signing public key. Reconstruction verifies each locked OCI signature with that key before extracting and staging the component trees. Oras and Cosign use the same registry configuration; the local configuration uses the macOS Keychain credential helper. Public-key trust does not replace owning runtime proof or protected automation.

The promoted OrlixKit build verifies the embedded initramfs against the reconstructed guest rootfs and checks the locked buildset in its composition metadata. The Apple matrix treats this as supported guest-resource consumption, with runtime proof, parity, and cutover still separate gates. Warm reconstruction uses local immutable CAS entries before any network request.
