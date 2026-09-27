#!/usr/bin/env python3
"""The shipped stale-object list follows dependency mtimes."""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parent / "product_stale_objects.py"


class StaleObjectTests(unittest.TestCase):
    def test_cli_lists_only_objects_with_newer_or_missing_dependencies(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            objects = root / "objects"
            objects.mkdir()
            cwd = root / "work"
            cwd.mkdir()
            header = cwd / "header.h"
            header.write_text("int value;\n")
            fresh = objects / "fresh.o"
            fresh.write_bytes(b"fresh")
            stale = objects / "stale.o"
            stale.write_bytes(b"stale")
            missing = objects / "missing.o"
            missing.write_bytes(b"missing")
            escaped = cwd / "my header.h"
            escaped.write_text("int other;\n")
            escaped_obj = objects / "escaped.o"
            escaped_obj.write_bytes(b"escaped")
            now = 1_700_000_000_000_000_000
            os.utime(header, ns=(now, now))
            os.utime(escaped, ns=(now, now))
            os.utime(fresh, ns=(now + 2_000_000_000, now + 2_000_000_000))
            os.utime(stale, ns=(now - 2_000_000_000, now - 2_000_000_000))
            os.utime(escaped_obj, ns=(now, now))
            (objects / "fresh.d").write_text("fresh.o: header.h\n")
            (objects / "stale.d").write_text("stale.o: \\\n header.h\n")
            (objects / "escaped.d").write_text("escaped.o: my\\ header.h\n")
            completed = subprocess.run(
                [sys.executable, str(SCRIPT), "--objects", str(objects), "--cwd", str(cwd)],
                check=True,
                capture_output=True,
                text=True,
            )
            self.assertEqual(
                completed.stdout.splitlines(),
                [str(escaped_obj), str(missing), str(stale)],
            )


if __name__ == "__main__":
    unittest.main()
