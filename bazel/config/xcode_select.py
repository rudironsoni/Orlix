#!/usr/bin/env python3
"""Select and verify the product Xcode through xcodes, never guessed paths.

The requested Xcode release comes from the repository-root .xcode-version
file. `xcodes` is the sole authority for resolving that version: selection
runs `xcodes select --print-path` in the repository root so xcodes consumes
`.xcode-version` itself, and the read-only preflight uses xcodes' direct
lookup form `xcodes installed <version>`. Nothing here parses the
human-readable `xcodes installed` listing, normalizes Xcode versions, or
constructs an /Applications path. The selected developer directory is always
observed from `xcode-select -p`, and acceptance requires the exact product
version and build. The ambient XCODES_DIRECTORY / --directory contract is
honored by inheriting the environment and never passing --directory.
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


def _resolution_failure(version: str, detail: str) -> XcodeSelectError:
    message = f"xcodes could not resolve Xcode {version} in its configured Xcode directory"
    if detail:
        message += f": {detail}"
    return XcodeSelectError(f"{message}; install with: xcodes install {version}")


def resolve_requested(version: str) -> str:
    """Return xcodes' own resolved application path for the requested version.

    Uses xcodes' direct lookup form so xcodes performs the resolution. The
    configured Xcode directory (XCODES_DIRECTORY, --directory, or the
    /Applications default) is honored by inheriting the environment.
    """
    require_xcodes()
    try:
        observed = subprocess.run(
            ["xcodes", "installed", version], check=True, capture_output=True, text=True, timeout=60
        ).stdout.strip()
    except subprocess.CalledProcessError as error:
        detail = ((error.stderr or "") + " " + (error.stdout or "")).strip()
        raise _resolution_failure(version, detail) from error
    except OSError as error:
        raise _resolution_failure(version, str(error)) from error
    if not observed:
        raise _resolution_failure(version, "xcodes reported no path")
    return observed


def select_requested(repo_root: str | Path, version: str) -> str:
    """Run `xcodes select` in the repository root so xcodes reads .xcode-version.

    No version argument is passed: `.xcode-version` drives the selection.
    Returns xcodes' reported selected path verbatim.
    """
    require_xcodes()
    try:
        observed = subprocess.run(
            ["xcodes", "select", "--print-path"],
            check=True, capture_output=True, text=True, timeout=300, cwd=str(repo_root),
        ).stdout.strip()
    except subprocess.CalledProcessError as error:
        detail = ((error.stderr or "") + " " + (error.stdout or "")).strip()
        raise _resolution_failure(version, detail) from error
    except OSError as error:
        raise _resolution_failure(version, str(error)) from error
    if not observed:
        raise _resolution_failure(version, "xcodes reported no selected path")
    return observed


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
    resolved = resolve_requested(version)
    selected = selected_developer_dir()
    if developer_dir is not None and developer_dir != selected:
        raise XcodeSelectError(
            f"configured developer directory {developer_dir} does not match "
            f"the selected Xcode {selected}; unset it or run xcodes select from {repo_root}"
        )
    observed_version, observed_build = observed_toolchain(selected)
    if observed_version != version:
        raise XcodeSelectError(
            f"selected Xcode is Xcode {observed_version} (expected Xcode {version}); "
            f"run xcodes select from {repo_root}"
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
        "xcodes_resolved_path": resolved,
    }


def select(repo_root: str | Path, build: str) -> dict:
    """Select the requested Xcode via .xcode-version, then verify it. May prompt for sudo."""
    version = read_requested_version(repo_root)
    selected_path = select_requested(repo_root, version)
    payload = verify(repo_root, build)
    payload["xcodes_selected_path"] = selected_path
    return payload


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
