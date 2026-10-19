"""Two ext4 builds of one tree must be byte-identical."""

from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
import time
import unittest
from pathlib import Path


RULE = Path(__file__).with_name("rootfs.bzl")
TOOL_CANDIDATES = (
    "/usr/sbin/{name}",
    "/opt/homebrew/opt/e2fsprogs/sbin/{name}",
    "/usr/local/opt/e2fsprogs/sbin/{name}",
)


def _tool(name: str) -> str:
    found = shutil.which(name)
    if found and os.access(found, os.X_OK):
        return found
    for pattern in TOOL_CANDIDATES:
        path = pattern.format(name=name)
        if os.access(path, os.X_OK):
            return path
    raise AssertionError(f"missing {name}")


def _build_ext4_function() -> str:
    text = RULE.read_text(encoding="utf-8")
    start = text.index("build_ext4() {")
    end = text.index('\nbuild_ext4 "$base_tree"', start)
    body = text[start:end]
    if not body.rstrip().endswith("}"):
        raise AssertionError("build_ext4 function did not end at the rootfs call")
    for required in (
        "SOURCE_DATE_EPOCH=1 E2FSPROGS_FAKE_TIME=1",
        'set_inode_field "%s" atime 1',
        'set_inode_field "%s" ctime 1',
        'set_inode_field "%s" mtime 1',
        'set_inode_field "%s" crtime 1',
        "set_super_value mkfs_time 1",
        "set_super_value wtime 1",
        "set_super_value lastcheck 1",
        "-O ^orphan_file -U clear",
        "hash_seed=00000000-0000-0000-0000-000000000001",
        "no_copy_xattrs",
        "root_owner=0:0",
    ):
        if required not in body:
            raise AssertionError(f"build_ext4 is missing {required}")
    return body + "\n"


def _stamp(tree: Path, seconds: int, nanos: int) -> None:
    stamp = seconds * 1_000_000_000 + nanos
    for path in tree.rglob("*"):
        os.utime(path, ns=(stamp, stamp), follow_symlinks=False)
    os.utime(tree, ns=(stamp, stamp), follow_symlinks=False)


def _populate(tree: Path) -> None:
    for directory in ("bin", "sbin", "usr/bin"):
        binary = tree / directory
        binary.mkdir(parents=True)
        tool = binary / "tool"
        tool.write_bytes(b"#!/bin/sh\necho orlix\n")
        tool.chmod(0o755)
        (binary / "tool-link").symlink_to("tool")
    (tree / "empty").mkdir()


def _mark_output_base(tree: Path, output_base: Path) -> None:
    """Store the output-base path in host xattrs on the tree mke2fs reads."""
    payload = str(output_base).encode()
    os.setxattr(tree, "user.output_base", payload)
    for path in tree.rglob("*"):
        if path.is_symlink():
            continue
        os.setxattr(path, "user.output_base", payload)


def _build(function: str, tree: Path, image: Path, work: Path) -> None:
    work.mkdir(parents=True)
    image.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        ["/bin/dd", "if=/dev/zero", f"of={image}", "bs=1048576", "count=8", "status=none"],
        check=True,
    )
    script = f"""
set -euo pipefail
umask 022
mke2fs={_tool("mke2fs")!s}
debugfs={_tool("debugfs")!s}
work={work}
{function}
build_ext4 {tree} {image} ORLIXROOT
"""
    completed = subprocess.run(
        ["bash", "-c", script],
        check=False,
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0:
        owners = work / "owners.err"
        detail = owners.read_text(encoding="utf-8", errors="replace") if owners.exists() else ""
        raise AssertionError(
            f"build_ext4 failed ({completed.returncode})\n{completed.stdout}\n{completed.stderr}\n{detail}"
        )


def _assert_stat(image: Path, target: str = "/bin/tool") -> None:
    completed = subprocess.run(
        [_tool("debugfs"), "-R", f"stat {target}", str(image)],
        check=False,
        capture_output=True,
        text=True,
    )
    output = f"{completed.stdout}\n{completed.stderr}"
    if completed.returncode != 0 or "checksum does not match" in output or "Inode:" not in completed.stdout:
        raise AssertionError(f"debugfs stat {target} failed ({completed.returncode})\n{output}")


def _first_difference(left: bytes, right: bytes) -> str:
    if len(left) != len(right):
        return f"length {len(left)} != {len(right)}"
    for index, (first, second) in enumerate(zip(left, right)):
        if first != second:
            return f"offset {index}: {first:02x} != {second:02x}"
    return "identical"


class Ext4ByteTests(unittest.TestCase):
    def test_two_builds_match_when_source_times_differ(self) -> None:
        function = _build_ext4_function()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            tree = root / "tree"
            _populate(tree)
            first = root / "first.ext4"
            second = root / "second.ext4"
            _stamp(tree, 1_700_000_000, 111_111_111)
            _build(function, tree, first, root / "work-first")
            time.sleep(1.1)
            _stamp(tree, 1_800_000_000, 333_333_333)
            _build(function, tree, second, root / "work-second")
            _assert_stat(first)
            _assert_stat(second)
            left = first.read_bytes()
            right = second.read_bytes()
            self.assertEqual(left, right, _first_difference(left, right))

    def test_output_base_xattrs_do_not_change_ext4_bytes(self) -> None:
        function = _build_ext4_function()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            images = []
            for side, seconds in (("seed-output-base", 1_700_000_000), ("uncached-output-base", 1_800_000_000)):
                base = root / side / "execroot" / "_main"
                tree = base / "rootfs-work" / "base-tree"
                _populate(tree)
                _mark_output_base(tree, base)
                _stamp(tree, seconds, 111_111_111)
                image = base / "base.ext4"
                _build(function, tree, image, base / "work")
                images.append(image)
            left = images[0].read_bytes()
            right = images[1].read_bytes()
            self.assertEqual(left, right, _first_difference(left, right))
            self.assertNotIn(b"seed-output-base", left)
            self.assertNotIn(b"uncached-output-base", right)


if __name__ == "__main__":
    unittest.main()
