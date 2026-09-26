#!/usr/bin/env python3
"""Classify changed paths for the origin resolver.

Unknown paths fail closed. A lock-record change is not a source change.
Proof, Build noise, and app/host edits do not change guest boundaries.
"""

from __future__ import annotations

from dataclasses import dataclass

DISPOSABLE_PREFIXES = (
    "Build/",
    "bazel-bin/",
    "bazel-out/",
    "bazel-testlogs/",
)

APP_PREFIXES = (
    "Orlix/",
    "OrlixHostAdapter/",
    "OrlixOSTestApp/",
    "OrlixTestApp/",
    "OrlixCloud.xcodeproj/",
    "OrlixOS/Sources/include",
    "OrlixOS/Sources/Session",
    "third_party/swift/",
    "third_party/patches/",
    "docs/",
    "Resources/",
    "fastlane/",
    "Tools/",
    ".github/",
)

KERNEL_IMPL_PREFIXES = (
    "OrlixKernel/",
)

UAPI_EXACT_SUFFIXES = (
    "/include/uapi/",
    "/include/asm-generic/",
)

MLIBC_PREFIXES = (
    "OrlixMLibC/",
    "bazel/feasibility/mlibc/",
)

PACKAGE_PREFIXES = (
    "OrlixCoreUtils/",
    "bazel/feasibility/packages/",
)

ROOTFS_PREFIXES = (
    "OrlixOS/Sources/distribution",
    "OrlixOS/Sources/init",
    "OrlixOS/Sources/make",
    "OrlixOS/Sources/patches",
    "bazel/feasibility/rootfs/",
)

FEASIBILITY_NON_PRODUCT_PREFIXES = (
    "bazel/feasibility/native/",
    "bazel/feasibility/apple/",
    "bazel/feasibility/analysis/",
    "bazel/proof/",
)

LOCK_RECORD_FILES = frozenset({"artifacts.lock.json"})

TOOLCHAIN_FILES = frozenset(
    {
        "upstreams.lock.json",
        "MODULE.bazel",
        "MODULE.bazel.lock",
        ".bazelrc",
        ".bazelrc.local.example",
        "Makefile",
        "Brewfile",
        "Gemfile",
        "Gemfile.lock",
        "bazel/build_state.py",
        "bazel/content_digest.py",
        "bazel/artifact_identity.bzl",
        "bazel/BUILD.bazel",
        "bazel/bootstrap.rb",
        "BUILD.bazel",
    }
)

ROOTFS_FILES = frozenset({"OrlixOS/Makefile"})

TOOLCHAIN_PREFIXES = (
    "bazel/",
    "make/",
    "xcode/",
)

NON_PRODUCT_FILES = frozenset(
    {
        "AGENTS.md",
        "README.md",
        "IMPLEMENT.md",
        "CLAUDE.md",
        "rulesync.jsonc",
        "skills-lock.json",
        "opencode.jsonc",
        "buildServer.json",
        "builder.json",
        "project.yml",
        "project.local.yml",
    }
)

UAPI_FILES = frozenset(
    {
        "bazel/feasibility/kernel/kernel_uapi.bzl",
        "scripts/headers_install.sh",
        "scripts/Makefile.headersinst",
    }
)

FACT_KEYS = (
    "kernel_changed",
    "uapi_changed",
    "mlibc_changed",
    "rootfs_changed",
    "package_changed",
    "toolchain_changed",
)


class ClassifierError(ValueError):
    pass


@dataclass(frozen=True)
class Classification:
    facts: dict[str, bool]
    probe_uapi: bool
    classes: tuple[str, ...]
    paths: tuple[str, ...]


def _starts(path: str, prefix: str) -> bool:
    bare = prefix.rstrip("/")
    return path == bare or path.startswith(prefix)


def _invalid(path: str) -> bool:
    if not path or "\x00" in path or path.startswith("/"):
        return True
    return any(part in ("", ".", "..") for part in path.split("/"))


def classify_path(path: str) -> str:
    normalized = str(path).strip().lstrip("./")
    if _invalid(normalized):
        raise ClassifierError(f"unknown path: {path!r}")
    for prefix in DISPOSABLE_PREFIXES:
        if _starts(normalized, prefix):
            return "disposable"
    if normalized in LOCK_RECORD_FILES:
        return "lock_record"
    if normalized in NON_PRODUCT_FILES:
        return "non_product"
    if normalized in UAPI_FILES or _is_uapi_path(normalized):
        return "uapi_sensitive"
    if normalized.startswith("OrlixKernel/Sources/ports/orlix/patches/"):
        name = normalized.rsplit("/", 1)[-1]
        return "uapi_sensitive" if "uapi" in name else "kernel_impl"
    for prefix in KERNEL_IMPL_PREFIXES:
        if _starts(normalized, prefix):
            return "kernel_impl"
    for prefix in MLIBC_PREFIXES:
        if _starts(normalized, prefix):
            return "mlibc"
    for prefix in PACKAGE_PREFIXES:
        if _starts(normalized, prefix):
            return "package"
    if normalized in ROOTFS_FILES:
        return "rootfs_policy"
    for prefix in ROOTFS_PREFIXES:
        if _starts(normalized, prefix):
            return "rootfs_policy"
    for prefix in FEASIBILITY_NON_PRODUCT_PREFIXES:
        if _starts(normalized, prefix):
            return "non_product"
    if normalized in TOOLCHAIN_FILES:
        return "toolchain"
    for prefix in TOOLCHAIN_PREFIXES:
        if _starts(normalized, prefix):
            return "toolchain"
    for prefix in APP_PREFIXES:
        if _starts(normalized, prefix):
            return "non_product"
    raise ClassifierError(f"unknown path: {normalized}")


def _is_uapi_path(path: str) -> bool:
    wrapped = f"/{path}"
    if wrapped.endswith("/include/uapi") or wrapped.endswith("/include/asm-generic"):
        return True
    return any(suffix in wrapped for suffix in UAPI_EXACT_SUFFIXES)


def _empty_facts() -> dict[str, bool]:
    return {key: False for key in FACT_KEYS}


def classify_paths(paths: list[str]) -> Classification:
    classes: list[str] = []
    for path in paths:
        classes.append(classify_path(path))
    unique = tuple(dict.fromkeys(classes))
    facts = _empty_facts()
    if "toolchain" in unique:
        facts = {key: True for key in FACT_KEYS}
        return Classification(facts=facts, probe_uapi=False, classes=unique, paths=tuple(paths))
    facts["kernel_changed"] = "kernel_impl" in unique
    facts["mlibc_changed"] = "mlibc" in unique
    facts["rootfs_changed"] = "rootfs_policy" in unique
    facts["package_changed"] = "package" in unique
    probe = "uapi_sensitive" in unique
    return Classification(facts=facts, probe_uapi=probe, classes=unique, paths=tuple(paths))
