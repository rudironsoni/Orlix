#!/usr/bin/env python3
"""Select and verify the product Xcode through xcodes, never guessed paths.

The requested Xcode release comes from the repository-root .xcode-version
file. `xcodes` is the authority for which Xcodes are installed and which one
is selected. The selected developer directory is always observed from
`xcode-select -p`, and acceptance requires the exact product version and
build. Nothing here constructs an /Applications path.
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

VERSION_FILENAME = ".xcode-version"
VERSION_PATTERN = re.compile(r"^\d+\.\d+(\.\d+)?$")
XCODES_INSTALL_HINT = "brew install xcodesorg/made/xcodes"


class XcodeSelectError(RuntimeError):
    pass


def repo_version_path(repo_root: str | Path) -> Path:
    return Path(repo_root) / VERSION_FILENAME


def read_requested_version(repo_root: str | Path) -> str:
    try:
        text = repo_version_path(repo_root).read_text(encoding="utf-8")
    except OSError as error:
        raise XcodeSelectError(f"missing {VERSION_FILENAME} in {repo_root}") from error
    version = "".join(text.split())
    if not VERSION_PATTERN.match(version):
        raise XcodeSelectError(f"unparseable Xcode version in {VERSION_FILENAME}: {text!r}")
    return version


def require_xcodes() -> str:
    path = shutil.which("xcodes")
    if path is None:
        raise XcodeSelectError(f"xcodes is required for Xcode discovery; install with: {XCODES_INSTALL_HINT}")
    return path


def installed_versions() -> list[tuple[str, str]]:
    """Return (version, build) pairs reported by `xcodes installed`."""
    require_xcodes()
    try:
        observed = subprocess.run(
            ["xcodes", "installed"], check=True, capture_output=True, text=True, timeout=60
        ).stdout
    except (subprocess.CalledProcessError, OSError) as error:
        raise XcodeSelectError(f"xcodes installed failed: {error}") from error
    entries = []
    for line in observed.splitlines():
        stripped = line.strip()
        if not stripped:
            continue
        first = stripped.split()[0]
        build_match = re.search(r"\(([^)]*)\)", stripped)
        build = build_match.group(1).strip() if build_match else ""
        entries.append((first, build))
    return entries


def require_installed(version: str) -> None:
    versions = [entry_version for entry_version, _ in installed_versions()]
    if version not in versions:
        raise XcodeSelectError(f"Xcode {version} is not installed; install with: xcodes install {version}")


def select_version(version: str) -> None:
    require_xcodes()
    try:
        subprocess.run(
            ["xcodes", "select", version], check=True, capture_output=True, text=True, timeout=300
        )
    except (subprocess.CalledProcessError, OSError) as error:
        detail = getattr(error, "stderr", "") or str(error)
        raise XcodeSelectError(f"xcodes select {version} failed: {detail.strip()}") from error


def selected_developer_dir() -> str:
    try:
        observed = subprocess.run(
            ["xcode-select", "-p"], check=True, capture_output=True, text=True, timeout=30
        ).stdout.strip()
    except (subprocess.CalledProcessError, OSError, FileNotFoundError) as error:
        raise XcodeSelectError(f"xcode-select -p failed: {error}") from error
    if not observed:
        raise XcodeSelectError("xcode-select -p reported no developer directory")
    return observed


def observed_toolchain(developer_dir: str) -> tuple[str, str]:
    import os

    env = dict(os.environ, DEVELOPER_DIR=developer_dir)
    try:
        observed = subprocess.run(
            ["/usr/bin/xcodebuild", "-version"],
            check=True, capture_output=True, text=True, timeout=60, env=env,
        ).stdout.splitlines()
    except (subprocess.CalledProcessError, OSError, FileNotFoundError) as error:
        raise XcodeSelectError(f"xcodebuild -version failed for {developer_dir}: {error}") from error
    if len(observed) < 2:
        raise XcodeSelectError(f"xcodebuild -version reported too few lines for {developer_dir}")
    version = observed[0].removeprefix("Xcode ").strip()
    build = observed[1].removeprefix("Build version ").strip()
    return version, build


def verify(repo_root: str | Path, build: str, developer_dir: str | None = None) -> dict:
    """Verify the selected Xcode without changing machine state."""
    version = read_requested_version(repo_root)
    require_xcodes()
    selected = selected_developer_dir()
    if developer_dir is not None and developer_dir != selected:
        raise XcodeSelectError(
            f"configured developer directory {developer_dir} does not match "
            f"the selected Xcode {selected}; unset it or run: xcodes select {version}"
        )
    require_installed(version)
    observed_version, observed_build = observed_toolchain(selected)
    if observed_version != version:
        raise XcodeSelectError(
            f"selected Xcode is Xcode {observed_version} (expected Xcode {version}); "
            f"run: xcodes select {version}"
        )
    if observed_build != build:
        raise XcodeSelectError(
            f"selected Xcode build is {observed_build} (expected {build}); "
            f"the accepted product toolchain identity is Xcode {version} build {build}"
        )
    return {
        "schema": 1,
        "kind": "xcode-selection",
        "version": observed_version,
        "build": observed_build,
        "developer_dir": selected,
    }


def select(repo_root: str | Path, build: str) -> dict:
    """Select the requested Xcode, then verify it. May prompt for sudo."""
    version = read_requested_version(repo_root)
    select_version(version)
    return verify(repo_root, build)


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", required=True)
    parser.add_argument("--build", required=True)
    parser.add_argument("--developer-dir", default=None)
    parser.add_argument("--select", action="store_true")
    args = parser.parse_args(argv)
    try:
        if args.select:
            payload = select(args.repo, args.build)
        else:
            payload = verify(args.repo, args.build, developer_dir=args.developer_dir)
    except XcodeSelectError as error:
        print(f"xcode-select: {error}", file=sys.stderr)
        return 1
    print(json.dumps(payload, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
