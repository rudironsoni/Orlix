#!/usr/bin/env python3
"""Decide which GitHub Actions setup steps can be reused.

The ccache directory this module identifies is a local compiler-object cache.
It is not a BuildBuddy remote action-cache hit, and it is not the Bazel output
base.
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
import plistlib
import re
import shutil
import stat
import subprocess
import sys
import tarfile
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


def runtime_name_matches(name: str, version: str) -> bool:
    prefix = f"iOS {version}"
    if not name.startswith(prefix):
        return False
    return len(name) == len(prefix) or not name[len(prefix)].isdigit()


def runtime_identifier_matches(identifier: str, version: str) -> bool:
    token = "iOS-" + version.replace(".", "-")
    if identifier.endswith(token):
        return True
    return f"{token}-" in identifier


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
    name = runtime.get("name")
    if isinstance(name, str) and runtime_name_matches(name, version):
        return True
    for key in ("identifier", "runtimeIdentifier"):
        identifier = runtime.get(key)
        if isinstance(identifier, str) and runtime_identifier_matches(identifier, version):
            return True
    return False


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
    collected = []
    if isinstance(payload, list):
        collected.extend(payload)
    elif isinstance(payload, dict):
        runtimes = payload.get("runtimes")
        if isinstance(runtimes, list):
            collected.extend(runtimes)
        else:
            for value in payload.values():
                if isinstance(value, list):
                    collected.extend(value)
    return [
        runtime for runtime in collected
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
        if any(bundle.name == f"iOS {version}.simruntime" for version in IMAGE_SIMRUNTIME_VERSIONS):
            continue
        if not simruntime_bootable(bundle):
            continue
        if profiles.resolve() == bundle.parent.resolve():
            linked.append(bundle)
            continue
        profiles.mkdir(parents=True, exist_ok=True)
        dest = profiles / bundle.name
        if not dest.exists() and not dest.is_symlink():
            dest.symlink_to(bundle.resolve(), target_is_directory=True)
        linked.append(dest)
    return linked


def read_simctl_json(command: list[str]) -> dict:
    completed = subprocess.run(command, check=False, capture_output=True, text=True)
    if completed.returncode != 0:
        detail = (completed.stderr or completed.stdout or "").strip()
        lowered = detail.lower()
        if "permission denied" in lowered or "operation not permitted" in lowered:
            raise RuntimeProbeDenied(detail)
        if command[2:4] == ["runtime", "list"]:
            return {"runtimes": []}
        raise SystemExit(f"{' '.join(command)} failed: {detail}")
    payload = json.loads(completed.stdout or "{}")
    if isinstance(payload, list):
        return {"runtimes": payload}
    if not isinstance(payload, dict):
        raise SystemExit(f"{' '.join(command)} did not return an object")
    return payload


def read_simctl_runtimes() -> dict:
    return read_simctl_json(["xcrun", "simctl", "list", "runtimes", "-j"])


def read_simctl_runtime_images() -> dict:
    """Runtimes from ``simctl runtime list``, including ones ``list runtimes`` omits."""
    return read_simctl_json(["xcrun", "simctl", "runtime", "list", "-j"])


def publish_installed_runtime(version: str, payload: dict, runtime_dir: Path) -> bool:
    """Report an installed runtime without copying it.

    ``shutil.copytree`` walks ``RuntimeRoot/private/var/mobile`` and raises
    ``PermissionError``. A runtime simctl already lists is not copied and is
    not installed again.
    """
    del runtime_dir
    if simctl_runtime_listed(version, payload):
        return True
    source = simctl_bundle_path(version, payload)
    if source is None:
        return False
    return path_probe(source) in {"present", "permission"}


def listed_bundle_path(version: str) -> Path | None:
    """Return a readable bundle path from ``list runtimes`` or ``runtime list``."""
    try:
        for reader in (read_simctl_runtimes, read_simctl_runtime_images):
            payload = reader()
            if not simctl_runtime_listed(version, payload):
                continue
            source = simctl_bundle_path(version, payload)
            if source is not None and path_probe(source) == "present":
                return source
    except (RuntimeProbeDenied, PermissionError):
        return None
    return None


IMAGE_SIMRUNTIME_VERSIONS = frozenset({"26.5"})
BOOTABLE_SIMRUNTIME_VERSION = "15.5"
BOOTABLE_SIMRUNTIME_ARCHIVE_NAME = "iOS-15.5.simruntime.tar"
BOOTABLE_SIMRUNTIME_CACHE_KEY = "orlix-ios15-simruntime-bootable-v1"
IOS15_DMG_CACHE_KEY = (
    "orlix-ios-runtime-15.5-arm64-71fd7d0159a4439ebef1abb2b1e0b26204b74af903dcc579a7b6e131065a1150"
)
CI_TOOLCHAIN_ARCHIVE_NAME = "ci-toolchain.tar.gz"
CI_TOOLCHAIN_MANIFEST_NAME = "orlix-toolchain-manifest.json"
CI_TOOLCHAIN_CACHE_KEY_PREFIX = "orlix-ci-toolchain"
PINNED_CI_CLANG = "/opt/homebrew/opt/llvm/bin/clang"
EXCLUDED_TOOLCHAIN_FORMULAE = frozenset({"llvm@20"})
# Actions cache quota is about 10GB. Run 39305085161 restored the bootable
# iOS 15.5 runtime at BOOTABLE_SIMRUNTIME_CACHE_BYTES. The CI bottle HEAD
# total is CI_TOOLCHAIN_BOTTLE_BYTES and the installed members are
# CI_TOOLCHAIN_MEMBER_BYTES. Those members plus the runtime are about 7.5GB.
# ccache and the Bazel repository cache are the other live keys. They are
# dropped before the toolchain or the iOS runtime if the sum would pass 10GB.
ACTIONS_CACHE_QUOTA_BYTES = 10_000_000_000
BOOTABLE_SIMRUNTIME_CACHE_BYTES = 5_126_954_916
CCACHE_CACHE_BYTES = 97_654_617
BAZEL_REPOSITORY_CACHE_BYTES = 571_396_067
CI_TOOLCHAIN_BOTTLE_BYTES = 582_115_184
CI_TOOLCHAIN_MEMBER_BYTES = 2_349_225_560
TOOLCHAIN_LINK_ROOTS = ("bin", "sbin", "lib", "include", "share", "etc", "libexec", "opt")
_KEG_REL = re.compile(r"^Cellar/([^/]+)/([^/]+)$")
_CACHE_KEY_VERSION = re.compile(r"[0-9A-Za-z][0-9A-Za-z._+-]*$")


def simruntime_bootable(bundle: Path) -> bool:
    """True when a real bundle still has the entries a restore must boot.

    A symlink is not bootable. A copy that skipped unreadable entries omits
    ``RuntimeRoot/private/var/mobile`` and must not be treated as present.
    """
    if bundle.is_symlink():
        return False
    info = bundle / "Contents" / "Info.plist"
    root = bundle / "Contents" / "Resources" / "RuntimeRoot"
    mobile = root / "private" / "var" / "mobile"
    if path_probe(info) not in {"present", "permission"}:
        return False
    if path_probe(root) not in {"present", "permission"}:
        return False
    if path_probe(mobile) == "missing":
        return False
    if path_probe(root) == "permission":
        return True
    try:
        return any(True for _ in root.iterdir())
    except OSError:
        return False


def simruntime_present(runtime_dir: Path) -> bool:
    """True only when the cache directory contains a real bootable bundle.

    iOS 26.5 is the runner image runtime. A second copy in this directory is
    what simctl prefers, so that version does not count. ``simctl runtime list``
    and a symlink do not count either.
    """
    for version in ("15.5", "26.5"):
        if version in IMAGE_SIMRUNTIME_VERSIONS:
            continue
        if simruntime_bootable(runtime_dir / f"iOS {version}.simruntime"):
            return True
    return False


def bootable_archive_file(archive_dir: Path) -> Path:
    return archive_dir / BOOTABLE_SIMRUNTIME_ARCHIVE_NAME


def bootable_archive_ready(archive_dir: Path) -> bool:
    """True when the sudo tar of a bootable iOS 15.5 bundle is readable and non-empty."""
    archive = bootable_archive_file(archive_dir)
    try:
        return archive.is_file() and archive.stat().st_size > 0
    except OSError:
        return False


def ios15_cache_plan(*, bootable_archive: bool, dmg_archive: bool, bundle_bootable: bool) -> dict[str, str]:
    """Choose the iOS 15.5 installer and which cache object this job may touch.

    A restored bootable archive skips ``xcodes`` and leaves the DMG key alone.
    A missing archive with a restored DMG installs from that DMG. A partial
    tree is not ``bundle_bootable`` and is not saved.
    """
    if bootable_archive:
        return {"xcodes": "skip", "dmg": "leave", "save_bootable_archive": "no"}
    if bundle_bootable:
        return {"xcodes": "skip", "dmg": "leave", "save_bootable_archive": "yes"}
    if dmg_archive:
        return {"xcodes": "install-local", "dmg": "restore", "save_bootable_archive": "after-install"}
    return {"xcodes": "download", "dmg": "restore", "save_bootable_archive": "after-install"}


def bootable_archive_save_allowed(bundle: Path, *, already_cached: bool) -> bool:
    """A tree that failed the mobile-directory check is not uploaded."""
    if already_cached:
        return False
    expected = f"iOS {BOOTABLE_SIMRUNTIME_VERSION}.simruntime"
    return bundle.name == expected and simruntime_bootable(bundle)


def bootable_tar_create_command(bundle: Path, destination: Path) -> list[str]:
    """Archive one bundle with root privileges so ``private/var/mobile`` is included.

    Symlinks stay symlinks. ``-h`` would expand them into a second copy of the tree.
    """
    return ["sudo", "tar", "-C", str(bundle.parent), "-cf", str(destination), bundle.name]


def bootable_tar_extract_command(archive: Path, runtime_dir: Path) -> list[str]:
    """Extract only the iOS 15.5 member into the directory simctl scans."""
    return [
        "sudo", "tar", "-C", str(runtime_dir), "-xf", str(archive),
        f"iOS {BOOTABLE_SIMRUNTIME_VERSION}.simruntime",
    ]


def write_bootable_tar(bundle: Path, archive_dir: Path) -> bool:
    """Write one runner-readable tar. A partial bundle writes nothing."""
    if not bootable_archive_save_allowed(bundle, already_cached=False):
        return False
    archive_dir.mkdir(parents=True, exist_ok=True)
    destination = bootable_archive_file(archive_dir)
    partial = destination.with_suffix(destination.suffix + ".partial")
    partial.unlink(missing_ok=True)
    try:
        run_checked(bootable_tar_create_command(bundle, partial))
        run_checked(["sudo", "chown", f"{os.getuid()}:{os.getgid()}", str(partial)])
        run_checked(["sudo", "chmod", "644", str(partial)])
    except SystemExit:
        partial.unlink(missing_ok=True)
        raise
    try:
        if not partial.is_file() or partial.stat().st_size <= 0:
            partial.unlink(missing_ok=True)
            return False
    except OSError:
        partial.unlink(missing_ok=True)
        return False
    partial.replace(destination)
    return bootable_archive_ready(archive_dir)


def installed_bootable_bundle(version: str, runtime_dir: Path | None) -> Path | None:
    """Return the bootable iOS 15.5 bundle, preferring the directory simctl scans."""
    if version != BOOTABLE_SIMRUNTIME_VERSION:
        return None
    if runtime_dir is not None:
        candidate = runtime_dir / f"iOS {version}.simruntime"
        if bootable_archive_save_allowed(candidate, already_cached=False):
            return candidate
    listed = listed_bundle_path(version)
    if listed is not None and bootable_archive_save_allowed(listed, already_cached=False):
        return listed
    return None


def archive_installed_simruntime(version: str, archive_dir: Path, runtime_dir: Path | None = None) -> bool:
    """Tar a bootable iOS 15.5 bundle. An existing archive and a partial tree stay put."""
    if version in IMAGE_SIMRUNTIME_VERSIONS or version != BOOTABLE_SIMRUNTIME_VERSION:
        return False
    if bootable_archive_ready(archive_dir):
        return True
    bundle = installed_bootable_bundle(version, runtime_dir)
    if bundle is None:
        return False
    return write_bootable_tar(bundle, archive_dir)


def extract_bootable_simruntime(archive_dir: Path, runtime_dir: Path) -> bool:
    """Extract the restored tar into the bundle location simctl boots.

    Only the iOS 15.5 member is extracted. iOS 26.5 stays the runner image runtime.
    """
    if not bootable_archive_ready(archive_dir):
        return False
    runtime_dir.mkdir(parents=True, exist_ok=True)
    run_checked(bootable_tar_extract_command(bootable_archive_file(archive_dir), runtime_dir))
    bundle = runtime_dir / f"iOS {BOOTABLE_SIMRUNTIME_VERSION}.simruntime"
    return simruntime_bootable(bundle)


def copy_simruntime_bundle(source: Path, destination: Path, seen: set[Path] | None = None) -> bool:
    """Copy a bundle. Return True when an entry was skipped because it could not be read.

    ``shutil.copytree`` raises ``PermissionError`` on
    ``RuntimeRoot/private/var/mobile``. A skipped entry means this tree is not
    a runtime simctl can boot, so the caller must not leave it in the scan
    directory. The destination is a real directory, not a symlink to ``source``.
    """
    if seen is None:
        seen = set()
    try:
        info = source.lstat()
    except OSError:
        return True
    if stat.S_ISLNK(info.st_mode):
        try:
            return copy_simruntime_bundle(source.resolve(), destination, seen)
        except OSError:
            return True
    try:
        resolved = source.resolve()
    except OSError:
        resolved = source
    if resolved in seen:
        return False
    seen.add(resolved)
    if stat.S_ISDIR(info.st_mode):
        try:
            children = list(source.iterdir())
        except OSError:
            return True
        try:
            destination.mkdir(parents=True, exist_ok=True)
        except OSError:
            return True
        skipped = False
        for child in children:
            if copy_simruntime_bundle(child, destination / child.name, seen):
                skipped = True
        return skipped
    if not stat.S_ISREG(info.st_mode):
        return False
    try:
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination, follow_symlinks=False)
    except OSError:
        return True
    return False


def stage_listed_simruntime(version: str, runtime_dir: Path | None) -> None:
    """Copy a non-image runtime into the directory the cache save uploads.

    iOS 26.5 is already on the runner image. A second copy in the directory
    simctl scans is preferred over that image runtime on the next job, so it
    is not staged. The iOS 15.5 copy is the bundle simctl reports, and it is
    kept only when that tree is still bootable after unreadable entries are
    skipped. A symlink is never the staged result.
    """
    if version in IMAGE_SIMRUNTIME_VERSIONS:
        return
    directory = user_runtime_profiles() if runtime_dir is None else runtime_dir
    destination = directory / f"iOS {version}.simruntime"
    source = listed_bundle_path(version)
    if destination.is_symlink():
        try:
            destination.unlink()
        except OSError:
            return
    try:
        if source is not None and destination.exists() and source.resolve() == destination.resolve():
            return
    except OSError:
        pass
    if simruntime_bootable(destination):
        return
    if destination.exists():
        shutil.rmtree(destination, ignore_errors=True)
    if source is None:
        return
    directory.mkdir(parents=True, exist_ok=True)
    print(f"copying iOS {version} simulator bundle into {directory}", flush=True)
    skipped = copy_simruntime_bundle(source, destination)
    if skipped or not simruntime_bootable(destination):
        shutil.rmtree(destination, ignore_errors=True)
        print(
            f"not keeping iOS {version} in {directory}; "
            "the copy skipped unreadable entries and simctl must not boot that tree",
            flush=True,
        )


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
            f"iOS {version} is not installed. "
            f"{archive_dir} is a restored download archive, not an installed runtime; "
            "installing from that archive without downloading and without copying it"
        )
    if plan == "download":
        return f"iOS {version} runtime is not on the runner; downloading it into {archive_dir}"
    raise ValueError(f"unknown runtime plan: {plan}")


_BREWFILE_PIN = re.compile(r"#\s+(\S+)\s+(\d+\.\d+(?:\.\d+)*)\b")
_FORMULA_VERSION = re.compile(r'^\s*version\s+"([^"]+)"', re.M)
_FORMULA_URL = re.compile(r'^\s*url\s+"([^"]+)"', re.M)
_VERSION_TOKEN = re.compile(r"(\d+\.\d+(?:\.\d+)*)")


def brewfile_entries(text: str) -> list[tuple[str, str | None]]:
    """Formula names and a version pin carried by the comment above the entry.

    ``# xcodes 2.1.0 from tap ...`` pins the following ``brew`` line. Other
    entries keep a null pin and take the version from the formula file.
    """
    entries: list[tuple[str, str | None]] = []
    pending_name: str | None = None
    pending_version: str | None = None
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped:
            pending_name = None
            pending_version = None
            continue
        if stripped.startswith("#"):
            match = _BREWFILE_PIN.match(stripped)
            if match:
                pending_name, pending_version = match.group(1), match.group(2)
            continue
        if not stripped.startswith("brew "):
            pending_name = None
            pending_version = None
            continue
        quoted = stripped.split('"', 2)
        if len(quoted) < 3 or not quoted[1]:
            continue
        name = quoted[1]
        short = name.rsplit("/", 1)[-1]
        version = pending_version if pending_name in {short, name} else None
        entries.append((name, version))
        pending_name = None
        pending_version = None
    return entries


def brewfile_formulae(text: str) -> list[str]:
    return [name for name, _version in brewfile_entries(text)]


def homebrew_prefix() -> Path:
    for candidate in (Path("/opt/homebrew"), Path("/usr/local")):
        if (candidate / "bin" / "brew").is_file():
            return candidate
    return Path("/opt/homebrew")


def formula_short_name(name: str) -> str:
    return name.rsplit("/", 1)[-1]


def formula_source(prefix: Path, name: str) -> Path | None:
    """Locate a formula file without asking Homebrew to load it.

    Core formulae usually live at ``Formula/<letter>/<name>.rb``. ``libtool``
    lives at ``Formula/lib/libtool.rb``, so a letter-only lookup misses it and
    the toolchain pin cannot be recorded.
    """
    short = formula_short_name(name)
    if name.count("/") == 2:
        user, repo, _formula = name.split("/")
        roots = [prefix / "Library" / "Taps" / user / f"homebrew-{repo}"]
    else:
        roots = [prefix / "Library" / "Taps" / "homebrew" / "homebrew-core"]
    relative = [
        Path("Formula") / short[:1].lower() / f"{short}.rb",
        Path("Formula") / f"{short}.rb",
    ]
    if short.startswith("lib"):
        relative.append(Path("Formula") / "lib" / f"{short}.rb")
    for root in roots:
        for candidate in relative:
            path = root / candidate
            if path.is_file():
                return path
        formula_root = root / "Formula"
        if not formula_root.is_dir():
            continue
        try:
            children = list(formula_root.iterdir())
        except OSError:
            continue
        for child in children:
            if not child.is_dir():
                continue
            candidate = child / f"{short}.rb"
            if candidate.is_file():
                return candidate
    return None


def formula_declared_version(text: str) -> str | None:
    """Stable version from a formula file, without loading Homebrew.

    A ``head`` spec before ``stable do`` is not the pin. LLVM's formula is
    written that way.
    """
    stable_at = text.find("\n  stable do\n")
    if stable_at == -1 and text.startswith("  stable do\n"):
        stable_at = 0
    if stable_at != -1:
        rest = text[stable_at:]
        end = rest.find("\n  end\n")
        body = rest if end == -1 else rest[:end]
    else:
        bottle_at = text.find("\n  bottle do")
        body = text if bottle_at == -1 else text[:bottle_at]
        head_at = body.find("\n  head ")
        if head_at == -1:
            head_at = body.find("\n  head do")
        if head_at != -1:
            body = body[:head_at]
    explicit = _FORMULA_VERSION.search(body)
    if explicit:
        return explicit.group(1)
    url_match = _FORMULA_URL.search(body)
    if url_match is None:
        return None
    archive = url_match.group(1).rstrip("/").rsplit("/", 1)[-1]
    tokens = _VERSION_TOKEN.findall(archive) or _VERSION_TOKEN.findall(url_match.group(1))
    if not tokens:
        return None
    return tokens[-1]


def formula_version_matches(installed: str, declared: str) -> bool:
    """True when the active keg is the declared upstream version.

    Homebrew appends ``_1`` for a bottle revision. That revision is the same
    compiler. A different upstream version is not.
    """
    return installed.split("_", 1)[0] == declared.split("_", 1)[0]


def active_keg(prefix: Path, name: str) -> Path | None:
    """The Cellar directory ``opt/<formula>`` currently selects.

    ``opt/llvm@20`` does not satisfy ``llvm``. The build invokes
    ``opt/llvm/bin/clang``.
    """
    short = formula_short_name(name)
    opt = prefix / "opt" / short
    if not opt.is_symlink() and not opt.exists():
        return None
    resolved = opt.resolve()
    if resolved.parent.name != short or resolved.parent.parent.name != "Cellar":
        return None
    return resolved


def pinned_llvm_clang(prefix: Path, keg: Path) -> bool:
    """True when the clang the build runs is inside the pinned llvm keg."""
    clang = prefix / "opt" / "llvm" / "bin" / "clang"
    if not clang.is_symlink() and not clang.is_file():
        return False
    resolved = clang.resolve()
    if not resolved.is_file():
        return False
    try:
        resolved.relative_to(keg)
    except ValueError:
        return False
    return True


def declared_tool_status(brewfile: Path, prefix: Path) -> tuple[bool, str]:
    entries = brewfile_entries(brewfile.read_text(encoding="utf-8"))
    if not entries:
        return False, f"{brewfile} declares no Homebrew formulae"
    mismatches: list[str] = []
    for name, pinned in entries:
        version = pinned if pinned is not None else formula_version_from_prefix(prefix, name)
        short = formula_short_name(name)
        if version is None:
            mismatches.append(f"{name} has no readable pinned version")
            continue
        keg = active_keg(prefix, name)
        if keg is None:
            if short == "llvm" and _llvm_alias_present(prefix):
                mismatches.append(
                    "llvm is not installed at opt/llvm; "
                    "llvm@20 on the runner is not the pinned clang"
                )
            else:
                mismatches.append(f"{name} {version} is not the active keg")
            continue
        if not formula_version_matches(keg.name, version):
            mismatches.append(f"{name} active keg {keg.name} is not pinned {version}")
            continue
        if short == "llvm" and not pinned_llvm_clang(prefix, keg):
            mismatches.append(
                f"pinned llvm clang is not {keg}/bin/clang; "
                "the macOS runner llvm@20 keg is not this pin"
            )
    if mismatches:
        return False, "Homebrew pin is not installed: " + "; ".join(mismatches)
    return True, "pinned Homebrew formulae are already installed"


def formula_version_from_prefix(prefix: Path, name: str) -> str | None:
    path = formula_source(prefix, name)
    if path is None:
        return None
    return formula_declared_version(path.read_text(encoding="utf-8"))


def _llvm_alias_present(prefix: Path) -> bool:
    alias = prefix / "opt" / "llvm@20"
    return alias.is_symlink() or alias.exists()


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


def bundle_satisfied(brewfile: Path, prefix: Path | None = None) -> bool:
    """True when every declared formula is already installed at its pin.

    The check reads the Homebrew prefix. It does not run ``brew bundle check``
    and it does not spawn ``brew info`` per formula. A permission error is not
    an unsatisfied Brewfile. A missing keg, including the pinned ``llvm``
    clang at ``opt/llvm/bin/clang``, is unsatisfied so ``brew bundle`` still
    installs it. ``opt/llvm@20`` is not that pin. ``etc/mke2fs.conf`` is not a
    keg member, and a missing file is unsatisfied so the workflow does not
    skip ``brew bundle``.
    """
    root = prefix if prefix is not None else homebrew_prefix()
    try:
        ok, detail = declared_tool_status(brewfile, root)
    except PermissionError:
        print("Homebrew prefix is not readable; not treating the Brewfile as unsatisfied")
        return True
    print(detail)
    if not ok:
        return False
    conf = root / "etc" / "mke2fs.conf"
    if not conf.is_file():
        print(f"Homebrew pin is not installed: {conf} is not a file")
        return False
    return True


def brewfile_digest(brewfile: Path) -> str:
    """SHA-256 of the CI Brewfile bytes. This is the Brewfile half of the cache key."""
    return hashlib.sha256(brewfile.read_bytes()).hexdigest()


def pinned_tool_versions(brewfile: Path, prefix: Path) -> dict[str, str]:
    """Declared version of every CI Brewfile formula.

    A comment pin wins. Every other formula takes the stable version from the
    formula file already in the Homebrew tap. Nothing here shells out to
    ``brew``, and ``llvm@20`` is not a stand-in for ``llvm``.
    """
    entries = brewfile_entries(brewfile.read_text(encoding="utf-8"))
    if not entries:
        raise ValueError(f"{brewfile} declares no Homebrew formulae")
    record: dict[str, str] = {}
    for name, pinned in entries:
        version = pinned if pinned is not None else formula_version_from_prefix(prefix, name)
        if version is None or _CACHE_KEY_VERSION.fullmatch(version) is None:
            raise ValueError(f"{name} has no readable pinned version")
        record[name] = version
    if "llvm" not in record:
        raise ValueError("CI Brewfile does not pin llvm")
    return record


def toolchain_version_token(record: dict[str, str]) -> str:
    payload = json.dumps(record, sort_keys=True, separators=(",", ":")).encode()
    return hashlib.sha256(payload).hexdigest()


def toolchain_pin_suffix(record: dict[str, str]) -> str:
    """Cache-key suffix: the visible llvm version plus every recorded tool version."""
    llvm = record["llvm"]
    if _CACHE_KEY_VERSION.fullmatch(llvm) is None:
        raise ValueError(f"llvm version cannot be a cache key: {llvm}")
    return f"llvm-{llvm}-{toolchain_version_token(record)}"


def toolchain_cache_key(brewfile_hash: str, pin: str) -> str:
    return f"{CI_TOOLCHAIN_CACHE_KEY_PREFIX}-{brewfile_hash}-{pin}"


def installed_formula_dependencies(names: list[str]) -> list[str]:
    """Installed runtime dependencies of the CI formulae.

    ``llvm@20`` is dropped by the caller. A failure here does not invent a
    dependency list; the Brewfile kegs are still packed.
    """
    if not names:
        return []
    completed = subprocess.run(
        ["brew", "deps", "--installed", "--formula", "--union", *names],
        check=False, capture_output=True, text=True,
    )
    if completed.returncode != 0:
        detail = (completed.stderr or completed.stdout or "").strip()
        print(f"could not list Homebrew dependencies: {detail}", file=sys.stderr)
        return []
    return [line.strip() for line in completed.stdout.splitlines() if line.strip()]


def _points_into_kegs(path: Path, kegs: set[Path]) -> bool:
    if not path.is_symlink():
        return False
    try:
        resolved = path.resolve()
    except OSError:
        return False
    for keg in kegs:
        if resolved == keg or keg in resolved.parents:
            return True
    return False


def toolchain_links(prefix: Path, kegs: set[Path]) -> list[str]:
    """Symlinks outside the kegs that resolve into those kegs.

    ``opt/llvm`` is one of them. ``opt/llvm@20`` points at a different keg and
    is not collected. The walk does not follow symlinks into the kegs.
    """
    found: list[str] = []
    for folder in TOOLCHAIN_LINK_ROOTS:
        root = prefix / folder
        if not root.exists() or root.is_symlink():
            continue
        for dirpath, dirnames, filenames in os.walk(root, followlinks=False):
            current = Path(dirpath)
            kept: list[str] = []
            for name in dirnames:
                path = current / name
                if path.is_symlink():
                    if _points_into_kegs(path, kegs):
                        found.append(path.relative_to(prefix).as_posix())
                else:
                    kept.append(name)
            dirnames[:] = kept
            for name in filenames:
                path = current / name
                if path.is_symlink() and _points_into_kegs(path, kegs):
                    found.append(path.relative_to(prefix).as_posix())
    return sorted(set(found))


def _selected_kegs(
    brewfile: Path, prefix: Path, dependencies: list[str] | None,
) -> list[tuple[str, Path]] | None:
    entries = brewfile_entries(brewfile.read_text(encoding="utf-8"))
    names = [name for name, _version in entries]
    required = set(names)
    required.update(formula_short_name(name) for name in names)
    if dependencies is None:
        dependencies = installed_formula_dependencies([name for name, _version in entries])
    selected: list[tuple[str, Path]] = []
    seen: set[str] = set()
    for name in [entry for entry, _version in entries] + list(dependencies):
        short = formula_short_name(name)
        if short in seen or short in EXCLUDED_TOOLCHAIN_FORMULAE:
            continue
        keg = active_keg(prefix, name)
        if keg is None:
            if name in required or short in required:
                print(f"not packing the CI toolchain; {name} has no active keg", file=sys.stderr)
                return None
            continue
        if keg.parent.name in EXCLUDED_TOOLCHAIN_FORMULAE:
            continue
        relative = keg.relative_to(prefix).as_posix()
        if _KEG_REL.fullmatch(relative) is None:
            print(f"not packing the CI toolchain; {relative} is not one keg", file=sys.stderr)
            return None
        selected.append((short, keg))
        seen.add(short)
    if "llvm" not in seen:
        print("not packing the CI toolchain; the pinned llvm keg is missing", file=sys.stderr)
        return None
    return selected


def toolchain_paths_valid(manifest: dict) -> bool:
    kegs = manifest.get("kegs")
    links = manifest.get("links")
    if not isinstance(kegs, list) or not kegs or not isinstance(links, list):
        return False
    for keg in kegs:
        if not isinstance(keg, str):
            return False
        match = _KEG_REL.fullmatch(keg)
        if match is None or match.group(1) in EXCLUDED_TOOLCHAIN_FORMULAE:
            return False
    for link in links:
        if not isinstance(link, str) or not link or link.startswith("/"):
            return False
        parts = Path(link).parts
        if ".." in parts or "llvm@20" in parts:
            return False
        if not parts or parts[0] not in TOOLCHAIN_LINK_ROOTS:
            return False
    return True


def normalize_tar_name(name: str) -> str:
    normalized = name[2:] if name.startswith("./") else name
    if normalized.endswith("/"):
        normalized = normalized[:-1]
    return normalized


def toolchain_members_allowed(members: list[tarfile.TarInfo], manifest: dict) -> bool:
    """Reject a whole-prefix archive and any member outside the recorded kegs."""
    if not toolchain_paths_valid(manifest):
        return False
    kegs = [keg for keg in manifest["kegs"] if isinstance(keg, str)]
    links = {link for link in manifest["links"] if isinstance(link, str)}
    saw_manifest = False
    for member in members:
        name = normalize_tar_name(member.name)
        if not name or name.startswith("/") or ".." in Path(name).parts or "llvm@20" in name:
            return False
        if name in {"", ".", "Cellar", "opt", "bin", "sbin", "lib", "include", "share", "etc", "libexec"}:
            return False
        if name == CI_TOOLCHAIN_MANIFEST_NAME:
            saw_manifest = member.isfile()
            continue
        if any(name == keg or name.startswith(keg + "/") for keg in kegs):
            continue
        if name in links:
            continue
        return False
    return saw_manifest


def opt_llvm_member_is_pinned(members: list[tarfile.TarInfo], keg: str) -> bool:
    found = False
    for member in members:
        if normalize_tar_name(member.name) != "opt/llvm":
            continue
        if not member.issym():
            return False
        link = member.linkname.replace("\\", "/")
        if "llvm@20" in link or keg not in link:
            return False
        found = True
    return found


def _manifest_matches(manifest: dict, brewfile: Path, prefix: Path, pin: str) -> bool:
    try:
        record = pinned_tool_versions(brewfile, prefix)
        expected = toolchain_pin_suffix(record)
    except (OSError, ValueError) as error:
        print(str(error), file=sys.stderr)
        return False
    clang = manifest.get("clang")
    if manifest.get("schema") != 1 or manifest.get("kind") != "orlix-ci-toolchain":
        return False
    if manifest.get("brewfile_sha256") != brewfile_digest(brewfile):
        return False
    if manifest.get("versions") != record or manifest.get("pin") != pin or pin != expected:
        return False
    if not isinstance(clang, dict) or clang.get("path") != PINNED_CI_CLANG:
        return False
    keg = clang.get("keg")
    digest = clang.get("sha256")
    if not isinstance(keg, str) or not keg.startswith("Cellar/llvm/") or "llvm@20" in keg:
        return False
    if not formula_version_matches(keg.rsplit("/", 1)[-1], record["llvm"]):
        return False
    if not isinstance(digest, str) or len(digest) != 64:
        return False
    if keg not in manifest.get("kegs", []):
        return False
    return toolchain_paths_valid(manifest)


def read_toolchain_manifest(archive: Path) -> tuple[dict, list[tarfile.TarInfo]]:
    with tarfile.open(archive, "r") as bundle:
        members = bundle.getmembers()
        manifest_member = next(
            (item for item in members if normalize_tar_name(item.name) == CI_TOOLCHAIN_MANIFEST_NAME),
            None,
        )
        if manifest_member is None or not manifest_member.isfile():
            raise ValueError("toolchain archive has no manifest")
        extracted = bundle.extractfile(manifest_member)
        if extracted is None:
            raise ValueError("toolchain manifest is not a file")
        payload = json.loads(extracted.read().decode())
    if not isinstance(payload, dict):
        raise ValueError("toolchain manifest is not an object")
    return payload, members


def _reject_tar_name(info: tarfile.TarInfo) -> tarfile.TarInfo:
    name = normalize_tar_name(info.name)
    if not name or name.startswith("/") or ".." in Path(name).parts or "llvm@20" in name:
        raise ValueError(f"refusing to archive {info.name}")
    return info


def write_toolchain_archive(
    destination: Path, prefix: Path, manifest: dict, kegs: list[str], links: list[str],
) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    partial = destination.with_name(destination.name + ".partial")
    partial.unlink(missing_ok=True)
    payload = json.dumps(manifest, sort_keys=True, separators=(",", ":")).encode()
    try:
        with tarfile.open(partial, "w:gz", format=tarfile.PAX_FORMAT, compresslevel=6) as bundle:
            info = tarfile.TarInfo(CI_TOOLCHAIN_MANIFEST_NAME)
            info.size = len(payload)
            info.mtime = 0
            info.mode = 0o644
            bundle.addfile(info, io.BytesIO(payload))
            for relative in kegs:
                bundle.add(prefix / relative, arcname=relative, recursive=True, filter=_reject_tar_name)
            for relative in links:
                bundle.add(prefix / relative, arcname=relative, recursive=False, filter=_reject_tar_name)
    except (OSError, tarfile.TarError, ValueError):
        partial.unlink(missing_ok=True)
        raise
    partial.replace(destination)


def extract_toolchain_archive(archive: Path, prefix: Path) -> None:
    """Extract pinned kegs into the prefix. This does not replace ``/opt/homebrew``."""
    prefix.mkdir(parents=True, exist_ok=True)
    flags = "-xzf" if archive.name.endswith(".gz") else "-xf"
    completed = subprocess.run(
        ["tar", "-C", str(prefix), flags, str(archive), f"--exclude={CI_TOOLCHAIN_MANIFEST_NAME}"],
        check=False,
    )
    stray = prefix / CI_TOOLCHAIN_MANIFEST_NAME
    if stray.is_symlink() or stray.is_file():
        stray.unlink()
    if completed.returncode != 0:
        raise OSError(f"tar extract failed with status {completed.returncode}")


def remove_restored_tools(prefix: Path, manifest: dict) -> None:
    """Remove only the kegs and symlinks this archive recorded.

    ``llvm@20`` is not in that list. A failed identity check must leave the
    runner's unlinked compiler in place and let ``brew bundle`` install ``llvm``.
    """
    if not toolchain_paths_valid(manifest):
        return
    for link in manifest["links"]:
        path = prefix / link
        if path.is_symlink():
            path.unlink()
    for keg in manifest["kegs"]:
        path = prefix / keg
        if path.is_symlink():
            path.unlink()
        elif path.is_dir():
            shutil.rmtree(path)


def pinned_clang_bytes_match(prefix: Path, manifest: dict) -> bool:
    """True when the clang the build runs is the archived file inside the llvm keg."""
    clang = prefix / "opt" / "llvm" / "bin" / "clang"
    keg = prefix / manifest["clang"]["keg"]
    if not pinned_llvm_clang(prefix, keg):
        return False
    try:
        resolved = clang.resolve()
    except OSError:
        return False
    if "llvm@20" in resolved.parts:
        return False
    try:
        digest = hashlib.sha256(resolved.read_bytes()).hexdigest()
    except OSError:
        return False
    if digest != manifest["clang"]["sha256"]:
        return False
    if prefix.resolve() == Path("/opt/homebrew").resolve() and Path(PINNED_CI_CLANG).resolve() != resolved:
        return False
    return True


def cache_plan(toolchain_bytes: int) -> dict[str, str]:
    """Decide which optional Actions caches to drop.

    The bootable iOS 15.5 runtime and this toolchain stay. ccache and the
    Bazel repository cache are dropped, smallest sufficient cut first, only
    when keeping them would pass the 10GB quota.
    """
    kept = BOOTABLE_SIMRUNTIME_CACHE_BYTES + max(toolchain_bytes, 0)
    ccache = CCACHE_CACHE_BYTES
    repository = BAZEL_REPOSITORY_CACHE_BYTES
    quota = ACTIONS_CACHE_QUOTA_BYTES
    drop_ccache = False
    drop_repository = False
    if kept > quota:
        drop_ccache = True
        drop_repository = True
    elif kept + ccache + repository > quota:
        if kept + repository <= quota:
            drop_ccache = True
        elif kept + ccache <= quota:
            drop_repository = True
        else:
            drop_ccache = True
            drop_repository = True
    return {
        "drop_ccache": "true" if drop_ccache else "false",
        "drop_repository": "true" if drop_repository else "false",
    }


def pack_toolchain(
    brewfile: Path,
    archive: Path,
    prefix: Path,
    *,
    pin: str,
    dependencies: list[str] | None = None,
) -> bool:
    """Pack the CI Brewfile kegs and the symlinks that point into them.

    The archive is one gzip file under the Orlix cache directory. It is not a
    tar of ``/opt/homebrew``. ``llvm@20`` is never a member. The bootable iOS
    15.5 runtime stays in the Actions cache. ccache or the Bazel repository
    cache is dropped first if the sum would pass 10GB.
    """
    try:
        record = pinned_tool_versions(brewfile, prefix)
        expected = toolchain_pin_suffix(record)
    except (OSError, ValueError) as error:
        print(str(error), file=sys.stderr)
        return False
    if pin != expected:
        print("not packing the CI toolchain; the recorded pin does not match this prefix", file=sys.stderr)
        return False
    if not _pinned_formulae_ready(brewfile, prefix):
        print("not packing the CI toolchain; brew bundle has not satisfied the pin", file=sys.stderr)
        return False
    selected = _selected_kegs(brewfile, prefix, dependencies)
    if selected is None:
        return False
    kegs = [keg for _short, keg in selected]
    links = toolchain_links(prefix, set(kegs))
    if "opt/llvm" not in links:
        print("not packing the CI toolchain; opt/llvm does not point at the pinned keg", file=sys.stderr)
        return False
    llvm_keg = next(keg for short, keg in selected if short == "llvm")
    clang = prefix / "opt" / "llvm" / "bin" / "clang"
    if not pinned_llvm_clang(prefix, llvm_keg):
        print(f"not packing the CI toolchain; {PINNED_CI_CLANG} is not inside {llvm_keg}", file=sys.stderr)
        return False
    try:
        clang_bytes = clang.resolve().read_bytes()
    except OSError as error:
        print(f"not packing the CI toolchain; {error}", file=sys.stderr)
        return False
    keg_names = [keg.relative_to(prefix).as_posix() for keg in kegs]
    manifest = {
        "schema": 1,
        "kind": "orlix-ci-toolchain",
        "brewfile_sha256": brewfile_digest(brewfile),
        "versions": record,
        "pin": expected,
        "clang": {
            "path": PINNED_CI_CLANG,
            "sha256": hashlib.sha256(clang_bytes).hexdigest(),
            "keg": llvm_keg.relative_to(prefix).as_posix(),
        },
        "kegs": keg_names,
        "links": links,
    }
    try:
        write_toolchain_archive(archive, prefix, manifest, keg_names, links)
        stored, members = read_toolchain_manifest(archive)
    except (OSError, ValueError, tarfile.TarError, json.JSONDecodeError) as error:
        archive.unlink(missing_ok=True)
        print(f"not packing the CI toolchain; {error}", file=sys.stderr)
        return False
    if stored != manifest or not toolchain_members_allowed(members, manifest):
        archive.unlink(missing_ok=True)
        print("not packing the CI toolchain; the archive is not limited to the pinned tools", file=sys.stderr)
        return False
    if not opt_llvm_member_is_pinned(members, manifest["clang"]["keg"]):
        archive.unlink(missing_ok=True)
        print("not packing the CI toolchain; opt/llvm does not point at the pinned keg", file=sys.stderr)
        return False
    size = archive.stat().st_size
    if BOOTABLE_SIMRUNTIME_CACHE_BYTES + size > ACTIONS_CACHE_QUOTA_BYTES:
        archive.unlink(missing_ok=True)
        print(
            "not saving the CI toolchain archive; "
            f"{size} bytes plus {BOOTABLE_SIMRUNTIME_CACHE_KEY} exceeds "
            f"{ACTIONS_CACHE_QUOTA_BYTES}"
        )
        print("toolchain archive bytes=0")
        print("toolchain archive omitted=toolchain")
        return True
    plan = cache_plan(size)
    print(f"toolchain archive bytes={size}")
    print("toolchain archive omitted=none")
    print(f"drop_ccache={plan['drop_ccache']}")
    print(f"drop_repository={plan['drop_repository']}")
    print(f"packed CI toolchain {archive}")
    return True


def _pinned_formulae_ready(brewfile: Path, prefix: Path) -> bool:
    """True when the Brewfile kegs match their pins.

    This does not require ``etc/mke2fs.conf``. ``restore_toolchain`` links that
    file after the keg check. ``bundle_satisfied`` still reports it missing.
    """
    try:
        ok, detail = declared_tool_status(brewfile, prefix)
    except PermissionError:
        print("Homebrew prefix is not readable; not treating the Brewfile as unsatisfied")
        return True
    print(detail)
    return ok


def ensure_mke2fs_conf(prefix: Path) -> bool:
    """Keep ``etc/mke2fs.conf`` or link it from the e2fsprogs keg."""
    conf = prefix / "etc" / "mke2fs.conf"
    if conf.is_file():
        return True
    sources = [prefix / "opt" / "e2fsprogs" / "etc" / "mke2fs.conf"]
    sources.extend(sorted(prefix.glob("Cellar/e2fsprogs/*/etc/mke2fs.conf")))
    source = next((path for path in sources if path.is_file()), None)
    if source is None:
        print("restored CI toolchain has no mke2fs.conf", file=sys.stderr)
        return False
    if conf.is_symlink():
        conf.unlink()
    conf.parent.mkdir(parents=True, exist_ok=True)
    conf.symlink_to(source)
    return True


def restore_toolchain(archive: Path, brewfile: Path, prefix: Path, *, pin: str) -> bool:
    """Require the pinned clang, and do not unpack an archive for ``mke2fs.conf``.

    A missing identity check returns false before ``brew bundle`` runs. Members
    outside the recorded kegs, including a tar of the Homebrew prefix or
    ``llvm@20``, are rejected without being extracted. When the prefix already
    has that clang, a missing ``etc/mke2fs.conf`` does not unpack the archive
    and does not delete it. ``ensure_mke2fs_conf`` links the file from the
    e2fsprogs keg when the keg has it. Otherwise the caller installs or
    reinstalls e2fsprogs only. An extract that is required for the clang keeps
    the archive and the restored kegs when the conf is still absent.
    """
    try:
        manifest, members = read_toolchain_manifest(archive)
    except (OSError, ValueError, tarfile.TarError, json.JSONDecodeError) as error:
        print(f"CI toolchain archive is not reusable: {error}", file=sys.stderr)
        return False
    if not _manifest_matches(manifest, brewfile, prefix, pin):
        print("CI toolchain archive does not match the recorded tool pin", file=sys.stderr)
        return False
    if not toolchain_members_allowed(members, manifest) or not opt_llvm_member_is_pinned(members, manifest["clang"]["keg"]):
        print("CI toolchain archive is not limited to the pinned clang", file=sys.stderr)
        return False
    if pinned_clang_bytes_match(prefix, manifest) and _pinned_formulae_ready(brewfile, prefix):
        if ensure_mke2fs_conf(prefix):
            print(f"restored pinned clang {PINNED_CI_CLANG}")
            return True
        print(
            "pinned clang is already installed; not unpacking the toolchain archive for a missing mke2fs.conf",
            file=sys.stderr,
        )
        return True
    # Drop recorded links first so an opt/llvm symlink aimed at llvm@20 cannot
    # stay in front of the extracted keg. The llvm@20 cellar is not recorded.
    remove_restored_tools(prefix, manifest)
    try:
        extract_toolchain_archive(archive, prefix)
    except OSError as error:
        print(str(error), file=sys.stderr)
        remove_restored_tools(prefix, manifest)
        return False
    if pinned_clang_bytes_match(prefix, manifest) and _pinned_formulae_ready(brewfile, prefix):
        if ensure_mke2fs_conf(prefix):
            print(f"restored pinned clang {PINNED_CI_CLANG}")
            return True
        print(
            "restored pinned clang; mke2fs.conf is absent, leaving the archive and kegs in place",
            file=sys.stderr,
        )
        return True
    print(f"restored clang is not {PINNED_CI_CLANG} at the recorded identity", file=sys.stderr)
    remove_restored_tools(prefix, manifest)
    return False


def emit_cache_plan(toolchain_bytes: int) -> int:
    plan = cache_plan(toolchain_bytes)
    print(f"drop_ccache={plan['drop_ccache']}")
    print(f"drop_repository={plan['drop_repository']}")
    print(f"ccache_bytes={CCACHE_CACHE_BYTES}")
    print(f"repository_bytes={BAZEL_REPOSITORY_CACHE_BYTES}")
    print(f"bootable_bytes={BOOTABLE_SIMRUNTIME_CACHE_BYTES}")
    print(f"toolchain_bytes={toolchain_bytes}")
    return 0


def emit_toolchain_cache_key(brewfile: Path, prefix: Path) -> int:
    try:
        record = pinned_tool_versions(brewfile, prefix)
        pin = toolchain_pin_suffix(record)
    except (OSError, ValueError) as error:
        print(str(error), file=sys.stderr)
        return 1
    print(f"pin={pin}")
    print(f"llvm={record['llvm']}")
    print(f"versions={toolchain_version_token(record)}")
    return 0


def runtime_already_installed(version: str, runtime_dir: Path | None = None) -> bool:
    """True when simctl lists the runtime or the bundle cannot be read.

    ``xcodes runtimes install`` must not run in either case. ``list runtimes``
    and ``runtime list`` both count. A restored download archive does not
    change this. A permission error is not a missing runtime.
    """
    try:
        if runtime_bundle_installed(version, runtime_dir):
            return True
        if simctl_runtime_listed(version, read_simctl_runtimes()):
            return True
        return simctl_runtime_listed(version, read_simctl_runtime_images())
    except (RuntimeProbeDenied, PermissionError):
        return True


def prepare_runtime(version: str, archive_dir: Path, runtime_dir: Path | None = None) -> str:
    """Install a simulator runtime only when simctl does not already list it.

    iOS 26.5 skips ``xcodes`` when simctl already lists it. iOS 15.5 skips
    ``xcodes`` when the restored bundle is ``simruntime_bootable``. That check
    does not read the download archive. A restored DMG is not an installed
    runtime and is not copied into a protected directory. iOS 26.5 is not
    copied into the directory simctl scans. A partial iOS 15.5 tree is removed
    before any installer runs.
    """
    try:
        if runtime_dir is not None and version not in IMAGE_SIMRUNTIME_VERSIONS:
            bundle = runtime_dir / f"iOS {version}.simruntime"
            if (bundle.is_symlink() or bundle.exists()) and not simruntime_bootable(bundle):
                print(
                    f"removing partial iOS {version} runtime from {runtime_dir}; "
                    "a copy that skipped unreadable entries is not booted by simctl",
                    flush=True,
                )
                if bundle.is_symlink() or bundle.is_file():
                    bundle.unlink()
                else:
                    shutil.rmtree(bundle, ignore_errors=True)
            if simruntime_bootable(bundle):
                decision = ios15_cache_plan(
                    bootable_archive=False, dmg_archive=False, bundle_bootable=True,
                )
                if decision["xcodes"] != "skip" or decision["dmg"] != "leave":
                    raise RuntimeError("a bootable iOS 15.5 bundle must skip xcodes and leave the DMG")
                expose_restored_runtimes(runtime_dir)
                print(runtime_message("skip", version, archive_dir), flush=True)
                return "skip"
        if runtime_dir is not None:
            expose_restored_runtimes(runtime_dir)
        if runtime_already_installed(version, runtime_dir):
            print(runtime_message("skip", version, archive_dir), flush=True)
            return "skip"
    except (RuntimeProbeDenied, PermissionError):
        print(runtime_message("skip", version, archive_dir), flush=True)
        return "skip"
    # Ask simctl again immediately before any installer. A runtime that is
    # listed now must not fall through to xcodes. The download archive is not
    # an installed runtime; the listed bundle is copied into the runtime directory.
    try:
        if simctl_runtime_listed(version, read_simctl_runtimes()) or simctl_runtime_listed(version, read_simctl_runtime_images()):
            print(runtime_message("skip", version, archive_dir), flush=True)
            return "skip"
    except (RuntimeProbeDenied, PermissionError):
        print(runtime_message("skip", version, archive_dir), flush=True)
        return "skip"
    # A restored download archive is not an installed runtime. Do not copy it.
    plan = runtime_plan(available=False, archive_ready=archive_complete(archive_dir))
    print(runtime_message(plan, version, archive_dir), flush=True)
    archive_dir.mkdir(parents=True, exist_ok=True)
    for command in runtime_commands(plan, version, archive_dir):
        run_checked(command)
    return plan


def simulator_already_booted(detail: str) -> bool:
    return "current state: booted" in detail.lower()


def command_detail(completed: subprocess.CompletedProcess[str]) -> str:
    if completed.stdout:
        print(completed.stdout, end="" if completed.stdout.endswith("\n") else "\n")
    if completed.stderr:
        print(completed.stderr, end="" if completed.stderr.endswith("\n") else "\n", file=sys.stderr)
    return f"{completed.stderr or ''}\n{completed.stdout or ''}"


def boot_simulator(identifier: str) -> None:
    """Boot one simulator once.

    A missing runtime fails on this attempt. Copying a runtime into the
    profiles directory before this call makes simctl boot that copy instead of
    the image runtime, so the copy happens only after boot returns.
    """
    boot = subprocess.run(
        ["xcrun", "simctl", "boot", identifier],
        check=False, capture_output=True, text=True,
    )
    boot_detail = command_detail(boot)
    if boot.returncode != 0 and not simulator_already_booted(boot_detail):
        raise SystemExit(boot.returncode or 1)
    status = subprocess.run(
        ["xcrun", "simctl", "bootstatus", identifier, "-b"],
        check=False, capture_output=True, text=True,
    )
    command_detail(status)
    if status.returncode != 0:
        raise SystemExit(status.returncode or 1)


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

    boot = commands.add_parser("boot-simulator")
    boot.add_argument("--udid", required=True)

    stage = commands.add_parser("stage-runtime")
    stage.add_argument("--version", required=True)
    stage.add_argument("--runtime-dir", required=True, type=Path)

    extract = commands.add_parser("extract-bootable-runtime")
    extract.add_argument("--archive-dir", required=True, type=Path)
    extract.add_argument("--runtime-dir", required=True, type=Path)

    archive_bootable = commands.add_parser("archive-bootable-runtime")
    archive_bootable.add_argument("--version", required=True)
    archive_bootable.add_argument("--archive-dir", required=True, type=Path)
    archive_bootable.add_argument("--runtime-dir", type=Path)

    bootable_present = commands.add_parser("bootable-archive-present")
    bootable_present.add_argument("--archive-dir", required=True, type=Path)

    satisfied = commands.add_parser("bundle-satisfied")
    satisfied.add_argument("--brewfile", required=True, type=Path)

    install = commands.add_parser("install-bottles")
    install.add_argument("--bottle-dir", required=True, type=Path)

    capture = commands.add_parser("capture-bottles")
    capture.add_argument("--bottle-dir", required=True, type=Path)
    capture.add_argument("--brewfile", required=True, type=Path)

    identity = commands.add_parser("compiler-identity")
    identity.add_argument("--developer-dir", required=True, type=Path)

    toolchain_key = commands.add_parser("toolchain-cache-key")
    toolchain_key.add_argument("--brewfile", required=True, type=Path)
    toolchain_key.add_argument("--prefix", type=Path)

    restore = commands.add_parser("restore-toolchain")
    restore.add_argument("--archive", required=True, type=Path)
    restore.add_argument("--brewfile", required=True, type=Path)
    restore.add_argument("--pin", required=True)
    restore.add_argument("--prefix", type=Path)

    pack = commands.add_parser("pack-toolchain")
    pack.add_argument("--archive", required=True, type=Path)
    pack.add_argument("--brewfile", required=True, type=Path)
    pack.add_argument("--pin", required=True)
    pack.add_argument("--prefix", type=Path)

    retention = commands.add_parser("cache-retention")
    retention.add_argument("--toolchain-bytes", required=True, type=int)

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
        return 0 if simruntime_present(args.runtime_dir) else 1
    if args.command == "boot-simulator":
        boot_simulator(args.udid)
        return 0
    if args.command == "stage-runtime":
        stage_listed_simruntime(args.version, args.runtime_dir)
        return 0
    if args.command == "extract-bootable-runtime":
        if extract_bootable_simruntime(args.archive_dir, args.runtime_dir):
            return 0
        print("restored iOS 15.5 archive is not a bootable simulator bundle", file=sys.stderr)
        return 1
    if args.command == "archive-bootable-runtime":
        if archive_installed_simruntime(args.version, args.archive_dir, args.runtime_dir):
            return 0
        print(
            f"not saving iOS {args.version}; the bundle failed the mobile-directory check or is not installed",
            file=sys.stderr,
        )
        return 1
    if args.command == "bootable-archive-present":
        return 0 if bootable_archive_ready(args.archive_dir) else 1
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
    if args.command == "toolchain-cache-key":
        root = args.prefix if args.prefix is not None else homebrew_prefix()
        return emit_toolchain_cache_key(args.brewfile, root)
    if args.command == "restore-toolchain":
        root = args.prefix if args.prefix is not None else homebrew_prefix()
        return 0 if restore_toolchain(args.archive, args.brewfile, root, pin=args.pin) else 1
    if args.command == "pack-toolchain":
        root = args.prefix if args.prefix is not None else homebrew_prefix()
        return 0 if pack_toolchain(args.brewfile, args.archive, root, pin=args.pin) else 1
    if args.command == "cache-retention":
        return emit_cache_plan(args.toolchain_bytes)
    raise AssertionError(args.command)


if __name__ == "__main__":
    raise SystemExit(main())
