from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

import checkpoint


SOURCE = "ab" * 20
TOOLCHAIN = "cd" * 32


def _identity(root: Path, inputs: list[str], build_config: str = "release,promotion,26.6,iphoneos,dbg") -> str:
    return checkpoint.component_input_identity(
        root, inputs, root / "components.json", build_config
    )


def _stage(root: Path, component: str, digest: str) -> None:
    for side in ("a", "b"):
        tree = root / component / side / "staged"
        tree.mkdir(parents=True, exist_ok=True)
        (tree / "artifact-identity-v2.json").write_text("{}\n", encoding="utf-8")
        (root / component / side / "digest.sha256").write_text(digest + "\n", encoding="ascii")


class CheckpointTests(unittest.TestCase):
    def _fixture(self, root: Path) -> str:
        (root / "components.json").write_text('{"components": []}\n', encoding="utf-8")
        (root / "kernel.c").write_text("kernel v1\n", encoding="utf-8")
        (root / "mlibc.c").write_text("mlibc v1\n", encoding="utf-8")
        (root / "rule.bzl").write_text("rule v1\n", encoding="utf-8")
        return _identity(root, ["kernel.c", "mlibc.c", "rule.bzl"])

    def test_same_inputs_reuse(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            identity = self._fixture(root)
            _stage(root, "uapi", "11" * 32)
            checkpoint.write_checkpoint(str(root), SOURCE, identity, TOOLCHAIN, ["uapi"])
            validated = checkpoint.validate_checkpoint(str(root), identity, TOOLCHAIN, ["uapi"])
            self.assertEqual(validated["proof_source_sha"], SOURCE)

    def test_control_plane_only_change_reuses(self) -> None:
        # A different commit (proof_source_sha) with identical component inputs
        # and toolchain must reuse the expensive proof.
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            identity = self._fixture(root)
            _stage(root, "uapi", "11" * 32)
            checkpoint.write_checkpoint(str(root), SOURCE, identity, TOOLCHAIN, ["uapi"])
            validated = checkpoint.validate_checkpoint(str(root), identity, TOOLCHAIN, ["uapi"])
            self.assertEqual(validated["proof_source_sha"], SOURCE)

    def test_producer_and_toolchain_and_registry_changes_reject(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            identity = self._fixture(root)
            _stage(root, "uapi", "11" * 32)
            checkpoint.write_checkpoint(str(root), SOURCE, identity, TOOLCHAIN, ["uapi"])

            (root / "kernel.c").write_text("kernel v2\n", encoding="utf-8")
            with self.assertRaisesRegex(checkpoint.CheckpointError, "different component inputs"):
                checkpoint.validate_checkpoint(str(root), _identity(root, ["kernel.c", "mlibc.c", "rule.bzl"]), TOOLCHAIN, ["uapi"])

            (root / "kernel.c").write_text("kernel v1\n", encoding="utf-8")
            (root / "mlibc.c").write_text("mlibc v2\n", encoding="utf-8")
            with self.assertRaisesRegex(checkpoint.CheckpointError, "different component inputs"):
                checkpoint.validate_checkpoint(str(root), _identity(root, ["kernel.c", "mlibc.c", "rule.bzl"]), TOOLCHAIN, ["uapi"])

            (root / "mlibc.c").write_text("mlibc v1\n", encoding="utf-8")
            (root / "rule.bzl").write_text("rule v2\n", encoding="utf-8")
            with self.assertRaisesRegex(checkpoint.CheckpointError, "different component inputs"):
                checkpoint.validate_checkpoint(str(root), _identity(root, ["kernel.c", "mlibc.c", "rule.bzl"]), TOOLCHAIN, ["uapi"])

            (root / "rule.bzl").write_text("rule v1\n", encoding="utf-8")
            (root / "components.json").write_text('{"components": [1]}\n', encoding="utf-8")
            with self.assertRaisesRegex(checkpoint.CheckpointError, "different component inputs"):
                checkpoint.validate_checkpoint(str(root), _identity(root, ["kernel.c", "mlibc.c", "rule.bzl"]), TOOLCHAIN, ["uapi"])

            (root / "components.json").write_text('{"components": []}\n', encoding="utf-8")
            with self.assertRaisesRegex(checkpoint.CheckpointError, "different toolchain"):
                checkpoint.validate_checkpoint(str(root), identity, "ef" * 32, ["uapi"])

    def test_build_config_changes_identity(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            base = self._fixture(root)
            self.assertNotEqual(base, _identity(root, ["kernel.c", "mlibc.c", "rule.bzl"], "release,promotion,26.6,iphoneos,opt"))

    def test_tampered_staged_digest_rejects(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            identity = self._fixture(root)
            _stage(root, "uapi", "11" * 32)
            checkpoint.write_checkpoint(str(root), SOURCE, identity, TOOLCHAIN, ["uapi"])
            (root / "uapi" / "b" / "digest.sha256").write_text("ff" * 32 + "\n", encoding="ascii")
            with self.assertRaisesRegex(checkpoint.CheckpointError, "differs from checkpoint"):
                checkpoint.validate_checkpoint(str(root), identity, TOOLCHAIN, ["uapi"])

    def test_component_set_change_rejects(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            identity = self._fixture(root)
            _stage(root, "uapi", "11" * 32)
            checkpoint.write_checkpoint(str(root), SOURCE, identity, TOOLCHAIN, ["uapi"])
            with self.assertRaisesRegex(checkpoint.CheckpointError, "different component set"):
                checkpoint.validate_checkpoint(str(root), identity, TOOLCHAIN, ["uapi", "mlibc"])

    def test_missing_declared_input_changes_identity(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            identity = self._fixture(root)
            self.assertNotEqual(identity, _identity(root, ["kernel.c", "mlibc.c", "rule.bzl", "absent.c"]))

    def test_cli_compute_and_check(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            identity = self._fixture(root)
            _stage(root, "uapi", "11" * 32)
            inputs = root / "inputs.txt"
            inputs.write_text("kernel.c\nmlibc.c\nrule.bzl\n", encoding="utf-8")
            import io
            from contextlib import redirect_stdout

            buffer = io.StringIO()
            with redirect_stdout(buffer):
                rc = checkpoint.main(
                    ["--compute-identity", "--repo-root", str(root), "--inputs-file", str(inputs),
                     "--registry", str(root / "components.json"), "--build-config", "release,promotion,26.6,iphoneos,dbg"]
                )
            self.assertEqual(rc, 0)
            self.assertEqual(buffer.getvalue().strip(), identity)
            self.assertEqual(
                checkpoint.main(
                    ["--write", "--promote-root", str(root), "--proof-source-sha", SOURCE,
                     "--component-input-identity", identity, "--toolchain-sha256", TOOLCHAIN, "--components", "uapi"]
                ),
                0,
            )
            self.assertEqual(
                checkpoint.main(
                    ["--check", "--promote-root", str(root), "--component-input-identity", identity,
                     "--toolchain-sha256", TOOLCHAIN, "--components", "uapi"]
                ),
                0,
            )
            self.assertEqual(
                checkpoint.main(
                    ["--check", "--promote-root", str(root), "--component-input-identity", "ff" * 32,
                     "--toolchain-sha256", TOOLCHAIN, "--components", "uapi"]
                ),
                1,
            )


if __name__ == "__main__":
    unittest.main()
