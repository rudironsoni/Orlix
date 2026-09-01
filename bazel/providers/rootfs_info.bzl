"""Narrow provider for the curated OrlixOS root filesystem."""

OrlixRootfsInfo = provider(
    doc = "Provides deterministic rootfs trees, images, package closure, and payload metadata.",
    fields = {
        "base_ext4": "Deterministic base ext4 image.",
        "base_tree": "Merged immutable base tree.",
        "file_manifest": "Complete installed path manifest.",
        "initramfs": "Deterministic initramfs archive.",
        "package_closure": "Exact package and dependency digests.",
        "payload_metadata": "Target-derived OrlixOS payload metadata.",
        "source_input_digest": "Rootfs policy and package closure digest.",
        "state_ext4": "Deterministic state ext4 image.",
        "state_tree": "Mutable-state seed tree.",
    },
)
