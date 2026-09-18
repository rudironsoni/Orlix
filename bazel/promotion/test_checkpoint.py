from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

import checkpoint


SOURCE = "ab" * 20
TOOLCHAIN = "cd" * 32


def _stage(root: Path, component: str, digest: str) -> None:
    for side in ("a", "b"):
        tree = root / component / side / "staged"
        tree.mkdir(parents=True, exist_ok=True)
        (tree / "artifact-identity-v2.json").write_text("{}\n", encoding="utf-8")
        (root / component / side / "digest.sha256").write_text(digest + "\n", encoding="ascii")


class CheckpointTests(unittest.TestCase):
    def test_write_then_validate_round_trip(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            _stage(root, "uapi", "11" * 32)
            _stage(root, "mlibc", "22" * 32)
            payload = checkpoint.write_checkpoint(str(root), SOURCE, TOOLCHAIN, ["mlibc", "uapi"])
            self.assertEqual(sorted(payload["components"]), ["mlibc", "uapi"])
            validated = checkpoint.validate_checkpoint(str(root), SOURCE, TOOLCHAIN, ["uapi", "mlibc"])
            self.assertEqual(validated["source_sha"], SOURCE)

    def test_validate_rejects_different_source(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            _stage(root, "uapi", "11" * 32)
            checkpoint.write_checkpoint(str(root), SOURCE, TOOLCHAIN, ["uapi"])
            with self.assertRaisesRegex(checkpoint.CheckpointError, "different source"):
                checkpoint.validate_checkpoint(str(root), "ff" * 20, TOOLCHAIN, ["uapi"])

    def test_validate_rejects_different_toolchain_and_component_set(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            _stage(root, "uapi", "11" * 32)
            checkpoint.write_checkpoint(str(root), SOURCE, TOOLCHAIN, ["uapi"])
            with self.assertRaisesRegex(checkpoint.CheckpointError, "different toolchain"):
                checkpoint.validate_checkpoint(str(root), SOURCE, "ef" * 32, ["uapi"])
            with self.assertRaisesRegex(checkpoint.CheckpointError, "different component set"):
                checkpoint.validate_checkpoint(str(root), SOURCE, TOOLCHAIN, ["uapi", "mlibc"])

    def test_validate_rejects_tampered_digest(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            _stage(root, "uapi", "11" * 32)
            checkpoint.write_checkpoint(str(root), SOURCE, TOOLCHAIN, ["uapi"])
            (root / "uapi" / "b" / "digest.sha256").write_text("ff" * 32 + "\n", encoding="ascii")
            with self.assertRaisesRegex(checkpoint.CheckpointError, "differs from checkpoint"):
                checkpoint.validate_checkpoint(str(root), SOURCE, TOOLCHAIN, ["uapi"])

    def test_write_rejects_sides_that_disagree(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            _stage(root, "uapi", "11" * 32)
            (root / "uapi" / "b" / "digest.sha256").write_text("ff" * 32 + "\n", encoding="ascii")
            with self.assertRaisesRegex(checkpoint.CheckpointError, "differ between sides"):
                checkpoint.write_checkpoint(str(root), SOURCE, TOOLCHAIN, ["uapi"])

    def test_missing_checkpoint_is_an_error(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaisesRegex(checkpoint.CheckpointError, "no promotion checkpoint"):
                checkpoint.validate_checkpoint(tmp, SOURCE, TOOLCHAIN, ["uapi"])

    def test_cli_check_and_write(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            _stage(root, "uapi", "11" * 32)
            self.assertEqual(
                checkpoint.main(
                    ["--write", "--promote-root", str(root), "--source-sha", SOURCE,
                     "--toolchain-sha256", TOOLCHAIN, "--components", "uapi"]
                ),
                0,
            )
            self.assertEqual(
                checkpoint.main(
                    ["--check", "--promote-root", str(root), "--source-sha", SOURCE,
                     "--toolchain-sha256", TOOLCHAIN, "--components", "uapi"]
                ),
                0,
            )
            self.assertEqual(
                checkpoint.main(
                    ["--check", "--promote-root", str(root), "--source-sha", "ff" * 20,
                     "--toolchain-sha256", TOOLCHAIN, "--components", "uapi"]
                ),
                1,
            )


if __name__ == "__main__":
    unittest.main()
