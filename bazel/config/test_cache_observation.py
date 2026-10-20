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
            self.assertEqual(result["buildbuddy_remote_cache_hits"], 1)
            self.assertEqual(result["local_cache_hits"], 1)
            self.assertEqual(result["elapsed_ms"], 42)
            self.assertEqual(result["byte_source"], "buildMetrics.networkMetrics.systemNetworkStats")
            self.assertIsNone(result["downloaded_bytes"])
            self.assertIsNone(result["uploaded_bytes"])
            self.assertIsNone(result["remote_downloads"])
            self.assertIsNone(result["remote_uploads"])
            self.assertIsNot(result.get("fresh_runner"), True)
            self.assertEqual(result["counters"]["bazel_disk_cache_hits"], 1)
            self.assertEqual(result["counters"]["buildbuddy_remote_cache_hits"], 1)
            self.assertIsNone(result["counters"]["ccache_hits"])
            self.assertIsNone(result["counters"]["ccache_misses"])
            self.assertFalse(result["ccache"]["statistics_read"])
            self.assertFalse(result["ccache"]["fresh_runner"])

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
            self.assertEqual(result["counters"]["buildbuddy_remote_cache_hits"], 1)
            self.assertEqual(result["counters"]["bazel_disk_cache_hits"], 1)
            self.assertIsNone(result["counters"]["ccache_hits"])
            self.assertFalse(result["ccache"]["fresh_runner"])
            row = cache_observation.reduce_scenario(
                scenario="kernel-tcti",
                execution=execution,
                bep=bep,
            )
            self.assertEqual(row["buildbuddy_remote_cache_hits"], 1)
            self.assertEqual(row["execution"]["remote_cache_hits"], 1)
            self.assertIsNot(row.get("fresh_runner"), True)
            self.assertEqual(row["counters"]["buildbuddy_remote_cache_hits"], 1)
            self.assertEqual(row["counters"]["bazel_disk_cache_hits"], 1)
            self.assertFalse(row["ccache"]["fresh_runner"])

    def test_coreutils_counters_stay_separate_from_disk_and_buildbuddy(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            execution = root / "execution.json"
            bep = root / "bep.json"
            execution.write_text(
                "".join(
                    json.dumps({"runner": runner, "cacheHit": runner != "local"}, indent=2)
                    for runner in (
                        "remote cache hit",
                        "disk cache hit",
                        "local",
                        "output-base action cache",
                    )
                )
            )
            bep.write_text(json.dumps({"buildMetrics": {
                "actionSummary": {"actionsCreated": "7", "actionsExecuted": "3"},
                "timingMetrics": {"wallTimeInMs": "42"},
            }}) + "\n")
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
            self.assertIsNone(sized["counters"]["ccache_misses"])
            self.assertEqual(sized["counters"]["bazel_disk_cache_hits"], 1)
            self.assertEqual(sized["counters"]["buildbuddy_remote_cache_hits"], 1)
            self.assertFalse(sized["ccache"]["fresh_runner"])
            stats = root / "ccache-statistics.json"
            stats.write_text(
                '{"statistics_read": true, "direct_hits": 2, "preprocessed_hits": 1, '
                '"misses": 4, "called_for_link": 0, "cache_size_kibibytes": 16, "ccache_hits": 3}\n',
                encoding="utf-8",
            )
            loaded = cache_observation.load_ccache_statistics(stats)
            counted = cache_observation.observe(execution, bep, "read", loaded)
            self.assertEqual(counted["counters"]["ccache_hits"], 3)
            self.assertEqual(counted["counters"]["ccache_misses"], 4)
            self.assertEqual(counted["counters"]["bazel_disk_cache_hits"], 1)
            self.assertEqual(counted["counters"]["buildbuddy_remote_cache_hits"], 1)
            self.assertEqual(counted["ccache"]["direct_hits"], 2)
            self.assertEqual(counted["ccache"]["preprocessed_hits"], 1)
            self.assertEqual(counted["ccache"]["called_for_link"], 0)
            self.assertEqual(counted["ccache"]["cache_size_kibibytes"], 16)
            self.assertFalse(counted["ccache"]["fresh_runner"])
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
            self.assertIsNotNone(action)
            assert action is not None
            self.assertEqual(action["direct_cache_hit"], 2)
            self.assertEqual(action["preprocessed_cache_hit"], 8)
            self.assertEqual(action["cache_miss"], 870)
            self.assertIsNone(action["direct_cache_miss"])
            self.assertEqual(action["ccache_misses"], 870)
            reported = cache_observation.observe(
                execution, bep, "read", shared_zero, action, True,
            )
            self.assertEqual(reported["ccache"]["direct_cache_hit"], 2)
            self.assertEqual(reported["ccache"]["preprocessed_cache_hit"], 8)
            self.assertEqual(reported["ccache"]["cache_miss"], 870)
            self.assertEqual(reported["ccache"]["ccache_misses"], 870)
            self.assertEqual(reported["ccache"]["misses"], 870)
            self.assertEqual(reported["ccache"]["source"], "coreutils-action-log")
            self.assertFalse(reported["ccache"]["fresh_runner"])
            self.assertEqual(reported["counters"]["ccache_hits"], 10)
            self.assertEqual(reported["counters"]["ccache_misses"], 870)
            self.assertEqual(reported["counters"]["bazel_disk_cache_hits"], 1)
            self.assertEqual(reported["counters"]["buildbuddy_remote_cache_hits"], 1)
            self.assertEqual(reported["shared_ccache_directory"]["misses"], 0)
            self.assertFalse(reported["shared_ccache_directory"]["fresh_runner"])

    def test_coreutils_misses_include_direct_cache_miss(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            execution = root / "execution.json"
            bep = root / "bep.json"
            execution.write_text(json.dumps({"runner": "remote cache hit", "cacheHit": True}))
            bep.write_text(json.dumps({"buildMetrics": {
                "actionSummary": {"actionsCreated": "1", "actionsExecuted": "1"},
                "timingMetrics": {"wallTimeInMs": "1"},
            }}) + "\n")
            log = (
                "INFO: From Building GNU Coreutils with incremental Make state:\n"
                "{\n"
                '  "cache_miss": 0,\n'
                '  "direct_cache_hit": 879,\n'
                '  "direct_cache_miss": 173,\n'
                '  "preprocessed_cache_hit": 1,\n'
                '  "preprocessed_cache_miss": 131\n'
                "}\n"
            )
            action = cache_observation.coreutils_action_counters(log)
            self.assertIsNotNone(action)
            assert action is not None
            self.assertEqual(action["direct_cache_hit"], 879)
            self.assertEqual(action["direct_cache_miss"], 173)
            self.assertEqual(action["preprocessed_cache_hit"], 1)
            self.assertEqual(action["cache_miss"], 0)
            self.assertEqual(action["ccache_misses"], 173)
            self.assertEqual(action["misses"], 173)
            execution.write_text(
                "".join(
                    json.dumps(record)
                    for record in (
                        {"runner": "remote cache hit", "cacheHit": False},
                        {"runner": "disk cache hit", "cacheHit": True},
                        {"runner": "remote cache hit", "cacheHit": True},
                    )
                ),
                encoding="utf-8",
            )
            reported = cache_observation.observe(execution, bep, "read", None, action, True)
            self.assertEqual(reported["ccache"]["direct_cache_hit"], 879)
            self.assertEqual(reported["ccache"]["direct_cache_miss"], 173)
            self.assertEqual(reported["ccache"]["cache_miss"], 0)
            self.assertEqual(reported["ccache"]["ccache_misses"], 173)
            self.assertEqual(reported["ccache"]["misses"], 173)
            self.assertEqual(reported["ccache"]["source"], "coreutils-action-log")
            self.assertFalse(reported["ccache"]["fresh_runner"])
            self.assertIsNot(reported.get("fresh_runner"), True)
            self.assertEqual(reported["counters"]["ccache_hits"], 880)
            self.assertEqual(reported["counters"]["ccache_misses"], 173)
            self.assertEqual(reported["counters"]["bazel_disk_cache_hits"], 1)
            self.assertEqual(reported["counters"]["buildbuddy_remote_cache_hits"], 1)

    def test_bep_system_network_stats_parse_decimal_strings(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bep = root / "build-events.json"
            bep.write_text(
                "\n".join([
                    json.dumps({"id": {"progress": {}}}),
                    json.dumps({"buildMetrics": {
                        "actionSummary": {
                            "actionsExecuted": "428",
                            "actionCacheStatistics": {"sizeInBytes": "999"},
                        },
                        "networkMetrics": {"systemNetworkStats": {
                            "bytesRecv": "139074560",
                            "bytesSent": "5031936",
                        }},
                    }}),
                    "",
                ]),
                encoding="utf-8",
            )
            execution = root / "execution.json"
            execution.write_text(json.dumps({"runner": "remote cache hit", "cacheHit": True}) + "\n")
            result = cache_observation.bep_transfer_bytes(bep)
            self.assertEqual(result["byte_source"], "buildMetrics.networkMetrics.systemNetworkStats")
            self.assertEqual(result["downloaded_bytes"], 139074560)
            self.assertEqual(result["uploaded_bytes"], 5031936)
            observed = cache_observation.observe(execution, bep, "read")
            self.assertEqual(observed["remote_hits"], 1)
            self.assertEqual(observed["downloaded_bytes"], 139074560)
            self.assertEqual(observed["uploaded_bytes"], 5031936)
            self.assertIsNone(observed["remote_downloads"])
            self.assertIsNone(observed["remote_uploads"])

    def test_cas_transfer_bytes_sum_bytestream_payloads_only(self) -> None:
        def varint(value: int) -> bytes:
            encoded = bytearray()
            while True:
                piece = value & 0x7F
                value >>= 7
                if value:
                    encoded.append(piece | 0x80)
                else:
                    encoded.append(piece)
                    return bytes(encoded)

        def message(field: int, payload: bytes) -> bytes:
            return varint((field << 3) | 2) + varint(len(payload)) + payload

        def entry(details: bytes) -> bytes:
            body = message(4, details)
            return varint(len(body)) + body

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            missing = cache_observation.cas_transfer_bytes(root / "remote-grpc.log")
            self.assertIsNone(missing["downloaded_bytes"])
            self.assertIsNone(missing["uploaded_bytes"])
            empty = root / "empty.log"
            empty.write_bytes(b"")
            zeros = cache_observation.cas_transfer_bytes(empty)
            self.assertEqual(zeros["downloaded_bytes"], 0)
            self.assertEqual(zeros["uploaded_bytes"], 0)
            read_one = varint((3 << 3) | 0) + varint(300)
            read_two = varint((2 << 3) | 0) + varint(4) + varint((3 << 3) | 0) + varint(200000)
            write = varint((3 << 3) | 0) + varint(7) + varint((3 << 3) | 0) + varint(20)
            action_cache = message(8, varint((3 << 3) | 0) + varint(999))
            find_missing = message(10, varint((1 << 3) | 2) + varint(0))
            log = root / "remote-grpc.log"
            log.write_bytes(
                entry(message(5, read_one))
                + entry(message(5, read_two))
                + entry(message(6, write))
                + entry(action_cache)
                + entry(find_missing)
            )
            counted = cache_observation.cas_transfer_bytes(log)
            self.assertEqual(counted["downloaded_bytes"], 200300)
            self.assertEqual(counted["uploaded_bytes"], 20)
            self.assertIn("read.bytes_read", counted["byte_source"])
            self.assertIn("write.bytes_sent", counted["byte_source"])
            truncated = root / "truncated.log"
            truncated.write_bytes(b"\x80")
            with self.assertRaises(ValueError):
                cache_observation.cas_transfer_bytes(truncated)

    def test_bep_transfer_bytes_stay_absent_without_fields(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            empty = root / "empty.json"
            empty.write_text("", encoding="utf-8")
            absent = cache_observation.bep_transfer_bytes(empty)
            self.assertEqual(absent["byte_source"], "buildMetrics.networkMetrics.systemNetworkStats")
            self.assertIsNone(absent["downloaded_bytes"])
            self.assertIsNone(absent["uploaded_bytes"])
            partial = root / "partial.json"
            partial.write_text(json.dumps({"buildMetrics": {"networkMetrics": {"systemNetworkStats": {
                "bytesRecv": "12",
                "bytesSent": 40,
            }}}}) + "\n")
            one_sided = cache_observation.bep_transfer_bytes(partial)
            self.assertEqual(one_sided["downloaded_bytes"], 12)
            self.assertIsNone(one_sided["uploaded_bytes"])
            rates = root / "profile.json"
            rates.write_text(json.dumps({"traceEvents": [
                {"ph": "C", "name": "Network Down usage (total)", "args": {"system network down (Mbps)": 12.5}},
                {"ph": "C", "name": "Network Up usage (total)", "args": {"system network up (Mbps)": 1.25}},
            ]}) + "\n")
            profile = cache_observation.bep_transfer_bytes(rates)
            self.assertIsNone(profile["downloaded_bytes"])
            self.assertIsNone(profile["uploaded_bytes"])

    def test_local_hits_are_not_counted_as_buildbuddy_remote_hits(self) -> None:
        runners = (
            "remote cache hit",
            "disk cache hit",
            "local",
            "output-base action cache",
            "ccache hit",
            "previously generated file",
        )
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            execution = root / "execution.json"
            bep = root / "bep.json"
            execution.write_text(
                "".join(
                    json.dumps({"runner": runner, "cacheHit": runner != "local"})
                    for runner in runners
                ),
                encoding="utf-8",
            )
            bep.write_text(
                json.dumps({"buildMetrics": {"timingMetrics": {"wallTimeInMs": 1}}}),
                encoding="utf-8",
            )
            result = cache_observation.observe(execution, bep, "read")
            self.assertEqual(result["remote_hits"], 1)
            self.assertEqual(result["buildbuddy_remote_cache_hits"], 1)
            self.assertEqual(result["local_cache_hits"], 1)
            self.assertIsNot(result.get("fresh_runner"), True)
            self.assertEqual(result["counters"]["bazel_disk_cache_hits"], 1)
            self.assertEqual(result["counters"]["buildbuddy_remote_cache_hits"], 1)
            self.assertIsNone(result["counters"]["ccache_hits"])
            self.assertIsNone(result["counters"]["ccache_misses"])
            self.assertFalse(result["ccache"]["fresh_runner"])
            row = cache_observation.reduce_scenario(
                scenario="corrupt-cache",
                execution=execution,
                bep=bep,
                access="read",
            )
            counted = row["execution"]["runner"]
            self.assertEqual(row["execution"]["remote_cache_hits"], 1)
            self.assertEqual(counted["remote cache hit"], 1)
            self.assertEqual(counted["disk cache hit"], 1)
            self.assertEqual(counted["local"], 1)
            self.assertEqual(counted["output-base action cache"], 1)
            self.assertEqual(counted["ccache hit"], 1)
            self.assertEqual(counted["previously generated file"], 1)
            self.assertEqual(row["buildbuddy_remote_cache_hits"], 1)
            self.assertEqual(row["counters"]["bazel_disk_cache_hits"], 1)
            self.assertEqual(row["counters"]["buildbuddy_remote_cache_hits"], 1)
            self.assertIsNone(row["counters"]["ccache_hits"])
            self.assertIsNone(row["counters"]["ccache_misses"])
            self.assertFalse(row["ccache"]["fresh_runner"])
            self.assertIsNot(row.get("fresh_runner"), True)


if __name__ == "__main__":
    unittest.main()
