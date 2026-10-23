"""Remote reuse of OrlixKernel compiler objects on the macOS kernel action.

The build is ``//bazel/feasibility/kernel:kernel-release-iphonesimulator``.
Upstream Kbuild still chooses the units. ccache talks to a content-addressed
Actions cache through the existing launcher. The Bazel action runs locally,
so a remote hit of the whole Mach-O archive is not the result.
"""

from __future__ import annotations

import json
import os
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from bazel.compiler_object_remote import ActionsBlobStore, actions_runtime, remote_storage_url, serve
from bazel.config.cache_observation import _records


TARGET = "//bazel/feasibility/kernel:kernel-release-iphonesimulator"
KERNEL_TREE = "@orlix_linux_source linux-6.12.105-port"
MNEMONIC = "OrlixKernelMachOArchive"


def main(argv: list[str]) -> None:
    if len(argv) != 2 or argv[1] not in {"produce", "reuse", "mutate"}:
        raise SystemExit("usage: kernel_archive_remote_reuse.py produce|reuse|mutate")
    mode = argv[1]
    writing = os.environ.get("ORLIX_COMPILER_OBJECT_REMOTE_WRITE") == "1"
    if mode == "produce" and not writing:
        raise SystemExit("the producer requires ORLIX_COMPILER_OBJECT_REMOTE_WRITE=1")
    if mode != "produce" and writing:
        raise SystemExit("clean runners do not get compiler-object write authorization")
    work = Path(os.environ.get("ORLIX_KERNEL_OBJECT_REMOTE_WORK", "Build/kernel-object-remote-reuse")).resolve()
    producer = None
    if mode != "produce":
        producer_path = Path(os.environ["ORLIX_KERNEL_OBJECT_REMOTE_PRODUCER"])
        if not producer_path.is_file():
            raise SystemExit("main has not published kernel compiler objects")
        producer = json.loads(producer_path.read_text(encoding="utf-8"))
    evidence = run_mode(mode, work, writing=writing, producer=producer)
    destination = work / "evidence.json"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(evidence, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(evidence, indent=2, sort_keys=True), flush=True)


def run_mode(mode: str, work: Path, *, writing: bool, producer: dict | None) -> dict:
    bazel = require_env("ORLIX_BAZEL")
    developer = require_env("ORLIX_PINNED_DEVELOPER_DIR")
    xcode = require_env("ORLIX_XCODE_VERSION")
    repository = require_env("ORLIX_BAZEL_REPOSITORY_CACHE")
    if not Path(bazel).is_file():
        raise SystemExit(f"bazel is missing: {bazel}")
    if not Path(developer).is_dir():
        raise SystemExit(f"Xcode developer directory is missing: {developer}")
    work.mkdir(parents=True, exist_ok=True)
    ccache_dir = work / "ccache"
    ccache_dir.mkdir(parents=True, exist_ok=True)
    logfile = work / "ccache.log"
    logfile.write_text("", encoding="utf-8")
    output_base = work / "output-base"
    execution_log = work / "execution.json"
    store = actions_store(allow_write=writing)
    httpd, port = serve(store)
    try:
        remote = remote_storage_url(port, read_only=not writing)
        command = bazel_command(
            bazel=bazel,
            output_base=output_base,
            developer=developer,
            xcode=xcode,
            repository=repository,
            ccache_dir=ccache_dir,
            logfile=logfile,
            remote=remote,
            execution_log=execution_log,
        )
        env = os.environ.copy()
        env["DEVELOPER_DIR"] = developer
        env.pop("BUILDBUDDY_API_KEY", None)
        env.pop("ORLIX_CI_BUILDBUDDY_WRITE_API_KEY", None)
        completed = subprocess.run(command, cwd=ROOT, env=env, check=False, text=True)
        if completed.returncode != 0:
            raise SystemExit(f"kernel archive build failed: {completed.returncode}")
        action = kernel_action(execution_log)
        if action.get("cacheHit") is True or "remote cache hit" in str(action.get("runner", "")):
            raise SystemExit("the whole Mach-O archive was a Bazel remote cache hit")
        stats = ccache_log_stats(logfile.read_text(encoding="utf-8", errors="replace"))
        digest = archive_digest(output_base)
        evidence = {
            "schema": 2,
            "mode": mode,
            "commit": subprocess.check_output(
                ["git", "-C", str(ROOT), "rev-parse", "HEAD"], text=True
            ).strip(),
            "kernel_tree": KERNEL_TREE,
            "target": TARGET,
            "compiler": "staged guest clang through ORLIX_COMPILER_LAUNCHER and ccache",
            "runner": os.environ.get("RUNNER_OS", ""),
            "archive_digest": digest,
            "action": action,
            "ccache": stats,
            "remote_read_bytes": store.read_bytes,
            "remote_write_bytes": store.write_bytes,
            "kbuild_command": action.get("command", ""),
        }
    finally:
        httpd.shutdown()
    check_mode(mode, evidence, producer)
    return evidence


def check_mode(mode: str, evidence: dict, producer: dict | None) -> None:
    stats = evidence["ccache"]
    if mode == "produce":
        if stats["misses"] < 1 or evidence["remote_write_bytes"] < 1:
            raise SystemExit("producer did not store compiler objects")
        return
    if producer is None:
        raise SystemExit("main has not published kernel compiler objects")
    if evidence["remote_write_bytes"] != 0:
        raise SystemExit("read-only runner stored compiler objects")
    if stats["direct_hits"] < 1:
        raise SystemExit("unchanged units did not reuse remote compiler objects")
    if mode == "reuse":
        if stats["misses"] != 0:
            raise SystemExit("same commit recompiled a unit")
        if evidence["archive_digest"] != producer["archive_digest"]:
            raise SystemExit("same commit produced a different kernel archive")
        return
    if stats["misses"] < 1:
        raise SystemExit("commit B did not compile a changed unit")
    if evidence["archive_digest"] == producer["archive_digest"]:
        raise SystemExit("commit B did not change the kernel archive")


def bazel_command(
    *,
    bazel: str,
    output_base: Path,
    developer: str,
    xcode: str,
    repository: str,
    ccache_dir: Path,
    logfile: Path,
    remote: str,
    execution_log: Path,
) -> list[str]:
    return [
        bazel,
        "--batch",
        f"--output_base={output_base}",
        "build",
        TARGET,
        "--compilation_mode=dbg",
        "--config=release",
        "--config=source",
        "--apple_platform_type=ios",
        "--ios_multi_cpus=sim_arm64",
        "--platforms=@build_bazel_apple_support//platforms:ios_sim_arm64",
        f"--xcode_version={xcode}",
        "--noremote_accept_cached",
        "--remote_cache=",
        "--remote_executor=",
        "--remote_upload_local_results=false",
        "--nouse_action_cache",
        f"--repo_env=DEVELOPER_DIR={developer}",
        f"--host_action_env=ORLIX_PINNED_DEVELOPER_DIR={developer}",
        f"--action_env=ORLIX_PINNED_DEVELOPER_DIR={developer}",
        f"--action_env=CCACHE_DIR={ccache_dir}",
        f"--action_env=CCACHE_REMOTE_STORAGE={remote}",
        "--action_env=CCACHE_REMOTE_ONLY=true",
        "--action_env=CCACHE_NOINODECACHE=true",
        f"--action_env=CCACHE_LOGFILE={logfile}",
        "--disk_cache=",
        f"--repository_cache={repository}",
        "--strategy=OrlixKernelMachOArchive=local",
        "--execution_log_compact_file=",
        f"--execution_log_json_file={execution_log}",
    ]


def kernel_action(path: Path) -> dict:
    if not path.is_file():
        raise SystemExit("kernel execution log is missing")
    records = _records(path)
    matches = [item for item in records if isinstance(item, dict) and item.get("mnemonic") == MNEMONIC]
    if len(matches) != 1:
        raise SystemExit(f"expected one {MNEMONIC} action, found {len(matches)}")
    item = matches[0]
    command = item.get("commandArgs") or item.get("argv") or item.get("args") or []
    if isinstance(command, list):
        command = " ".join(str(part) for part in command)
    return {
        "mnemonic": MNEMONIC,
        "targetLabel": item.get("targetLabel", ""),
        "runner": item.get("runner", ""),
        "cacheHit": item.get("cacheHit", False),
        "exitCode": item.get("exitCode", 0),
        "command": command,
    }


def ccache_log_stats(text: str) -> dict:
    return {
        "direct_hits": text.count("Result: direct_cache_hit"),
        "preprocessed_hits": text.count("Result: preprocessed_cache_hit"),
        "misses": text.count("Result: cache_miss"),
    }


def archive_digest(output_base: Path) -> str:
    matches = sorted(output_base.rglob("kernel.artifact-identity-v2.sha256"))
    if not matches:
        matches = sorted(output_base.rglob("OrlixKernel.a.sha256"))
    if not matches:
        raise SystemExit("kernel archive digest is missing")
    digest = matches[0].read_text(encoding="utf-8").strip().split()[0]
    if len(digest) != 64:
        raise SystemExit(f"kernel archive digest is not sha256: {digest}")
    return digest


def actions_store(*, allow_write: bool) -> ActionsBlobStore:
    url, token, service_v2 = actions_runtime(os.environ)
    return ActionsBlobStore(url, token, allow_write=allow_write, service_v2=service_v2)


def require_env(name: str) -> str:
    value = os.environ.get(name, "")
    if not value:
        raise SystemExit(f"{name} is required")
    return value


if __name__ == "__main__":
    main(sys.argv)
