"""Materialize declared Linux inputs without rewriting unchanged source files."""

from __future__ import annotations

import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import tempfile

from bazel import build_state
from bazel.build_state import _entry, _files, directory, sync


def prepare(linux: Path, destination: Path, overlays: list[Path], patches: list[Path],
            config: Path, isa: Path, profile: str, boundary: Path) -> dict:
    files: dict[str, Path | bytes] = {name: path for name, path in _files(linux).items() if name != "BUILD.bazel"}
    prefix = "OrlixKernel/Sources/ports/orlix/overlay/"
    for path in overlays:
        _, separator, name = path.as_posix().partition(prefix)
        if not separator:
            raise ValueError(f"overlay path missing overlay prefix: {path}")
        files[name] = path
    uapi = "arch/orlix/include/uapi/asm/"
    files = {name: source for name, source in files.items() if not name.startswith(uapi)}
    for name, path in _files(linux / "arch/arm64/include/uapi/asm").items():
        files[uapi + name] = path
    files.setdefault(uapi + "types.h", linux / "include/uapi/asm-generic/types.h")
    files["arch/orlix/configs/defconfig"] = (
        re.sub(rb"^CONFIG_PAGE_SIZE_[0-9]+KB=y\n", b"", config.read_bytes(), flags=re.M)
        + b"CONFIG_PAGE_SIZE_16KB=y\n"
    )
    with tempfile.TemporaryDirectory(prefix="orlix-kernel-patches.") as temporary:
        staging = Path(temporary)
        patched: set[str] = set()
        for patch in patches:
            if patch.suffix not in (".patch", ".diff"):
                continue
            names = set()
            for line in patch.read_text().splitlines():
                if line.startswith(("--- ", "+++ ")):
                    field = line.split()[1]
                    if field == "/dev/null":
                        continue
                    name = field.partition("/")[2]
                    if not name or ".." in PurePosixPath(name).parts or name.startswith("/"):
                        raise ValueError(f"invalid patch path: {field}")
                    names.add(name)
            for name in names - patched:
                path = staging / name
                path.parent.mkdir(parents=True, exist_ok=True)
                if name in files:
                    mode, data = _entry(files[name])
                    if not stat.S_ISREG(mode):
                        raise ValueError(f"patch input is not a regular file: {name}")
                    path.write_bytes(data)
                    path.chmod(stat.S_IMODE(mode))
            with patch.open("rb") as stream:
                subprocess.run(["/usr/bin/patch", "--batch", "-p1", "-d", str(staging)],
                               stdin=stream, check=True, stdout=subprocess.DEVNULL)
            patched.update(names)
        for name in patched:
            path = staging / name
            if path.is_file():
                files[name] = path
            else:
                files.pop(name, None)
        for name, path in _files(isa).items():
            files["arch/orlix/hosted_exec/orlix_tcti/isa/" + name] = path
        files[".orlix-port-profile"] = (
            f"linux_version=6.12.105\nprofile={profile}\nlinux_uapi_arch=arm64\nlinux_page_size=16384\n"
        ).encode()
        result = sync(files, destination, boundary)
    print("Orlix prepared source: " + json.dumps(result, sort_keys=True), flush=True)
    return result


_TREES = ("OrlixKernel/build", "OrlixKernel/release", "OrlixKernel/development")


def resume(root: Path, identity_inputs: list[Path]) -> None:
    directory(root / "OrlixKernel", root)
    valid = build_state.resume(root, identity_inputs, _TREES)
    print("Orlix Kbuild state: " + ("verified" if valid else "clean"), flush=True)


def record(root: Path) -> None:
    build_state.record(root, _TREES)


def run_locked(script: str, arguments: list[str]) -> int:
    environment = dict(os.environ)
    if environment.get("ORLIX_KERNEL_INCREMENTAL", "1") == "0":
        with tempfile.TemporaryDirectory(prefix="orlix-kernel-macho.") as temporary:
            environment["ORLIX_KERNEL_WORK_ROOT"] = str(Path(temporary).resolve())
            return subprocess.call(["/bin/bash", script, *arguments], env=environment)
    base = Path.cwd().parent.parent
    root = base / "orlix-kernel-state" / environment["PROFILE"] / environment["ORLIX_KERNEL_ARCHIVE_PLATFORMS"]
    directory(root, base)
    environment["ORLIX_KERNEL_WORK_ROOT"] = str(root)
    with build_state.locked(root, base):
        return subprocess.call(["/bin/bash", script, *arguments], env=environment)
