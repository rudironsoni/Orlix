"""Two ext4 builds of one tree must be byte-identical."""

from __future__ import annotations

import ctypes
import fcntl
import os
import shutil
import struct
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
        'set_inode_field "%s" generation 0',
        "1\\t/lost+found",
        "set_inode_field <8> generation 0",
        ".orlix-csum",
        '"$debugfs" -n -w',
        "set_super_value mkfs_time 1",
        "set_super_value wtime 1",
        "set_super_value lastcheck 1",
        "set_super_value kbytes_written 0",
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


def _set_xattr(path: Path, name: str, payload: bytes) -> None:
    if hasattr(os, "setxattr"):
        os.setxattr(path, name, payload)
        return
    tool = shutil.which("xattr")
    if tool is None:
        raise AssertionError("missing xattr")
    completed = subprocess.run(
        [tool, "-w", name, payload.decode("utf-8"), str(path)],
        check=False,
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0:
        raise AssertionError(
            f"xattr -w failed ({completed.returncode})\n{completed.stdout}\n{completed.stderr}"
        )


def _ioctl_getversion() -> int:
    """FS_IOC_GETVERSION: _IOR('v', 1, long)."""
    size = ctypes.sizeof(ctypes.c_long)
    direction = 2
    return (direction << 30) | (size << 16) | (ord("v") << 8) | 1


def _host_generation(path: Path) -> int:
    """Host inode generation. macOS exposes st_gen; Linux exposes FS_IOC_GETVERSION."""
    stat = path.stat(follow_symlinks=False)
    generation = getattr(stat, "st_gen", None)
    if generation is not None:
        return int(generation)
    fd = os.open(path, os.O_RDONLY)
    try:
        raw = fcntl.ioctl(fd, _ioctl_getversion(), struct.pack("l", 0))
    except OSError as error:
        raise AssertionError(f"cannot read inode generation on {path}: {error}") from error
    finally:
        os.close(fd)
    if not isinstance(raw, bytes):
        raise AssertionError(f"inode generation ioctl returned {raw!r}")
    return int(struct.unpack("l", raw)[0])


def _source_generations(tree: Path) -> set[int] | None:
    """Host generations, or None when this host cannot report them.

    One tree does not need two values. macOS reports st_gen 0 for every new file.
    """
    generations: set[int] = set()
    for path in [tree, *tree.rglob("*")]:
        if path.is_symlink():
            continue
        try:
            generations.add(_host_generation(path))
        except AssertionError:
            return None
    return generations


def _generation(image: Path, target: str = "/bin/tool") -> str:
    completed = subprocess.run(
        [_tool("debugfs"), "-R", f"stat {target}", str(image)],
        check=False,
        capture_output=True,
        text=True,
    )
    output = f"{completed.stdout}\n{completed.stderr}"
    for line in output.splitlines():
        if line.startswith("Generation:"):
            return line.split()[1]
    raise AssertionError(f"debugfs stat {target} has no generation\n{output}")


def _kbytes_written(image: Path) -> str:
    completed = subprocess.run(
        [_tool("dumpe2fs"), "-h", str(image)],
        check=False,
        capture_output=True,
        text=True,
    )
    output = f"{completed.stdout}\n{completed.stderr}"
    for line in output.splitlines():
        if "Lifetime writes" in line:
            return line.split(":")[-1].strip()
    if completed.returncode != 0:
        raise AssertionError(f"dumpe2fs failed ({completed.returncode})\n{output}")
    # e2fsprogs prints this counter only when it is non-zero.
    return "0"


def _mark_output_base(tree: Path, output_base: Path) -> None:
    """Store the output-base path in host xattrs on the tree mke2fs reads."""
    payload = str(output_base).encode()
    _set_xattr(tree, "user.output_base", payload)
    for path in tree.rglob("*"):
        if path.is_symlink():
            continue
        _set_xattr(path, "user.output_base", payload)


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

    def test_two_directories_build_the_same_ext4_bytes(self) -> None:
        function = _build_ext4_function()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            images = []
            for name in ("seed-output-base", "uncached-output-base"):
                tree = root / name / "execroot" / "tree"
                _populate(tree)
                image = root / name / "base.ext4"
                _build(function, tree, image, root / name / "work")
                images.append(image)
            left = images[0].read_bytes()
            right = images[1].read_bytes()
            self.assertEqual(left, right, _first_difference(left, right))
            _assert_stat(images[0])
            _assert_stat(images[1])
            for image in images:
                _assert_stat(image, "/")
                _assert_stat(image, "/empty")
                _assert_stat(image, "/lost+found")
                self.assertEqual(_generation(image), "0")
                self.assertEqual(_generation(image, "/"), "0")
                self.assertEqual(_generation(image, "/lost+found"), "0")
                self.assertEqual(_kbytes_written(image), "0")

    def test_distinct_source_generations_do_not_change_ext4_bytes(self) -> None:
        function = _build_ext4_function()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            images = []
            for index in (1, 2):
                tree = root / f"gen-{index}" / "tree"
                _populate(tree)
                # macOS reports st_gen 0 for every new file. That is not a failure.
                _source_generations(tree)
                image = root / f"gen-{index}" / "base.ext4"
                _build(function, tree, image, root / f"gen-{index}" / "work")
                images.append(image)
            left = images[0].read_bytes()
            right = images[1].read_bytes()
            self.assertEqual(left, right, _first_difference(left, right))
            for image in images:
                _assert_stat(image, "/")
                _assert_stat(image, "/empty")
                _assert_stat(image, "/lost+found")
                self.assertEqual(_generation(image), "0")
                self.assertEqual(_generation(image, "/lost+found"), "0")
                self.assertEqual(_kbytes_written(image), "0")


if __name__ == "__main__":
    unittest.main()
