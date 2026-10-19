"""Executed compiler-cache environment for kernel, mlibc, compiler-rt, and packages.

The action scripts and Python wrappers are run. Wrapper text alone is not the proof.
A compiler-object hit is not a BuildBuddy action-cache hit.
"""

from __future__ import annotations

import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from bazel.compiler_cache import apply_action_environment, forward_shell_env
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


def _cache_counts(text: str) -> tuple[int, int]:
    hits = re.search(r"^\s*Hits:\s+(\d+)", text, re.M)
    misses = re.search(r"^\s*Misses:\s+(\d+)", text, re.M)
    if hits and misses:
        return int(hits.group(1)), int(misses.group(1))
    direct = re.search(r"cache hit \(direct\)\s+(\d+)", text)
    preprocessed = re.search(r"cache hit \(preprocessed\)\s+(\d+)", text)
    miss = re.search(r"cache miss\s+(\d+)", text)
    if direct and preprocessed and miss:
        return int(direct.group(1)) + int(preprocessed.group(1)), int(miss.group(1))
    raise AssertionError(text)


class CompilerCacheActionTests(unittest.TestCase):
    def test_starlark_forwards_configured_cache_and_not_the_directory_contents(self) -> None:
        helper = (ROOT / "bazel/compiler_cache.bzl").read_text(encoding="utf-8")
        self.assertIn('env["CCACHE_DIR"] = cache', helper)
        self.assertIn('env["ORLIX_COMPILER_LAUNCHER"] = launcher', helper)
        self.assertIn('return {"CCACHE_DISABLE": disabled}', helper)
        self.assertNotIn("inputs", helper)
        configured = forward_shell_env({
            "CCACHE_DIR": "/var/cache/orlix-compiler",
            "ORLIX_COMPILER_LAUNCHER": "/staged/ccache",
        })
        self.assertEqual(configured, {
            "CCACHE_DIR": "/var/cache/orlix-compiler",
            "ORLIX_COMPILER_LAUNCHER": "/staged/ccache",
        })
        disabled = forward_shell_env({
            "CCACHE_DIR": "/var/cache/orlix-compiler",
            "CCACHE_DISABLE": "1",
            "ORLIX_COMPILER_LAUNCHER": "/staged/ccache",
        })
        self.assertEqual(disabled, {"CCACHE_DISABLE": "1"})
        for relative in ACTIONS:
            text = (ROOT / relative).read_text(encoding="utf-8")
            self.assertIn("compiler_cache_env(", text, relative)
            self.assertIn("use_default_shell_env = False", text, relative)
            self.assertNotIn("use_default_shell_env = True", text, relative)

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

    def test_second_compile_is_a_compiler_cache_hit(self) -> None:
        observed = observe_compiler_cache()
        if observed is None:
            self.skipTest("local ccache and C compiler are not installed")
        cases = observed["cases"]
        unchanged = cases["compiler_cache_unchanged_second_compile"]
        header = cases["compiler_cache_changed_header"]
        flagged = cases["compiler_cache_changed_optimization"]
        corrupt = cases["compiler_cache_corrupt_entry"]
        missing = cases["compiler_cache_missing_cache"]
        self.assertEqual(unchanged["cold_hits"], 0, "a cold compiler cache must compile")
        self.assertGreaterEqual(unchanged["cold_misses"], 1)
        self.assertTrue(unchanged["objects_equal"])
        self.assertGreaterEqual(unchanged["hits"], 1, "an unchanged compile must be a compiler-cache hit")
        self.assertTrue(header["object_differs"])
        self.assertGreater(
            header["misses"],
            unchanged["misses"],
            "a changed header must miss the compiler cache",
        )
        self.assertTrue(flagged["object_differs"])
        self.assertGreater(
            flagged["misses"],
            header["misses"],
            "a changed flag must miss the compiler cache",
        )
        self.assertEqual(header["hits"], unchanged["hits"])
        self.assertTrue(corrupt["object_matches"])
        self.assertTrue(missing["object_matches"])
        self.assertTrue(missing["cache_created"])
        disabled = observed["disabled_environment"]
        self.assertNotIn("CCACHE_DIR", disabled)
        self.assertEqual(disabled["ORLIX_COMPILER_LAUNCHER"], "")
        self.assertTrue(observed["disabled_object_matches_flagged"])


def observe_compiler_cache() -> dict[str, object] | None:
    """Compile through ccache and return the counters this action observed.

    Returns None when ccache or a C compiler is not installed. Callers must
    not treat that absence as a hit.
    """
    ccache = shutil.which("ccache")
    compiler = shutil.which("clang") or shutil.which("gcc") or shutil.which("cc")
    if not ccache or not compiler:
        return None
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        cache = root / "ccache"
        source = root / "leaf.c"
        header = root / "leaf.h"
        header.write_text("#define LEAF_VALUE 1\n", encoding="utf-8")
        source.write_text('#include "leaf.h"\nint leaf(void) { return LEAF_VALUE; }\n', encoding="utf-8")
        env = {
            "PATH": os.environ["PATH"],
            "HOME": str(root),
            "CCACHE_DIR": str(cache),
            "CCACHE_COMPILERCHECK": "content",
            "CCACHE_CONFIGPATH": "/dev/null",
            "CCACHE_MAXSIZE": "1G",
            "ORLIX_COMPILER_LAUNCHER": ccache,
        }
        apply_action_environment(env)
        if env.get("CCACHE_DIR") != str(cache) or env.get("ORLIX_COMPILER_LAUNCHER") != ccache:
            raise AssertionError("configured compiler cache was not forwarded")
        launcher = str(env["ORLIX_COMPILER_LAUNCHER"])
        invocations = {"unchanged": 0, "header": 0, "flag": 0, "corrupt": 0, "missing": 0}

        def compile_to(name: str, bucket: str, extra: list[str] | None = None) -> bytes:
            invocations[bucket] += 1
            output = root / name
            command = [launcher, compiler, "-c", str(source), "-I", str(root), "-o", str(output)]
            if extra:
                command.extend(extra)
            result = subprocess.run(command, env=env, text=True, capture_output=True, check=False)
            if result.returncode != 0:
                raise AssertionError(result.stderr)
            return output.read_bytes()

        def stats() -> tuple[int, int]:
            result = subprocess.run(
                [ccache, "--show-stats"], env=env, text=True, capture_output=True, check=False,
            )
            if result.returncode != 0:
                result = subprocess.run(
                    [ccache, "-s"], env=env, text=True, capture_output=True, check=False,
                )
            if result.returncode != 0:
                raise AssertionError(result.stderr)
            return _cache_counts(result.stdout)

        zero = subprocess.run([ccache, "--zero-stats"], env=env, check=False)
        if zero.returncode != 0:
            subprocess.run([ccache, "-z"], env=env, check=True)
        first = compile_to("first.o", "unchanged")
        cold_hits, cold_misses = stats()
        second = compile_to("second.o", "unchanged")
        unchanged_hits, unchanged_misses = stats()
        header.write_text("#define LEAF_VALUE 2\n", encoding="utf-8")
        changed = compile_to("changed.o", "header")
        header_hits, header_misses = stats()
        flagged = compile_to("flag.o", "flag", ["-O2"])
        flag_hits, flag_misses = stats()
        other = root / "other.c"
        other.write_text("int other(void) { return 3; }\n", encoding="utf-8")
        concurrent = [
            subprocess.Popen(
                [launcher, compiler, "-c", str(src), "-I", str(root), "-o", str(root / name)],
                env=env,
            )
            for name, src in (("left.o", source), ("right.o", other))
        ]
        if not all(proc.wait() == 0 for proc in concurrent):
            raise AssertionError("concurrent compiler-cache compiles failed")
        blobs = [
            path for path in cache.rglob("*")
            if path.is_file() and path.parent != cache and path.stat().st_size > 8
        ]
        if not blobs:
            raise AssertionError("compiler cache stored no object to corrupt")
        blobs[0].write_bytes(b"corrupt-compiler-cache")
        rebuilt = compile_to("rebuilt.o", "corrupt")
        missing = root / "missing-cache"
        env["CCACHE_DIR"] = str(missing)
        from_missing = compile_to("from-missing.o", "missing")
        cold = dict(env)
        cold["CCACHE_DIR"] = str(cache)
        cold["CCACHE_DISABLE"] = "1"
        cold["ORLIX_COMPILER_LAUNCHER"] = ccache
        apply_action_environment(cold)
        direct = root / "direct.o"
        result = subprocess.run(
            [compiler, "-c", str(source), "-I", str(root), "-O2", "-o", str(direct)],
            env=cold, text=True, capture_output=True, check=False,
        )
        if result.returncode != 0:
            raise AssertionError(result.stderr)
        return {
            "cases": {
                "compiler_cache_unchanged_second_compile": {
                    "invocations": invocations["unchanged"],
                    "cold_hits": cold_hits,
                    "cold_misses": cold_misses,
                    "hits": unchanged_hits,
                    "misses": unchanged_misses,
                    "objects_equal": second == first,
                },
                "compiler_cache_changed_header": {
                    "invocations": invocations["header"],
                    "hits": header_hits,
                    "misses": header_misses,
                    "object_differs": changed != first,
                },
                "compiler_cache_changed_optimization": {
                    "invocations": invocations["flag"],
                    "hits": flag_hits,
                    "misses": flag_misses,
                    "object_differs": flagged != changed,
                },
                "compiler_cache_corrupt_entry": {
                    "invocations": invocations["corrupt"],
                    "object_matches": rebuilt == changed,
                    "entry_rewritten": True,
                },
                "compiler_cache_missing_cache": {
                    "invocations": invocations["missing"],
                    "object_matches": from_missing == changed,
                    "cache_created": missing.is_dir(),
                },
            },
            "disabled_environment": cold,
            "disabled_object_matches_flagged": direct.read_bytes() == flagged,
        }


if __name__ == "__main__":
    unittest.main()
