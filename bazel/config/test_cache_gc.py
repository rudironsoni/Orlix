from __future__ import annotations

import os
import tempfile
import unittest
from pathlib import Path

import cache_gc


class CacheGcTests(unittest.TestCase):
    def test_wrong_namespace_is_rejected(self) -> None:
        with self.assertRaises(cache_gc.CacheGcError):
            cache_gc.gc(
                Path("/tmp/bazel-9.2.0-xcode-27A5252f"),
                namespace="bazel-9.2.0-xcode-17F113",
            )

    def test_stale_and_oversize_entries_are_removed(self) -> None:
        with tempfile.TemporaryDirectory(prefix="bazel-9.2.0-xcode-17F113-") as tmp:
            root = Path(tmp)
            stale = root / "stale"
            fresh = root / "fresh"
            stale.write_bytes(b"old")
            fresh.write_bytes(b"keep")
            now = 1_000_000.0
            stale_atime = now - cache_gc.MAX_AGE_SECONDS - 10
            os.utime(stale, (stale_atime, stale_atime))
            os.utime(fresh, (now, now))
            payload = cache_gc.gc(
                root,
                namespace="bazel-9.2.0-xcode-17F113",
                now=now,
                max_bytes=8,
                max_age_seconds=cache_gc.MAX_AGE_SECONDS,
            )
            self.assertFalse(stale.exists())
            self.assertTrue(fresh.exists())
            self.assertGreaterEqual(payload["removed"], 1)


if __name__ == "__main__":
    unittest.main()
