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
_CONFIGURATION_NAMES = {
    "configure", "configure.ac", "Makefile", "Make.Rules", "Makefile.am", "Makefile.in", "config.hin",
    "ltmain.sh", "config.guess", "config.sub", "install-sh", "missing",
    "compile", "depcomp", "ar-lib", "mkinstalldirs", "ylwrap", "config.rpath", "VERSION",
}
_COMPILED_SUFFIXES = {".c", ".h", ".cc", ".cpp", ".cxx", ".m", ".mm", ".s", ".S", ".y", ".l"}


def _source_identity_file(path: Path) -> bool:
    return (
        path.name in _CONFIGURATION_NAMES
        or path.name.startswith("Makefile.")
        or path.suffix == ".mk"
        or path.suffix == ".m4"
        or (path.name.startswith("config") and path.suffix == ".in")
        or path.suffix in _COMPILED_SUFFIXES
    )


def _source_identity(source: Path) -> bytes:
    entries = {}
    for name, path in state._files(source).items():
        if name == "BUILD.bazel" or not _source_identity_file(path):
            continue
        mode, data = state._entry(path)
        digest = ""
        if (
            path.is_symlink()
            or path.name in _CONFIGURATION_NAMES
            or path.name.startswith("Makefile.")
            or path.suffix == ".mk"
            or path.suffix in {".m4", ".y", ".l"}
            or (path.name.startswith("config") and path.suffix == ".in")
        ):
            digest = hashlib.sha256(data).hexdigest()
        entries[name] = [mode, digest]
    return (json.dumps(entries, sort_keys=True) + "\n").encode()


def prepare(root: Path, arguments: list[str], *, in_tree: bool = False, extra_tree: str | None = None) -> None:
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
    source_root = root / ("build" if in_tree else "src")
    source_result = state.sync(changed, source_root, root, remove_stale=False)
    for name, path in state._files(source_root).items():
        if name in previous and name not in current:
            state._remove(path)
            source_result["changed"] += 1
    source_result["files"] = len(current)
    source_result["sha256"] = hashlib.sha256(json.dumps(current, sort_keys=True).encode()).hexdigest()
    state._remove(source_index)
    source_index.write_text(json.dumps(current, sort_keys=True) + "\n")
    configuration = hashlib.sha256()
    for name, path in sorted(files.items()):
        if path.name in _CONFIGURATION_NAMES or path.suffix == ".m4" or (path.name.startswith("config") and path.suffix == ".in"):
            configuration.update(name.encode() + hashlib.sha256(path.read_bytes()).digest())
    for name, argument in zip(("headers", "uapi", "libraries"), arguments[1:4]):
        result = state.sync(state._files(Path(argument)), root / "inputs" / name, root)
        configuration.update(name.encode() + result["sha256"].encode())
    result = state.sync({"libcompiler_rt.a": Path(arguments[4])}, root / "inputs/runtime", root)
    configuration.update(result["sha256"].encode())
    if extra_tree:
        result = state.sync(state._files(Path(extra_tree)), root / "inputs/extra", root)
        configuration.update(result["sha256"].encode())
    signature = root / "inputs/configured.sha256"
    current = configuration.hexdigest()
    configure = not signature.is_file() or signature.read_text() != current or not (root / "build/Makefile").is_file()
    state._remove(root / "inputs/configure-required")
    (root / "inputs/configure-required").write_text("1" if configure else "0")
    state._remove(signature)
    signature.write_text(current)
    print("Orlix package source: " + json.dumps(source_result, sort_keys=True), flush=True)


def run(package: str, script: str, toolchain: str, compiler: str, arguments: list[str], *, configuration: str | None = None, in_tree: bool = False, make_only: bool = False, extra_tree: str | None = None) -> int:
    if not re.fullmatch(r"[a-z0-9][a-z0-9+.-]*", package):
        raise ValueError(f"invalid package state name: {package}")
    environment = dict(os.environ)
    identity = [Path(script), Path(toolchain), Path(compiler), Path(__file__), Path(state.__file__)]
    if configuration:
        identity.append(Path(configuration))
    compatibility = {"package": package, "target": "aarch64-linux-gnu"}

    def build(root: Path) -> int:
        launcher = root / "compiler-launcher"
        state._remove(launcher)
        launcher.write_text(environment.get("ORLIX_COMPILER_LAUNCHER", "/opt/homebrew/bin/ccache"))
        identity_inputs = [*identity, launcher]
        if make_only and arguments:
            source_identity = root / "source-identity.json"
            state.sync(
                {source_identity.name: _source_identity(Path(arguments[0]).absolute().parent)},
                root,
                root,
                remove_stale=False,
            )
            identity_inputs.append(source_identity)
        valid = state.resume(root, identity_inputs, _TREES, compatibility=compatibility)
        print("Orlix package Make state: " + ("verified" if valid else "clean"), flush=True)
        prepare(root, arguments, in_tree=in_tree, extra_tree=extra_tree)
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
