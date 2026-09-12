from __future__ import annotations

import hashlib
import os
import tempfile
import unittest
import subprocess
from pathlib import Path
from unittest import mock

import sign
from bazel.content_digest import artifact_manifest_v2
from locked_buildset import V2_DIGEST_FILENAME, V2_MANIFEST_FILENAME


class SignTests(unittest.TestCase):
    def test_other_registries_are_rejected_before_push(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            artifact = Path(tmp) / "artifact"
            artifact.write_bytes(b"component")
            run = mock.Mock()
            with mock.patch.dict(os.environ, {"ORLIX_COSIGN_KEY": "file:///unused"}), \
                 mock.patch("sign.shutil.which", return_value="/usr/bin/tool"):
                for repository in ("localhost:5001/orlix", "ghcr.io/example/orlix"):
                    with self.subTest(repository=repository), self.assertRaises(sign.SignError):
                        sign.sign_digest("ab" * 32, "uapi", artifact=str(artifact), repository=repository, run=run)
            run.assert_not_called()

    def test_unset_key_does_not_invent_signature(self) -> None:
        os.environ.pop("ORLIX_COSIGN_KEY", None)
        with self.assertRaises(sign.SignError) as raised:
            sign.sign_digest("ab" * 32, "uapi")
        self.assertIn("ORLIX_COSIGN_KEY", str(raised.exception))

    def test_missing_cosign_does_not_invent_signature(self) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"
        try:
            with mock.patch("sign.shutil.which", return_value=None):
                with self.assertRaises(sign.SignError) as raised:
                    sign.sign_digest("cd" * 32, "mlibc")
            self.assertIn("cosign is required", str(raised.exception))
        finally:
            os.environ.pop("ORLIX_COSIGN_KEY", None)

    def test_missing_artifact_does_not_invent_oci_digest(self) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"
        os.environ.pop("ORLIX_PROMOTE_ARTIFACT", None)
        try:
            with mock.patch("sign.shutil.which", return_value="/usr/bin/cosign"):
                with self.assertRaises(sign.SignError) as raised:
                    sign.sign_digest("ef" * 32, "rootfs")
            self.assertIn("artifact path is required", str(raised.exception))
        finally:
            os.environ.pop("ORLIX_COSIGN_KEY", None)

    def test_oras_digest_is_recorded_not_invented(self) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"
        observed = "11" * 32
        calls: list[list[str]] = []

        class Result:
            def __init__(self, stdout: str) -> None:
                self.stdout = stdout

        def fake_run(argv: list[str], env=None, cwd=None):
            calls.append(list(argv))
            if argv[0] == "oras" and "push" in argv:
                return Result(f"Pushed [registry] ghcr.io/rudironsoni/orlix/uapi\nDigest: sha256:{observed}\n")
            if argv[0] == "cosign" and "sign" in argv:
                return Result("")
            raise AssertionError(argv)

        with tempfile.TemporaryDirectory() as tmp:
            artifact = Path(tmp) / "uapi.sha256"
            artifact.write_text("ab" * 32 + "\n", encoding="utf-8")
            try:
                with mock.patch("sign.shutil.which", return_value="/usr/bin/tool"):
                    payload = sign.sign_digest(
                        "ab" * 32,
                        "uapi",
                        artifact=str(artifact),
                        repository="ghcr.io/rudironsoni/orlix",
                        run=fake_run,
                    )
            finally:
                os.environ.pop("ORLIX_COSIGN_KEY", None)
        self.assertTrue(payload["signed"])
        self.assertEqual(payload["oci_digest"], f"sha256:{observed}")
        self.assertEqual(payload["oci_reference"], f"ghcr.io/rudironsoni/orlix/uapi@sha256:{observed}")
        self.assertTrue(any(call[0] == "oras" and "push" in call for call in calls))
        self.assertTrue(any(call[0] == "cosign" and "sign" in call for call in calls))
        self.assertTrue(
            any(
                "--annotation" in call
                and "org.opencontainers.image.source=https://github.com/rudironsoni/Orlix" in call
                for call in calls
            )
        )

    def test_v2_signing_recomputes_the_declared_product_identity(self) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"
        observed = "11" * 32

        class Result:
            def __init__(self, stdout: str) -> None:
                self.stdout = stdout

        def fake_run(argv: list[str], env=None, cwd=None):
            if argv[0] == "oras":
                return Result("Digest: sha256:" + observed + "\n")
            if argv[0] == "cosign":
                return Result("")
            raise AssertionError(argv)

        with tempfile.TemporaryDirectory() as tmp:
            artifact = Path(tmp) / "component"
            product = artifact / "product"
            product.mkdir(parents=True)
            payload = product / "OrlixKernel.a"
            payload.write_bytes(b"kernel")
            dts = product / "arch" / "orlix" / "boot" / "dts"
            dts.mkdir(parents=True)
            development_dtb = dts / "development.dtb"
            release_dtb = dts / "release.dtb"
            development_dtb.write_bytes(b"development")
            release_dtb.write_bytes(b"release")
            manifest = artifact_manifest_v2(
                artifacts={
                    "OrlixKernel.a": payload,
                    "arch/orlix/boot/dts/development.dtb": development_dtb,
                    "arch/orlix/boot/dts/release.dtb": release_dtb,
                }
            )
            (artifact / V2_MANIFEST_FILENAME).write_bytes(manifest)
            digest = hashlib.sha256(manifest).hexdigest()
            (artifact / V2_DIGEST_FILENAME).write_text(digest + "\n", encoding="ascii")
            try:
                with mock.patch("sign.shutil.which", return_value="/usr/bin/tool"):
                    result = sign.sign_digest(
                        digest,
                        "kernel-release-iphoneos",
                        artifact=str(artifact),
                        repository="ghcr.io/rudironsoni/orlix",
                        identity_format="artifact-identity-v2",
                        run=fake_run,
                    )
                    (artifact / "symbols.txt").write_text("proof", encoding="utf-8")
                    with self.assertRaisesRegex(
                        sign.SignError,
                        "outside its product boundary",
                    ):
                        sign.sign_digest(
                            digest,
                            "kernel-release-iphoneos",
                            artifact=str(artifact),
                            repository="ghcr.io/rudironsoni/orlix",
                            identity_format="artifact-identity-v2",
                            run=fake_run,
                        )
            finally:
                os.environ.pop("ORLIX_COSIGN_KEY", None)
        self.assertEqual(result["schema"], 2)
        self.assertEqual(result["artifact_identity"]["digest"], digest)

    def test_public_password_env_is_mapped_for_cosign(self) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"
        os.environ["ORLIX_COSIGN_KEY_PASSWORD"] = "test-password-not-real"
        seen: dict[str, str | None] = {}

        class Result:
            def __init__(self, stdout: str) -> None:
                self.stdout = stdout

        def fake_run(argv: list[str], env=None, cwd=None):
            if argv[0] == "oras" and "push" in argv:
                return Result("Digest: sha256:" + ("11" * 32) + "\n")
            if argv[0] == "cosign" and "sign" in argv:
                seen["password"] = None if env is None else env.get("COSIGN_PASSWORD")
                return Result("")
            raise AssertionError(argv)

        with tempfile.TemporaryDirectory() as tmp:
            artifact = Path(tmp) / "uapi.sha256"
            artifact.write_text("ab" * 32 + "\n", encoding="utf-8")
            try:
                with mock.patch("sign.shutil.which", return_value="/usr/bin/tool"):
                    payload = sign.sign_digest(
                        "ab" * 32,
                        "uapi",
                        artifact=str(artifact),
                        repository="ghcr.io/rudironsoni/orlix",
                        run=fake_run,
                    )
            finally:
                os.environ.pop("ORLIX_COSIGN_KEY", None)
                os.environ.pop("ORLIX_COSIGN_KEY_PASSWORD", None)
        self.assertTrue(payload["signed"])
        self.assertEqual(seen.get("password"), "test-password-not-real")

    def test_missing_oras_does_not_invent_oci_digest(self) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"
        try:
            with mock.patch(
                "sign.shutil.which",
                side_effect=lambda name: "/usr/bin/cosign" if name == "cosign" else None,
            ):
                with tempfile.TemporaryDirectory() as tmp:
                    artifact = Path(tmp) / "uapi.sha256"
                    artifact.write_text("ab" * 32 + "\n", encoding="utf-8")
                    with self.assertRaises(sign.SignError) as raised:
                        sign.sign_digest("ab" * 32, "uapi", artifact=str(artifact))
            self.assertIn("oras is required", str(raised.exception))
        finally:
            os.environ.pop("ORLIX_COSIGN_KEY", None)

    @mock.patch("sign.subprocess.run", side_effect=subprocess.CalledProcessError(1, "oras", output="denied: permission_denied"))
    def test_ghcr_push_failure_does_not_invent_oci_digest(self, run) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"
        with tempfile.TemporaryDirectory() as tmp:
            artifact = Path(tmp) / "uapi.sha256"
            artifact.write_text("ab" * 32 + "\n", encoding="utf-8")
            proposal = Path(tmp) / "signed.json"
            try:
                with self.assertRaises(sign.SignError) as raised:
                    sign.write_signed_proposal(
                        str(proposal),
                        "uapi",
                        "ab" * 32,
                        artifact=str(artifact),
                        repository="ghcr.io/rudironsoni/orlix",
                    )
                message = str(raised.exception)
                self.assertTrue(
                    "oras failed" in message or "ghcr.io" in message,
                    message,
                )
                self.assertNotIn('"signed": true', message)
                self.assertFalse(proposal.exists())
            finally:
                os.environ.pop("ORLIX_COSIGN_KEY", None)

    def test_oras_without_digest_line_does_not_invent_oci_digest(self) -> None:
        os.environ["ORLIX_COSIGN_KEY"] = "file:///unused"

        class Result:
            stdout = "Pushed [registry] ghcr.io/rudironsoni/orlix/uapi\n"

        def fake_run(argv: list[str], env=None, cwd=None):
            if argv[0] == "oras":
                return Result()
            raise AssertionError(argv)

        with tempfile.TemporaryDirectory() as tmp:
            artifact = Path(tmp) / "uapi.sha256"
            artifact.write_text("ab" * 32 + "\n", encoding="utf-8")
            try:
                with mock.patch("sign.shutil.which", return_value="/usr/bin/tool"):
                    with self.assertRaises(sign.SignError) as raised:
                        sign.sign_digest(
                            "ab" * 32,
                            "uapi",
                            artifact=str(artifact),
                            repository="ghcr.io/rudironsoni/orlix",
                            run=fake_run,
                        )
                self.assertIn("refusing to invent oci_digest", str(raised.exception))
            finally:
                os.environ.pop("ORLIX_COSIGN_KEY", None)


if __name__ == "__main__":
    unittest.main()
