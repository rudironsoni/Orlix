#!/usr/bin/env python3
"""Fast red-capable gate: can Bazel analyze every promoted provider from a
reconstructed schema-2 component set?

Synthesizes the canonical schema-2 product for each of the seven components,
stages them through the real substitute.stage_imported() path, and asks Bazel
to analyze the real //bazel/promotion promoted targets. It never touches a
signed buildset and never publishes. Staging is disposable: any existing
bazel/promotion/imported projection is saved and restored.

One command:
    python3 bazel/promotion/promoted_contract_check.py
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import substitute
from content_digest import artifact_manifest_v2
from locked_buildset import buildset_digest_v2

ROOT = Path(__file__).resolve().parents[2]
STAGE = ROOT / "bazel" / "promotion" / "imported"

PROMOTED_TARGETS = [
    "//bazel/promotion:promoted_uapi",
    "//bazel/promotion:promoted_sysroot",
    "//bazel/promotion:promoted_rootfs",
    "//bazel/promotion:promoted_apple_inputs",
    "//bazel/promotion:promoted_kernel_release_iphoneos",
    "//bazel/promotion:promoted_kernel_release_iphonesimulator",
    "//bazel/promotion:promoted_kernel_development_iphoneos",
    "//bazel/promotion:promoted_kernel_development_iphonesimulator",
]

# Canonical semantic product per component: everything its promoted provider
# exposes and its downstream consumers genuinely require.
KERNEL_PRODUCT = {
    "OrlixKernel.a": b"kernel archive\n",
    "arch/orlix/boot/dts/development.dtb": b"development dtb\n",
    "arch/orlix/boot/dts/release.dtb": b"release dtb\n",
}

PRODUCTS = {
    "uapi": {
        "include/linux/unistd.h": b"uapi unistd\n",
        "include/asm/unistd.h": b"asm unistd\n",
        "kbuild-archive.tar": b"kbuild archive\n",
        "manifest.json": b'{"component": "OrlixKernel"}\n',
        "uapi.sha256": b"0" * 64 + b"\n",
    },
    "mlibc": {
        "usr/include/stdio.h": b"stdio\n",
        "usr/lib/libc.a": b"libc\n",
        "usr/lib/ld.so": b"loader\n",
        "libcompiler_rt.a": b"runtime\n",
        "abi.txt": b"_start\n",
        "manifest.json": b'{"component": "OrlixMLibC"}\n',
        "sysroot.sha256": b"1" * 64 + b"\n",
    },
    "rootfs": {
        "initramfs.cpio.gz": b"initramfs\n",
        "base.ext4": b"base\n",
        "state.ext4": b"state\n",
        "file-manifest.txt": b"./bin\n",
        "payload-metadata.txt": b"init=/init\n",
        "source-input.sha256": b"2" * 64 + b"\n",
    },
    "kernel-release-iphoneos": dict(KERNEL_PRODUCT),
    "kernel-release-iphonesimulator": dict(KERNEL_PRODUCT),
    "kernel-development-iphoneos": dict(KERNEL_PRODUCT),
    "kernel-development-iphonesimulator": dict(KERNEL_PRODUCT),
}


def build_fixture(root: Path, name: str, files: dict[str, bytes]) -> dict:
    component = root / name
    product = component / "product"
    for relative, content in files.items():
        target = product / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(content)
        target.chmod(0o644)
    manifest = artifact_manifest_v2(product, files_only=True)
    digest = hashlib.sha256(manifest).hexdigest()
    (component / "artifact-identity-v2.json").write_bytes(manifest)
    (component / "artifact-identity-v2.sha256").write_text(digest + "\n", encoding="ascii")
    return {
        "unsigned_digest": digest,
        "artifact_identity": {
            "format": "artifact-identity-v2",
            "version": 2,
            "digest": digest,
        },
        "oci_digest": "sha256:" + "a" * 64,
        "oci_reference": "ghcr.io/rudironsoni/orlix/%s@sha256:%s" % (name, "a" * 64),
    }


def stage_fixtures(workspace: Path) -> dict:
    fixtures = workspace / "fixtures"
    components = {
        name: build_fixture(fixtures, name, files) for name, files in PRODUCTS.items()
    }
    buildset = buildset_digest_v2(components)
    reconstruct = workspace / buildset
    reconstruct.mkdir()
    for name in components:
        (fixtures / name).rename(reconstruct / name)
    lock = workspace / "artifacts.lock.json"
    lock.write_text(
        json.dumps(
            {"schema": 2, "buildset": buildset, "components": components}, indent=2
        )
        + "\n",
        encoding="utf-8",
    )
    payload = substitute.substitute(str(lock), str(workspace), str(workspace / "out.json"))
    return payload


def run_bazel() -> int:
    bazel = os.environ.get("ORLIX_BAZEL")
    if not bazel or not Path(bazel).is_file():
        for candidate in (
            Path.home() / "Library/Caches/Orlix/Tools/bazel/9.2.0/bazel",
            Path("/opt/homebrew/bin/bazel"),
            Path("/usr/local/bin/bazel"),
        ):
            if candidate.is_file():
                bazel = str(candidate)
                break
    if not bazel:
        raise SystemExit("bazel is required for the promoted contract check")
    command = [
        bazel,
        "build",
        "--nobuild",
        *PROMOTED_TARGETS,
        "--noshow_progress",
        f"--repo_env=DEVELOPER_DIR={os.environ.get('DEVELOPER_DIR', '')}",
    ]
    completed = subprocess.run(command, cwd=str(ROOT), text=True)
    return completed.returncode


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.parse_args(argv)
    import tempfile

    backup = None
    if STAGE.exists() or STAGE.is_symlink():
        backup = STAGE.with_name("imported.contract-check-backup")
        if backup.exists() or backup.is_symlink():
            shutil.rmtree(backup, ignore_errors=True)
        STAGE.rename(backup)
    status = 1
    try:
        with tempfile.TemporaryDirectory(prefix="orlix-promoted-contract-") as tmp:
            payload = stage_fixtures(Path(tmp))
            substitute.stage_imported(payload, str(STAGE))
            status = run_bazel()
    finally:
        if STAGE.exists() or STAGE.is_symlink():
            shutil.rmtree(STAGE, ignore_errors=True)
        if backup is not None and (backup.exists() or backup.is_symlink()):
            backup.rename(STAGE)
    return status


if __name__ == "__main__":
    raise SystemExit(main())
