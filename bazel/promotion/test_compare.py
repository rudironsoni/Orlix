from __future__ import annotations

import json
import os
import tempfile
import unittest
from pathlib import Path

import compare


class PromotionCompareTests(unittest.TestCase):
    def test_matching_digests_write_unsigned_proposal(self) -> None:
        digest = "a" * 64
        with tempfile.TemporaryDirectory() as tmp:
            first = Path(tmp) / "a.sha256"
            second = Path(tmp) / "b.sha256"
            proposal = Path(tmp) / "proposal.json"
            first.write_text(digest + "\n", encoding="utf-8")
            second.write_text(digest + "\n", encoding="utf-8")
            self.assertEqual(compare.compare_digests(str(first), str(second)), digest)
            env = os.environ.get("ORLIX_COSIGN_KEY")
            if env is not None:
                del os.environ["ORLIX_COSIGN_KEY"]
            try:
                self.assertEqual(
                    compare.main(
                        [str(first), str(second), "--component", "uapi", "--proposal", str(proposal)]
                    ),
                    0,
                )
            finally:
                if env is not None:
                    os.environ["ORLIX_COSIGN_KEY"] = env
            payload = json.loads(proposal.read_text(encoding="utf-8"))
            self.assertEqual(payload["unsigned_digest"], digest)
            self.assertIs(payload["signed"], False)
            self.assertIsNone(payload["oci_digest"])
            self.assertEqual(payload["component"], "uapi")
            self.assertNotIn("signature", payload)
            self.assertNotIn("ghcr", json.dumps(payload))

    def test_mismatch_fails_loud(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            first = Path(tmp) / "a.sha256"
            second = Path(tmp) / "b.sha256"
            first.write_text("a" * 64 + "\n", encoding="utf-8")
            second.write_text("b" * 64 + "\n", encoding="utf-8")
            with self.assertRaises(ValueError):
                compare.compare_digests(str(first), str(second))

    def test_unsigned_lock_proposal_does_not_mutate_signed_lock(self) -> None:
        digest = "d" * 64
        signed_lock = {
            "schema": 1,
            "buildset": "e" * 64,
            "components": {
                "uapi": {
                    "unsigned_digest": digest,
                    "oci_digest": "sha256:" + ("f" * 64),
                    "oci_reference": "localhost:5001/orlix/uapi@sha256:" + ("f" * 64),
                }
            },
        }
        with tempfile.TemporaryDirectory() as tmp:
            first = Path(tmp) / "a.sha256"
            second = Path(tmp) / "b.sha256"
            proposal = Path(tmp) / "lock-proposal.json"
            lock_path = Path(tmp) / "artifacts.lock.json"
            first.write_text(digest + "\n", encoding="utf-8")
            second.write_text(digest + "\n", encoding="utf-8")
            lock_path.write_text(json.dumps(signed_lock, indent=2) + "\n", encoding="utf-8")
            before = lock_path.read_text(encoding="utf-8")
            self.assertEqual(
                compare.main(
                    [
                        str(first),
                        str(second),
                        "--component",
                        "uapi",
                        "--lock-proposal",
                        str(proposal),
                        "--lock",
                        str(lock_path),
                    ]
                ),
                0,
            )
            self.assertEqual(lock_path.read_text(encoding="utf-8"), before)
            payload = json.loads(proposal.read_text(encoding="utf-8"))
            self.assertIs(payload["signed"], False)
            self.assertIsNone(payload["buildset"])

    def test_unset_cosign_key_does_not_invent_signature(self) -> None:
        digest = "c" * 64
        with tempfile.TemporaryDirectory() as tmp:
            first = Path(tmp) / "a.sha256"
            proposal = Path(tmp) / "proposal.json"
            first.write_text(digest + "\n", encoding="utf-8")
            os.environ.pop("ORLIX_COSIGN_KEY", None)
            compare.write_proposal(str(proposal), "uapi", digest)
            payload = json.loads(proposal.read_text(encoding="utf-8"))
            self.assertIs(payload["signed"], False)
            self.assertIsNone(payload["oci_digest"])

    def test_make_promote_uses_unique_component_output_roots(self) -> None:
        text = Path(__file__).resolve().parents[2] / "make" / "bazel-migration.mk"
        body = text.read_text(encoding="utf-8")
        self.assertIn('promote="$(ORLIX_BUILD_ROOT)/Bazel/promote/$(1)"', body)
        self.assertIn("--nouse_action_cache", body)
        self.assertIn("ORLIX_KBUILD_PERSIST=", body)
        self.assertIn("bazel/promotion/sign.py", body)
        self.assertIn("unsigned promote must not Cosign-sign", body)
        self.assertIn("unsigned promote mutated artifacts.lock.json", body)
        self.assertIn("__bazel-reconstruct-source", body)
        self.assertIn("--config=promoted", body)
        self.assertIn("--lock", body)
        self.assertIn("ORLIX_KBUILD_PERSIST", body)
        self.assertIn("ORLIX_BAZEL_PROMOTE,uapi,", body)
        self.assertIn("ORLIX_BAZEL_PROMOTE,mlibc,", body)
        self.assertIn("ORLIX_BAZEL_PROMOTE,rootfs,", body)
        self.assertIn("feasibility/rootfs/rootfs/source-input.sha256", body)
        self.assertNotIn("ORLIX_GHCR_REPOSITORY", body.split("ORLIX_BAZEL_PROMOTE")[1].split("endef")[0])

    def test_package_digest_hashes_relative_paths(self) -> None:
        root = Path(__file__).resolve().parents[2]
        for rel in (
            "bazel/feasibility/packages/autotools.bzl",
            "bazel/feasibility/packages/coreutils.bzl",
            "bazel/feasibility/packages/bash.bzl",
        ):
            body = (root / rel).read_text(encoding="utf-8")
            self.assertIn('cd "$install_out"', body)
            self.assertIn("-ffile-prefix-map", body)
        rootfs = (root / "bazel/feasibility/rootfs/rootfs.bzl").read_text(encoding="utf-8")
        self.assertIn('cd "$base_tree"', rootfs)
        self.assertIn('shasum -a 256 < "$payload_metadata"', rootfs)
        self.assertIn("touch -t 197001010000", rootfs)
        self.assertIn("pkg.source_input_digest", rootfs)
        self.assertIn("kbuild-archive.tar", (root / "bazel/feasibility/analysis/providers.bzl").read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
