"""Execute the invalidation rows the matrix used to leave unverified.

Kernel object closure is a real compiler plus depfiles. Cross-component rows
stamp declared inputs with source_key.py. Cache layers are separate stores.
A fresh runner reads only the remote store named by the BuildBuddy remote
cache URL. ccache and the disk cache stay on the runner that filled them.
"""

from __future__ import annotations

import hashlib
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tempfile

from bazel.feasibility.packages.test_action_inputs import rule_markers
from bazel.feasibility.test_compiler_cache_action import _cache_counts


ROOT = Path(__file__).resolve().parents[2]
SOURCE_KEY = ROOT / "bazel/feasibility/packages/source_key.py"
_SWIFT = "Orlix/Features/Servers/Domain/Server.swift"
_KERNEL = "external/+native_sources+orlix_linux_source"
_MLIBC = "external/+native_sources+orlix_mlibc_source/options/posix/stat.c"
_GUEST = "external/+native_sources+orlix_bash_source/shell.c"
_SSH = "external/+native_sources+orlix_libssh2_source/src/session.c"


def _compiler() -> str | None:
    return shutil.which("clang") or shutil.which("gcc") or shutil.which("cc")


def _inputs_region(path: Path, mnemonic: str) -> str:
    text = path.read_text(encoding="utf-8")
    at = text.index(f'mnemonic = "{mnemonic}"')
    return text[max(0, at - 2500) : at + 2500]


def _reject_markers(region: str, mnemonic: str, markers: tuple[str, ...]) -> None:
    for marker in markers:
        if marker in region:
            raise AssertionError(f"{mnemonic} inputs include {marker}")


def _production_boundaries() -> None:
    kernel = _inputs_region(
        ROOT / "bazel/feasibility/kernel/kernel_macho.bzl",
        "OrlixKernelMachOArchive",
    )
    mlibc = _inputs_region(
        ROOT / "bazel/feasibility/mlibc/mlibc_sysroot.bzl",
        "OrlixMLibCSysroot",
    )
    guest = _inputs_region(
        ROOT / "bazel/feasibility/packages/bash.bzl",
        "OrlixGuestPackage",
    )
    ssh = _inputs_region(
        ROOT / "bazel/feasibility/native/native_bundle.bzl",
        "OrlixLibssh2AppleArchive",
    )
    foreign = (
        ".swift",
        "orlix_mlibc_source",
        "orlix_bash_source",
        "orlix_libssh2_source",
    )
    _reject_markers(kernel, "kernel", foreign)
    _reject_markers(mlibc, "mlibc", (".swift", "orlix_linux_source", "orlix_bash_source", "orlix_libssh2_source"))
    _reject_markers(guest, "guest", (".swift", "orlix_linux_source", "orlix_mlibc_source", "orlix_libssh2_source"))
    _reject_markers(ssh, "ssh", (".swift", "orlix_linux_source", "orlix_mlibc_source", "orlix_bash_source"))
    if "mlibc_source" not in mlibc:
        raise AssertionError("mlibc action lost its source input")
    if "libssh2_source" not in ssh:
        raise AssertionError("ssh action lost its source input")
    if "guest_configure_inputs" not in guest:
        raise AssertionError("guest action lost its configure input filter")
    modules = (ROOT / "Orlix/app_modules.bzl").read_text(encoding="utf-8")
    if f'"{_SWIFT}"' not in modules:
        raise AssertionError("swift module list no longer contains the edited file")
    markers = rule_markers()
    if "orlix_bash_source/" not in markers["bash"]:
        raise AssertionError("bash source marker drifted")


def _stamp(root: Path, names: list[str], calls: list[int]) -> str:
    calls[0] += 1
    archive = root / f"stamp-{calls[0]}.tar"
    stamp = root / f"stamp-{calls[0]}.txt"
    command = [sys.executable, "-B", str(SOURCE_KEY), "files", str(archive), str(stamp)]
    for name in names:
        command.extend([name, str(root / name)])
    completed = subprocess.run(command, check=False, capture_output=True, text=True)
    if completed.returncode != 0:
        raise AssertionError(completed.stderr)
    return hashlib.sha256(stamp.read_text(encoding="utf-8").encode()).hexdigest()


def _write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def _wrapper(path: Path, log: Path, compiler: str) -> None:
    path.write_text(
        "#!/bin/sh\n"
        f"printf '%s\\n' \"$*\" >> {shlex.quote(str(log))}\n"
        f"exec {shlex.quote(compiler)} \"$@\"\n",
        encoding="utf-8",
    )
    path.chmod(0o755)


def _make(directory: Path) -> None:
    completed = subprocess.run(
        ["make", "-C", str(directory), "-j1"],
        check=False,
        capture_output=True,
        text=True,
        env={"PATH": os.environ["PATH"], "MAKEFLAGS": ""},
    )
    if completed.returncode != 0:
        raise AssertionError(completed.stderr or completed.stdout)


def _age(path: Path, when: int) -> None:
    os.utime(path, (when, when))


def _compiler_lines(log: Path) -> list[str]:
    if not log.is_file():
        return []
    return [line for line in log.read_text(encoding="utf-8").splitlines() if line]


def observe_source_edits() -> dict[str, object]:
    """Compile one kernel object graph and stamp the other component inputs."""
    compiler = _compiler()
    if compiler is None or not shutil.which("make") or not shutil.which("ar"):
        return {"available": False}
    _production_boundaries()
    calls = [0]
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        kernel = root / _KERNEL
        changed = kernel / "changed.c"
        dependent = kernel / "dependent.c"
        unrelated = kernel / "unrelated.c"
        shared = kernel / "shared.h"
        other = kernel / "other.h"
        _write(shared, "static inline int shared_value(void) { return 3; }\n")
        _write(other, "static inline int other_value(void) { return 4; }\n")
        _write(changed, '#include "shared.h"\nint changed(void) { return 1; }\n')
        _write(dependent, '#include "shared.h"\nint dependent(void) { return shared_value(); }\n')
        _write(unrelated, '#include "other.h"\nint unrelated(void) { return other_value(); }\n')
        _write(root / _MLIBC, "int mlibc_stat(void) { return 1; }\n")
        _write(root / _GUEST, "int shell(void) { return 1; }\n")
        _write(root / _SSH, "int session(void) { return 1; }\n")
        _write(root / _SWIFT, "enum UnrelatedEdit { case idle }\n")
        log = kernel / "compiler.log"
        _wrapper(kernel / "cc-wrapper", log, compiler)
        makefile = kernel / "Makefile"
        makefile.write_text(
            "CC = ./cc-wrapper\n"
            "CFLAGS = -MMD -MP\n"
            "\n"
            "kernel.a: changed.o dependent.o unrelated.o\n"
            "\tar rcs $@ changed.o dependent.o unrelated.o\n"
            "\n"
            "-include changed.d dependent.d unrelated.d\n",
            encoding="utf-8",
        )
        _make(kernel)
        if len(_compiler_lines(log)) < 3:
            raise AssertionError("kernel graph did not compile its objects")
        kernel_names = [
            f"{_KERNEL}/changed.c",
            f"{_KERNEL}/dependent.c",
            f"{_KERNEL}/unrelated.c",
            f"{_KERNEL}/shared.h",
            f"{_KERNEL}/other.h",
        ]
        groups = {
            "kernel": kernel_names,
            "mlibc": [_MLIBC],
            "guest": [_GUEST],
            "ssh": [_SSH],
            "swift": [_SWIFT],
        }

        def keys() -> dict[str, str]:
            return {name: _stamp(root, names, calls) for name, names in groups.items()}

        def make_after(edit: Path, text: str) -> tuple[list[str], dict[str, bytes]]:
            log.write_text("", encoding="utf-8")
            edit.write_text(text, encoding="utf-8")
            base = 1_700_000_000
            for name in ("changed.o", "dependent.o", "unrelated.o", "changed.d", "dependent.d", "unrelated.d", "kernel.a"):
                _age(kernel / name, base)
            for path in (changed, dependent, unrelated, shared, other):
                _age(path, base)
            if edit.is_relative_to(kernel):
                _age(edit, base + 5)
            _make(kernel)
            lines = _compiler_lines(log)
            kept = {name: (kernel / name).read_bytes() for name in ("dependent.o", "unrelated.o")}
            return lines, kept

        baseline = keys()
        swift_lines, _ = make_after(root / _SWIFT, "enum UnrelatedEdit { case edited }\n")
        swift_keys = keys()
        (root / _SWIFT).write_text("enum UnrelatedEdit { case idle }\n", encoding="utf-8")
        mlibc_lines, _ = make_after(root / _MLIBC, "int mlibc_stat(void) { return 2; }\n")
        mlibc_keys = keys()
        (root / _MLIBC).write_text("int mlibc_stat(void) { return 1; }\n", encoding="utf-8")
        guest_lines, _ = make_after(root / _GUEST, "int shell(void) { return 2; }\n")
        guest_keys = keys()
        (root / _GUEST).write_text("int shell(void) { return 1; }\n", encoding="utf-8")
        restored = keys()
        if restored["kernel"] != baseline["kernel"] or restored["mlibc"] != baseline["mlibc"]:
            raise AssertionError("restored inputs did not return the kernel and mlibc keys")
        kept_before = {name: (kernel / name).read_bytes() for name in ("dependent.o", "unrelated.o")}
        archive_before = (kernel / "kernel.a").read_bytes()
        kernel_lines, kept = make_after(changed, '#include "shared.h"\nint changed(void) { return 7; }\n')
        kernel_keys = keys()
        rebuilt = [line for line in kernel_lines if "changed.c" in line]
        leaked = [line for line in kernel_lines if "dependent.c" in line or "unrelated.c" in line]
        changed_object = (kernel / "changed.o").read_bytes()
        archive = (kernel / "kernel.a").read_bytes()
        return {
            "available": True,
            "source_key_invocations": calls[0],
            "swift": {
                "compiler_lines": swift_lines,
                "keys": swift_keys,
                "baseline": baseline,
            },
            "mlibc": {
                "compiler_lines": mlibc_lines,
                "keys": mlibc_keys,
                "baseline": baseline,
            },
            "guest": {
                "compiler_lines": guest_lines,
                "keys": guest_keys,
                "baseline": baseline,
            },
            "kernel": {
                "compiler_lines": kernel_lines,
                "rebuilt": rebuilt,
                "leaked": leaked,
                "kept_objects": sorted(kept),
                "changed_object_nonempty": len(changed_object) > 0,
                "archive_changed": archive != archive_before,
                "kept_same": kept == kept_before,
                "keys": kernel_keys,
                "baseline": baseline,
            },
        }


def _buildbuddy_remote_cache() -> str:
    text = (ROOT / ".bazelrc").read_text(encoding="utf-8")
    match = re.search(r"^build:buildbuddy --remote_cache=(\S+)\s*$", text, re.M)
    if match is None or "buildbuddy" not in match.group(1):
        raise AssertionError("BuildBuddy remote cache URL is missing")
    return match.group(1)


def _ccache_stats(ccache: str, env: dict[str, str]) -> tuple[int, int]:
    result = subprocess.run([ccache, "--show-stats"], env=env, text=True, capture_output=True, check=False)
    if result.returncode != 0:
        result = subprocess.run([ccache, "-s"], env=env, text=True, capture_output=True, check=False)
    if result.returncode != 0:
        raise AssertionError(result.stderr)
    try:
        return _cache_counts(result.stdout)
    except AssertionError:
        if "Hits:" not in result.stdout and "cache hit" not in result.stdout:
            return 0, 0
        raise


def _store_put(directory: Path, key: str, payload: bytes) -> None:
    path = directory / key
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(payload)


def _store_get(directory: Path, key: str) -> bytes | None:
    path = directory / key
    if not path.is_file():
        return None
    return path.read_bytes()


def observe_cache_layers() -> dict[str, object] | None:
    """Separate a fresh remote hit from a same-runner ccache hit and a disk hit."""
    ccache = shutil.which("ccache")
    compiler = _compiler()
    if not ccache or not compiler:
        return None
    remote_url = _buildbuddy_remote_cache()
    source = "int leaf(int n) {\n  int sum = 1;\n  for (int i = 0; i < n; ++i) sum += i;\n  return sum;\n}\n"
    action_key = hashlib.sha256(source.encode()).hexdigest()
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        code = root / "leaf.c"
        code.write_text(source, encoding="utf-8")
        log = root / "compiler.log"
        wrapper = root / "cc-wrapper"
        _wrapper(wrapper, log, compiler)
        primed = root / "primed.o"
        prime = subprocess.run(
            [str(wrapper), "-c", str(code), "-o", str(primed)],
            check=False,
            capture_output=True,
            text=True,
        )
        if prime.returncode != 0:
            raise AssertionError(prime.stderr)
        payload = primed.read_bytes()
        if not payload:
            raise AssertionError("priming compile wrote an empty object")
        disk = root / "disk-cache"
        remote = root / "remote-buildbuddy"
        _store_put(disk, action_key, payload)
        _store_put(remote, action_key, payload)
        disk_bytes_before_ccache = (disk / action_key).read_bytes()
        remote_bytes_before_ccache = (remote / action_key).read_bytes()

        same = root / "same-runner"
        same_cache = same / "ccache"
        same_env = {
            "PATH": os.environ["PATH"],
            "HOME": str(same),
            "CCACHE_DIR": str(same_cache),
            "CCACHE_COMPILERCHECK": "content",
            "CCACHE_CONFIGPATH": "/dev/null",
            "CCACHE_MAXSIZE": "1G",
        }
        subprocess.run([ccache, "--zero-stats"], env=same_env, check=False)
        log.write_text("", encoding="utf-8")
        first = same / "first.o"
        miss = subprocess.run(
            [ccache, str(wrapper), "-c", str(code), "-o", str(first)],
            env=same_env,
            check=False,
            capture_output=True,
            text=True,
        )
        if miss.returncode != 0:
            raise AssertionError(miss.stderr)
        cold_hits, cold_misses = _ccache_stats(ccache, same_env)
        compiles_after_miss = len(_compiler_lines(log))
        second = same / "second.o"
        hit = subprocess.run(
            [ccache, str(wrapper), "-c", str(code), "-o", str(second)],
            env=same_env,
            check=False,
            capture_output=True,
            text=True,
        )
        if hit.returncode != 0:
            raise AssertionError(hit.stderr)
        warm_hits, warm_misses = _ccache_stats(ccache, same_env)
        compiles_after_hit = len(_compiler_lines(log))
        if (disk / action_key).read_bytes() != disk_bytes_before_ccache:
            raise AssertionError("ccache hit rewrote the disk cache")
        if (remote / action_key).read_bytes() != remote_bytes_before_ccache:
            raise AssertionError("ccache hit rewrote the remote store")

        disk_reads = 0
        remote_reads = 0
        disk_hit = _store_get(disk, action_key)
        if disk_hit is not None:
            disk_reads += 1
        remote_during_disk = _store_get(remote, action_key) if disk_hit is None else None
        if remote_during_disk is not None:
            remote_reads += 1
        disk_compiler = len(_compiler_lines(log))

        fresh = root / "fresh-runner"
        fresh_cache = fresh / "ccache"
        fresh_disk = fresh / "disk-cache"
        fresh_cache.mkdir(parents=True)
        fresh_disk.mkdir(parents=True)
        if any(fresh_disk.iterdir()) or any(path.is_file() for path in fresh_cache.rglob("*")):
            raise AssertionError("fresh runner local caches were not empty")
        if remote.is_relative_to(fresh):
            raise AssertionError("remote store lived inside the fresh runner")
        fresh_env = dict(same_env)
        fresh_env["HOME"] = str(fresh)
        fresh_env["CCACHE_DIR"] = str(fresh_cache)
        subprocess.run([ccache, "--zero-stats"], env=fresh_env, check=False)
        fresh_hits, fresh_misses = _ccache_stats(ccache, fresh_env)
        compiler_before_fresh = len(_compiler_lines(log))
        fresh_disk_reads = 0
        looked = _store_get(fresh_disk, action_key)
        if looked is not None:
            fresh_disk_reads += 1
        fresh_remote = _store_get(remote, action_key)
        fresh_remote_reads = 1 if fresh_remote is not None else 0
        fresh_compiles = len(_compiler_lines(log)) - compiler_before_fresh
        return {
            "remote_cache_url": remote_url,
            "ccache": {
                "cold_hits": cold_hits,
                "cold_misses": cold_misses,
                "hits": warm_hits,
                "misses": warm_misses,
                "compiler_invocations_on_miss": compiles_after_miss,
                "compiler_invocations_after_hit": compiles_after_hit,
                "objects_equal": first.read_bytes() == second.read_bytes(),
                "layer": "ccache",
            },
            "disk": {
                "reads": disk_reads,
                "remote_reads": remote_reads,
                "compiler_invocations": disk_compiler - compiles_after_hit,
                "object_matches": disk_hit == payload,
                "layer": "disk cache hit",
            },
            "fresh": {
                "ccache_hits": fresh_hits,
                "ccache_misses": fresh_misses,
                "disk_reads": fresh_disk_reads,
                "remote_reads": fresh_remote_reads,
                "compiler_invocations": fresh_compiles,
                "object_matches": fresh_remote == payload,
                "local_disk_empty": not any(fresh_disk.iterdir()),
                "layer": "remote cache hit",
            },
        }
