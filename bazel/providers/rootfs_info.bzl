"""Narrow provider for the curated OrlixOS root filesystem."""

OrlixRootfsInfo = provider(
    doc = "Provides deterministic rootfs images, package closure, and payload metadata.",
    fields = {
        "artifact_identity_digest": "SHA-256 of the canonical artifact-identity-v2 manifest.",
        "artifact_identity_manifest": "Canonical artifact-identity-v2 manifest.",
        "base_ext4": "Deterministic base ext4 image.",
        "file_manifest": "Complete installed path manifest.",
        "initramfs": "Deterministic initramfs archive.",
        "package_closure": "Depset of package artifact-identity-v2 digest files used as dependency/provenance metadata; not part of the rootfs artifact identity.",
        "payload_metadata": "Target-derived OrlixOS payload metadata.",
        "source_input_digest": "Rootfs policy and package closure digest.",
        "state_ext4": "Deterministic state ext4 image.",
    },
)
