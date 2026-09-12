from __future__ import annotations

import hashlib
import io
import json
import os
import shutil
import tarfile
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import locked_buildset
import lock_proposal
import reconstruct
from bazel.content_digest import artifact_manifest_v2


_VERIFICATION = {
    "signing_key_fingerprint": "11" * 32,
    "trust_policy_sha256": "22" * 32,
    "verification_policy_version": 1,
}


def _lock_payload(reference: str) -> dict:
    payload = {
        "schema": 1,
        "buildset": "ff" * 32,
        "components": {
            "uapi": {
                "unsigned_digest": "aa" * 32,
                "oci_digest": "sha256:" + ("ab" * 32),
                "oci_reference": reference,
            }
        },
    }
    payload["buildset"] = locked_buildset.buildset_digest(payload["components"])
    return payload


class ReconstructTests(unittest.TestCase):
    def test_component_tar_preserves_directories_and_cannot_escape(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            blob = Path(tmp) / "component.tar"
            payload = b"escape"
            with tarfile.open(blob, "w") as archive:
                first = tarfile.TarInfo("a")
                first.type = tarfile.SYMTYPE
                first.linkname = "."
                archive.addfile(first)
                second = tarfile.TarInfo("b")
                second.type = tarfile.SYMTYPE
                second.linkname = "a/.."
                archive.addfile(second)
                member = tarfile.TarInfo("b/escaped")
                member.size = len(payload)
                archive.addfile(member, io.BytesIO(payload))
            with self.assertRaises(reconstruct.ReconstructError) as raised:
                reconstruct._extract_component_tar(blob, Path(tmp) / "tree")
            self.assertFalse((Path(tmp) / "escaped").exists())
            self.assertIn("unsafe component tar link", str(raised.exception))

            valid = Path(tmp) / "valid.tar"
            with tarfile.open(valid, "w") as archive:
                directory = tarfile.TarInfo("include")
                directory.type = tarfile.DIRTYPE
                directory.mode = 0o555
                archive.addfile(directory)
                member = tarfile.TarInfo("include/a.h")
                member.size = len(payload)
                archive.addfile(member, io.BytesIO(payload))
            destination = Path(tmp) / "valid"
            reconstruct._extract_component_tar(valid, destination)
            self.assertEqual((destination / "include" / "a.h").read_bytes(), payload)
            self.assertEqual((destination / "include").stat().st_mode & 0o777, 0o555)

    def test_empty_lock_cannot_reconstruct(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "artifacts.lock.json"
            path.write_text(json.dumps({"schema": 1, "buildset": None, "components": {}}) + "\n")
            with self.assertRaises(reconstruct.ReconstructError):
                reconstruct.reconstruct(str(path), tmp)

    def test_missing_oras_fails_closed(self) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"
        try:
            with tempfile.TemporaryDirectory() as tmp:
                path = Path(tmp) / "artifacts.lock.json"
                path.write_text(json.dumps(_lock_payload("ghcr.io/rudironsoni/orlix/uapi@sha256:" + ("ab" * 32))) + "\n")
                with mock.patch("reconstruct.verification_context", return_value=_VERIFICATION), mock.patch(
                    "reconstruct.shutil.which",
                    side_effect=lambda name: None if name == "oras" else "/usr/bin/cosign",
                ):
                    with self.assertRaises(reconstruct.ReconstructError) as raised:
                        reconstruct.reconstruct(str(path), tmp, store_root=Path(tmp) / "store")
            self.assertIn("oras is required", str(raised.exception))
        finally:
            os.environ.pop("ORLIX_COSIGN_KEY", None)

    def test_missing_cosign_fails_closed(self) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"
        try:
            with tempfile.TemporaryDirectory() as tmp:
                path = Path(tmp) / "artifacts.lock.json"
                path.write_text(json.dumps(_lock_payload("ghcr.io/rudironsoni/orlix/uapi@sha256:" + ("ab" * 32))) + "\n")
                with mock.patch("reconstruct.verification_context", return_value=_VERIFICATION), mock.patch(
                    "reconstruct.shutil.which",
                    side_effect=lambda name: None if name == "cosign" else "/usr/bin/oras",
                ):
                    with self.assertRaises(reconstruct.ReconstructError) as raised:
                        reconstruct.reconstruct(str(path), tmp, store_root=Path(tmp) / "store")
            self.assertIn("cosign is required", str(raised.exception))
        finally:
            os.environ.pop("ORLIX_COSIGN_KEY", None)

    def test_missing_key_fails_closed(self) -> None:
        os.environ.pop("ORLIX_COSIGN_KEY", None)
        os.environ.pop("ORLIX_COSIGN_PUB", None)
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "artifacts.lock.json"
            path.write_text(json.dumps(_lock_payload("ghcr.io/rudironsoni/orlix/uapi@sha256:" + ("ab" * 32))) + "\n")
            with mock.patch("reconstruct.shutil.which", return_value="/usr/bin/tool"):
                with self.assertRaises(reconstruct.ReconstructError) as raised:
                    reconstruct.reconstruct(str(path), tmp)
        self.assertIn("ORLIX_COSIGN_PUB", str(raised.exception))

    def test_localhost_registry_is_rejected(self) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"
        try:
            with tempfile.TemporaryDirectory() as tmp:
                path = Path(tmp) / "artifacts.lock.json"
                path.write_text(
                    json.dumps(
                        _lock_payload("localhost:5001/orlix/uapi@sha256:" + ("ab" * 32))
                    )
                    + "\n"
                )
                with mock.patch("reconstruct.shutil.which", return_value="/usr/bin/tool"):
                    with self.assertRaises(reconstruct.ReconstructError) as raised:
                        reconstruct.reconstruct(str(path), tmp)
            self.assertIn("ghcr.io/rudironsoni", str(raised.exception))
            self.assertIn("localhost", str(raised.exception))
        finally:
            os.environ.pop("ORLIX_COSIGN_KEY", None)

    def test_unsigned_lock_fails_closed(self) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"
        payload = {
            "schema": 1,
            "buildset": "ff" * 32,
            "components": {
                "uapi": {
                    "unsigned_digest": "aa" * 32,
                    "oci_digest": None,
                    "oci_reference": None,
                }
            },
        }
        try:
            with tempfile.TemporaryDirectory() as tmp:
                path = Path(tmp) / "artifacts.lock.json"
                path.write_text(json.dumps(payload) + "\n")
                with mock.patch("reconstruct.shutil.which", return_value="/usr/bin/tool"):
                    with self.assertRaises(reconstruct.ReconstructError) as raised:
                        reconstruct.reconstruct(str(path), tmp)
            self.assertIn("oci_digest", str(raised.exception))
        finally:
            os.environ.pop("ORLIX_COSIGN_KEY", None)

    @mock.patch("reconstruct.verification_context", return_value=_VERIFICATION)
    @mock.patch("publish.trusted_public_key", return_value="/unused.pub")
    def test_pulls_extracts_component_tar_and_verifies(self, public_key, verification) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"
        calls: list[list[str]] = []
        reference = "ghcr.io/rudironsoni/orlix/uapi@sha256:" + ("ab" * 32)

        def fake_run(argv: list[str], env=None, cwd=None):
            calls.append(list(argv))
            if argv[0] == "oras" and "pull" in argv:
                dest = Path(argv[argv.index("-o") + 1])
                blob = dest / "component.tar"
                with tarfile.open(blob, "w") as archive:
                    digest = dest / "uapi.sha256"
                    digest.write_text("aa" * 32 + "\n", encoding="utf-8")
                    archive.add(digest, arcname="uapi.sha256")
                    headers = dest / "include"
                    headers.mkdir()
                    (headers / "unistd.h").write_text("/* uapi */\n", encoding="utf-8")
                    archive.add(headers, arcname="include")

            class Result:
                stdout = ""

            return Result()

        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "artifacts.lock.json"
            path.write_text(json.dumps(_lock_payload(reference)) + "\n")
            out_dir = Path(tmp) / "reconstruct"
            try:
                with mock.patch("reconstruct.shutil.which", return_value="/usr/bin/tool"):
                    payload = reconstruct.reconstruct(
                        str(path), str(out_dir), run=fake_run, store_root=Path(tmp) / "store"
                    )
            finally:
                os.environ.pop("ORLIX_COSIGN_KEY", None)
            tree = Path(payload["components"]["uapi"]["tree"])
            self.assertTrue((tree / "include" / "unistd.h").is_file())
            self.assertEqual((tree / "uapi.sha256").read_text(encoding="utf-8").strip(), "aa" * 32)
        self.assertEqual(payload["components"]["uapi"]["oci_reference"], reference)
        self.assertTrue(any(call[0] == "oras" and "pull" in call and reference in call for call in calls))
        self.assertTrue(any(call[0] == "cosign" and "verify" in call and reference in call for call in calls))

    @mock.patch("reconstruct.verification_context", return_value=_VERIFICATION)
    @mock.patch("publish.trusted_public_key", return_value="/unused.pub")
    def test_raw_digest_blob_cannot_substitute(self, public_key, verification) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"
        reference = "ghcr.io/rudironsoni/orlix/uapi@sha256:" + ("ab" * 32)

        def fake_run(argv: list[str], env=None, cwd=None):
            if argv[0] == "oras" and "pull" in argv:
                dest = Path(argv[argv.index("-o") + 1])
                (dest / "component.tar").write_text("aa" * 32 + "\n", encoding="utf-8")

            class Result:
                stdout = ""

            return Result()

        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "artifacts.lock.json"
            path.write_text(json.dumps(_lock_payload(reference)) + "\n")
            try:
                with mock.patch("reconstruct.shutil.which", return_value="/usr/bin/tool"):
                    with self.assertRaises(reconstruct.ReconstructError) as raised:
                        reconstruct.reconstruct(
                            str(path), tmp, run=fake_run, store_root=Path(tmp) / "store"
                        )
            finally:
                os.environ.pop("ORLIX_COSIGN_KEY", None)
        self.assertIn("not a component tar", str(raised.exception))

    @mock.patch("reconstruct.verification_context", return_value=_VERIFICATION)
    @mock.patch("publish.trusted_public_key", return_value="/unused.pub")
    def test_warm_hit_has_no_network_tool_calls(self, public_key, verification) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"
        reference = "ghcr.io/rudironsoni/orlix/uapi@sha256:" + ("ab" * 32)
        calls: list[list[str]] = []

        def fake_run(argv: list[str], env=None, cwd=None):
            calls.append(list(argv))
            if argv[0] == "oras":
                dest = Path(argv[argv.index("-o") + 1])
                with tarfile.open(dest / "component.tar", "w") as archive:
                    marker = dest / "uapi.sha256"
                    marker.write_text("aa" * 32 + "\n", encoding="utf-8")
                    archive.add(marker, arcname="uapi.sha256")

            class Result:
                stdout = ""

            return Result()

        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "artifacts.lock.json"
            path.write_text(json.dumps(_lock_payload(reference)) + "\n")
            store = Path(tmp) / "store"
            try:
                with mock.patch("reconstruct.shutil.which", return_value="/usr/bin/tool"):
                    reconstruct.reconstruct(str(path), tmp, run=fake_run, store_root=store)
                calls.clear()
                with mock.patch("reconstruct.shutil.which", return_value=None):
                    reconstruct.reconstruct(str(path), str(Path(tmp) / "second"), run=fake_run, store_root=store)
            finally:
                os.environ.pop("ORLIX_COSIGN_KEY", None)
        self.assertEqual(calls, [])

    @mock.patch("reconstruct.verification_context", return_value=_VERIFICATION)
    @mock.patch("publish.trusted_public_key", return_value="/unused.pub")
    def test_schema2_buildset_applies_and_reuses_all_components(
        self, public_key, verification
    ) -> None:
        calls: list[list[str]] = []
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            tars = {}
            signed_paths = []
            for name in locked_buildset.required_components(locked_buildset.SCHEMA2):
                tree = root / "fixtures" / name
                tree.mkdir(parents=True)
                if name in locked_buildset.KERNEL_COMPONENTS:
                    product = tree / locked_buildset.V2_PRODUCT_DIRECTORY
                    files = {}
                    for relative in sorted(locked_buildset.KERNEL_PRODUCT_PATHS):
                        source = product / relative
                        source.parent.mkdir(parents=True, exist_ok=True)
                        source.write_text(f"{name}:{relative}\n", encoding="utf-8")
                        files[relative] = source
                    manifest = artifact_manifest_v2(artifacts=files)
                    digest = hashlib.sha256(manifest).hexdigest()
                    (tree / locked_buildset.V2_MANIFEST_FILENAME).write_bytes(manifest)
                    (tree / locked_buildset.V2_DIGEST_FILENAME).write_text(
                        digest + "\n", encoding="ascii"
                    )
                    identity = {
                        "format": locked_buildset.ARTIFACT_IDENTITY_V2_FORMAT,
                        "version": locked_buildset.ARTIFACT_IDENTITY_V2_VERSION,
                        "digest": digest,
                    }
                else:
                    digest = hashlib.sha256(name.encode("ascii")).hexdigest()
                    marker = f"{name}.sha256"
                    (tree / marker).write_text(digest + "\n", encoding="ascii")
                    identity = {
                        "format": locked_buildset.LEGACY_IDENTITY_FORMAT,
                        "version": locked_buildset.LEGACY_IDENTITY_VERSION,
                        "digest": digest,
                        "marker": marker,
                    }
                blob = root / "fixtures" / f"{name}.tar"
                with tarfile.open(blob, "w") as archive:
                    for source in sorted(tree.iterdir()):
                        archive.add(source, arcname=source.name)
                tars[name] = blob
                oci = "sha256:" + hashlib.sha256(
                    f"oci:{name}".encode("ascii")
                ).hexdigest()
                signed = {
                    "schema": locked_buildset.SCHEMA2,
                    "component": name,
                    "unsigned_digest": digest,
                    "artifact_identity": identity,
                    "signed": True,
                    "oci_digest": oci,
                    "oci_reference": f"{locked_buildset.GHCR_REPOSITORY}/{name}@{oci}",
                }
                signed_path = root / f"{name}-signed.json"
                signed_path.write_text(json.dumps(signed) + "\n", encoding="utf-8")
                signed_paths.append(str(signed_path))

            def validate(payload: dict) -> dict:
                return locked_buildset.validate_component(
                    payload["component"], payload, schema=payload.get("schema", 1)
                )

            proposal_path = root / "lock-proposal.json"
            lock_path = root / "artifacts.lock.json"
            lock_path.write_text(
                json.dumps(lock_proposal.EMPTY_LOCK) + "\n", encoding="utf-8"
            )
            with mock.patch("lock_proposal.verify_component", side_effect=validate):
                proposal = lock_proposal.write_signed_lock_proposal(
                    str(proposal_path), signed_paths
                )
                lock_proposal.apply_lock_proposal(str(proposal_path), str(lock_path))
            expected = set(locked_buildset.required_components(locked_buildset.SCHEMA2))
            self.assertEqual(proposal["schema"], locked_buildset.SCHEMA2)
            self.assertEqual(set(proposal["components"]), expected)

            def fake_run(argv: list[str], env=None, cwd=None):
                calls.append(list(argv))
                if argv[0] == "oras":
                    reference = next(
                        value for value in argv if value.startswith("ghcr.io/")
                    )
                    name = reference.rsplit("/", 1)[1].split("@", 1)[0]
                    dest = Path(argv[argv.index("-o") + 1])
                    shutil.copyfile(tars[name], dest / "component.tar")

                class Result:
                    stdout = ""

                return Result()

            store = root / "store"
            with mock.patch.dict(
                os.environ, {"ORLIX_ORAS_REGISTRY_CONFIG": ""}
            ), mock.patch("reconstruct.shutil.which", return_value="/usr/bin/tool"):
                first = reconstruct.reconstruct(
                    str(lock_path), str(root / "first"), run=fake_run, store_root=store
                )
            self.assertEqual(set(first["components"]), expected)
            self.assertEqual(sum(call[0] == "oras" for call in calls), 7)
            self.assertEqual(sum(call[0] == "cosign" for call in calls), 7)

            calls.clear()
            with mock.patch.dict(
                os.environ, {"ORLIX_ORAS_REGISTRY_CONFIG": ""}
            ), mock.patch("reconstruct.shutil.which", return_value=None):
                second = reconstruct.reconstruct(
                    str(lock_path), str(root / "second"), run=fake_run, store_root=store
                )
            self.assertEqual(first["buildset"], second["buildset"])
            self.assertEqual(calls, [])

    @mock.patch("publish.trusted_public_key", return_value="/unused.pub")
    def test_trust_policy_change_reverifies_without_oras_pull(self, public_key) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"
        reference = "ghcr.io/rudironsoni/orlix/uapi@sha256:" + ("ab" * 32)
        calls: list[list[str]] = []
        changed = {**_VERIFICATION, "trust_policy_sha256": "33" * 32}

        def fake_run(argv: list[str], env=None, cwd=None):
            calls.append(list(argv))
            if argv[0] == "oras":
                dest = Path(argv[argv.index("-o") + 1])
                with tarfile.open(dest / "component.tar", "w") as archive:
                    marker = dest / "uapi.sha256"
                    marker.write_text("aa" * 32 + "\n", encoding="utf-8")
                    archive.add(marker, arcname="uapi.sha256")

            class Result:
                stdout = ""

            return Result()

        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "artifacts.lock.json"
            path.write_text(json.dumps(_lock_payload(reference)) + "\n")
            store = Path(tmp) / "store"
            try:
                with mock.patch("reconstruct.verification_context", return_value=_VERIFICATION), mock.patch(
                    "reconstruct.shutil.which", return_value="/usr/bin/tool"
                ):
                    reconstruct.reconstruct(str(path), tmp, run=fake_run, store_root=store)
                calls.clear()
                with mock.patch("reconstruct.verification_context", return_value=changed), mock.patch(
                    "reconstruct.shutil.which",
                    side_effect=lambda name: None if name == "oras" else "/usr/bin/cosign",
                ):
                    reconstruct.reconstruct(
                        str(path), str(Path(tmp) / "second"), run=fake_run, store_root=store
                    )
            finally:
                os.environ.pop("ORLIX_COSIGN_KEY", None)
        self.assertTrue(any(call[0] == "cosign" for call in calls))
        self.assertFalse(any(call[0] == "oras" for call in calls))


if __name__ == "__main__":
    unittest.main()
