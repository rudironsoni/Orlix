"""Linux proof that the rootfs payload product is the three images."""

from __future__ import annotations

import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from bazel.feasibility.rootfs.stage_payload import (
    EXCLUDED_NAMES,
    PRODUCT_IMAGES,
    stage_payload,
)

RULE = Path(__file__).with_name("rootfs.bzl")
STAGER = Path(__file__).with_name("stage_payload.py")


def _action_requirements(text: str, mnemonic: str) -> str:
    marker = f'mnemonic = "{mnemonic}"'
    start = text.index(marker)
    following = text.find("\ndef ", start)
    block = text[start:following if following != -1 else None]
    match = re.search(r"execution_requirements = \{([^}]+)\}", block)
    if match is None:
        raise AssertionError(f"{mnemonic} has no execution_requirements")
    return match.group(1)


class RootfsPayloadTests(unittest.TestCase):
    def test_payload_directory_is_the_three_images(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            source.mkdir()
            images = {}
            for name in PRODUCT_IMAGES:
                path = source / name
                path.write_bytes(f"image:{name}\n".encode())
                images[name] = path
            for name in EXCLUDED_NAMES:
                (source / name).write_bytes(b"not-a-payload-member\n")
            destination = root / "payload" / "rootfs"
            completed = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(STAGER),
                    "--dest",
                    str(destination),
                    "--image",
                    f"initramfs.cpio.gz={images['initramfs.cpio.gz']}",
                    "--image",
                    f"base.ext4={images['base.ext4']}",
                    "--image",
                    f"state.ext4={images['state.ext4']}",
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertEqual(sorted(path.name for path in destination.iterdir()), list(PRODUCT_IMAGES))
            for name in PRODUCT_IMAGES:
                self.assertEqual((destination / name).read_bytes(), images[name].read_bytes())
            for name in EXCLUDED_NAMES:
                self.assertFalse((destination / name).exists(), name)
            self.assertNotIn(b"not-a-payload-member", b"".join(path.read_bytes() for path in destination.iterdir()))

    def test_digest_and_provenance_names_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            images = {}
            for name in ("source-input.sha256", "base.ext4", "state.ext4"):
                path = root / name
                path.write_bytes(b"bytes\n")
                images[name] = path
            destination = root / "payload"
            with self.assertRaises(ValueError):
                stage_payload(destination, images)
            self.assertFalse(destination.exists())

    def test_assembly_keeps_no_remote_cache_and_payload_may_cache(self) -> None:
        text = RULE.read_text(encoding="utf-8")
        assembly = _action_requirements(text, "OrlixRootfs")
        payload = _action_requirements(text, "OrlixRootfsPayload")
        self.assertIn('"no-remote-cache": "1"', assembly)
        self.assertIn('"no-remote-exec": "1"', assembly)
        self.assertNotIn("no-remote-cache", payload)
        self.assertIn('"no-remote-exec": "1"', payload)
        payload_impl = text.split("def _rootfs_payload_impl", 1)[1].split("orlix_rootfs_payload = rule", 1)[0]
        payload_rule = text.split("orlix_rootfs_payload = rule", 1)[1].split("orlix_rootfs = rule", 1)[0]
        self.assertIn("_stage_payload", payload_impl)
        self.assertIn("stage_payload.py", payload_rule)
        self.assertIn("info.initramfs", payload_impl)
        self.assertIn("info.base_ext4", payload_impl)
        self.assertIn("info.state_ext4", payload_impl)
        self.assertNotIn("source_input_digest", payload_impl)
        self.assertNotIn("file_manifest", payload_impl)
        self.assertNotIn("payload_metadata", payload_impl)
        self.assertNotIn("install_tree", payload_impl)
        self.assertIn("return [DefaultInfo(files = depset([root])), info]", payload_impl)
        self.assertIn("source_input_digest = digest", text)

    def test_package_trees_invalidate_assembly_without_object_incrementality(self) -> None:
        text = RULE.read_text(encoding="utf-8")
        assembly = text.split("def _rootfs_impl", 1)[1].split("def _rootfs_payload_impl", 1)[0]
        self.assertIn("pkg_inputs = [pkg.install_tree for pkg in pkgs]", assembly)
        self.assertIn("direct = pkg_inputs +", assembly)
        self.assertIn("ctx.file.toolchain_identity", assembly)
        self.assertNotIn("pkg.objects", assembly)
        self.assertNotRegex(assembly, r"\.o[\"'\s]")
        payload = text.split("def _rootfs_payload_impl", 1)[1].split("orlix_rootfs_payload = rule", 1)[0]
        self.assertNotIn("toolchain_identity", payload)
        self.assertNotIn("pkg_inputs", payload)


if __name__ == "__main__":
    unittest.main()
