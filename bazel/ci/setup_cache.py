#!/usr/bin/env python3
"""Decide which GitHub Actions setup steps can be reused.

The ccache directory this module identifies is a local compiler-object cache.
It is not a BuildBuddy remote action-cache hit, and it is not the Bazel output
base.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import plistlib
import subprocess
import sys
from pathlib import Path

ARCHIVE_SUFFIXES = {".dmg", ".pkg", ".ipsw", ".aea", ".zip"}
XCODE_APPLICATIONS = Path("/Applications")


def version_parts(value: str) -> tuple[int, int, int]:
    numbers = []
    for part in value.split("."):
        if not part.isdigit():
            raise ValueError(f"unparseable version: {value}")
        numbers.append(int(part))
    if not numbers or len(numbers) > 3:
        raise ValueError(f"unparseable version: {value}")
    while len(numbers) < 3:
        numbers.append(0)
    return numbers[0], numbers[1], numbers[2]


def version_satisfies(installed: str, spec: str) -> bool:
    """Match the version the way maxim-lobanov/setup-xcode does.

    A two-component spec such as ``26.6`` accepts any patch of that minor.
    A three-component spec must match exactly. setup-xcode uses this to decide
    that an installed Xcode can be selected, so the same match means no download.
    """
    observed = version_parts(installed)
    requested = version_parts(spec)
    if spec.count(".") >= 2:
        return observed == requested
    return observed[0] == requested[0] and observed[1] == requested[1]


def xcode_short_version(plist_path: Path) -> str | None:
    if not plist_path.is_file():
        return None
    try:
        with plist_path.open("rb") as stream:
            payload = plistlib.load(stream)
    except (OSError, plistlib.InvalidFileException):
        return None
    version = payload.get("CFBundleShortVersionString")
    if not isinstance(version, str) or not version.strip():
        return None
    return version.strip()


def xcode_build(plist_path: Path) -> str:
    with plist_path.open("rb") as stream:
        payload = plistlib.load(stream)
    build = payload.get("ProductBuildVersion")
    if not isinstance(build, str) or not build.strip():
        raise ValueError(f"missing ProductBuildVersion in {plist_path}")
    return build.strip()


def installed_xcode_versions(applications_dir: Path) -> list[str]:
    if not applications_dir.is_dir():
        return []
    found = []
    for app in sorted(applications_dir.iterdir()):
        if app.is_symlink() or not app.is_dir():
            continue
        if not (app.name.startswith("Xcode") and app.name.endswith(".app")):
            continue
        version = xcode_short_version(app / "Contents" / "version.plist")
        if version is not None:
            found.append(version)
    return found


def xcode_present(version: str, applications_dir: Path = XCODE_APPLICATIONS) -> bool:
    return any(version_satisfies(installed, version) for installed in installed_xcode_versions(applications_dir))


def archive_complete(directory: Path) -> bool:
    """A restored runtime archive is complete when a non-empty image has no aria2 sidecar."""
    if not directory.is_dir():
        return False
    for path in sorted(directory.rglob("*")):
        if not path.is_file() or path.name.endswith(".aria2"):
            continue
        if path.suffix.lower() not in ARCHIVE_SUFFIXES:
            continue
        if path.with_name(path.name + ".aria2").exists():
            continue
        if path.stat().st_size <= 0:
            continue
        return True
    return False


def simctl_runtime_available(version: str, payload: dict) -> bool:
    runtimes = payload.get("runtimes")
    if not isinstance(runtimes, list):
        return False
    for runtime in runtimes:
        if not isinstance(runtime, dict):
            continue
        if runtime.get("version") == version and runtime.get("isAvailable") is True:
            return True
    return False


def runtime_plan(*, available: bool, archive_ready: bool) -> str:
    """Return skip, install-local, or download.

    ``skip`` does not run ``xcodes runtimes install``: the runner image already
    registered the runtime. ``install-local`` runs the install against a restored
    archive, and xcodes reuses that file instead of downloading it. ``download``
    is the cold path.
    """
    if available:
        return "skip"
    if archive_ready:
        return "install-local"
    return "download"


def runtime_commands(plan: str, version: str, archive_dir: Path) -> list[list[str]]:
    if plan == "skip":
        return []
    if plan not in {"install-local", "download"}:
        raise ValueError(f"unknown runtime plan: {plan}")
    return [[
        "sudo", "xcodes", "runtimes", "install", f"iOS {version}",
        "--architecture", "arm64",
        "--directory", str(archive_dir),
        "--keep-archive",
        "--no-color",
    ]]


def runtime_message(plan: str, version: str, archive_dir: Path) -> str:
    if plan == "skip":
        return f"iOS {version} runtime is already available; skipping xcodes runtimes install"
    if plan == "install-local":
        return (
            f"iOS {version} runtime archive is restored at {archive_dir}; "
            "installing from that archive without downloading"
        )
    if plan == "download":
        return f"iOS {version} runtime is not on the runner; downloading it into {archive_dir}"
    raise ValueError(f"unknown runtime plan: {plan}")


def brewfile_formulae(text: str) -> list[str]:
    formulae = []
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if not stripped.startswith("brew "):
            continue
        quoted = stripped.split('"', 2)
        if len(quoted) < 3 or not quoted[1]:
            continue
        formulae.append(quoted[1])
    return formulae


def formula_was_source_built(payload: dict) -> bool:
    """True only when Homebrew reports an install that was not poured from a bottle."""
    formulae = payload.get("formulae")
    if not isinstance(formulae, list) or not formulae or not isinstance(formulae[0], dict):
        return False
    installed = formulae[0].get("installed")
    if not isinstance(installed, list) or not installed or not isinstance(installed[0], dict):
        return False
    return installed[0].get("poured_from_bottle") is False


def compiler_identity_digest(*, xcode_build: str, xcode_clang: bytes, llvm_clang: bytes) -> str:
    payload = {
        "schema": 1,
        "kind": "pinned-compiler-identity",
        "xcode_build": xcode_build,
        "xcode_clang_sha256": hashlib.sha256(xcode_clang).hexdigest(),
        "llvm_clang_sha256": hashlib.sha256(llvm_clang).hexdigest(),
    }
    encoded = json.dumps(payload, sort_keys=True, separators=(",", ":")).encode()
    return hashlib.sha256(encoded).hexdigest()


def observed_compiler_digest(developer_dir: Path) -> str:
    developer = developer_dir.resolve()
    plist = developer.parent / "version.plist"
    xcode_clang = developer / "Toolchains/XcodeDefault.xctoolchain/usr/bin/clang"
    llvm_clang = Path("/opt/homebrew/opt/llvm/bin/clang")
    missing = [str(path) for path in (plist, xcode_clang, llvm_clang) if not path.is_file()]
    if missing:
        raise SystemExit("pinned compiler identity is missing: " + ", ".join(missing))
    return compiler_identity_digest(
        xcode_build=xcode_build(plist),
        xcode_clang=xcode_clang.read_bytes(),
        llvm_clang=llvm_clang.read_bytes(),
    )


def local_bottles(directory: Path) -> list[Path]:
    if not directory.is_dir():
        return []
    return sorted(path for path in directory.glob("*.bottle.tar.gz") if path.is_file())


def run_checked(command: list[str], *, cwd: Path | None = None) -> None:
    completed = subprocess.run(command, cwd=cwd, check=False)
    if completed.returncode != 0:
        raise SystemExit(completed.returncode)


def install_bottles(directory: Path) -> None:
    bottles = local_bottles(directory)
    if not bottles:
        print(f"no restored Homebrew bottles in {directory}")
        return
    for bottle in bottles:
        print(f"installing restored Homebrew bottle {bottle.name}")
        completed = subprocess.run(["brew", "install", str(bottle)], check=False)
        if completed.returncode != 0:
            print(
                f"restored bottle {bottle.name} was not reusable; brew bundle will build it",
                file=sys.stderr,
            )


def capture_bottles(directory: Path, brewfile: Path) -> None:
    directory.mkdir(parents=True, exist_ok=True)
    for formula in brewfile_formulae(brewfile.read_text(encoding="utf-8")):
        info = subprocess.run(
            ["brew", "info", "--json=v2", formula],
            check=False, capture_output=True, text=True,
        )
        if info.returncode != 0 or not info.stdout.strip():
            print(f"skipping bottle capture for {formula}", file=sys.stderr)
            continue
        if not formula_was_source_built(json.loads(info.stdout)):
            continue
        print(f"bottling source-built {formula}")
        completed = subprocess.run(
            ["brew", "bottle", "--json", "--no-rebuild", formula],
            cwd=directory, check=False,
        )
        if completed.returncode != 0:
            print(f"could not bottle {formula}", file=sys.stderr)


def prepare_runtime(version: str, archive_dir: Path) -> str:
    completed = subprocess.run(
        ["xcrun", "simctl", "list", "runtimes", "-j"],
        check=False, capture_output=True, text=True,
    )
    if completed.returncode != 0:
        detail = (completed.stderr or completed.stdout or "").strip()
        raise SystemExit(f"simctl list runtimes failed: {detail}")
    payload = json.loads(completed.stdout)
    plan = runtime_plan(
        available=simctl_runtime_available(version, payload),
        archive_ready=archive_complete(archive_dir),
    )
    print(runtime_message(plan, version, archive_dir))
    if plan == "skip":
        return plan
    archive_dir.mkdir(parents=True, exist_ok=True)
    for command in runtime_commands(plan, version, archive_dir):
        run_checked(command)
    return plan


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    commands = parser.add_subparsers(dest="command", required=True)

    present = commands.add_parser("xcode-present")
    present.add_argument("--version", required=True)

    runtime = commands.add_parser("prepare-runtime")
    runtime.add_argument("--version", required=True)
    runtime.add_argument("--archive-dir", required=True, type=Path)

    archive = commands.add_parser("archive-present")
    archive.add_argument("--archive-dir", required=True, type=Path)

    install = commands.add_parser("install-bottles")
    install.add_argument("--bottle-dir", required=True, type=Path)

    capture = commands.add_parser("capture-bottles")
    capture.add_argument("--bottle-dir", required=True, type=Path)
    capture.add_argument("--brewfile", required=True, type=Path)

    identity = commands.add_parser("compiler-identity")
    identity.add_argument("--developer-dir", required=True, type=Path)

    args = parser.parse_args(argv)
    if args.command == "xcode-present":
        found = xcode_present(args.version)
        print("true" if found else "false")
        return 0 if found else 1
    if args.command == "prepare-runtime":
        prepare_runtime(args.version, args.archive_dir)
        return 0
    if args.command == "archive-present":
        return 0 if archive_complete(args.archive_dir) else 1
    if args.command == "install-bottles":
        install_bottles(args.bottle_dir)
        return 0
    if args.command == "capture-bottles":
        capture_bottles(args.bottle_dir, args.brewfile)
        return 0
    if args.command == "compiler-identity":
        print(observed_compiler_digest(args.developer_dir))
        return 0
    raise AssertionError(args.command)


if __name__ == "__main__":
    raise SystemExit(main())
