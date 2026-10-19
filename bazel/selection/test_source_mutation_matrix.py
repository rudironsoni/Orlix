"""The source-mutation matrix records execution. UNVERIFIED is not a pass."""

from __future__ import annotations

from pathlib import Path
import unittest

from bazel.selection.source_mutation_matrix import (
    COMPILER_CACHE_ROWS,
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
