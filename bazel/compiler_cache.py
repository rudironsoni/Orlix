"""Compiler-object cache policy for actions that build with ccache.

The directory named by CCACHE_DIR is shared storage. It is not a Bazel input
and it is not a BuildBuddy action-cache result. ccache's key is the compiler
binary, the flags, and the preprocessed source, including transitive headers.
Statistics files and ccache timestamps stay out of the action key.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
from collections.abc import Mapping
from typing import Any


PINNED_CLANG = Path("/opt/homebrew/opt/llvm/bin/clang")
# A loop, not `return LEAF_VALUE`. arm64 clang can emit the same bytes for a
# constant return at -O0 and -O2, which would make a cache hit look like a
# recompile. object_differs stays false when the bytes match.
COMPILER_OBJECT_UNIT = """\
static int mix(int n) {
    int sum = n;
    int i = 0;
    while (i < 32) {
        sum = (sum * 33) + i;
        if ((sum & 7) == 0) {
            sum = sum ^ (i << 3);
        }
        i = i + 1;
    }
    return sum;
}

int leaf(int n) {
    return mix(n) + mix(n ^ 15);
}
"""

_MACHINE_FIELDS = {
    "direct_cache_hit": "direct_hits",
    "cache_hit_direct": "direct_hits",
    "preprocessed_cache_hit": "preprocessed_hits",
    "cache_hit_preprocessed": "preprocessed_hits",
    "cache_miss": "misses",
    "called_for_link": "called_for_link",
    "cache_size_kibibyte": "cache_size_kibibytes",
}
_REQUIRED_STATS = (
    "direct_hits",
    "preprocessed_hits",
    "misses",
    "called_for_link",
    "cache_size_kibibytes",
)


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
    return env


def apply_action_environment(environment: dict[str, str]) -> None:
    """Keep the configured cache. Drop it when the cache is off.

    A cold build sets CCACHE_DISABLE. An unset CCACHE_DIR is not a configured
    cache, so a host launcher path is cleared with the absolute Xcode paths.
    """
    if environment.get("CCACHE_DISABLE") or not environment.get("CCACHE_DIR"):
        environment.pop("CCACHE_DIR", None)
        environment["ORLIX_COMPILER_LAUNCHER"] = ""


def object_differs(left: bytes, right: bytes) -> bool:
    """True only when both objects have bytes and those bytes differ.

    Equal bytes are not a difference. An empty object is not a difference.
    Callers must not treat a match as success by weakening this check.
    """
    if not left or not right:
        return False
    return left != right


def unread_statistics() -> dict[str, Any]:
    """No ccache counters. A cache directory existing is not a hit."""
    return {
        "statistics_read": False,
        "direct_hits": None,
        "preprocessed_hits": None,
        "misses": None,
        "called_for_link": None,
        "cache_size_kibibytes": None,
        "ccache_hits": None,
    }


def _finish_statistics(counts: dict[str, int]) -> dict[str, Any]:
    missing = [name for name in _REQUIRED_STATS if name not in counts]
    if missing:
        raise ValueError("ccache statistics did not include " + ", ".join(missing))
    stats = {name: counts[name] for name in _REQUIRED_STATS}
    stats["ccache_hits"] = stats["direct_hits"] + stats["preprocessed_hits"]
    stats["statistics_read"] = True
    return stats


def _machine_statistics(text: str) -> dict[str, int]:
    counts: dict[str, int] = {}
    for line in text.splitlines():
        parts = line.split()
        if len(parts) != 2 or not parts[1].isdigit():
            continue
        field = _MACHINE_FIELDS.get(parts[0])
        if field is not None:
            counts[field] = int(parts[1])
    return counts


def _human_size_kibibytes(text: str) -> int | None:
    labeled = re.search(r"Cache size \(([^)]+)\):\s+([0-9]+(?:\.[0-9]+)?)", text)
    if labeled:
        unit = labeled.group(1).strip().lower()
        value = float(labeled.group(2))
        # ccache's summary rounds a small cache to 0.0 GB. That is not a size.
        if unit in {"gb", "gib"} and value == 0:
            return None
        scale = {"b": 1 / 1024, "bytes": 1 / 1024, "kb": 1, "kib": 1, "mb": 1024, "mib": 1024, "gb": 1024 * 1024, "gib": 1024 * 1024}
        factor = scale.get(unit)
        if factor is None:
            return None
        return int(value * factor)
    legacy = re.search(r"(?m)^cache size\s+([0-9]+(?:\.[0-9]+)?)\s*([kKmMgG]?[bB])", text)
    if legacy is None:
        return None
    value = float(legacy.group(1))
    unit = legacy.group(2).lower()
    factor = {"b": 1 / 1024, "kb": 1, "mb": 1024, "gb": 1024 * 1024}[unit]
    return int(value * factor)


def _human_statistics(text: str) -> dict[str, int]:
    counts: dict[str, int] = {}
    direct = re.search(r"cache hit \(direct\)\s+(\d+)", text) or re.search(r"(?m)^\s*Direct:\s+(\d+)", text)
    preprocessed = re.search(r"cache hit \(preprocessed\)\s+(\d+)", text) or re.search(r"(?m)^\s*Preprocessed:\s+(\d+)", text)
    misses = re.search(r"(?m)^cache miss\s+(\d+)", text) or re.search(r"(?m)^\s*Misses:\s+(\d+)", text)
    called = re.search(r"called for link\s+(\d+)", text) or re.search(r"(?m)^\s*Called for link:\s+(\d+)", text)
    if direct:
        counts["direct_hits"] = int(direct.group(1))
    if preprocessed:
        counts["preprocessed_hits"] = int(preprocessed.group(1))
    if misses:
        counts["misses"] = int(misses.group(1))
    if called:
        counts["called_for_link"] = int(called.group(1))
    elif direct and preprocessed and misses:
        counts["called_for_link"] = 0
    size = _human_size_kibibytes(text)
    if size is not None:
        counts["cache_size_kibibytes"] = size
    return counts


def parse_ccache_statistics(text: str) -> dict[str, Any]:
    """Parse ccache --print-stats or ccache -s. Timestamps are ignored."""
    machine = _machine_statistics(text)
    if set(_REQUIRED_STATS).issubset(machine):
        return _finish_statistics(machine)
    human = _human_statistics(text)
    if set(_REQUIRED_STATS).issubset(human):
        return _finish_statistics(human)
    raise ValueError("ccache statistics were not present")


def ccache_binary() -> str | None:
    pinned = Path("/opt/homebrew/bin/ccache")
    if pinned.is_file():
        return str(pinned)
    return shutil.which("ccache")


def compiler_for_proof() -> str | None:
    """The pinned LLVM clang when it exists. Never the unlinked llvm@20 keg."""
    if PINNED_CLANG.is_file():
        if "llvm@20" in PINNED_CLANG.resolve().parts:
            raise RuntimeError("pinned clang resolved to llvm@20; the pin is /opt/homebrew/opt/llvm/bin/clang")
        return str(PINNED_CLANG)
    found = shutil.which("clang") or shutil.which("cc")
    if found is None:
        return None
    if "llvm@20" in Path(found).resolve().parts:
        return None
    return found


def compiler_environment(cache_dir: Path, home: Path) -> dict[str, str]:
    return {
        "PATH": os.environ.get("PATH", ""),
        "HOME": str(home),
        "CCACHE_DIR": str(cache_dir),
        "CCACHE_COMPILERCHECK": "content",
        "CCACHE_CONFIGPATH": "/dev/null",
        "CCACHE_MAXSIZE": "1G",
    }


def read_ccache_statistics(ccache: str, environment: Mapping[str, str]) -> dict[str, Any]:
    env = dict(environment)
    printed = subprocess.run([ccache, "--print-stats"], env=env, text=True, capture_output=True, check=False)
    if printed.returncode == 0:
        try:
            return parse_ccache_statistics(printed.stdout)
        except ValueError:
            pass
    shown = subprocess.run([ccache, "-s"], env=env, text=True, capture_output=True, check=False)
    if shown.returncode != 0:
        detail = printed.stderr or shown.stderr or "ccache statistics failed"
        raise RuntimeError(detail.strip())
    return parse_ccache_statistics(shown.stdout)


def zero_ccache_statistics(ccache: str, environment: Mapping[str, str]) -> None:
    env = dict(environment)
    zeroed = subprocess.run([ccache, "--zero-stats"], env=env, text=True, capture_output=True, check=False)
    if zeroed.returncode == 0:
        return
    fallback = subprocess.run([ccache, "-z"], env=env, text=True, capture_output=True, check=False)
    if fallback.returncode != 0:
        detail = zeroed.stderr or fallback.stderr or "ccache --zero-stats failed"
        raise RuntimeError(detail.strip())


def collect_statistics(cache_dir: Path, ccache: str | None = None) -> dict[str, Any]:
    binary = ccache or ccache_binary()
    if not binary:
        return unread_statistics()
    environment = dict(os.environ)
    environment["CCACHE_DIR"] = str(cache_dir)
    environment["CCACHE_CONFIGPATH"] = "/dev/null"
    return read_ccache_statistics(binary, environment)


def statistics_line(stats: Mapping[str, Any], *, label: str, cache_key: str = "", restored: str = "") -> str:
    if stats.get("statistics_read") is not True:
        return " ".join((
            "ccache_statistics",
            f"label={label}",
            f"cache_key={cache_key or '-'}",
            f"restored={restored or '-'}",
            "statistics_read=false",
            "ccache_hits=none",
            "note=a_cache_directory_is_not_a_hit",
        ))
    return " ".join((
        "ccache_statistics",
        f"label={label}",
        f"cache_key={cache_key or '-'}",
        f"restored={restored or '-'}",
        f"direct_hits={stats['direct_hits']}",
        f"preprocessed_hits={stats['preprocessed_hits']}",
        f"misses={stats['misses']}",
        f"called_for_link={stats['called_for_link']}",
        f"cache_size_kibibytes={stats['cache_size_kibibytes']}",
        f"ccache_hits={stats['ccache_hits']}",
        "note=ccache_hits_are_not_bazel_disk_cache_hits_or_buildbuddy_remote_cache_hits",
    ))


def compile_object(
    *,
    compiler: str,
    ccache: str,
    source: Path,
    output: Path,
    environment: Mapping[str, str],
    flags: list[str],
) -> bytes:
    output.unlink(missing_ok=True)
    command = [ccache, compiler, "-c", str(source), "-o", str(output), *flags]
    result = subprocess.run(command, env=dict(environment), text=True, capture_output=True, check=False)
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip() or "compiler failed")
    return output.read_bytes()


def prove_wiped_foreign_output(*, compiler: str, ccache: str, cache_dir: Path, work: Path) -> dict[str, Any]:
    """Delete one compiler object, keep CCACHE_DIR, and require a real hit.

    The object file is the foreign compiler output. The same-runner hit is not
    a fresh-runner pass. Bazel disk-cache hits and BuildBuddy remote-cache hits
    are not measured here.
    """
    if "return LEAF_VALUE" in COMPILER_OBJECT_UNIT:
        raise ValueError("constant-return fixture cannot prove a compiler-cache hit")
    source = work / "leaf.c"
    source.write_text(COMPILER_OBJECT_UNIT, encoding="utf-8")
    environment = compiler_environment(cache_dir, work)
    zero_ccache_statistics(ccache, environment)
    cold_path = work / "cold.o"
    cold = compile_object(
        compiler=compiler, ccache=ccache, source=source, output=cold_path,
        environment=environment, flags=["-O0"],
    )
    cold_stats = read_ccache_statistics(ccache, environment)
    if cold_stats["ccache_hits"] != 0 or cold_stats["misses"] < 1:
        raise RuntimeError("a cold compiler cache must miss: " + statistics_line(cold_stats, label="cold"))
    optimized_path = work / "optimized.o"
    optimized = compile_object(
        compiler=compiler, ccache=ccache, source=source, output=optimized_path,
        environment=environment, flags=["-O2"],
    )
    if not object_differs(cold, optimized):
        raise RuntimeError("object_differs failed: -O0 and -O2 emitted the same bytes")
    compiled = read_ccache_statistics(ccache, environment)
    if compiled["misses"] <= cold_stats["misses"] or compiled["ccache_hits"] != cold_stats["ccache_hits"]:
        raise RuntimeError("a changed flag must miss the compiler cache")
    optimized_path.unlink()
    if optimized_path.exists() or not cache_dir.is_dir():
        raise RuntimeError("the object delete must keep CCACHE_DIR")
    restored = compile_object(
        compiler=compiler, ccache=ccache, source=source, output=work / "restored.o",
        environment=environment, flags=["-O2"],
    )
    if restored != optimized:
        raise RuntimeError("a ccache hit did not restore the object bytes")
    after = read_ccache_statistics(ccache, environment)
    if after["ccache_hits"] <= compiled["ccache_hits"]:
        raise RuntimeError("rebuilding a deleted object must be a ccache hit")
    if after["called_for_link"] != 0:
        raise RuntimeError("a -c compile must not be called for link")
    return {
        "ccache": after,
        "object_bytes_match": True,
        "object_differs": True,
        "same_runner": True,
        "fresh_runner": False,
        "bazel_disk_cache_hits": None,
        "buildbuddy_remote_cache_hits": None,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--statistics", action="store_true")
    parser.add_argument("--cache-dir", type=Path)
    parser.add_argument("--out", type=Path)
    parser.add_argument("--cache-key", default="")
    parser.add_argument("--restored", default="")
    parser.add_argument("--label", default="ccache")
    parser.add_argument("--zero-after", action="store_true")
    args = parser.parse_args(argv)
    if not args.statistics:
        parser.error("--statistics is required")
    if args.cache_dir is None:
        parser.error("--cache-dir is required")
    stats = collect_statistics(args.cache_dir)
    label = f"{args.label}_file_counters" if args.zero_after else args.label
    print(statistics_line(stats, label=label, cache_key=args.cache_key, restored=args.restored))
    if args.zero_after and stats.get("statistics_read") is True:
        binary = ccache_binary()
        if binary:
            environment = dict(os.environ)
            environment["CCACHE_DIR"] = str(args.cache_dir)
            environment["CCACHE_CONFIGPATH"] = "/dev/null"
            zero_ccache_statistics(binary, environment)
            print(
                "ccache_statistics "
                f"label={args.label} counters_zeroed=true objects_kept=true "
                "note=stored_counters_were_not_hits_executed_by_this_runner"
            )
    if args.out is not None:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(stats, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
