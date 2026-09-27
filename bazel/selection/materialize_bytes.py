#!/usr/bin/env python3
"""Materialize selected bytes under stable logical names.

Trees use `cp -R -L`. Files use `cp -L`. Consumer outputs are regular files
and directories, never symlinks. Producer digest sidecars are not part of
the selected bytes. `artifact_identity_digest` is those bytes.
"""

from __future__ import annotations

import argparse
import hashlib
import shutil
import subprocess
import sys
from pathlib import Path

_BAZEL = Path(__file__).resolve().parents[1]
if str(_BAZEL) not in sys.path:
    sys.path.insert(0, str(_BAZEL))

from content_digest import artifact_identity_v2, artifact_manifest_v2

DTB_DIRECTORY = "arch/orlix/boot/dts"
PROVENANCE_NAMES = frozenset(
    {
        "archive.sha256",
        "consumed_uapi.sha256",
        "file-manifest.txt",
        "payload-metadata.txt",
        "source-input.sha256",
        "sysroot.sha256",
        "uapi.sha256",
    }
)


class MaterializeError(ValueError):
    pass


def dtb_logical_name(filename: str) -> str:
    name = Path(filename).name
    if name != filename or not name.endswith(".dtb") or name.startswith("."):
        raise MaterializeError(f"DTB path must stay under {DTB_DIRECTORY}")
    return f"{DTB_DIRECTORY}/{name}"


def materialize_file(src: Path, dest: Path) -> None:
    source = Path(src)
    target = Path(dest)
    if not source.exists() and not source.is_symlink():
        raise MaterializeError(f"selected boundary missing file: {source}")
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.is_symlink() or target.exists():
        if target.is_dir() and not target.is_symlink():
            raise MaterializeError(f"refusing to replace a directory with a file: {target}")
        target.unlink()
    completed = subprocess.run(
        ["/bin/cp", "-L", str(source), str(target)],
        check=False,
        text=True,
        capture_output=True,
    )
    if completed.returncode != 0:
        raise MaterializeError(completed.stderr.strip() or "cp -L failed")
    if target.is_symlink() or not target.is_file():
        raise MaterializeError(f"materialized file is not regular bytes: {target}")


def materialize_tree(src: Path, dest: Path) -> None:
    source = Path(src)
    target = Path(dest)
    if source.is_symlink():
        followed = source.resolve()
        if not followed.is_dir():
            raise MaterializeError(f"selected boundary missing tree: {source}")
    elif not source.is_dir():
        raise MaterializeError(f"selected boundary missing tree: {source}")
    if target.is_symlink():
        target.unlink()
    elif target.exists():
        shutil.rmtree(target)
    target.mkdir(parents=True)
    completed = subprocess.run(
        ["/bin/cp", "-R", "-L", f"{source}/.", f"{target}/"],
        check=False,
        text=True,
        capture_output=True,
    )
    if completed.returncode != 0:
        raise MaterializeError(completed.stderr.strip() or "cp -R -L failed")
    _drop_provenance(target)
    _reject_symlinks(target)


def selected_identity(root: Path | None = None, artifacts: list[tuple[str, Path]] | None = None) -> str:
    digest = artifact_identity_v2(root, artifacts=artifacts)
    if not digest:
        raise MaterializeError("artifact_identity_digest is missing")
    return digest


def write_identity(
    manifest: Path,
    digest_path: Path,
    *,
    root: Path | None = None,
    artifacts: list[tuple[str, Path]] | None = None,
) -> str:
    payload = artifact_manifest_v2(root, artifacts=artifacts)
    digest = hashlib.sha256(payload).hexdigest()
    if not digest:
        raise MaterializeError("artifact_identity_digest is missing")
    Path(manifest).write_bytes(payload)
    Path(digest_path).write_text(digest + "\n", encoding="ascii")
    return digest


def file_pairs(root: Path, prefix: str = "") -> list[tuple[str, Path]]:
    pairs: list[tuple[str, Path]] = []
    base = Path(root)
    for path in sorted(base.rglob("*")):
        if path.is_symlink():
            raise MaterializeError(f"symlink consumer input: {path}")
        if not path.is_file():
            continue
        relative = path.relative_to(base).as_posix()
        logical = f"{prefix}/{relative}" if prefix else relative
        pairs.append((logical, path))
    return pairs


def _drop_provenance(root: Path) -> None:
    for path in sorted(root.rglob("*"), reverse=True):
        if not path.is_symlink() and not path.is_file():
            continue
        if path.name in PROVENANCE_NAMES or path.name.endswith(".artifact-identity-v2.sha256"):
            path.unlink()
        elif path.name.endswith(".artifact-identity-v2.json"):
            path.unlink()


def _reject_symlinks(root: Path) -> None:
    for path in root.rglob("*"):
        if path.is_symlink():
            raise MaterializeError(f"symlink consumer input: {path}")


def _project_tree(src: Path, dest: Path, manifest: Path, digest: Path) -> None:
    materialize_tree(src, dest)
    write_identity(manifest, digest, artifacts=file_pairs(dest))


def _parse_maps(values: list[str]) -> list[tuple[str, Path, Path]]:
    if len(values) % 3:
        raise MaterializeError("file map requires logical, source, and destination")
    maps = []
    for index in range(0, len(values), 3):
        logical, src, dest = values[index : index + 3]
        maps.append((logical, Path(src), Path(dest)))
    return maps


def project_sysroot(
    *,
    headers_src: Path,
    headers_dest: Path,
    libraries_src: Path,
    libraries_dest: Path,
    runtime_src: Path,
    runtime_dest: Path,
    loader_src: Path,
    loader_dest: Path,
    manifest: Path,
    digest: Path,
) -> str:
    materialize_tree(headers_src, headers_dest)
    materialize_tree(libraries_src, libraries_dest)
    materialize_file(runtime_src, runtime_dest)
    materialize_file(loader_src, loader_dest)
    pairs = file_pairs(headers_dest, "headers") + file_pairs(libraries_dest, "libraries")
    pairs.extend(
        (
            ("libcompiler_rt.a", runtime_dest),
            ("ld.so", loader_dest),
        )
    )
    return write_identity(manifest, digest, artifacts=pairs)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    subcommands = parser.add_subparsers(dest="command", required=True)
    tree = subcommands.add_parser("project-tree")
    tree.add_argument("--src", type=Path, required=True)
    tree.add_argument("--dest", type=Path, required=True)
    tree.add_argument("--manifest", type=Path, required=True)
    tree.add_argument("--digest", type=Path, required=True)
    files = subcommands.add_parser("project-files")
    files.add_argument("--manifest", type=Path, required=True)
    files.add_argument("--digest", type=Path, required=True)
    files.add_argument("--map", nargs=3, action="append", required=True, metavar=("LOGICAL", "SRC", "DEST"))
    sysroot = subcommands.add_parser("project-sysroot")
    for name in (
        "headers-src",
        "headers-dest",
        "libraries-src",
        "libraries-dest",
        "runtime-src",
        "runtime-dest",
        "loader-src",
        "loader-dest",
        "manifest",
        "digest",
    ):
        sysroot.add_argument(f"--{name}", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        if args.command == "project-tree":
            _project_tree(args.src, args.dest, args.manifest, args.digest)
        elif args.command == "project-sysroot":
            project_sysroot(
                headers_src=args.headers_src,
                headers_dest=args.headers_dest,
                libraries_src=args.libraries_src,
                libraries_dest=args.libraries_dest,
                runtime_src=args.runtime_src,
                runtime_dest=args.runtime_dest,
                loader_src=args.loader_src,
                loader_dest=args.loader_dest,
                manifest=args.manifest,
                digest=args.digest,
            )
        else:
            pairs = []
            for logical, src, dest in _parse_maps([item for triple in args.map for item in triple]):
                if logical.startswith(f"{DTB_DIRECTORY}/") and not logical.endswith(".dtb"):
                    raise MaterializeError(f"DTB path must stay under {DTB_DIRECTORY}")
                materialize_file(src, dest)
                pairs.append((logical, dest))
            write_identity(args.manifest, args.digest, artifacts=pairs)
    except (MaterializeError, ValueError) as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
