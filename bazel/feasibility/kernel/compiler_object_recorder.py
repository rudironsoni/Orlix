"""Record one Kbuild compiler launch from ccache statistics.

Kbuild decides whether a unit is compiled. This process is the launcher in
front of that decision: it runs ccache, then reads ``ccache --print-stats``.
A zero exit status is not a hit. A hit is a direct or preprocessed counter
moving without a miss. Compiler execution is the counters ccache increments
only when it runs the compiler.
"""

from __future__ import annotations

import argparse
import fcntl
import json
import os
from pathlib import Path
import re
import subprocess
import sys


HIT_COUNTERS = ("direct_cache_hit", "preprocessed_cache_hit")
COMPILER_EXECUTED_COUNTERS = (
    "cache_miss",
    "compile_failed",
    "compiler_produced_stdout",
    "compiler_produced_no_output",
    "compiler_produced_empty_output",
    "called_for_preprocessing",
)
DELTA_COUNTERS = HIT_COUNTERS + COMPILER_EXECUTED_COUNTERS + (
    "remote_storage_hit",
    "remote_storage_miss",
    "remote_storage_read_hit",
    "remote_storage_read_miss",
    "remote_storage_write",
    "remote_storage_error",
    "remote_storage_timeout",
    "local_storage_hit",
    "local_storage_miss",
    "local_storage_read_hit",
    "local_storage_write",
)
BYTE_COUNTERS = (
    "remote_storage_read_bytes",
    "remote_storage_write_bytes",
    "remote_storage_recv_bytes",
    "remote_storage_sent_bytes",
)
ORLIXCC = re.compile(r"ORLIXCC\s+(\S+)\s+(\S+)")
SOURCE_FLAG = re.compile(r"(?:^|\s)-c\s+(\S+)")


def parse_print_stats(text: str) -> dict[str, int]:
    """Parse ``ccache --print-stats`` tab rows. Human ``ccache -s`` is not used."""
    stats: dict[str, int] = {}
    for line in text.splitlines():
        key, separator, value = line.partition("\t")
        if not separator:
            continue
        try:
            stats[key] = int(value)
        except ValueError:
            continue
    if not stats:
        raise ValueError("ccache --print-stats returned no counters")
    return stats


def read_stats(ccache: str, env: dict[str, str]) -> dict[str, int]:
    completed = subprocess.run(
        [ccache, "--print-stats"],
        env=env,
        text=True,
        capture_output=True,
        check=False,
    )
    if completed.returncode != 0:
        raise RuntimeError(completed.stderr.strip() or "ccache --print-stats failed")
    return parse_print_stats(completed.stdout)


def _delta(before: dict[str, int], after: dict[str, int]) -> dict[str, int]:
    keys = set(DELTA_COUNTERS) | set(BYTE_COUNTERS) | {
        key for key in after if key.endswith("_bytes")
    }
    return {key: after.get(key, 0) - before.get(key, 0) for key in sorted(keys)}


def classify(before: dict[str, int], after: dict[str, int], exit_code: int) -> dict[str, object]:
    """Separate wrapper hit/miss from compiler execution.

    ``exit_code`` is stored and never selects a hit.
    """
    delta = _delta(before, after)
    hits = sum(delta.get(counter, 0) for counter in HIT_COUNTERS)
    misses = delta.get("cache_miss", 0)
    executed = sum(delta.get(counter, 0) for counter in COMPILER_EXECUTED_COUNTERS)
    if hits > 0 and misses == 0 and executed == 0:
        wrapper = "hit"
    elif misses > 0 and hits == 0:
        wrapper = "miss"
    else:
        wrapper = "unobserved"
    transferred = sum(delta.get(counter, 0) for counter in BYTE_COUNTERS)
    transferred += sum(
        value for key, value in delta.items() if key.endswith("_bytes") and key not in BYTE_COUNTERS
    )
    return {
        "kbuild": "issued",
        "wrapper": wrapper,
        "compiler_executed": executed > 0,
        "exit_code": exit_code,
        "stats_source": "ccache --print-stats",
        "delta": delta,
        "remote_storage_hit": delta.get("remote_storage_hit", 0),
        "transferred_bytes": transferred if any(key.endswith("_bytes") for key in after) else None,
    }


def unit_from_args(args: list[str]) -> str:
    if "-c" in args:
        index = args.index("-c")
        if index + 1 < len(args):
            return args[index + 1]
    for arg in args:
        if arg.endswith(".c"):
            return arg
    if "-o" in args:
        index = args.index("-o")
        if index + 1 < len(args):
            return args[index + 1]
    return " ".join(args)


class _Lock:
    def __init__(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        self._handle = path.open("a+")

    def __enter__(self) -> "_Lock":
        fcntl.flock(self._handle.fileno(), fcntl.LOCK_EX)
        return self

    def __exit__(self, *_exc: object) -> None:
        fcntl.flock(self._handle.fileno(), fcntl.LOCK_UN)
        self._handle.close()


def record(args: list[str]) -> int:
    ccache = os.environ.get("ORLIX_CCACHE_BIN", "")
    if not ccache:
        print("ORLIX_CCACHE_BIN is required", file=sys.stderr)
        return 127
    record_path = os.environ.get("ORLIX_COMPILER_CACHE_RECORD", "")
    env = os.environ.copy()
    cache_dir = env.get("CCACHE_DIR", "")
    lock_path = Path(cache_dir) / "orlix-compiler-recorder.lock" if cache_dir else Path(record_path).with_suffix(".lock")
    with _Lock(lock_path):
        before = read_stats(ccache, env)
        completed = subprocess.run([ccache, *args], env=env, check=False)
        after = read_stats(ccache, env)
        if record_path:
            row = classify(before, after, completed.returncode)
            row["unit"] = unit_from_args(args)
            with Path(record_path).open("a", encoding="utf-8") as handle:
                handle.write(json.dumps(row, sort_keys=True) + "\n")
    return completed.returncode


def _load_units(path: Path) -> list[dict[str, object]]:
    if not path.is_file():
        return []
    rows: list[dict[str, object]] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.strip():
            value = json.loads(line)
            if isinstance(value, dict):
                rows.append(value)
    return rows


def _issued_sources(log_text: str) -> list[tuple[str, str]]:
    return [(platform, source) for platform, source in ORLIXCC.findall(log_text)]


def _command_sources(work: Path) -> list[str]:
    sources: list[str] = []
    for command in work.rglob("*.command"):
        if not command.is_file():
            continue
        match = SOURCE_FLAG.search(command.read_text(encoding="utf-8", errors="replace"))
        if match:
            sources.append(match.group(1))
    return sources


def _matches(unit: str, source: str) -> bool:
    return unit == source or unit.endswith("/" + source)


def join_records(
    units: list[dict[str, object]],
    kbuild_log: str,
    command_sources: list[str],
    output_digest: str = "",
) -> dict[str, object]:
    """Join wrapper stats with Kbuild's ORLIXCC lines and untouched command files."""
    issued = _issued_sources(kbuild_log)
    consumed: set[int] = set()
    rows: list[dict[str, object]] = []
    for platform, source in issued:
        match = next(
            (
                index
                for index, row in enumerate(units)
                if index not in consumed and _matches(str(row.get("unit", "")), source)
            ),
            None,
        )
        if match is None:
            rows.append({
                "unit": source,
                "platform": platform,
                "kbuild": "issued",
                "wrapper": "unobserved",
                "compiler_executed": None,
                "stats_source": "ccache --print-stats",
            })
            continue
        consumed.add(match)
        row = dict(units[match])
        row["platform"] = platform
        row["kbuild"] = "issued"
        rows.append(row)
    for index, row in enumerate(units):
        if index not in consumed:
            rows.append(row)
    issued_sources = {source for _platform, source in issued}
    for source in command_sources:
        if any(_matches(source, item) or _matches(item, source) for item in issued_sources):
            continue
        if any(_matches(str(row.get("unit", "")), source) or _matches(source, str(row.get("unit", ""))) for row in rows):
            continue
        rows.append({
            "unit": source,
            "kbuild": "skipped",
            "wrapper": None,
            "compiler_executed": False,
            "stats_source": "ccache --print-stats",
        })
    return {
        "output_digest": output_digest,
        "stats_source": "ccache --print-stats",
        "units": rows,
    }


def summarize(record: dict[str, object]) -> dict[str, object]:
    units = record.get("units")
    if not isinstance(units, list):
        units = []
    remote_hits = 0
    executed = 0
    transferred: int | None = 0
    saw_bytes = False
    for unit in units:
        if not isinstance(unit, dict):
            continue
        if unit.get("wrapper") == "hit" and int(unit.get("remote_storage_hit") or 0) > 0 and unit.get("compiler_executed") is False:
            remote_hits += 1
        if unit.get("compiler_executed") is True:
            executed += 1
        if unit.get("kbuild") == "skipped" or unit.get("wrapper") in {None, "unobserved"}:
            continue
        if "transferred_bytes" not in unit or unit.get("transferred_bytes") is None:
            transferred = None
            continue
        if transferred is not None:
            saw_bytes = True
            transferred += int(unit["transferred_bytes"])
    if not saw_bytes:
        transferred = None
    return {
        "remote_object_hits": remote_hits,
        "compiler_processes_executed": executed,
        "transferred_bytes": transferred,
        "output_digest": record.get("output_digest") or "",
        "units": units,
    }


def _join_cli(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--units", required=True)
    parser.add_argument("--kbuild-log", required=True)
    parser.add_argument("--work", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--output-digest", default="")
    args = parser.parse_args(argv)
    work = Path(args.work)
    record = join_records(
        _load_units(Path(args.units)),
        Path(args.kbuild_log).read_text(encoding="utf-8", errors="replace"),
        _command_sources(work),
        args.output_digest,
    )
    destination = Path(args.output)
    destination.write_text(json.dumps(record, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0


def main(argv: list[str] | None = None) -> int:
    args = list(sys.argv[1:] if argv is None else argv)
    if args and args[0] == "join":
        return _join_cli(args[1:])
    if args and args[0] == "experiment":
        from bazel.feasibility.kernel.compiler_remote_experiment import main as experiment_main

        return experiment_main(args[1:])
    if args and args[0] == "record":
        args = args[1:]
    try:
        return record(args)
    except (OSError, RuntimeError, ValueError) as error:
        print(str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
