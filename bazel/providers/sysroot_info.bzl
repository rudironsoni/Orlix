"""Narrow provider for the OrlixMLibC guest sysroot."""

OrlixLibcSysrootInfo = provider(
    doc = "Provides the installed OrlixMLibC sysroot and its consumed UAPI identity.",
    fields = {
        "abi_manifest": "Exported libc ABI manifest.",
        "compiler_runtime": "Guest compiler runtime archive.",
        "consumed_uapi_digest": "Digest of OrlixInstalledUapiInfo consumed by the build.",
        "dynamic_loader": "Dynamic loader contract and artifact.",
        "headers": "Installed libc headers.",
        "libraries": "Installed guest libraries.",
        "sysroot_digest": "Content digest of installed headers and libraries.",
        "target_triple": "Guest compiler target triple.",
    },
)
