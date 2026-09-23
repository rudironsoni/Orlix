#!/usr/bin/env python3
"""Make-side auto preflight: classify, optional UAPI probe, resolve origins."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

from origin_resolver import OriginError, enforce_promoted_compiler, payload, resolve
from worktree_classifier import ClassifierError, classify


def _lock_source_sha(lock_path: Path) -> str:
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "promotion"))
    from locked_buildset import load_locked_buildset, require_source_sha

    return require_source_sha(load_locked_buildset(str(lock_path)))


PROMOTED_UAPI_DIGEST = Path("bazel/promotion/imported/uapi/product/uapi.sha256")


def _acquire_promoted_uapi(repo: Path) -> None:
    make = os.environ.get("MAKE", "gmake")
    try:
        subprocess.run(
            [make, "-C", str(repo), "__bazel-reconstruct", "ORLIX_PROMOTED_ACQUIRE=uapi"],
            check=True,
        )
        reconstruct = repo / "Build/Bazel/reconstruct"
        out = repo / "Build/Bazel/proof/uapi-probe-promoted.json"
        out.parent.mkdir(parents=True, exist_ok=True)
        env = dict(os.environ)
        env["PYTHONPATH"] = str(repo / "bazel/promotion") + (
            os.pathsep + env["PYTHONPATH"] if env.get("PYTHONPATH") else ""
        )
        subprocess.run(
            [
                sys.executable,
                str(repo / "bazel/promotion/substitute.py"),
                "--lock",
                str(repo / "artifacts.lock.json"),
                "--reconstruct-dir",
                str(reconstruct),
                "--out",
                str(out),
                "--stage",
                str(repo / "bazel/promotion/imported"),
                "--components",
                "uapi",
            ],
            check=True,
            env=env,
        )
    except (OSError, subprocess.CalledProcessError) as error:
        raise ClassifierError("missing promoted UAPI semantic digest") from error


def _promoted_uapi_semantic_digest(repo: Path) -> str:
    digest_path = repo / PROMOTED_UAPI_DIGEST
    if not digest_path.is_file():
        _acquire_promoted_uapi(repo)
    if not digest_path.is_file():
        raise ClassifierError("missing promoted UAPI semantic digest")
    return _read_digest(digest_path)


def _read_digest(path: Path) -> str:
    try:
        text = path.read_text(encoding="utf-8").strip()
    except OSError as error:
        raise ClassifierError("UAPI probe digest is missing") from error
    if len(text) != 64 or any(char not in "0123456789abcdef" for char in text):
        raise ClassifierError("UAPI probe digest parse failure")
    return text


def probe_source_uapi(repo: Path) -> str:
    make = os.environ.get("MAKE", "gmake")
    try:
        subprocess.run([make, "-C", str(repo), "__bazel-kernel-uapi"], check=True)
    except (OSError, subprocess.CalledProcessError) as error:
        raise ClassifierError("UAPI probe build failure") from error
    return _read_digest(repo / "bazel-bin/bazel/feasibility/kernel/uapi/uapi.sha256")


def run_preflight(
    repo: Path,
    lock_path: Path,
    *,
    requested_mode: str,
    profile: str,
    destination: str,
    probe: bool = True,
) -> dict:
    if requested_mode in ("source", "promoted"):
        vector = resolve(requested_mode=requested_mode, lock_path=lock_path)
        return payload(vector, profile=profile, destination=destination)
    try:
        source_sha = _lock_source_sha(lock_path)
        classification = classify(repo, source_sha)
        facts = dict(classification.facts)
        if classification.probe_uapi:
            if not probe:
                raise ClassifierError("UAPI-sensitive change requires the source UAPI probe")
            locked = _promoted_uapi_semantic_digest(repo)
            source_digest = probe_source_uapi(repo)
            facts["uapi_changed"] = source_digest != locked
            facts["kernel_changed"] = True
        vector = resolve(requested_mode="auto", facts=facts, lock_path=lock_path)
        developer = Path(os.environ["DEVELOPER_DIR"]) if os.environ.get("DEVELOPER_DIR") else None
        enforce_promoted_compiler(repo, vector, developer)
        body = payload(vector, profile=profile, destination=destination)
        body["classification"] = {
            "classes": list(classification.classes),
            "probe_uapi": classification.probe_uapi,
            "source_sha": source_sha,
        }
        return body
    except (ClassifierError, OriginError, OSError, ValueError) as error:
        raise ClassifierError(str(error)) from error


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", default=".")
    parser.add_argument("--lock", required=True)
    parser.add_argument("--requested-mode", default="auto")
    parser.add_argument("--profile", default="release")
    parser.add_argument("--destination", default="iphonesimulator")
    parser.add_argument("--out")
    parser.add_argument("--print-flags", action="store_true")
    parser.add_argument("--print-acquire", action="store_true")
    parser.add_argument("--no-probe", action="store_true")
    args = parser.parse_args(argv)
    try:
        body = run_preflight(
            Path(args.repo).resolve(),
            Path(args.lock),
            requested_mode=args.requested_mode,
            profile=args.profile,
            destination=args.destination,
            probe=not args.no_probe,
        )
    except ClassifierError as error:
        print(f"auto-preflight: {error}", file=sys.stderr)
        return 1
    if args.out:
        destination = Path(args.out)
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(json.dumps(body, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    if args.print_acquire:
        print(" ".join(body["acquire"]))
        return 0
    if args.print_flags:
        print(" ".join(body["bazel_flags"]))
        return 0
    print(json.dumps(body, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
