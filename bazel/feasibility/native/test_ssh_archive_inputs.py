#!/usr/bin/env python3
"""OpenSSL and libssh2 actions stay on their own sources and Apple slices."""

from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
RULE = Path(__file__).with_name("native_bundle.bzl")
BUILD = Path(__file__).with_name("BUILD.bazel")
SLICES = (
    ("ios-device", "arm64"),
    ("ios-simulator", "arm64"),
    ("macos", "arm64"),
)


def slice_pairs(rule_text: str) -> list[tuple[str, str]]:
    start = rule_text.index("_SLICES = [")
    end = rule_text.index("\n]", start)
    block = rule_text[start:end]
    destinations = re.findall(r'destination = "([^"]+)"', block)
    architectures = re.findall(r'architecture = "([^"]+)"', block)
    if len(destinations) != len(architectures):
        raise AssertionError("slice destination and architecture counts differ")
    return list(zip(destinations, architectures))


def function_body(rule_text: str, name: str) -> str:
    marker = f"def {name}("
    start = rule_text.index(marker)
    next_def = rule_text.find("\ndef ", start + len(marker))
    next_rule = rule_text.find("\norlix_ssh_archives", start + len(marker))
    ends = [pos for pos in (next_def, next_rule) if pos >= 0]
    if not ends:
        raise AssertionError(f"no end for {name}")
    return rule_text[start:min(ends)]


def archive_input_rejected(component: str, destination: str, architecture: str, path: str) -> bool:
    """Return whether this path must not be an input of the named archive action."""
    if component == "libssh2" and "orlix_openssl_source" in path:
        return True
    if component == "openssl" and (
        "orlix_libssh2_source" in path or "orlix_cmake_tool" in path
    ):
        return True
    owned = f"/{destination}/{architecture}"
    for other_destination, other_architecture in SLICES:
        marker = f"/{other_destination}/{other_architecture}"
        if marker != owned and marker in path:
            return True
    return False


class SshArchiveInputTests(unittest.TestCase):
    def test_libssh2_rejects_openssl_sources_and_other_slices(self) -> None:
        openssl_source = "external/+native_sources+orlix_openssl_source/Configure"
        libssh2_source = "external/+native_sources+orlix_libssh2_source/src/session.c"
        device_spec = "bazel-out/ssh_archives/ios-device/arm64.slice"
        device_ssl = "bazel-out/ssh_archives/ios-device/arm64/libssl.a"
        simulator_ssl = "bazel-out/ssh_archives/ios-simulator/arm64/libssl.a"
        simulator_crypto = "bazel-out/ssh_archives/ios-simulator/arm64/libcrypto.a"
        simulator_headers = "bazel-out/ssh_archives/ios-simulator/arm64/openssl-include"
        self.assertTrue(archive_input_rejected("libssh2", "ios-simulator", "arm64", openssl_source))
        self.assertTrue(archive_input_rejected("libssh2", "ios-simulator", "arm64", device_spec))
        self.assertTrue(archive_input_rejected("libssh2", "ios-simulator", "arm64", device_ssl))
        self.assertTrue(archive_input_rejected("openssl", "ios-simulator", "arm64", device_spec))
        self.assertTrue(archive_input_rejected("openssl", "ios-device", "arm64", simulator_ssl))
        self.assertTrue(archive_input_rejected("openssl", "ios-device", "arm64", libssh2_source))
        self.assertTrue(
            archive_input_rejected(
                "openssl",
                "macos",
                "arm64",
                "external/+native_sources+orlix_cmake_tool/CMake.app/Contents/bin/cmake",
            )
        )
        self.assertFalse(archive_input_rejected("libssh2", "ios-simulator", "arm64", simulator_ssl))
        self.assertFalse(archive_input_rejected("libssh2", "ios-simulator", "arm64", simulator_crypto))
        self.assertFalse(archive_input_rejected("libssh2", "ios-simulator", "arm64", simulator_headers))
        self.assertFalse(archive_input_rejected("libssh2", "ios-simulator", "arm64", libssh2_source))
        self.assertFalse(archive_input_rejected("openssl", "ios-simulator", "arm64", openssl_source))
        self.assertFalse(archive_input_rejected("openssl", "ios-device", "arm64", device_ssl))

    def test_rule_keeps_openssl_sources_out_of_libssh2(self) -> None:
        rule_text = RULE.read_text(encoding="utf-8")
        self.assertEqual(slice_pairs(rule_text), list(SLICES))
        openssl = function_body(rule_text, "_openssl_action")
        libssh2 = function_body(rule_text, "_libssh2_action")
        self.assertIn("ctx.attr.openssl_source[DefaultInfo].files", openssl)
        self.assertNotIn("xcrun -f clang", openssl)
        self.assertNotIn("xcrun -f clang", rule_text)
        self.assertIn("ctx.file.clang", openssl)
        self.assertIn("*/tools/bin/clang", openssl)
        cc_line = next(line for line in openssl.splitlines() if "export CC=" in line)
        self.assertIn('export CC="$clang ', cc_line)
        self.assertNotIn("xcrun", cc_line)
        self.assertNotIn("DEVELOPER_DIR", cc_line)
        self.assertNotIn("Toolchains", cc_line)
        for line in openssl.splitlines():
            if line.strip().startswith("clang="):
                self.assertNotIn("xcrun", line)
                self.assertNotIn("DEVELOPER_DIR", line)
        self.assertNotIn("libssh2_source", openssl)
        self.assertNotIn("libssh2_cmake", openssl)
        self.assertNotIn("cmake_runtime", openssl)
        self.assertNotIn("ctx.executable.cmake", openssl)
        self.assertIn("ctx.attr.libssh2_source[DefaultInfo].files", libssh2)
        self.assertNotIn("openssl_source", libssh2)
        self.assertNotIn("openssl_configure", libssh2)
        self.assertIn("ssl, crypto, headers", libssh2)
        impl = function_body(rule_text, "_ssh_impl")
        self.assertIn("for slice in _SLICES:", impl)
        self.assertIn("_openssl_action(ctx, slice, spec, ssl, crypto, headers)", impl)
        self.assertIn("_libssh2_action(ctx, slice, spec, ssl, crypto, headers, libssh2)", impl)
        self.assertNotIn("OrlixSshAppleArchives", rule_text)
        self.assertNotIn("herdr", rule_text.lower())
        self.assertNotIn("tssh", rule_text.lower())

    def test_link_uses_separate_archives_and_pinned_ghostty(self) -> None:
        build = BUILD.read_text(encoding="utf-8")
        self.assertIn('_SSH_DESTINATIONS = ["ios_device", "ios_simulator", "macos"]', build)
        self.assertIn('_SSH_LIBRARIES = ["libssh2", "libssl", "libcrypto"]', build)
        self.assertIn('name = "ssh_%s_%s" % (destination, library)', build)
        self.assertIn('output_group = "%s_%s" % (destination, library)', build)
        self.assertIn('name = "ssh_%s" % destination', build)
        self.assertIn('":ssh_%s_%s" % (destination, library)', build)
        self.assertIn('srcs = ["@orlix_ghostty_kit//:ios_device_lib"]', build)
        self.assertIn('srcs = ["@orlix_ghostty_kit//:ios_simulator_lib"]', build)
        self.assertIn('srcs = ["@orlix_ghostty_kit//:macos_lib"]', build)
        self.assertNotIn("herdr", build.lower())
        self.assertNotIn("tssh", build.lower())
        self.assertNotIn(".swift", build)

    def test_kernel_and_autotools_cache_exclusions_stay(self) -> None:
        kernel = (ROOT / "bazel/feasibility/kernel/kernel_macho.bzl").read_text(encoding="utf-8")
        autotools = (ROOT / "bazel/feasibility/packages/autotools.bzl").read_text(encoding="utf-8")
        coreutils = (ROOT / "bazel/feasibility/packages/coreutils.bzl").read_text(encoding="utf-8")
        bash = (ROOT / "bazel/feasibility/packages/bash.bzl").read_text(encoding="utf-8")
        for mnemonic in ("OrlixTctiIsaRestore", "OrlixKernelMachOArchive"):
            start = kernel.index(f'mnemonic = "{mnemonic}"')
            block = kernel[start:kernel.index("\n", kernel.index("execution_requirements = ", start))]
            self.assertNotIn("no-remote-cache", block, mnemonic)
            self.assertIn('"no-remote-exec": "1"', block, mnemonic)
        self.assertNotIn("no-remote-cache", autotools)
        self.assertNotIn("xcrun", autotools)
        self.assertNotIn("/opt/homebrew", autotools)
        self.assertIn('clang="$tool_bin/clang"', autotools)
        self.assertIn('autom4te_perllibdir="$share/autoconf"', autotools)
        for text in (kernel, autotools, coreutils, bash):
            self.assertIn('"no-sandbox": "1"', text)
        for text in (autotools, coreutils, bash, RULE.read_text(encoding="utf-8")):
            self.assertNotIn("/opt/homebrew/bin", text)


if __name__ == "__main__":
    unittest.main()
