from __future__ import annotations

import hashlib
import json
import tempfile
import unittest
from pathlib import Path

import bind


SUBJECT = hashlib.sha256(b"artifact").hexdigest()
TOOLCHAIN = hashlib.sha256(b"toolchain").hexdigest()


def write_evidence(root: Path, tier: str, log: str, buildset=None) -> Path:
    artifact = root / "artifact"
    toolchain = root / "toolchain"
    output = root / f"{tier}.log"
    artifact.write_bytes(b"artifact")
    toolchain.write_bytes(b"toolchain")
    output.write_text(log)
    payload = {
        "kind": "proof-evidence", "proof_tier": tier,
        "subject_digest": SUBJECT, "profile": "release",
        "destination": "iphonesimulator", "buildset_digest": buildset,
        "command": ["gmake", "test"], "exit_code": 0, "failures": 0, "skips": 0,
        "test_host": "Orlix" if tier == "product-integration" else "OrlixOSTestApp",
    }
    for name, path in (("artifact", artifact), ("toolchain", toolchain), ("log", output)):
        payload[f"{name}_path"] = str(path)
        payload[f"{name}_digest"] = hashlib.sha256(path.read_bytes()).hexdigest()
    evidence = root / f"{tier}.evidence"
    evidence.write_text(json.dumps(payload))
    return evidence


class BindTests(unittest.TestCase):
    def test_binds_subject_and_prerequisites(self) -> None:
        subject = SUBJECT
        toolchain = TOOLCHAIN
        prior = "33" * 32
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "report.json"
            evidence = write_evidence(Path(tmp), "orlixmlibc", "ORLIX-MLIBC-TEST-END\n")
            payload = bind.bind_report(
                str(path),
                tier="orlixmlibc",
                owner="OrlixMLibC",
                subject_digest=subject,
                profile="release",
                destination="iphonesimulator",
                toolchain_digest=toolchain,
                result="pass",
                prerequisite_digests=[prior],
                evidence_path=str(evidence),
            )
            self.assertEqual(payload["subject_digest"], subject)
            self.assertEqual(payload["prerequisite_digests"], [prior])
            self.assertEqual(payload["proof_tier"], "orlixmlibc")
            self.assertIs(payload["cache_hit_only"], False)
            loaded = json.loads(path.read_text(encoding="utf-8"))
            self.assertEqual(loaded["result"], "pass")

    def test_cache_hit_cannot_authorize(self) -> None:
        with self.assertRaises(bind.BindError):
            bind.bind_report(
                "/unused.json",
                tier="kunit",
                owner="OrlixKernel",
                subject_digest="aa" * 32,
                profile="release",
                destination="iphonesimulator",
                toolchain_digest="bb" * 32,
                result="pass",
                cache_hit_only=True,
            )

    def test_unknown_tier_fails_loud(self) -> None:
        with self.assertRaises(bind.BindError):
            bind.bind_report(
                "/unused.json",
                tier="simulator-launch",
                owner="Orlix",
                subject_digest="aa" * 32,
                profile="release",
                destination="iphonesimulator",
                toolchain_digest="bb" * 32,
                result="pass",
            )


if __name__ == "__main__":
    unittest.main()
