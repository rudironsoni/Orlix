"""Source-producer and promoted-presence policy for execution proof.

This is the only mapping from resolved origins to Bazel mnemonics and labels.
Workflows and tests must import it. The promotion registry names published
OCI artifacts. It is not a source-producer denylist.
"""

from __future__ import annotations

# Semantic origin -> source-producing actions that must not execute when that
# origin is promoted. Mnemonics are the primary signal so a producer hidden
# behind a non-registry label is still detected.
SOURCE_PRODUCERS = {
    "kernel": {
        "mnemonics": (
            "OrlixKernelMachOArchive",
            "OrlixTctiIsaRestore",
        ),
        "labels": (
            "//bazel/feasibility/kernel:macho",
            "//bazel/feasibility/kernel:tcti_isa",
            "//bazel/feasibility/kernel:kernel-release-iphoneos",
            "//bazel/feasibility/kernel:kernel-release-iphonesimulator",
            "//bazel/feasibility/kernel:kernel-development-iphoneos",
            "//bazel/feasibility/kernel:kernel-development-iphonesimulator",
        ),
    },
    "uapi": {
        "mnemonics": (
            "OrlixLinuxHeadersInstall",
            "OrlixPromotedUapiProduct",
        ),
        "labels": (
            "//bazel/feasibility/kernel:uapi",
        ),
    },
    "mlibc": {
        "mnemonics": (
            "OrlixMLibCSysroot",
            "OrlixCompilerRuntime",
            "OrlixPromotedMlibcProduct",
        ),
        "labels": (
            "//bazel/feasibility/mlibc:sysroot",
        ),
    },
    "rootfs": {
        "mnemonics": (
            "OrlixRootfs",
            "OrlixGuestPackage",
            "OrlixPackageInterface",
        ),
        "labels": (
            "//bazel/feasibility/rootfs:rootfs",
        ),
        "label_prefixes": (
            "//bazel/feasibility/packages:",
        ),
    },
}

PROMOTED_PRESENCE = {
    "kernel": {
        "mnemonics": ("OrlixPromotedKernel",),
        "label_template": "//bazel/promotion:promoted_kernel_{profile}_{destination}",
    },
    "uapi": {
        "mnemonics": (
            "OrlixPromotedUapi",
            "OrlixPromotedUapiHeaders",
        ),
        "labels": ("//bazel/promotion:promoted_uapi",),
    },
    "mlibc": {
        "mnemonics": (
            "OrlixPromotedMlibc",
            "OrlixPromotedMlibcHeaders",
            "OrlixPromotedMlibcLibraries",
        ),
        "labels": ("//bazel/promotion:promoted_sysroot",),
    },
    "rootfs": {
        "mnemonics": ("OrlixPromotedRootfs",),
        "labels": ("//bazel/promotion:promoted_rootfs",),
    },
}


def _normalize_label(label: str) -> str:
    text = label.strip()
    if text.startswith("@@"):
        _, _, rest = text.partition("//")
        return "//" + rest if rest else text
    if text.startswith("@//"):
        return text[1:]
    return text


def classify_source_producer(mnemonic: str | None, label: str | None) -> str | None:
    name = mnemonic or ""
    target = _normalize_label(label or "")
    for origin, spec in SOURCE_PRODUCERS.items():
        if name in spec["mnemonics"]:
            return origin
        if target in spec.get("labels", ()):
            return origin
        for prefix in spec.get("label_prefixes", ()):
            if target.startswith(prefix):
                return origin
    return None


def forbidden_origins(origins: dict[str, str]) -> tuple[str, ...]:
    return tuple(name for name, value in origins.items() if value == "promoted")


def required_promoted(origins: dict[str, str], *, profile: str, destination: str) -> list[dict]:
    required = []
    for origin, value in origins.items():
        if value != "promoted":
            continue
        spec = PROMOTED_PRESENCE[origin]
        labels = list(spec.get("labels", ()))
        template = spec.get("label_template")
        if template:
            labels.append(template.format(profile=profile, destination=destination))
        required.append(
            {
                "origin": origin,
                "mnemonics": list(spec["mnemonics"]),
                "labels": labels,
            }
        )
    return required


def matches_presence(entry: dict, requirement: dict) -> bool:
    mnemonic = entry.get("mnemonic") or ""
    label = _normalize_label(entry.get("targetLabel") or entry.get("target_label") or "")
    if mnemonic in requirement["mnemonics"]:
        if requirement["origin"] == "kernel" and requirement["labels"]:
            return label in requirement["labels"]
        return True
    return label in requirement["labels"]
