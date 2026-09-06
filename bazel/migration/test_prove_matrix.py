from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

import prove_matrix


class ProveMatrixTests(unittest.TestCase):
    def setUp(self) -> None:
        self.matrix = json.loads(Path(__file__).with_name("apple-build-matrix.json").read_text(encoding="utf-8"))

    def test_supported_row_records_evidence(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "ipa"
            path.write_bytes(b"ipa")
            payload = prove_matrix.prove_rows(
                self.matrix,
                {"ios-15.0-app-sdk-compile": str(path)},
            )
            self.assertFalse(payload["complete"])
            self.assertNotIn("ios-15.5-ci-runtime", payload["gated"])
            self.assertEqual(payload["proved"][0]["id"], "ios-15.0-app-sdk-compile")

    def test_gated_row_cannot_pass(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "log"
            path.write_text("nope\n", encoding="utf-8")
            with self.assertRaises(prove_matrix.ProveError):
                prove_matrix.prove_rows(self.matrix, {"signing-distribution": str(path)})

    def test_ios_15_5_local_log_can_be_proved(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "ios15.log"
            path.write_text("TEST EXECUTE SUCCEEDED\n", encoding="utf-8")
            payload = prove_matrix.prove_rows(self.matrix, {"ios-15.5-ci-runtime": str(path)})
            self.assertEqual(payload["proved"][0]["id"], "ios-15.5-ci-runtime")


if __name__ == "__main__":
    unittest.main()
