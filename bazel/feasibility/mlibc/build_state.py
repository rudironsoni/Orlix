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
    runtime_digest = hashlib.sha256(runtime.read_bytes()).hexdigest()
    state.sync({"libcompiler_rt-" + runtime_digest + ".a": runtime}, inputs / "runtime", root)
    print("Orlix mlibc source: " + json.dumps(result, sort_keys=True), flush=True)
    print("Orlix mlibc UAPI: " + json.dumps(uapi_result, sort_keys=True), flush=True)


def run(script: str, toolchain: str, compiler: str, arguments: list[str]) -> int:
    environment = dict(os.environ)
    identity = [Path(toolchain), Path(compiler), Path(script), Path(__file__), Path(state.__file__)]

    def build(root: Path) -> int:
        launcher = root / "compiler-launcher"
        state._remove(launcher)
        launcher.write_text(environment.get("ORLIX_COMPILER_LAUNCHER", "/opt/homebrew/bin/ccache"))
        valid = state.resume(root, [*identity, launcher], _TREES)
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
