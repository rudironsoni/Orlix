from __future__ import annotations

import hashlib
import json
import os
import subprocess
import tempfile
import unittest
from unittest import mock

import locked_buildset
from pathlib import Path

import lock_proposal

_VERIFICATION = {"signing_key_fingerprint": "11" * 32, "trust_policy_sha256": "22" * 32, "verification_policy_version": 1}

def _schema2_proposal() -> dict:
    components = {}
    for name in locked_buildset.required_components(locked_buildset.SCHEMA2):
        artifact_digest = hashlib.sha256(f"{name}:artifact".encode()).hexdigest()
        oci_digest = "sha256:" + hashlib.sha256(f"{name}:oci".encode()).hexdigest()
        identity = {"format": "artifact-identity-v2", "version": 2, "digest": artifact_digest}
        components[name] = {
            "artifact_identity": identity,
            "oci_digest": oci_digest,
            "oci_reference": f"ghcr.io/rudironsoni/orlix/{name}@{oci_digest}",
        }
    return {
        "schema": 2,
        "kind": "lock-proposal",
        "signed": True,
        "buildset": locked_buildset.buildset_digest(components, schema=2),
        "component_types": {name: "kernel-apple-product" if name in locked_buildset.KERNEL_COMPONENTS else name for name in sorted(components)},
        "components": components,
        "verification": _VERIFICATION,
    }


class LockProposalTests(unittest.TestCase):
    def _evidence(self, root: Path, payload: dict) -> dict:
        source_sha = "ab" * 20
        payload["source_sha"] = source_sha
        toolchain = {"kind": "observed-toolchain", "schema": 1}
        (root / "toolchain-manifest.json").write_text(json.dumps(toolchain) + "\n", encoding="utf-8")
        index = {
            "schema": 1,
            "kind": "promotion-proof-index",
            "source_sha": source_sha,
            "components": {name: {} for name in payload["components"]},
        }
        (root / "promotion-proof-index.json").write_text(json.dumps(index) + "\n", encoding="utf-8")
        bound = {}
        for name in sorted(payload["components"]):
            component_dir = root / name
            component_dir.mkdir(parents=True, exist_ok=True)
            sbom = {
                "bomFormat": "CycloneDX",
                "specVersion": "1.6",
                "serialNumber": f"urn:uuid:{name}",
                "version": 1,
                "metadata": {"component": {"name": name}},
                "components": [],
            }
            provenance = {
                "schema": 1,
                "kind": "in-toto",
                "_type": "https://in-toto.io/Statement/v1",
                "subject": [{"name": name, "digest": {"sha256": "ab" * 32}}],
                "predicateType": "https://slsa.dev/provenance/v1",
                "predicate": {},
            }
            sbom_path = component_dir / f"{name}-sbom.json"
            provenance_path = component_dir / f"{name}-in-toto.json"
            sbom_path.write_text(json.dumps(sbom) + "\n", encoding="utf-8")
            provenance_path.write_text(json.dumps(provenance) + "\n", encoding="utf-8")
            bound[name] = {
                "sbom_sha256": hashlib.sha256(sbom_path.read_bytes()).hexdigest(),
                "provenance_sha256": hashlib.sha256(provenance_path.read_bytes()).hexdigest(),
            }
        evidence = {
            "source_sha": source_sha,
            "toolchain_manifest_sha256": hashlib.sha256(
                (root / "toolchain-manifest.json").read_bytes()
            ).hexdigest(),
            "promotion_proof_index_sha256": hashlib.sha256(
                (root / "promotion-proof-index.json").read_bytes()
            ).hexdigest(),
            "components": bound,
        }
        payload["evidence"] = evidence
        return evidence

    def _activate(self, payload: dict, run: mock.Mock) -> dict:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            proposal = root / "proposal.json"
            bundle = root / "proposal.sigstore.json"
            lock = root / "artifacts.lock.json"
            self._evidence(root, payload)
            proposal.write_text(json.dumps(payload) + "\n", encoding="utf-8")
            bundle.write_text("{}\n", encoding="utf-8")
            lock.write_text(json.dumps(lock_proposal.EMPTY_LOCK) + "\n", encoding="utf-8")
            before = lock.read_bytes()
            verify = lambda item: locked_buildset.validate_component(
                item["component"], item, schema=item["schema"]
            )
            try:
                with mock.patch.dict(os.environ, {"ORLIX_COSIGN_PUB": "/public.pub"}, clear=True), \
                     mock.patch("lock_proposal.shutil.which", return_value="/usr/bin/cosign"), \
                     mock.patch("lock_proposal.trusted_public_key", return_value="/public.pub"), \
                     mock.patch("lock_proposal.verification_context", return_value=_VERIFICATION), \
                     mock.patch("lock_proposal.verify_component", side_effect=verify):
                    result = lock_proposal.activate_signed_lock_proposal(
                        str(proposal), str(bundle), str(lock), run=run, evidence_dir=str(root)
                    )
                    self.assertNotIn("ORLIX_COSIGN_KEY", os.environ)
            except ValueError:
                self.assertEqual(lock.read_bytes(), before)
                raise
            return result

    def test_activation_requires_only_public_key_and_existing_signed_proposal(self) -> None:
        run = mock.Mock(return_value=subprocess.CompletedProcess([], 0, ""))
        lock = self._activate(_schema2_proposal(), run)
        self.assertEqual(lock["schema"], 2)
        self.assertEqual(set(lock["components"]), set(locked_buildset.required_components(2)))
        self.assertEqual(run.call_args.args[0][0:2], ["cosign", "verify-blob"])

    def test_activation_rejects_invalid_signed_input_without_changing_lock(self) -> None:
        mutations = {
            "schema": lambda payload: payload.__setitem__("schema", 1),
            "component set": lambda payload: payload["components"].pop("rootfs"),
            "OCI digest": lambda payload: payload["components"]["uapi"].__setitem__("oci_digest", "sha256:short"),
            "artifact identity": lambda payload: payload["components"]["kernel-release-iphoneos"]["artifact_identity"].__setitem__("digest", "short"),
            "legacy identity": lambda payload: payload["components"]["uapi"].__setitem__(
                "artifact_identity",
                {"format": "legacy-marker-sha256", "version": 1, "digest": "ab" * 32, "marker": "uapi.sha256"},
            ),
        }
        for name, mutate in mutations.items():
            with self.subTest(name=name):
                payload = _schema2_proposal()
                mutate(payload)
                run = mock.Mock(return_value=subprocess.CompletedProcess([], 0, ""))
                with self.assertRaises(ValueError):
                    self._activate(payload, run)
                run.assert_called_once()

        for name, output in (("signature", "invalid signature"), ("bundle", "invalid bundle")):
            with self.subTest(name=name):
                run = mock.Mock(
                    side_effect=subprocess.CalledProcessError(1, "cosign", output=output)
                )
                with self.assertRaisesRegex(ValueError, output):
                    self._activate(_schema2_proposal(), run)

    def test_activation_rejects_evidence_mismatch_without_changing_lock(self) -> None:
        cases = {
            "absent file": lambda root, payload: (root / "uapi" / "uapi-sbom.json").unlink(),
            "digest mismatch": lambda root, payload: (
                root / "mlibc" / "mlibc-in-toto.json"
            ).write_text("{}\n", encoding="utf-8"),
            "wrong schema": lambda root, payload: (
                root / "promotion-proof-index.json"
            ).write_text(
                json.dumps({"schema": 2, "kind": "promotion-proof-index"}) + "\n", encoding="utf-8"
            ),
            "wrong names": lambda root, payload: payload["evidence"]["components"].pop("rootfs"),
        }
        for name, break_evidence in cases.items():
            with self.subTest(name=name):
                payload = _schema2_proposal()
                run = mock.Mock(return_value=subprocess.CompletedProcess([], 0, ""))
                with tempfile.TemporaryDirectory() as tmp:
                    root = Path(tmp)
                    self._evidence(root, payload)
                    break_evidence(root, payload)
                    (root / "proposal.json").write_text(json.dumps(payload) + "\n", encoding="utf-8")
                    (root / "proposal.sigstore.json").write_text("{}\n", encoding="utf-8")
                    lock = root / "artifacts.lock.json"
                    lock.write_text(json.dumps(lock_proposal.EMPTY_LOCK) + "\n", encoding="utf-8")
                    before = lock.read_bytes()
                    verify = lambda item: locked_buildset.validate_component(
                        item["component"], item, schema=item["schema"]
                    )
                    with mock.patch.dict(os.environ, {"ORLIX_COSIGN_PUB": "/public.pub"}, clear=True), \
                         mock.patch("lock_proposal.shutil.which", return_value="/usr/bin/cosign"), \
                         mock.patch("lock_proposal.trusted_public_key", return_value="/public.pub"), \
                         mock.patch("lock_proposal.verification_context", return_value=_VERIFICATION), \
                         mock.patch("lock_proposal.verify_component", side_effect=verify):
                        with self.assertRaises(ValueError):
                            lock_proposal.activate_signed_lock_proposal(
                                str(root / "proposal.json"),
                                str(root / "proposal.sigstore.json"),
                                str(lock),
                                run=run,
                                evidence_dir=str(root),
                            )
                    self.assertEqual(lock.read_bytes(), before)

    def test_forged_signed_flag_cannot_change_lock(self) -> None:
        component = {
            "unsigned_digest": "ab" * 32,
            "oci_digest": "sha256:" + "cd" * 32,
            "oci_reference": "ghcr.io/rudironsoni/orlix/uapi@sha256:" + "cd" * 32,
        }
        components = {name: {**component, "oci_reference": component["oci_reference"].replace("/uapi@", f"/{name}@")} for name in locked_buildset.REQUIRED}
        with tempfile.TemporaryDirectory() as tmp:
            lock = Path(tmp) / "artifacts.lock.json"
            proposal = Path(tmp) / "proposal.json"
            lock.write_text(json.dumps(lock_proposal.EMPTY_LOCK))
            before = lock.read_bytes()
            proposal.write_text(json.dumps({
                "signed": True,
                "buildset": locked_buildset.buildset_digest(components),
                "components": components,
            }))
            with mock.patch.dict(os.environ, {"ORLIX_ORAS_REGISTRY_CONFIG": ""}), \
                 mock.patch("publish.trusted_public_key", return_value="/unused.pub"), \
                 mock.patch("publish.shutil.which", return_value="cosign"), \
                 mock.patch("publish.subprocess.run", side_effect=subprocess.CalledProcessError(1, "cosign", output="invalid signature")):
                with self.assertRaisesRegex(ValueError, "invalid signature"):
                    lock_proposal.apply_lock_proposal(str(proposal), str(lock))
            self.assertEqual(lock.read_bytes(), before)

    def test_unsigned_proposal_does_not_set_signed_buildset(self) -> None:
        digest = "12" * 32
        with tempfile.TemporaryDirectory() as tmp:
            proposal_path = Path(tmp) / "lock-proposal.json"
            lock_path = Path(tmp) / "artifacts.lock.json"
            lock_path.write_text(json.dumps(lock_proposal.EMPTY_LOCK) + "\n", encoding="utf-8")
            payload = lock_proposal.write_lock_proposal(str(proposal_path), "uapi", digest)
            self.assertIs(payload["signed"], False)
            self.assertIsNone(payload["buildset"])
            self.assertIsNone(payload["oci_digest"])
            self.assertEqual(payload["components"]["uapi"]["unsigned_digest"], digest)
            before = lock_path.read_text(encoding="utf-8")
            with self.assertRaises(ValueError):
                lock_proposal.apply_lock_proposal(str(proposal_path), str(lock_path))
            self.assertEqual(lock_path.read_text(encoding="utf-8"), before)
            empty = lock_proposal.assert_lock_unsigned_empty(str(lock_path))
            self.assertIsNone(empty["buildset"])
            self.assertEqual(empty["components"], {})

    def test_unsigned_proposals_match_lock_and_do_not_apply(self) -> None:
        digest = "12" * 32
        with tempfile.TemporaryDirectory() as tmp:
            proposal_path = Path(tmp) / "uapi-lock-proposal.json"
            lock_path = Path(tmp) / "artifacts.lock.json"
            lock_path.write_text(
                json.dumps(
                    {
                        "schema": 1,
                        "buildset": "ab" * 32,
                        "components": {"uapi": {"unsigned_digest": digest}},
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            lock_proposal.write_lock_proposal(str(proposal_path), "uapi", digest)
            before = lock_path.read_text(encoding="utf-8")
            lock_proposal.assert_unsigned_lock_proposals([str(proposal_path)], str(lock_path))
            lock_proposal.write_lock_proposal(
                str(proposal_path),
                "uapi",
                digest,
                artifact_identity={
                    "format": "legacy-marker-sha256",
                    "version": 1,
                    "digest": digest,
                    "marker": "uapi.sha256",
                },
            )
            lock_proposal.assert_unsigned_lock_proposals([str(proposal_path)], str(lock_path))
            self.assertEqual(lock_path.read_text(encoding="utf-8"), before)

    def test_invalid_digest_fails_loud(self) -> None:
        with self.assertRaises(ValueError):
            lock_proposal.write_lock_proposal("/unused.json", "uapi", "short")

    @mock.patch("lock_proposal.verification_context", return_value={
        "signing_key_fingerprint": "11" * 32,
        "trust_policy_sha256": "22" * 32,
        "verification_policy_version": 1,
    })
    @mock.patch("lock_proposal.verify_component", side_effect=lambda p: locked_buildset.validate_component(p["component"], p))
    def test_signed_components_write_stable_buildset_and_can_apply(self, verify, verification) -> None:
        unsigned = "ab" * 32
        oci = "sha256:" + ("cd" * 32)
        reference = f"ghcr.io/rudironsoni/orlix/uapi@{oci}"
        with tempfile.TemporaryDirectory() as tmp:
            signed_path = Path(tmp) / "uapi-signed.json"
            signed_path.write_text(
                json.dumps(
                    {
                        "schema": 1,
                        "component": "uapi",
                        "unsigned_digest": unsigned,
                        "signed": True,
                        "oci_digest": oci,
                        "oci_reference": reference,
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            signed_paths = [str(signed_path)]
            for name in ("mlibc", "rootfs"):
                payload = json.loads(signed_path.read_text())
                payload["component"] = name
                payload["oci_reference"] = reference.replace("/uapi@", f"/{name}@")
                path = Path(tmp) / f"{name}-signed.json"
                path.write_text(json.dumps(payload))
                signed_paths.append(str(path))
            out1 = Path(tmp) / "lock-1.json"
            out2 = Path(tmp) / "lock-2.json"
            first = lock_proposal.write_signed_lock_proposal(str(out1), signed_paths)
            second = lock_proposal.write_signed_lock_proposal(str(out2), signed_paths)
            self.assertIs(first["signed"], True)
            self.assertEqual(first["buildset"], second["buildset"])
            self.assertEqual(len(first["buildset"]), 64)
            self.assertEqual(first["components"]["uapi"]["oci_digest"], oci)
            self.assertEqual(first["components"]["uapi"]["oci_reference"], reference)
            self.assertEqual(first["component_types"]["uapi"], "uapi")
            self.assertEqual(first["verification"]["trust_policy_sha256"], "22" * 32)
            lock_path = Path(tmp) / "artifacts.lock.json"
            lock_path.write_text(json.dumps(lock_proposal.EMPTY_LOCK) + "\n", encoding="utf-8")
            lock_proposal.apply_lock_proposal(str(out1), str(lock_path))
            lock = json.loads(lock_path.read_text(encoding="utf-8"))
            self.assertEqual(lock["buildset"], first["buildset"])
            self.assertEqual(lock["components"]["uapi"]["unsigned_digest"], unsigned)
            self.assertNotIn("kind", lock)
            self.assertEqual(verify.call_count, 9)

    def test_unsigned_signed_json_cannot_enter_signed_lock(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            signed_path = Path(tmp) / "uapi-signed.json"
            signed_path.write_text(
                json.dumps(
                    {
                        "schema": 1,
                        "component": "uapi",
                        "unsigned_digest": "ab" * 32,
                        "signed": False,
                        "oci_digest": None,
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            with self.assertRaises(ValueError):
                lock_proposal.write_signed_lock_proposal(str(Path(tmp) / "out.json"), [str(signed_path)])


if __name__ == "__main__":
    unittest.main()
