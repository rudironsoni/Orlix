"""One deterministic archive of the upstream Linux tree.

Bazel configures one target per file in a filegroup glob. The kernel still
needs every source byte: this archive is that tree, packed after Bazel
extracts it, and the macho action unpacks it before Kbuild.
"""

from __future__ import annotations

import os
from pathlib import Path, PurePosixPath
import stat
import sys
import tarfile

ARCHIVE_NAME = "linux-source.tar"
_EXCLUDED = {"BUILD.bazel", ARCHIVE_NAME}


def package_build(exports: list[str]) -> str:
    """BUILD text for the Linux repo. :source is one archive, not a glob."""
    if not exports:
        raise ValueError("linux source package requires explicit exports")
    quoted = "\n".join('    "%s",' % name for name in exports)
    return (
        'package(default_visibility = ["//visibility:public"])\n'
        "\n"
        "exports_files([\n"
        "%s\n"
        "])\n"
        "\n"
        "filegroup(\n"
        '    name = "source",\n'
        '    srcs = ["%s"],\n'
        ")\n"
    ) % (quoted, ARCHIVE_NAME)


def configured_source_files(build_text: str) -> list[str]:
    """Source labels configured for the :source filegroup."""
    if 'glob(["**"]' in build_text or "glob(['**']" in build_text:
        raise ValueError("linux source package globs every file")
    marker = 'name = "source"'
    start = build_text.index(marker)
    block = build_text[start:build_text.index(")", start)]
    if "glob(" in block:
        raise ValueError("source filegroup globs files")
    srcs = block.split("srcs = [", 1)[1].split("]", 1)[0]
    return [item.strip().strip('"') for item in srcs.split(",") if item.strip()]


def _relative(root: Path, path: Path) -> str:
    name = path.relative_to(root).as_posix()
    if not name or name.startswith("/") or ".." in PurePosixPath(name).parts:
        raise ValueError("invalid linux source path: %s" % name)
    return name


def _members(root: Path) -> list[Path]:
    found: list[Path] = []
    for current, dirnames, filenames in os.walk(root, followlinks=False):
        current_path = Path(current)
        kept = []
        for name in dirnames:
            path = current_path / name
            if path.is_symlink():
                found.append(path)
            else:
                kept.append(name)
                found.append(path)
        dirnames[:] = kept
        for name in filenames:
            path = current_path / name
            relative = _relative(root, path)
            if relative in _EXCLUDED:
                continue
            found.append(path)
    return sorted(found, key=lambda path: _relative(root, path))


def _tarinfo(path: Path, name: str) -> tarfile.TarInfo:
    info = tarfile.TarInfo(name)
    info.mtime = 0
    info.uid = 0
    info.gid = 0
    info.uname = ""
    info.gname = ""
    info.pax_headers = {}
    if path.is_symlink():
        info.type = tarfile.SYMTYPE
        info.linkname = os.readlink(path)
        info.mode = 0o777
        return info
    if path.is_dir():
        info.type = tarfile.DIRTYPE
        info.mode = 0o755
        return info
    if path.is_file():
        info.type = tarfile.REGTYPE
        info.mode = 0o755 if path.stat().st_mode & 0o111 else 0o644
        info.size = path.stat().st_size
        return info
    raise ValueError("unsupported linux source path: %s" % name)


def pack(root: Path, archive: Path) -> None:
    """Pack root into a byte-stable tar. Executable bits follow Kbuild inputs."""
    root = root.resolve()
    if not (root / "Makefile").is_file():
        raise ValueError("linux source archive requires a Makefile")
    archive.parent.mkdir(parents=True, exist_ok=True)
    temporary = archive.with_suffix(archive.suffix + ".tmp")
    if temporary.exists():
        temporary.unlink()
    with tarfile.open(temporary, "w", format=tarfile.PAX_FORMAT) as tar:
        for path in _members(root):
            name = _relative(root, path)
            info = _tarinfo(path, name)
            if info.isfile():
                with path.open("rb") as stream:
                    tar.addfile(info, stream)
            else:
                tar.addfile(info)
    os.replace(temporary, archive)


def extract(archive: Path, destination: Path) -> None:
    """Unpack one linux source archive. Rejects paths that leave destination."""
    destination.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive, "r:") as tar:
        for member in tar.getmembers():
            name = member.name
            if not name or name.startswith("/") or ".." in PurePosixPath(name).parts:
                raise ValueError("invalid linux archive member: %s" % name)
            target = destination / name
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            if target.is_symlink() or target.exists():
                target.unlink()
            if member.issym():
                target.symlink_to(member.linkname)
            elif member.isfile():
                extracted = tar.extractfile(member)
                if extracted is None:
                    raise ValueError("missing linux archive member: %s" % name)
                target.write_bytes(extracted.read())
                target.chmod(stat.S_IMODE(member.mode))
            else:
                raise ValueError("unsupported linux archive member: %s" % name)


def main(argv: list[str]) -> None:
    command = argv[1]
    if command == "pack":
        pack(Path(argv[2]), Path(argv[3]))
    elif command == "extract":
        extract(Path(argv[2]), Path(argv[3]))
    elif command == "package-build":
        sys.stdout.write(package_build(argv[2:]))
    else:
        raise SystemExit("unknown linux source archive command: %s" % command)


if __name__ == "__main__":
    main(sys.argv)
