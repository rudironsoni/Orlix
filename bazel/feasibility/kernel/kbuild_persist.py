#!/usr/bin/env python3
"""Serialize Kbuild incremental output deterministically for Bazel."""

from __future__ import annotations

import argparse
import tarfile


def write_archive(source: str, output: str) -> None:
    def normalized(info: tarfile.TarInfo) -> tarfile.TarInfo:
        info.uid = 0
        info.gid = 0
        info.uname = ""
        info.gname = ""
        info.mtime = 0
        info.pax_headers = {}
        return info

    with tarfile.open(output, "w", format=tarfile.PAX_FORMAT) as archive:
        archive.add(source, arcname=".", filter=normalized)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="cmd", required=True)
    archive_p = sub.add_parser("archive")
    archive_p.add_argument("source")
    archive_p.add_argument("output")
    args = parser.parse_args(argv)
    write_archive(args.source, args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
