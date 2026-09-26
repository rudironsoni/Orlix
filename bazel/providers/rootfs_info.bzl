"""Narrow provider for the curated OrlixOS root filesystem."""

OrlixRootfsInfo = provider(
    doc = "Provides deterministic rootfs images, package closure, and payload metadata.",
    fields = {
        "artifact_identity_digest": "SHA-256 of the selected image bytes under stable logical paths. Not None and not written into the payload directory.",
        "artifact_identity_manifest": "Canonical artifact-identity-v2 manifest.",
        "base_ext4": "Deterministic base ext4 image.",
        "file_manifest": "Complete installed path manifest.",
        "initramfs": "Deterministic initramfs archive.",
        "package_closure": "Depset of package artifact-identity-v2 digest files used as dependency/provenance metadata; not part of the rootfs artifact identity.",
        "payload_metadata": "Target-derived OrlixOS payload metadata.",
        "source_input_digest": "Semantic rootfs digest. It stays on this provider and is not a consumer action input or a payload file.",
        "state_ext4": "Deterministic state ext4 image.",
    },
)
