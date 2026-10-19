"""Executable source-mutation matrix.

A row is PASS only after this process records compiler invocations or action-key
bytes. Rows that were not executed stay UNVERIFIED. This report has no timings.
"""

from __future__ import annotations

from bazel.feasibility.packages.test_action_inputs import observe_unrelated_package_action_key
from bazel.feasibility.test_compiler_cache_action import observe_compiler_cache


COMPILER_CACHE_ROWS = (
    "compiler_cache_unchanged_second_compile",
    "compiler_cache_changed_header",
    "compiler_cache_changed_optimization",
    "compiler_cache_corrupt_entry",
    "compiler_cache_missing_cache",
)

# These stay UNVERIFIED. Nothing in this module executes them.
UNVERIFIED_ROWS = (
    ("kernel_kbuild_macos", "full kernel Kbuild on macOS is not executed"),
    ("fresh_runner_buildbuddy_hit", "fresh-runner BuildBuddy hits are not executed"),
    ("swift_module_rebuild", "Swift module rebuilds are not executed"),
    ("orlix_engine", "OrlixEngine rebuild is not executed"),
    ("orlix_bootloader", "OrlixBootloader rebuild is not executed"),
    ("orlix_host_adapter", "OrlixHostAdapter rebuild is not executed"),
    ("installed_uapi", "installed UAPI rebuild is not executed"),
    ("mlibc_rebuild", "mlibc rebuild is not executed"),
    ("rootfs_policy", "rootfs policy rebuild is not executed"),
)


def _unverified(row_id: str, reason: str) -> dict[str, object]:
    return {"id": row_id, "status": "UNVERIFIED", "reason": reason}


def _fail(row_id: str, reason: str) -> dict[str, object]:
    return {"id": row_id, "status": "FAIL", "reason": reason}


def _compiler_cache_rows() -> list[dict[str, object]]:
    try:
        observed = observe_compiler_cache()
    except (AssertionError, OSError) as error:
        return [_fail(row_id, error.__class__.__name__) for row_id in COMPILER_CACHE_ROWS]
    if observed is None:
        return [
            _unverified(row_id, "ccache or C compiler was not executed")
            for row_id in COMPILER_CACHE_ROWS
        ]
    cases = observed["cases"]
    if not isinstance(cases, dict):
        return [_fail(row_id, "compiler cache observation has no cases") for row_id in COMPILER_CACHE_ROWS]
    unchanged = cases["compiler_cache_unchanged_second_compile"]
    header = cases["compiler_cache_changed_header"]
    flagged = cases["compiler_cache_changed_optimization"]
    corrupt = cases["compiler_cache_corrupt_entry"]
    missing = cases["compiler_cache_missing_cache"]
    rows = [
        _compiler_row(
            "compiler_cache_unchanged_second_compile",
            unchanged["invocations"] >= 2
            and unchanged["cold_hits"] == 0
            and unchanged["cold_misses"] >= 1
            and unchanged["objects_equal"] is True
            and unchanged["hits"] >= 1,
            unchanged,
            "cold_hits",
            "cold_misses",
            "hits",
            "misses",
        ),
        _compiler_row(
            "compiler_cache_changed_header",
            header["invocations"] >= 1
            and header["object_differs"] is True
            and header["misses"] > unchanged["misses"]
            and header["hits"] == unchanged["hits"],
            header,
            "hits",
            "misses",
        ),
        _compiler_row(
            "compiler_cache_changed_optimization",
            flagged["invocations"] >= 1
            and flagged["object_differs"] is True
            and flagged["misses"] > header["misses"],
            flagged,
            "hits",
            "misses",
        ),
        _compiler_row(
            "compiler_cache_corrupt_entry",
            corrupt["invocations"] >= 1
            and corrupt["object_matches"] is True
            and corrupt["entry_rewritten"] is True,
            corrupt,
            "object_matches",
            "entry_rewritten",
        ),
        _compiler_row(
            "compiler_cache_missing_cache",
            missing["invocations"] >= 1
            and missing["object_matches"] is True
            and missing["cache_created"] is True,
            missing,
            "object_matches",
            "cache_created",
        ),
    ]
    for row in rows:
        if row["status"] == "PASS" and int(row["invocations"]) < 1:
            row["status"] = "FAIL"
            row["reason"] = "no compiler invocation was recorded"
    return rows


def _compiler_row(
    row_id: str,
    passed: bool,
    case: dict[str, object],
    *fields: str,
) -> dict[str, object]:
    row: dict[str, object] = {
        "id": row_id,
        "status": "PASS" if passed else "FAIL",
        "invocations": case["invocations"],
    }
    for field in fields:
        row[field] = case[field]
    if not passed:
        row["reason"] = "observed compiler cache did not match the closure"
    return row


def _guest_package_row() -> dict[str, object]:
    try:
        observed = observe_unrelated_package_action_key()
    except (AssertionError, OSError) as error:
        return _fail("guest_package_unrelated_input", error.__class__.__name__)
    passed = (
        int(observed["source_key_invocations"]) >= 3
        and observed["fail_closed"] is True
        and observed["foreign_edit_changes_action_key"] is False
        and observed["owned_edit_changes_action_key"] is True
        and observed["included_foreign_changes_key"] is True
        and observed["foreign_digest_in_stamp"] is False
        and observed["owned_digest_in_stamp"] is True
        and observed["observed_paths"] == observed["expected_paths"]
        and bool(observed["observed_paths"])
    )
    row: dict[str, object] = {
        "id": "guest_package_unrelated_input",
        "status": "PASS" if passed else "FAIL",
        "invocations": observed["source_key_invocations"],
        "selector_calls": observed["selector_calls"],
        "expected_paths": observed["expected_paths"],
        "observed_paths": observed["observed_paths"],
        "rejected_paths": observed["rejected_paths"],
        "fail_closed": observed["fail_closed"],
        "foreign_digest_in_stamp": observed["foreign_digest_in_stamp"],
        "owned_digest_in_stamp": observed["owned_digest_in_stamp"],
        "foreign_edit_changes_action_key": observed["foreign_edit_changes_action_key"],
        "owned_edit_changes_action_key": observed["owned_edit_changes_action_key"],
        "included_foreign_changes_key": observed["included_foreign_changes_key"],
        "stamp_sha256": observed["stamp_sha256"],
    }
    if not passed:
        row["reason"] = "unrelated package input changed the action key or was not executed"
    return row


def run_source_mutation_matrix() -> list[dict[str, object]]:
    rows = [*_compiler_cache_rows(), _guest_package_row()]
    rows.extend(_unverified(row_id, reason) for row_id, reason in UNVERIFIED_ROWS)
    return rows


def format_report(rows: list[dict[str, object]]) -> str:
    lines = []
    for row in rows:
        parts = [f"source-mutation-matrix {row['id']} {row['status']}"]
        for key in sorted(name for name in row if name not in {"id", "status"}):
            parts.append(f"{key}={_format_value(row[key])}")
        lines.append(" ".join(parts))
    return "\n".join(lines) + "\n"


def _format_value(value: object) -> str:
    if isinstance(value, bool):
        return "true" if value else "false"
    text = str(value)
    if any(character.isspace() for character in text):
        return '"' + text.replace('"', "'") + '"'
    return text


def main() -> int:
    rows = run_source_mutation_matrix()
    print(format_report(rows), end="", flush=True)
    return 1 if any(row["status"] == "FAIL" for row in rows) else 0


if __name__ == "__main__":
    raise SystemExit(main())
