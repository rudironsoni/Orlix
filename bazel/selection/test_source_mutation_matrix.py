"""The source-mutation matrix records execution. UNVERIFIED is not a pass."""

from __future__ import annotations

from pathlib import Path
import unittest

from bazel.selection.source_mutation_matrix import (
    COMPILER_CACHE_ROWS,
    EDIT_ROWS,
    UNVERIFIED_ROWS,
    format_report,
    run_source_mutation_matrix,
)


_TIMING_TOKENS = ("elapsed", "duration", "timing", "wall", "second")
_PASS_EVIDENCE = {
    "compiler_cache_unchanged_second_compile": ("invocations", "hits", "misses", "cold_hits"),
    "compiler_cache_changed_header": ("invocations", "hits", "misses"),
    "compiler_cache_changed_optimization": ("invocations", "hits", "misses"),
    "compiler_cache_corrupt_entry": ("invocations", "object_matches"),
    "compiler_cache_missing_cache": ("invocations", "object_matches", "cache_created"),
    "guest_package_unrelated_input": ("invocations", "stamp_sha256", "observed_paths"),
    "kernel_c_closure": ("invocations", "rebuilt", "kept_objects"),
    "swift_unrelated_edit": ("invocations", "compiler_invocations", "kernel_key_changed"),
    "mlibc_edit_keeps_kernel": ("invocations", "compiler_invocations", "kernel_key_changed"),
    "guest_package_edit_keeps_kernel_and_mlibc": ("invocations", "compiler_invocations", "kernel_key_changed"),
    "fresh_runner_buildbuddy_hit": ("invocations", "fresh_layer", "ccache_layer", "disk_layer"),
}


class SourceMutationMatrixTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.rows = run_source_mutation_matrix()
        cls.by_id = {str(row["id"]): row for row in cls.rows}
        cls.report = format_report(cls.rows)

    def test_report_lists_every_row_status(self) -> None:
        print(self.report, end="", flush=True)
        self.assertEqual(len(self.by_id), len(self.rows))
        expected = [
            *COMPILER_CACHE_ROWS,
            "guest_package_unrelated_input",
            *EDIT_ROWS,
            "fresh_runner_buildbuddy_hit",
            *[row_id for row_id, _reason in UNVERIFIED_ROWS],
        ]
        self.assertEqual([row["id"] for row in self.rows], expected)
        for row in self.rows:
            self.assertIn(row["status"], {"PASS", "FAIL", "UNVERIFIED"})
            self.assertIn(
                f"source-mutation-matrix {row['id']} {row['status']}",
                self.report,
            )

    def test_pass_rows_record_invocations_or_cache_results(self) -> None:
        executed = [row for row in self.rows if row["status"] != "UNVERIFIED"]
        self.assertTrue(executed)
        for row in executed:
            self.assertIn(row["status"], {"PASS", "FAIL"})
            if row["status"] != "PASS":
                continue
            self.assertGreaterEqual(int(row["invocations"]), 1)
            for field in _PASS_EVIDENCE[str(row["id"])]:
                self.assertIn(field, row)
            if row["id"] == "guest_package_unrelated_input":
                self.assertEqual(len(str(row["stamp_sha256"])), 64)
                self.assertFalse(row["foreign_digest_in_stamp"])
                self.assertFalse(row["foreign_edit_changes_action_key"])
                self.assertTrue(row["owned_edit_changes_action_key"])

    def test_required_rows_record_closures_and_cache_layers(self) -> None:
        kernel = self.by_id["kernel_c_closure"]
        swift = self.by_id["swift_unrelated_edit"]
        mlibc = self.by_id["mlibc_edit_keeps_kernel"]
        guest = self.by_id["guest_package_edit_keeps_kernel_and_mlibc"]
        remote = self.by_id["fresh_runner_buildbuddy_hit"]
        for row in (kernel, swift, mlibc, guest, remote):
            self.assertEqual(row["status"], "PASS", self.report)
        self.assertIn("changed.c", str(kernel["rebuilt"]))
        self.assertNotIn("dependent.c", str(kernel["rebuilt"]))
        self.assertNotIn("unrelated.c", str(kernel["rebuilt"]))
        self.assertEqual(kernel["leaked"], "")
        self.assertIn("dependent.o", str(kernel["kept_objects"]))
        self.assertIn("unrelated.o", str(kernel["kept_objects"]))
        self.assertTrue(kernel["kernel_key_changed"])
        self.assertFalse(kernel["mlibc_key_changed"])
        self.assertFalse(kernel["guest_key_changed"])
        self.assertFalse(kernel["ssh_key_changed"])
        self.assertEqual(swift["compiler_invocations"], 0)
        self.assertTrue(swift["swift_key_changed"])
        self.assertFalse(swift["kernel_key_changed"])
        self.assertFalse(swift["mlibc_key_changed"])
        self.assertFalse(swift["guest_key_changed"])
        self.assertFalse(swift["ssh_key_changed"])
        self.assertEqual(mlibc["compiler_invocations"], 0)
        self.assertTrue(mlibc["mlibc_key_changed"])
        self.assertFalse(mlibc["kernel_key_changed"])
        self.assertEqual(guest["compiler_invocations"], 0)
        self.assertTrue(guest["guest_key_changed"])
        self.assertFalse(guest["kernel_key_changed"])
        self.assertFalse(guest["mlibc_key_changed"])
        self.assertEqual(remote["fresh_layer"], "local store read")
        self.assertEqual(remote["ccache_layer"], "ccache")
        self.assertEqual(remote["disk_layer"], "disk cache hit")
        self.assertTrue(remote["layers_distinct"])
        self.assertFalse(remote["fresh_runner"])
        self.assertFalse(remote["live_remote_hit"])
        self.assertFalse(remote["opened_endpoint"])
        self.assertEqual(remote["local_store_reads"], 1)
        self.assertEqual(remote["fresh_ccache_hits"], 0)
        self.assertEqual(remote["fresh_disk_reads"], 0)
        self.assertEqual(remote["fresh_remote_reads"], 0)
        self.assertEqual(remote["fresh_compiler_invocations"], 0)
        self.assertEqual(remote["ccache_compiler_invocations_on_hit"], 0)
        self.assertEqual(remote["disk_compiler_invocations"], 0)
        self.assertEqual(remote["disk_remote_reads"], 0)
        self.assertIn("buildbuddy", str(remote["remote_cache_url"]))

    def test_named_rows_stay_unverified_without_timings(self) -> None:
        for row_id, reason in UNVERIFIED_ROWS:
            row = self.by_id[row_id]
            self.assertEqual(row["status"], "UNVERIFIED")
            self.assertEqual(row["reason"], reason)
            self.assertEqual(set(row), {"id", "status", "reason"})
        for row in self.rows:
            for key in row:
                lowered = key.lower()
                self.assertFalse(any(token in lowered for token in _TIMING_TOKENS), key)
            if row["status"] == "UNVERIFIED":
                self.assertNotIn("hits", row)
                self.assertNotIn("misses", row)
                self.assertNotIn("invocations", row)
                self.assertNotIn("stamp_sha256", row)

    def test_matrix_check_runs_the_executable_rows(self) -> None:
        makefile = Path(__file__).resolve().parents[2] / "make" / "bazel-migration.mk"
        recipe = makefile.read_text(encoding="utf-8").split("__bazel-matrix-check:", 1)[1]
        recipe = recipe.split("__bazel-orlixos:", 1)[0]
        self.assertIn("python3 -m unittest bazel.selection.test_source_mutation_matrix", recipe)
        self.assertIn("python3 -m unittest bazel.feasibility.test_compiler_cache_action", recipe)

    def test_executed_failures_fail_the_matrix(self) -> None:
        failed = [row for row in self.rows if row["status"] == "FAIL"]
        self.assertEqual(failed, [], self.report)


if __name__ == "__main__":
    unittest.main()
