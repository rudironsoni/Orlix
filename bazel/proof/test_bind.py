from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

import bind


class BindTests(unittest.TestCase):
    def test_binds_subject_and_prerequisites(self) -> None:
        subject = "11" * 32
        toolchain = "22" * 32
        prior = "33" * 32
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "report.json"
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
