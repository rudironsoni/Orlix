#!/usr/bin/env python3
"""Developer-only persistent Kbuild headers_install reuse."""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path


def identity(linux_revision: str, linux_tag_commit: str, xcode_build: str) -> str:
    return f"{linux_revision}:{linux_tag_commit}:{xcode_build}"


def can_reuse(persist_dir: str, ident: str) -> bool:
    if not persist_dir:
        return False
    root = Path(persist_dir)
    stamp = root / "stamp"
    hdr = root / "hdr" / "include" / "linux" / "unistd.h"
    archive = root / "kbuild-archive.tar"
    if not stamp.is_file() or not hdr.is_file() or not archive.is_file():
        return False
    return stamp.read_text(encoding="utf-8").strip() == ident


def reuse_outputs(persist_dir: str, ident: str, headers_out: str, archive_out: str) -> bool:
    if not persist_dir or not can_reuse(persist_dir, ident):
        return False
    root = Path(persist_dir)
    dest_hdr = Path(headers_out) / "include"
    if dest_hdr.exists():
        shutil.rmtree(dest_hdr)
    dest_hdr.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(root / "hdr" / "include", dest_hdr)
    Path(archive_out).parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(root / "kbuild-archive.tar", archive_out)
    return True


def store_outputs(persist_dir: str, ident: str, headers_src: str, archive_src: str) -> None:
    if not persist_dir:
        return
    root = Path(persist_dir)
    hdr = root / "hdr" / "include"
    if hdr.exists():
        shutil.rmtree(hdr)
    hdr.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(Path(headers_src) / "include", hdr)
    shutil.copy2(archive_src, root / "kbuild-archive.tar")
    (root / "stamp").write_text(ident + "\n", encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="cmd", required=True)
    ident_p = sub.add_parser("identity")
    ident_p.add_argument("linux_revision")
    ident_p.add_argument("linux_tag_commit")
    ident_p.add_argument("xcode_build")
    reuse_p = sub.add_parser("reuse")
    reuse_p.add_argument("persist_dir")
    reuse_p.add_argument("ident")
    reuse_p.add_argument("headers_out")
    reuse_p.add_argument("archive_out")
    store_p = sub.add_parser("store")
    store_p.add_argument("persist_dir")
    store_p.add_argument("ident")
    store_p.add_argument("headers_src")
    store_p.add_argument("archive_src")
    args = parser.parse_args(argv)
    if args.cmd == "identity":
        print(identity(args.linux_revision, args.linux_tag_commit, args.xcode_build))
        return 0
    if args.cmd == "reuse":
        return 0 if reuse_outputs(args.persist_dir, args.ident, args.headers_out, args.archive_out) else 1
    store_outputs(args.persist_dir, args.ident, args.headers_src, args.archive_src)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
