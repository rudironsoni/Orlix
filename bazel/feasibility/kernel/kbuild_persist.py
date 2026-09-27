#!/usr/bin/env python3
"""Serialize Kbuild incremental output deterministically for Bazel."""

from __future__ import annotations

import argparse
import gzip
import hashlib
import io
import os
import re
import stat
import tarfile
import tempfile
from pathlib import Path, PurePosixPath
from typing import Iterable, Sequence


_SHA256 = re.compile(r"^[0-9a-f]{64}$")


def _normalized(info: tarfile.TarInfo) -> tarfile.TarInfo:
    info.uid = 0
    info.gid = 0
    info.uname = ""
    info.gname = ""
    info.mtime = 0
    info.pax_headers = {}
    return info


def _replacement_pairs(replacements: Sequence[tuple[str, str]]) -> list[tuple[str, str]]:
    pairs: list[tuple[str, str]] = []
    seen: set[str] = set()
    for old, new in replacements:
        if not old.startswith("/") or old in ("/", "") or not new:
            raise ValueError(f"archive replacement must rewrite an absolute path: {old!r}")
        for candidate in (old, os.path.realpath(old)):
            if candidate in seen:
                continue
            seen.add(candidate)
            pairs.append((candidate, new))
    pairs.sort(key=lambda item: len(item[0]), reverse=True)
    return pairs


def _rewrite_text(text: str, pairs: Sequence[tuple[str, str]]) -> str:
    for old, new in pairs:
        text = text.replace(old, new)
    return text


def _rewrite_bytes(data: bytes, pairs: Sequence[tuple[str, str]]) -> bytes:
    for old, new in pairs:
        data = data.replace(old.encode(), new.encode())
    return data


def stabilize_tree(root: str, replacements: Sequence[tuple[str, str]] = ()) -> None:
    """Rewrite absolute build paths in one output tree without changing its shape."""
    pairs = _replacement_pairs(replacements)
    if not pairs:
        return
    tree = Path(root)
    if not tree.is_dir():
        raise ValueError(f"missing tree to stabilize: {root}")
    paths = sorted(tree.rglob("*"), key=lambda path: path.relative_to(tree).as_posix())
    for path in paths:
        if path.is_symlink():
            target = os.readlink(path)
            updated = _rewrite_text(target, pairs)
            if updated != target:
                path.unlink()
                path.symlink_to(updated)
            continue
        if not path.is_file():
            continue
        mode = stat.S_IMODE(path.stat().st_mode)
        data = path.read_bytes()
        if data.startswith(b"\x7fELF") or data.startswith(b"!<arch>\n"):
            continue
        updated = _rewrite_bytes(data, pairs)
        if updated == data:
            continue
        if not os.access(path, os.W_OK):
            path.chmod(mode | 0o200)
        path.write_bytes(updated)
        path.chmod(mode)


def write_archive(source: str, output: str, replacements: Sequence[tuple[str, str]] = ()) -> None:
    """Write a sorted archive whose bytes ignore mtime and absolute build paths."""
    root = Path(source)
    if not root.is_dir():
        raise ValueError(f"missing archive source: {source}")
    pairs = _replacement_pairs(replacements)
    entries = sorted(root.rglob("*"), key=lambda path: path.relative_to(root).as_posix())
    with tarfile.open(output, "w", format=tarfile.PAX_FORMAT) as archive:
        root_info = _normalized(tarfile.TarInfo(name="."))
        root_info.type = tarfile.DIRTYPE
        root_info.mode = 0o755
        archive.addfile(root_info)
        for path in entries:
            relative = path.relative_to(root).as_posix()
            info = _normalized(tarfile.TarInfo(name=relative))
            if path.is_symlink():
                info.type = tarfile.SYMTYPE
                info.linkname = _rewrite_text(os.readlink(path), pairs)
                info.mode = 0o777
                archive.addfile(info)
                continue
            if path.is_dir():
                info.type = tarfile.DIRTYPE
                info.mode = 0o755
                archive.addfile(info)
                continue
            if not path.is_file():
                raise ValueError(f"unsupported archive entry: {relative}")
            data = _rewrite_bytes(path.read_bytes(), pairs)
            info.type = tarfile.REGTYPE
            info.mode = 0o644
            info.size = len(data)
            archive.addfile(info, io.BytesIO(data))


def _validated_names(members: Iterable[str]) -> list[str]:
    selected: list[str] = []
    seen: set[str] = set()
    for name in members:
        path_name = PurePosixPath(name)
        if (
            not name
            or path_name.is_absolute()
            or path_name.as_posix() != name
            or any(part in ("", ".", "..") for part in path_name.parts)
            or name in seen
        ):
            raise ValueError(f"invalid or duplicate archive member: {name!r}")
        selected.append(name)
        seen.add(name)
    if not selected:
        raise ValueError("archive member list is empty")
    return selected


def _validated_members(source: str, members: Iterable[str]) -> list[tuple[str, Path]]:
    root = Path(source).resolve(strict=True)
    selected: list[tuple[str, Path]] = []
    for name in _validated_names(members):
        path = root / name
        if not path.is_file() or path.is_symlink() or path.stat().st_size == 0:
            raise ValueError(f"archive member is not a regular file: {name}")
        selected.append((name, path))
    return selected


def _add_selected_members(
    archive: tarfile.TarFile, selected: Sequence[tuple[str, Path]]
) -> None:
    for name, path in selected:
        info = _normalized(archive.gettarinfo(str(path), arcname=name))
        info.mode = 0o644
        with path.open("rb") as source:
            archive.addfile(info, source)


def write_gzip_archive(source: str, output: str, members: Iterable[str]) -> None:
    selected = _validated_members(source, members)
    with Path(output).open("wb") as raw:
        with gzip.GzipFile(fileobj=raw, mode="wb", filename="", mtime=0) as compressed:
            with tarfile.open(
                fileobj=compressed, mode="w", format=tarfile.USTAR_FORMAT
            ) as archive:
                _add_selected_members(archive, selected)


def write_pin(archive_path: str, pin_path: str, archive_name: str | None = None) -> str:
    archive = Path(archive_path)
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    Path(pin_path).write_text(
        f"{digest}  {archive_name or archive.name}\n", encoding="utf-8"
    )
    return digest


def extract_archive(
    archive_path: str, pin_path: str, destination: str, members: Iterable[str]
) -> str:
    expected = _validated_names(members)
    data = Path(archive_path).read_bytes()
    fields = Path(pin_path).read_text(encoding="utf-8").split()
    if len(fields) != 2 or fields[1] != Path(archive_path).name or not _SHA256.fullmatch(fields[0]):
        raise ValueError("invalid archive pin")
    digest = hashlib.sha256(data).hexdigest()
    if digest != fields[0]:
        raise ValueError(f"archive digest mismatch: {digest} != {fields[0]}")
    output = Path(destination)
    with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as archive:
        infos = archive.getmembers()
        if [info.name for info in infos] != expected:
            raise ValueError("archive members differ from the canonical declaration")
        for info in infos:
            if not info.isreg() or info.size == 0 or info.mode != 0o644:
                raise ValueError(f"archive member is not a non-empty 0644 regular file: {info.name}")
        if output.is_symlink() or (output.exists() and (not output.is_dir() or any(output.iterdir()))):
            raise ValueError(f"extraction destination is not an empty directory: {output}")
        output.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=output.parent, prefix=f".{output.name}.") as temporary:
            tree = Path(temporary) / "tree"
            tree.mkdir()
            for info in infos:
                target = tree / info.name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(archive.extractfile(info).read())
                target.chmod(0o644)
            os.replace(tree, output)
    return digest


def publish_archive(
    source: str, archive_path: str, pin_path: str, members: Iterable[str]
) -> str:
    archive, pin = Path(archive_path), Path(pin_path)
    if archive == pin:
        raise ValueError("archive and pin paths must differ")
    archive.parent.mkdir(parents=True, exist_ok=True)
    pin.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=archive.parent, prefix=f".{archive.name}.") as temporary:
        candidate = Path(temporary) / "archive"
        candidate_pin = Path(temporary) / "pin"
        write_gzip_archive(source, str(candidate), members)
        digest = write_pin(str(candidate), str(candidate_pin), archive.name)
        candidate.chmod(0o644)
        candidate_pin.chmod(0o644)
        os.replace(candidate, archive)
        os.replace(candidate_pin, pin)
    return digest


def _parse_replacements(values: list[str]) -> list[tuple[str, str]]:
    pairs: list[tuple[str, str]] = []
    for value in values:
        old, separator, new = value.partition("=")
        if not separator or not old or not new:
            raise ValueError(f"replacement must be OLD=NEW: {value!r}")
        pairs.append((old, new))
    return pairs


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="cmd", required=True)
    archive_p = sub.add_parser("archive")
    archive_p.add_argument("source")
    archive_p.add_argument("output")
    archive_p.add_argument("--replace", action="append", default=[])
    stabilize_p = sub.add_parser("stabilize")
    stabilize_p.add_argument("root")
    stabilize_p.add_argument("--replace", action="append", default=[])
    publish_p = sub.add_parser("publish")
    publish_p.add_argument("source")
    publish_p.add_argument("archive")
    publish_p.add_argument("pin")
    publish_p.add_argument("members", nargs="+")
    extract_p = sub.add_parser("extract")
    extract_p.add_argument("archive")
    extract_p.add_argument("pin")
    extract_p.add_argument("destination")
    extract_p.add_argument("members_file")
    args = parser.parse_args(argv)
    if args.cmd == "archive":
        write_archive(args.source, args.output, _parse_replacements(args.replace))
    elif args.cmd == "stabilize":
        stabilize_tree(args.root, _parse_replacements(args.replace))
    elif args.cmd == "publish":
        publish_archive(args.source, args.archive, args.pin, args.members)
    else:
        extract_archive(
            args.archive,
            args.pin,
            args.destination,
            Path(args.members_file).read_text(encoding="utf-8").splitlines(),
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
