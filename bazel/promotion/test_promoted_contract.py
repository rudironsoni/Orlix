#!/usr/bin/env python3
"""Contract coverage for the promoted-provider analysis gate.

Two levels:
* a fast unit check that the synthetic spec and the Bazel BUILD agree on every
  component's product boundary and identity envelope; and
* the real analysis gate (skipped without Bazel) that analyzes all promoted
  providers from reconstructed schema-2 fixtures.
"""

from __future__ import annotations

import os
import subprocess
import sys
import unittest
from pathlib import Path

import promoted_contract_check
from components import load_registry

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "bazel" / "promotion" / "BUILD.bazel"


def _bazel() -> str | None:
    candidate = os.environ.get("ORLIX_BAZEL")
    if candidate and Path(candidate).is_file():
        return candidate
    for path in (
        Path.home() / "Library/Caches/Orlix/Tools/bazel/9.2.0/bazel",
        Path("/opt/homebrew/bin/bazel"),
        Path("/usr/local/bin/bazel"),
    ):
        if path.is_file():
            return str(path)
    return None


class PromotedContractSpecTests(unittest.TestCase):
    def test_spec_covers_every_registry_component(self) -> None:
        names = {entry["name"] for entry in load_registry()["components"]}
        self.assertEqual(set(promoted_contract_check.PRODUCTS), names)

    def test_build_references_every_product_boundary(self) -> None:
        text = BUILD.read_text(encoding="utf-8")
        for name in promoted_contract_check.PRODUCTS:
            stem = "kernel-%s-%s" if name.startswith("kernel-") else name
            self.assertIn(f"imported/{stem}/artifact-identity-v2.sha256", text)
            self.assertIn(f"imported/{stem}/product/", text)


@unittest.skipUnless(_bazel() is not None, "bazel is required for the promoted analysis gate")
class PromotedContractAnalysisTests(unittest.TestCase):
    def test_all_promoted_providers_analyze(self) -> None:
        env = dict(os.environ)
        env["ORLIX_BAZEL"] = _bazel()
        existing = env.get("PYTHONPATH", "")
        env["PYTHONPATH"] = os.pathsep.join(
            [str(ROOT / "bazel" / "promotion"), str(ROOT / "bazel"), existing]
        )
        completed = subprocess.run(
            [sys.executable, str(ROOT / "bazel" / "promotion" / "promoted_contract_check.py")],
            cwd=str(ROOT),
            env=env,
            capture_output=True,
            text=True,
            timeout=1800,
        )
        self.assertEqual(
            completed.returncode,
            0,
            (completed.stdout + completed.stderr)[-2000:],
        )


if __name__ == "__main__":
    unittest.main()
