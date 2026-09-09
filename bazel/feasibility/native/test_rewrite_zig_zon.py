#!/usr/bin/env python3
"""Rewrite Ghostty zon URLs to local paths so zig build stays offline."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

import rewrite_zig_zon


SAMPLE = """
.{
    .dependencies = .{
        .uucode = .{
            .url = "https://deps.files.ghostty.org/uucode-2826a37a4562284fdacd8fa029d49509cc9bffcd.tar.gz",
            .hash = "uucode-0.2.0-ZZjBPlK5VADj7fdoq7G8LIHzD5o6FSkcBXXrRWr4jnrA",
        },
        .missing = .{
            .url = "https://example.invalid/missing.tar.gz",
            .hash = "missing-0.0.0-not-in-tree",
        },
    },
}
"""


class RewriteZigZonTests(unittest.TestCase):
    def test_known_hash_becomes_local_path(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            pkg = root / "uucode-0.2.0-ZZjBPlK5VADj7fdoq7G8LIHzD5o6FSkcBXXrRWr4jnrA"
            pkg.mkdir()
            zon_dir = root / "src"
            zon_dir.mkdir()
            text = rewrite_zig_zon.rewrite_text(SAMPLE, zon_dir, {pkg.name: pkg})
            self.assertIn('.path = "../uucode-0.2.0-ZZjBPlK5VADj7fdoq7G8LIHzD5o6FSkcBXXrRWr4jnrA"', text)
            self.assertNotIn("deps.files.ghostty.org/uucode-", text)
            self.assertIn("https://example.invalid/missing.tar.gz", text)

    def test_materialize_rewrites_copied_tree(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            packages = root / "p"
            ghostty = root / "ghostty"
            pkg_name = "uucode-0.2.0-ZZjBPlK5VADj7fdoq7G8LIHzD5o6FSkcBXXrRWr4jnrA"
            (packages / pkg_name).mkdir(parents=True)
            ghostty.mkdir()
            (ghostty / "build.zig.zon").write_text(SAMPLE, encoding="utf-8")
            rewrite_zig_zon.materialize(ghostty, packages)
            text = (ghostty / "build.zig.zon").read_text(encoding="utf-8")
            self.assertIn(".path = \"vendor-zig/", text)
            self.assertNotIn("deps.files.ghostty.org/uucode-", text)
            self.assertTrue((ghostty / "vendor-zig" / pkg_name).is_dir())

    def test_rewrite_replaces_readonly_zon(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            pkg_name = "uucode-0.2.0-ZZjBPlK5VADj7fdoq7G8LIHzD5o6FSkcBXXrRWr4jnrA"
            vendor = root / "vendor-zig" / pkg_name
            vendor.mkdir(parents=True)
            zon = root / "build.zig.zon"
            zon.write_text(SAMPLE, encoding="utf-8")
            zon.chmod(0o444)
            rewrite_zig_zon.rewrite_existing_tree(root)
            text = zon.read_text(encoding="utf-8")
            self.assertIn(".path = \"vendor-zig/", text)
            self.assertNotIn("deps.files.ghostty.org/uucode-", text)


if __name__ == "__main__":
    unittest.main()
