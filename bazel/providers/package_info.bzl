"""Narrow provider for one guest package install tree."""

OrlixPackageTreeInfo = provider(
    doc = "Provides a deterministic guest package install tree and provenance.",
    fields = {
        "artifact_identity_digest": "SHA-256 of the selected install-tree bytes under stable logical paths. Not None and not a producer sidecar.",
        "artifact_identity_closure": "Depset of package artifact-identity-v2 digest files used as dependency/provenance metadata; not part of this package's artifact identity.",
        "artifact_identity_manifest": "Canonical artifact-identity-v2 manifest.",
        "dependency_digests": "Exact sysroot and package dependency digests.",
        "file_manifest": "Installed path, type, mode, ownership, and digest manifest.",
        "install_tree": "Package install tree.",
        "license_manifest": "Package license manifest.",
        "package_metadata": "Canonical package metadata.",
        "source_input_digest": "Package source, patch, configuration, and tool input digest for provenance. Not a consumer action input.",
    },
)
