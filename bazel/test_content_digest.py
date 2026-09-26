"""Consumer-key tests for artifact-identity-v2.

The key is the logical path, type, mode, file bytes, and symlink target.
An origin exec path is not part of that key.
"""

from __future__ import annotations

import hashlib
import io
import json
import os
import stat
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path

from bazel.content_digest import (
    artifact_identity_v2,
    artifact_manifest_v2,
    assert_staged_product_match,
    consumer_file_identity,
    consumer_tree_identity,
    main,
    restore_recorded_modes,
)

ROOT = Path(__file__).resolve().parents[1]
FORBIDDEN_KEY_FIELDS = (
    "source_input_digest",
    "linux_revision",
    "profile",
    "destination",
    "target_triple",
)


class ConsumerIdentityTests(unittest.TestCase):
    def test_logical_path_and_bytes_form_the_key_without_the_origin_path(self) -> None:
        payload = b"selected-bytes\n"
        logical = "arch/orlix/boot/dts/development.dtb"
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            origin_a = base / "origin-a" / "execroot" / "out" / "development.dtb"
            origin_b = base / "origin-b" / "promoted" / "verified" / "development.dtb"
            for path in (origin_a, origin_b):
                path.parent.mkdir(parents=True)
                path.write_bytes(payload)
                path.chmod(0o644)
            first = artifact_manifest_v2(artifacts={logical: origin_a})
            second = artifact_manifest_v2(artifacts={logical: origin_b})
            self.assertEqual(first, second)
            self.assertEqual(
                artifact_identity_v2(artifacts={logical: origin_a}),
                consumer_file_identity(origin_b, logical),
            )
            text = first.decode("ascii")
            manifest = json.loads(first)
            self.assertEqual(
                [entry["path"] for entry in manifest["entries"]],
                [logical],
            )
            self.assertEqual(
                manifest["entries"][0]["content_sha256"],
                hashlib.sha256(payload).hexdigest(),
            )
            self.assertEqual(manifest["entries"][0]["type"], "file")
            self.assertEqual(manifest["entries"][0]["mode"], 0o644)
            self.assertIn(logical, text)
            self.assertNotIn(str(origin_a), text)
            self.assertNotIn(str(origin_b), text)
            self.assertNotIn("origin-a", text)
            self.assertNotIn("origin-b", text)
            self.assertNotIn("execroot", text)
            for field in FORBIDDEN_KEY_FIELDS:
                self.assertNotIn(field, manifest)
                self.assertNotIn(field, manifest["entries"][0])
            origin_b.write_bytes(b"changed-bytes\n")
            self.assertNotEqual(first, artifact_manifest_v2(artifacts={logical: origin_b}))
            moved = artifact_manifest_v2(
                artifacts={"arch/orlix/boot/dts/release.dtb": origin_a}
            )
            self.assertNotEqual(first, moved)

    def test_absolute_staging_symlink_uses_file_bytes_not_the_origin_path(self) -> None:
        payload = b"kernel-archive"
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            real = base / "output-base" / "OrlixKernel.a"
            real.parent.mkdir()
            real.write_bytes(payload)
            real.chmod(0o644)
            staged = base / "execroot" / "OrlixKernel.a"
            staged.parent.mkdir()
            staged.symlink_to(real)
            logical = "OrlixKernel.a"
            via_link = artifact_manifest_v2(artifacts={logical: staged})
            via_file = artifact_manifest_v2(artifacts={logical: real})
            self.assertEqual(via_link, via_file)
            text = via_link.decode("ascii")
            self.assertNotIn(str(real), text)
            self.assertNotIn(str(staged), text)
            self.assertNotIn("output-base", text)
            self.assertEqual(json.loads(via_link)["entries"][0]["type"], "file")
            directory = base / "not-a-file"
            directory.mkdir()
            link = base / "dir-link"
            link.symlink_to(directory)
            with self.assertRaisesRegex(ValueError, "not a file"):
                artifact_identity_v2(artifacts={"tree": link})

    def test_relative_symlink_target_stays_and_tree_origin_does_not(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "tree"
            (root / "bin").mkdir(parents=True)
            tool = root / "bin" / "tool"
            tool.write_bytes(b"same")
            tool.chmod(0o755)
            (root / "alias").symlink_to("bin/tool")
            manifest = json.loads(artifact_manifest_v2(root))
            encoded = artifact_manifest_v2(root).decode("ascii")
            alias = next(entry for entry in manifest["entries"] if entry["path"] == "alias")
            self.assertEqual(alias["type"], "symlink")
            self.assertEqual(alias["target"], "bin/tool")
            self.assertNotIn(str(root), encoded)
            self.assertNotIn(str(tmp), encoded)
            other = Path(tmp) / "other-origin" / "tree"
            (other / "bin").mkdir(parents=True)
            copied = other / "bin" / "tool"
            copied.write_bytes(b"same")
            copied.chmod(0o755)
            (other / "alias").symlink_to("bin/tool")
            self.assertEqual(artifact_identity_v2(root), artifact_identity_v2(other))

    def test_consumer_tree_ignores_directory_mode_and_origin_and_misses_on_bytes(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            first = base / "origin-a" / "include"
            second = base / "origin-b" / "include"
            for root in (first, second):
                nested = root / "linux"
                nested.mkdir(parents=True)
                header = nested / "unistd.h"
                header.write_bytes(b"int read(void);\n")
                header.chmod(0o644)
            self.assertEqual(consumer_tree_identity(first), consumer_tree_identity(second))
            manifest = artifact_manifest_v2(first, files_only=True).decode("ascii")
            self.assertIn("linux/unistd.h", manifest)
            self.assertNotIn("origin-a", manifest)
            self.assertNotIn(str(first), manifest)
            os.chmod(first / "linux", 0o700)
            self.assertEqual(consumer_tree_identity(first), consumer_tree_identity(second))
            (second / "linux" / "unistd.h").write_bytes(b"int write(void);\n")
            self.assertNotEqual(consumer_tree_identity(first), consumer_tree_identity(second))

    def test_mode_change_misses_and_owner_write_staging_matches(self) -> None:
        payload = b"payload"
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "payload"
            path.write_bytes(payload)
            path.chmod(0o444)
            recorded = artifact_manifest_v2(artifacts={"payload": path})
            path.chmod(0o755)
            changed = artifact_manifest_v2(artifacts={"payload": path})
            self.assertNotEqual(recorded, changed)
            with self.assertRaisesRegex(ValueError, "mode differs"):
                assert_staged_product_match(recorded, changed)
            path.chmod(0o644)
            staged = artifact_manifest_v2(artifacts={"payload": path})
            self.assertNotEqual(recorded, staged)
            assert_staged_product_match(recorded, staged)
            path.write_bytes(b"other")
            with self.assertRaisesRegex(ValueError, "content differs"):
                assert_staged_product_match(recorded, artifact_manifest_v2(artifacts={"payload": path}))

    def test_restore_recorded_modes_and_cli_consumer_file(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "tree"
            root.mkdir()
            payload = root / "payload"
            payload.write_bytes(b"payload")
            payload.chmod(0o755)
            recorded = artifact_manifest_v2(root)
            payload.chmod(0o644)
            restore_recorded_modes(recorded, root)
            self.assertEqual(stat.S_IMODE(payload.stat().st_mode), 0o755)
            stdout = io.StringIO()
            with redirect_stdout(stdout):
                self.assertEqual(
                    main(["--consumer-file", "--logical-path", "payload", str(payload)]),
                    0,
                )
            self.assertEqual(
                stdout.getvalue().strip(),
                consumer_file_identity(payload, "payload"),
            )
            self.assertNotIn(str(payload), stdout.getvalue())

    def test_composition_consumer_omits_origin_metadata(self) -> None:
        composition = (ROOT / "bazel/product/composition.bzl").read_text(encoding="utf-8")
        implementation = composition.split("def _kernel_composition_impl", 1)[1].split(
            "orlix_kernel_composition = rule", 1
        )[0]
        self.assertIn("artifact_identity_v2", implementation)
        self.assertIn('"OrlixKernel.a"', implementation)
        self.assertIn('"arch/orlix/boot/dts/"', implementation)
        self.assertIn("archive.artifact_identity_digest", implementation)
        self.assertNotIn("source_input_digest", implementation)
        self.assertNotIn("linux_revision", composition)
        self.assertNotIn("target_triple", composition)
        self.assertNotIn("locked_buildset", implementation)
        self.assertNotIn("ctx.attr.profile", implementation)
        self.assertNotIn("ctx.attr.destination", implementation)
        self.assertNotIn("DEVELOPER_DIR", implementation)
        identity = (ROOT / "bazel/artifact_identity.bzl").read_text(encoding="utf-8")
        self.assertNotIn("DEVELOPER_DIR", identity)
        self.assertNotIn("source_input_digest", identity)
        product = (ROOT / "bazel/product/BUILD.bazel").read_text(encoding="utf-8")
        self.assertIn('linux_archive = "//bazel/feasibility/kernel:macho"', product)
        self.assertIn('hostadapter = "//OrlixHostAdapter/Sources:OrlixHostAdapter_srcs"', product)


if __name__ == "__main__":
    unittest.main()
