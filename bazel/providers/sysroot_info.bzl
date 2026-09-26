"""Narrow provider for the OrlixMLibC guest sysroot."""

OrlixLibcSysrootInfo = provider(
    doc = "Provides the installed OrlixMLibC sysroot and its consumed UAPI identity.",
    fields = {
        "abi_manifest": "Exported libc ABI manifest.",
        "artifact_identity_digest": "SHA-256 of the selected sysroot bytes under stable logical paths. Not None and not a producer sidecar.",
        "artifact_identity_manifest": "Canonical sysroot artifact-identity-v2 manifest.",
        "compiler_runtime": "Guest compiler runtime archive.",
        "compiler_runtime_identity_digest": "SHA-256 of the canonical compiler-runtime artifact-identity-v2 manifest.",
        "compiler_runtime_identity_manifest": "Canonical compiler-runtime artifact-identity-v2 manifest.",
        "consumed_uapi_digest": "Digest of OrlixInstalledUapiInfo consumed by the build.",
        "dynamic_loader": "Dynamic loader contract and artifact.",
        "headers": "Installed libc headers.",
        "libraries": "Installed guest libraries.",
        "sysroot_digest": "Content digest of installed headers and libraries.",
        "target_triple": "Guest compiler target triple for the fail-closed check. Not a consumer action input.",
    },
)
