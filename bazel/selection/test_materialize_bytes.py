#!/usr/bin/env python3
"""Selected bytes are copied, not symlink consumer inputs."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from materialize_bytes import (
    DTB_DIRECTORY,
    dtb_logical_name,
    materialize_file,
    materialize_tree,
    project_sysroot,
    selected_identity,
    file_pairs,
)


class MaterializeBytesTest(unittest.TestCase):
    def test_file_and_tree_materialize_bytes_without_symlinks(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            payload = root / "payload.txt"
            payload.write_text("selected\n", encoding="utf-8")
            link = root / "payload.link"
            link.symlink_to(payload)
            copied = root / "out" / "payload.txt"
            materialize_file(link, copied)
            self.assertFalse(copied.is_symlink())
            self.assertEqual(copied.read_text(encoding="utf-8"), "selected\n")

            tree = root / "tree" / "include"
            tree.mkdir(parents=True)
            (tree / "real.h").write_text("header\n", encoding="utf-8")
            (tree / "alias.h").symlink_to(tree / "real.h")
            (tree / "source-input.sha256").write_text("sidecar\n", encoding="utf-8")
            dest = root / "projected"
            materialize_tree(root / "tree", dest)
            projected = dest / "include" / "alias.h"
            self.assertFalse(projected.is_symlink())
            self.assertEqual(projected.read_text(encoding="utf-8"), "header\n")
            self.assertFalse((dest / "include" / "source-input.sha256").exists())

    def test_equal_bytes_keep_stable_logical_identity(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            digests = []
            for name in ("source-origin", "promoted-origin"):
                src = root / name / "include"
                src.mkdir(parents=True)
                (src / "linux.h").write_text("same\n", encoding="utf-8")
                dest = root / f"{name}-out"
                materialize_tree(root / name, dest)
                digests.append(selected_identity(artifacts=file_pairs(dest)))
                self.assertEqual([path for path, _ in file_pairs(dest)], ["include/linux.h"])
            self.assertEqual(digests[0], digests[1])
            self.assertNotIn("source-origin", json.dumps(digests))

    def test_dtb_logical_path_stays_under_arch_orlix(self) -> None:
        self.assertEqual(dtb_logical_name("release.dtb"), f"{DTB_DIRECTORY}/release.dtb")
        with self.assertRaises(ValueError):
            dtb_logical_name("elsewhere/release.dtb")

    def test_sysroot_identity_is_selected_bytes_without_consumed_sidecar(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)

            def tree(name: str, body: str) -> Path:
                path = root / name / "include"
                path.mkdir(parents=True)
                (path / "a.h").write_text(body, encoding="utf-8")
                return root / name

            headers = tree("headers", "h\n")
            libraries = tree("libraries", "l\n")
            runtime = root / "libcompiler_rt.a"
            runtime.write_bytes(b"rt")
            loader = root / "ld.so"
            loader.write_bytes(b"ld")
            dest = root / "selected"
            manifest = dest / "artifact-identity-v2.json"
            digest = dest / "artifact-identity-v2.sha256"
            value = project_sysroot(
                headers_src=headers,
                headers_dest=dest / "headers",
                libraries_src=libraries,
                libraries_dest=dest / "libraries",
                runtime_src=runtime,
                runtime_dest=dest / "libcompiler_rt.a",
                loader_src=loader,
                loader_dest=dest / "ld.so",
                manifest=manifest,
                digest=digest,
            )
            self.assertEqual(len(value), 64)
            self.assertNotEqual(value, None)
            text = manifest.read_text(encoding="utf-8")
            self.assertIn("libcompiler_rt.a", text)
            self.assertNotIn("consumed_uapi.sha256", text)
            self.assertIn(value, digest.read_text(encoding="ascii"))

    def test_boundary_rule_copies_bytes(self) -> None:
        rule = Path(__file__).with_name("boundary.bzl").read_text(encoding="utf-8")
        tool = Path(__file__).with_name("materialize_bytes.py").read_text(encoding="utf-8")
        self.assertNotIn("ctx.actions.symlink", rule)
        self.assertNotIn("artifact_identity_digest = None", rule)
        self.assertIn('"/bin/cp", "-R", "-L"', tool)
        self.assertIn('"/bin/cp", "-L"', tool)
        self.assertNotIn("DEVELOPER_DIR", rule)


if __name__ == "__main__":
    unittest.main()
