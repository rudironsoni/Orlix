"""Guest configure inputs stay on one package. Rootfs policy stays on assembly."""

from __future__ import annotations

import hashlib
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
PACKAGES = ROOT / "bazel/feasibility/packages"
ROOTFS = ROOT / "bazel/feasibility/rootfs"
MARKERS = {
    "acl": "orlix_acl_source/",
    "attr": "orlix_attr_source/",
    "bash": "orlix_bash_source/",
    "coreutils": "orlix_coreutils_source/",
    "curl": "orlix_curl_source/",
    "e2fsprogs": "orlix_e2fsprogs_source/",
    "findutils": "orlix_findutils_source/",
    "grep": "orlix_grep_source/",
    "jq": "orlix_jq_source/",
    "libcap": "orlix_libcap_source/",
    "libselinux": "orlix_libselinux_source/",
    "libsepol": "orlix_libsepol_source/",
    "musl-fts": "orlix_musl_fts_source/",
    "ncurses": "orlix_ncurses_source/",
    "pcre2": "orlix_pcre2_source/",
    "zsh": "orlix_zsh_source/",
}


def configure_accepts(package: str, path: str) -> bool:
    if path.endswith("assembly_policy.txt"):
        return False
    for other, marker in MARKERS.items():
        if other != package and marker in path:
            return False
    return True


class ConfigureInputRejected(Exception):
    """The guest configure action refuses this path instead of keying it."""


def rule_markers() -> dict[str, str]:
    """Read the production package markers. A copied name list is not the rule."""
    text = (PACKAGES / "action_inputs.bzl").read_text(encoding="utf-8")
    start = text.index("_PACKAGE_SOURCE_MARKERS = {")
    body = text[start:text.index("}", start)]
    parsed = dict(re.findall(r'"([^"]+)": "([^"]+)"', body))
    if parsed != MARKERS:
        raise AssertionError((sorted(parsed), sorted(MARKERS)))
    return parsed


def fail_closed_configure_inputs(package_name: str, paths: list[str]) -> list[str]:
    """Apply `guest_configure_inputs`: the first foreign path fails the action."""
    markers = rule_markers()
    if package_name not in markers:
        raise ConfigureInputRejected(f"unknown guest package configure owner: {package_name}")
    selected: list[str] = []
    for path in paths:
        if path.endswith("assembly_policy.txt"):
            raise ConfigureInputRejected(
                f"rootfs policy is not a {package_name} configure input: {path}"
            )
        for other, marker in markers.items():
            if other != package_name and marker in path:
                raise ConfigureInputRejected(
                    f"foreign package source is not a {package_name} configure input: {path}"
                )
        selected.append(path)
    return selected


def observe_unrelated_package_action_key() -> dict[str, object]:
    """Key one package's accepted inputs and show a foreign source stays out.

    The stamp is the declared-byte action key from `source_key.py`. A path
    list without that invocation is not a result.
    """
    package = "bash"
    owned_name = "external/+native_sources+orlix_bash_source/shell.c"
    foreign_name = "external/+native_sources+orlix_grep_source/src/grep.c"
    policy_name = "bazel/feasibility/rootfs/assembly_policy.txt"
    candidates = [owned_name, foreign_name, policy_name]
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        owned = root / owned_name
        foreign = root / foreign_name
        policy = root / policy_name
        owned.parent.mkdir(parents=True)
        foreign.parent.mkdir(parents=True)
        policy.parent.mkdir(parents=True)
        owned_bytes = b"int shell(void) { return 1; }\n"
        foreign_bytes = b"int grep_main(void) { return 2; }\n"
        owned.write_bytes(owned_bytes)
        foreign.write_bytes(foreign_bytes)
        policy.write_bytes(b"copy_bin usr/bin/bash bin/bash\n")
        selected: list[str] = []
        rejected: list[str] = []
        selector_calls = 0
        for path in candidates:
            selector_calls += 1
            try:
                fail_closed_configure_inputs(package, [path])
            except ConfigureInputRejected:
                rejected.append(path)
            else:
                selected.append(path)
            if configure_accepts(package, path) != (path in selected):
                raise AssertionError(f"configure filter drifted for {path}")
        selector_calls += 1
        try:
            fail_closed_configure_inputs(package, candidates)
        except ConfigureInputRejected as error:
            fail_closed = foreign_name in str(error)
        else:
            fail_closed = False
        invocations = 0

        def stamp_for(names: list[str]) -> str:
            nonlocal invocations
            invocations += 1
            archive = root / f"sources-{invocations}.tar"
            stamp = root / f"sources-{invocations}.stamp"
            command = [
                sys.executable,
                "-B",
                str(PACKAGES / "source_key.py"),
                "files",
                str(archive),
                str(stamp),
            ]
            for name in names:
                command.extend([name, str(root / name)])
            completed = subprocess.run(command, check=False, capture_output=True, text=True)
            if completed.returncode != 0:
                raise AssertionError(completed.stderr)
            return stamp.read_text(encoding="utf-8")

        action_stamp = stamp_for(selected)
        action_digest = hashlib.sha256(action_stamp.encode()).hexdigest()
        observed_paths = [line.split("  ", 1)[1] for line in action_stamp.splitlines() if line]
        foreign_digest = hashlib.sha256(foreign_bytes).hexdigest()
        owned_digest = hashlib.sha256(owned_bytes).hexdigest()
        foreign.write_bytes(b"int grep_main(void) { return 9; }\n")
        after_foreign = stamp_for(selected)
        mixed = stamp_for(sorted([*selected, foreign_name]))
        foreign.write_bytes(b"int grep_main(void) { return 10; }\n")
        mixed_after = stamp_for(sorted([*selected, foreign_name]))
        owned.write_bytes(b"int shell(void) { return 4; }\n")
        after_owned = stamp_for(selected)
        return {
            "source_key_invocations": invocations,
            "selector_calls": selector_calls,
            "expected_paths": ",".join(sorted(selected)),
            "observed_paths": ",".join(observed_paths),
            "rejected_paths": ",".join(rejected),
            "fail_closed": fail_closed,
            "foreign_digest_in_stamp": foreign_digest in action_stamp,
            "owned_digest_in_stamp": owned_digest in action_stamp,
            "foreign_edit_changes_action_key": after_foreign != action_stamp,
            "owned_edit_changes_action_key": after_owned != action_stamp,
            "included_foreign_changes_key": mixed != action_stamp and mixed_after != mixed,
            "stamp_sha256": action_digest,
        }


def _package_blocks(text: str) -> list[str]:
    parts = re.split(r"\n(?=orlix_(?:autotools|bash|coreutils)_package\()", text)
    return [part for part in parts if part.startswith("orlix_")]


class PackageRootfsInputTests(unittest.TestCase):
    def test_foreign_source_and_rootfs_policy_are_not_configure_inputs(self) -> None:
        grep_source = "external/+native_sources+orlix_grep_source/src/grep.c"
        bash_source = "external/+native_sources+orlix_bash_source/shell.c"
        policy = "bazel/feasibility/rootfs/assembly_policy.txt"
        self.assertTrue(configure_accepts("bash", bash_source))
        self.assertFalse(configure_accepts("bash", grep_source))
        self.assertFalse(configure_accepts("grep", bash_source))
        self.assertFalse(configure_accepts("coreutils", "external/+native_sources+orlix_attr_source/libattr/attr.c"))
        self.assertTrue(configure_accepts("coreutils", "external/+native_sources+orlix_coreutils_source/src/ls.c"))
        for package in MARKERS:
            self.assertFalse(configure_accepts(package, policy), package)
        rule = (PACKAGES / "action_inputs.bzl").read_text(encoding="utf-8")
        for marker in MARKERS.values():
            self.assertIn(f'"{marker}"', rule)
        self.assertIn('path.endswith("assembly_policy.txt")', rule)
        self.assertIn("foreign package source is not", rule)
        self.assertIn("rootfs policy is not", rule)
        for name in ("autotools.bzl", "bash.bzl", "coreutils.bzl"):
            text = (PACKAGES / name).read_text(encoding="utf-8")
            self.assertIn("guest_configure_inputs(ctx.attr.package_name,", text, name)
            self.assertNotIn("assembly_policy", text, name)

    def test_unrelated_package_input_is_outside_the_action_key(self) -> None:
        observed = observe_unrelated_package_action_key()
        self.assertGreaterEqual(observed["source_key_invocations"], 3)
        self.assertGreaterEqual(observed["selector_calls"], 1)
        self.assertTrue(observed["fail_closed"])
        self.assertEqual(observed["observed_paths"], observed["expected_paths"])
        self.assertEqual(
            observed["expected_paths"],
            "external/+native_sources+orlix_bash_source/shell.c",
        )
        self.assertNotIn("orlix_grep_source/", str(observed["observed_paths"]))
        self.assertNotIn("assembly_policy.txt", str(observed["observed_paths"]))
        self.assertIn("orlix_grep_source/", str(observed["rejected_paths"]))
        self.assertFalse(observed["foreign_digest_in_stamp"])
        self.assertTrue(observed["owned_digest_in_stamp"])
        self.assertFalse(observed["foreign_edit_changes_action_key"])
        self.assertTrue(observed["owned_edit_changes_action_key"])
        self.assertTrue(observed["included_foreign_changes_key"])
        self.assertEqual(len(str(observed["stamp_sha256"])), 64)

    def test_each_guest_package_has_its_own_source_repo(self) -> None:
        blocks = _package_blocks((PACKAGES / "BUILD.bazel").read_text(encoding="utf-8"))
        sources: dict[str, str] = {}
        for block in blocks:
            name = re.search(r'\n    name = "([^"]+)"', block)
            label = re.search(r'\n    sources = "([^"]+)"', block)
            self.assertIsNotNone(name, block[:80])
            self.assertIsNotNone(label, name.group(1) if name else block[:80])
            assert name is not None and label is not None
            sources[name.group(1)] = label.group(1)
        self.assertEqual(set(sources), set(MARKERS))
        self.assertEqual(len(set(sources.values())), len(sources))
        for package, label in sources.items():
            self.assertEqual(label, f"@orlix_{package.replace('-', '_')}_source//:source")

    def test_rootfs_assembly_reads_policy_and_package_outputs(self) -> None:
        rule = (ROOTFS / "rootfs.bzl").read_text(encoding="utf-8")
        assembly = rule.split("def _rootfs_impl", 1)[1].split("def _rootfs_payload_impl", 1)[0]
        self.assertIn("pkg_inputs = [pkg.install_tree for pkg in pkgs]", assembly)
        self.assertIn("direct = pkg_inputs +", assembly)
        self.assertIn("ctx.file.assembly_policy", assembly)
        self.assertNotIn("ctx.files.sources", assembly)
        self.assertNotIn("orlix_grep_source", assembly)
        self.assertNotIn("orlix_bash_source", assembly)
        self.assertNotIn("orlix_coreutils_source", assembly)
        self.assertIn('"no-sandbox": "1"', assembly)
        self.assertNotIn("/opt/homebrew/bin", rule)
        build = (ROOTFS / "BUILD.bazel").read_text(encoding="utf-8")
        packages = build.split("packages = [", 1)[1].split("]", 1)[0]
        self.assertNotIn("_source", packages)
        self.assertIn("exports_files([\"assembly_policy.txt\"])", build)
        policy = (ROOTFS / "assembly_policy.txt").read_text(encoding="utf-8").splitlines()
        for line in (
            "copy_bin usr/bin/grep bin/grep",
            "copy_bin usr/bin/bash bin/bash",
            "require usr/bin/getpolicyload",
            "require sbin/init",
            "symlink bin/sh bash",
            "metadata base_packages=bash coreutils grep findutils e2fsprogs jq curl zsh attr acl libcap libselinux",
            "image_link base /bin/sh bash",
            "image_file base /usr/bin/jq",
        ):
            self.assertIn(line, policy)
        for name in ("autotools.bzl", "bash.bzl", "coreutils.bzl", "local.bzl", "package.bzl", "build_state.py"):
            self.assertNotIn("assembly_policy", (PACKAGES / name).read_text(encoding="utf-8"), name)

    def test_cache_tags_and_tool_path_stay(self) -> None:
        autotools = (PACKAGES / "autotools.bzl").read_text(encoding="utf-8")
        self.assertNotIn('"no-remote-cache": "1"', autotools)
        self.assertNotIn("no-remote-cache", autotools)
        self.assertIn('"no-sandbox": "1"', autotools)
        self.assertIn('"no-remote-exec": "1"', autotools)
        self.assertIn('export PATH="$tool_bin:$work/toolchain:/usr/bin:/bin"', autotools)
        self.assertIn('clang="$tool_bin/clang"', autotools)
        self.assertIn('autom4te_perllibdir="$share/autoconf"', autotools)
        self.assertNotIn("xcrun", autotools)
        kernel = (ROOT / "bazel/feasibility/kernel/kernel_macho.bzl").read_text(encoding="utf-8")
        for mnemonic in ("OrlixTctiIsaRestore", "OrlixKernelMachOArchive"):
            start = kernel.index(f'mnemonic = "{mnemonic}"')
            block = kernel[start:kernel.index("\n", kernel.index("execution_requirements = ", start))]
            self.assertNotIn("no-remote-cache", block, mnemonic)
            self.assertIn('"no-remote-exec": "1"', block, mnemonic)
        self.assertIn('"no-sandbox": "1"', kernel)
        for name in ("coreutils.bzl", "bash.bzl"):
            text = (PACKAGES / name).read_text(encoding="utf-8")
            self.assertIn('"no-sandbox": "1"', text, name)
            self.assertNotIn("no-remote-cache", text, name)
            self.assertIn('export PATH="$tool_bin:/usr/bin:/bin"', text, name)
            self.assertIn('clang="$tool_bin/clang"', text, name)
            self.assertNotIn("/opt/homebrew/bin", text, name)
            self.assertNotIn("xcrun", text, name)
        rootfs = (ROOTFS / "rootfs.bzl").read_text(encoding="utf-8")
        self.assertIn('"PATH": "/usr/bin:/bin"', rootfs)

    def test_rootfs_command_is_valid_bash(self) -> None:
        rule = (ROOTFS / "rootfs.bzl").read_text(encoding="utf-8")
        start = rule.index('command = r"""') + len('command = r"""\n')
        end = rule.index('\n""",', start)
        script = rule[start:end]
        self.assertIn('policy="$exec_root/${14}"', script)
        self.assertNotIn('copy_bin "$base_tree/usr/bin/grep"', script)
        completed = subprocess.run(["/bin/bash", "-n"], input=script, text=True, capture_output=True)
        self.assertEqual(completed.returncode, 0, completed.stderr)


if __name__ == "__main__":
    unittest.main()
