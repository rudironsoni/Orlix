#!/usr/bin/env python3
"""The shipped initcall symbol scan reads nm caches and the objects list."""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from product_initcall_symbols import collect

SCRIPT = Path(__file__).resolve().parent / "product_initcall_symbols.py"


class InitcallSymbolTests(unittest.TestCase):
    def test_collect_reads_fresh_cache_and_ignores_other_symbols(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            metadata = root / "meta"
            metadata.mkdir()
            obj = root / "widget.o"
            obj.write_bytes(b"object")
            cache = metadata / "widget.o.nm-m"
            cache.write_text(
                "0000000000000000 (__DATA,__initcall1) external ___initcall____widget\n"
                "0000000000000008 (__TEXT,__text) external _widget\n"
            )
            os.utime(cache, ns=(obj.stat().st_mtime_ns + 1_000_000_000,) * 2)
            self.assertEqual(
                collect(metadata, "nm-must-not-run", [str(obj)]),
                "__initcall1 ___initcall____widget",
            )

    def test_cli_reads_the_object_list(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            metadata = root / "meta"
            metadata.mkdir()
            obj = root / "board.o"
            obj.write_bytes(b"object")
            cache = metadata / "board.o.nm-m"
            cache.write_text("0000000000000000 (__DATA,__initcall0s) external ___initcall____board\n")
            os.utime(cache, ns=(obj.stat().st_mtime_ns + 1_000_000_000,) * 2)
            listing = root / "objects.txt"
            listing.write_text(str(obj) + "\n")
            proc = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--metadata",
                    str(metadata),
                    "--nm",
                    "nm-must-not-run",
                    "--objects",
                    str(listing),
                ],
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
            )
            self.assertEqual(proc.returncode, 0, proc.stderr)
            self.assertEqual(proc.stdout, "__initcall0s ___initcall____board\n")


if __name__ == "__main__":
    unittest.main()
