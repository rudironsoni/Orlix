"""Narrow providers for prepared Linux, archives, installed UAPI, and Apple packaging."""

OrlixPreparedLinuxInfo = provider(
    doc = "Identifies one deterministic prepared upstream Linux source tree.",
    fields = {
        "source_tree": "Prepared Linux source tree.",
        "source_input_digest": "Prepared-input digest for the fail-closed kernel check. Not a consumer action input.",
        "source_manifest": "Manifest that attributes prepared paths to inputs.",
    },
)

OrlixLinuxArchiveInfo = provider(
    doc = "Provides one private Linux product without exposing installed UAPI.",
    fields = {
        "archive": "Platform archive.",
        "artifact_identity_digest": "SHA-256 of the selected archive bytes under stable logical paths. Not None and not a producer source-input sidecar.",
        "artifact_identity_manifest": "Canonical artifact-identity-v2 manifest for the delivered product.",
        "boot_resources": "Delivered device-tree resource files.",
        "build_manifest": "Canonical component build manifest.",
        "destination": "Apple destination for the fail-closed kernel check. Not a consumer action input.",
        "product": "Product tree containing the archive and delivered device trees.",
        "profile": "Orlix release or development profile for the fail-closed kernel check. Not a consumer action input.",
        "source_input_digest": "Prepared Linux source input digest for the fail-closed kernel check. Not a consumer action input.",
        "symbol_manifest": "Exported and required symbol manifest.",
    },
)

OrlixInstalledUapiInfo = provider(
    doc = "Provides only upstream Linux headers_install output for ARCH=arm64.",
    fields = {
        "arch": "Upstream Linux architecture, fixed to arm64.",
        "artifact_identity_digest": "SHA-256 of the selected header bytes under stable logical paths. Not None and not a producer sidecar.",
        "artifact_identity_manifest": "Canonical artifact-identity-v2 manifest.",
        "headers": "Installed UAPI headers tree.",
        "linux_revision": "Exact upstream Linux revision for the fail-closed kernel check. Not a consumer action input.",
        "uapi_digest": "Content digest of the installed UAPI tree.",
    },
)

OrlixKernelAppleProductInfo = provider(
    doc = "Provides the private Apple-packaged OrlixKernel product.",
    fields = {
        "archive_dependency_digest": "Artifact-identity-v2 digest of the selected Linux archive bytes.",
        "apple_metadata": "Apple packaging metadata.",
        "destination_slices": "Declared device and simulator slices.",
        "xcframework": "Private OrlixKernel XCFramework.",
    },
)
