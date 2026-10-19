from __future__ import annotations

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
            self.assertEqual(result["local_cache_hits"], 1)
            self.assertEqual(result["counters"]["bazel_disk_cache_hits"], 1)
            self.assertEqual(result["counters"]["buildbuddy_remote_cache_hits"], 1)
            self.assertIsNone(result["counters"]["ccache_hits"])
            self.assertFalse(result["ccache"]["statistics_read"])
            self.assertEqual(result["elapsed_ms"], 42)
            self.assertIsNone(result["downloaded_bytes"])
            self.assertIsNone(result["uploaded_bytes"])
            cache = root / "ccache"
            cache.mkdir()
            (cache / "object").write_bytes(b"cached-object-bytes")
            sized = cache_observation.reduce_scenario(
                scenario="kernel-tcti",
                execution=execution,
                bep=bep,
                ccache_dir=cache,
            )
            self.assertIsInstance(sized["disk_kibibytes"]["ccache"], int)
            self.assertGreater(sized["disk_kibibytes"]["ccache"], 0)
            self.assertIsNone(sized["counters"]["ccache_hits"])
            self.assertEqual(sized["counters"]["bazel_disk_cache_hits"], 1)
            self.assertEqual(sized["counters"]["buildbuddy_remote_cache_hits"], 1)
            stats = root / "ccache-statistics.json"
            stats.write_text(
                '{"statistics_read": true, "direct_hits": 2, "preprocessed_hits": 1, '
                '"misses": 4, "called_for_link": 0, "cache_size_kibibytes": 16, "ccache_hits": 3}\n',
                encoding="utf-8",
            )
            loaded = cache_observation.load_ccache_statistics(stats)
            counted = cache_observation.observe(execution, bep, "read", loaded)
            self.assertEqual(counted["counters"]["ccache_hits"], 3)
            self.assertEqual(counted["counters"]["bazel_disk_cache_hits"], 1)
            self.assertEqual(counted["counters"]["buildbuddy_remote_cache_hits"], 1)
            self.assertEqual(counted["ccache"]["direct_hits"], 2)
            self.assertEqual(counted["ccache"]["preprocessed_hits"], 1)
            self.assertEqual(counted["ccache"]["called_for_link"], 0)
            self.assertEqual(counted["ccache"]["cache_size_kibibytes"], 16)
            unread = root / "unread.json"
            unread.write_text('{"statistics_read": false, "ccache_hits": 1}\n', encoding="utf-8")
            with self.assertRaises(ValueError):
                cache_observation.load_ccache_statistics(unread)
            shared_zero = {
                "statistics_read": True,
                "direct_hits": 0,
                "preprocessed_hits": 0,
                "misses": 0,
                "called_for_link": 0,
                "cache_size_kibibytes": 124172,
                "ccache_hits": 0,
            }
            log = (
                "INFO: From Building GNU Coreutils with incremental Make state:\n"
                "{\n"
                '  "cache_miss": 870,\n'
                '  "direct_cache_hit": 2,\n'
                '  "preprocessed_cache_hit": 8,\n'
                '  "called_for_link": 454\n'
                "}\n"
            )
            action = cache_observation.coreutils_action_counters(log)
            reported = cache_observation.observe(
                execution, bep, "read", shared_zero, action, True,
            )
            self.assertEqual(reported["ccache"]["direct_hits"], 2)
            self.assertEqual(reported["ccache"]["preprocessed_hits"], 8)
            self.assertEqual(reported["ccache"]["misses"], 870)
            self.assertEqual(reported["ccache"]["source"], "coreutils-action-log")
            self.assertFalse(reported["ccache"]["fresh_runner"])
            self.assertEqual(reported["counters"]["ccache_hits"], 10)
            self.assertEqual(reported["counters"]["buildbuddy_remote_cache_hits"], 1)
            self.assertEqual(reported["shared_ccache_directory"]["misses"], 0)


if __name__ == "__main__":
    unittest.main()
