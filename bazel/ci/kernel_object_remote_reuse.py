"""Remote reuse of unchanged Linux kernel compiler objects.

Upstream Kbuild decides which units to compile. ccache is the existing
compiler launcher. Results are addressed by ccache's digest and stored
through the Bazel HTTP layout. The proof backend is the GitHub Actions
cache. A local ccache directory, a file: URL, and a BuildBuddy hit of the
whole kernel archive do not satisfy it.

produce writes. reuse and mutate are clean runners and are read-only.
"""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
import urllib.request


ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from bazel.compiler_object_remote import ActionsBlobStore, actions_runtime, remote_storage_url, serve


LINUX_URL = "https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.12.105.tar.xz"
LINUX_SHA256 = "eb36801e119529b13513c3459dc20e2a32f7053629f3aabb63ea501a4d88f63d"
LINUX_REVISION = "6.12.105"
MUTATION_SYMBOL = "orlix_kernel_object_remote_reuse_mutation"
MUTATION_SOURCE = "kernel/locking/semaphore.c"
_LOCKING_SOURCE = re.compile(r"kernel/locking/[A-Za-z0-9_.+-]+\.c")
_COMPILER = {"gcc", "cc1", "collect2"}


def main(argv: list[str]) -> None:
    if len(argv) != 2 or argv[1] not in {"produce", "reuse", "mutate"}:
        raise SystemExit("usage: kernel_object_remote_reuse.py produce|reuse|mutate")
    mode = argv[1]
    writing = os.environ.get("ORLIX_COMPILER_OBJECT_REMOTE_WRITE") == "1"
    if mode == "produce" and not writing:
        raise SystemExit("the producer requires ORLIX_COMPILER_OBJECT_REMOTE_WRITE=1")
    if mode != "produce" and writing:
        raise SystemExit("clean runners do not get compiler-object write authorization")
    work = Path(os.environ.get("ORLIX_KERNEL_OBJECT_REMOTE_WORK", "Build/kernel-object-remote-reuse")).resolve()
    evidence_path = work / "evidence.json"
    producer_path = Path(
        os.environ.get("ORLIX_KERNEL_OBJECT_REMOTE_PRODUCER", str(work / "producer" / "evidence.json"))
    )
    producer = json.loads(producer_path.read_text(encoding="utf-8")) if mode != "produce" else None
    evidence = run_mode(mode, work, writing=writing, producer=producer)
    evidence_path.parent.mkdir(parents=True, exist_ok=True)
    evidence_path.write_text(json.dumps(evidence, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(evidence, indent=2, sort_keys=True), flush=True)


def run_mode(mode: str, work: Path, *, writing: bool, producer: dict | None) -> dict:
    gcc = require_tool("gcc")
    gxx = require_tool("g++")
    ccache = require_tool("ccache")
    require_tool("make")
    require_tool("strace")
    require_tool("nm")
    compiler = compiler_identity(gcc)
    ccache_version = subprocess.check_output([ccache, "--version"], text=True).splitlines()[0].strip()
    if producer is not None:
        if producer["compiler"]["sha256"] != compiler["sha256"] or producer["compiler"]["cc1_sha256"] != compiler["cc1_sha256"]:
            raise SystemExit("compiler identity differs from the producer; refusing to call that a hit")
        if producer["ccache_version"] != ccache_version:
            raise SystemExit("ccache version differs from the producer")
    source = ensure_linux(work)
    if mode == "mutate":
        mutate_semaphore(source / MUTATION_SOURCE)
    build = work / "kbuild"
    ccache_dir = work / "ccache"
    if ccache_dir.exists():
        shutil.rmtree(ccache_dir)
    ccache_dir.mkdir(parents=True)
    store = actions_store(allow_write=writing)
    if producer is not None:
        wait_for_sentinel(store, producer["sentinel_digest"], producer["sentinel_identity"])
    httpd, port = serve(store)
    try:
        env = ccache_environment(
            work=work,
            ccache_dir=ccache_dir,
            port=port,
            read_only=not writing,
        )
        logs = work / "logs"
        logs.mkdir(parents=True, exist_ok=True)
        prepare_commands = prepare_kbuild(source, build, gcc, gxx, ccache, env, logs / "prepare.log")
        subprocess.run([ccache, "-z"], env=env, check=True, stdout=subprocess.DEVNULL)
        Path(env["CCACHE_LOGFILE"]).write_text("", encoding="utf-8")
        read_before = store.read_bytes
        write_before = store.write_bytes
        command = kbuild_command(source, build, gcc, gxx, ccache, "kernel/locking/built-in.a")
        strace_path = logs / "phase3.strace"
        log_path = logs / "phase3.log"
        status = run_logged(["strace", "-f", "-e", "trace=execve", "-s", "262144", "-o", str(strace_path), *command], env, log_path)
        if status != 0:
            raise SystemExit(f"kbuild failed\n{log_tail(log_path)}")
        stats_text = subprocess.check_output([ccache, "-sv"], env=env, text=True)
        machine_text = subprocess.check_output([ccache, "--print-stats"], env=env, text=True)
        counters = parse_print_stats(machine_text)
        remote = remote_counters(counters)
        direct = counter(counters, "direct_cache_hit")
        preprocessed = counter(counters, "preprocessed_cache_hit")
        executions = compiler_executions(strace_path.read_text(encoding="utf-8", errors="replace"))
        log_text = Path(env["CCACHE_LOGFILE"]).read_text(encoding="utf-8", errors="replace")
        logged = ccache_source_results(log_text)
        objects = collect_objects(build)
        archive = build / "kernel/locking/built-in.a"
        if not archive.is_file() or archive.stat().st_size == 0:
            raise SystemExit("Kbuild did not link kernel/locking/built-in.a")
        phase_log = log_path.read_text(encoding="utf-8", errors="replace")
        evidence = {
            "mode": mode,
            "linux_revision": LINUX_REVISION,
            "linux_archive_sha256": LINUX_SHA256,
            "compiler": compiler,
            "ccache_version": ccache_version,
            "kbuild_command": command,
            "prepare_commands": prepare_commands,
            "objects": objects,
            "archive": {
                "path": "kernel/locking/built-in.a",
                "sha256": sha256_file(archive),
                "bytes": archive.stat().st_size,
            },
            "archive_symbols": defined_symbols(archive),
            "remote_storage": "github-actions-cache",
            "remote_layout": "ccache-bazel-http",
            "ccache_remote": remote,
            "ccache_direct_hits": direct,
            "ccache_preprocessed_hits": preprocessed,
            "ccache_stats": stats_text,
            "ccache_counters": counters,
            "transfer_bytes": {
                "read": store.read_bytes - read_before,
                "write": store.write_bytes - write_before,
            },
            "compiler_executions": executions,
            "semaphore_symbols": defined_symbols(build / "kernel/locking/semaphore.o"),
            "ccache_results": logged,
            "compile_lines": [line for line in phase_log.splitlines() if "kernel/locking/" in line and ".c" in line],
            "link_lines": [line for line in phase_log.splitlines() if "built-in.a" in line and ("ar " in line or "ar: " in line or "ld " in line)],
        }
        check_mode(evidence, producer)
        if mode == "produce":
            identity = sentinel_identity(objects)
            digest = hashlib.sha256(identity).hexdigest()
            store.put(digest, identity)
            evidence["sentinel_digest"] = digest
            evidence["sentinel_identity"] = identity.decode()
        return evidence
    finally:
        httpd.shutdown()


def check_mode(evidence: dict, producer: dict | None) -> None:
    objects = evidence["objects"]
    if "kernel/locking/semaphore.o" not in objects or len(objects) < 3:
        raise SystemExit(f"locking objects were not built: {sorted(objects)}")
    remote = evidence["ccache_remote"]
    if remote["errors"] or remote["timeouts"]:
        raise SystemExit(f"remote storage errors: {remote}")
    executions = evidence["compiler_executions"]
    if evidence["mode"] == "produce":
        if evidence["transfer_bytes"]["write"] <= 0 or remote["misses"] <= 0 or remote["writes"] <= 0:
            raise SystemExit(
                "producer did not miss and upload compiler objects: "
                f"{remote} transfer={evidence['transfer_bytes']}"
            )
        if not executions:
            raise SystemExit("producer did not execute the real compiler")
        if MUTATION_SYMBOL in evidence["archive_symbols"]:
            raise SystemExit("producer archive already contains the mutation")
        return
    assert producer is not None
    if evidence["transfer_bytes"]["write"] != 0:
        raise SystemExit("read-only runner uploaded compiler objects")
    if evidence["transfer_bytes"]["read"] <= 0 or remote["hits"] <= 0:
        raise SystemExit("clean runner did not read remote compiler objects")
    results = evidence["ccache_results"]
    preprocessed = sorted(name for name, meta in results.items() if "preprocessed" in str(meta["result"]))
    if preprocessed:
        raise SystemExit(f"preprocessed hit ran the compiler front end: {preprocessed}")
    if "kernel/locking/semaphore.c" not in results or len(results) < 3:
        raise SystemExit(f"ccache log did not record the locking units: {sorted(results)}")
    if evidence["mode"] == "reuse":
        if executions:
            raise SystemExit(f"unchanged units executed the compiler: {executions}")
        missed = sorted(
            name
            for name, meta in results.items()
            if meta["executed"] or "direct" not in str(meta["result"])
        )
        if missed:
            raise SystemExit(f"unchanged units were not direct remote hits: {missed}")
        if evidence["ccache_direct_hits"] < len(results):
            raise SystemExit("direct hit count is below the recorded locking units")
        compare_objects(objects, producer["objects"], changed=set())
        if MUTATION_SYMBOL in evidence["archive_symbols"]:
            raise SystemExit("reused archive contains the mutation")
    else:
        changed = {MUTATION_SOURCE}
        if set(executions) != changed:
            raise SystemExit(f"mutation compiler executions were {executions}")
        for name, meta in results.items():
            if name == MUTATION_SOURCE:
                if not meta["executed"] or "miss" not in str(meta["result"]):
                    raise SystemExit(f"mutated unit was not a remote miss: {meta}")
            elif meta["executed"] or "direct" not in str(meta["result"]):
                raise SystemExit(f"{name} was not a direct remote hit: {meta}")
        compare_objects(objects, producer["objects"], changed={"kernel/locking/semaphore.o"})
        if MUTATION_SYMBOL not in evidence["archive_symbols"]:
            raise SystemExit("linked archive is missing the mutated symbol")
        if MUTATION_SYMBOL not in evidence["semaphore_symbols"]:
            raise SystemExit("mutated object is missing its symbol")
    if not evidence["link_lines"]:
        raise SystemExit("Kbuild log has no archive link command")
    for name in ("down", "mutex_lock"):
        if name not in evidence["archive_symbols"]:
            raise SystemExit(f"linked archive is missing {name}")


def compare_objects(actual: dict, expected: dict, *, changed: set[str]) -> None:
    if set(actual) != set(expected):
        raise SystemExit(f"object set changed: {sorted(set(actual) ^ set(expected))}")
    for name, meta in expected.items():
        same = actual[name]["sha256"] == meta["sha256"]
        if name in changed and same:
            raise SystemExit(f"{name} was unchanged after the source edit")
        if name not in changed and not same:
            raise SystemExit(f"{name} changed even though its source did not")


def actions_store(*, allow_write: bool) -> ActionsBlobStore:
    url, token, service_v2 = actions_runtime(os.environ)
    return ActionsBlobStore(url, token, allow_write=allow_write, service_v2=service_v2)


def wait_for_sentinel(store: ActionsBlobStore, digest: str, identity: str) -> None:
    deadline = time.time() + 180
    while time.time() < deadline:
        blob = store.get(digest)
        if blob is not None:
            if blob.decode() != identity:
                raise SystemExit("remote sentinel bytes do not match the producer identity")
            return
        time.sleep(2)
    raise SystemExit("producer compiler-object sentinel was not visible to this runner")


def ensure_linux(work: Path) -> Path:
    source = work / "src" / f"linux-{LINUX_REVISION}"
    if (source / "Makefile").is_file() and (source / MUTATION_SOURCE).is_file():
        return source
    archive = work / f"linux-{LINUX_REVISION}.tar.xz"
    work.mkdir(parents=True, exist_ok=True)
    if not archive.is_file() or sha256_file(archive) != LINUX_SHA256:
        print(f"downloading {LINUX_URL}", flush=True)
        urllib.request.urlretrieve(LINUX_URL, archive)
        digest = sha256_file(archive)
        if digest != LINUX_SHA256:
            raise SystemExit(f"linux archive digest {digest} != {LINUX_SHA256}")
    dest = work / "src"
    if dest.exists():
        shutil.rmtree(dest)
    dest.mkdir(parents=True)
    subprocess.run(["tar", "-xJf", str(archive), "-C", str(dest)], check=True)
    if not (source / "Makefile").is_file():
        raise SystemExit("extracted linux tree has no Makefile")
    return source


def mutate_semaphore(path: Path) -> None:
    text = path.read_text(encoding="utf-8")
    if MUTATION_SYMBOL in text:
        raise SystemExit(f"{path} already contains the mutation")
    path.write_text(
        text
        + "\nvoid "
        + MUTATION_SYMBOL
        + "(void);\nvoid "
        + MUTATION_SYMBOL
        + "(void)\n{\n}\n",
        encoding="utf-8",
    )


def prepare_kbuild(source: Path, build: Path, gcc: str, gxx: str, ccache: str, env: dict[str, str], log_path: Path) -> list[list[str]]:
    commands = [kbuild_command(source, build, gcc, gxx, ccache, "defconfig")]
    status = run_logged(commands[0], env, log_path)
    if status != 0:
        raise SystemExit(f"kbuild prepare failed\n{log_tail(log_path)}")
    config = source / "scripts" / "config"
    build_config = build / ".config"
    for symbol in ("DEBUG_INFO_BTF", "DEBUG_INFO_BTF_MODULES"):
        command = [str(config), "--file", str(build_config), "--disable", symbol]
        commands.append(command)
        status = run_logged(command, env, log_path)
        if status != 0:
            raise SystemExit(f"kbuild prepare failed\n{log_tail(log_path)}")
    for target in ("olddefconfig", "prepare"):
        command = kbuild_command(source, build, gcc, gxx, ccache, target)
        commands.append(command)
        status = run_logged(command, env, log_path)
        if status != 0:
            raise SystemExit(f"kbuild prepare failed\n{log_tail(log_path)}")
    return commands


def kbuild_command(source: Path, build: Path, gcc: str, gxx: str, ccache: str, target: str) -> list[str]:
    jobs = str(min(4, os.cpu_count() or 2))
    return [
        "make",
        "-C",
        str(source),
        f"O={build}",
        "ARCH=x86_64",
        f"CC={ccache} {gcc}",
        f"HOSTCC={gcc}",
        f"HOSTCXX={gxx}",
        "KBUILD_BUILD_USER=orlix",
        "KBUILD_BUILD_HOST=orlix",
        "KBUILD_BUILD_TIMESTAMP=1970-01-01",
        "KBUILD_BUILD_VERSION=1",
        "-j",
        jobs,
        "V=1",
        target,
    ]


def ccache_environment(*, work: Path, ccache_dir: Path, port: int, read_only: bool) -> dict[str, str]:
    env = os.environ.copy()
    env.update(
        {
            "CCACHE_REMOTE_STORAGE": remote_storage_url(port, read_only=read_only),
            "CCACHE_REMOTE_ONLY": "true",
            "CCACHE_COMPILERCHECK": "content",
            "CCACHE_BASEDIR": str(work),
            "CCACHE_DIR": str(ccache_dir),
            "CCACHE_CONFIGPATH": "/dev/null",
            "CCACHE_NOHASHDIR": "true",
            "CCACHE_DEBUG": "true",
            "CCACHE_DEBUGDIR": str(work / "ccache-debug"),
            "CCACHE_LOGFILE": str(work / "logs" / "ccache.log"),
            "CCACHE_MAXSIZE": "1G",
            "CCACHE_SLOPPINESS": "include_file_mtime,include_file_ctime,locale",
            "LC_ALL": "C",
            "LANG": "C",
            "SOURCE_DATE_EPOCH": "0",
        }
    )
    return env


def run_logged(command: list[str], env: dict[str, str], log_path: Path) -> int:
    print("kbuild:", " ".join(command), flush=True)
    log_path.parent.mkdir(parents=True, exist_ok=True)
    with log_path.open("a", encoding="utf-8") as log:
        log.write("$ " + " ".join(command) + "\n")
        process = subprocess.Popen(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        assert process.stdout is not None
        for line in process.stdout:
            sys.stdout.write(line)
            log.write(line)
        return process.wait()


def collect_objects(build: Path) -> dict[str, dict[str, int | str]]:
    objects: dict[str, dict[str, int | str]] = {}
    for path in sorted((build / "kernel" / "locking").glob("*.o")):
        if not path.is_file():
            continue
        relative = path.relative_to(build).as_posix()
        objects[relative] = {"sha256": sha256_file(path), "bytes": path.stat().st_size}
    return objects


def compiler_executions(strace_text: str) -> dict[str, int]:
    counts: dict[str, int] = {}
    for line in strace_text.splitlines():
        if "execve(" not in line:
            continue
        binary_match = re.search(r'execve\("([^"]+)"', line)
        if binary_match is None:
            continue
        binary = binary_match.group(1).rsplit("/", 1)[-1]
        if binary not in _COMPILER:
            continue
        if line.endswith(" <unfinished ...>"):
            line = line[: -len(" <unfinished ...>")]
        if "..." in line:
            raise SystemExit("strace truncated a compiler command; refusing to claim it did not run")
        for source in _LOCKING_SOURCE.findall(line):
            counts[source] = counts.get(source, 0) + 1
    return counts


def ccache_source_results(log_text: str) -> dict[str, dict[str, object]]:
    results: dict[str, dict[str, object]] = {}
    current: str | None = None
    for line in log_text.splitlines():
        if "Command line:" in line:
            match = _LOCKING_SOURCE.search(line)
            current = match.group(0) if match else None
            if current is not None and current not in results:
                results[current] = {"result": "", "executed": False}
            continue
        if current is None:
            continue
        if "Executing " in line and "compiler check" not in line:
            results[current]["executed"] = True
        elif "Result:" in line:
            results[current]["result"] = line.strip()
    return results


def parse_print_stats(text: str) -> dict[str, int]:
    """Parse `ccache --print-stats`. ccache 4.9 prints remote hits only here."""
    values: dict[str, int] = {}
    for line in text.splitlines():
        if "\t" not in line:
            continue
        key, raw = line.split("\t", 1)
        values[key.strip()] = int(raw.strip())
    if "remote_storage_read_hit" not in values:
        raise SystemExit(f"ccache --print-stats has no remote counters:\n{text}")
    return values


def counter(values: dict[str, int], name: str) -> int:
    if name not in values:
        raise SystemExit(f"ccache --print-stats missing {name}")
    return values[name]


def remote_counters(values: dict[str, int]) -> dict[str, int]:
    return {
        "hits": counter(values, "remote_storage_read_hit"),
        "misses": counter(values, "remote_storage_miss"),
        "errors": counter(values, "remote_storage_error"),
        "timeouts": counter(values, "remote_storage_timeout"),
        "writes": counter(values, "remote_storage_write"),
        "storage_hits": counter(values, "remote_storage_hit"),
    }


def defined_symbols(path: Path) -> list[str]:
    text = subprocess.check_output(["nm", path], text=True, errors="replace")
    names: list[str] = []
    for line in text.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[-2] in {"T", "t", "D", "d", "B", "b", "R", "r", "W", "w"}:
            names.append(parts[-1])
    return names


def sentinel_identity(objects: dict) -> bytes:
    payload = {name: meta["sha256"] for name, meta in objects.items()}
    return json.dumps(payload, sort_keys=True, separators=(",", ":")).encode()


def compiler_identity(gcc: str) -> dict[str, str]:
    cc1 = subprocess.check_output([gcc, "-print-file-name=cc1"], text=True).strip()
    cc1_path = Path(cc1)
    return {
        "path": gcc,
        "version": subprocess.check_output([gcc, "-dumpversion"], text=True).strip(),
        "machine": subprocess.check_output([gcc, "-dumpmachine"], text=True).strip(),
        "sha256": sha256_file(gcc),
        "cc1": cc1,
        "cc1_sha256": sha256_file(cc1_path) if cc1_path.is_file() else "",
    }


def sha256_file(path: Path | str) -> str:
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def require_tool(name: str) -> str:
    path = shutil.which(name)
    if path is None:
        raise SystemExit(f"required tool is not installed: {name}")
    return path


def log_tail(path: Path) -> str:
    if not path.is_file():
        return ""
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    return "\n".join(lines[-80:])


if __name__ == "__main__":
    main(sys.argv)
