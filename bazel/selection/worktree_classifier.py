#!/usr/bin/env python3
"""Classify worktree changes for auto origin resolution.

Git plus lock.source_sha. No bazel query. Unknown or unreadable state
fails closed.
"""

from __future__ import annotations

import os
import subprocess
from dataclasses import dataclass
from pathlib import Path

DISPOSABLE_PREFIXES = (
    "Build/",
    "bazel-bin/",
    "bazel-out/",
    "bazel-testlogs/",
    ".git/",
)

APP_PREFIXES = (
    "Orlix/",
    "OrlixHostAdapter/",
    "OrlixOSTestApp/",
    "OrlixTestApp/",
    "OrlixOS/Sources/include",
    "OrlixOS/Sources/Session",
    "third_party/swift/",
    "docs/",
    "Resources/",
    "fastlane/",
)

KERNEL_IMPL_PREFIXES = (
    "OrlixKernel/Sources/ports/orlix/overlay/",
    "OrlixKernel/Sources/ports/orlix/kbuild/",
    "OrlixKernel/Sources/ports/orlix/isa/",
    "OrlixKernel/Sources/ports/orlix/configs/",
    "OrlixKernel/Sources/boot/",
    "OrlixKernel/Sources/include/",
)

UAPI_PREFIXES = (
    "include/uapi/",
    "arch/arm64/include/uapi/",
    "arch/orlix/include/uapi/",
    "include/asm-generic/",
    "usr/include/",
    "scripts/headers_install.sh",
    "scripts/Makefile.headersinst",
    "bazel/feasibility/kernel/kernel_uapi.bzl",
)

MLIBC_PREFIXES = (
    "OrlixMLibC/",
    "bazel/feasibility/mlibc/",
)

ROOTFS_PREFIXES = (
    "OrlixCoreUtils/",
    "OrlixOS/Sources/make",
    "OrlixOS/Sources/init",
    "OrlixOS/Sources/patches",
    "OrlixOS/Sources/distribution",
    "bazel/feasibility/packages/",
    "bazel/feasibility/rootfs/",
)

TOOLCHAIN_PREFIXES = (
    "bazel/",
    "make/",
    ".github/workflows/",
    "third_party/patches/",
    "xcode/",
)

TOOLCHAIN_FILES = (
    "artifacts.lock.json",
    "upstreams.lock.json",
    "MODULE.bazel",
    "MODULE.bazel.lock",
    ".bazelrc",
    "Makefile",
    "Brewfile",
    "project.yml",
)

CLASSES = (
    "disposable",
    "app",
    "kernel_impl",
    "uapi_sensitive",
    "mlibc",
    "rootfs",
    "toolchain",
)


class ClassifierError(ValueError):
    pass


@dataclass(frozen=True)
class Classification:
    facts: dict[str, bool]
    probe_uapi: bool
    classes: tuple[str, ...]
    paths: tuple[str, ...]


def _run_git(repo: Path, args: list[str]) -> bytes:
    try:
        completed = subprocess.run(
            ["git", "-C", str(repo), *args],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
    except (OSError, subprocess.CalledProcessError) as error:
        raise ClassifierError(f"git failed: {args[0] if args else 'git'}") from error
    return completed.stdout


def _decode_z(payload: bytes) -> list[str]:
    if not payload:
        return []
    parts = payload.split(b"\0")
    out = []
    for part in parts:
        if not part:
            continue
        try:
            out.append(part.decode("utf-8"))
        except UnicodeDecodeError as error:
            raise ClassifierError("unparseable git path") from error
    return out


def _name_status_paths(payload: bytes) -> list[str]:
    tokens = _decode_z(payload)
    paths: list[str] = []
    index = 0
    while index < len(tokens):
        status = tokens[index]
        index += 1
        if not status:
            continue
        code = status[0]
        if code in "RC":
            if index + 1 >= len(tokens):
                raise ClassifierError("unparseable git rename")
            paths.append(tokens[index])
            paths.append(tokens[index + 1])
            index += 2
        else:
            if index >= len(tokens):
                raise ClassifierError("unparseable git name-status")
            paths.append(tokens[index])
            index += 1
    return paths


def classify_path(path: str) -> str:
    normalized = path.lstrip("./")
    if not normalized or "\0" in normalized:
        raise ClassifierError(f"invalid path: {path!r}")
    for prefix in DISPOSABLE_PREFIXES:
        if normalized == prefix.rstrip("/") or normalized.startswith(prefix):
            return "disposable"
    if normalized in TOOLCHAIN_FILES:
        return "toolchain"
    for prefix in UAPI_PREFIXES:
        if normalized == prefix or normalized.startswith(prefix):
            return "uapi_sensitive"
    if "/include/uapi/" in f"/{normalized}" or normalized.endswith("/include/uapi"):
        return "uapi_sensitive"
    if normalized.startswith("OrlixKernel/Sources/ports/orlix/patches/"):
        return "uapi_sensitive" if "uapi" in normalized else "kernel_impl"
    for prefix in KERNEL_IMPL_PREFIXES:
        if normalized.startswith(prefix):
            return "kernel_impl"
    if normalized.startswith("OrlixKernel/"):
        return "kernel_impl"
    for prefix in MLIBC_PREFIXES:
        if normalized.startswith(prefix):
            return "mlibc"
    for prefix in ROOTFS_PREFIXES:
        if normalized.startswith(prefix):
            return "rootfs"
    for prefix in TOOLCHAIN_PREFIXES:
        if normalized.startswith(prefix):
            return "toolchain"
    for prefix in APP_PREFIXES:
        if normalized.startswith(prefix):
            return "app"
    if normalized in (
        "AGENTS.md",
        "README.md",
        "IMPLEMENT.md",
        "CLAUDE.md",
        "rulesync.jsonc",
        "skills-lock.json",
        "opencode.jsonc",
        "buildServer.json",
    ):
        return "app"
    raise ClassifierError(f"unknown relevant path: {normalized}")


def collect_paths(repo: Path, source_sha: str) -> list[str]:
    if not isinstance(source_sha, str) or len(source_sha) != 40:
        raise ClassifierError("missing source_sha")
    try:
        _run_git(repo, ["cat-file", "-e", f"{source_sha}^{{commit}}"])
    except ClassifierError as error:
        raise ClassifierError("baseline commit unavailable") from error
    paths: list[str] = []
    paths.extend(_name_status_paths(_run_git(repo, ["diff", "-z", "--name-status", source_sha, "HEAD"])))
    paths.extend(_name_status_paths(_run_git(repo, ["diff", "-z", "--cached", "--name-status"])))
    paths.extend(_name_status_paths(_run_git(repo, ["diff", "-z", "--name-status"])))
    paths.extend(_decode_z(_run_git(repo, ["ls-files", "-z", "--others", "--exclude-standard"])))
    unique: list[str] = []
    seen = set()
    for path in paths:
        if path not in seen:
            seen.add(path)
            unique.append(path)
    return unique


def classify(repo: Path, source_sha: str) -> Classification:
    paths = collect_paths(repo, source_sha)
    classes = []
    for path in paths:
        kind = classify_path(path)
        if kind != "disposable":
            classes.append(kind)
    unique_classes = tuple(dict.fromkeys(classes))
    if "toolchain" in unique_classes:
        facts = {
            "kernel_changed": True,
            "uapi_changed": True,
            "mlibc_changed": True,
            "rootfs_changed": True,
        }
        return Classification(facts=facts, probe_uapi=False, classes=unique_classes, paths=tuple(paths))
    facts = {
        "kernel_changed": "kernel_impl" in unique_classes or "uapi_sensitive" in unique_classes,
        "uapi_changed": False,
        "mlibc_changed": "mlibc" in unique_classes,
        "rootfs_changed": "rootfs" in unique_classes,
    }
    probe = "uapi_sensitive" in unique_classes
    return Classification(facts=facts, probe_uapi=probe, classes=unique_classes, paths=tuple(paths))
