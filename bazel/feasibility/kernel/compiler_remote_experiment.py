"""Make-facing kernel compiler-object experiments.

The producer may publish only from a trusted main push. A pull request never
receives that URL. The clean consumer uses an empty local ccache directory and
read-only remote storage. ``CCACHE_DISABLE=1`` stays on the cache-equivalence
consumer and is not how this consumer disables its local directory.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import time


WRITE_ENV = "ORLIX_COMPILER_REMOTE_STORAGE_WRITE"
READ_ENV = "ORLIX_COMPILER_REMOTE_STORAGE_READ"
MUTATION = Path("OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/kernel/time.c")
MUTATION_BEFORE = "orlix_host_time_monotonic_ns() + delta);"
MUTATION_AFTER = "orlix_host_time_monotonic_ns() + delta + 1);"
KERNEL_LABEL = "//bazel/feasibility/kernel:macho"
UAPI_LABEL = "//bazel/feasibility/kernel:uapi"


def readonly_url(url: str) -> str:
    if "read-only" in url.split("|"):
        return url
    return url + "|read-only"


def storage_scheme(url: str) -> str:
    scheme = url.split(":", 1)[0].split("|", 1)[0]
    if scheme in {"file", "http", "https", "redis", "redis+unix", "rediss"}:
        return scheme
    return "other"


def select_storage(*, event: str, ref: str, write_url: str, read_url: str, role: str) -> dict[str, str]:
    """Choose a remote URL. A pull request never receives the publication URL."""
    write_url = write_url.strip()
    read_url = read_url.strip()
    if event in {"pull_request", "pull_request_target"}:
        return {
            "access": "off",
            "url": "",
            "reason": "pull_request cannot publish or read the compiler-object cache with a publication credential",
        }
    trusted = ref in {"", "refs/heads/main"} and event in {"", "push", "workflow_dispatch"}
    if not trusted:
        return {"access": "off", "url": "", "reason": "push ref is not trusted main"}
    if role == "producer":
        if not write_url:
            return {"access": "off", "url": "", "reason": f"{WRITE_ENV} is unset"}
        if "read-only" in write_url.split("|"):
            return {"access": "off", "url": "", "reason": "producer URL is read-only"}
        return {"access": "write", "url": write_url, "reason": ""}
    url = read_url or write_url
    if not url:
        return {"access": "off", "url": "", "reason": "no compiler-object remote storage URL"}
    return {"access": "read", "url": readonly_url(url), "reason": ""}


def host_blocker() -> str | None:
    system = platform.system()
    if system != "Darwin":
        return (
            "kernel Mach-O Kbuild/clang proof did not execute: "
            f"this host is {system} {platform.release()} {platform.machine()}, not macOS"
        )
    developer = os.environ.get("ORLIX_PINNED_DEVELOPER_DIR", "")
    if not developer or not Path(developer).is_dir():
        return (
            "kernel Mach-O Kbuild/clang proof did not execute: "
            "ORLIX_PINNED_DEVELOPER_DIR is not a directory"
        )
    return None


def _blocked(kind: str, blocker: str) -> dict[str, object]:
    return {
        "experiment": kind,
        "status": "not_executed",
        "executed": False,
        "blocker": blocker,
        "remote_object_hits": None,
        "compiler_processes_executed": None,
        "transferred_bytes": None,
        "elapsed_seconds": None,
        "output_digest": None,
        "installed_uapi_digest": None,
        "installed_uapi_byte_identical": None,
        "mlibc_rebuilt": None,
        "units": None,
        "local_compiler_cache": "not used",
        "github_actions_cache": "orlix-ccache-${compiler digest} is not this store",
    }


def _write(path: Path, record: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(record, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def bazel_flags(
    *,
    bazel: str,
    output_base: Path,
    developer_dir: str,
    xcode_version: str,
    ccache_dir: Path,
    remote_url: str,
    remote_only: bool,
    label: str,
    execution_log: Path | None = None,
    ccache_disable: bool = False,
) -> list[str]:
    """Flags for one kernel action. Bazel result caches stay off."""
    flags = [
        bazel,
        "--output_base=" + str(output_base),
        "build",
        label,
        "--compilation_mode=dbg",
        "--config=release",
        "--config=source",
        "--apple_platform_type=ios",
        "--ios_multi_cpus=sim_arm64",
        "--platforms=@build_bazel_apple_support//platforms:ios_sim_arm64",
        "--xcode_version=" + xcode_version,
        "--nouse_action_cache",
        "--disk_cache=",
        "--noremote_accept_cached",
        "--remote_cache=",
        "--remote_executor=",
        "--repo_env=DEVELOPER_DIR=" + developer_dir,
        "--host_action_env=ORLIX_PINNED_DEVELOPER_DIR=" + developer_dir,
        "--action_env=ORLIX_PINNED_DEVELOPER_DIR=" + developer_dir,
        "--action_env=CCACHE_DIR=" + str(ccache_dir),
    ]
    if ccache_disable:
        flags.append("--action_env=CCACHE_DISABLE=1")
    else:
        flags.append("--action_env=CCACHE_DISABLE=")
        if remote_url:
            flags.append("--action_env=ORLIX_COMPILER_REMOTE_STORAGE=" + remote_url)
        if remote_only:
            flags.append("--action_env=ORLIX_COMPILER_REMOTE_ONLY=1")
    if execution_log is not None:
        flags.extend([
            "--execution_log_compact_file=",
            "--execution_log_json_file=" + str(execution_log),
        ])
    return flags


def _identity_digest(output_base: Path) -> str:
    matches = sorted(output_base.rglob("uapi.artifact-identity-v2.sha256"))
    if len(matches) != 1:
        raise RuntimeError(f"expected one installed UAPI identity digest, found {len(matches)}")
    return matches[0].read_text(encoding="utf-8").strip()


def _run(command: list[str], env: dict[str, str]) -> None:
    completed = subprocess.run(command, env=env, check=False)
    if completed.returncode != 0:
        raise RuntimeError(f"command exited {completed.returncode}: {command[0]} {command[2] if len(command) > 2 else ''}")


def _record_path(output_base: Path) -> Path:
    return output_base / "orlix-kernel-state" / "release" / "iphonesimulator" / "compiler-object-record.json"


def _execute(kind: str, output: Path, bazel: str, developer_dir: str, xcode_version: str) -> int:
    event = os.environ.get("GITHUB_EVENT_NAME", "")
    ref = os.environ.get("GITHUB_REF", "")
    producer = select_storage(
        event=event,
        ref=ref,
        write_url=os.environ.get(WRITE_ENV, ""),
        read_url=os.environ.get(READ_ENV, ""),
        role="producer",
    )
    consumer = select_storage(
        event=event,
        ref=ref,
        write_url=os.environ.get(WRITE_ENV, ""),
        read_url=os.environ.get(READ_ENV, ""),
        role="consumer",
    )
    if producer["access"] != "write" or not producer["url"]:
        _write(output, _blocked(kind, producer["reason"] or "trusted producer remote storage is not configured"))
        return 2
    if consumer["access"] != "read" or not consumer["url"]:
        _write(output, _blocked(kind, consumer["reason"] or "clean consumer remote storage is not configured"))
        return 2
    root = Path.cwd()
    proof = root / "Build" / "AgentHarness" / "bazel-migration" / "compiler-remote" / kind
    producer_base = proof / "producer-output-base"
    consumer_base = proof / "consumer-output-base"
    producer_cache = proof / "producer-ccache"
    consumer_cache = proof / "consumer-ccache"
    for path in (producer_base, consumer_base, producer_cache, consumer_cache):
        if path.exists():
            shutil.rmtree(path)
        path.mkdir(parents=True)
    env = os.environ.copy()
    env.pop("CCACHE_DISABLE", None)
    env["ORLIX_PINNED_DEVELOPER_DIR"] = developer_dir
    started = time.monotonic()
    producer_log = proof / "producer-execution.json"
    _run(bazel_flags(
        bazel=bazel,
        output_base=producer_base,
        developer_dir=developer_dir,
        xcode_version=xcode_version,
        ccache_dir=producer_cache,
        remote_url=producer["url"],
        remote_only=False,
        label=KERNEL_LABEL,
        execution_log=producer_log,
    ), env)
    original = (root / MUTATION).read_bytes()
    mutated = False
    try:
        if kind == "source-edit":
            text = original.decode()
            if MUTATION_BEFORE not in text:
                raise RuntimeError("kernel time.c mutation site is missing")
            (root / MUTATION).write_bytes(text.replace(MUTATION_BEFORE, MUTATION_AFTER, 1).encode())
            mutated = True
        consumer_log = proof / "consumer-execution.json"
        consumer_started = time.monotonic()
        _run(bazel_flags(
            bazel=bazel,
            output_base=consumer_base,
            developer_dir=developer_dir,
            xcode_version=xcode_version,
            ccache_dir=consumer_cache,
            remote_url=consumer["url"],
            remote_only=True,
            label=KERNEL_LABEL,
            execution_log=consumer_log,
        ), env)
        elapsed = time.monotonic() - consumer_started
        record_path = _record_path(consumer_base)
        if not record_path.is_file():
            raise RuntimeError(f"missing compiler-object record: {record_path}")
        detail = json.loads(record_path.read_text(encoding="utf-8"))
        from bazel.feasibility.kernel.compiler_object_recorder import summarize

        summary = summarize(detail)
        uapi_identical = None
        uapi_digest = None
        if kind == "source-edit":
            baseline_uapi = proof / "baseline-uapi"
            mutated_uapi = proof / "mutated-uapi"
            for path in (baseline_uapi, mutated_uapi):
                if path.exists():
                    shutil.rmtree(path)
                path.mkdir()
            uapi_env = dict(env)
            uapi_env["CCACHE_DISABLE"] = "1"
            _run(bazel_flags(
                bazel=bazel,
                output_base=baseline_uapi,
                developer_dir=developer_dir,
                xcode_version=xcode_version,
                ccache_dir=proof / "uapi-ccache-off",
                remote_url="",
                remote_only=False,
                label=UAPI_LABEL,
                ccache_disable=True,
            ), uapi_env)
            _run(bazel_flags(
                bazel=bazel,
                output_base=mutated_uapi,
                developer_dir=developer_dir,
                xcode_version=xcode_version,
                ccache_dir=proof / "uapi-ccache-off-mutated",
                remote_url="",
                remote_only=False,
                label=UAPI_LABEL,
                ccache_disable=True,
            ), uapi_env)
            baseline_digest = _identity_digest(baseline_uapi)
            mutated_digest = _identity_digest(mutated_uapi)
            uapi_digest = mutated_digest
            uapi_identical = baseline_digest == mutated_digest
        log_text = consumer_log.read_text(encoding="utf-8") if consumer_log.is_file() else ""
        mlibc_rebuilt = "OrlixMLibCSysroot" in log_text
        units = summary["units"]
        changed = []
        if isinstance(units, list):
            changed = [
                unit for unit in units
                if isinstance(unit, dict) and str(unit.get("unit", "")).endswith("arch/orlix/kernel/time.c")
            ]
        record = {
            "experiment": kind,
            "status": "executed",
            "executed": True,
            "blocker": "",
            "remote_storage_scheme": storage_scheme(consumer["url"]),
            "remote_object_hits": summary["remote_object_hits"],
            "compiler_processes_executed": summary["compiler_processes_executed"],
            "transferred_bytes": summary["transferred_bytes"],
            "elapsed_seconds": elapsed,
            "producer_elapsed_seconds": consumer_started - started,
            "output_digest": summary["output_digest"],
            "installed_uapi_digest": uapi_digest,
            "installed_uapi_byte_identical": uapi_identical,
            "mlibc_rebuilt": mlibc_rebuilt,
            "changed_unit": changed,
            "units": units,
            "local_compiler_cache": "empty consumer directory",
            "github_actions_cache": "orlix-ccache-${compiler digest} is not this store",
        }
        if kind == "source-edit" and uapi_identical is not True:
            record["status"] = "failed"
        if mlibc_rebuilt:
            record["status"] = "failed"
        _write(output, record)
        return 0 if record["status"] == "executed" else 2
    finally:
        if mutated:
            (root / MUTATION).write_bytes(original)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--kind", required=True, choices=("same", "edit"))
    parser.add_argument("--output", required=True)
    parser.add_argument("--bazel", default=os.environ.get("ORLIX_BAZEL", ""))
    parser.add_argument("--developer-dir", default=os.environ.get("ORLIX_PINNED_DEVELOPER_DIR", ""))
    parser.add_argument("--xcode-version", default=os.environ.get("ORLIX_XCODE_VERSION", ""))
    parser.add_argument("--xcode-build", default=os.environ.get("ORLIX_XCODE_BUILD", ""))
    args = parser.parse_args(argv)
    kind = "same-commit" if args.kind == "same" else "source-edit"
    output = Path(args.output)
    if args.developer_dir:
        os.environ["ORLIX_PINNED_DEVELOPER_DIR"] = args.developer_dir
    blocker = host_blocker()
    if blocker:
        _write(output, _blocked(kind, blocker))
        print(blocker, file=sys.stderr)
        return 2
    if not args.bazel:
        blocker = "kernel Mach-O Kbuild/clang proof did not execute: ORLIX_BAZEL is unset"
        _write(output, _blocked(kind, blocker))
        print(blocker, file=sys.stderr)
        return 2
    try:
        return _execute(kind, output, args.bazel, args.developer_dir, args.xcode_version)
    except (OSError, RuntimeError, ValueError) as error:
        _write(output, _blocked(kind, str(error)))
        print(str(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
