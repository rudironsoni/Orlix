from __future__ import annotations

import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import proof_index
from locked_buildset import required_components


def _signed_record(name: str) -> dict:
    digest = hashlib.sha256(f"{name}:unsigned".encode()).hexdigest()
    oci = "sha256:" + hashlib.sha256(f"{name}:oci".encode()).hexdigest()
    return {
        "schema": 2,
        "component": name,
        "unsigned_digest": digest,
        "artifact_identity": {
            "format": "artifact-identity-v2",
            "version": 2,
            "digest": digest,
        },
        "signed": True,
        "oci_digest": oci,
        "oci_reference": f"ghcr.io/rudironsoni/orlix/{name}@{oci}",
    }


def _layout(root: Path) -> dict:
    (root / "buildset" / "a").mkdir(parents=True)
    (root / "buildset" / "b").mkdir(parents=True)
    (root / "buildset" / "a" / "execution.json").write_text("{}\n", encoding="utf-8")
    (root / "buildset" / "a" / "build-events.json").write_text("{}\n", encoding="utf-8")
    (root / "buildset" / "b" / "execution.json").write_text("{}\n", encoding="utf-8")
    (root / "buildset" / "b" / "build-events.json").write_text("{}\n", encoding="utf-8")
    (root / "toolchain-manifest.json").write_text(
        json.dumps({"schema": 1, "kind": "observed-toolchain"}) + "\n", encoding="utf-8"
    )
    (root / "trust-policy.json").write_text(json.dumps({"keys": []}) + "\n", encoding="utf-8")
    for name in required_components(2):
        component_dir = root / name
        component_dir.mkdir(parents=True)
        (component_dir / f"{name}-signed.json").write_text(
            json.dumps(_signed_record(name)) + "\n", encoding="utf-8"
        )
        (component_dir / f"{name}-sbom.json").write_text(
            json.dumps({"bomFormat": "CycloneDX"}) + "\n", encoding="utf-8"
        )
        (component_dir / f"{name}-in-toto.json").write_text(
            json.dumps({"kind": "in-toto"}) + "\n", encoding="utf-8"
        )
    return {
        "promote_root": str(root),
        "source_sha": "ab" * 20,
        "toolchain_manifest": str(root / "toolchain-manifest.json"),
        "trust_policy": str(root / "trust-policy.json"),
        "builder_id": "https://example.invalid/promote",
        "workflow": "bazel-promote.yml",
        "run_id": "123",
    }


class ProofIndexTests(unittest.TestCase):
    def test_binds_candidate_buildset_evidence(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            kwargs = _layout(root)
            out = root / "promotion-proof-index.json"
            payload = proof_index.write_proof_index(str(out), **kwargs)
            self.assertEqual(payload["schema"], 1)
            self.assertEqual(payload["kind"], "promotion-proof-index")
            self.assertEqual(payload["source_sha"], "ab" * 20)
            self.assertEqual(
                payload["toolchain_manifest_sha256"],
                hashlib.sha256((root / "toolchain-manifest.json").read_bytes()).hexdigest(),
            )
            self.assertEqual(set(payload["components"]), set(required_components(2)))
            for name, entry in payload["components"].items():
                self.assertEqual(entry["reproducibility"], "match")
                self.assertTrue(entry["sbom_sha256"])
                self.assertTrue(entry["provenance_sha256"])
            self.assertTrue(all(gate["result"] == "pass" for gate in payload["gates"]))
            self.assertEqual(
                [gate["name"] for gate in payload["gates"]],
                list(proof_index.REQUIRED_GATES),
            )
            loaded = json.loads(out.read_text(encoding="utf-8"))
            self.assertEqual(loaded, payload)

    def test_missing_prerequisite_fails_instead_of_weakening(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            kwargs = _layout(root)
            (root / "buildset" / "b" / "build-events.json").unlink()
            with self.assertRaises(ValueError):
                proof_index.write_proof_index(str(root / "out.json"), **kwargs)

    def test_legacy_identity_fails(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            kwargs = _layout(root)
            record_path = root / "uapi" / "uapi-signed.json"
            record = json.loads(record_path.read_text(encoding="utf-8"))
            record["artifact_identity"] = {
                "format": "legacy-marker-sha256",
                "version": 1,
                "digest": record["unsigned_digest"],
                "marker": "uapi.sha256",
            }
            record_path.write_text(json.dumps(record) + "\n", encoding="utf-8")
            with self.assertRaises(ValueError):
                proof_index.write_proof_index(str(root / "out.json"), **kwargs)

    def test_bad_source_sha_fails(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            kwargs = _layout(root)
            kwargs["source_sha"] = "not-a-sha"
            with self.assertRaises(ValueError):
                proof_index.write_proof_index(str(root / "out.json"), **kwargs)


if __name__ == "__main__":
    unittest.main()
