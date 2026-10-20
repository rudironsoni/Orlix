from __future__ import annotations

import gzip
import json
import tempfile
import unittest
from pathlib import Path

import cache_observation


class CacheObservationTests(unittest.TestCase):
    def test_reports_available_metrics_without_inventing_transfer_bytes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            execution = root / "execution.json"
            bep = root / "bep.json"
            execution.write_text(
                ''.join(json.dumps({"runner": runner, "cacheHit": runner != "local"}, indent=2) for runner in (
                    "remote cache hit", "disk cache hit", "local", "output-base action cache"
                ))
            )
            bep.write_text(json.dumps({"buildMetrics": {
                "actionSummary": {"actionsCreated": "7", "actionsExecuted": "3"},
                "timingMetrics": {"wallTimeInMs": "42"},
            }}) + "\n")
            result = cache_observation.observe(execution, bep, "read")
            self.assertEqual(result["remote_hits"], 1)
            self.assertEqual(result["buildbuddy_remote_cache_hits"], 1)
            self.assertEqual(result["local_cache_hits"], 1)
            self.assertEqual(result["elapsed_ms"], 42)
            self.assertIsNone(result["downloaded_bytes"])
            self.assertIsNone(result["uploaded_bytes"])
            self.assertIsNot(result.get("fresh_runner"), True)

    def test_false_cache_hit_does_not_increment_buildbuddy_remote_hits(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            execution = root / "execution.json"
            bep = root / "bep.json"
            execution.write_text(
                "".join(
                    json.dumps(action)
                    for action in (
                        {"runner": "remote cache hit", "cacheHit": False},
                        {"runner": "disk cache hit", "cacheHit": True},
                        {"runner": "remote cache", "cacheHit": True},
                        {"runner": "remote cache hit", "cacheHit": True},
                    )
                ),
                encoding="utf-8",
            )
            bep.write_text(json.dumps({"buildMetrics": {
                "actionSummary": {"actionsCreated": "4", "actionsExecuted": "4"},
                "timingMetrics": {"wallTimeInMs": "1"},
            }}) + "\n")
            result = cache_observation.observe(execution, bep, "read")
            self.assertEqual(result["buildbuddy_remote_cache_hits"], 1)
            self.assertEqual(result["remote_hits"], 1)
            self.assertEqual(result["local_cache_hits"], 1)
            self.assertIsNot(result.get("fresh_runner"), True)
            row = cache_observation.reduce_scenario(
                scenario="kernel-tcti",
                execution=execution,
                bep=bep,
            )
            self.assertEqual(row["buildbuddy_remote_cache_hits"], 1)
            self.assertEqual(row["execution"]["remote_cache_hits"], 1)
            self.assertIsNot(row.get("fresh_runner"), True)

    def test_profile_transfer_bytes_sum_download_and_upload_counters(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            profile = Path(temporary) / "profile.json.gz"
            payload = {"traceEvents": [
                {"ph": "C", "name": "remote download", "args": {"bytes": 120, "files": 2}},
                {"ph": "C", "name": "remote upload", "args": {"bytes": "40"}},
                {"ph": "X", "name": "remote download", "args": {"bytes": 999}},
                {"ph": "C", "name": "action cache hits", "args": {"hits": 3}},
            ]}
            with gzip.open(profile, "wt", encoding="utf-8") as handle:
                json.dump(payload, handle)
            result = cache_observation.profile_transfer_bytes(profile)
            self.assertEqual(result["byte_source"], "bazel-profile")
            self.assertEqual(result["downloaded_bytes"], 122)
            self.assertEqual(result["uploaded_bytes"], 40)

    def test_profile_transfer_bytes_stay_absent_without_counters(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            profile = Path(temporary) / "profile.json.gz"
            with gzip.open(profile, "wt", encoding="utf-8") as handle:
                json.dump({"traceEvents": [{"ph": "X", "name": "build"}]}, handle)
            result = cache_observation.profile_transfer_bytes(profile)
            self.assertEqual(result["byte_source"], "bazel-profile")
            self.assertIsNone(result["downloaded_bytes"])
            self.assertIsNone(result["uploaded_bytes"])


if __name__ == "__main__":
    unittest.main()
