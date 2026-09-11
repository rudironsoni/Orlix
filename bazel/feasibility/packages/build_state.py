"""Reconcile package inputs before invoking the owning upstream build engine."""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

from bazel import build_state as state

_TREES = ("src", "build", "inputs")


def prepare(root: Path, arguments: list[str]) -> None:
    source = Path(arguments[0]).absolute().parent
    files = state._files(source)
    files.pop("BUILD.bazel", None)
    state.directory(root / "inputs", root)
    source_index = root / "inputs/source-files.json"
    previous = json.loads(source_index.read_text()) if source_index.is_file() else {}
    current = {}
    for name, path in files.items():
        mode, data = state._entry(path)
        current[name] = [mode, hashlib.sha256(data).hexdigest()]
    changed = {name: path for name, path in files.items() if current[name] != previous.get(name)}
    source_result = state.sync(changed, root / "src", root, remove_stale=False)
    for name, path in state._files(root / "src").items():
        if name in previous and name not in current:
            state._remove(path)
            source_result["changed"] += 1
    source_result["files"] = len(current)
    source_result["sha256"] = hashlib.sha256(json.dumps(current, sort_keys=True).encode()).hexdigest()
    state._remove(source_index)
    source_index.write_text(json.dumps(current, sort_keys=True) + "\n")
    configuration = hashlib.sha256()
    for name, path in sorted(files.items()):
        if path.name in ("configure", "configure.ac", "Makefile.am", "Makefile.in", "config.hin") or path.suffix == ".m4" or (path.name.startswith("config") and path.suffix == ".in"):
            configuration.update(name.encode() + hashlib.sha256(path.read_bytes()).digest())
    for name, argument in zip(("headers", "uapi", "libraries"), arguments[1:4]):
        result = state.sync(state._files(Path(argument)), root / "inputs" / name, root)
        configuration.update(name.encode() + result["sha256"].encode())
    result = state.sync({"libcompiler_rt.a": Path(arguments[4])}, root / "inputs/runtime", root)
    configuration.update(result["sha256"].encode())
    signature = root / "inputs/configured.sha256"
    current = configuration.hexdigest()
    configure = not signature.is_file() or signature.read_text() != current or not (root / "build/Makefile").is_file()
    state._remove(root / "inputs/configure-required")
    (root / "inputs/configure-required").write_text("1" if configure else "0")
    state._remove(signature)
    signature.write_text(current)
    print("Orlix package source: " + json.dumps(source_result, sort_keys=True), flush=True)


def run(package: str, script: str, toolchain: str, compiler: str, arguments: list[str]) -> int:
    if not re.fullmatch(r"[a-z0-9][a-z0-9+.-]*", package):
        raise ValueError(f"invalid package state name: {package}")
    environment = dict(os.environ)
    identity = [Path(script), Path(toolchain), Path(compiler), Path(__file__), Path(state.__file__)]
    compatibility = {"package": package, "target": "aarch64-linux-gnu"}

    def build(root: Path) -> int:
        launcher = root / "compiler-launcher"
        state._remove(launcher)
        launcher.write_text(environment.get("ORLIX_COMPILER_LAUNCHER", "/opt/homebrew/bin/ccache"))
        valid = state.resume(root, [*identity, launcher], _TREES, compatibility=compatibility)
        print("Orlix package Make state: " + ("verified" if valid else "clean"), flush=True)
        prepare(root, arguments)
        state._remove(root / "dest")
        state._remove(root / "compiler-cache.log")
        environment["ORLIX_PACKAGE_WORK_ROOT"] = str(root)
        result = subprocess.call(["/bin/bash", script, *arguments], env=environment)
        if result == 0:
            state.record(root, _TREES, compatibility=compatibility)
        return result

    if environment.get("ORLIX_PACKAGE_INCREMENTAL", "1") == "0":
        with tempfile.TemporaryDirectory(prefix="orlix-" + package + ".") as temporary:
            return build(Path(temporary).resolve())
    base = Path.cwd().parent.parent
    root = base / "orlix-package-state" / package / "aarch64-linux-gnu"
    with state.locked(root, base):
        return build(root)
