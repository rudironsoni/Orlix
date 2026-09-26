#!/usr/bin/env python3
"""The shipped metadata scan reads fresh caches and refreshes a stale one."""

from __future__ import annotations

import os
import stat
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parent / "product_object_metadata.py"


class ObjectMetadataTests(unittest.TestCase):
    def test_cli_uses_fresh_caches_and_refreshes_a_stale_undefined_cache(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            metadata = root / "meta"
            metadata.mkdir()
            obj = root / "widget.o"
            obj.write_bytes(b"object")
            sections = metadata / "widget.o.sections"
            sections.write_text("__TEXT,__text\n")
            undefined = metadata / "widget.o.undefined"
            undefined.write_text("_old\n")
            future = obj.stat().st_mtime_ns + 1_000_000_000
            os.utime(sections, ns=(future, future))
            os.utime(undefined, ns=(obj.stat().st_mtime_ns - 1_000_000_000,) * 2)
            otool = root / "otool"
            otool.write_text("#!/bin/sh\nexit 99\n")
            otool.chmod(otool.stat().st_mode | stat.S_IEXEC)
            nm = root / "nm"
            nm.write_text("#!/bin/sh\nprintf '%s\\n' '                 U _fresh'\n")
            nm.chmod(nm.stat().st_mode | stat.S_IEXEC)
            listing = root / "objects"
            listing.write_text(str(obj) + "\n")
            sections_out = root / "sections"
            undefined_out = root / "undefined"
            subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--metadata",
                    str(metadata),
                    "--nm",
                    str(nm),
                    "--otool",
                    str(otool),
                    "--objects",
                    str(listing),
                    "--sections-out",
                    str(sections_out),
                    "--undefined-out",
                    str(undefined_out),
                ],
                check=True,
            )
            self.assertEqual(sections_out.read_text(), "__TEXT,__text\n")
            self.assertEqual(undefined_out.read_text(), "_fresh\n")


if __name__ == "__main__":
    unittest.main()
