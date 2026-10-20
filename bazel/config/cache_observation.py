from __future__ import annotations

import argparse
import gzip
import importlib.util
import json
import re
import subprocess
from pathlib import Path
from typing import Any


_DIGEST_PATH = Path(__file__).resolve().parents[1] / "content_digest.py"
_DIGEST_SPEC = importlib.util.spec_from_file_location("content_digest", _DIGEST_PATH)
if _DIGEST_SPEC is None or _DIGEST_SPEC.loader is None:
    raise ImportError(f"missing content digest module: {_DIGEST_PATH}")
_CONTENT_DIGEST = importlib.util.module_from_spec(_DIGEST_SPEC)
_DIGEST_SPEC.loader.exec_module(_CONTENT_DIGEST)

_ORLIXCC = re.compile(r"(?:^|\s)ORLIXCC(?:\s|$)")
_COMPILER_SUFFIXES = (".o", ".lo", ".obj")
_SLOW_WALL_MS = 60_000
_SLOW_PHASE_MS = 5_000


def _records(path: Path) -> list[dict[str, Any]]:
    if not path.is_file():
        return []
    text = path.read_text()
    decoder = json.JSONDecoder()
    records = []
    offset = 0
    while offset < len(text):
        while offset < len(text) and text[offset].isspace():
            offset += 1
        if offset < len(text):
            record, offset = decoder.raw_decode(text, offset)
            records.append(record)
    return records


def _buildbuddy_remote_hits(actions: list[dict[str, Any]]) -> int:
    """A BuildBuddy remote action-cache hit needs both fields.

    The runner must be exactly "remote cache hit" and cacheHit must be true.
    A false cacheHit and a disk-cache hit stay out of this count.
    """
    return sum(
        1
        for action in actions
        if action.get("cacheHit") is True and action.get("runner") == "remote cache hit"
    )


_CCACHE_COUNTERS = (
    "direct_hits",
    "preprocessed_hits",
    "misses",
    "called_for_link",
    "cache_size_kibibytes",
)


def _ccache_record(payload: dict[str, Any] | None) -> dict[str, Any]:
    empty = {
        "statistics_read": False,
        "direct_hits": None,
        "preprocessed_hits": None,
        "misses": None,
        "ccache_misses": None,
        "called_for_link": None,
        "cache_size_kibibytes": None,
        "ccache_hits": None,
        "fresh_runner": False,
    }
    if payload is None:
        return empty
    if payload.get("statistics_read") is not True:
        if payload.get("ccache_hits") is not None or payload.get("ccache_misses") is not None:
            raise ValueError("unread ccache statistics cannot report a hit")
        return empty
    for key in _CCACHE_COUNTERS:
        if not isinstance(payload.get(key), int):
            raise ValueError(f"ccache statistics missing {key}")
    hits = payload["direct_hits"] + payload["preprocessed_hits"]
    if payload.get("ccache_hits") != hits:
        raise ValueError("ccache hits must be direct hits plus preprocessed hits")
    misses = payload["misses"]
    if payload.get("ccache_misses") not in (None, misses):
        raise ValueError("ccache_misses must match misses")
    return {
        "statistics_read": True,
        "direct_hits": payload["direct_hits"],
        "preprocessed_hits": payload["preprocessed_hits"],
        "misses": misses,
        "ccache_misses": misses,
        "called_for_link": payload["called_for_link"],
        "cache_size_kibibytes": payload["cache_size_kibibytes"],
        "ccache_hits": hits,
        "fresh_runner": False,
    }


def _labeled_counters(
    runners: list[str],
    ccache: dict[str, Any],
    buildbuddy_remote_cache_hits: int,
) -> dict[str, Any]:
    """Three caches. A ccache hit is not a Bazel disk hit or a BuildBuddy hit."""
    return {
        "ccache_hits": ccache["ccache_hits"],
        "ccache_misses": ccache["ccache_misses"],
        "bazel_disk_cache_hits": runners.count("disk cache hit"),
        "buildbuddy_remote_cache_hits": buildbuddy_remote_cache_hits,
    }


_COREUTILS_MARKER = "Building GNU Coreutils with incremental Make state"


def coreutils_action_counters(text: str) -> dict[str, int | None] | None:
    """Counters from the coreutils action log, not the shared directory file.

    `direct_cache_hit`, `direct_cache_miss`, `preprocessed_cache_hit`, and
    `cache_miss` stay ccache counters. They are not Bazel disk hits and they
    are not BuildBuddy remote hits. `ccache_misses` adds `direct_cache_miss`
    to `cache_miss`.
    """
    start = text.rfind(_COREUTILS_MARKER)
    if start < 0:
        return None
    window = text[start:]
    required = ("direct_cache_hit", "preprocessed_cache_hit", "cache_miss")
    decoder = json.JSONDecoder()
    search_from = 0
    while True:
        brace = window.find("{", search_from)
        if brace < 0:
            return None
        try:
            payload, _ = decoder.raw_decode(window[brace:])
        except json.JSONDecodeError:
            search_from = brace + 1
            continue
        if isinstance(payload, dict) and all(isinstance(payload.get(name), int) for name in required):
            direct_misses = payload.get("direct_cache_miss")
            misses = payload["cache_miss"]
            if isinstance(direct_misses, int):
                misses += direct_misses
            return {
                "direct_hits": payload["direct_cache_hit"],
                "direct_cache_hit": payload["direct_cache_hit"],
                "direct_cache_miss": direct_misses if isinstance(direct_misses, int) else None,
                "preprocessed_hits": payload["preprocessed_cache_hit"],
                "preprocessed_cache_hit": payload["preprocessed_cache_hit"],
                "cache_miss": payload["cache_miss"],
                "misses": misses,
                "ccache_misses": misses,
                "called_for_link": int(payload.get("called_for_link") or 0),
            }
        search_from = brace + 1


def load_ccache_statistics(path: Path | None) -> dict[str, Any] | None:
    if path is None:
        return None
    if not path.is_file():
        raise ValueError(f"ccache statistics are missing: {path}")
    payload = json.loads(path.read_text())
    if not isinstance(payload, dict):
        raise ValueError("ccache statistics file is not a statistics record")
    return _ccache_record(payload)


def _reported_ccache(
    shared: dict[str, Any],
    action: dict[str, int | None] | None,
    *,
    build_log_provided: bool,
) -> dict[str, Any]:
    """The summary counts the coreutils action. A shared-directory zero is not it."""
    if action is not None:
        hits = action["direct_hits"] + action["preprocessed_hits"]
        return {
            "statistics_read": True,
            "direct_hits": action["direct_hits"],
            "direct_cache_hit": action["direct_cache_hit"],
            "direct_cache_miss": action["direct_cache_miss"],
            "preprocessed_hits": action["preprocessed_hits"],
            "preprocessed_cache_hit": action["preprocessed_cache_hit"],
            "cache_miss": action["cache_miss"],
            "misses": action["misses"],
            "ccache_misses": action["ccache_misses"],
            "called_for_link": action["called_for_link"],
            "cache_size_kibibytes": None,
            "ccache_hits": hits,
            "source": "coreutils-action-log",
            "fresh_runner": False,
        }
    if build_log_provided:
        unread = _ccache_record(None)
        unread["source"] = "coreutils-action-log-missing"
        unread["fresh_runner"] = False
        return unread
    shared = dict(shared)
    shared["source"] = "shared-directory"
    shared["fresh_runner"] = False
    return shared


def observe(
    execution: Path,
    bep: Path,
    access: str,
    ccache_statistics: dict[str, Any] | None = None,
    action_ccache: dict[str, int | None] | None = None,
    build_log_provided: bool = False,
) -> dict[str, Any]:
    actions = _records(execution)
    metrics = next(
        (record["buildMetrics"] for record in reversed(_records(bep)) if "buildMetrics" in record),
        {},
    )
    summary = metrics.get("actionSummary", {})
    timing = metrics.get("timingMetrics", {})
    runners = [action.get("runner", "") for action in actions]
    remote_hits = _buildbuddy_remote_hits(actions)
    shared = _ccache_record(ccache_statistics)
    ccache = _reported_ccache(shared, action_ccache, build_log_provided=build_log_provided)
    transfer = system_network_transfer(metrics if isinstance(metrics, dict) else {})
    return {
        "schema": 1,
        "buildbuddy_access": access,
        "remote_hits": remote_hits,
        "buildbuddy_remote_cache_hits": remote_hits,
        "remote_misses": None,
        "remote_uploads": None,
        "remote_downloads": None,
        "uploaded_bytes": transfer["uploaded_bytes"],
        "downloaded_bytes": transfer["downloaded_bytes"],
        "byte_source": transfer["byte_source"],
        "local_cache_hits": runners.count("disk cache hit"),
        "counters": _labeled_counters(runners, ccache, remote_hits),
        "ccache": ccache,
        "shared_ccache_directory": shared,
        "elapsed_ms": int(timing.get("wallTimeInMs", 0)),
        "actions_created": int(summary.get("actionsCreated", 0)),
        "actions_executed": int(summary.get("actionsExecuted", 0)),
    }


_BYTE_SOURCE = "buildMetrics.networkMetrics.systemNetworkStats"


def _decimal_byte_count(value: Any) -> int | None:
    """Bazel JSON encodes these uint64 counters as decimal strings."""
    if isinstance(value, str) and value.isdecimal():
        return int(value)
    return None


def system_network_transfer(metrics: dict[str, Any]) -> dict[str, Any]:
    """Host totals from BuildMetrics.networkMetrics.systemNetworkStats.

    bytesRecv and bytesSent are whole-process byte counters, including BES.
    They are not CAS-only. A missing field stays null.
    """
    network = metrics.get("networkMetrics")
    stats = network.get("systemNetworkStats") if isinstance(network, dict) else None
    if not isinstance(stats, dict):
        stats = {}
    downloaded = _decimal_byte_count(stats["bytesRecv"]) if "bytesRecv" in stats else None
    uploaded = _decimal_byte_count(stats["bytesSent"]) if "bytesSent" in stats else None
    return {
        "byte_source": _BYTE_SOURCE,
        "downloaded_bytes": downloaded,
        "uploaded_bytes": uploaded,
    }


def bep_transfer_bytes(bep: Path) -> dict[str, Any]:
    """Read systemNetworkStats from the build event file the recipe already writes."""
    metrics: dict[str, Any] = {}
    if bep.is_file():
        found = next(
            (
                record["buildMetrics"]
                for record in reversed(_records(bep))
                if isinstance(record, dict) and isinstance(record.get("buildMetrics"), dict)
            ),
            None,
        )
        if isinstance(found, dict):
            metrics = found
    return system_network_transfer(metrics)


def _load_profile(path: Path) -> Any:
    raw = path.read_bytes()
    if path.suffix == ".gz" or raw.startswith(b"\x1f\x8b"):
        raw = gzip.decompress(raw)
    return json.loads(raw)


def _profile_timing(profile: Path | None) -> tuple[int | None, list[dict[str, Any]]]:
    if profile is None or not profile.is_file():
        return None, []
    payload = _load_profile(profile)
    events = payload.get("traceEvents", []) if isinstance(payload, dict) else payload
    if not isinstance(events, list):
        return None, []
    critical = 0
    saw_critical = False
    phases: list[dict[str, Any]] = []
    for event in events:
        if not isinstance(event, dict):
            continue
        duration = event.get("dur")
        if not isinstance(duration, (int, float)):
            continue
        category = str(event.get("cat") or "")
        name = str(event.get("name") or "")
        if category == "critical path component":
            saw_critical = True
            critical += duration
        if category == "phase" or name.endswith(" phase"):
            duration_ms = int(duration / 1000)
            if duration_ms > _SLOW_PHASE_MS:
                phases.append({"duration_ms": duration_ms, "name": name})
    critical_ms = int(critical / 1000) if saw_critical else None
    phases.sort(key=lambda item: (-int(item["duration_ms"]), str(item["name"])))
    return critical_ms, phases


def _execution_fields(actions: list[dict[str, Any]]) -> dict[str, Any]:
    runners: dict[str, int] = {}
    mnemonics: dict[str, int] = {}
    labels: list[str] = []
    hits = 0
    for action in actions:
        if action.get("cacheHit") is True:
            hits += 1
        runner = action.get("runner")
        if isinstance(runner, str) and runner:
            runners[runner] = runners.get(runner, 0) + 1
        mnemonic = action.get("mnemonic")
        if isinstance(mnemonic, str) and mnemonic:
            mnemonics[mnemonic] = mnemonics.get(mnemonic, 0) + 1
            if mnemonic == "OrlixGuestPackage":
                label = action.get("targetLabel") or action.get("label")
                if isinstance(label, str) and label:
                    labels.append(label)
    return {
        "cacheHit": hits,
        "label": sorted(set(labels)),
        "mnemonic": dict(sorted(mnemonics.items())),
        "remote_cache_hits": _buildbuddy_remote_hits(actions),
        "runner": dict(sorted(runners.items())),
    }


def _orlixcc_lines(path: Path | None) -> int:
    if path is None or not path.is_file():
        return 0
    return sum(1 for line in path.read_text(errors="replace").splitlines() if _ORLIXCC.search(line))


def _ninja_compiler_edges(path: Path | None) -> int:
    if path is None or not path.is_file():
        return 0
    count = 0
    for line in path.read_text(errors="replace").splitlines():
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) < 4:
            continue
        if fields[3].endswith(_COMPILER_SUFFIXES):
            count += 1
    return count


def _acquisition(path: Path | None) -> dict[str, int]:
    empty = {"downloaded_bytes": 0, "local_store_hits": 0, "network_downloads": 0}
    if path is None or not path.is_file():
        return empty
    payload = json.loads(path.read_text())
    downloads = int(payload.get("network_downloads") or 0)
    hits = int(payload.get("local_store_hits") or 0)
    declared = payload.get("downloaded_bytes")
    if isinstance(declared, int):
        size = declared
    else:
        size = 0
        components = payload.get("components") or {}
        if isinstance(components, dict):
            for component in components.values():
                if not isinstance(component, dict) or component.get("source") != "network":
                    continue
                byte_size = component.get("bytes", component.get("downloaded_bytes", 0))
                if isinstance(byte_size, int):
                    size += byte_size
    return {"downloaded_bytes": size, "local_store_hits": hits, "network_downloads": downloads}


def _kibibytes(path: Path | None) -> int | None:
    if path is None or not path.exists():
        return None
    completed = subprocess.run(
        ["du", "-sk", str(path)],
        check=False,
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0 or not completed.stdout.split():
        return None
    return int(completed.stdout.split()[0])


def _content_digest(path: Path | None) -> str | None:
    if path is None or not path.is_dir():
        return None
    return _CONTENT_DIGEST.tree_digest(path)


def reduce_scenario(
    *,
    scenario: str,
    execution: Path,
    bep: Path,
    profile: Path | None = None,
    remote_grpc: Path | None = None,
    access: str = "off",
    orlixcc_log: Path | None = None,
    ninja_log: Path | None = None,
    acquisition: Path | None = None,
    output_base: Path | None = None,
    disk_cache: Path | None = None,
    repository_cache: Path | None = None,
    ccache_dir: Path | None = None,
    ccache_statistics: dict[str, Any] | None = None,
    product: Path | None = None,
    previous: Path | None = None,
) -> dict[str, Any]:
    """One scenario ledger row. It does not build."""
    if not scenario:
        raise ValueError("scenario name is required")
    metrics = next(
        (record["buildMetrics"] for record in reversed(_records(bep)) if "buildMetrics" in record),
        {},
    )
    wall = int(metrics.get("timingMetrics", {}).get("wallTimeInMs", 0))
    critical_ms, phases = _profile_timing(profile)
    digest = _content_digest(product)
    previous_digest = None
    if previous is not None and previous.is_file():
        previous_payload = json.loads(previous.read_text())
        if isinstance(previous_payload, dict):
            previous_digest = previous_payload.get("content_digest")
    matches = None
    if digest is not None and isinstance(previous_digest, str):
        matches = digest == previous_digest
    remote_bytes = None
    if access != "off":
        remote_bytes = remote_grpc.stat().st_size if remote_grpc is not None and remote_grpc.is_file() else 0
    actions = _records(execution)
    runners = [str(action.get("runner") or "") for action in actions]
    remote_hits = _buildbuddy_remote_hits(actions)
    ccache = _ccache_record(ccache_statistics)
    ccache["fresh_runner"] = False
    return {
        "schema": 1,
        "kind": "scenario-row",
        "scenario": scenario,
        "wallTimeInMs": wall,
        "critical_path_ms": critical_ms,
        "slow_phases": phases if wall > _SLOW_WALL_MS else [],
        "execution": _execution_fields(actions),
        "buildbuddy_remote_cache_hits": remote_hits,
        "counters": _labeled_counters(runners, ccache, remote_hits),
        "ccache": ccache,
        "remote_grpc_bytes": remote_bytes,
        "orlixcc_lines": _orlixcc_lines(orlixcc_log),
        "ninja_compiler_edges": _ninja_compiler_edges(ninja_log),
        "acquisition": _acquisition(acquisition),
        "disk_kibibytes": {
            "ccache": _kibibytes(ccache_dir),
            "disk_cache": _kibibytes(disk_cache),
            "output_base": _kibibytes(output_base),
            "repository_cache": _kibibytes(repository_cache),
        },
        "content_digest": digest,
        "content_digest_matches_previous": matches,
        "targets_unverified": {
            "status": "unverified-until-remeasured",
            "warm_noop_median_ms": 3000,
            "swift_terminal_engine_ms": 15000,
            "warm_promoted_ms": 3000,
            "warm_promoted_downloads": 0,
            "second_worktree_ms": 20000,
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--execution", type=Path, required=True)
    parser.add_argument("--bep", type=Path, required=True)
    parser.add_argument("--access", choices=("off", "read", "write"), required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--scenario")
    parser.add_argument("--profile", type=Path)
    parser.add_argument("--remote-grpc", type=Path)
    parser.add_argument("--orlixcc", type=Path)
    parser.add_argument("--ninja-log", type=Path)
    parser.add_argument("--acquisition", type=Path)
    parser.add_argument("--output-base", type=Path)
    parser.add_argument("--disk-cache", type=Path)
    parser.add_argument("--repository-cache", type=Path)
    parser.add_argument("--ccache-dir", type=Path)
    parser.add_argument("--ccache-statistics", type=Path)
    parser.add_argument("--build-log", type=Path)
    parser.add_argument("--product", type=Path)
    parser.add_argument("--previous", type=Path)
    args = parser.parse_args()
    ccache_statistics = load_ccache_statistics(args.ccache_statistics)
    action_ccache = None
    build_log_provided = args.build_log is not None
    if args.build_log is not None and args.build_log.is_file():
        action_ccache = coreutils_action_counters(args.build_log.read_text(encoding="utf-8", errors="replace"))
    if args.scenario:
        result = reduce_scenario(
            scenario=args.scenario,
            execution=args.execution,
            bep=args.bep,
            profile=args.profile,
            remote_grpc=args.remote_grpc,
            access=args.access,
            orlixcc_log=args.orlixcc,
            ninja_log=args.ninja_log,
            acquisition=args.acquisition,
            output_base=args.output_base,
            disk_cache=args.disk_cache,
            repository_cache=args.repository_cache,
            ccache_dir=args.ccache_dir,
            ccache_statistics=ccache_statistics,
            product=args.product,
            previous=args.previous,
        )
    else:
        result = observe(
            args.execution,
            args.bep,
            args.access,
            ccache_statistics,
            action_ccache,
            build_log_provided,
        )
    args.out.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
