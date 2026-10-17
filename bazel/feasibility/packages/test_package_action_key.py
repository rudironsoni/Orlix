"""Linux rule text for guest package cache keys. No Bazel product build."""

from __future__ import annotations

import base64
import hashlib
import json
import os
import re
import subprocess
import sys
import sysconfig
import tarfile
import tempfile
import textwrap
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
PACKAGES = ROOT / "bazel/feasibility/packages"
GUEST_RULES = ("coreutils.bzl", "bash.bzl", "autotools.bzl")
LOCAL_RULES = ("local.bzl", "package.bzl", "source_key.bzl")


def _execution_requirements(text: str) -> list[str]:
    return re.findall(r"execution_requirements\s*=\s*\{([^}]*)\}", text)


def _starlark_extract_command(text: str) -> str:
    match = re.search(r'def _extract\(\):\n(?:    #[^\n]*\n)?    return "(.*)"\n', text)
    if match is None:
        raise AssertionError("local extraction command is missing")
    return match.group(1).replace('\\"', '"')


class PackageActionKeyTests(unittest.TestCase):
    def test_guest_packages_declare_staged_tools(self) -> None:
        for name in ("coreutils.bzl", "bash.bzl", "local.bzl", "package.bzl"):
            text = (PACKAGES / name).read_text(encoding="utf-8")
            self.assertNotIn("no-remote-cache", text, name)
            self.assertNotIn("use_default_shell_env = True", text, name)
            self.assertNotIn("TMPDIR", text, name)
            self.assertNotIn("source-input.sha256", text, name)
            self.assertNotIn("DEVELOPER_DIR", text, name)
            self.assertNotIn("CCACHE_DIR", text, name)
            self.assertNotIn("/opt/homebrew", text, name)
            self.assertIn("no-remote-exec", text, name)
        for name in ("local.bzl", "package.bzl"):
            text = (PACKAGES / name).read_text(encoding="utf-8")
            self.assertIn('tool_bin="$(/usr/bin/dirname "$gmake")"', text, name)
            self.assertIn('tool_root="$(/usr/bin/dirname "$tool_bin")"', text, name)
            self.assertIn('DYLD_LIBRARY_PATH="$tool_root/lib:$tool_root/llvm/lib"', text, name)
            self.assertNotIn('dirname "$tool_bin")/lib', text, name)
        autotools_text = (PACKAGES / "autotools.bzl").read_text(encoding="utf-8")
        self.assertIn("no-remote-cache", autotools_text)
        self.assertNotIn("DEVELOPER_DIR", autotools_text)
        self.assertNotIn("CCACHE_DIR", autotools_text)
        for name in ("coreutils.bzl", "bash.bzl", "autotools.bzl"):
            text = (PACKAGES / name).read_text(encoding="utf-8")
            self.assertIn("ORLIX_PACKAGE_TOOL_BIN", text, name)
            self.assertIn("tool_bin=sys.argv[5]", text, name)
            self.assertIn("/orlix/tools", text, name)
            self.assertIn("ffile-prefix-map", text, name)
            self.assertIn("$work", text, name)
        autotools = (PACKAGES / "autotools.bzl").read_text(encoding="utf-8")
        self.assertIn('export PATH="$tool_bin:$work/toolchain:/usr/bin:/bin"', autotools)
        self.assertIn('export CC="${launcher:+$launcher }$clang --target=aarch64-linux-gnu', autotools)
        self.assertNotIn('export CC=aarch64-linux-gnu-gcc', autotools)
        self.assertNotIn("/opt/homebrew/bin", autotools)
        blocks = _execution_requirements(autotools)
        guest = [block for block in blocks if "no-sandbox" in block]
        interface = [block for block in blocks if "no-sandbox" not in block]
        self.assertTrue(guest)
        self.assertTrue(interface)
        for block in guest:
            self.assertIn("no-remote-cache", block)
        for block in interface:
            self.assertNotIn("no-remote-cache", block)
            self.assertIn("no-remote-exec", block)

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
            self.assertIn(r'hasattr(tarfile, \"data_filter\")', text)
            self.assertIn(r'filter=\"data\"', text)
            self.assertIn("else archive.extractall(sys.argv[2])", text)
            self.assertNotIn("TMPDIR", text)
            self.assertIn("use_default_shell_env = False", text)
        self.assertIn("configure_directory", package)
        build = (PACKAGES / "BUILD.bazel").read_text(encoding="utf-8")
        self.assertIn('name = "true_configure_directory"', build)
        self.assertIn('configure_directory = ":true_configure_directory"', build)
        self.assertIn('glob(["true/**"])', build)

    def test_local_extraction_runs_without_data_filter(self) -> None:
        command = _starlark_extract_command((PACKAGES / "local.bzl").read_text(encoding="utf-8"))
        package_command = _starlark_extract_command((PACKAGES / "package.bzl").read_text(encoding="utf-8"))
        self.assertEqual(command, package_command)
        code = command.split("-c ", 1)[1]
        self.assertTrue(code.startswith("'") and code.endswith("'"))
        code = code[1:-1]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = root / "sources.tar"
            member = root / "getconf.c"
            member.write_text("int main(void){return 0;}\n", encoding="utf-8")
            with tarfile.open(archive, "w") as tar:
                tar.add(member, arcname="getconf.c")
            modern = root / "modern"
            modern.mkdir()
            subprocess.run([sys.executable, "-B", "-c", code, str(archive), str(modern)], check=True)
            self.assertEqual((modern / "getconf.c").read_text(encoding="utf-8"), member.read_text(encoding="utf-8"))
            legacy = root / "legacy"
            legacy.mkdir()
            fake = root / "fakepy"
            fake.mkdir()
            stdlib = Path(sysconfig.get_path("stdlib")) / "tarfile.py"
            (fake / "tarfile.py").write_text(
                textwrap.dedent(
                    f"""
                    import importlib.util
                    spec = importlib.util.spec_from_file_location("_real_tarfile", {str(stdlib)!r})
                    real = importlib.util.module_from_spec(spec)
                    spec.loader.exec_module(real)

                    class _Archive:
                        def __init__(self, inner):
                            self._inner = inner
                        def extractall(self, path, **kwargs):
                            if "filter" in kwargs:
                                raise TypeError("extractall() got an unexpected keyword argument 'filter'")
                            return self._inner.extractall(path)

                    def open(*args, **kwargs):
                        return _Archive(real.open(*args, **kwargs))
                    """
                ),
                encoding="utf-8",
            )
            env = dict(os.environ)
            env["PYTHONPATH"] = str(fake)
            subprocess.run(
                [sys.executable, "-B", "-c", code, str(archive), str(legacy)],
                check=True,
                env=env,
            )
            self.assertEqual((legacy / "getconf.c").read_text(encoding="utf-8"), member.read_text(encoding="utf-8"))

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
