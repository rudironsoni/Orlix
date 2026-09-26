"""Linux rule text for guest package cache keys. No Bazel product build."""

from __future__ import annotations

import base64
import hashlib
import json
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
PACKAGES = ROOT / "bazel/feasibility/packages"
GUEST_RULES = ("coreutils.bzl", "bash.bzl", "autotools.bzl")
LOCAL_RULES = ("local.bzl", "package.bzl", "source_key.bzl")


def _execution_requirements(text: str) -> list[str]:
    return re.findall(r"execution_requirements\s*=\s*\{([^}]*)\}", text)


class PackageActionKeyTests(unittest.TestCase):
    def test_guest_packages_stay_uncached(self) -> None:
        for name in GUEST_RULES:
            text = (PACKAGES / name).read_text(encoding="utf-8")
            blocks = _execution_requirements(text)
            self.assertTrue(blocks, name)
            for block in blocks:
                self.assertIn("no-remote-cache", block, name)
            self.assertNotIn("use_default_shell_env = True", text, name)
            self.assertNotIn("TMPDIR", text, name)
            self.assertNotIn("source-input.sha256", text, name)

    def test_local_c_key_is_tar_plus_stamp(self) -> None:
        for name in LOCAL_RULES:
            text = (PACKAGES / name).read_text(encoding="utf-8")
            self.assertNotIn("no-remote-cache", text, name)
            self.assertNotIn("source-input.sha256", text, name)
        local = (PACKAGES / "local.bzl").read_text(encoding="utf-8")
        package = (PACKAGES / "package.bzl").read_text(encoding="utf-8")
        for text in (local, package):
            self.assertIn("declare_source_key", text)
            self.assertIn("source_archive", text)
            self.assertIn("source_stamp", text)
            self.assertIn("test -s \"$source_archive\"", text)
            self.assertIn("test -s \"$source_stamp\"", text)
            self.assertNotIn("TMPDIR", text)
            self.assertIn("use_default_shell_env = False", text)
        self.assertIn("configure_directory", package)
        build = (PACKAGES / "BUILD.bazel").read_text(encoding="utf-8")
        self.assertIn('name = "true_configure_directory"', build)
        self.assertIn('configure_directory = ":true_configure_directory"', build)
        self.assertIn('glob(["true/**"])', build)

    def test_lockfile_matches_coreutils_patch_input(self) -> None:
        extension = ROOT / "bazel/extensions/native_sources.bzl"
        text = extension.read_text(encoding="utf-8")
        block = re.search(r'name = "orlix_coreutils_source",(?P<body>.*?)\n    \)', text, re.S)
        self.assertIsNotNone(block)
        patch = re.search(r'patch = "([^"]+)"', block.group("body"))
        self.assertIsNotNone(patch)
        label = patch.group(1)
        self.assertEqual(label, "//bazel/extensions:coreutils-echo.patch")
        self.assertTrue((ROOT / "bazel/extensions/coreutils-echo.patch").is_file())
        exports = (ROOT / "bazel/extensions/BUILD.bazel").read_text(encoding="utf-8")
        self.assertIn('"coreutils-echo.patch"', exports)
        lock = json.loads((ROOT / "MODULE.bazel.lock").read_text(encoding="utf-8"))
        general = lock["moduleExtensions"]["//bazel/extensions:native_sources.bzl%native_sources"]["general"]
        attributes = general["generatedRepoSpecs"]["orlix_coreutils_source"]["attributes"]
        self.assertEqual(attributes["patch"], "@@" + label)
        digest = base64.b64encode(hashlib.sha256(hashlib.sha256(extension.read_bytes()).digest()).digest()).decode()
        self.assertEqual(general["bzlTransitiveDigest"], digest)
        self.assertEqual(general["usagesDigest"], "x1dTuAEuGxvds9QeOELcYr++4Nw4CyPJYhMfGMQ1HbA=")


if __name__ == "__main__":
    unittest.main()
