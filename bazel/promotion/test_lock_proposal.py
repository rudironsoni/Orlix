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
_MARKERS = {"uapi": "uapi.sha256", "mlibc": "sysroot.sha256", "rootfs": "source-input.sha256"}

def _schema2_proposal() -> dict:
    components = {}
    for name in locked_buildset.required_components(locked_buildset.SCHEMA2):
        artifact_digest = hashlib.sha256(f"{name}:artifact".encode()).hexdigest()
        oci_digest = "sha256:" + hashlib.sha256(f"{name}:oci".encode()).hexdigest()
        legacy = name in _MARKERS
        identity = {"format": "legacy-marker-sha256" if legacy else "artifact-identity-v2", "version": 1 if legacy else 2, "digest": artifact_digest}
        if legacy:
            identity["marker"] = _MARKERS[name]
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
    def _activate(self, payload: dict, run: mock.Mock) -> dict:
        with tempfile.TemporaryDirectory() as tmp:
            proposal = Path(tmp) / "proposal.json"
            bundle = Path(tmp) / "proposal.sigstore.json"
            lock = Path(tmp) / "artifacts.lock.json"
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
                        str(proposal), str(bundle), str(lock), run=run
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
