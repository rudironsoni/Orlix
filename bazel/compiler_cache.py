"""Compiler-object cache policy for actions that build with ccache.

The directory named by CCACHE_DIR is shared storage. It is not a Bazel input
and it is not a BuildBuddy action-cache result. ccache's key is the compiler
binary, the flags, and the preprocessed source, including transitive headers.
"""

from __future__ import annotations

from collections.abc import Mapping


def forward_shell_env(shell: Mapping[str, str]) -> dict[str, str]:
    """Match `compiler_cache_env` in `bazel/compiler_cache.bzl`."""
    disabled = shell.get("CCACHE_DISABLE")
    if disabled:
        return {"CCACHE_DISABLE": disabled}
    env: dict[str, str] = {}
    cache = shell.get("CCACHE_DIR")
    if cache:
        env["CCACHE_DIR"] = cache
    launcher = shell.get("ORLIX_COMPILER_LAUNCHER")
    if launcher:
        env["ORLIX_COMPILER_LAUNCHER"] = launcher
    for key in ("ORLIX_COMPILER_REMOTE_STORAGE", "ORLIX_COMPILER_REMOTE_ONLY"):
        value = shell.get(key)
        if value:
            env[key] = value
    return env


def apply_action_environment(environment: dict[str, str]) -> None:
    """Keep the configured cache. Drop it when the cache is off.

    A cold build sets CCACHE_DISABLE. An unset CCACHE_DIR is not a configured
    cache, so a host launcher path is cleared with the absolute Xcode paths.
    """
    if environment.get("CCACHE_DISABLE") or not environment.get("CCACHE_DIR"):
        environment.pop("CCACHE_DIR", None)
        environment["ORLIX_COMPILER_LAUNCHER"] = ""
