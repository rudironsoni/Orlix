"""Prepare declared mlibc inputs while Ninja retains its internal build state."""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from bazel import build_state as state
from bazel.feasibility.mlibc.header_digest import followed_tree_digest

_TREES = ("mlibc", "build", "inputs", "arch")


def prepare(root: Path, source: Path, patches: list[Path], subprojects: dict[str, Path],
            uapi: Path, runtime: Path) -> None:
    inputs = root / "inputs"
    state.directory(inputs, root)
    with tempfile.TemporaryDirectory(prefix="orlix-mlibc-source.") as temporary:
        staging = Path(temporary) / "mlibc"
        shutil.copytree(source, staging, symlinks=True)
        for path in (staging, *staging.rglob("*")):
            if path.is_dir() and not path.is_symlink():
                path.chmod(0o755)
        for patch in patches:
            subprocess.run(["/usr/bin/patch", "--batch", "--forward", "-p1", "-d", str(staging), "-i", str(patch.resolve())], check=True)
        files = state._files(staging)
        files.pop("BUILD.bazel", None)
        configuration = hashlib.sha256()
        for name, path in sorted(files.items()):
            if name.startswith("subprojects/"):
                mode, data = state._entry(path)
                configuration.update(json.dumps([name, mode, hashlib.sha256(data).hexdigest()]).encode())
        for name, source in sorted(subprojects.items()):
            prepared = state.sync(state._files(source), inputs / "subprojects" / name, root)
            configuration.update(name.encode() + prepared["sha256"].encode())
        previous = inputs / "subprojects.sha256"
        compatible = previous.is_file() and previous.read_text() == configuration.hexdigest()
        for name in subprojects:
            managed = root / "mlibc/subprojects" / name
            if not compatible:
                state._remove(managed)
            else:
                files.update({"subprojects/" + name + "/" + relative: path for relative, path in state._files(managed).items()})
        result = state.sync(files, root / "mlibc", root)
        state._remove(previous)
        previous.write_text(configuration.hexdigest())
    uapi_result = state.sync(state._files(uapi), inputs / "uapi", root)
    state._remove(inputs / "configure-cache")
    (inputs / "configure-cache").write_text("clear" if not compatible or uapi_result["changed"] else "keep")
    header_root = inputs / "uapi" / "include"
    if not header_root.is_dir():
        header_root = inputs / "uapi"
    followed = inputs / "followed-header.sha256"
    state._remove(followed)
    followed.write_text(followed_tree_digest(header_root) + "\n")
    runtime_digest = hashlib.sha256(runtime.read_bytes()).hexdigest()
    state.sync({"libcompiler_rt-" + runtime_digest + ".a": runtime}, inputs / "runtime", root)
    print("Orlix mlibc source: " + json.dumps(result, sort_keys=True), flush=True)
    print("Orlix mlibc UAPI: " + json.dumps(uapi_result, sort_keys=True), flush=True)


def meson_setup_plan(configure_cache: str, ninja_exists: bool) -> str:
    """Return Meson setup flags, or 'skip' when the Ninja graph is already valid."""
    cache = configure_cache.strip()
    if cache == "keep" and ninja_exists:
        return "skip"
    flags: list[str] = []
    if ninja_exists:
        flags.append("--reconfigure")
        if cache == "clear":
            flags.append("--clearcache")
    return " ".join(flags)


def compiler_outputs(build_ninja: str) -> set[str]:
    """Primary outputs of Ninja edges whose rule name is a compiler rule."""
    rules = set()
    for line in build_ninja.splitlines():
        if line.startswith("rule ") and "COMPILER" in line:
            rules.add(line.split(None, 1)[1].strip())
    outputs = set()
    for line in build_ninja.splitlines():
        if not line.startswith("build "):
            continue
        left, separator, right = line.partition(":")
        if not separator or not right.strip():
            continue
        rule = right.split()[0]
        if rule not in rules:
            continue
        for output in left[len("build "):].split("|", 1)[0].split():
            outputs.add(output)
    return outputs


def ninja_log_entries(path: Path) -> list[tuple[int, int, str]]:
    if not path.is_file():
        return []
    entries = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if not line or line.startswith("#"):
            continue
        parts = line.split("\t")
        if len(parts) < 4:
            continue
        try:
            end = int(parts[1])
            start = int(parts[0])
        except ValueError:
            continue
        entries.append((start, end, parts[3]))
    return entries


def ninja_log_end(path: Path) -> int:
    entries = ninja_log_entries(path)
    if not entries:
        return -1
    return max(entry[1] for entry in entries)


def compiler_edges_since(log: Path, build_ninja: Path, previous_end: int) -> list[str]:
    """Compiler outputs recorded in .ninja_log after previous_end.

    Stamp files and other non-compiler edges are ignored even when the log shows
    them as newer than the previous run.
    """
    outputs = compiler_outputs(build_ninja.read_text(encoding="utf-8"))
    return [
        output
        for _start, end, output in ninja_log_entries(log)
        if end > previous_end and output in outputs
    ]


def report_compiler_edges(log: Path, build_ninja: Path, previous_end: int) -> None:
    edges = compiler_edges_since(log, build_ninja, previous_end)
    print("Orlix Ninja compiler edges: " + str(len(edges)), flush=True)


def resume_identity(toolchain: Path, compiler: Path, script: Path, launcher: Path) -> list[Path]:
    return [
        toolchain,
        compiler,
        script,
        Path(__file__).resolve(),
        Path(state.__file__).resolve(),
        launcher,
    ]


def run(script: str, toolchain: str, compiler: str, arguments: list[str]) -> int:
    environment = dict(os.environ)

    def build(root: Path) -> int:
        launcher = root / "compiler-launcher"
        state._remove(launcher)
        launcher.write_text(environment.get("ORLIX_COMPILER_LAUNCHER", "/opt/homebrew/bin/ccache"))
        identity = resume_identity(Path(toolchain), Path(compiler), Path(script), launcher)
        valid = state.resume(root, identity, _TREES)
        print("Orlix Ninja state: " + ("verified" if valid else "clean"), flush=True)
        for name in ("cross.ini", "native.ini", "compiler-cache.log"):
            state._remove(root / name)
        environment["ORLIX_MLIBC_WORK_ROOT"] = str(root)
        result = subprocess.call(["/bin/bash", script, *arguments], env=environment)
        if result == 0:
            state.record(root, _TREES)
        return result

    if environment.get("ORLIX_MLIBC_INCREMENTAL", "1") == "0":
        with tempfile.TemporaryDirectory(prefix="orlix-mlibc.") as temporary:
            return build(Path(temporary).resolve())
    base = Path.cwd().parent.parent
    root = base / "orlix-mlibc-state/aarch64-linux-gnu"
    with state.locked(root, base):
        return build(root)
