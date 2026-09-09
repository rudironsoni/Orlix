#!/usr/bin/env python3
"""Relink selected Coreutils programs against mlibc shared objects for LD_PRELOAD tests."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path


def _ccld_inputs(build_dir: Path, program: str) -> list[str]:
    completed = subprocess.run(
        ["gmake", "-C", str(build_dir), "-n", f"src/{program}"],
        check=False,
        capture_output=True,
        text=True,
    )
    output = completed.stdout + completed.stderr
    line = ""
    for candidate in output.splitlines():
        if "CCLD" in candidate and f"src/{program}" in candidate:
            line = candidate
    if not line:
        raise SystemExit(f"missing CCLD line for src/{program}\n{output[-2000:]}")
    marker = f"-o src/{program}"
    idx = line.find(marker)
    if idx < 0:
        raise SystemExit(f"CCLD line for src/{program} has no -o output: {line}")
    rest = line[idx + len(marker) :].split()
    inputs: list[str] = []
    seen: set[str] = set()
    for token in rest:
        if token.startswith("-") or token in {")", "("}:
            continue
        path = Path(token)
        if not path.is_absolute():
            path = build_dir / token
        resolved = str(path)
        if resolved in seen:
            continue
        if not path.is_file():
            raise SystemExit(f"missing link input {path} for src/{program}")
        seen.add(resolved)
        inputs.append(resolved)
    if not inputs:
        raise SystemExit(f"no link inputs for src/{program}")
    return inputs


def relink(program: str, *, build_dir: Path, sysroot: Path, package_lib: Path, rtlib: Path, output: Path) -> None:
    clang = os.environ.get("ORLIX_COREUTILS_CC", "clang")
    inputs = _ccld_inputs(build_dir, program)
    output.parent.mkdir(parents=True, exist_ok=True)
    cmd = [
        clang,
        "--target=aarch64-linux-gnu",
        f"--sysroot={sysroot}",
        "-fPIE",
        "-pie",
        "-fuse-ld=lld",
        "-nostdlib",
        "-Wl,-dynamic-linker,/usr/lib/ld.so",
        "-Wl,-z,max-page-size=0x4000",
        f"-L{sysroot / 'usr/lib'}",
        f"-L{package_lib}",
        str(sysroot / "usr/lib/crt1.o"),
        str(sysroot / "usr/lib/crti.o"),
        "-o",
        str(output),
        *inputs,
        str(sysroot / "usr/lib/libc.so"),
        str(sysroot / "usr/lib/libm.so"),
        str(sysroot / "usr/lib/libpthread.so"),
        str(sysroot / "usr/lib/libdl.so"),
        str(rtlib),
        str(sysroot / "usr/lib/crtn.o"),
    ]
    completed = subprocess.run(cmd, check=False, capture_output=True, text=True)
    if completed.returncode != 0:
        raise SystemExit(completed.stdout + completed.stderr)
    if not output.is_file() or output.stat().st_size == 0:
        raise SystemExit(f"dynamic {program} was not written to {output}")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("program")
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--sysroot", required=True, type=Path)
    parser.add_argument("--package-lib", required=True, type=Path)
    parser.add_argument("--rtlib", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args(argv)
    relink(
        args.program,
        build_dir=args.build_dir,
        sysroot=args.sysroot,
        package_lib=args.package_lib,
        rtlib=args.rtlib,
        output=args.output,
    )
    print(f"relinked dynamic {args.program}: {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
