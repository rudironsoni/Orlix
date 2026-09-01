"""Narrow providers for prepared Linux, archives, installed UAPI, and Apple packaging."""

OrlixPreparedLinuxInfo = provider(
    doc = "Identifies one deterministic prepared upstream Linux source tree.",
    fields = {
        "source_tree": "Prepared Linux source tree.",
        "source_input_digest": "Digest of upstream, patches, overlays, and configuration inputs.",
        "source_manifest": "Manifest that attributes prepared paths to inputs.",
    },
)

OrlixLinuxArchiveInfo = provider(
    doc = "Provides one private Linux archive without exposing installed UAPI.",
    fields = {
        "archive": "Platform archive.",
        "build_manifest": "Canonical component build manifest.",
        "destination": "Apple destination for the archive.",
        "profile": "Orlix release or development profile.",
        "source_input_digest": "Prepared Linux source input digest.",
        "symbol_manifest": "Exported and required symbol manifest.",
    },
)

OrlixInstalledUapiInfo = provider(
    doc = "Provides only upstream Linux headers_install output for ARCH=arm64.",
    fields = {
        "arch": "Upstream Linux architecture, fixed to arm64.",
        "headers": "Installed UAPI headers tree.",
        "linux_revision": "Exact upstream Linux revision.",
        "uapi_digest": "Content digest of the installed UAPI tree.",
    },
)

OrlixKernelAppleProductInfo = provider(
    doc = "Provides the private Apple-packaged OrlixKernel product.",
    fields = {
        "archive_dependency_digest": "Digest of the Linux archives used by the product.",
        "apple_metadata": "Apple packaging metadata.",
        "destination_slices": "Declared device and simulator slices.",
        "xcframework": "Private OrlixKernel XCFramework.",
    },
)
