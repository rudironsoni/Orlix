from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

import sbom


def _entries():
    return [
        {"content_sha256": "00" * 32, "mode": 0o444, "path": "include/a.h", "type": "file"},
        {"content_sha256": "11" * 32, "mode": 0o755, "path": "bin/tool", "type": "file"},
    ]


class SbomTests(unittest.TestCase):
    def test_cyclonedx_inventory_is_deterministic(self) -> None:
        digest = "ab" * 32
        with tempfile.TemporaryDirectory() as tmp:
            first = Path(tmp) / "first.json"
            second = Path(tmp) / "second.json"
            sbom.write_sbom(str(first), "uapi", digest, actual_digest=digest, manifest_entries=_entries())
            sbom.write_sbom(str(second), "uapi", digest, actual_digest=digest, manifest_entries=_entries())
            self.assertEqual(first.read_bytes(), second.read_bytes())

    def test_cyclonedx_shape_names_software_only(self) -> None:
        digest = "ab" * 32
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "sbom.json"
            payload = sbom.write_sbom(str(path), "uapi", digest, actual_digest=digest, manifest_entries=_entries())
            self.assertEqual(payload["bomFormat"], "CycloneDX")
            self.assertEqual(payload["specVersion"], "1.6")
            self.assertNotIn("timestamp", json.dumps(payload))
            self.assertEqual(payload["metadata"]["component"]["name"], "uapi")
            self.assertEqual(payload["metadata"]["component"]["version"], digest)
            names = [item["name"] for item in payload["components"]]
            self.assertEqual(names, ["bin/tool", "include/a.h"])
            for item in payload["components"]:
                self.assertEqual(item["type"], "file")
                self.assertIn("hashes", item)
            text = path.read_text(encoding="utf-8")
            self.assertNotIn("cosign", text.lower())
            self.assertNotIn("workflow", text.lower())

    def test_serial_derives_from_component_identity_not_buildset(self) -> None:
        first = sbom.component_serial("uapi", "ab" * 32)
        second = sbom.component_serial("uapi", "ab" * 32)
        third = sbom.component_serial("mlibc", "ab" * 32)
        self.assertEqual(first, second)
        self.assertNotEqual(first, third)
        self.assertTrue(first.startswith("urn:uuid:"))

    def test_mismatched_or_missing_subject_fails_loud(self) -> None:
        with self.assertRaises(ValueError):
            sbom.bind_subject("ab" * 32, "cd" * 32)
        with self.assertRaises(ValueError):
            sbom.write_sbom("/unused.json", "uapi", "not-a-digest")
        with self.assertRaises(ValueError):
            sbom.write_sbom("/unused.json", "uapi", "ab" * 32, actual_digest="cd" * 32)
        with self.assertRaises(ValueError):
            sbom.write_sbom("/unused.json", "", "ab" * 32)
        with self.assertRaises(ValueError):
            sbom.write_sbom("/unused.json", "not-a-component", "ab" * 32, manifest_entries=[])
        with self.assertRaises(ValueError):
            sbom.write_sbom(
                "/unused.json", "uapi", "ab" * 32,
                manifest_entries=[{"type": "directory", "path": "include"}],
            )


if __name__ == "__main__":
    unittest.main()
