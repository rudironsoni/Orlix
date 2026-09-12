#!/usr/bin/env python3
"""Locate local artifacts for every supported Apple matrix row."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


class EvidenceError(ValueError):
    pass


def _first(paths: list[Path]) -> Path:
    for path in paths:
        if path.is_file() and path.stat().st_size > 0:
            return path
        if path.is_dir() and any(path.iterdir()):
            nested = path / "Info.plist"
            if nested.is_file() and nested.stat().st_size > 0:
                return nested
    raise EvidenceError("missing evidence files: " + ", ".join(str(path) for path in paths))


def discover(output_base: Path, build_root: Path, repo: Path) -> dict[str, str]:
    bazel_out = output_base / "execroot/_main/bazel-out"
    ipas = [
        path
        for path in bazel_out.glob("*/bin/Orlix/Orlix.ipa")
        if "runfiles" not in path.parts
    ]
    app_ipa = _first(ipas)
    smoke = _first(
        [
            path
            for path in bazel_out.glob("*/bin/bazel/feasibility/apple/SmokeApp.ipa")
            if "runfiles" not in path.parts
        ]
    )
    opt_smoke = [
        path
        for path in bazel_out.glob("*-opt-*/bin/bazel/feasibility/apple/SmokeApp.ipa")
        if "runfiles" not in path.parts
    ]
    live = _first(
        [
            path
            for path in bazel_out.glob("*/bin/bazel/feasibility/apple/LiveActivitySmoke_archive-root/LiveActivitySmoke.appex/Info.plist")
            if "runfiles" not in path.parts
        ]
        + [
            path
            for path in bazel_out.glob("*/bin/Orlix/Orlix_archive-root/Payload/Orlix.app/PlugIns/OrlixLiveActivity.appex/Info.plist")
            if "runfiles" not in path.parts
        ]
    )
    device = _first(
        [
            path
            for path in bazel_out.glob("ios_arm64-*/bin/bazel/feasibility/apple/NativeDependencySmokeApp.ipa")
            if "runfiles" not in path.parts
        ]
    )
    mac = _first(
        [
            path
            for path in bazel_out.glob("darwin_arm64-*/bin/bazel/feasibility/apple/NativeDependencyMacSmokeApp.zip")
            if "runfiles" not in path.parts
        ]
    )
    locked = _first(
        [
            path
            for path in bazel_out.glob("*/bin/Orlix/Orlix_archive-root/Payload/Orlix.app/locked-buildset.json")
            if "runfiles" not in path.parts
        ]
        + [
            path
            for path in bazel_out.glob("*/bin/bazel/promotion/locked_buildset/locked-buildset.json")
            if "runfiles" not in path.parts
        ]
    )
    ios15 = build_root / "iOS15" / "Orlix-iOS15.log"
    if not ios15.is_file() or ios15.stat().st_size == 0:
        raise EvidenceError("missing iOS 15.5 launch log")
    evidence = {
        "ios-15.0-app-sdk-compile": str(app_ipa),
        "ipados-15.0-app-sdk-compile": str(app_ipa),
        "ios-15.5-ci-runtime": str(ios15),
        "ios-15.0-development-dbg-source": str(smoke),
        "ios-15.0-promoted-buildset": str(locked),
        "ios-16.1-live-activity": str(live),
        "iphoneos-15.0-device-compile": str(device),
        "macos-13.3-apple-silicon": str(mac),
    }
    if opt_smoke:
        evidence["ios-15.0-release-opt-source"] = str(_first(opt_smoke))
    return evidence


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-base", required=True)
    parser.add_argument("--build-root", required=True)
    parser.add_argument("--repo", required=True)
    parser.add_argument("--out")
    args = parser.parse_args(argv)
    evidence = discover(Path(args.output_base), Path(args.build_root), Path(args.repo))
    if args.out:
        Path(args.out).write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
    else:
        for row_id, path in evidence.items():
            print(f"{row_id}={path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
