"""Executed compiler-cache environment for kernel, mlibc, compiler-rt, and packages.

The action scripts and Python wrappers are run. Wrapper text alone is not the proof.
A compiler-object hit is not a BuildBuddy action-cache hit.
"""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from bazel.compiler_cache import (
    COMPILER_OBJECT_UNIT,
    apply_action_environment,
    compiler_environment,
    compiler_for_proof,
    ccache_binary,
    compile_object,
    forward_shell_env,
    object_differs,
    parse_ccache_statistics,
    prove_wiped_foreign_output,
    read_ccache_statistics,
    unread_statistics,
)
from bazel.feasibility.mlibc import build_state as mlibc_state
from bazel.feasibility.packages import build_state as package_state


ROOT = Path(__file__).resolve().parents[2]
ACTIONS = (
    "bazel/feasibility/kernel/kernel_macho.bzl",
    "bazel/feasibility/mlibc/mlibc_sysroot.bzl",
    "bazel/feasibility/mlibc/compiler_runtime.bzl",
    "bazel/feasibility/packages/coreutils.bzl",
    "bazel/feasibility/packages/bash.bzl",
    "bazel/feasibility/packages/autotools.bzl",
    "bazel/feasibility/packages/local.bzl",
    "bazel/feasibility/packages/package.bzl",
)
INHERITED_CACHE = {
    "bazel/feasibility/packages/coreutils.bzl",
    "bazel/feasibility/packages/bash.bzl",
    "bazel/feasibility/packages/autotools.bzl",
    "bazel/feasibility/packages/local.bzl",
    "bazel/feasibility/packages/package.bzl",
}


def _snippet(text: str) -> str:
    start = text.index('if [ -n "${CCACHE_DISABLE:-}" ]')
    exported = text.find('export ORLIX_COMPILER_LAUNCHER="$launcher"', start)
    if exported != -1 and exported - start < 800:
        return text[start:text.index("\n", exported) + 1]
    return text[start:text.index("\nfi\n", start) + 4]


def _bash(script: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(["bash"], input=script, text=True, capture_output=True, check=False)


def _package_arguments(base: Path) -> list[str]:
    source = base / "source"
    source.mkdir()
    (source / "configure").write_bytes(b"configure\n")
    (source / "Makefile").write_bytes(b"all:\n")
    (source / "leaf.c").write_bytes(b"int value = 1;\n")
    arguments = [str(source / "configure")]
    for name in ("headers", "uapi", "libraries"):
        tree = base / name
        tree.mkdir()
        (tree / "input").write_bytes(b"dependency\n")
        arguments.append(str(tree))
    runtime = base / "runtime.a"
    runtime.write_bytes(b"runtime\n")
    arguments.append(str(runtime))
    return arguments


def _input_blocks(text: str) -> str:
    blocks = []
    start = 0
    while True:
        found = text.find("inputs =", start)
        if found < 0:
            break
        outputs = text.find("outputs", found)
        blocks.append(text[found:outputs if outputs >= 0 else found + 800])
        start = found + len("inputs =")
    return "\n".join(blocks)


class CompilerCacheActionTests(unittest.TestCase):
    def test_starlark_forwards_configured_cache_and_not_the_directory_contents(self) -> None:
        helper = (ROOT / "bazel/compiler_cache.bzl").read_text(encoding="utf-8")
        self.assertIn('env["CCACHE_DIR"] = cache', helper)
        self.assertIn('env["ORLIX_COMPILER_LAUNCHER"] = launcher', helper)
        self.assertIn('return {"CCACHE_DISABLE": disabled}', helper)
        self.assertNotIn("inputs", helper)
        self.assertNotIn("compiler-cache.log", helper)
        self.assertNotIn("stats_updated_timestamp", helper)
        self.assertNotIn("stats_zeroed_timestamp", helper)
        configured = forward_shell_env({
            "CCACHE_DIR": "/var/cache/orlix-compiler",
            "ORLIX_COMPILER_LAUNCHER": "/staged/ccache",
        })
        self.assertEqual(configured, {
            "CCACHE_DIR": "/var/cache/orlix-compiler",
            "ORLIX_COMPILER_LAUNCHER": "/staged/ccache",
        })
        inherited = forward_shell_env({
            "CCACHE_DIR": "/var/cache/orlix-compiler",
            "ORLIX_COMPILER_LAUNCHER": "/staged/ccache",
        }, include_directory=False)
        self.assertEqual(inherited, {"ORLIX_COMPILER_LAUNCHER": "/staged/ccache"})
        disabled = forward_shell_env({
            "CCACHE_DIR": "/var/cache/orlix-compiler",
            "CCACHE_DISABLE": "1",
            "ORLIX_COMPILER_LAUNCHER": "/staged/ccache",
        })
        self.assertEqual(disabled, {"CCACHE_DISABLE": "1"})
        for relative in ACTIONS:
            text = (ROOT / relative).read_text(encoding="utf-8")
            self.assertIn("compiler_cache_env(", text, relative)
            if relative in INHERITED_CACHE:
                self.assertIn("use_default_shell_env = True", text, relative)
                self.assertIn("compiler_cache_env(", text, relative)
                self.assertNotIn('export CCACHE_DIR="${HOME}/Library/Caches/Orlix/ccache"', text, relative)
            else:
                self.assertIn("use_default_shell_env = False", text, relative)
                self.assertNotIn("use_default_shell_env = True", text, relative)
            inputs = _input_blocks(text)
            self.assertNotIn("CCACHE_DIR", inputs, relative)
            self.assertNotIn("compiler-cache.log", inputs, relative)
            self.assertNotIn("stats_updated_timestamp", inputs, relative)
            self.assertNotIn("stats_zeroed_timestamp", inputs, relative)

    def test_action_scripts_keep_directory_and_launcher(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            tool = Path(temporary) / "bin"
            tool.mkdir()
            ccache = tool / "ccache"
            ccache.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
            ccache.chmod(0o755)
            cache = "/var/cache/orlix-compiler-configured"
            for relative in ACTIONS:
                snippet = _snippet((ROOT / relative).read_text(encoding="utf-8"))
                configured = f"""
set -euo pipefail
unset CCACHE_DISABLE || true
unset ORLIX_COMPILER_LAUNCHER || true
export HOME=/var/empty
export CCACHE_DIR={cache}
tool_bin={tool}
ccache_bin={ccache}
ccache_staged={ccache}
mkdir_bin=/bin/mkdir
{snippet}
test "$CCACHE_DIR" = {cache}
test -n "$ORLIX_COMPILER_LAUNCHER"
case "$ORLIX_COMPILER_LAUNCHER" in
  "{ccache}") ;;
  *) printf '%s\\n' "launcher dropped in {relative}: $ORLIX_COMPILER_LAUNCHER" >&2; exit 1 ;;
esac
"""
                result = _bash(configured)
                self.assertEqual(result.returncode, 0, f"{relative}\n{result.stderr}")
                disabled = f"""
set -euo pipefail
export CCACHE_DISABLE=1
export CCACHE_DIR={cache}
export ORLIX_COMPILER_LAUNCHER=/usr/bin/false
export HOME=/var/empty
tool_bin={tool}
ccache_bin={ccache}
ccache_staged={ccache}
mkdir_bin=/bin/mkdir
{snippet}
test -z "$ORLIX_COMPILER_LAUNCHER"
"""
                result = _bash(disabled)
                self.assertEqual(result.returncode, 0, f"{relative} disabled\n{result.stderr}")

    def test_meson_and_compiler_rt_prefix_the_real_launcher(self) -> None:
        mlibc = (ROOT / "bazel/feasibility/mlibc/mlibc_sysroot.bzl").read_text(encoding="utf-8")
        prefix = 'compiler_prefix=""\nif [ -n "$launcher" ]; then compiler_prefix="\'$launcher\', "; fi\n'
        self.assertIn(prefix, mlibc)
        runtime = (ROOT / "bazel/feasibility/mlibc/compiler_runtime.bzl").read_text(encoding="utf-8")
        start = runtime.index('compile=("$clang")')
        compile = runtime[start:runtime.index("\n", runtime.index("fi", start)) + 1]
        script = f"""
set -euo pipefail
launcher=/staged/ccache
clang=/staged/clang
{prefix}
test "$compiler_prefix" = "'/staged/ccache', "
{compile}
test "${{compile[0]}}" = /staged/ccache
test "${{compile[1]}}" = /staged/clang
"""
        result = _bash(script)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_mlibc_and_package_wrappers_execute_with_the_cache(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            self._assert_mlibc_env(base, configured=True)
            self._assert_mlibc_env(base / "disabled", configured=False)
            self._assert_package_env(base / "package", configured=True)
            self._assert_package_env(base / "package-disabled", configured=False)

    def _assert_mlibc_env(self, base: Path, *, configured: bool) -> None:
        base.mkdir(exist_ok=True)
        script = base / "build.sh"
        if configured:
            script.write_text(
                "set -euo pipefail\n"
                'test "$CCACHE_DIR" = "/var/folders/cache"\n'
                'test "$ORLIX_COMPILER_LAUNCHER" = "/opt/homebrew/bin/ccache"\n'
                'test -z "${DEVELOPER_DIR:-}"\n'
                'test -z "${SDKROOT:-}"\n'
                'test -z "${TMPDIR:-}"\n',
                encoding="utf-8",
            )
        else:
            script.write_text(
                "set -euo pipefail\n"
                'test -z "${CCACHE_DIR:-}"\n'
                'test -z "$ORLIX_COMPILER_LAUNCHER"\n'
                'test "$CCACHE_DISABLE" = 1\n',
                encoding="utf-8",
            )
        (base / "toolchain.json").write_text("{}\n", encoding="utf-8")
        (base / "compiler.json").write_text("{}\n", encoding="utf-8")
        tools = base / "tools" / "bin"
        tools.mkdir(parents=True)
        sdk = base / "sdk"
        sdk.mkdir()
        (sdk / "SDKSettings.json").write_text("{}\n", encoding="utf-8")
        environment = {
            "CCACHE_DIR": "/var/folders/cache",
            "ORLIX_COMPILER_LAUNCHER": "/opt/homebrew/bin/ccache",
            "DEVELOPER_DIR": "/Applications/Xcode.app/Contents/Developer",
            "SDKROOT": "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk",
            "TMPDIR": "/tmp/client",
        }
        if configured:
            environment["CCACHE_DISABLE"] = ""
        else:
            environment["CCACHE_DISABLE"] = "1"
        with patch.dict(os.environ, environment, clear=False):
            status = mlibc_state.run(
                str(script), str(base / "toolchain.json"), str(base / "compiler.json"),
                str(base / "mlibc-work"), str(tools), str(sdk), [],
            )
        self.assertEqual(status, 0)

    def _assert_package_env(self, base: Path, *, configured: bool) -> None:
        base.mkdir()
        arguments = _package_arguments(base)
        tool = base / "tools" / "bin" / "gmake"
        tool.parent.mkdir(parents=True)
        tool.write_bytes(b"gmake")
        script = base / "build.sh"
        if configured:
            script.write_text(
                "set -euo pipefail\n"
                'test "$CCACHE_DIR" = "/var/folders/cache"\n'
                'test "$ORLIX_COMPILER_LAUNCHER" = "/opt/homebrew/bin/ccache"\n'
                'test -z "${DEVELOPER_DIR:-}"\n',
                encoding="utf-8",
            )
        else:
            script.write_text(
                "set -euo pipefail\n"
                'test -z "${CCACHE_DIR:-}"\n'
                'test -z "$ORLIX_COMPILER_LAUNCHER"\n'
                'test "$CCACHE_DISABLE" = 1\n',
                encoding="utf-8",
            )
        environment = {
            "CCACHE_DIR": "/var/folders/cache",
            "ORLIX_COMPILER_LAUNCHER": "/opt/homebrew/bin/ccache",
            "DEVELOPER_DIR": "/Applications/Xcode.app/Contents/Developer",
            "ORLIX_PACKAGE_INCREMENTAL": "1",
        }
        environment["CCACHE_DISABLE"] = "" if configured else "1"
        seen = {}

        def build(*_args, **kwargs):
            seen["env"] = kwargs["env"]
            return 0

        with patch.object(Path, "cwd", return_value=base / "execroot/_main"), \
             patch.dict(os.environ, environment, clear=False), \
             patch.object(package_state.subprocess, "call", side_effect=build):
            status = package_state.run(
                "grep", str(script), str(tool), str(tool), arguments, tool_bin=str(tool),
            )
        self.assertEqual(status, 0)
        env = seen["env"]
        if configured:
            self.assertEqual(env["CCACHE_DIR"], "/var/folders/cache")
            self.assertEqual(env["ORLIX_COMPILER_LAUNCHER"], "/opt/homebrew/bin/ccache")
        else:
            self.assertNotIn("CCACHE_DIR", env)
            self.assertEqual(env["ORLIX_COMPILER_LAUNCHER"], "")
        self.assertNotIn("DEVELOPER_DIR", env)

    def test_object_differs_rejects_equal_bytes(self) -> None:
        blob = b"\x7fELF-same-object"
        self.assertFalse(object_differs(blob, blob))
        self.assertFalse(object_differs(b"", b"abc"))
        self.assertFalse(object_differs(b"abc", b""))
        self.assertTrue(object_differs(b"aaa", b"bbb"))
        self.assertNotIn("return LEAF_VALUE", COMPILER_OBJECT_UNIT)

    def test_statistics_come_from_ccache_counters(self) -> None:
        printed = """\
direct_cache_hit\t1
preprocessed_cache_hit\t2
cache_miss\t3
called_for_link\t4
cache_size_kibibyte\t16
stats_updated_timestamp\t1792443436
stats_zeroed_timestamp\t1792443436
"""
        stats = parse_ccache_statistics(printed)
        self.assertEqual(stats["direct_hits"], 1)
        self.assertEqual(stats["preprocessed_hits"], 2)
        self.assertEqual(stats["misses"], 3)
        self.assertEqual(stats["called_for_link"], 4)
        self.assertEqual(stats["cache_size_kibibytes"], 16)
        self.assertEqual(stats["ccache_hits"], 3)
        self.assertNotIn("stats_updated_timestamp", stats)
        self.assertNotIn("stats_zeroed_timestamp", stats)
        human = """\
cache hit (direct)                     4
cache hit (preprocessed)               1
cache miss                             2
called for link                        0
cache size                             8.0 kB
"""
        legacy = parse_ccache_statistics(human)
        self.assertEqual(legacy["ccache_hits"], 5)
        self.assertEqual(legacy["cache_size_kibibytes"], 8)
        with self.assertRaises(ValueError):
            parse_ccache_statistics("total 8\n-rw-r--r-- ccache\n")
        empty = Path("/tmp")
        self.assertTrue(empty.is_dir())
        unread = unread_statistics()
        self.assertIsNone(unread["ccache_hits"])
        self.assertFalse(unread["statistics_read"])

    def test_wiped_foreign_output_is_a_ccache_hit(self) -> None:
        ccache = ccache_binary()
        if sys.platform == "darwin":
            self.assertTrue(
                Path("/opt/homebrew/opt/llvm/bin/clang").is_file(),
                "pinned clang stays /opt/homebrew/opt/llvm/bin/clang",
            )
        compiler = compiler_for_proof()
        if not ccache or not compiler:
            self.skipTest("ccache and a C compiler are required")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            cache = root / "ccache"
            proof = prove_wiped_foreign_output(
                compiler=compiler, ccache=ccache, cache_dir=cache, work=root,
            )
            stats = proof["ccache"]
            self.assertGreaterEqual(stats["ccache_hits"], 1)
            self.assertGreaterEqual(stats["direct_hits"] + stats["preprocessed_hits"], 1)
            self.assertGreaterEqual(stats["misses"], 2)
            self.assertEqual(stats["called_for_link"], 0)
            self.assertGreaterEqual(stats["cache_size_kibibytes"], 1)
            self.assertTrue(proof["object_bytes_match"])
            self.assertTrue(proof["object_differs"])
            self.assertTrue(proof["same_runner"])
            self.assertFalse(proof["fresh_runner"])
            self.assertIsNone(proof["bazel_disk_cache_hits"])
            self.assertIsNone(proof["buildbuddy_remote_cache_hits"])
            print(
                "compiler-cache-proof "
                f"direct_hits={stats['direct_hits']} "
                f"preprocessed_hits={stats['preprocessed_hits']} "
                f"misses={stats['misses']} "
                f"called_for_link={stats['called_for_link']} "
                f"cache_size_kibibytes={stats['cache_size_kibibytes']} "
                f"ccache_hits={stats['ccache_hits']} "
                "bazel_disk_cache_hits=not_this_proof "
                "buildbuddy_remote_cache_hits=not_this_proof "
                "object_bytes_match=true object_differs=true "
                "same_runner=true fresh_runner=false"
            )
            env = compiler_environment(cache, root)
            header = root / "leaf.h"
            header.write_text("static int mix(int n) { return n ^ 17; }\n", encoding="utf-8")
            source = root / "leaf.c"
            source.write_text('#include "leaf.h"\nint leaf(int n) { return mix(n) + mix(n ^ 15); }\n', encoding="utf-8")
            before = read_ccache_statistics(ccache, env)
            changed = compile_object(
                compiler=compiler, ccache=ccache, source=source, output=root / "changed.o",
                environment=env, flags=["-I", str(root), "-O2"],
            )
            self.assertTrue(object_differs(changed, (root / "restored.o").read_bytes()))
            after_header = read_ccache_statistics(ccache, env)
            self.assertGreater(after_header["misses"], before["misses"])
            blobs = [
                path for path in cache.rglob("*")
                if path.is_file() and path.parent != cache and path.stat().st_size > 8
            ]
            self.assertTrue(blobs, "compiler cache stored no object to corrupt")
            blobs[0].write_bytes(b"corrupt-compiler-cache")
            rebuilt = compile_object(
                compiler=compiler, ccache=ccache, source=source, output=root / "rebuilt.o",
                environment=env, flags=["-I", str(root), "-O2"],
            )
            self.assertEqual(rebuilt, changed)
            missing = root / "missing-cache"
            env["CCACHE_DIR"] = str(missing)
            from_missing = compile_object(
                compiler=compiler, ccache=ccache, source=source, output=root / "from-missing.o",
                environment=env, flags=["-I", str(root), "-O2"],
            )
            self.assertEqual(from_missing, changed)
            self.assertTrue(missing.is_dir())
            cold = dict(env)
            cold["CCACHE_DIR"] = str(cache)
            cold["CCACHE_DISABLE"] = "1"
            cold["ORLIX_COMPILER_LAUNCHER"] = ccache
            apply_action_environment(cold)
            self.assertNotIn("CCACHE_DIR", cold)
            self.assertEqual(cold["ORLIX_COMPILER_LAUNCHER"], "")
            direct = root / "direct.o"
            result = subprocess.run(
                [compiler, "-c", str(source), "-I", str(root), "-O2", "-o", str(direct)],
                env=cold, text=True, capture_output=True, check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(direct.read_bytes(), changed)


if __name__ == "__main__":
    unittest.main()
