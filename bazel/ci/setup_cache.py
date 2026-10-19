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
import os
import plistlib
import shutil
import stat
import subprocess
import sys
import zipfile
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


def bundle_name(version: str) -> str:
    return f"iOS {version}.simruntime"


def marker_path(runtime_dir: Path, version: str) -> Path:
    return runtime_dir / f"{bundle_name(version)}.orlix-complete"


def archive_candidates(runtime_dir: Path, version: str) -> tuple[Path, Path]:
    name = bundle_name(version)
    return (
        runtime_dir / f"{name}.orlix.zip",
        runtime_dir / f"{name}.orlix.tar",
    )


def find_archive(runtime_dir: Path, version: str) -> Path | None:
    for path in archive_candidates(runtime_dir, version):
        try:
            if path.is_file() and path.stat().st_size > 0:
                return path
        except OSError:
            continue
    return None


def marker_is_complete(runtime_dir: Path, version: str) -> bool:
    path = marker_path(runtime_dir, version)
    try:
        text = path.read_text(encoding="utf-8")
    except OSError:
        return False
    return text.strip() == "complete"


def simruntime_present(runtime_dir: Path) -> bool:
    """True only for a booted bundle whose stored copy skipped no entries.

    iOS 26.5 is the runner image runtime and does not count. A symlink does
    not count. A tree or archive that dropped an unreadable entry does not
    count, because that is not the bundle simctl booted.
    """
    for version in ("15.5", "26.5"):
        if version in IMAGE_SIMRUNTIME_VERSIONS:
            continue
        if not marker_is_complete(runtime_dir, version):
            continue
        bundle = runtime_dir / bundle_name(version)
        if simruntime_bootable(bundle):
            return True
        archive = find_archive(runtime_dir, version)
        if archive is not None:
            return True
    return False


def sudo_noninteractive() -> bool:
    if shutil.which("sudo") is None:
        return False
    completed = subprocess.run(
        ["sudo", "-n", "true"],
        check=False, capture_output=True, text=True,
    )
    return completed.returncode == 0


def remove_path(path: Path) -> None:
    if path.is_symlink() or path.is_file():
        try:
            path.unlink()
        except OSError:
            if sudo_noninteractive():
                subprocess.run(["sudo", "rm", "-f", str(path)], check=False)
        return
    if not path.exists():
        return
    try:
        shutil.rmtree(path)
    except OSError:
        if sudo_noninteractive():
            subprocess.run(["sudo", "rm", "-rf", str(path)], check=False)


def clear_stored_archive(runtime_dir: Path, version: str) -> None:
    marker = marker_path(runtime_dir, version)
    if marker.exists() or marker.is_symlink():
        remove_path(marker)
    for archive in archive_candidates(runtime_dir, version):
        if archive.exists() or archive.is_symlink():
            remove_path(archive)


def write_marker(runtime_dir: Path, version: str) -> None:
    marker_path(runtime_dir, version).write_text("complete\n", encoding="utf-8")


def slash_entry(value: str) -> str:
    text = value.replace("\\", "/").strip()
    if text.startswith("./"):
        text = text[2:]
    return text.rstrip("/")


def walk_entries(bundle: Path) -> set[str]:
    """Relative entries, including the bundle directory. Raise if any entry cannot be listed."""
    found = {bundle.name}

    def raise_error(error: OSError) -> None:
        raise error

    for dirpath, dirnames, filenames in os.walk(bundle, followlinks=False, onerror=raise_error):
        for name in dirnames + filenames:
            full = Path(dirpath) / name
            found.add(slash_entry(str(Path(bundle.name) / full.relative_to(bundle))))
    return found


def entries_from_find(bundle: Path) -> set[str] | None:
    completed = subprocess.run(
        ["sudo", "find", "-P", str(bundle)],
        check=False, capture_output=True, text=True,
    )
    if completed.returncode != 0:
        return None
    prefix = str(bundle.parent) + os.sep
    found = set()
    for line in completed.stdout.splitlines():
        if not line.startswith(prefix):
            continue
        found.add(slash_entry(line[len(prefix):]))
    if bundle.name not in found:
        return None
    return found


def entries_from_archive(archive: Path) -> set[str] | None:
    if archive.suffix == ".zip":
        try:
            with zipfile.ZipFile(archive) as zipped:
                names = zipped.namelist()
        except (OSError, zipfile.BadZipFile):
            return None
        return {slash_entry(name) for name in names if slash_entry(name)}
    completed = subprocess.run(
        ["tar", "-tf", str(archive)],
        check=False, capture_output=True, text=True,
    )
    if completed.returncode != 0 and sudo_noninteractive():
        completed = subprocess.run(
            ["sudo", "tar", "-tf", str(archive)],
            check=False, capture_output=True, text=True,
        )
    if completed.returncode != 0:
        return None
    return {slash_entry(line) for line in completed.stdout.splitlines() if slash_entry(line)}


def cache_readable_file(path: Path) -> bool:
    try:
        if path.stat().st_size <= 0:
            return False
        with path.open("rb") as stream:
            stream.read(1)
    except OSError:
        return False
    return True


def cache_readable_tree(bundle: Path) -> bool:
    try:
        for dirpath, _dirnames, filenames in os.walk(bundle, followlinks=False, onerror=_raise_walk):
            for name in filenames:
                file_path = Path(dirpath) / name
                if file_path.is_symlink():
                    continue
                with file_path.open("rb") as stream:
                    stream.read(1)
    except OSError:
        return False
    return True


def _raise_walk(error: OSError) -> None:
    raise error


def same_readable_entries(source: Path, destination: Path) -> bool:
    try:
        return walk_entries(source) == walk_entries(destination)
    except OSError:
        return False


def copy_without_skipping(source: Path, destination: Path, seen: set[Path] | None = None) -> None:
    """Copy every entry. Raise when an entry cannot be read or stored.

    This does not drop unreadable files. The caller removes ``destination``
    when this raises, so a partial tree is not left for simctl or actions/cache.
    """
    if seen is None:
        seen = set()
    info = source.lstat()
    if stat.S_ISLNK(info.st_mode):
        destination.parent.mkdir(parents=True, exist_ok=True)
        if destination.exists() or destination.is_symlink():
            destination.unlink()
        destination.symlink_to(os.readlink(source))
        return
    try:
        resolved = source.resolve()
    except OSError:
        resolved = source
    if resolved in seen:
        return
    seen.add(resolved)
    if stat.S_ISDIR(info.st_mode):
        destination.mkdir(parents=True, exist_ok=True)
        for child in source.iterdir():
            copy_without_skipping(child, destination / child.name, seen)
        return
    if not stat.S_ISREG(info.st_mode):
        raise OSError(f"unsupported simulator bundle entry: {source}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination, follow_symlinks=False)


def store_readable_directory(source: Path, destination: Path) -> bool:
    try:
        copy_without_skipping(source, destination)
    except OSError:
        remove_path(destination)
        return False
    if not same_readable_entries(source, destination) or not cache_readable_tree(destination):
        remove_path(destination)
        return False
    if not simruntime_bootable(destination):
        remove_path(destination)
        return False
    return True


def store_privileged_archive(source: Path, runtime_dir: Path, version: str) -> bool:
    """Archive the booted bundle without dropping unreadable entries.

    ``sudo`` reads the files actions/cache cannot. The archive itself is then
    owned by the current user, so the cache save can store those bytes. The
    archive is deleted when its listing is not the full bundle.
    """
    if not sudo_noninteractive():
        return False
    if shutil.which("ditto"):
        archive = runtime_dir / f"{bundle_name(version)}.orlix.zip"
        create = [
            "sudo", "ditto", "-c", "-k", "--keepParent", "--rsrc",
            str(source), str(archive),
        ]
    else:
        archive = runtime_dir / f"{bundle_name(version)}.orlix.tar"
        create = ["sudo", "tar", "-C", str(source.parent), "-cf", str(archive), source.name]
    remove_path(archive)
    created = subprocess.run(create, check=False, capture_output=True, text=True)
    if created.returncode != 0:
        remove_path(archive)
        return False
    owned = subprocess.run(
        ["sudo", "chown", f"{os.getuid()}:{os.getgid()}", str(archive)],
        check=False, capture_output=True, text=True,
    )
    if owned.returncode != 0 or not cache_readable_file(archive):
        remove_path(archive)
        return False
    archived = entries_from_archive(archive)
    found = entries_from_find(source)
    if archived is None or found is None or not archive_covers(found, archived):
        remove_path(archive)
        return False
    return True


def archive_covers(found: set[str], archived: set[str]) -> bool:
    """True when every booted-bundle entry was stored.

    A directory counts as stored when the archive lists it or lists a path
    inside it. A file that was skipped is not covered by a parent directory.
    """
    for entry in found:
        if entry in archived:
            continue
        prefix = entry + "/"
        if any(item.startswith(prefix) for item in archived):
            continue
        return False
    return True


def extract_archive(archive: Path, runtime_dir: Path) -> bool:
    if archive.suffix == ".zip":
        command = ["sudo", "ditto", "-x", "-k", str(archive), str(runtime_dir)]
    else:
        command = ["sudo", "tar", "-C", str(runtime_dir), "-xf", str(archive)]
    completed = subprocess.run(command, check=False, capture_output=True, text=True)
    return completed.returncode == 0


def restore_booted_bundle(version: str, runtime_dir: Path) -> None:
    """Put the archived booted bundle back where simctl scans.

    A partial directory is removed first. It must not be the runtime that
    boots. Extraction uses the archive that kept every entry.
    """
    if version in IMAGE_SIMRUNTIME_VERSIONS:
        return
    bundle = runtime_dir / bundle_name(version)
    if (bundle.exists() or bundle.is_symlink()) and not (
        marker_is_complete(runtime_dir, version) and simruntime_bootable(bundle)
    ):
        print(
            f"removing partial iOS {version} runtime from {runtime_dir}; "
            "a copy that skipped unreadable entries is not booted by simctl",
            flush=True,
        )
        remove_path(bundle)
    if marker_is_complete(runtime_dir, version) and simruntime_bootable(bundle):
        return
    archive = find_archive(runtime_dir, version)
    if archive is None or not marker_is_complete(runtime_dir, version):
        return
    print(f"restoring the booted iOS {version} simulator bundle from {archive}", flush=True)
    if not extract_archive(archive, runtime_dir) or not simruntime_bootable(bundle):
        remove_path(bundle)
        print(
            f"not using iOS {version} from {archive}; "
            "the restored bundle is not complete, so xcodes runtimes install stays",
            flush=True,
        )


def full_copy_failed(version: str, directory: Path) -> None:
    print(
        f"not keeping iOS {version} in {directory}; "
        "a full copy of the booted bundle was not possible, so xcodes runtimes install stays",
        flush=True,
    )


def stage_listed_simruntime(version: str, runtime_dir: Path | None) -> None:
    """Store the bundle simctl booted, without dropping unreadable entries.

    iOS 26.5 is already on the runner image and is not stored. The iOS 15.5
    result is a complete archive of the installed bundle. A directory copy is
    kept only when every entry is readable by actions/cache. A partial tree
    is removed.
    """
    if version in IMAGE_SIMRUNTIME_VERSIONS:
        return
    directory = user_runtime_profiles() if runtime_dir is None else runtime_dir
    destination = directory / bundle_name(version)
    source = listed_bundle_path(version)
    directory.mkdir(parents=True, exist_ok=True)
    clear_stored_archive(directory, version)
    if destination.is_symlink():
        try:
            destination.unlink()
        except OSError:
            remove_path(destination)
    same = False
    if source is not None and destination.exists():
        try:
            same = source.resolve() == destination.resolve()
        except OSError:
            same = False
    if source is None:
        if destination.exists() and not simruntime_bootable(destination):
            remove_path(destination)
        return
    print(f"storing the booted iOS {version} simulator bundle into {directory}", flush=True)
    if store_privileged_archive(source, directory, version):
        if destination.exists() and not same:
            remove_path(destination)
        write_marker(directory, version)
        print(f"stored the booted iOS {version} simulator bundle in {directory}", flush=True)
        return
    if same:
        full_copy_failed(version, directory)
        return
    if destination.exists():
        remove_path(destination)
    if store_readable_directory(source, destination):
        write_marker(directory, version)
        print(f"stored the booted iOS {version} simulator bundle in {directory}", flush=True)
        return
    remove_path(destination)
    full_copy_failed(version, directory)


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

    iOS 26.5 skips ``xcodes`` when simctl already lists it. iOS 15.5 is not
    installed when simctl does not list it. A restored download archive is
    not an installed runtime and is not copied into a protected directory.
    A complete archive of the bundle simctl booted is extracted into the
    directory simctl scans before this check. A partial copy is removed and
    does not skip the install. ``simruntime-present`` is true only for a
    bundle that did not skip entries.
    """
    try:
        if runtime_dir is not None:
            restore_booted_bundle(version, runtime_dir)
            expose_restored_runtimes(runtime_dir)
        if runtime_already_installed(version, runtime_dir):
            print(runtime_message("skip", version, archive_dir), flush=True)
            return "skip"
    except (RuntimeProbeDenied, PermissionError):
        print(runtime_message("skip", version, archive_dir), flush=True)
        return "skip"
    # Ask simctl again immediately before any installer. A runtime that is
    # listed now must not fall through to xcodes. The download archive is not
    # an installed runtime.
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
        return 0 if simruntime_present(args.runtime_dir) else 1
    if args.command == "boot-simulator":
        boot_simulator(args.udid)
        return 0
    if args.command == "stage-runtime":
        stage_listed_simruntime(args.version, args.runtime_dir)
        return 0
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
