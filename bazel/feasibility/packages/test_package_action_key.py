"""Linux rule text for guest package cache keys. No Bazel product build."""

from __future__ import annotations

import base64
import hashlib
import json
import os
import re
import shlex
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


def _declared_output_writes(text: str, variable: str) -> list[str]:
    suffix = f'> "${variable}"'
    return [line.strip() for line in text.splitlines() if line.strip().endswith(suffix)]


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
            self.assertIn("use_default_shell_env = True", text, name)
            self.assertNotIn("TMPDIR", text, name)
            self.assertNotIn("source-input.sha256", text, name)
            self.assertNotIn("DEVELOPER_DIR", text, name)
            self.assertIn('if [ -n "${CCACHE_DISABLE:-}" ] || [ ! -x "$tool_bin/ccache" ]; then', text, name)
            self.assertNotIn('export CCACHE_DIR="${HOME}/Library/Caches/Orlix/ccache"', text, name)
            self.assertIn("compiler_cache_env(", text, name)
            if name in ("coreutils.bzl", "bash.bzl"):
                self.assertIn("compiler_cache_env(shell, False)", text, name)
            else:
                self.assertIn("compiler_cache_env(ctx.configuration.default_shell_env, False)", text, name)
            self.assertIn('launcher="$tool_bin/ccache"', text, name)
            self.assertIn('export ORLIX_COMPILER_LAUNCHER="$launcher"', text, name)
            self.assertIn("compiler_cache_env(", text, name)
            self.assertNotIn("/opt/homebrew", text, name)
            self.assertIn("no-remote-exec", text, name)
        for name in ("local.bzl", "package.bzl"):
            text = (PACKAGES / name).read_text(encoding="utf-8")
            self.assertIn('tool_bin="$(/usr/bin/dirname "$gmake")"', text, name)
            self.assertIn('tool_root="$(/usr/bin/dirname "$tool_bin")"', text, name)
            self.assertIn('DYLD_LIBRARY_PATH="$tool_root/lib:$tool_root/llvm/lib"', text, name)
            self.assertNotIn('dirname "$tool_bin")/lib', text, name)
        autotools_text = (PACKAGES / "autotools.bzl").read_text(encoding="utf-8")
        self.assertNotIn("no-remote-cache", autotools_text)
        self.assertNotIn("DEVELOPER_DIR", autotools_text)
        self.assertIn('if [ -n "${CCACHE_DISABLE:-}" ] || [ ! -x "$tool_bin/ccache" ]; then', autotools_text)
        self.assertNotIn('export CCACHE_DIR="${HOME}/Library/Caches/Orlix/ccache"', autotools_text)
        self.assertIn("compiler_cache_env(shell, False)", autotools_text)
        self.assertIn("use_default_shell_env = True", autotools_text)
        self.assertIn('launcher="$tool_bin/ccache"', autotools_text)
        self.assertNotIn("xcrun", autotools_text)
        for name in ("coreutils.bzl", "bash.bzl", "autotools.bzl"):
            text = (PACKAGES / name).read_text(encoding="utf-8")
            self.assertIn("ORLIX_PACKAGE_TOOL_BIN", text, name)
            self.assertIn("tool_bin=sys.argv[5]", text, name)
            self.assertIn("/orlix/tools", text, name)
            self.assertIn("ffile-prefix-map", text, name)
            self.assertIn("$work", text, name)
            self.assertIn('clang="$tool_bin/clang"', text, name)
            self.assertIn('export DYLD_LIBRARY_PATH="$tool_root/lib:$tool_root/llvm/lib"', text, name)
            self.assertIn("ctx.files.tool_libs", text, name)
            self.assertIn("ctx.files.clang_resources", text, name)
            self.assertIn("ctx.files.guest_clang_support", text, name)
            self.assertIn("ctx.files.macos_sdk", text, name)
            self.assertNotIn("xcrun", text, name)
            self.assertNotIn("/opt/homebrew", text, name)
            self.assertNotIn("no-remote-cache", text, name)
        autotools = autotools_text
        self.assertIn('export PATH="$tool_bin:$work/toolchain:/usr/bin:/bin"', autotools)
        self.assertIn('export CC="${launcher:+$launcher }$clang --target=aarch64-linux-gnu', autotools)
        self.assertIn('share="$tool_root/share"', autotools)
        self.assertIn('autom4te_perllibdir="$share/autoconf"', autotools)
        self.assertIn('AC_MACRODIR="$share/autoconf"', autotools)
        self.assertIn('"/share/"', autotools)
        self.assertNotIn('export CC=aarch64-linux-gnu-gcc', autotools)
        self.assertNotIn("/opt/homebrew/bin", autotools)
        self.assertIn('export PATH="$tool_bin:/usr/bin:/bin"', (PACKAGES / "bash.bzl").read_text(encoding="utf-8"))
        self.assertIn('export PATH="$tool_bin:/usr/bin:/bin"', (PACKAGES / "coreutils.bzl").read_text(encoding="utf-8"))
        blocks = _execution_requirements(autotools)
        guest = [block for block in blocks if "no-sandbox" in block]
        interface = [block for block in blocks if "no-sandbox" not in block]
        self.assertTrue(guest)
        self.assertTrue(interface)
        for block in guest:
            self.assertNotIn("no-remote-cache", block)
            self.assertIn("no-remote-exec", block)
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
            self.assertIn("use_default_shell_env = True", text)
        self.assertIn("configure_directory", package)
        build = (PACKAGES / "BUILD.bazel").read_text(encoding="utf-8")
        self.assertIn('name = "true_configure_directory"', build)
        self.assertIn('configure_directory = ":true_configure_directory"', build)
        self.assertIn('glob(["true/**"])', build)

    def test_local_and_package_manifests_omit_execroot_and_mktemp(self) -> None:
        relative_manifest = '(cd "$install_out" && /usr/bin/find . -type f -print | /usr/bin/sort) > "$file_manifest"'
        for name in ("local.bzl", "package.bzl"):
            text = (PACKAGES / name).read_text(encoding="utf-8")
            manifest_writes = _declared_output_writes(text, "file_manifest")
            license_writes = _declared_output_writes(text, "license_manifest")
            metadata_writes = _declared_output_writes(text, "metadata")
            self.assertEqual(manifest_writes, [relative_manifest], name)
            self.assertEqual(len(license_writes), 1, name)
            self.assertEqual(len(metadata_writes), 1, name)
            for line in (*license_writes, *metadata_writes):
                self.assertNotIn("$install_out", line, name)
                self.assertNotIn("$exec_root", line, name)
                self.assertNotIn("$PWD", line, name)
                self.assertNotIn("$work", line, name)
                self.assertNotIn("/tmp/orlix-", line, name)
            mktemp_lines = [line.strip() for line in text.splitlines() if "/tmp/orlix-" in line]
            self.assertEqual(len(mktemp_lines), 1, name)
            self.assertIn("mktemp -d", mktemp_lines[0], name)
            for token in ("file_manifest", "license_manifest", "metadata", "install_out"):
                self.assertNotIn(token, mktemp_lines[0], name)
            with tempfile.TemporaryDirectory() as temporary, tempfile.TemporaryDirectory(prefix="orlix-", dir="/tmp") as mktemp_work:
                base = Path(temporary)
                execroot = base / "output-base" / "execroot" / "_main"
                install = execroot / "bazel-out" / "pkg" / "install"
                (install / "usr" / "bin").mkdir(parents=True)
                (install / "usr" / "share").mkdir()
                (install / "usr" / "bin" / "true").write_text("guest\n", encoding="utf-8")
                (install / "usr" / "share" / "doc.txt").write_text("doc\n", encoding="utf-8")
                (execroot / "outside.txt").write_text("outside\n", encoding="utf-8")
                work = Path(mktemp_work)
                self.assertTrue(str(work).startswith("/tmp/orlix-"), str(work))
                (work / "leaked.txt").write_text("mktemp\n", encoding="utf-8")
                manifest = base / "file-manifest.txt"
                license_manifest = base / "license-manifest.txt"
                metadata = base / "package-metadata.txt"
                script = "\n".join(
                    [
                        "set -euo pipefail",
                        f"exec_root={shlex.quote(str(execroot))}",
                        'PWD="$exec_root"',
                        'cd "$PWD"',
                        f"install_out={shlex.quote(str(install))}",
                        f"file_manifest={shlex.quote(str(manifest))}",
                        f"license_manifest={shlex.quote(str(license_manifest))}",
                        f"metadata={shlex.quote(str(metadata))}",
                        f"work={shlex.quote(str(work))}",
                        "package_name=true",
                        "package_version=0",
                        manifest_writes[0],
                        license_writes[0],
                        metadata_writes[0],
                    ]
                )
                subprocess.run(["/bin/bash", "-c", script], check=True)
                for output in (manifest, license_manifest, metadata):
                    body = output.read_text(encoding="utf-8")
                    self.assertNotIn(str(execroot), body, f"{name} {output.name}")
                    self.assertNotIn(str(base), body, f"{name} {output.name}")
                    self.assertNotIn(str(work), body, f"{name} {output.name}")
                    self.assertNotIn("/tmp/orlix-", body, f"{name} {output.name}")
                    self.assertNotIn("outside.txt", body, f"{name} {output.name}")
                    self.assertNotIn("leaked.txt", body, f"{name} {output.name}")
                self.assertEqual(manifest.read_text(encoding="utf-8"), "./usr/bin/true\n./usr/share/doc.txt\n")

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
