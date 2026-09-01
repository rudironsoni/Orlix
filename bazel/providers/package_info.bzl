"""Narrow provider for one guest package install tree."""

OrlixPackageTreeInfo = provider(
    doc = "Provides a deterministic guest package install tree and provenance.",
    fields = {
        "dependency_digests": "Exact sysroot and package dependency digests.",
        "file_manifest": "Installed path, type, mode, ownership, and digest manifest.",
        "install_tree": "Package install tree.",
        "license_manifest": "Package license manifest.",
        "package_metadata": "Canonical package metadata.",
        "source_input_digest": "Package source, patch, configuration, and tool input digest.",
    },
)
