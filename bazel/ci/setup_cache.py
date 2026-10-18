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
import shutil
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


def selected_developer_dir() -> Path | None:
    completed = subprocess.run(
        ["xcode-select", "-p"],
        check=False, capture_output=True, text=True,
    )
    detail = f"{completed.stderr or ''}\n{completed.stdout or ''}"
    if completed.returncode != 0:
        if "permission denied" in detail.lower() or "operation not permitted" in detail.lower():
            raise PermissionError(detail.strip())
        return None
    text = (completed.stdout or "").strip()
    if not text:
        return None
    return Path(text)


def xcode_selected(version: str, developer_dir: Path | None = None) -> bool:
    """True when the selected Xcode already matches, or its plist cannot be read.

    A permission error while reading the selected Xcode is not a missing Xcode.
    """
    directory = selected_developer_dir() if developer_dir is None else developer_dir
    if directory is None:
        return False
    plist = directory.parent / "version.plist"
    probe = path_probe(plist)
    if probe == "permission":
        return True
    if probe == "missing":
        return False
    try:
        with plist.open("rb") as stream:
            payload = plistlib.load(stream)
    except PermissionError:
        return True
    except (OSError, plistlib.InvalidFileException):
        return False
    short = payload.get("CFBundleShortVersionString")
    if not isinstance(short, str) or not short.strip():
        return False
    try:
        return version_satisfies(short.strip(), version)
    except ValueError:
        return False


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


def runtime_matches(runtime: dict, version: str) -> bool:
    """True when simctl lists this runtime, including a patch of a two-part version."""
    observed = runtime.get("version")
    if isinstance(observed, str):
        if observed == version:
            return True
        try:
            if version_satisfies(observed, version):
                return True
        except ValueError:
            pass
    if runtime.get("name") == f"iOS {version}":
        return True
    identifier = runtime.get("identifier")
    suffix = "iOS-" + version.replace(".", "-")
    return isinstance(identifier, str) and identifier.endswith(suffix)


class RuntimeProbeDenied(Exception):
    """The runtime bundle could not be read. That is not a missing runtime."""


def path_probe(path: Path) -> str:
    """Return present, missing, or permission.

    ``Path.is_dir`` and ``Path.is_file`` swallow ``PermissionError`` and report
    false. A runtime that exists but cannot be read must not look missing.
    """
    try:
        path.stat()
    except PermissionError:
        return "permission"
    except OSError:
        return "missing"
    return "present"


def simctl_runtime_entries(version: str, payload: dict) -> list[dict]:
    runtimes = payload.get("runtimes")
    if not isinstance(runtimes, list):
        return []
    return [
        runtime for runtime in runtimes
        if isinstance(runtime, dict) and runtime_matches(runtime, version)
    ]


def simctl_runtime_listed(version: str, payload: dict) -> bool:
    """True when simctl lists this runtime, even if the bundle is not readable."""
    return bool(simctl_runtime_entries(version, payload))


def simctl_runtime_available(version: str, payload: dict) -> bool:
    for runtime in simctl_runtime_entries(version, payload):
        if runtime.get("isAvailable") is False:
            continue
        return True
    return False


def simctl_runtime_identifier(version: str, payload: dict) -> str | None:
    fallback = None
    for runtime in simctl_runtime_entries(version, payload):
        identifier = runtime.get("identifier")
        if not isinstance(identifier, str) or not identifier.strip():
            continue
        if runtime.get("isAvailable") is False:
            fallback = fallback or identifier.strip()
            continue
        return identifier.strip()
    return fallback


def simctl_bundle_path(version: str, payload: dict) -> Path | None:
    fallback = None
    for runtime in simctl_runtime_entries(version, payload):
        bundle = runtime.get("bundlePath")
        if not isinstance(bundle, str) or not bundle.strip():
            continue
        path = Path(bundle.strip())
        if runtime.get("isAvailable") is False:
            fallback = fallback or path
            continue
        return path
    return fallback


def simruntime_bundles(directory: Path) -> list[Path]:
    """Return installed ``.simruntime`` directories without walking their contents.

    Walking ``RuntimeRoot`` hits root-owned paths such as ``private/var/mobile``
    and raises ``PermissionError``. That error is an installed runtime, not a
    missing one.
    """
    probe = path_probe(directory)
    if probe == "permission":
        raise RuntimeProbeDenied(str(directory))
    if probe == "missing":
        return []
    try:
        entries = list(directory.iterdir())
    except PermissionError as error:
        raise RuntimeProbeDenied(str(directory)) from error
    found = []
    for path in sorted(entries):
        if not path.name.endswith(".simruntime"):
            continue
        state = path_probe(path)
        if state == "permission":
            raise RuntimeProbeDenied(str(path))
        if state == "present":
            found.append(path)
    return found


def user_runtime_profiles() -> Path:
    return Path.home() / "Library/Developer/CoreSimulator/Profiles/Runtimes"


def runtime_profile_directories(runtime_dir: Path | None = None) -> list[Path]:
    directories = []
    if runtime_dir is not None:
        directories.append(runtime_dir)
    directories.append(Path("/Library/Developer/CoreSimulator/Profiles/Runtimes"))
    user_profiles = user_runtime_profiles()
    if user_profiles not in directories:
        directories.append(user_profiles)
    return directories


def runtime_bundle_installed(version: str, runtime_dir: Path | None = None) -> bool:
    """True when the ``.simruntime`` bundle is on disk or cannot be read."""
    name = f"iOS {version}.simruntime"
    for directory in runtime_profile_directories(runtime_dir):
        if path_probe(directory / name) in {"present", "permission"}:
            return True
    return False


def expose_restored_runtimes(runtime_dir: Path, profiles_dir: Path | None = None) -> list[Path]:
    """Link a restored installed runtime into the directory simctl scans.

    Linking is not an install. ``xcodes runtimes install`` is not invoked.
    """
    profiles = user_runtime_profiles() if profiles_dir is None else profiles_dir
    linked = []
    for bundle in simruntime_bundles(runtime_dir):
        if profiles.resolve() == bundle.parent.resolve():
            linked.append(bundle)
            continue
        profiles.mkdir(parents=True, exist_ok=True)
        dest = profiles / bundle.name
        if not dest.exists() and not dest.is_symlink():
            dest.symlink_to(bundle.resolve(), target_is_directory=True)
        linked.append(dest)
    return linked


def read_simctl_runtimes() -> dict:
    completed = subprocess.run(
        ["xcrun", "simctl", "list", "runtimes", "-j"],
        check=False, capture_output=True, text=True,
    )
    if completed.returncode != 0:
        detail = (completed.stderr or completed.stdout or "").strip()
        lowered = detail.lower()
        if "permission denied" in lowered or "operation not permitted" in lowered:
            raise RuntimeProbeDenied(detail)
        raise SystemExit(f"simctl list runtimes failed: {detail}")
    payload = json.loads(completed.stdout or "{}")
    if not isinstance(payload, dict):
        raise SystemExit("simctl list runtimes did not return an object")
    return payload


def publish_installed_runtime(version: str, payload: dict, runtime_dir: Path) -> bool:
    """Copy an installed bundle into the cache directory after a cold install."""
    source = simctl_bundle_path(version, payload)
    if source is None or not source.is_dir():
        return False
    if source.parent.resolve() == runtime_dir.resolve():
        return True
    runtime_dir.mkdir(parents=True, exist_ok=True)
    target = runtime_dir / source.name
    if target.exists():
        return True
    try:
        shutil.copytree(source, target, symlinks=True)
    except PermissionError as error:
        print(f"could not stage installed runtime {source}: {error}", file=sys.stderr)
        if target.exists() or target.is_symlink():
            shutil.rmtree(target, ignore_errors=True)
        # The source runtime is installed. A permission error is not a miss.
        return True
    except OSError as error:
        print(f"could not stage installed runtime {source}: {error}", file=sys.stderr)
        if target.exists() or target.is_symlink():
            shutil.rmtree(target, ignore_errors=True)
        return False
    return path_probe(target) in {"present", "permission"}


def runtime_plan(*, available: bool, archive_ready: bool) -> str:
    """Return skip, install-local, or download.

    ``skip`` does not run ``xcodes runtimes install``. A runtime ``simctl list
    runtimes`` already lists is skipped even when a download archive was
    restored. Restoring that download is not an install. ``install-local`` is
    only the cold path for an archive simctl does not list yet. ``download``
    is the path with no archive.
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
        return (
            f"iOS {version} runtime is already listed by simctl; "
            "skipping xcodes runtimes install"
        )
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


def bundle_satisfied(brewfile: Path) -> bool:
    """True when the Brewfile is already satisfied.

    A permission error from ``brew bundle check`` is not an unsatisfied Brewfile.
    """
    completed = subprocess.run(
        ["brew", "bundle", "check", "--file", str(brewfile)],
        check=False, capture_output=True, text=True,
    )
    if completed.stdout:
        print(completed.stdout, end="" if completed.stdout.endswith("\n") else "\n")
    if completed.stderr:
        print(completed.stderr, end="" if completed.stderr.endswith("\n") else "\n", file=sys.stderr)
    if completed.returncode == 0:
        return True
    detail = f"{completed.stderr or ''}\n{completed.stdout or ''}".lower()
    return "permission denied" in detail or "operation not permitted" in detail


def runtime_already_installed(version: str, runtime_dir: Path | None = None) -> bool:
    """True when simctl lists the runtime or the bundle cannot be read.

    ``xcodes runtimes install`` must not run in either case. A restored download
    archive does not change this. A permission error is not a missing runtime.
    """
    try:
        if runtime_bundle_installed(version, runtime_dir):
            return True
        return simctl_runtime_listed(version, read_simctl_runtimes())
    except (RuntimeProbeDenied, PermissionError):
        return True


def prepare_runtime(version: str, archive_dir: Path, runtime_dir: Path | None = None) -> str:
    """Install a simulator runtime only when simctl does not already list it.

    A restored installed bundle is exposed before the listing. A restored
    download archive does not by itself run ``xcodes runtimes install``.
    """
    try:
        if runtime_dir is not None:
            expose_restored_runtimes(runtime_dir)
            if simruntime_bundles(runtime_dir):
                # Select Xcode may have started CoreSimulator before the cache
                # restore. Restart it so the restored bundle is listed.
                subprocess.run(
                    ["killall", "-9", "com.apple.CoreSimulator.CoreSimulatorService"],
                    check=False,
                )
        if runtime_already_installed(version, runtime_dir):
            print(runtime_message("skip", version, archive_dir))
            return "skip"
    except (RuntimeProbeDenied, PermissionError):
        print(runtime_message("skip", version, archive_dir))
        return "skip"
    plan = runtime_plan(available=False, archive_ready=archive_complete(archive_dir))
    print(runtime_message(plan, version, archive_dir))
    archive_dir.mkdir(parents=True, exist_ok=True)
    for command in runtime_commands(plan, version, archive_dir):
        run_checked(command)
    if runtime_dir is not None:
        # Staging failure must not be read as "the runtime was missing".
        publish_installed_runtime(version, read_simctl_runtimes(), runtime_dir)
    return plan


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    commands = parser.add_subparsers(dest="command", required=True)

    present = commands.add_parser("xcode-present")
    present.add_argument("--version", required=True)

    selected = commands.add_parser("xcode-selected")
    selected.add_argument("--version", required=True)
    selected.add_argument("--developer-dir", type=Path)

    runtime = commands.add_parser("prepare-runtime")
    runtime.add_argument("--version", required=True)
    runtime.add_argument("--archive-dir", required=True, type=Path)
    runtime.add_argument("--runtime-dir", type=Path)

    archive = commands.add_parser("archive-present")
    archive.add_argument("--archive-dir", required=True, type=Path)

    staged = commands.add_parser("simruntime-present")
    staged.add_argument("--runtime-dir", required=True, type=Path)

    identifier = commands.add_parser("runtime-identifier")
    identifier.add_argument("--version", required=True)

    satisfied = commands.add_parser("bundle-satisfied")
    satisfied.add_argument("--brewfile", required=True, type=Path)

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
    if args.command == "xcode-selected":
        try:
            found = xcode_selected(args.version, args.developer_dir)
        except PermissionError:
            print("true")
            return 0
        print("true" if found else "false")
        return 0 if found else 1
    if args.command == "prepare-runtime":
        prepare_runtime(args.version, args.archive_dir, args.runtime_dir)
        return 0
    if args.command == "archive-present":
        return 0 if archive_complete(args.archive_dir) else 1
    if args.command == "simruntime-present":
        return 0 if simruntime_bundles(args.runtime_dir) else 1
    if args.command == "runtime-identifier":
        found = simctl_runtime_identifier(args.version, read_simctl_runtimes())
        if found is None:
            raise SystemExit(f"iOS {args.version} runtime is not listed by simctl")
        print(found)
        return 0
    if args.command == "bundle-satisfied":
        ok = bundle_satisfied(args.brewfile)
        print("true" if ok else "false")
        return 0 if ok else 1
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
