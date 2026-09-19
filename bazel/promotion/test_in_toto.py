from __future__ import annotations

import json
import os
import tempfile
import unittest
from pathlib import Path

import in_toto


def _context(**overrides):
    context = {
        "source_sha": "ab" * 20,
        "builder_id": "https://github.com/rudironsoni/Orlix/.github/workflows/bazel-promote.yml",
        "invocation_id": "123",
        "build_config": ["release", "promotion"],
        "toolchain_digest": "cd" * 32,
        "materials": [
            {"uri": "orlix:promotion:build-tree:a", "digest": {"sha256": "ef" * 32}},
            {"uri": "orlix:promotion:build-tree:b", "digest": {"sha256": "ef" * 32}},
        ],
    }
    context.update(overrides)
    return context


class InTotoTests(unittest.TestCase):
    def test_slsa_provenance_binds_subject_and_context(self) -> None:
        digest = "ef" * 32
        env = os.environ.pop("ORLIX_COSIGN_KEY", None)
        try:
            with tempfile.TemporaryDirectory() as tmp:
                path = Path(tmp) / "in-toto.json"
                payload = in_toto.write_provenance(str(path), "uapi", digest, **_context())
                self.assertEqual(payload["_type"], "https://in-toto.io/Statement/v1")
                self.assertEqual(payload["predicateType"], "https://slsa.dev/provenance/v1")
                self.assertEqual(payload["subject"], [{"name": "uapi", "digest": {"sha256": digest}}])
                predicate = payload["predicate"]
                self.assertEqual(
                    predicate["buildDefinition"]["externalParameters"]["source"], "ab" * 20
                )
                self.assertEqual(
                    predicate["buildDefinition"]["externalParameters"]["config"],
                    ["promotion", "release"],
                )
                self.assertEqual(
                    predicate["runDetails"]["builder"]["id"],
                    "https://github.com/rudironsoni/Orlix/.github/workflows/bazel-promote.yml",
                )
                self.assertEqual(predicate["runDetails"]["metadata"]["invocationId"], "123")
                self.assertEqual(payload["toolchain_digest"], "cd" * 32)
                self.assertIs(payload["signed"], False)
                self.assertIsNone(payload["oci_digest"])
                self.assertNotIn("signature", payload)
                loaded = json.loads(path.read_text(encoding="utf-8"))
                self.assertEqual(loaded["predicateType"], "https://slsa.dev/provenance/v1")
        finally:
            if env is not None:
                os.environ["ORLIX_COSIGN_KEY"] = env

    def test_missing_context_fails_loud(self) -> None:
        digest = "ef" * 32
        with self.assertRaises(ValueError):
            in_toto.write_provenance("/unused.json", "uapi", digest)
        with self.assertRaises(ValueError):
            in_toto.write_provenance("/unused.json", "uapi", digest, **_context(source_sha="short"))
        with self.assertRaises(ValueError):
            in_toto.write_provenance("/unused.json", "uapi", digest, **_context(builder_id=""))
        with self.assertRaises(ValueError):
            in_toto.write_provenance("/unused.json", "uapi", digest, **_context(toolchain_digest="nope"))
        with self.assertRaises(ValueError):
            in_toto.write_provenance("/unused.json", "uapi", digest, **_context(materials=[]))
        with self.assertRaises(ValueError):
            in_toto.write_provenance("/unused.json", "uapi", "nope", **_context())

    def test_deterministic_bytes(self) -> None:
        digest = "ef" * 32
        with tempfile.TemporaryDirectory() as tmp:
            first = Path(tmp) / "first.json"
            second = Path(tmp) / "second.json"
            in_toto.write_provenance(str(first), "uapi", digest, **_context())
            in_toto.write_provenance(str(second), "uapi", digest, **_context())
            self.assertEqual(first.read_bytes(), second.read_bytes())


if __name__ == "__main__":
    unittest.main()
