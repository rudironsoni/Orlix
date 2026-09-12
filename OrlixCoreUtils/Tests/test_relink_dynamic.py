#!/usr/bin/env python3
"""Drive the shipped dynamic relink of cp and check PT_INTERP."""

from __future__ import annotations

import os
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RELINK = Path(__file__).resolve().parent / "relink_dynamic.py"


class RelinkDynamicTests(unittest.TestCase):
    def test_cp_requests_mlibc_ldso(self) -> None:
        build_dir = ROOT / "Build/OrlixCoreUtils/build/release/coreutils-9.11"
        sysroot = ROOT / "Build/OrlixMLibC/sysroot/release"
        package_lib = ROOT / "Build/OrlixOS/packages/release/usr/lib"
        rtlib = ROOT / "Build/OrlixMLibC/compiler-rt/release/liborlix_compiler_rt.a"
        for required in (build_dir / "src/cp.o", sysroot / "usr/lib/libc.so", sysroot / "usr/lib/ld.so", rtlib):
            if not required.is_file():
                self.skipTest(f"missing {required}")
        scratch = Path(os.environ["ORLIXOS_HELPER_TEST_SCRATCH"]) if os.environ.get("ORLIXOS_HELPER_TEST_SCRATCH") else Path(tempfile.mkdtemp(prefix="orlix-dyn-cp-"))
        output = scratch / "cp"
        completed = subprocess.run(
            [
                "python3",
                str(RELINK),
                "cp",
                "--build-dir",
                str(build_dir),
                "--sysroot",
                str(sysroot),
                "--package-lib",
                str(package_lib),
                "--rtlib",
                str(rtlib),
                "--output",
                str(output),
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        self.assertGreater(output.stat().st_size, 0)
        readelf_bin = "llvm-readelf"
        if subprocess.run(["which", "llvm-readelf"], check=False, capture_output=True).returncode != 0:
            readelf_bin = "/opt/homebrew/opt/llvm/bin/llvm-readelf"
        readelf = subprocess.run(
            [readelf_bin, "-l", str(output)],
            check=True,
            capture_output=True,
            text=True,
        )
        self.assertIn("[Requesting program interpreter: /usr/lib/ld.so]", readelf.stdout)


if __name__ == "__main__":
    unittest.main()
