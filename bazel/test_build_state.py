"""Resume reuses retained trees when mode and bytes match.

A past mtime change is not a content change. A future mtime is rejected.
Validity still reads and hashes file bytes.
"""

from __future__ import annotations

import json
import os
import shutil
import tempfile
import time
import unittest
from pathlib import Path

from bazel.build_state import record, resume


_PAST_MTIME_NS = 1_000_000_000_000_000_000


class BuildStateResumeTests(unittest.TestCase):
    def _recorded_tree(self) -> tuple[Path, Path, Path, Path]:
        root = Path(self.temporary.name)
        identity = root / "identity-input"
        identity.write_bytes(b"rules\n")
        self.assertFalse(resume(root, [identity], ("build",)))
        artifact = root / "build" / "nested" / "object.o"
        artifact.parent.mkdir(parents=True)
        artifact.write_bytes(b"correct object\n")
        link = root / "build" / "alias"
        link.symlink_to("nested/object.o")
        record(root, ("build",))
        return root, identity, artifact, link

    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def _stored_mtime(self, root: Path, name: str) -> int:
        stored = json.loads((root / "build-state.json").read_text())
        return stored["files"][name][2]

    def test_same_bytes_with_changed_mtime_stay_valid(self) -> None:
        root, identity, artifact, link = self._recorded_tree()
        stored_file = self._stored_mtime(root, "build/nested/object.o")
        stored_link = self._stored_mtime(root, "build/alias")
        self.assertNotEqual(stored_file, _PAST_MTIME_NS)
        self.assertNotEqual(stored_link, _PAST_MTIME_NS)
        os.utime(artifact, ns=(_PAST_MTIME_NS, _PAST_MTIME_NS))
        os.utime(link, ns=(_PAST_MTIME_NS, _PAST_MTIME_NS), follow_symlinks=False)
        self.assertEqual(artifact.stat().st_mtime_ns, _PAST_MTIME_NS)
        self.assertEqual(link.lstat().st_mtime_ns, _PAST_MTIME_NS)
        self.assertTrue(resume(root, [identity], ("build",)))
        self.assertEqual(artifact.read_bytes(), b"correct object\n")
        self.assertEqual(os.readlink(link), "nested/object.o")
        self.assertTrue((root / "build").is_dir())

    def test_changed_byte_deletes_trees(self) -> None:
        root, identity, artifact, _link = self._recorded_tree()
        stored = self._stored_mtime(root, "build/nested/object.o")
        payload = bytearray(artifact.read_bytes())
        payload[0] ^= 0xFF
        artifact.write_bytes(bytes(payload))
        os.utime(artifact, ns=(stored, stored))
        self.assertEqual(artifact.stat().st_mtime_ns, stored)
        self.assertFalse(resume(root, [identity], ("build",)))
        self.assertFalse((root / "build").exists())

    def test_future_mtime_is_rejected(self) -> None:
        root, identity, artifact, _link = self._recorded_tree()
        future = time.time_ns() + 3600 * 10**9
        os.utime(artifact, ns=(future, future))
        self.assertGreater(artifact.stat().st_mtime_ns, time.time_ns())
        self.assertFalse(resume(root, [identity], ("build",)))
        self.assertFalse((root / "build").exists())

    def test_executable_bit_change_deletes_trees(self) -> None:
        root, identity, artifact, _link = self._recorded_tree()
        artifact.chmod(0o755)
        self.assertFalse(resume(root, [identity], ("build",)))
        self.assertFalse((root / "build").exists())

    def test_identity_input_change_deletes_trees(self) -> None:
        root, identity, _artifact, _link = self._recorded_tree()
        identity.write_bytes(b"other rules\n")
        self.assertFalse(resume(root, [identity], ("build",)))
        self.assertFalse((root / "build").exists())

    def test_package_mismatch_keeps_the_record_and_trees(self) -> None:
        root, identity, artifact, _link = self._recorded_tree()
        record(root, ("build",), compatibility={"package": "bash", "target": "aarch64-linux-gnu"})
        before = (root / "build-state.json").read_bytes()
        with self.assertRaisesRegex(RuntimeError, "package state mismatch: bash != coreutils"):
            resume(
                root,
                [identity],
                ("build",),
                compatibility={"package": "coreutils", "target": "aarch64-linux-gnu"},
            )
        self.assertEqual(artifact.read_bytes(), b"correct object\n")
        self.assertEqual((root / "build-state.json").read_bytes(), before)

    def test_symlink_build_tree_is_rejected_without_following_it(self) -> None:
        root, identity, _artifact, _link = self._recorded_tree()
        outside = root / "outside"
        outside.mkdir()
        secret = outside / "secret"
        secret.write_bytes(b"keep\n")
        build = root / "build"
        shutil.rmtree(build)
        build.symlink_to(outside, target_is_directory=True)
        self.assertFalse(resume(root, [identity], ("build",)))
        self.assertEqual(secret.read_bytes(), b"keep\n")
        self.assertFalse(build.exists())
